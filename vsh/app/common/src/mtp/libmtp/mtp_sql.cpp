/*
 * SceLibMtp: the SQLite calls (SQL) the databases and the handle maps share.
 *
 * Every SQLite result code becomes an MTP error: a database file SQLite cannot read tells
 * the callbacks (event 0x100) and is SCE_MTP_ERROR_DATABASE_BROKEN, a constraint (a path
 * already there) SCE_MTP_ERROR_OBJECT_EXISTS, and any other code SCE_MTP_ERROR_SQLITE.
 */

#include <sqlite3.h>

#include "mtp_internal.h"

#define MTP_TAG_SQL	" SQL:"

/* the callbacks' event for a database SQLite cannot read */
#define MTP_SQL_EVENT_DATABASE_BROKEN	0x100

extern "C" {

static int _execCallback(void *arg, int numColumns, char **values, char **names);

int mtpSqlConvertError(int err)
{
	switch ((SceUInt8)err) {
	case SQLITE_IOERR:
	case SQLITE_CORRUPT:
	case SQLITE_CANTOPEN:
	case SQLITE_EMPTY:
	case SQLITE_FORMAT:
	case SQLITE_NOTADB:
		mtpCallbackNotify(MTP_SQL_EVENT_DATABASE_BROKEN);
		return SCE_MTP_ERROR_DATABASE_BROKEN;
	case SQLITE_CONSTRAINT:
		return SCE_MTP_ERROR_OBJECT_EXISTS;
	default:
		if (err != SQLITE_OK)
			err = SCE_MTP_ERROR_SQLITE(err);
		return err;
	}
}

int mtpSqlCreateStatement(sqlite3 *db, const char *sql, sqlite3_stmt **stmt)
{
	int ret;

	ret = sqlite3_prepare_v2(db, sql, mtpUtilStrlen(sql), stmt, NULL);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_SQL, 46, "%s failed: %d, (%s)\n", "sqlite3_prepare_v2", ret,
			sqlite3_errmsg(db));
		return mtpSqlConvertError(ret);
	}
	ret = sqlite3_reset(*stmt);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_SQL, 51, "%s failed: %d, (%s)\n", "sqlite3_reset", ret,
			sqlite3_errmsg(db));
		sqlite3_finalize(*stmt);
		return mtpSqlConvertError(ret);
	}
	return 0;
}

int mtpSqlBindStatement(sqlite3_stmt *stmt, const MtpSqlBind *bind, int num)
{
	void *text;
	int ret;
	int i;

	ret = sqlite3_reset(stmt);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_SQL, 66, "sqlite3_reset failed: %d\n", ret);
		return mtpSqlConvertError(ret);
	}
	for (i = 0; i < num; i++, bind++) {
		switch (bind->type) {
		case MTP_SQL_BIND_INTEGER:
			ret = sqlite3_bind_int64(stmt, i + 1, bind->value.integer);
			break;
		case MTP_SQL_BIND_TEXT:
			if (bind->size == 0) {
				ret = sqlite3_bind_text(stmt, i + 1, NULL, 0, NULL);
				break;
			}
			text = sqlite3_malloc(bind->size);
			if (text == NULL) {
				ret = SCE_MTP_ERROR_NO_MEMORY;
				break;
			}
			mtpUtilMemcpy(text, bind->value.data, bind->size);
			ret = sqlite3_bind_text(stmt, i + 1, (const char *)text, bind->size,
				sqlite3_free);
			break;
		default:
			MTP_ERR(MTP_TAG_SQL, 92, "wrong type: %d\n", bind->type);
			ret = SCE_MTP_ERROR_NOT_INITIALIZED;
			break;
		}
		if (ret != SQLITE_OK)
			break;
	}
	return ret;
}

int mtpSqlExec(sqlite3 *db, const char *const *sqls)
{
	char *errmsg = NULL;
	int ret = SQLITE_OK;
	int i;

	for (i = 0; sqls[i] != NULL; i++) {
		ret = sqlite3_exec(db, sqls[i], _execCallback, NULL, &errmsg);
		if (ret != SQLITE_OK) {
			MTP_ERR(MTP_TAG_SQL, 108, "Exec [%d] failed: %d, error: %s\n", i, ret,
				errmsg);
			sqlite3_free(errmsg);
			break;
		}
	}
	return mtpSqlConvertError(ret);
}

/* the statements mtpSqlExec runs return no rows it reads */
static int _execCallback(void *arg, int numColumns, char **values, char **names)
{
	return 0;
}

} /* extern "C" */
