/*
 * SceLibMtp: the handle maps (OMAP).
 *
 * Each physical storage gives its objects' handles from a table of its own, in a temporary
 * SQLite database: a row per object, its handle the row's base number with the storage's
 * bits and the object's flags (mask) over it, as the table's trigger sets it.
 */

#include <sqlite3.h>

#include "mtp_internal.h"

#define MTP_TAG_OMAP	"OMAP:"

/* the handle map's lock, held while the object lives */
class AutoLock {
public:
	AutoLock(SceKernelLwMutexWork *work) : m_work(work)
	{
		int ret = sceKernelLockLwMutex(work, 1, NULL);

		if (ret < 0) {
			MTP_ERR(MTP_TAG_OMAP, 31, "sceKernelLockLwMutex failed: 0x%x\n", ret);
			m_work = NULL;
		}
	}

	~AutoLock()
	{
		int ret;

		if (m_work == NULL)
			return;
		ret = sceKernelUnlockLwMutex(m_work, 1);
		if (ret < 0)
			MTP_ERR(MTP_TAG_OMAP, 42, "sceKernelUnlockMutex failed: 0x%x\n", ret);
	}

private:
	SceKernelLwMutexWork *m_work;
};

/* the first column of the row a statement stepped to */
static inline int getUInt32Result(sqlite3_stmt *stmt, SceUInt32 *value)
{
	int type = sqlite3_column_type(stmt, 0);

	if (type != SQLITE_INTEGER) {
		MTP_ERR(MTP_TAG_OMAP, 95, "Invalid result type: %d\n", type);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	*value = sqlite3_column_int64(stmt, 0);
	return 0;
}

/* the one value sql (one parameter: key) selects */
static int findObjectId(MtpObjmap *objmap, const char *sql, SceUInt32 key, SceUInt32 *value)
{
	MtpSqlBind bind = { MTP_SQL_BIND_INTEGER, { key }, sizeof(key) };
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int found, step;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	db = objmap->db;
	stmt = NULL;
	ret = mtpSqlCreateStatement(db, sql, &stmt);
	if (ret != 0)
		return ret;
	ret = mtpSqlBindStatement(stmt, &bind, 1);
	if (ret < 0) {
		sqlite3_finalize(stmt);
		return ret;
	}
	found = 0;
	for (;;) {
		step = sqlite3_step(stmt);
		if (step != SQLITE_ROW)
			break;
		if (found++ != 0) {
			MTP_ERR(MTP_TAG_OMAP, 133, "found %d results for key 0x%x\n", found, key);
			break;
		}
		ret = getUInt32Result(stmt, value);
		if (ret < 0) {
			sqlite3_finalize(stmt);
			return ret;
		}
	}
	sqlite3_finalize(stmt);
	if (step != SQLITE_DONE) {
		MTP_ERR(MTP_TAG_OMAP, 145, "%s failed: %d, (%s)\n", "sqlite3_step", step,
			sqlite3_errmsg(db));
		return mtpSqlConvertError(step);
	}
	if (found == 0) {
		MTP_ERR(MTP_TAG_OMAP, 153, "key (0x%x) is not found\n", key);
		return SCE_MTP_ERROR_NOT_FOUND;
	}
	return ret;
}

extern "C" {

int mtpObjmapCreate(MtpObjmap *objmap, SceUInt32 storageId)
{
	sqlite3 *db = NULL;
	SceUInt32 handleBase;
	const char *sqls[] = {
		"PRAGMA journal_mode=off",
		"PRAGMA cache_size=4",
		"CREATE TABLE tbl_handle(base INTEGER PRIMARY KEY AUTOINCREMENT CHECK "
			"(base<67108864), mask INTEGER, handle INTEGER, uniqueId INTEGER, "
			"parentUid INTEGER);",
		"CREATE TRIGGER insert_handle AFTER INSERT ON tbl_handle BEGIN UPDATE tbl_handle "
			"SET handle = new.base | new.mask where base = new.base;END;",
		"CREATE INDEX index_handle ON tbl_handle(handle);",
		"CREATE INDEX index_uniqueId ON tbl_handle(uniqueId);",
		NULL,
	};
	int ret;

	if (objmap == NULL || objmap->db != NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	ret = sceKernelCreateLwMutex(&objmap->lock, "SceMtpObjmap",
		SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 1, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OMAP, 198, "CreateLwMutex failed: 0x%x\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	/* "": a temporary database of its own */
	ret = sqlite3_open_v2("", &db,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_OMAP, 204, "sqlite open failed: %d\n", ret);
		sceKernelDeleteLwMutex(&objmap->lock);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (mtpSqlExec(db, sqls) < 0) {
		sqlite3_close(db);
		sceKernelDeleteLwMutex(&objmap->lock);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	objmap->db = db;
	/* the logical storage's number in bits 27-28, the physical's in bits 29-30 */
	handleBase = ((storageId - 1) & 3) << 27;
	handleBase = (handleBase & ~(3 << 29)) | (((storageId >> 16) - 1) & 3) << 29;
	objmap->handleBase = handleBase;
	ret = sceKernelUnlockLwMutex(&objmap->lock, 1);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 222, "sceKernelUnlockLwMutex failed: 0x%x\n", ret);
	return 0;
}

int mtpObjmapDestroy(MtpObjmap *objmap)
{
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (objmap->db == NULL) {
		MTP_ERR(MTP_TAG_OMAP, 237, "DB is not initialized\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = sqlite3_close(objmap->db);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_OMAP, 243, "sqlite close failed: %d\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = sceKernelDeleteLwMutex(&objmap->lock);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 249, "sceKernelDeleteLwMutex failed: 0x%x\n", ret);
	objmap->db = NULL;
	return 0;
}

int mtpObjmapSetupHandles(MtpObjmap *objmap, MtpDB *db)
{
	sqlite3 *sqlite;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	sqlite = objmap->db;
	AutoLock lock(&objmap->lock);
	const char *begin[] = { "BEGIN TRANSACTION", NULL };

	ret = mtpSqlExec(sqlite, begin);
	if (ret < 0)
		return ret;
	ret = mtpDBSetupMtpHandlesList(db, objmap);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_OMAP, 274, "mtpDBSetupMtpHandlesList: 0x%x\n", ret);
		const char *end[] = { "END TRANSACTION", NULL };

		mtpSqlExec(sqlite, end);
		return ret;
	}
	const char *end[] = { "END TRANSACTION", NULL };
	return mtpSqlExec(sqlite, end);
}

int mtpObjmapUnsetupHandles(MtpObjmap *objmap)
{
	const char *sqls[] = { "DELETE FROM tbl_handle;", NULL };

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	return mtpSqlExec(objmap->db, sqls);
}

int mtpObjmapDeallocate(MtpObjmap *objmap, SceUInt32 uniqueId)
{
	MtpSqlBind bind = { MTP_SQL_BIND_INTEGER, { uniqueId }, sizeof(uniqueId) };
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int changes, step;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	stmt = NULL;
	AutoLock lock(&objmap->lock);
	db = objmap->db;
	ret = mtpSqlCreateStatement(db, "DELETE from tbl_handle WHERE uniqueId=?;", &stmt);
	if (ret != 0)
		return ret;
	ret = mtpSqlBindStatement(stmt, &bind, 1);
	if (ret < 0) {
		sqlite3_finalize(stmt);
		return ret;
	}
	do {
		step = sqlite3_step(stmt);
	} while (step == SQLITE_ROW);
	sqlite3_finalize(stmt);
	if (step != SQLITE_DONE) {
		MTP_ERR(MTP_TAG_OMAP, 332, "%s failed: %d, (%s)\n", "sqlite3_step", step,
			sqlite3_errmsg(db));
		return mtpSqlConvertError(step);
	}
	changes = sqlite3_changes(db);
	if (changes <= 0) {
		MTP_ERR(MTP_TAG_OMAP, 341, "UID 0x%x is not found (%d)\n", uniqueId, changes);
		return SCE_MTP_ERROR_NOT_FOUND;
	}
	return 0;
}

/* returns SQLITE_DONE once the object has its handle */
int mtpObjmapAllocate(MtpObjmap *objmap, SceUInt32 flags, SceUInt32 uniqueId, SceUInt32 parent,
	SceUInt32 *handle)
{
	SceUInt32 mask;
	sqlite3_stmt *stmt;
	sqlite3 *db;
	SceUInt32 base;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mask = flags | objmap->handleBase;
	MtpSqlBind bind[] = {
		{ MTP_SQL_BIND_INTEGER, { mask }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { uniqueId }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { parent }, sizeof(SceInt64) },
	};
	stmt = NULL;
	AutoLock lock(&objmap->lock);
	db = objmap->db;
	ret = mtpSqlCreateStatement(db,
		"INSERT INTO tbl_handle(mask, uniqueId, parentUid) VALUES (?, ?, ?);", &stmt);
	if (ret >= 0) {
		ret = mtpSqlBindStatement(stmt, bind, sizeof(bind) / sizeof(bind[0]));
		if (ret >= 0) {
			do {
				ret = sqlite3_step(stmt);
			} while (ret == SQLITE_ROW);
			if (ret == SQLITE_DONE) {
				base = sqlite3_last_insert_rowid(db);
				sqlite3_finalize(stmt);
				if (handle != NULL)
					*handle = base | mask;
			} else {
				MTP_ERR(MTP_TAG_OMAP, 390, "%s failed: %d, (%s)\n", "sqlite3_step",
					ret, sqlite3_errmsg(db));
				MTP_ERR(MTP_TAG_OMAP, 391,
					"Failed: mask 0x%x, uid 0x%x, pid 0x%x\n", mask, uniqueId,
					parent);
				ret = mtpSqlConvertError(ret);
				sqlite3_finalize(stmt);
			}
		} else {
			sqlite3_finalize(stmt);
		}
	}
	return ret;
}

int mtpObjmapConvertMtpHandleToUniqueId(MtpObjmap *objmap, SceUInt32 handle,
	SceUInt32 *uniqueId)
{
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	ret = findObjectId(objmap, "SELECT uniqueId from tbl_handle WHERE handle=?;", handle,
		uniqueId);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 433, "find failed: 0x%x, hdl 0x%x (%u)\n", ret, handle,
			handle);
	return ret;
}

int mtpObjmapConvertUniqueIdToMtpHandle(MtpObjmap *objmap, SceUInt32 uniqueId,
	SceUInt32 *handle)
{
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	ret = findObjectId(objmap, "SELECT handle from tbl_handle WHERE uniqueId=?;", uniqueId,
		handle);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 455, "find failed: 0x%x, uid 0x%x\n", ret, uniqueId);
	return ret;
}

int mtpObjmapGetParentMtpHandleFromUniqueId(MtpObjmap *objmap, SceUInt32 uniqueId,
	SceUInt32 *parent)
{
	SceUInt32 parentUid;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	ret = findObjectId(objmap, "SELECT parentUid from tbl_handle WHERE uniqueId=?;",
		uniqueId, &parentUid);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 477, "find failed: 0x%x, uid 0x%x\n", ret, uniqueId);
	if (ret == 0)
		ret = mtpObjmapConvertUniqueIdToMtpHandle(objmap, parentUid, parent);
	return ret;
}

