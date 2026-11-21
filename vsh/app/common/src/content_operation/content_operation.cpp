/*
 * SceContentOperation: the deletion of contents from the media database. A content's file is
 * deleted by the first handler of its type that takes it (a video's with the files its profile
 * names, an empr0: video's on a loopback mount); then its row, and then the lists left empty.
 */
#include <std/string>
#include <std/vector>
#include <std/new>
#include <std/stdio.h>
#include <std/string.h>
#include <common/char_encoding.h>
#include <module/module.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include <dbutil/record.h>
#include <dbutil/rowid_result.h>
#include <dbutil/value.h>
#include <libvideoprofiler/videoprofiler.h>
#include "content_operation.h"

/*
 * The functions marked noinline are calls in Sony's binary, where our SNC would inline them
 * (Sony's were most likely in files of their own).
 */

/* Sony calls the entries' push_back out of line */
PAF_VECTOR_NOINLINE_PUSH_BACK(ContentOperationEntry, paf::allocator<ContentOperationEntry>)

/* the loopback mount ID empr0: is mounted under */
#define CONTENT_OPERATION_EMPR_MOUNT_ID		600
/* sceAppMgrLoopBackMount's: mounted already */
#define CONTENT_OPERATION_ERROR_MOUNTED		(-2139095037)	/* 0x80800003 */
/* paf::LocalFile::RemoveFile's: no such file */
#define CONTENT_OPERATION_ERROR_NO_FILE		(-2135994334)	/* 0x80AF5022 */
/* the characters a list title keeps */
#define CONTENT_OPERATION_TITLE_LENGTH		128
/* the bytes of a content compared to find a copy */
#define CONTENT_OPERATION_COMPARE_SIZE		0x400

/* the media database's contents, by type */
class ContentDatabase {
public:
	virtual ~ContentDatabase()
	{
	}

	virtual int Open() = 0;
	/* the text column of the content id */
	virtual int GetValue(SceInt64 id, int type, const paf::string &column,
			     paf::string *value) = 0;
	virtual int Delete(SceInt64 id, int type) = 0;
	virtual int DeleteList(SceInt64 listId) = 0;
	virtual int GetContentPath(SceInt64 id, int type, paf::string *path) = 0;
	virtual int FindList(const paf::string &title, SceInt64 *listId) = 0;
	virtual int GetListOfItem(SceInt64 itemId, SceInt64 *listId) = 0;
	virtual int CountListItems(SceInt64 listId, int *count) = 0;
	virtual int Begin() = 0;
	virtual int Commit() = 0;
	virtual int Rollback() = 0;
	/* the contents of query's size */
	virtual int FindContents(const ContentOperationQuery *query,
				 paf::vector<ContentOperationEntry> *entries) = 0;
	/* the photos of query's size */
	virtual int FindPhotos(const ContentOperationQuery *query,
			       paf::vector<ContentOperationEntry> *entries) = 0;

	paf::string m_databaseName;	/* the connection's database the tables are in */
};

/* the media database on a dbutil connection */
class ContentDatabaseImpl : public ContentDatabase {
public:
	ContentDatabaseImpl(dbutil::Connection *connection)
		: m_noTransaction(false), m_connection(connection)
	{
	}

	virtual ~ContentDatabaseImpl()
	{
	}

	virtual int Open();
	virtual int GetValue(SceInt64 id, int type, const paf::string &column, paf::string *value);
	virtual int Delete(SceInt64 id, int type);
	virtual int DeleteList(SceInt64 listId);
	virtual int GetContentPath(SceInt64 id, int type, paf::string *path);
	virtual int FindList(const paf::string &title, SceInt64 *listId);
	virtual int GetListOfItem(SceInt64 itemId, SceInt64 *listId);
	virtual int CountListItems(SceInt64 listId, int *count);
	virtual int Begin();
	virtual int Commit();
	virtual int Rollback();
	virtual int FindContents(const ContentOperationQuery *query,
				 paf::vector<ContentOperationEntry> *entries);
	virtual int FindPhotos(const ContentOperationQuery *query,
			       paf::vector<ContentOperationEntry> *entries);

	__attribute__((noinline)) int OpenRecord(SceInt64 id, int type,
						 const paf::string &columns,
						 dbutil::RefPtr<dbutil::Record> *record);
	int DeleteRow(SceInt64 id, const paf::string &table);
	__attribute__((noinline)) int GetTableName(int type, paf::string *table);
	void ConvertMusicPath(paf::string *path);

