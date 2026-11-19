/*
 * SceLibMtp: the databases (DB).
 *
 * Each logical storage keeps its objects in a database file under its root folder
 * (mtp/pspmtp.db). The calls here check the database and pass each request to what its
 * type does; only the SQLite database is left.
 */

#include <stddef.h>
#include <kernel/error.h>

#include "mtp_internal.h"

#define MTP_TAG_DB	"DB  :"

/* the folder, under a logical storage's root, that holds its database */
#define MTP_DB_FOLDER		"/mtp"

extern "C" {

/* by MTP_DB_TYPE_* */
static const MtpDBOperation *const s_dbOperation[] = {
	NULL,
	&dbSqliteOperation,
};

static inline const MtpDBOperation *_getDBOperation(int type)
{
	switch (type) {
	case MTP_DB_TYPE_NORMAL:
		MTP_ERR(MTP_TAG_DB, 89, "NORMAL DB is obsolete\n");
		break;
	case MTP_DB_TYPE_SQLITE:
		return s_dbOperation[MTP_DB_TYPE_SQLITE];
	}
	return NULL;
}

int mtpDBOpen(MtpDB *db, int create)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL) {
		MTP_ERR(MTP_TAG_DB, 403, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelCreateMutex("SceMtpDB", 0, 0, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_DB, 412, "unable to create mutex for DB=0x%x\n", ret);
		return ret;
	}
	db->mutex = ret;
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 420, "Not supported Database type=%x\n", db->type);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto error;
	}
	if (op->open == NULL) {
		MTP_ERR(MTP_TAG_DB, 431, "open operation is not supported\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->open(db, create);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_DB, 427, "unable to open database=0x%x\n", ret);
		goto error;
	}
	return ret;
error:
	sceKernelDeleteMutex(db->mutex);
	return ret;
}

