/*
 * SceLibMtp: the SQLite database (DBSQ), what mtpDB* passes a MTP_DB_TYPE_SQLITE database's
 * requests to.
 *
 * A logical storage's objects are the rows of one table, tbl_mtpObject, a column per
 * property (s_columnName, by MTP_DB_QUERY_*). A query reads or writes one column, the
 * columns of ObjectDataBasic, the ones after them (ObjectDataExtension) or the whole row.
 * Dates are kept as the text mtpUtilDateTimeToString writes.
 */

#include <sqlite3.h>

#include "mtp_internal.h"

#define MTP_TAG_DBSQ	"DBSQ:"

/* a date column's text (mtpUtilDateTimeToString), and the buffer it is read into */
#define MTP_DBSQ_DATE_LENGTH	18
#define MTP_DBSQ_DATE_SIZE	20

/* the columns of ObjectDataBasic, and of ObjectDataExtension after them */
#define MTP_DBSQ_NUM_BASIC_COLUMNS	MTP_DB_QUERY_ARTIST
#define MTP_DBSQ_NUM_COLUMNS		MTP_DB_QUERY_OBJECT_DATA_BASIC
#define MTP_DBSQ_NUM_EXTENSION_COLUMNS	(MTP_DBSQ_NUM_COLUMNS - MTP_DBSQ_NUM_BASIC_COLUMNS)

/* the most values a statement binds */
#define MTP_DBSQ_MAX_BIND	32

/* tbl_mtpObject's columns, by MTP_DB_QUERY_* */
static const char *s_columnName[MTP_DB_QUERY_OBJECT_DATA + 1] = {
	"UID",
	"ParentUID",
	"ObjectFormat",
	"ProtectionStatus",
	"ObjectSize",
	"DateCreated",
	"DateModified",
	"FullPathName",
	"NonConsumable",
	"Name",
	"PUOID",
	"Artist",
	"Genre",
	"Album",
	"AlbumArtist",
	"UseCount",
	"SampleRate",
	"AudioWaveCodec",
	"AudioBitRate",
	"OriginalReleaseDate",
	"Duration",
	"NumberOfChannels",
	"Rating",
	"BitrateType",
	"DrmStatus",
	"Track",
	"SampleFormat",
	"Buyflag",
	"AudioBitDepth",
	"Width",
	"Height",
};

static inline int _getObjectPropertySize(int type, SceUInt32 *size)
{
	SceUInt32 offset = 0;
	int ret;

	ret = mtpDBGetObjectPropertyOffset(type, &offset, size);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_DBSQ, 95, "GetObjectPropertyOffset %d failed: 0x%x\n", type, ret);
		return ret;
	}
	return 0;
}

