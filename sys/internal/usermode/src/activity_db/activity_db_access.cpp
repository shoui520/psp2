/*
 * The activity database's access object: the connection, opened (and rebuilt when it was
 * damaged or built for another system), its transactions, and the database files' helpers.
 */
#include <string.h>
#include "activity_db_internal.h"

/* paf's error for a file that does not exist */
#define ACTIVITY_DB_PAF_ERROR_NO_ENTRY	((int)0x80AF5022)

/* the SQLite errors that mean the database file is damaged */
#define ACTIVITY_DB_SQLITE_CORRUPT	((int)(DBUTIL_ERROR_SQLITE | SQLITE_CORRUPT))
#define ACTIVITY_DB_SQLITE_EMPTY	((int)(DBUTIL_ERROR_SQLITE | SQLITE_EMPTY))
#define ACTIVITY_DB_SQLITE_FORMAT	((int)(DBUTIL_ERROR_SQLITE | SQLITE_FORMAT))
#define ACTIVITY_DB_SQLITE_NOTADB	((int)(DBUTIL_ERROR_SQLITE | SQLITE_NOTADB))

/* the database's busy timeout, in milliseconds */
#define ACTIVITY_DB_BUSY_TIMEOUT	30000

ActivityDbAccess::ActivityDbAccess()
	: m_connection(NULL), m_lock("SceLsdbDbAcc", 0), m_depth(0)
{
}

void ActivityDbAccess::Close()
{
	m_lock.Lock();
	if (m_connection != NULL) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
	}
	m_lock.Unlock();
}

ActivityDbAccess::~ActivityDbAccess()
{
	Close();
}

/* a dbutil error the caller sees: a damaged database's as ACTIVITY_DB_ERROR_REBUILT_7 */
static __attribute__((noinline)) int ActivityDbConvertError(int result)
{
	switch (result) {
	case ACTIVITY_DB_SQLITE_CORRUPT:
	case ACTIVITY_DB_SQLITE_EMPTY:
	case ACTIVITY_DB_SQLITE_FORMAT:
	case ACTIVITY_DB_SQLITE_NOTADB:
		return ACTIVITY_DB_ERROR_REBUILT_7;
	}
	return result;
}