	bool m_noTransaction;		/* the caller runs the transaction */
	dbutil::Connection *m_connection;
	dbutil::RefPtr<dbutil::Statement> m_statement;
};

/* deletes a content's file */
class ContentDeleter {
public:
	ContentDeleter(ContentDatabase *database, void *context)
		: m_database(database), m_context(context), m_modulesLoaded(false)
	{
	}

	virtual ~ContentDeleter()
	{
	}

	virtual int Run(SceInt64 id, int type);

	ContentDatabase *m_database;
	void *m_context;
	bool m_modulesLoaded;
};

/* deletes a video's file, with the files its profile names */
class VideoContentDeleter : public ContentDeleter {
public:
	VideoContentDeleter(ContentDatabase *database, void *context)
		: ContentDeleter(database, context), m_mounted(false)
	{
	}

	virtual int Run(SceInt64 id, int type);

	int Mount(paf::string *mountedPath, paf::string path);

	ScePfsRndDriveId m_mountPoint;	/* empr0:'s loopback mount */
	bool m_mounted;
	SceUInt32 reserved[2];
};

/* takes the contents of a type it can delete, with its deleter */
class ContentHandler {
public:
	ContentHandler() : m_deleter(NULL)
	{
	}

	virtual ~ContentHandler();
	virtual bool CanHandle(ContentDatabase *database, SceInt64 id, int type,
			       const paf::string &path);
	virtual ContentDeleter *Create(ContentDatabase *database, void *context);

	ContentDeleter *m_deleter;
};

/* takes the videos in the formats the profiler reads */
class VideoContentHandler : public ContentHandler {
public:
	__attribute__((noinline)) VideoContentHandler();
	virtual ~VideoContentHandler();
	virtual bool CanHandle(ContentDatabase *database, SceInt64 id, int type,
			       const paf::string &path);
	virtual ContentDeleter *Create(ContentDatabase *database, void *context);

	paf::Module *m_module;
};

/* the handlers of each type, tried in order */
class ContentHandlerSet {
public:
	ContentHandlerSet() : m_music(0), m_photo(0), m_video(0), m_context(NULL)
	{
	}

	~ContentHandlerSet();

	void DeleteHandlers(paf::vector<ContentHandler *> *handlers);
	int Init(void *context);
	__attribute__((noinline)) int Find(ContentDatabase *database, int type, SceInt64 id,
					   const paf::string &path, ContentDeleter **deleter);

	paf::vector<ContentHandler *> m_music;
	paf::vector<ContentHandler *> m_photo;
	paf::vector<ContentHandler *> m_video;
	void *m_context;
};

/* the video formats VideoContentHandler takes */
static const char *s_videoExtensions[] = {
	".mp4",
	".mnv",
	".mgv",
	".m4v",
	NULL
};

/* the context SceVideoProfiler works in while a video is deleted */
void *g_videoProfilerContext;

static int RemoveFile(const char *path);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	return 0;
}

ContentOperation *SceContentOperation_668CB1E5(ContentOperation *op)
{
	op->database = NULL;
	op->progress = new ContentOperationProgress;
	op->handlers = new ContentHandlerSet;
	return op;
}

ContentOperation *SceContentOperation_AF89DF37(ContentOperation *op)
{
	if (op->database != NULL)
		delete op->database;
	delete op->progress;
	delete op->handlers;
	return op;
}

static __attribute__((noinline))
int OpenDatabase(ContentOperation *op, dbutil::Connection *connection)
{
	ContentDatabaseImpl *database = new ContentDatabaseImpl(connection);

	if (database == NULL)
		return CONTENT_OPERATION_ERROR_NO_MEMORY;

	int ret = database->Open();

	if (ret < 0) {
		delete database;
		return ret;
	}
	op->database = database;
	return 0;
}

int SceContentOperation_1A67467D(ContentOperation *op, dbutil::Connection *connection,
				 void *context)
{
	if (op->database != NULL)
		return CONTENT_OPERATION_ERROR_ALREADY_OPEN;

	int ret = op->handlers->Init(context);

	if (ret < 0)
		return ret;
	return OpenDatabase(op, connection);
}

/* deletes the rows of ids, in one transaction */
static __attribute__((noinline))
int DeleteRows(ContentOperation *op, const paf::vector<SceInt64> *ids, int type)
{
	int ret = op->database->Begin();

	if (ret < 0)
		return ret;
	for (SceUInt32 i = 0; i < ids->size(); i++) {
		ret = op->database->Delete((*ids)[i], type);
		if (ret < 0) {
			op->database->Rollback();
			return ret;
		}
	}
	return op->database->Commit();
}