/* a caller's buffer of size bytes holds the property */
static inline int _checkObjectProperty(int type, SceSize size)
{
	SceUInt32 propSize = 0;
	int ret;

	ret = _getObjectPropertySize(type, &propSize);
	if (ret < 0)
		return ret;
	if (size < propSize) {
		MTP_ERR(MTP_TAG_DBSQ, 113, "Specified size %u is too small for %u\n", size,
			propSize);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return 0;
}

/* the columns a query of type reads or writes, [*first, *last) */
static inline void _getQueryColumns(int type, int *first, int *last)
{
	if (type == MTP_DB_QUERY_OBJECT_DATA_BASIC) {
		*first = 0;
		*last = MTP_DBSQ_NUM_BASIC_COLUMNS;
	} else if (type == MTP_DB_QUERY_OBJECT_DATA_EXTENSION) {
		*first = MTP_DBSQ_NUM_BASIC_COLUMNS;
		*last = MTP_DBSQ_NUM_COLUMNS;
	} else if (type == MTP_DB_QUERY_OBJECT_DATA) {
		*first = 0;
		*last = MTP_DBSQ_NUM_COLUMNS;
	} else {
		*first = type;
		*last = type + 1;
	}
}

/* the separator after the last column written at column: a space */
static inline char *_endColumnList(char *column)
{
	char *p;

	for (p = column; *p++ != '\0'; ) {
		if (*p == ',') {
			*p++ = ' ';
			break;
		}
	}
	return p;
}

/* SELECT <type's columns> from tbl_mtpObject WHERE UID=<uniqueId>; */
static inline int _createGetQueryStatement(sqlite3 *db, SceUInt32 uniqueId, int type,
	sqlite3_stmt **stmt)
{
	const char *format = "SELECT from tbl_mtpObject WHERE UID=;";
	SceSize size, rest, length;
	int first, last, i;
	char *sql, *p;
	int ret;

	if (type > MTP_DB_QUERY_OBJECT_DATA) {
		MTP_ERR(MTP_TAG_DBSQ, 169, "wrong type: %d\n", type);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	_getQueryColumns(type, &first, &last);
	size = 0;
	for (i = first; i < last; i++)
		size += mtpUtilStrlen(s_columnName[i]) + 2;
	size += mtpUtilStrlen(format) + 10;
	sql = (char *)mtpUtilMalloc(size);
	if (sql == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 198, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(sql, "SELECT ");
	p = sql;
	rest = size;
	for (i = first; i < last; i++) {
		length = mtpUtilStrlen(p);
		if (length > rest) {
			MTP_ERR(MTP_TAG_DBSQ, 208, "wrong size %u < %u (%d)\n", rest, length, i);
			mtpUtilFree(sql);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		p += length;
		rest -= length;
		mtpUtilStrcpy(p, s_columnName[i]);
		mtpUtilStrcat(p, ", ");
	}
	p = _endColumnList(p);
	sceClibSnprintf(p, size - (p - sql), "from tbl_mtpObject WHERE UID=%llu;",
		(SceUInt64)uniqueId);
	ret = mtpSqlCreateStatement(db, sql, stmt);
	mtpUtilFree(sql);
	return ret;
}

static void _getIntegerResult(sqlite3_stmt *stmt, int column, void *value, SceSize size)
{
	SceInt64 result = sqlite3_column_int64(stmt, column);

	switch (size) {
	case 1:
		*(SceUInt8 *)value = result;
		break;
	case 2:
		*(SceUInt16 *)value = result;
		break;
	case 4:
		*(SceUInt32 *)value = result;
		break;
	case 8:
		*(SceInt64 *)value = result;
		break;
	default:
		MTP_ERR(MTP_TAG_DBSQ, 256, "Warning unknown integer size: %u\n", size);
		break;
	}
}

static void _setIntegerData(const void *value, SceSize size, MtpSqlBind *bind)
{
	SceInt64 data;

	switch (size) {
	case 1:
		data = *(const SceUInt8 *)value;
		break;
	case 2:
		data = *(const SceUInt16 *)value;
		break;
	case 4:
		data = *(const SceUInt32 *)value;
		break;
	case 8:
		data = *(const SceInt64 *)value;
		break;
	default:
		MTP_ERR(MTP_TAG_DBSQ, 278, "Warning unknown integer size: %u\n", size);
		return;
	}
	bind->value.integer = data;
	bind->type = MTP_SQL_BIND_INTEGER;
	bind->size = sizeof(SceInt64);
}

/* UPDATE tbl_mtpObject SET <each of type's columns>=? WHERE UID=?; */
static inline int _createSetQueryStatement(sqlite3 *db, int type, sqlite3_stmt **stmt)
{
	const char *format = "UPDATE tbl_mtpObject SET WHERE UID=?;";
	SceSize size, length;
	int first, last, i;
	char *sql, *p;
	int ret;

	if (type > MTP_DB_QUERY_OBJECT_DATA) {
		MTP_ERR(MTP_TAG_DBSQ, 316, "wrong type: %d\n", type);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	_getQueryColumns(type, &first, &last);
	size = 0;
	for (i = first; i < last; i++)
		size += mtpUtilStrlen(s_columnName[i]) + 4;
	size += mtpUtilStrlen(format);
	sql = (char *)mtpUtilMalloc(size);
	if (sql == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 344, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(sql, "UPDATE tbl_mtpObject SET ");
	p = sql;
	for (i = first; i < last; i++) {
		length = mtpUtilStrlen(p);
		if (length > size) {
			MTP_ERR(MTP_TAG_DBSQ, 354, "wrong size %u < %u (%d)\n", size, length, i);
			mtpUtilFree(sql);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		p += length;
		size -= length;
		mtpUtilStrcpy(p, s_columnName[i]);
		mtpUtilStrcat(p, "=?, ");
	}
	p = _endColumnList(p);
	mtpUtilStrcpy(p, "WHERE UID=?;");
	ret = mtpSqlCreateStatement(db, sql, stmt);
	mtpUtilFree(sql);
	return ret;
}

/*
 * SELECT UID from tbl_mtpObject WHERE <type's column>=? (or, isNull, is NULL), then
 * suffix, or the end of the statement
 */
static int _createGetQueryByTypeStatement(int type, sqlite3 *db, const char *suffix,
	int isNull, sqlite3_stmt **stmt)
{
	SceSize size;
	char *sql, *p;
	int ret;

	if (type >= MTP_DB_QUERY_OBJECT_DATA_BASIC) {
		MTP_ERR(MTP_TAG_DBSQ, 391, "not supported type: %d\n", type);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	size = mtpUtilStrlen("SELECT UID from tbl_mtpObject WHERE %s=?%s") +
		mtpUtilStrlen(s_columnName[type]);
	if (suffix != NULL)
		size += mtpUtilStrlen(suffix);
	if (isNull)
		size += 4;
	sql = (char *)mtpUtilMalloc(size);
	if (sql == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 405, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilStrcpy(sql, "SELECT UID from tbl_mtpObject WHERE ");
	mtpUtilStrcpy(sql + mtpUtilStrlen(sql), s_columnName[type]);
	p = sql + mtpUtilStrlen(sql);
	if (suffix != NULL) {
		if (isNull)
			mtpUtilStrcpy(p, " is NULL");
		else
			mtpUtilStrcpy(p, "=?");
		mtpUtilStrcpy(sql + mtpUtilStrlen(sql), suffix);
	} else if (isNull) {
		mtpUtilStrcpy(p, " is NULL;");
	} else {
		mtpUtilStrcpy(p, "=?;");
	}
	ret = mtpSqlCreateStatement(db, sql, stmt);
	mtpUtilFree(sql);
	return ret;
}

/* ObjectDataBasic's columns, the dates left for the caller (bind[5], bind[6]) */
static void _bindObjectDataBasic(const ObjectDataBasic *object, MtpSqlBind *bind)
{
	_setIntegerData(&object->uid, sizeof(object->uid), &bind[0]);
	_setIntegerData(&object->parentUid, sizeof(object->parentUid), &bind[1]);
	_setIntegerData(&object->objectFormat, sizeof(object->objectFormat), &bind[2]);
	_setIntegerData(&object->protectionStatus, sizeof(object->protectionStatus),
		&bind[3]);
	_setIntegerData(&object->objectSize, sizeof(object->objectSize), &bind[4]);
	bind[5].type = MTP_SQL_BIND_TEXT;
	bind[5].value.data = NULL;
	bind[5].size = 0;
	bind[6].type = MTP_SQL_BIND_TEXT;
	bind[6].value.data = NULL;
	bind[6].size = 0;
	bind[7].type = MTP_SQL_BIND_TEXT;
	bind[7].value.data = object->fullPathName;
	bind[7].size = mtpUtilStrlen(object->fullPathName);
	_setIntegerData(&object->nonConsumable, sizeof(object->nonConsumable), &bind[8]);
	bind[9].type = MTP_SQL_BIND_TEXT;
	bind[9].value.data = object->name;
	bind[9].size = mtpUtilStrlen(object->name);
	bind[10].type = MTP_SQL_BIND_TEXT;
	bind[10].value.data = object->puoid;
	bind[10].size = sizeof(object->puoid);
}

/*
 * ObjectDataExtension's columns, the release date left for the caller (bind[8]);
 * SampleFormat is 0
 */
static void _bindObjectDataExtension(const ObjectDataExtension *object, MtpSqlBind *bind)
{
	bind[0].type = MTP_SQL_BIND_TEXT;
	bind[0].value.data = object->artist;
	bind[0].size = mtpUtilStrlen(object->artist);
	bind[1].type = MTP_SQL_BIND_TEXT;
	bind[1].value.data = object->genre;
	bind[1].size = mtpUtilStrlen(object->genre);
	bind[2].type = MTP_SQL_BIND_TEXT;
	bind[2].value.data = object->album;
	bind[2].size = mtpUtilStrlen(object->album);
	bind[3].type = MTP_SQL_BIND_TEXT;
	bind[3].value.data = object->albumArtist;
	bind[3].size = mtpUtilStrlen(object->albumArtist);
	_setIntegerData(&object->useCount, sizeof(object->useCount), &bind[4]);
	_setIntegerData(&object->sampleRate, sizeof(object->sampleRate), &bind[5]);
	_setIntegerData(&object->audioWaveCodec, sizeof(object->audioWaveCodec), &bind[6]);
	_setIntegerData(&object->audioBitRate, sizeof(object->audioBitRate), &bind[7]);
	bind[8].type = MTP_SQL_BIND_TEXT;
	bind[8].value.data = NULL;
	bind[8].size = 0;
	_setIntegerData(&object->duration, sizeof(object->duration), &bind[9]);
	_setIntegerData(&object->numberOfChannels, sizeof(object->numberOfChannels),
		&bind[10]);
	_setIntegerData(&object->rating, sizeof(object->rating), &bind[11]);
	_setIntegerData(&object->bitrateType, sizeof(object->bitrateType), &bind[12]);
	_setIntegerData(&object->drmStatus, sizeof(object->drmStatus), &bind[13]);
	_setIntegerData(&object->track, sizeof(object->track), &bind[14]);
	bind[15].type = MTP_SQL_BIND_INTEGER;
	bind[15].value.integer = 0;
	_setIntegerData(&object->buyflag, sizeof(object->buyflag), &bind[16]);
	_setIntegerData(&object->audioBitDepth, sizeof(object->audioBitDepth), &bind[17]);
	_setIntegerData(&object->width, sizeof(object->width), &bind[18]);
	_setIntegerData(&object->height, sizeof(object->height), &bind[19]);
}

/* binds value, a query of type's columns (size: a scalar's), then the extra values */
static int _bindSetQueryStatement(int type, sqlite3_stmt *stmt, const void *value,
	SceSize size, const MtpSqlBind *extra, int numExtra)
{
	MtpSqlBind bind[MTP_DBSQ_MAX_BIND];
	char date[MTP_DBSQ_DATE_SIZE];
	char dateModified[MTP_DBSQ_DATE_SIZE];
	char releaseDate[MTP_DBSQ_DATE_SIZE];
	const ObjectData *data = (const ObjectData *)value;
	SceSize length;
	int num, i;

	switch (type) {
	case MTP_DB_QUERY_UID:
	case MTP_DB_QUERY_PARENT_UID:
	case MTP_DB_QUERY_OBJECT_FORMAT:
	case MTP_DB_QUERY_PROTECTION_STATUS:
	case MTP_DB_QUERY_OBJECT_SIZE:
	case MTP_DB_QUERY_NON_CONSUMABLE:
		_setIntegerData(value, size, &bind[0]);
		num = 1;
		break;
	case MTP_DB_QUERY_DATE_CREATED:
	case MTP_DB_QUERY_DATE_MODIFIED:
	case MTP_DB_QUERY_ORIGINAL_RELEASE_DATE:
		mtpUtilDateTimeToString(date, (const SceDateTime *)value);
		bind[0].value.data = date;
		bind[0].type = MTP_SQL_BIND_TEXT;
		bind[0].size = MTP_DBSQ_DATE_LENGTH;
		num = 1;
		break;
	case MTP_DB_QUERY_FULL_PATH_NAME:
	case MTP_DB_QUERY_NAME:
	case MTP_DB_QUERY_ARTIST:
	case MTP_DB_QUERY_GENRE:
	case MTP_DB_QUERY_ALBUM:
	case MTP_DB_QUERY_ALBUM_ARTIST:
	case MTP_DB_QUERY_SAMPLE_RATE:
		length = mtpUtilStrlen((const char *)value);
		if (length < size)
			size = length;
		/* fall through */
	case MTP_DB_QUERY_PUOID:
		bind[0].value.data = value;
		bind[0].type = MTP_SQL_BIND_TEXT;
		bind[0].size = size;
		num = 1;
		break;
	case MTP_DB_QUERY_USE_COUNT:
	case MTP_DB_QUERY_AUDIO_WAVE_CODEC:
	case MTP_DB_QUERY_AUDIO_BIT_RATE:
	case MTP_DB_QUERY_DURATION:
	case MTP_DB_QUERY_NUMBER_OF_CHANNELS:
	case MTP_DB_QUERY_RATING:
	case MTP_DB_QUERY_BITRATE_TYPE:
	case MTP_DB_QUERY_DRM_STATUS:
	case MTP_DB_QUERY_TRACK:
	case MTP_DB_QUERY_SAMPLE_FORMAT:
	case MTP_DB_QUERY_BUYFLAG:
	case MTP_DB_QUERY_AUDIO_BIT_DEPTH:
	case MTP_DB_QUERY_WIDTH:
	case MTP_DB_QUERY_HEIGHT:
		_setIntegerData(value, size, &bind[0]);
		num = 1;
		break;
	case MTP_DB_QUERY_OBJECT_DATA_BASIC:
		_bindObjectDataBasic(&data->basic, bind);
		mtpUtilDateTimeToString(date, &data->basic.dateCreated);
		bind[5].value.data = date;
		bind[5].type = MTP_SQL_BIND_TEXT;
		bind[5].size = MTP_DBSQ_DATE_LENGTH;
		mtpUtilDateTimeToString(dateModified, &data->basic.dateModified);
		bind[6].type = MTP_SQL_BIND_TEXT;
		bind[6].value.data = dateModified;
		bind[6].size = MTP_DBSQ_DATE_LENGTH;
		num = MTP_DBSQ_NUM_BASIC_COLUMNS;
		break;
	case MTP_DB_QUERY_OBJECT_DATA_EXTENSION:
		_bindObjectDataExtension((const ObjectDataExtension *)value, bind);
		mtpUtilDateTimeToString(releaseDate,
			&((const ObjectDataExtension *)value)->originalReleaseDate);
		bind[8].value.data = releaseDate;
		bind[8].type = MTP_SQL_BIND_TEXT;
		bind[8].size = MTP_DBSQ_DATE_LENGTH;
		num = MTP_DBSQ_NUM_EXTENSION_COLUMNS;
		break;
	case MTP_DB_QUERY_OBJECT_DATA:
		_bindObjectDataBasic(&data->basic, bind);
		_bindObjectDataExtension(&data->extension, &bind[MTP_DBSQ_NUM_BASIC_COLUMNS]);
		mtpUtilDateTimeToString(date, &data->basic.dateCreated);
		bind[5].value.data = date;
		bind[5].type = MTP_SQL_BIND_TEXT;
		bind[5].size = MTP_DBSQ_DATE_LENGTH;
		mtpUtilDateTimeToString(dateModified, &data->basic.dateModified);
		bind[6].type = MTP_SQL_BIND_TEXT;
		bind[6].value.data = dateModified;
		bind[6].size = MTP_DBSQ_DATE_LENGTH;
		mtpUtilDateTimeToString(releaseDate, &data->extension.originalReleaseDate);
		bind[MTP_DBSQ_NUM_BASIC_COLUMNS + 8].type = MTP_SQL_BIND_TEXT;
		bind[MTP_DBSQ_NUM_BASIC_COLUMNS + 8].value.data = releaseDate;
		bind[MTP_DBSQ_NUM_BASIC_COLUMNS + 8].size = MTP_DBSQ_DATE_LENGTH;
		num = MTP_DBSQ_NUM_COLUMNS;
		break;
	default:
		MTP_ERR(MTP_TAG_DBSQ, 580, "not supported: %d\n", type);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	for (i = 0; i < numExtra && num < MTP_DBSQ_MAX_BIND; i++, num++) {
		bind[num].type = extra->type;
		if (extra->type == MTP_SQL_BIND_INTEGER)
			bind[num].value.integer = extra->value.integer;
		else
			bind[num].value.data = extra->value.data;
		bind[num].size = extra->size;
		extra++;
	}
	return mtpSqlBindStatement(stmt, bind, num);
}

/* a text column of at most size bytes, ended with a NUL when it is shorter */
static inline void _getTextResult(sqlite3_stmt *stmt, int column, char *text, SceSize size)
{
	const char *result = (const char *)sqlite3_column_text(stmt, column);
	SceSize length = sqlite3_column_bytes(stmt, column);

	if (length > size)
		length = size;
	mtpUtilMemcpy(text, result, length);
	if (length < size)
		text[length] = '\0';
}

/* text: a buffer of MTP_DBSQ_DATE_SIZE bytes */
static inline void _getDateResult(sqlite3_stmt *stmt, int column, char *text,
	SceDateTime *date)
{
	_getTextResult(stmt, column, text, MTP_DBSQ_DATE_LENGTH);
	mtpUtilStringToDateTime(text, date);
}

/* ObjectDataBasic from the row's columns from column on */
static void _getObjectDataBasicResult(sqlite3_stmt *stmt, int column, ObjectDataBasic *object)
{
	char date[MTP_DBSQ_DATE_SIZE];

	_getIntegerResult(stmt, column, &object->uid, sizeof(object->uid));
	_getIntegerResult(stmt, column + 1, &object->parentUid, sizeof(object->parentUid));
	_getIntegerResult(stmt, column + 2, &object->objectFormat,
		sizeof(object->objectFormat));
	_getIntegerResult(stmt, column + 3, &object->protectionStatus,
		sizeof(object->protectionStatus));
	_getIntegerResult(stmt, column + 4, &object->objectSize, sizeof(object->objectSize));
	_getDateResult(stmt, column + 5, date, &object->dateCreated);
	_getDateResult(stmt, column + 6, date, &object->dateModified);
	_getTextResult(stmt, column + 7, object->fullPathName, sizeof(object->fullPathName));
	_getIntegerResult(stmt, column + 8, &object->nonConsumable,
		sizeof(object->nonConsumable));
	_getTextResult(stmt, column + 9, object->name, sizeof(object->name));
	_getTextResult(stmt, column + 10, (char *)object->puoid, sizeof(object->puoid));
}

/* ObjectDataExtension from the row's columns from column on; SampleFormat is not read */
static void _getObjectDataExtensionResult(sqlite3_stmt *stmt, int column,
	ObjectDataExtension *object)
{
	char date[MTP_DBSQ_DATE_SIZE];

	_getTextResult(stmt, column, object->artist, sizeof(object->artist));
	_getTextResult(stmt, column + 1, object->genre, sizeof(object->genre));
	_getTextResult(stmt, column + 2, object->album, sizeof(object->album));
	_getTextResult(stmt, column + 3, object->albumArtist, sizeof(object->albumArtist));
	_getIntegerResult(stmt, column + 4, &object->useCount, sizeof(object->useCount));
	_getIntegerResult(stmt, column + 5, &object->sampleRate, sizeof(object->sampleRate));
	_getIntegerResult(stmt, column + 6, &object->audioWaveCodec,
		sizeof(object->audioWaveCodec));
	_getIntegerResult(stmt, column + 7, &object->audioBitRate,
		sizeof(object->audioBitRate));
	_getDateResult(stmt, column + 8, date, &object->originalReleaseDate);
	_getIntegerResult(stmt, column + 9, &object->duration, sizeof(object->duration));
	_getIntegerResult(stmt, column + 10, &object->numberOfChannels,
		sizeof(object->numberOfChannels));
	_getIntegerResult(stmt, column + 11, &object->rating, sizeof(object->rating));
	_getIntegerResult(stmt, column + 12, &object->bitrateType,
		sizeof(object->bitrateType));
	_getIntegerResult(stmt, column + 13, &object->drmStatus, sizeof(object->drmStatus));
	_getIntegerResult(stmt, column + 14, &object->track, sizeof(object->track));
	_getIntegerResult(stmt, column + 16, &object->buyflag, sizeof(object->buyflag));
	_getIntegerResult(stmt, column + 17, &object->audioBitDepth,
		sizeof(object->audioBitDepth));
	_getIntegerResult(stmt, column + 18, &object->width, sizeof(object->width));
	_getIntegerResult(stmt, column + 19, &object->height, sizeof(object->height));
}

/* the row a query of type read, into buf (size bytes) */
static inline int _getQueryResult(sqlite3_stmt *stmt, int type, void *buf, SceSize size)
{
	ObjectData *data = (ObjectData *)buf;
	int count, columnType;

	count = sqlite3_column_count(stmt);
	if (type < MTP_DB_QUERY_OBJECT_DATA_BASIC) {
		if (count <= 0) {
			MTP_ERR(MTP_TAG_DBSQ, 666, "not found result: %d\n", count);
			return SCE_MTP_ERROR_NOT_FOUND;
		}
		columnType = sqlite3_column_type(stmt, 0);
		switch (columnType) {
		case SQLITE_NULL:
			mtpUtilMemset(buf, 0, size);
			return 0;
		case SQLITE_TEXT:
			_getTextResult(stmt, 0, (char *)buf, size);
			return 0;
		case SQLITE_INTEGER:
			_getIntegerResult(stmt, 0, buf, size);
			return 0;
		}
		MTP_ERR(MTP_TAG_DBSQ, 684, "unexpected type is returned: %d\n", columnType);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	switch (type) {
	case MTP_DB_QUERY_OBJECT_DATA_BASIC:
		if (count < MTP_DBSQ_NUM_BASIC_COLUMNS) {
			MTP_ERR(MTP_TAG_DBSQ, 695, "not found result: %d\n", count);
			return SCE_MTP_ERROR_NOT_FOUND;
		}
		_getObjectDataBasicResult(stmt, 0, &data->basic);
		return 0;
	case MTP_DB_QUERY_OBJECT_DATA_EXTENSION:
		if (count < MTP_DBSQ_NUM_EXTENSION_COLUMNS) {
			MTP_ERR(MTP_TAG_DBSQ, 708, "not found result: %d\n", count);
			return SCE_MTP_ERROR_NOT_FOUND;
		}
		_getObjectDataExtensionResult(stmt, 0, (ObjectDataExtension *)buf);
		return 0;
	case MTP_DB_QUERY_OBJECT_DATA:
		if (count < MTP_DBSQ_NUM_COLUMNS) {
			MTP_ERR(MTP_TAG_DBSQ, 722, "not found result: %d\n", count);
			return SCE_MTP_ERROR_NOT_FOUND;
		}
		_getObjectDataBasicResult(stmt, 0, &data->basic);
		_getObjectDataExtensionResult(stmt, MTP_DBSQ_NUM_BASIC_COLUMNS, &data->extension);
		return 0;
	}
	return SCE_MTP_ERROR_INVALID_ARGUMENT;
}

static int dbSqliteOpen(MtpDB *db, int create)
{
	sqlite3 *sqlite = NULL;
	const char *sqls[] = {
		"PRAGMA journal_mode=off",
		"BEGIN TRANSACTION;",
		"CREATE TABLE IF NOT EXISTS tbl_mtpObject("
			"UID INTEGER NOT NULL PRIMARY KEY CHECK (UID<67108864), "
			"ParentUID INTEGER, ObjectFormat INTEGER, ProtectionStatus INTEGER, "
			"ObjectSize INTEGER, DateCreated TEXT, DateModified TEXT, "
			"FullPathName TEXT UNIQUE, Name TEXT, NonConsumable INTEGER, PUOID TEXT, "
			"Artist TEXT, Genre TEXT, Album TEXT, AlbumArtist TEXT, UseCount INTEGER, "
			"SampleRate INTEGER, AudioWaveCodec INTEGER, AudioBitRate INTEGER, "
			"OriginalReleaseDate TEXT, Duration INTEGER, NumberOfChannels INTEGER, "
			"Rating INTEGER, BitrateType INTEGER, DrmStatus INTEGER, Track INTEGER, "
			"SampleFormat INTEGER, Buyflag INTEGER, AudioBitDepth INTEGER, "
			"Width INTEGER, Height INTEGER)",
		"END TRANSACTION;",
		NULL,
	};
	int flags;
	int ret;

	if (db == NULL || db->sqlite != MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DBSQ, 781, "invalid parameter or db already opened\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	/* each session starts the database afresh */
	mtpFileDelete(db->path, MTP_FORMAT_UNDEFINED);
	flags = SQLITE_OPEN_READWRITE;
	if (create)
		flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	ret = sqlite3_open_v2(db->path, &sqlite, flags | SQLITE_OPEN_NOMUTEX, NULL);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_DBSQ, 804, "sliqte open failed %s: %d\n", db->path, ret);
		ret = mtpSqlConvertError(ret);
	} else {
		db->sqlite = sqlite;
		if (create && (ret = mtpSqlExec(sqlite, sqls)) < 0) {
			sqlite3_close(sqlite);
			db->sqlite = MTP_DB_NOT_OPEN;
		} else {
			db->opened = db;
		}
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteClose(MtpDB *db)
{
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DBSQ, 835, "invalid parameter or db already opened\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	ret = sqlite3_close(db->sqlite);
	if (ret != SQLITE_OK) {
		MTP_ERR(MTP_TAG_DBSQ, 843, "%s failed: %d, (%s)\n", "sqlite3_close", ret,
			sqlite3_errmsg(db->sqlite));
		ret = mtpSqlConvertError(ret);
	} else {
		db->sqlite = MTP_DB_NOT_OPEN;
		db->opened = NULL;
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteFormat(MtpDB *db, SceUInt32 maxObjects)
{
	const char *sqls[] = {
		"BEGIN TRANSACTION;",
		"DELETE FROM tbl_mtpObject;",
		"END TRANSACTION;",
		NULL,
	};
	int ret;

	if (db == NULL || db->sqlite == MTP_DB_NOT_OPEN) {
		MTP_ERR(MTP_TAG_DBSQ, 873, "invalid parameter or db already opened\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	ret = mtpSqlExec(db->sqlite, sqls);
	if (ret < 0)
		MTP_ERR(MTP_TAG_DBSQ, 881, "unable to format DB: 0x%x\n", ret);
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteGetUniqueIdInfo(MtpDB *db, SceUInt32 *numObjects)
{
	sqlite3_stmt *stmt = NULL;
	sqlite3 *sqlite;
	int step;
	int ret;

	if (db == NULL || numObjects == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 900, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	*numObjects = 0;
	sqlite = db->sqlite;
	ret = mtpSqlCreateStatement(sqlite, "SELECT count(*) FROM tbl_mtpObject;", &stmt);
	if (ret >= 0) {
		while ((step = sqlite3_step(stmt)) == SQLITE_ROW)
			*numObjects = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		if (step == SQLITE_DONE) {
			ret = 0;
		} else {
			MTP_ERR(MTP_TAG_DBSQ, 922, "%s failed: %d, (%s)\n", "sqlite3_step", step,
				sqlite3_errmsg(sqlite));
			ret = mtpSqlConvertError(step);
		}
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteRegisterObject(MtpDB *db, ObjectDataBasic *object)
{
	char dateCreated[MTP_DBSQ_DATE_SIZE];
	char dateModified[MTP_DBSQ_DATE_SIZE];
	MtpSqlBind bind[] = {
		{ MTP_SQL_BIND_INTEGER, { object->parentUid }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { object->objectFormat }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { object->protectionStatus }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { (SceInt64)object->objectSize }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_TEXT, { .data = dateCreated }, MTP_DBSQ_DATE_LENGTH },
		{ MTP_SQL_BIND_TEXT, { .data = dateModified }, MTP_DBSQ_DATE_LENGTH },
		{ MTP_SQL_BIND_TEXT, { .data = object->fullPathName },
			mtpUtilStrlen(object->fullPathName) },
		{ MTP_SQL_BIND_TEXT, { .data = object->name }, mtpUtilStrlen(object->name) },
		{ MTP_SQL_BIND_INTEGER, { object->nonConsumable }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_TEXT, { .data = object->puoid }, sizeof(object->puoid) },
	};
	sqlite3_stmt *stmt = NULL;
	sqlite3 *sqlite;
	int step;
	int ret;

	if (db == NULL || db->opened == NULL || object == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 965, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpUtilDateTimeToString(dateCreated, &object->dateCreated);
	mtpUtilDateTimeToString(dateModified, &object->dateModified);
	mtpDBLock(db);
	sqlite = db->sqlite;
	ret = mtpSqlCreateStatement(sqlite,
		"INSERT INTO tbl_mtpObject (ParentUID, ObjectFormat, ProtectionStatus, "
		"ObjectSize, DateCreated, DateModified, FullPathName, Name, NonConsumable, "
		"PUOID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", &stmt);
	if (ret >= 0) {
		ret = mtpSqlBindStatement(stmt, bind, sizeof(bind) / sizeof(bind[0]));
		if (ret >= 0) {
			do {
				step = sqlite3_step(stmt);
			} while (step == SQLITE_ROW);
			if (step == SQLITE_DONE) {
				object->uid = sqlite3_last_insert_rowid(sqlite);
				sqlite3_finalize(stmt);
				ret = 0;
			} else {
				MTP_ERR(MTP_TAG_DBSQ, 1000, "%s failed: %d, (%s)\n", "sqlite3_step",
					step, sqlite3_errmsg(sqlite));
				ret = mtpSqlConvertError(step);
				sqlite3_finalize(stmt);
			}
		} else {
			sqlite3_finalize(stmt);
		}
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteUnregisterObject(MtpDB *db, SceUInt32 uniqueId)
{
	MtpSqlBind bind = { MTP_SQL_BIND_INTEGER, { uniqueId }, sizeof(uniqueId) };
	sqlite3_stmt *stmt = NULL;
	sqlite3 *sqlite;
	int step;
	int ret;

	if (db == NULL || db->sqlite == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1036, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	sqlite = db->sqlite;
	ret = mtpSqlCreateStatement(sqlite, "DELETE FROM tbl_mtpObject WHERE UID=?;", &stmt);
	if (ret == 0) {
		ret = mtpSqlBindStatement(stmt, &bind, 1);
		if (ret >= 0) {
			do {
				step = sqlite3_step(stmt);
			} while (step == SQLITE_ROW);
			sqlite3_finalize(stmt);
			if (step == SQLITE_DONE) {
				ret = 0;
			} else {
				MTP_ERR(MTP_TAG_DBSQ, 1062, "%s failed: %d, (%s)\n", "sqlite3_step",
					step, sqlite3_errmsg(sqlite));
				ret = mtpSqlConvertError(step);
			}
		} else {
			sqlite3_finalize(stmt);
		}
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteGetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type, void *buf,
	SceSize size)
{
	sqlite3_stmt *stmt = NULL;
	sqlite3 *sqlite;
	int found, step;
	int ret;

	if (db == NULL || db->opened == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1093, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	ret = _checkObjectProperty(type, size);
	if (ret >= 0) {
		sqlite = db->sqlite;
		ret = _createGetQueryStatement(sqlite, uniqueId, type, &stmt);
		if (ret >= 0) {
			found = 0;
			for (;;) {
				step = sqlite3_step(stmt);
				if (step != SQLITE_ROW)
					break;
				if (found++ != 0) {
					MTP_ERR(MTP_TAG_DBSQ, 1115, "why found more than 2 result\n");
					break;
				}
				ret = _getQueryResult(stmt, type, buf, size);
				if (ret < 0) {
					sqlite3_finalize(stmt);
					goto out;
				}
			}
			sqlite3_finalize(stmt);
			if (step != SQLITE_DONE) {
				MTP_ERR(MTP_TAG_DBSQ, 1126, "%s failed: %d, (%s)\n", "sqlite3_step",
					step, sqlite3_errmsg(sqlite));
				ret = mtpSqlConvertError(step);
			} else if (found == 0) {
				MTP_ERR(MTP_TAG_DBSQ, 1134, "Not found for 0x%x\n", uniqueId);
				ret = SCE_MTP_ERROR_NOT_FOUND;
			} else {
				ret = 0;
			}
		}
	}
out:
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteSetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type,
	const void *buf, SceSize size)
{
	MtpSqlBind uid = { MTP_SQL_BIND_INTEGER, { uniqueId }, sizeof(SceInt64) };
	sqlite3_stmt *stmt = NULL;
	sqlite3 *sqlite;
	int step;
	int ret;

	if (db == NULL || db->opened == NULL || buf == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1161, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpDBLock(db);
	ret = _checkObjectProperty(type, size);
	if (ret >= 0) {
		sqlite = db->sqlite;
		ret = _createSetQueryStatement(sqlite, type, &stmt);
		if (ret >= 0) {
			ret = _bindSetQueryStatement(type, stmt, buf, size, &uid, 1);
			if (ret >= 0) {
				do {
					step = sqlite3_step(stmt);
				} while (step == SQLITE_ROW);
				sqlite3_finalize(stmt);
				if (step == SQLITE_DONE) {
					ret = 0;
				} else {
					MTP_ERR(MTP_TAG_DBSQ, 1190, "%s failed: %d, (%s)\n",
						"sqlite3_step", step, sqlite3_errmsg(sqlite));
					ret = mtpSqlConvertError(step);
				}
			} else {
				sqlite3_finalize(stmt);
			}
		}
	}
	mtpDBUnlock(db);
	return ret;
}

static int dbSqliteGetNumObjectsByType(MtpDB *db, int type, const void *value,
	SceUInt32 *num)
{
	sqlite3_stmt *stmt = NULL;
	SceUInt32 size = 0;
	sqlite3 *sqlite;
	int isNull;
	int step, i;
	int ret;

	if (db == NULL || db->opened == NULL || num == NULL || value == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1251, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	/* an empty path: the objects without one */
	isNull = 0;
	if (type == MTP_DB_QUERY_FULL_PATH_NAME)
		isNull = mtpUtilStrcmp((const char *)value, "") == 0;
	mtpDBLock(db);
	sqlite = db->sqlite;
	ret = _getObjectPropertySize(type, &size);
	if (ret >= 0) {
		ret = _createGetQueryByTypeStatement(type, sqlite, NULL, isNull, &stmt);
		if (ret >= 0) {
			if (isNull ||
			    (ret = _bindSetQueryStatement(type, stmt, value, size, NULL, 0)) >= 0) {
				for (i = 0; (step = sqlite3_step(stmt)) == SQLITE_ROW; i++)
					;
				sqlite3_finalize(stmt);
				if (step == SQLITE_DONE) {
					*num = i;
					ret = 0;
				} else {
					MTP_ERR(MTP_TAG_DBSQ, 1287, "%s failed: %d, (%s)\n",
						"sqlite3_step", step, sqlite3_errmsg(sqlite));
					ret = mtpSqlConvertError(step);
				}
			} else {
				sqlite3_finalize(stmt);
			}
		}
	}
	mtpDBUnlock(db);
	return ret;
}

/* the objects in the folder parent */
static int dbSqliteGetNumObjects(MtpDB *db, SceUInt32 parent, SceUInt32 *num)
{
	return dbSqliteGetNumObjectsByType(db, MTP_DB_QUERY_PARENT_UID, &parent, num);
}

/* the unique ids of count objects from offset on; returns their bytes in ids */
static int dbSqliteGetObjectUniqueIdsByType(MtpDB *db, int type, const void *value,
	SceUInt32 offset, SceUInt32 count, SceUInt32 *ids)
{
	const char *suffix = " LIMIT ? OFFSET ?;";
	MtpSqlBind range[] = {
		{ MTP_SQL_BIND_INTEGER, { count }, sizeof(SceInt64) },
		{ MTP_SQL_BIND_INTEGER, { offset }, sizeof(SceInt64) },
	};
	sqlite3_stmt *stmt = NULL;
	SceUInt32 size = 0;
	sqlite3 *sqlite;
	SceUInt32 i;
	int isNull;
	int step;
	int ret;

	if (db == NULL || db->opened == NULL || ids == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1348, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	isNull = 0;
	if (type == MTP_DB_QUERY_FULL_PATH_NAME)
		isNull = mtpUtilStrcmp((const char *)value, "") == 0;
	mtpDBLock(db);
	sqlite = db->sqlite;
	ret = _getObjectPropertySize(type, &size);
	if (ret >= 0) {
		ret = _createGetQueryByTypeStatement(type, sqlite, suffix, isNull, &stmt);
		if (ret >= 0) {
			if (isNull)
				ret = mtpSqlBindStatement(stmt, range, 2);
			else
				ret = _bindSetQueryStatement(type, stmt, value, size, range, 2);
			if (ret >= 0) {
				for (i = 0; (step = sqlite3_step(stmt)) == SQLITE_ROW && i < count;
				     i++) {
					_getIntegerResult(stmt, 0, ids, sizeof(*ids));
					ids++;
				}
				sqlite3_finalize(stmt);
				if (step == SQLITE_DONE) {
					if (count == 0 || i != 0)
						ret = i * sizeof(*ids);
					else
						ret = SCE_MTP_ERROR_NOT_FOUND;
				} else {
					MTP_ERR(MTP_TAG_DBSQ, 1388, "%s failed: %d, (%s)\n",
						"sqlite3_step", step, sqlite3_errmsg(sqlite));
					ret = mtpSqlConvertError(step);
				}
			} else {
				sqlite3_finalize(stmt);
			}
		}
	}
	mtpDBUnlock(db);
	return ret;
}

/* the unique ids of the objects in the folder parent */
static int dbSqliteGetObjectUniqueIds(MtpDB *db, SceUInt32 parent, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids)
{
	return dbSqliteGetObjectUniqueIdsByType(db, MTP_DB_QUERY_PARENT_UID, &parent, offset,
		count, ids);
}

/* a handle in objmap for each object of the database */
static int dbSqliteSetupMtpHandlesList(MtpDB *db, MtpObjmap *objmap)
{
	sqlite3_stmt *stmt = NULL;
	SceUInt16 format;
	SceUInt32 uniqueId, parent, handle, flags;
	sqlite3 *sqlite;
	char *path;
	int step;
	int ret;

	if (db == NULL || db->opened == NULL || objmap == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1430, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	path = (char *)mtpUtilMalloc(sizeof(((ObjectDataBasic *)0)->fullPathName));
	if (path == NULL) {
		MTP_ERR(MTP_TAG_DBSQ, 1436, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpDBLock(db);
	sqlite = db->sqlite;
	ret = mtpSqlCreateStatement(sqlite,
		"SELECT UID, ParentUID, ObjectFormat, FullPathName from tbl_mtpObject;", &stmt);
	if (ret >= 0) {
		while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
			_getIntegerResult(stmt, 0, &uniqueId, sizeof(uniqueId));
			_getIntegerResult(stmt, 1, &parent, sizeof(parent));
			_getIntegerResult(stmt, 2, &format, sizeof(format));
			_getTextResult(stmt, 3, path, sizeof(((ObjectDataBasic *)0)->fullPathName));
			if (parent == MTP_OBJ_NO_PARENT)
				flags = MTP_OBJECT_HANDLE_INDEX_MASK;
			else
				flags = format == MTP_FORMAT_ASSOCIATION ? MTP_OBJECT_HANDLE_DIR : 0;
			ret = mtpObjmapAllocate(objmap, flags, uniqueId, parent, &handle);
			if (ret < 0) {
				sqlite3_finalize(stmt);
				goto out;
			}
		}
		sqlite3_finalize(stmt);
		if (step == SQLITE_DONE) {
			ret = 0;
		} else {
			MTP_ERR(MTP_TAG_DBSQ, 1500, "%s failed: %d, (%s)\n", "sqlite3_step", step,
				sqlite3_errmsg(sqlite));
			ret = mtpSqlConvertError(step);
		}
	}
out:
	mtpDBUnlock(db);
	mtpUtilFree(path);
	return ret;
}

const MtpDBOperation dbSqliteOperation = {
	dbSqliteOpen,
	dbSqliteClose,
	dbSqliteFormat,
	dbSqliteGetUniqueIdInfo,
	dbSqliteRegisterObject,
	dbSqliteUnregisterObject,
	dbSqliteGetObjectProperty,
	dbSqliteSetObjectProperty,
	NULL,
	dbSqliteGetNumObjects,
	dbSqliteGetNumObjectsByType,
	dbSqliteGetObjectUniqueIds,
	dbSqliteGetObjectUniqueIdsByType,
	dbSqliteSetupMtpHandlesList,
};