/* the database's file: <mount point><root folder>/mtp/pspmtp.db */
static inline int _buildDBDirectory(MtpLogicalStorage *storage, MtpDB *db)
{
	char *path = db->path;
	int ret;

	if (path == NULL) {
		MTP_ERR(MTP_TAG_DB, 116, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpUtilStrcpy(path, storage->info->name);
	if (storage->rootFolder != NULL && mtpUtilStrlen(storage->rootFolder) != 0)
		mtpUtilStrcat(path, storage->rootFolder);
	ret = 0;
	if (mtpFileExists(path) || (ret = mtpStrgSetupMtpRootFolder(storage)) >= 0) {
		mtpUtilStrcat(path, MTP_DB_FOLDER);
		if (mtpFileExists(path) || (ret = mtpFileCreate(path, MTP_FORMAT_ASSOCIATION)) >= 0)
			mtpUtilStrcat(path, "/");
		else
			MTP_ERR(MTP_TAG_DB, 137, "Create dir %s failed: 0x%x\n", path, ret);
	} else {
		MTP_ERR(MTP_TAG_DB, 128, "unable to setup MTP root folder\n");
	}
	if (ret >= 0) {
		if (storage->dbType == MTP_DB_TYPE_SQLITE)
			mtpUtilStrcat(path, "pspmtp.db");
		else
			mtpUtilStrcat(path, "pspmtp.dat");
	}
	return ret;
}

static __attribute__((noinline)) int mtpDBInitialize(MtpLogicalStorage *storage, MtpDB **db)
{
	MtpDB *d;
	int ret;

	if (storage == NULL || db == NULL) {
		MTP_ERR(MTP_TAG_DB, 324, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	d = (MtpDB *)mtpUtilMalloc(sizeof(*d));
	if (d == NULL) {
		MTP_ERR(MTP_TAG_DB, 332, "unable to alloc memory for DB\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	d->type = storage->dbType;
	d->sqlite = MTP_DB_NOT_OPEN;
	d->opened = NULL;
	ret = _buildDBDirectory(storage, d);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_DB, 344, "unable to build db name=0x%x\n", ret);
	} else {
		ret = mtpDBOpen(d, 1);
		if (ret >= 0) {
			*db = d;
			return ret;
		}
		MTP_ERR(MTP_TAG_DB, 351, "unable to open database=0x%x\n", ret);
	}
	mtpUtilFree(d);
	*db = NULL;
	return ret;
}

static inline int mtpDBUninitialize(MtpDB **db)
{
	int ret;

	if (db == NULL || *db == NULL || (*db)->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 376, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = mtpDBClose(*db);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 383, "unable to close database=0x%x\n", ret);
	mtpUtilFree(*db);
	*db = NULL;
	return ret;
}

int mtpDBClose(MtpDB *db)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL) {
		MTP_ERR(MTP_TAG_DB, 453, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 460, "Not supported Database type=%x\n", db->type);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
	} else if (op->close == NULL) {
		MTP_ERR(MTP_TAG_DB, 471, "close operation is not supported\n");
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
	} else {
		ret = op->close(db);
		if (ret < 0)
			MTP_ERR(MTP_TAG_DB, 468, "unable to close database=0x%x\n", ret);
	}
	sceKernelDeleteMutex(db->mutex);
	return ret;
}

int mtpDBFormat(MtpDB *db, SceUInt32 maxObjects)
{
	const MtpDBOperation *op;
	int ret = 0;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 491, "invalid parameter\n");
		return ret;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 497, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->format == NULL) {
		MTP_ERR(MTP_TAG_DB, 509, "Format is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->format(db, maxObjects);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 505, "unable to format database=0x%x\n", ret);
	return ret;
}

int mtpDBGetUniqueIdInfo(MtpDB *db, SceUInt32 *numObjects)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 533, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 540, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->getUniqueIdInfo == NULL) {
		MTP_ERR(MTP_TAG_DB, 552, "GetHandleInfo is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->getUniqueIdInfo(db, numObjects);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 548, "unable to get handle info from database=0x%x\n", ret);
	return ret;
}

int mtpDBRegisterObject(MtpDB *db, ObjectDataBasic *object)
{
	const MtpDBOperation *op;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN || object == NULL) {
		MTP_ERR(MTP_TAG_DB, 577, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 584, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->registerObject == NULL) {
		MTP_ERR(MTP_TAG_DB, 596, "RegisterObject is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return op->registerObject(db, object);
}

int mtpDBUnregisterObject(MtpDB *db, SceUInt32 uniqueId)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 621, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 628, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->unregisterObject == NULL) {
		MTP_ERR(MTP_TAG_DB, 640, "UnregisterObject is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->unregisterObject(db, uniqueId);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 636, "unable to unregister object=0x%x\n", ret);
	return ret;
}

int mtpDBGetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type, void *buf, SceSize size)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 668, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 675, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->getObjectProperty == NULL) {
		MTP_ERR(MTP_TAG_DB, 690, "GetObjectProperty is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->getObjectProperty(db, uniqueId, type, buf, size);
	if (ret < 0 && ret != SCE_MTP_ERROR_NO_SUCH_OBJECT)
		MTP_ERR(MTP_TAG_DB, 685, "unable to get object property=0x%x\n", ret);
	return ret;
}

int mtpDBSetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type, const void *buf,
	SceSize size)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DB, 717, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 724, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->setObjectProperty == NULL) {
		MTP_ERR(MTP_TAG_DB, 736, "SetObjectProperty is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->setObjectProperty(db, uniqueId, type, buf, size);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 732, "unable to get object property=0x%x\n", ret);
	return ret;
}

#define MTP_DB_PROPERTY(field) \
	*offset = offsetof(ObjectData, field); \
	*size = sizeof(((ObjectData *)0)->field); \
	break

int mtpDBGetObjectPropertyOffset(int type, SceUInt32 *offset, SceUInt32 *size)
{
	int ret = 0;

	switch (type) {
	case MTP_DB_QUERY_UID:
		MTP_DB_PROPERTY(basic.uid);
	case MTP_DB_QUERY_PARENT_UID:
		MTP_DB_PROPERTY(basic.parentUid);
	case MTP_DB_QUERY_OBJECT_FORMAT:
		MTP_DB_PROPERTY(basic.objectFormat);
	case MTP_DB_QUERY_PROTECTION_STATUS:
		MTP_DB_PROPERTY(basic.protectionStatus);
	case MTP_DB_QUERY_OBJECT_SIZE:
		MTP_DB_PROPERTY(basic.objectSize);
	case MTP_DB_QUERY_DATE_CREATED:
		MTP_DB_PROPERTY(basic.dateCreated);
	case MTP_DB_QUERY_DATE_MODIFIED:
		MTP_DB_PROPERTY(basic.dateModified);
	case MTP_DB_QUERY_FULL_PATH_NAME:
		MTP_DB_PROPERTY(basic.fullPathName);
	case MTP_DB_QUERY_NON_CONSUMABLE:
		MTP_DB_PROPERTY(basic.nonConsumable);
	case MTP_DB_QUERY_NAME:
		MTP_DB_PROPERTY(basic.name);
	case MTP_DB_QUERY_PUOID:
		MTP_DB_PROPERTY(basic.puoid);
	case MTP_DB_QUERY_ARTIST:
		MTP_DB_PROPERTY(extension.artist);
	case MTP_DB_QUERY_GENRE:
		MTP_DB_PROPERTY(extension.genre);
	case MTP_DB_QUERY_ALBUM:
		MTP_DB_PROPERTY(extension.album);
	case MTP_DB_QUERY_ALBUM_ARTIST:
		MTP_DB_PROPERTY(extension.albumArtist);
	case MTP_DB_QUERY_USE_COUNT:
		MTP_DB_PROPERTY(extension.useCount);
	case MTP_DB_QUERY_SAMPLE_RATE:
		MTP_DB_PROPERTY(extension.sampleRate);
	case MTP_DB_QUERY_AUDIO_WAVE_CODEC:
		MTP_DB_PROPERTY(extension.audioWaveCodec);
	case MTP_DB_QUERY_AUDIO_BIT_RATE:
		MTP_DB_PROPERTY(extension.audioBitRate);
	case MTP_DB_QUERY_ORIGINAL_RELEASE_DATE:
		MTP_DB_PROPERTY(extension.originalReleaseDate);
	case MTP_DB_QUERY_DURATION:
		MTP_DB_PROPERTY(extension.duration);
	case MTP_DB_QUERY_NUMBER_OF_CHANNELS:
		MTP_DB_PROPERTY(extension.numberOfChannels);
	case MTP_DB_QUERY_RATING:
		MTP_DB_PROPERTY(extension.rating);
	case MTP_DB_QUERY_BITRATE_TYPE:
		MTP_DB_PROPERTY(extension.bitrateType);
	case MTP_DB_QUERY_DRM_STATUS:
		MTP_DB_PROPERTY(extension.drmStatus);
	case MTP_DB_QUERY_TRACK:
	case MTP_DB_QUERY_SAMPLE_FORMAT:
		MTP_DB_PROPERTY(extension.track);
	case MTP_DB_QUERY_BUYFLAG:
		MTP_DB_PROPERTY(extension.buyflag);
	case MTP_DB_QUERY_AUDIO_BIT_DEPTH:
		MTP_DB_PROPERTY(extension.audioBitDepth);
	case MTP_DB_QUERY_WIDTH:
		MTP_DB_PROPERTY(extension.width);
	case MTP_DB_QUERY_HEIGHT:
		MTP_DB_PROPERTY(extension.height);
	case MTP_DB_QUERY_OBJECT_DATA_BASIC:
		*offset = 0;
		*size = sizeof(ObjectDataBasic);
		break;
	case MTP_DB_QUERY_OBJECT_DATA_EXTENSION:
		*offset = offsetof(ObjectData, extension);
		*size = sizeof(ObjectDataExtension);
		break;
	case MTP_DB_QUERY_OBJECT_DATA:
		*offset = 0;
		*size = sizeof(ObjectData);
		break;
	default:
		MTP_ERR(MTP_TAG_DB, 933, "unknown type=%d", type);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		break;
	}
	return ret;
}

#undef MTP_DB_PROPERTY

int mtpDBGetNumObjectsByType(MtpDB *db, int type, const void *value, SceUInt32 *num)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN || num == NULL) {
		MTP_ERR(MTP_TAG_DB, 1048, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 1055, "not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->getNumObjectsByType == NULL) {
		MTP_ERR(MTP_TAG_DB, 1067, "GetNumObjectsByType is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->getNumObjectsByType(db, type, value, num);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 1063, "unable to get num objects=0x%x\n", ret);
	return ret;
}

int mtpDBGetObjectUniqueIdsByType(MtpDB *db, int type, const void *value, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN || value == NULL || ids == NULL) {
		MTP_ERR(MTP_TAG_DB, 1145, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 1152, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->getObjectUniqueIdsByType == NULL) {
		MTP_ERR(MTP_TAG_DB, 1167, "GetObjectHandlesByType is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->getObjectUniqueIdsByType(db, type, value, offset, count, ids);
	if (ret < 0 && ret != SCE_MTP_ERROR_NOT_FOUND)
		MTP_ERR(MTP_TAG_DB, 1162, "unable to get object handles=0x%x\n", ret);
	return ret;
}

int mtpDBSetupMtpHandlesList(MtpDB *db, MtpObjmap *objmap)
{
	const MtpDBOperation *op;
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN || objmap == NULL || objmap->magic == 0) {
		MTP_ERR(MTP_TAG_DB, 1191, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	op = _getDBOperation(db->type);
	if (op == NULL) {
		MTP_ERR(MTP_TAG_DB, 1198, "Not supported Database type=%x\n", db->type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (op->setupMtpHandlesList == NULL) {
		MTP_ERR(MTP_TAG_DB, 1210, "SetupMtpHandlesListFunc is not implemented\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	ret = op->setupMtpHandlesList(db, objmap);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DB, 1206, "unable to setup MTP handle list=0x%x\n", ret);
	return ret;
}

int mtpDBObjectPropCode2QueryType(SceUInt16 code)
{
	int type = -1;

	switch (code) {
	case MTP_OBJECT_PROP_STORAGE_ID:
		return type;
	case MTP_OBJECT_PROP_OBJECT_FORMAT:
		return MTP_DB_QUERY_OBJECT_FORMAT;
	case MTP_OBJECT_PROP_PROTECTION_STATUS:
		return MTP_DB_QUERY_PROTECTION_STATUS;
	case MTP_OBJECT_PROP_OBJECT_SIZE:
		return MTP_DB_QUERY_OBJECT_SIZE;
	case MTP_OBJECT_PROP_OBJECT_FILE_NAME:
		return MTP_DB_QUERY_FULL_PATH_NAME;
	case MTP_OBJECT_PROP_DATE_CREATED:
		return MTP_DB_QUERY_DATE_CREATED;
	case MTP_OBJECT_PROP_DATE_MODIFIED:
		return MTP_DB_QUERY_DATE_MODIFIED;
	case MTP_OBJECT_PROP_PARENT_OBJECT:
		return MTP_DB_QUERY_PARENT_UID;
	case MTP_OBJECT_PROP_PERSISTENT_UID:
		return MTP_DB_QUERY_PUOID;
	case MTP_OBJECT_PROP_NAME:
		return MTP_DB_QUERY_NAME;
	case MTP_OBJECT_PROP_ARTIST:
		return MTP_DB_QUERY_ARTIST;
	case MTP_OBJECT_PROP_NON_CONSUMABLE:
		return MTP_DB_QUERY_NON_CONSUMABLE;
	case MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_FORMAT:
		type = MTP_DB_QUERY_SAMPLE_FORMAT;
		break;
	case MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_HEIGHT:
	case MTP_OBJECT_PROP_HEIGHT:
		type = MTP_DB_QUERY_HEIGHT;
		break;
	case MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_WIDTH:
	case MTP_OBJECT_PROP_WIDTH:
		type = MTP_DB_QUERY_WIDTH;
		break;
	case MTP_OBJECT_PROP_DURATION:
		type = MTP_DB_QUERY_DURATION;
		break;
	case MTP_OBJECT_PROP_RATING:
		type = MTP_DB_QUERY_RATING;
		break;
	case MTP_OBJECT_PROP_TRACK:
		type = MTP_DB_QUERY_TRACK;
		break;
	case MTP_OBJECT_PROP_GENRE:
		type = MTP_DB_QUERY_GENRE;
		break;
	case MTP_OBJECT_PROP_USE_COUNT:
		type = MTP_DB_QUERY_USE_COUNT;
		break;
	case MTP_OBJECT_PROP_ORIGINAL_RELEASE_DATE:
		type = MTP_DB_QUERY_ORIGINAL_RELEASE_DATE;
		break;
	case MTP_OBJECT_PROP_ALBUM_NAME:
		type = MTP_DB_QUERY_ALBUM;
		break;
	case MTP_OBJECT_PROP_ALBUM_ARTIST:
		type = MTP_DB_QUERY_ALBUM_ARTIST;
		break;
	case MTP_OBJECT_PROP_DRM_STATUS:
		type = MTP_DB_QUERY_DRM_STATUS;
		break;
	case MTP_OBJECT_PROP_BITRATE_TYPE:
		type = MTP_DB_QUERY_BITRATE_TYPE;
		break;
	case MTP_OBJECT_PROP_SAMPLE_RATE:
		type = MTP_DB_QUERY_SAMPLE_RATE;
		break;
	case MTP_OBJECT_PROP_NUMBER_OF_CHANNELS:
		type = MTP_DB_QUERY_NUMBER_OF_CHANNELS;
		break;
	case MTP_OBJECT_PROP_AUDIO_BIT_DEPTH:
		type = MTP_DB_QUERY_AUDIO_BIT_DEPTH;
		break;
	case MTP_OBJECT_PROP_AUDIO_WAVE_CODEC:
		type = MTP_DB_QUERY_AUDIO_WAVE_CODEC;
		break;
	case MTP_OBJECT_PROP_AUDIO_BITRATE:
		type = MTP_DB_QUERY_AUDIO_BIT_RATE;
		break;
	case MTP_OBJECT_PROP_SCE_BUYFLAG:
		return MTP_DB_QUERY_BUYFLAG;
	default:
		MTP_ERR(MTP_TAG_DB, 1328, "unknown object prop code=%04x\n", code);
		break;
	}
	return type;
}

void mtpDBFixToObjectPropValue(int type, const void *src, void *dst, SceSize size)
{
	SceDateTime time;
	const char *name;
	char *out;

	switch (type) {
	case MTP_DB_QUERY_UID:
	case MTP_DB_QUERY_PARENT_UID:
		MTP_ERR(MTP_TAG_DB, 1353, "this code mustn't be executed\n");
		break;
	case MTP_DB_QUERY_DATE_CREATED:
	case MTP_DB_QUERY_DATE_MODIFIED:
	case MTP_DB_QUERY_ORIGINAL_RELEASE_DATE:
		mtpUtilMemcpy(&time, src, sizeof(time));
		mtpUtilDateTimeToString((char *)dst, &time);
		break;
	case MTP_DB_QUERY_FULL_PATH_NAME:
		/* the host sees the file's name */
		name = mtpUtilStrrchr((const char *)src, '/');
		if (name != NULL)
			name++;
		else
			name = (const char *)src;
		out = (char *)dst;
		while (*name != '\0')
			*out++ = *name++;
		*out = '\0';
		break;
	default:
		break;
	}
}

void mtpDBFixFromObjectPropValue(int type, const void *src, void *dst, SceSize size)
{
	SceDateTime time;

	switch (type) {
	case MTP_DB_QUERY_UID:
	case MTP_DB_QUERY_PARENT_UID:
	case MTP_DB_QUERY_DATE_CREATED:
	case MTP_DB_QUERY_DATE_MODIFIED:
	case MTP_DB_QUERY_FULL_PATH_NAME:
		break;
	case MTP_DB_QUERY_ORIGINAL_RELEASE_DATE:
		mtpUtilStringToDateTime((const char *)src, &time);
		mtpUtilMemcpy(dst, &time, sizeof(time));
		break;
	default:
		break;
	}
}

int mtpDBInitGenericWay(MtpStorageInfo *info, SceUInt32 cacheSize)
{
	MtpLogicalStorage *storage;
	SceUInt32 storageId;
	SceUInt32 i;
	SceUInt32 j;
	int ret = 0;

	if (info == NULL) {
		MTP_ERR(MTP_TAG_DB, 1439, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	for (i = 0; i < info->numLogicalStorages; i++) {
		storage = &info->logicalStorages[i];
		storage->info = info;
		storage->failed = 0;
		storageId = storage->index | (SceUInt32)info->index << 16;
		ret = mtpStrgSetupMtpRootFolder(storage);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_DB, 1457, "unable to setup MTP root folder\n");
			storage->failed |= 1;
			return ret;
		}
		ret = mtpCacheInitialize(storage, &storage->cache, cacheSize);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_DB, 1464,
				"unable to initialize cache for this storage %d\n", i);
			storage->failed |= 1;
			return ret;
		}
		ret = mtpDBInitialize(storage, &storage->db);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_DB, 1471,
				"unable to initialize DB for MS storage logical %d\n", i);
			storage->failed |= 1;
			return ret;
		}
		ret = mtpStrgUpdateLogicalInfo(storage);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_DB, 1478,
				"unable to update Storage Info using Database=0x%x\n", ret);
			goto error;
		}
		if (storage->numObjects == 0) {
			/* a new database: its root object first */
			ret = mtpDBFormat(storage->db, storage->maxObjects);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_DB, 1487, "unable to format database=0x%x\n", ret);
				goto error;
			}
			ret = mtpObjAddRootObject(storage);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_DB, 1493, "unable to add Root Object=0x%x\n", ret);
				goto error;
			}
		}
		ret = mtpStrgVTSetupMtpHandles(storageId);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_DB, 1500, "unable to setup VT Mtp Handle list=0x%x\n", ret);
			goto error;
		}
	}
	return ret;