/* deletes the files of ids, telling the callback of each; ids keeps the ones deleted */
static __attribute__((noinline))
int DeleteFiles(ContentOperation *op, paf::vector<SceInt64> *ids, int type,
		const ContentOperationOption *option)
{
	int ret = 0;
	SceUInt32 i;

	for (i = 0; i < ids->size();) {
		ContentDeleter *deleter = NULL;

		ret = op->handlers->Find(op->database, type, (*ids)[i], paf::string(""), &deleter);
		if (ret < 0)
			return ret;
		if (deleter != NULL)
			deleter->m_modulesLoaded = option->modulesLoaded;
		ret = deleter->Run(ids->at(i), type);
		if (ret < 0)
			break;
		ret = op->progress->Advance(1);
		i++;
		if (ret < 0)
			break;
	}
	if (ret < 0)
		ids->resize(i);
	return ret;
}

/* the lists the items ids are in */
static __attribute__((noinline))
int GetLists(ContentOperation *op, const paf::vector<SceInt64> *ids,
	     paf::vector<SceInt64> *lists)
{
	SceInt64 listId = 0;

	for (SceUInt32 i = 0; i < ids->size(); i++) {
		if (op->database->GetListOfItem((*ids)[i], &listId) >= 0)
			lists->push_back(listId);
	}
	return 0;
}

/* deletes the lists of lists that have no item left */
static inline int DeleteEmptyLists(ContentOperation *op, const paf::vector<SceInt64> *lists)
{
	int ret = 0;

	for (SceUInt32 i = 0; i < lists->size(); i++) {
		int count = 0;

		if (op->database->CountListItems((*lists)[i], &count) == 0) {
			if (count == 0)
				ret = op->database->DeleteList((*lists)[i]);
		}
	}
	return ret;
}

int SceContentOperation_70B406BD(ContentOperation *op, paf::vector<SceInt64> *ids,
				 const paf::string &database, int type,
				 const ContentOperationOption *option)
{
	if (op->database == NULL)
		return CONTENT_OPERATION_ERROR_INVALID;

	op->database->m_databaseName.assign(database);

	ContentOperationProgress *progress = op->progress;

	static_cast<ContentDatabaseImpl *>(op->database)->m_noTransaction = !option->transaction;
	progress->status = 0;
	progress->total = ids->size();
	progress->done = 0;
	progress->failed.clear();

	paf::vector<SceInt64> lists;

	GetLists(op, ids, &lists);

	int ret = DeleteFiles(op, ids, type, option);

	if (ret >= 0 || ret == CONTENT_OPERATION_ERROR_CANCELED) {
		DeleteRows(op, ids, type);
		if (type == CONTENT_OPERATION_TYPE_PHOTO || type == CONTENT_OPERATION_TYPE_VIDEO)
			DeleteEmptyLists(op, &lists);
	}
	return ret;
}

int SceContentOperation_16112947(ContentOperation *op, const paf::vector<SceInt64> *lists)
{
	return DeleteEmptyLists(op, lists);
}

int SceContentOperation_7EB57B7D(ContentOperation *op)
{
	op->progress->status = CONTENT_OPERATION_ERROR_CANCELED;
	return 0;
}

void SceContentOperation_F0AC6849(ContentOperation *op, ContentOperationCallback callback,
				  void *param, void *userdata)
{
	ContentOperationProgress *progress = op->progress;

	progress->callback = callback;
	progress->callbackParam = param;
	progress->userdata = userdata;
}

int SceContentOperation_CDCC7DBD(ContentOperation *op, const paf::string &title,
				 SceInt64 *listId)
{
	return op->database->FindList(title, listId);
}