int mtpObjmapGetParentMtpHandleFromMtpHandle(MtpObjmap *objmap, SceUInt32 handle,
	SceUInt32 *parent)
{
	SceUInt32 parentUid;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	ret = findObjectId(objmap, "SELECT parentUid from tbl_handle WHERE handle=?;", handle,
		&parentUid);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 501, "find failed: 0x%x, handle 0x%x\n", ret, handle);
	if (ret == 0)
		ret = mtpObjmapConvertUniqueIdToMtpHandle(objmap, parentUid, parent);
	return ret;
}

int mtpObjmapGetNumUniqueIds(MtpObjmap *objmap, SceUInt32 parent, SceUInt32 *num)
{
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	AutoLock lock(&objmap->lock);
	ret = findObjectId(objmap, "SELECT count(uniqueId) from tbl_handle WHERE parentUid=?;",
		parent, num);
	if (ret < 0)
		MTP_ERR(MTP_TAG_OMAP, 525, "find failed: 0x%x, pid 0x%x\n", ret, parent);
	return ret;
}

/* the unique ids of count objects in the folder parent from offset on; returns their bytes */
int mtpObjmapGetUniqueIds(MtpObjmap *objmap, SceUInt32 parent, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids)
{
	MtpSqlBind bind[] = {
		{ MTP_SQL_BIND_INTEGER, { 0 }, sizeof(SceUInt32) },
		{ MTP_SQL_BIND_INTEGER, { 0 }, sizeof(SceUInt32) },
		{ MTP_SQL_BIND_INTEGER, { 0 }, sizeof(SceUInt32) },
	};
	sqlite3_stmt *stmt;
	SceUInt32 value, i;
	sqlite3 *db;
	int step;
	int ret;

	if (objmap == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	bind[2].value.integer = offset;
	stmt = NULL;
	bind[1].value.integer = count;
	bind[0].value.integer = parent;
	AutoLock lock(&objmap->lock);
	db = objmap->db;
	ret = mtpSqlCreateStatement(db,
		"SELECT uniqueId from tbl_handle WHERE parentUid=? LIMIT ? OFFSET ?;", &stmt);
	if (ret != 0)
		return ret;
	ret = mtpSqlBindStatement(stmt, bind, sizeof(bind) / sizeof(bind[0]));
	if (ret < 0) {
		sqlite3_finalize(stmt);
		return ret;
	}
	value = 0;
	for (i = 0; (step = sqlite3_step(stmt)) == SQLITE_ROW; ) {
		if (i++ >= count) {
			MTP_ERR(MTP_TAG_OMAP, 569, "count idx is over than %u for key 0x%x\n", count,
				parent);
			break;
		}
		ret = getUInt32Result(stmt, &value);
		if (ret < 0) {
			sqlite3_finalize(stmt);
			goto out;
		}
		*ids++ = value;
	}
	sqlite3_finalize(stmt);
	if (step == SQLITE_DONE) {
		ret = i * sizeof(*ids);
	} else {
		MTP_ERR(MTP_TAG_OMAP, 581, "%s failed: %d, (%s)\n", "sqlite3_step", step,
			sqlite3_errmsg(db));
		ret = mtpSqlConvertError(step);
	}
out:
	return ret;
}

} /* extern "C" */