int ActivityDbAccess::EndTransaction(int result)
{
	int ret = 0;

	if (m_connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (m_depth == 0)
		return ACTIVITY_DB_ERROR_NOT_READY;
	if (--m_depth == 0) {
		if (result != 0)
			ret = m_connection->Rollback();
		else
			ret = m_connection->Commit();
		for (paf::vector<ActivityDbEndCallback>::iterator it = m_endCallbacks.begin();
		     it != m_endCallbacks.end(); ++it)
			(*it)(ret != 0 ? ret : result);
		m_endCallbacks.clear();
	}
	m_lock.Unlock();
	return ActivityDbConvertError(ret);
}

/* the parameters the database was built for (its user_version) */
static int ActivityDbReadParam(dbutil::Connection *connection, ActivityDbRebuildParam *param,
	int *format, SceBool *reserved, SceBool *modelType)
{
	dbutil::RefPtr<dbutil::Statement> statement;
	dbutil::Value value;
	unsigned int version;
	int result;

	result = connection->Prepare(paf::string("PRAGMA user_version"), &statement);
	if (result >= 0) {
		result = statement->Step(1);
		if (result >= 0) {
			result = statement->GetValue(0, &value);
			if (result >= 0) {
				version = 0;
				if (value.type == SQLITE_INTEGER)
					version = (unsigned int)value.data.integer;
				param->state = version & 0xff;
				param->version = (version >> 8) & 0xff;
				param->language = (version >> 16) & 0x1f;
				param->region = (version >> 21) & 0x1f;
				if (format != NULL)
					*format = (version >> 26) & 7;
				if (reserved != NULL)
					*reserved = (version >> 29) & 1;
				if (modelType != NULL)
					*modelType = (version >> 30) & 1;
			}
		}
	}
	return result;
}

/*
 * Reads the database's parameters into current and compares them with param's: *rebuilt is
 * then why the database must be rebuilt (ACTIVITY_DB_ERROR_REBUILT_*), left as it was when
 * it need not be.
 */
int ActivityDbAccess::CheckParam(dbutil::Connection *connection,
	const ActivityDbRebuildParam *param, ActivityDbRebuildParam *current, int *rebuilt)
{
	int format = 0;
	SceBool reserved = 0;
	SceBool modelType = 0;
	int result;

	result = ActivityDbReadParam(connection, current, &format, &reserved, &modelType);
	if (result == 0) {
		if (current->version == 0 || current->state == 0)
			*rebuilt = ACTIVITY_DB_ERROR_REBUILT_4;
		else if (current->region != param->region || format != ACTIVITY_DB_VERSION_FORMAT ||
			 reserved != 0 || paf::system::Model::GetType() != modelType)
			*rebuilt = ACTIVITY_DB_ERROR_REBUILT_1;
		else if (param->version < current->version)
			*rebuilt = ACTIVITY_DB_ERROR_REBUILT_2;
		else if (param->state != current->state)
			*rebuilt = ACTIVITY_DB_ERROR_REBUILT_3;
		else if (param->language != current->language)
			*rebuilt = ACTIVITY_DB_ERROR_REBUILT_0;
	}
	return result;
}

dbutil::Connection *ActivityDbAccess::BeginTransaction()
{
	m_lock.Lock();
	if (m_connection == NULL || (m_depth == 0 && m_connection->Begin(0) != 0)) {
		m_lock.Unlock();
		return NULL;
	}
	m_depth++;
	return m_connection;
}

/* the user_version that holds a database's parameters */
static inline unsigned int ActivityDbMakeVersion(int state, int version, int language, int region)
{
	return (language & 0x1f) << 16 | ACTIVITY_DB_VERSION_FORMAT << 26 | (region & 0x1f) << 21 |
		(paf::system::Model::GetType() & 1) << 30 | (version & 0xff) << 8 | (state & 0xff);
}

/* writes the database's parameters into its user_version */
static int ActivityDbWriteParam(dbutil::Connection *connection,
	const ActivityDbRebuildParam *param)
{
	char sql[64];
	int result;

	sce_paf_snprintf(sql, sizeof(sql), "PRAGMA user_version = %d",
		ActivityDbMakeVersion(param->state, param->version, param->language, param->region));
	result = connection->Execute(paf::string(sql), 0, NULL, NULL, NULL, NULL);
	if (result >= 0)
		return 0;
	return result;
}

/*
 * Brings the tables up to param's version through upgrade. Returns why the database must
 * be rebuilt (ACTIVITY_DB_ERROR_REBUILT_*), 0 when it need not be, or the error.
 */
int ActivityDbAccess::Upgrade(const ActivityDbRebuildParam *param,
	ActivityDbUpgradeCallback upgrade)
{
	int rebuilt = 0;
	ActivityDbRebuildParam current;
	dbutil::Connection *connection;
	int result;

	current.state = -1;
	current.version = -1;
	current.language = -1;
	current.region = -1;
	connection = BeginTransaction();
	if (connection == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = CheckParam(connection, param, &current, &rebuilt);
	if (result == 0 && current.version < param->version) {
		result = upgrade(connection, current.version);
		if (result == 0) {
			current.version = param->version;
			result = ActivityDbWriteParam(connection, &current);
		}
	}
	EndTransaction(result);
	if (result != 0)
		return ActivityDbConvertError(result);
	return rebuilt;
}

static void ActivityDbOnError(void *arg, int result);

/* opens the connection on path and sets it up */
int ActivityDbAccess::OpenConnection(const char *path, int flags)
{
	dbutil::Connection *connection = m_connection;
	int result;

	result = connection->Open(paf::string(path), flags);
	if (result == 0) {
		dbutil::Connection::ErrorHandler handler;

		handler.callback = ActivityDbOnError;
		handler.arg = this;
		m_connection->SetErrorHandler(&handler);
		connection = m_connection;
		connection->SetBusyTimeout(ACTIVITY_DB_BUSY_TIMEOUT);
		connection->Execute(paf::string("PRAGMA foreign_keys=ON"), 0, NULL, NULL, NULL, NULL);
		connection->Execute(paf::string("PRAGMA recursive_triggers=ON"), 0, NULL, NULL, NULL,
				    NULL);
		connection->SetOption(DBUTIL_OPTION_CHANGES_ONLY, true);
	}
	return ActivityDbConvertError(result);
}

/* whether path names a database file (not none, nor one in memory) */
static inline bool ActivityDbIsFile(const char *path)
{
	return path != NULL && *path != '\0' && sce_paf_memcmp(path, ":memory:", 9) != 0;
}

void SceActivityDb_B9A3FDCC(const char *path)
{
	if (ActivityDbIsFile(path)) {
		paf::string file = paf::string(path) + "-err";

		SceActivityDb_8EE00DC5(file.c_str());
	}
}

void SceActivityDb_1281238F(const char *path, SceBool errorMark)
{
	if (ActivityDbIsFile(path)) {
		if (errorMark)
			SceActivityDb_B9A3FDCC(path);

		paf::string journal = paf::string(path) + "-journal";

		SceActivityDb_8EE00DC5(journal.c_str());
		SceActivityDb_8EE00DC5(path);
	}
}

int SceActivityDb_17420D1D(const char *path, int result)
{
	int ret = 0;

	if (ActivityDbIsFile(path) && result == ACTIVITY_DB_ERROR_REBUILT_7) {
		paf::string file = paf::string(path) + "-err";
		paf::common::SharedPtr<paf::File> mark =
			paf::LocalFile::Open(file.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
					     6, NULL);

		if (mark.get() == NULL || !mark->IsOpened())
			return ACTIVITY_DB_ERROR_DATABASE;
		if (mark->Write(&result, sizeof(result)) != sizeof(result))
			ret = ACTIVITY_DB_ERROR_DATABASE;
		mark->Close();
	}
	return ret;
}

int SceActivityDb_C3CAF7DA(const char *path)
{
	int result;

	if (!ActivityDbIsFile(path))
		return 0;

	paf::string file = paf::string(path) + "-err";
	paf::common::SharedPtr<paf::File> mark = paf::LocalFile::Open(file.c_str(), SCE_O_RDONLY,
								      0, NULL);

	if (mark.get() == NULL || !mark->IsOpened())
		return 0;
	result = 0;
	mark->Read(&result, sizeof(result));
	mark->Close();
	if (result == ACTIVITY_DB_ERROR_REBUILT_7 || result == ACTIVITY_DB_ERROR_REBUILT_6 ||
	    result == ACTIVITY_DB_ERROR_REBUILT_5)
		return result;
	SceActivityDb_B9A3FDCC(path);
	return 0;
}

/* whether a result is one of the reasons to rebuild the database */
static inline bool ActivityDbIsRebuilt(int result)
{
	return result == ACTIVITY_DB_ERROR_REBUILT_0 || result == ACTIVITY_DB_ERROR_REBUILT_1 ||
		result == ACTIVITY_DB_ERROR_REBUILT_2 || result == ACTIVITY_DB_ERROR_REBUILT_3 ||
		result == ACTIVITY_DB_ERROR_REBUILT_4 || result == ACTIVITY_DB_ERROR_REBUILT_5 ||
		result == ACTIVITY_DB_ERROR_REBUILT_6 || result == ACTIVITY_DB_ERROR_REBUILT_7;
}

int ActivityDbAccess::Open(const char *path, int flags, const ActivityDbRebuildParam *param,
	bool keepOld, ActivityDbUpgradeCallback upgrade)
{
	int result;

	m_lock.Lock();
	if (param->state < 0 || param->version < 0) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	}
	if (m_connection != NULL) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	m_connection = new dbutil::Connection();
	result = SceActivityDb_C3CAF7DA(path);
	if (result != 0) {
		if (result == ACTIVITY_DB_ERROR_REBUILT_5)
			goto reopen;
		goto rebuild;
	}
	result = OpenConnection(path, flags);
	if (result == ACTIVITY_DB_ERROR_REBUILT_7) {
		if (keepOld)
			goto rebuild;
		goto mark;
	}
	if (result == 0) {
		result = Upgrade(param, upgrade);
		if (result == ACTIVITY_DB_ERROR_REBUILT_7) {
			if (keepOld)
				goto rebuild;
			goto mark;
		}
		if (result != ACTIVITY_DB_ERROR_REBUILT_4 &&
		    (result == ACTIVITY_DB_ERROR_REBUILT_2 ||
		     (result != ACTIVITY_DB_ERROR_REBUILT_5 && result == ACTIVITY_DB_ERROR_REBUILT_1)))
			goto rebuild;
	}
	goto out;
mark:
	SceActivityDb_17420D1D(path, result);
rebuild:
	m_connection->Close();
	SceActivityDb_1281238F(path, 0);
reopen:
	if (OpenConnection(path, flags) != 0) {
		result = ACTIVITY_DB_ERROR_DATABASE;
	} else {
		int ret = Upgrade(param, upgrade);

		if (!ActivityDbIsRebuilt(ret) && ret != 0)
			result = ACTIVITY_DB_ERROR_DATABASE;
	}
out:
	if (keepOld && ActivityDbIsRebuilt(result))
		result = 0;
	m_lock.Unlock();
	return result;
}

void ActivityDbAccess::AddEndCallback(ActivityDbEndCallback callback)
{
	m_endCallbacks.push_back(callback);
}

void ActivityDbAccess::RegisterFunction(dbutil::Function *function)
{
	m_lock.Lock();
	if (m_connection != NULL && function != NULL)
		function->Register(m_connection);
	m_lock.Unlock();
}

void ActivityDbAccess::UnregisterFunction(dbutil::Function *function)
{
	m_lock.Lock();
	if (m_connection != NULL && function != NULL)
		function->Unregister(m_connection);
	m_lock.Unlock();
}

int ActivityDbAccess::GetParam(ActivityDbRebuildParam *param)
{
	return ActivityDbReadParam(m_connection, param, NULL, NULL, NULL);
}

int ActivityDbAccess::SetParam(const ActivityDbRebuildParam *param)
{
	return ActivityDbWriteParam(m_connection, param);
}

int ActivityDbAccess::UpdateParam(ActivityDbRebuildParam *param)
{
	ActivityDbRebuildParam current;
	int result;

	current.state = -1;
	current.version = -1;
	current.language = -1;
	current.region = -1;
	result = ActivityDbReadParam(m_connection, &current, NULL, NULL, NULL);
	if (result != 0)
		return result;
	if (param->state < 0)
		param->state = current.state;
	if (param->version < 0)
		param->version = current.version;
	if (param->language < 0)
		param->language = current.language;
	if (param->region < 0)
		param->region = current.region;
	return ActivityDbWriteParam(m_connection, param);
}

/* told of the connection's SQLite errors: a damaged database is marked for its rebuild */
static void ActivityDbOnError(void *arg, int result)
{
	dbutil::Connection *connection;

	if (result != SQLITE_CORRUPT)
		return;
	connection = ((ActivityDbAccess *)arg)->m_connection;
	if (connection == NULL)
		return;

	paf::string path = connection->GetDatabasePath(paf::string("main"));

	if (path.size() != 0)
		SceActivityDb_17420D1D(path.c_str(), ACTIVITY_DB_ERROR_REBUILT_7);
}

int SceActivityDb_D3277649(ActivityDbConnection *connection, const char *const *sql, int count)
{
	int result = 0;
	int i;

	for (i = 0; i < count; i++, sql++) {
		int ret = connection->Execute(paf::string(*sql), 0, NULL, NULL, NULL, NULL);

		result = 0;
		if (ret != 0)
			result = ret;
		if (result != 0)
			break;
	}
	return result;
}

/* steps statement to its last row: 0, or the error */
int ActivityDbStepAll(dbutil::RefPtr<dbutil::Statement> *statement)
{
	int result;

	do
		result = (*statement)->Step(1);
	while (result == 0);
	if (result == 1)
		return 0;
	return result;
}

/* a tick's decimal text */
static inline paf::string ActivityDbTickString(paf::rtc::Tick tick)
{
	char text[64];

	sce_paf_snprintf(text, sizeof(text), "%lld", tick);
	return paf::string(text);
}

/* removes the file at path; one that cannot be removed is renamed aside (its name and a tick) */
void SceActivityDb_8EE00DC5(const char *path)
{
	int result;
	int i;

	result = paf::LocalFile::RemoveFile(path);
	if (result == 0 || result == ACTIVITY_DB_PAF_ERROR_NO_ENTRY)
		return;
	for (i = 0; i < 5; i++) {
		paf::rtc::Tick tick = 0;

		paf::rtc::GetCurrentTick(&tick);

		paf::string aside = paf::string(path) + ActivityDbTickString(tick);

		if (paf::LocalFile::RenameFile(path, aside.c_str()) == 0)
			break;
	}
}

SceInt64 SceActivityDb_ADFC2C4C(const char *text)
{
	paf::rtc::Tick tick = 0;

	paf::rtc::ParseSQLiteDateTime(&tick, text);
	return tick;
}

char *SceActivityDb_9A8944FC(const SceUInt64 *time, char *buf, int size)
{
	paf::rtc::FormatSQLiteDateTime(buf, size, time, ' ');
	return buf;
}