int SceContentOperation_0D35D892(ContentOperation *op, const paf::string &title,
				 paf::string *unique)
{
	SceInt64 listId = 0;
	paf::wstring wideTitle;
	int ret;

	paf::common::Utf8ToUtf16(title, &wideTitle, 0);

	int length = wideTitle.size();

	if (length <= CONTENT_OPERATION_TITLE_LENGTH) {
		unique->assign(title);
	} else {
		paf::wstring wideShort = wideTitle.substr(0, CONTENT_OPERATION_TITLE_LENGTH);
		paf::string shortTitle;

		paf::common::Utf16ToUtf8(wideShort, &shortTitle);
		unique->assign(shortTitle);
	}
	ret = op->database->FindList(*unique, &listId);
	if (ret == CONTENT_OPERATION_ERROR_NOT_FOUND)
		return 0;
	if (ret != 0)
		return ret;

	for (int n = 1;; n++) {
		int digits = 0;

		for (int left = n; left != 0; left /= 10)
			digits++;

		int maxLength = CONTENT_OPERATION_TITLE_LENGTH - 1 - digits;

		if (length < maxLength) {
			paf::string prefix = title + "-";
			char number[64];

			sce_paf_snprintf(number, sizeof(number), "%lld", (SceInt64)n);
			unique->assign(prefix + paf::string(number));
		} else {
			paf::wstring wideShort = wideTitle.substr(0, maxLength);
			paf::string shortTitle;

			paf::common::Utf16ToUtf8(wideShort, &shortTitle);
			unique->assign(shortTitle);

			char number[64];

			sce_paf_snprintf(number, sizeof(number), "%lld", (SceInt64)n);
			unique->append("-" + paf::string(number));
		}
		ret = op->database->FindList(*unique, &listId);
		if (ret == CONTENT_OPERATION_ERROR_NOT_FOUND)
			return 0;
		if (ret != 0)
			return ret;
	}
}

int SceContentOperation_52DCED70(ContentOperation *op, const ContentOperationQuery *query,
				 ContentOperationEntry *entry)
{
	entry->id = 0;
	entry->path.clear();
	if (op->database == NULL)
		return CONTENT_OPERATION_ERROR_UNAVAILABLE;

	paf::vector<ContentOperationEntry> entries;
	int result;

	if (query->type == CONTENT_OPERATION_TYPE_PHOTO) {
		result = op->database->FindPhotos(query, &entries);
		if (result < 0)
			return result;
	} else {
		result = op->database->FindContents(query, &entries);
		if (result < 0)
			return result;
	}
	if (entries.size() == 0)
		return result;
	if (query->read == NULL) {
		*entry = entries[0];
		return 0;
	}

	SceUInt32 size = query->size > CONTENT_OPERATION_COMPARE_SIZE ?
		CONTENT_OPERATION_COMPARE_SIZE : (SceUInt32)query->size;
	ContentOperationReadParam param;
	paf::vector<unsigned char> data;

	param.offset = 0;
	param.size = size;
	param.arg = query->readArg;
	if (query->read(&param, &data) != 0)
		return CONTENT_OPERATION_ERROR_READ_FAILED;

	char *buffer = NULL;
	int ret = result;

	for (SceUInt32 i = 0; i < entries.size() && i < query->maxCount; i++) {
		*entry = entries[i];
		if (entry->id == 0 || entry->path.size() == 0)
			break;
		if (buffer != NULL)
			delete[] buffer;
		buffer = new char[size];
		if (buffer == NULL) {
			ret = CONTENT_OPERATION_ERROR_NO_MEMORY;
			break;
		}

		SceUID fd = sceIoOpen(entry->path.c_str(), SCE_O_RDONLY, SCE_STM_RU | 0004);

		if (fd < 0) {
			ret = CONTENT_OPERATION_ERROR_READ_FAILED;
			break;
		}

		int read = sceIoRead(fd, buffer, size);

		sceIoClose(fd);
		if (read < 0 || read != size) {
			ret = CONTENT_OPERATION_ERROR_READ_FAILED;
			break;
		}
		if (sce_paf_memcmp(data.ptr, buffer, size) == 0) {
			ret = 0;
			break;
		}
		entry->id = 0;
		entry->path.clear();
	}
	delete[] buffer;
	return ret;
}

void ContentHandlerSet::DeleteHandlers(paf::vector<ContentHandler *> *handlers)
{
	for (paf::vector<ContentHandler *>::iterator it = handlers->begin(); it != handlers->end();
	     ++it) {
		if (*it != NULL)
			delete *it;
	}
	handlers->clear();
}

ContentHandlerSet::~ContentHandlerSet()
{
	DeleteHandlers(&m_photo);
	DeleteHandlers(&m_music);
	DeleteHandlers(&m_video);
}

