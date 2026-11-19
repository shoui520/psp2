/*
 * The titles the activity feed knows by two IDs (an NP title ID and the system's), and the
 * daemon's databases: which one a status's rowid or a query's type is in, and opening and
 * closing them.
 */
#include <regmgr/regmgr.h>
#include "activity_db_internal.h"

/* a system title that posts statuses under an NP title ID, in the region it ships in */
struct ActivityDbTitle {
	int region;			/* sceRegMgrSrvGetRegion's */
	const char *npTitleId;
	const char *titleId;
};

static const ActivityDbTitle activityDbTitles[] = {
	{ 1, "NPIA00036", "NPXS10093" },
	{ 2, "NPIA00054", "NPXS10102" },
};

#define ACTIVITY_DB_TITLE_COUNT	(sizeof(activityDbTitles) / sizeof(activityDbTitles[0]))

/*
 * The NP title ID this build posts as: the retail build's is its region's title, the kit builds
 * have none (Sony's code there compares against a constant NULL)
 */
static inline const char *ActivityDbGetOwnNpTitleId(void)
{
#if FW_DEVKIT
	return NULL;
#else
	int region = sceRegMgrSrvGetRegion();

	for (SceUInt32 i = 0; i < ACTIVITY_DB_TITLE_COUNT; i++) {
		if (region == activityDbTitles[i].region)
			return activityDbTitles[i].npTitleId;
	}
	return NULL;
#endif
}

const char *SceActivityDb_575DA6A6(const char *titleId)
{
	for (SceUInt32 i = 0; i < ACTIVITY_DB_TITLE_COUNT; i++) {
		if (sce_paf_strcmp(titleId, activityDbTitles[i].titleId) == 0) {
			const char *own = ActivityDbGetOwnNpTitleId();

			if (sce_paf_strcmp(own, activityDbTitles[i].npTitleId) == 0)
				return own;
		}
	}
	return titleId;
}

const char *SceActivityDb_F138D4E5(const char *npTitleId)
{
	for (SceUInt32 i = 0; i < ACTIVITY_DB_TITLE_COUNT; i++) {
		if (sce_paf_strcmp(npTitleId, activityDbTitles[i].npTitleId) == 0)
			return activityDbTitles[i].titleId;
	}
	return npTitleId;
}

/* the result of the last look for this build's title on vs0 (0: it is there) */
static int s_activityDbTitleStat = -1;

/* whether this is a PS TV that has the system title this build posts as */
int SceActivityDb_6D8000C1(void)
{
	if (paf::system::Model::GetType() != paf::system::Model::Type_Dolce)
		return 0;

	const char *npTitleId = ActivityDbGetOwnNpTitleId();

	if (npTitleId == NULL)
		return 0;
	if (s_activityDbTitleStat == 0)
		return 1;

	paf::LocalFileStat stat;
	paf::string path("vs0:app/");

	path += SceActivityDb_F138D4E5(npTitleId);
	path += "/sce_sys/param.sfo";
	s_activityDbTitleStat = paf::LocalFile::Getstat(path.c_str(), &stat);
	return s_activityDbTitleStat == 0;
}

const char *SceActivityDb_E6C23731(void)
{
	const char *own = ActivityDbGetOwnNpTitleId();

	for (SceUInt32 i = 0; i < ACTIVITY_DB_TITLE_COUNT; i++) {
		if (sce_paf_strcmp(own, activityDbTitles[i].npTitleId) == 0)
			return activityDbTitles[i].titleId;
	}
	return own;
}

int ActivityDbGetRowidDatabase(SceInt64 rowid)
{
	return rowid < ACTIVITY_DB_ACTIVITY_ROWID_BASE ? ACTIVITY_DB_DATABASE_NEWS :
		ACTIVITY_DB_DATABASE_ACTIVITY;
}

int ActivityDbGetCommentRowidDatabase(SceInt64 rowid)
{
	return rowid < ACTIVITY_DB_ACTIVITY_ROWID_BASE ? ACTIVITY_DB_DATABASE_NEWS :
		ACTIVITY_DB_DATABASE_ACTIVITY;
}

int ActivityDbGetQueryDatabase(int type)
{
	return type == ACTIVITY_DB_QUERY_NEWS ? ACTIVITY_DB_DATABASE_NEWS :
		ACTIVITY_DB_DATABASE_ACTIVITY;
}

ActivityDbDatabases::ActivityDbDatabases()
{
	m_access[ACTIVITY_DB_DATABASE_ACTIVITY] = NULL;
	m_access[ACTIVITY_DB_DATABASE_NEWS] = NULL;
	m_count = 0;
}

__attribute__((noinline)) int ActivityDbDatabases::Open(int database, const char *path,
	int flags, const ActivityDbRebuildParam *param, bool keepOld,
	ActivityDbUpgradeCallback upgrade)
{
	if (database >= ACTIVITY_DB_DATABASE_COUNT)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;
	if (m_access[database] != NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;

	ActivityDbAccess *access = new ActivityDbAccess;

	if (access == NULL)
		return ACTIVITY_DB_ERROR_NO_MEMORY;

	int result = access->Open(path, flags, param, keepOld, upgrade);

	if (result != 0) {
		delete access;
		return result;
	}
	m_access[database] = access;
	m_count++;
	return 0;
}

__attribute__((noinline)) int ActivityDbDatabases::Close(int database)
{
	if (database >= ACTIVITY_DB_DATABASE_COUNT)
		return ACTIVITY_DB_ERROR_INVALID_ARGUMENT;

	if (m_access[database] == NULL)
		return 0;
	m_access[database]->Close();
	delete m_access[database];
	m_access[database] = NULL;
	m_count--;
	return 0;
}

__attribute__((noinline)) bool ActivityDbDatabases::IsEmpty()
{
	return m_count == 0;
}

__attribute__((noinline)) ActivityDbAccess *ActivityDbDatabases::Get(int database)
{
	return m_access[database];
}

__attribute__((noinline)) ActivityDbAccess *ActivityDbDatabases::GetByRowid(SceInt64 rowid)
{
	return m_access[ActivityDbGetRowidDatabase(rowid)];
}

__attribute__((noinline)) ActivityDbAccess *ActivityDbDatabases::GetByType(int type)
{
	return m_access[ActivityDbGetQueryDatabase(type)];
}
