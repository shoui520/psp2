/*
 * The application database (ur0:shell/db/app.db): the activity database's access object, and
 * its daemons (application information, new events, LiveArea) opened and closed with it.
 */
#include "livespace_db_internal.h"
#include <dir/dir.h>

/* the database's directory */
#define LSDB_DB_DIR		"ur0:shell/db"
#define LSDB_DB_VERSION		7	/* the tables' version */

/* whether an open's result says the database was rebuilt (the open still succeeded) */
static inline bool LsdbDbRebuilt(int result)
{
	return result == ACTIVITY_DB_ERROR_REBUILT_0 || result == ACTIVITY_DB_ERROR_REBUILT_1 ||
		result == ACTIVITY_DB_ERROR_REBUILT_2 || result == ACTIVITY_DB_ERROR_REBUILT_3 ||
		result == ACTIVITY_DB_ERROR_REBUILT_4 || result == ACTIVITY_DB_ERROR_REBUILT_5 ||
		result == ACTIVITY_DB_ERROR_REBUILT_6 || result == ACTIVITY_DB_ERROR_REBUILT_7;
}

/*
 * Opens the database (in memory when inMemory) for state, a negative state deleting the old
 * one first, then each daemon on it. Returns the open's error, or its rebuilt code.
 */
int LsdbDatabase::Open(int state, bool inMemory)
{
	const char *path = ":memory:";
	int rebuilt = 0;
	ActivityDbRebuildParam param;
	ActivityDbAccess *access;
	void *memory;
	int result;

	if (!inMemory) {
		path = g_lsdbAppDbPath;
		if (!paf::Dir::IsExist(LSDB_DB_DIR))
			paf::Dir::CreateRecursive(LSDB_DB_DIR, 6);
		if (state < 0)
			SceActivityDb_1281238F(path, 1);
	}
	param.state = sce_paf_abs(state);
	param.language = SceLsdb_68059E87();
	param.region = LsdbGetRegion();
	access = NULL;
	param.version = LSDB_DB_VERSION;
	memory = operator new(ACTIVITY_DB_ACCESS_SIZE);
	if (memory != NULL)
		access = SceActivityDb_B2EF1EE0((ActivityDbAccess *)memory);
	m_access = access;
	result = SceActivityDb_AFB99B8D(access, path, ACTIVITY_DB_OPEN_FLAGS, &param, 0,
		LsdbDatabase::Upgrade);
	if (LsdbDbRebuilt(result))
		rebuilt = result;
	else if (result != 0)
		goto out;
	result = AppInfoDaemon::GetInstance()->Open(m_access, rebuilt);
	if (result == 0) {
		result = NeDaemon::GetInstance()->Open(m_access);
		if (result == 0) {
			result = LiDaemon::GetInstance()->Open(m_access);
			if (result == 0)
				result = LsdbTitleListenerInit();
		}
	}
out:
	return result != 0 ? result : rebuilt;
}

/* closes the daemons and the database */
int LsdbDatabase::Close()
{
	LsdbTitleListenerTerm();
	LiDaemon::GetInstance()->Close();
	NeDaemon::GetInstance()->Close();
	AppInfoDaemon::GetInstance()->Close();
	if (m_access != NULL) {
		SceActivityDb_3D89266C(m_access);
		if (m_access != NULL)
			operator delete(SceActivityDb_6107381D(m_access));
		m_access = NULL;
	}
	return 0;
}

LsdbDatabase *LsdbDatabase::GetInstance()
{
	static LsdbDatabase s_instance;

	return &s_instance;
}

/* sets the system language: the icons' and LiveAreas' titles follow it */
int LsdbDatabase::SetLanguage(int language)
{
	int appInfoResult;
	int liveAreaResult;
	ActivityDbRebuildParam param;

	if (SceActivityDb_82B45E74(m_access) == NULL)
		return LSDB_ERROR_NOT_READY;
	appInfoResult = AppInfoDaemon::GetInstance()->UpdateIconTitles(language);
	liveAreaResult = LiDaemon::GetInstance()->UpdateTitles(paf::string((const char *)NULL, 0),
		language, NULL);
	if (appInfoResult == 0 && liveAreaResult == 0) {
		param.state = -1;
		param.version = -1;
		param.language = language;
		param.region = -1;
		SceActivityDb_7A345D77(m_access, &param);
	}
	SceActivityDb_11D77056(m_access, 0);
	return 0;
}

/* the language the database's titles are in */
int LsdbDatabase::GetLanguage()
{
	ActivityDbRebuildParam param;
	int result;

	param.state = -1;
	param.version = -1;
	param.language = -1;
	param.region = -1;
	result = SceActivityDb_EA953F36(m_access, &param);
	if (result == 0)
		result = param.language;
	return result;
}

/* deletes the new events and the LiveAreas' updated titles */
int LsdbDatabase::ResetEvents()
{
	if (NeDaemon::GetInstance()->BeginTransaction() == NULL)
		return LSDB_ERROR_NOT_READY;
	NeDaemon::GetInstance()->DeleteEvents(0);
	LiDaemon::GetInstance()->UpdateTitles(paf::string(""), -1, NULL);
	NeDaemon::GetInstance()->EndTransaction(0);
	return 0;
}

int LsdbDatabase::GetOpenResult()
{
	return AppInfoDaemon::GetInstance()->GetOpenResult();
}

/* the configuration's text 1 */
int LsdbDatabase::GetConfigText(paf::string *text)
{
	*text = AppInfoDaemon::GetInstance()->GetConfigString(1, "");
	return 0;
}

int LsdbDatabase::SetConfigText(const paf::string *text)
{
	return AppInfoDaemon::GetInstance()->SetConfigString(1, text->c_str());
}

/* brings each daemon's tables up from version */
int LsdbDatabase::Upgrade(ActivityDbConnection *connection, int version)
{
	int result = AppInfoDaemon::Upgrade(connection, version);

	if (result == 0) {
		result = LiDaemon::Upgrade(connection, version);
		if (result == 0) {
			result = NeDaemon::Upgrade(connection, version);
			if (result == 0)
				return 0;
		}
	}
	return result;
}