int ContentHandlerSet::Init(void *context)
{
	ContentHandler *handler = new ContentHandler;

	if (handler == NULL)
		return CONTENT_OPERATION_ERROR_NO_MEMORY;
	m_photo.push_back(handler);

	handler = new ContentHandler;
	if (handler == NULL)
		return CONTENT_OPERATION_ERROR_NO_MEMORY;
	m_music.push_back(handler);

	handler = new VideoContentHandler;
	if (handler == NULL)
		return CONTENT_OPERATION_ERROR_NO_MEMORY;
	m_video.push_back(handler);

	handler = new ContentHandler;
	if (handler == NULL)
		return CONTENT_OPERATION_ERROR_NO_MEMORY;
	m_video.push_back(handler);

	m_context = context;
	return 0;
}

int ContentHandlerSet::Find(ContentDatabase *database, int type, SceInt64 id,
			    const paf::string &path, ContentDeleter **deleter)
{
	paf::vector<ContentHandler *> *handlers;

	switch (type) {
	case CONTENT_OPERATION_TYPE_PHOTO:
		handlers = &m_photo;
		break;
	case CONTENT_OPERATION_TYPE_MUSIC:
		handlers = &m_music;
		break;
	case CONTENT_OPERATION_TYPE_VIDEO:
		handlers = &m_video;
		break;
	default:
		return CONTENT_OPERATION_ERROR_UNSUPPORTED;
	}

	int ret = 0;

	for (paf::vector<ContentHandler *>::iterator it = handlers->begin(); it != handlers->end();
	     ++it) {
		ContentHandler *handler = *it;

		if (handler->CanHandle(database, id, type, path)) {
			*deleter = handler->Create(database, m_context);
			if (*deleter != NULL)
				return ret;
			return CONTENT_OPERATION_ERROR_NO_MEMORY;
		}
	}
	return CONTENT_OPERATION_ERROR_UNSUPPORTED;
}

int ContentOperationProgress::Advance(SceUInt32 count)
{
	done += count;
	if (callback != NULL) {
		ContentOperationProgressInfo info;

		info.status = status;
		info.total = total;
		info.done = done;
		info.failed = failed.size();
		callback(&info, userdata);
	}
	return status;
}

ContentHandler::~ContentHandler()
{
	if (m_deleter != NULL)
		delete m_deleter;
}

bool ContentHandler::CanHandle(ContentDatabase *database, SceInt64 id, int type,
			       const paf::string &path)
{
	return true;
}

ContentDeleter *ContentHandler::Create(ContentDatabase *database, void *context)
{
	if (m_deleter == NULL)
		m_deleter = new ContentDeleter(database, context);
	return m_deleter;
}

VideoContentHandler::VideoContentHandler() : m_module(NULL)
{
}

VideoContentHandler::~VideoContentHandler()
{
	if (m_module != NULL) {
		delete m_module;
		m_module = NULL;
	}
}

bool VideoContentHandler::CanHandle(ContentDatabase *database, SceInt64 id, int type,
				    const paf::string &path)
{
	paf::string contentPath;

	if (database->GetContentPath(id, type, &contentPath) != 0)
		return false;

	paf::string extension = contentPath.substr(
		contentPath.rfind(".", paf::string::npos, sce_paf_strlen(".")), paf::string::npos);

	for (int i = 0; i < 4; i++) {
		if (sce_paf_strcasecmp(extension.c_str(), s_videoExtensions[i]) == 0)
			return true;
	}
	return false;
}

ContentDeleter *VideoContentHandler::Create(ContentDatabase *database, void *context)
{
	if (m_deleter == NULL)
		m_deleter = new VideoContentDeleter(database, context);
	return m_deleter;
}

int ContentDatabaseImpl::Open()
{
	return 0;
}

int ContentDatabaseImpl::OpenRecord(SceInt64 id, int type, const paf::string &columns,
				    dbutil::RefPtr<dbutil::Record> *record)
{
	paf::string table;
	int ret = GetTableName(type, &table);

	if (ret < 0)
		return ret;
	ret = m_connection->OpenRecord(m_databaseName, table, id, columns, record);
	if (ret < 0)
		return ret;
	return 0;
}

int ContentDatabaseImpl::GetValue(SceInt64 id, int type, const paf::string &column,
				  paf::string *value)
{
	dbutil::RefPtr<dbutil::Record> record;
	int ret = OpenRecord(id, type, column, &record);

	if (ret < 0)
		return ret;

	dbutil::Value columnValue;

	ret = record->GetValue(0, &columnValue);
	if (ret < 0)
		return ret;
	if (columnValue.type != SQLITE_TEXT)
		return CONTENT_OPERATION_ERROR_INVALID;
	value->assign(columnValue.data.text);
	return 0;
}