error:
	for (j = 0; j < i; j++)
		mtpDBUninitialize(&info->logicalStorages[j].db);
	return ret;
}

int mtpDBUninitGenericWay(MtpStorageInfo *info)
{
	MtpLogicalStorage *storage;
	SceUInt32 i;
	int ret = 0;
	int err;

	for (i = 0; i < info->numLogicalStorages; i++) {
		err = mtpStrgVTUnsetupMtpHandles(info->logicalStorages[i].index |
			(SceUInt32)info->index << 16);
		if (err < 0)
			MTP_ERR(MTP_TAG_DB, 1553, "unable to unsetup VT MTP Handle list=0x%x\n", err);
		storage = &info->logicalStorages[i];
		err = mtpDBUninitialize(&storage->db);
		if (err < 0)
			MTP_ERR(MTP_TAG_DB, 1557, "database uninitialize failed=0x%x(logi=%d)\n",
				err, i);
		ret = mtpCacheUninitialize(&info->logicalStorages[i].cache);
		if (ret < 0)
			MTP_ERR(MTP_TAG_DB, 1561, "cache uninitialize failed=0x%x(logi=%d)\n", ret,
				i);
	}
	return ret;
}

int mtpDBLock(MtpDB *db)
{
	int ret;

	if (db == NULL) {
		MTP_ERR(MTP_TAG_DB, 1654, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelLockMutex(db->mutex, 1, NULL);
	if (ret != SCE_OK)
		MTP_ERR(MTP_TAG_DB, 1661, "unable to lock mutex for DB %x\n", ret);
	return ret;
}

int mtpDBUnlock(MtpDB *db)
{
	int ret;

	if (db == NULL) {
		MTP_ERR(MTP_TAG_DB, 1674, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelUnlockMutex(db->mutex, 1);
	if (ret != SCE_OK && ret != SCE_KERNEL_ERROR_MUTEX_NOT_OWNED)
		MTP_ERR(MTP_TAG_DB, 1680, "unable to unlock mutex for DB %x\n", ret);
	return ret;
}

} /* extern "C" */