int ContentDatabaseImpl::DeleteRow(SceInt64 id, const paf::string &table)
{
	paf::string sql = "DELETE from " + table + " WHERE mrid=?";
	dbutil::Value value;

	value.type = SQLITE_INTEGER;
	value.data.integer = id;

	int ret = m_connection->Execute(sql, 1, &value, NULL, NULL, NULL);

	if (ret < 0)
		return ret;
	if (m_connection->GetChanges() > 0)
		return 0;
	return CONTENT_OPERATION_ERROR_NOT_FOUND;
}

int ContentDatabaseImpl::Delete(SceInt64 id, int type)
{
	paf::string table;
	int ret = GetTableName(type, &table);

	if (ret < 0)
		return ret;
	return DeleteRow(id, table);
}

int ContentDatabaseImpl::DeleteList(SceInt64 listId)
{
	return DeleteRow(listId, paf::string("tbl_AVContentList"));
}

int ContentDatabaseImpl::GetContentPath(SceInt64 id, int type, paf::string *path)
{
	int ret = GetValue(id, type, paf::string("content_path"), path);

	if (ret < 0)
		return ret;
	ConvertMusicPath(path);
	return 0;
}

int ContentDatabaseImpl::FindList(const paf::string &title, SceInt64 *listId)
{
	dbutil::RowidResult result;
	paf::string sql("SELECT mrid FROM tbl_avcontentlist WHERE title=?");
	dbutil::Value value;

	value.type = SQLITE_TEXT;
	value.data.text = title.c_str();
	value.size = title.size();

	int ret = m_connection->Execute(sql, 1, &value, &result, NULL, NULL);

	if (ret < 0)
		return ret;
	if (result.m_rowCount != 0) {
		dbutil::RefPtr<dbutil::Record> record;

		ret = result.OpenRecord(0, paf::string("mrid"), &record);
		if (ret < 0)
			return ret;

		dbutil::Value column;

		ret = record->GetValue(0, &column);
		if (ret < 0)
			return ret;
		if (column.type == SQLITE_INTEGER) {
			ret = 0;
			*listId = column.data.integer;
		}
	} else {
		ret = CONTENT_OPERATION_ERROR_NOT_FOUND;
	}
	return ret;
}

int ContentDatabaseImpl::GetListOfItem(SceInt64 itemId, SceInt64 *listId)
{
	dbutil::RowidResult result;
	paf::string sql("SELECT id FROM tbl_AVContentItem WHERE item_id=?");
	dbutil::Value value;

	value.type = SQLITE_INTEGER;
	value.data.integer = itemId;

	int ret = m_connection->Execute(sql, 1, &value, &result, NULL, NULL);

	if (ret < 0)
		return ret;
	if (result.m_rowCount != 0) {
		dbutil::RefPtr<dbutil::Record> record;

		ret = result.OpenRecord(0, paf::string("list_id"), &record);
		if (ret < 0)
			return ret;

		dbutil::Value column;

		ret = record->GetValue(0, &column);
		if (ret < 0)
			return ret;
		ret = CONTENT_OPERATION_ERROR_NOT_FOUND;
		if (column.type == SQLITE_INTEGER) {
			ret = 0;
			*listId = column.data.integer;
		}
	} else {
		ret = CONTENT_OPERATION_ERROR_NOT_FOUND;
	}
	return ret;
}

int ContentDatabaseImpl::CountListItems(SceInt64 listId, int *count)
{
	dbutil::RowidResult result;
	paf::string sql("SELECT count(*) FROM tbl_AvContentItem WHERE list_id=?");
	dbutil::RefPtr<dbutil::Statement> statement;
	int ret = m_connection->Prepare(sql, &statement);

	if (ret == 0) {
		dbutil::Value value;

		value.type = SQLITE_INTEGER;
		value.data.integer = listId;
		ret = statement->Bind(1, &value, false);
		if (ret != 0)
			return ret;
		ret = statement->Step(1);
		if (ret != 0)
			return ret;

		dbutil::Value column;

		ret = statement->GetValue(0, &column);
		if (ret != 0)
			return ret;
		if (column.type == SQLITE_INTEGER) {
			ret = 0;
			*count = column.data.integer;
		}
	}
	return ret;
}

int ContentDatabaseImpl::Begin()
{
	if (m_noTransaction)
		return 0;
	return m_connection->Begin(DBUTIL_TRANSACTION_IMMEDIATE);
}

int ContentDatabaseImpl::Commit()
{
	if (m_noTransaction)
		return 0;
	return m_connection->Commit();
}

int ContentDatabaseImpl::Rollback()
{
	if (m_noTransaction)
		return 0;
	return m_connection->Rollback();
}

/* the contents of the statement's rows, at most query's maxCount */
static inline int ReadEntries(dbutil::RefPtr<dbutil::Statement> &statement,
			      const ContentOperationQuery *query,
			      paf::vector<ContentOperationEntry> *entries)
{
	int ret;

	while ((ret = statement->Step(1)) == 0) {
		ContentOperationEntry entry;
		int columns = statement->m_columnCount;

		for (int i = 0; i < columns; i++) {
			dbutil::Value value;

			ret = statement->GetValue(i, &value);
			if (ret != 0)
				break;
			switch (i) {
			case 0:
				entry.id = value.data.integer;
				break;
			case 1:
				entry.path.assign(value.data.text);
				break;
			}
		}
		entries->push_back(entry);
		if (query->maxCount <= entries->size())
			break;
	}
	return ret;
}

int ContentDatabaseImpl::FindContents(const ContentOperationQuery *query,
				      paf::vector<ContentOperationEntry> *entries)
{
	paf::string table;

	entries->clear();

	int ret = GetTableName(query->type, &table);

	if (ret < 0)
		return ret;

	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql = "SELECT mrid,content_path from " + table;

	{
		char size[64];

		sce_paf_snprintf(size, sizeof(size), "%lld", query->size);
		sql.append(" WHERE size=" + paf::string(size));
	}
	if (query->listId != 0) {
		char listId[64];

		sce_paf_snprintf(listId, sizeof(listId), "%lld", query->listId);
		sql.append(" AND mrid IN (SELECT item_id from tbl_AVContentItem WHERE list_id=" +
			   paf::string(listId) + ")");
	}
	ret = m_connection->Prepare(sql, &statement);
	if (ret != 0)
		return ret;
	return ReadEntries(statement, query, entries);
}

int ContentDatabaseImpl::FindPhotos(const ContentOperationQuery *query,
				    paf::vector<ContentOperationEntry> *entries)
{
	paf::string table;

	entries->clear();

	int ret = GetTableName(query->type, &table);

	if (ret < 0)
		return ret;

	dbutil::RefPtr<dbutil::Statement> statement;
	paf::string sql = "SELECT mrid,content_path from " + table;

	{
		char size[64];

		sce_paf_snprintf(size, sizeof(size), "%lld", query->size);
		sql.append(" WHERE size=" + paf::string(size));
	}
	if (query->listId != 0) {
		char listId[64];

		sce_paf_snprintf(listId, sizeof(listId), "%lld", query->listId);
		sql.append(" AND mrid IN (SELECT item_id from tbl_AVContentItem WHERE list_id=" +
			   paf::string(listId) + ")");
	}
	if (query->type == CONTENT_OPERATION_TYPE_PHOTO && query->codecType != 0) {
		char codecType[64];

		sce_paf_snprintf(codecType, sizeof(codecType), "%lld", (SceInt64)query->codecType);
		sql.append(" AND mrid IN (SELECT base_id from tbl_Photo WHERE codec_type=" +
			   paf::string(codecType) + ")");
	}
	ret = m_connection->Prepare(sql, &statement);
	if (ret != 0)
		return ret;
	return ReadEntries(statement, query, entries);
}

int ContentDeleter::Run(SceInt64 id, int type)
{
	paf::string path;
	int ret = m_database->GetContentPath(id, type, &path);

	if (ret < 0)
		return ret;
	return RemoveFile(path.c_str());
}

/* mounts empr0: and gives path's file on the mount */
int VideoContentDeleter::Mount(paf::string *mountedPath, paf::string path)
{
	paf::string relativePath;

	relativePath = path.substr(path.find(":") + 1, paf::string::npos);

	char id[36];

	if (SceVideoProfiler_B327FFA8(id) != 0)
		return CONTENT_OPERATION_ERROR_UNSUPPORTED;
	sce_paf_memset(&m_mountPoint, 0, sizeof(m_mountPoint));

	int ret = sceAppMgrLoopBackMount(CONTENT_OPERATION_EMPR_MOUNT_ID, id, &m_mountPoint);

	if (ret != 0 && ret != CONTENT_OPERATION_ERROR_MOUNTED)
		return CONTENT_OPERATION_ERROR_UNAVAILABLE;
	*mountedPath = paf::string() + m_mountPoint.drive_id + relativePath;
	m_mounted = ret != CONTENT_OPERATION_ERROR_MOUNTED;
	return 0;
}

/* whether path is on empr0: */
static inline bool IsEmprPath(paf::string path)
{
	return sce_paf_strncmp(path.c_str(), "empr0:", sce_paf_strlen("empr0:")) == 0;
}

/* removes a video's file with the files its profile names, empr0: mounted for it */
static inline int RemoveProfiledFiles(VideoContentDeleter *deleter, SceInt64 id, int type)
{
	VideoProfiler profiler;
	VideoProfilerContent *content = NULL;
	VideoProfilerFileList *files = NULL;
	paf::string path;
	paf::string profiledPath;
	int ret = deleter->m_database->GetContentPath(id, type, &path);

	if (ret < 0)
		return ret;
	if (IsEmprPath(path))
		deleter->Mount(&profiledPath, path);
	else
		profiledPath = path;
	SceVideoProfiler_4DBB7596(&profiler, profiledPath.c_str(), &content, 0);
	if (content == NULL)
		return ret;
	content->GetFileList(&files);
	if (files == NULL) {
		SceVideoProfiler_E5B10A6B(&profiler, content);
		return ret;
	}

	int count = files->GetCount();

	for (int i = 0; i < count; i++) {
		char *file = NULL;

		files->GetPath(i, &file);
		if (file != NULL) {
			RemoveFile(file);
			if (file != NULL) {
				delete[] file;
				file = NULL;
			}
		}
	}
	if (files != NULL)
		content->ReleaseFileList(files);
	SceVideoProfiler_E5B10A6B(&profiler, content);
	return RemoveFile(profiledPath.c_str());
}

int VideoContentDeleter::Run(SceInt64 id, int type)
{
	paf::Module *marlin = NULL;
	paf::Module *fflMp4 = NULL;
	paf::Module *senvuabs = NULL;
	paf::Module *videoProfiler = NULL;

	if (!m_modulesLoaded) {
		paf::Module::ExtraOption option;

		sce_paf_memset(&option, 0, sizeof(option));
		paf::Module *marlin = new paf::Module("vs0:vsh/common/libmarlin.suprx", NULL, 1,
						      &option);
		fflMp4 = new paf::Module("vs0:vsh/common/libFflMp4.suprx", NULL, 1, &option);
		senvuabs = new paf::Module("vs0:vsh/common/libSenvuabsFFsdk.suprx", NULL, 1,
					   &option);
		videoProfiler = new paf::Module("vs0:vsh/common/libvideoprofiler.suprx", NULL, 1,
						&option);
	}

	g_videoProfilerContext = m_context;
	if (g_videoProfilerContext != NULL)
		SceVideoProfiler_2DBE786B(m_context);

	int ret = RemoveProfiledFiles(this, id, type);

	if (m_mounted == true)
		sceAppMgrUmount(m_mountPoint.drive_id);
	if (videoProfiler != NULL)
		delete videoProfiler;
	if (senvuabs != NULL)
		delete senvuabs;
	if (fflMp4 != NULL)
		delete fflMp4;
	if (marlin != NULL)
		delete marlin;
	if (g_videoProfilerContext != NULL)
		g_videoProfilerContext = NULL;
	return ret;
}

int ContentDatabaseImpl::GetTableName(int type, paf::string *table)
{
	switch (type) {
	case CONTENT_OPERATION_TYPE_PHOTO:
	case CONTENT_OPERATION_TYPE_VIDEO:
		table->assign("tbl_vpcontent");
		return 0;
	case CONTENT_OPERATION_TYPE_MUSIC:
		table->assign("tbl_music");
		return 0;
	default:
		return CONTENT_OPERATION_ERROR_INVALID;
	}
}

/* a music file's path on music0: */
void ContentDatabaseImpl::ConvertMusicPath(paf::string *path)
{
	paf::string ux0Music("ux0:/music/");
	paf::string music0("music0:/");

	int diff = sce_paf_strcmp(path->substr(0, ux0Music.size()).c_str(), ux0Music.c_str());

	if (diff == 0)
		path->replace(0, ux0Music.size(), music0);
}

/* removes path's file; none is no error */
static __attribute__((noinline))
int RemoveFile(const char *path)
{
	int ret = paf::LocalFile::RemoveFile(path);

	if (ret < 0 && ret == CONTENT_OPERATION_ERROR_NO_FILE)
		ret = 0;
	return ret;
}
