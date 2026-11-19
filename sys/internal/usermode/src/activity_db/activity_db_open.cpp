/*
 * The library's open and close exports: the first open starts the activity daemon and the
 * network daemon, and the last close stops them.
 */
#include <libnet/netinternal.h>
#include "activity_db_internal.h"

/* the queue thread's priority, for the daemon's and the network daemon's */
#define ACTIVITY_DB_DAEMON_PRIORITY	0x1000010A

/* the titles whose statuses the activity feed's database drops when it opens */
#define ACTIVITY_DB_DROPPED_TITLE_0	"NPXS10998"
#define ACTIVITY_DB_DROPPED_TITLE_1	"NPXS10999"

/* the databases the library's exports opened, made at their first use */
struct ActivityDbDatabasesHolder {
	ActivityDbDatabasesHolder() : databases(NULL)
	{
	}

	ActivityDbDatabases *databases;
};

static inline ActivityDbDatabases **ActivityDbGetDatabases(void)
{
	static ActivityDbDatabasesHolder holder;

	return &holder.databases;
}

/* opens database at path, starting the daemons with the first one */
static int ActivityDbOpen(ActivityDbDatabases **databases, const char *path, int database)
{
	int result = 0;

	if (*databases == NULL) {
		SceUID threadId;

		*databases = new ActivityDbDatabases;
		result = SceActivityDb_FBB82B37()->Start(*databases, ACTIVITY_DB_DAEMON_PRIORITY);
		if (result != 0)
			return result;
		threadId = 0;
		result = SceActivityDb_7B68F69D()->Start(ACTIVITY_DB_DAEMON_PRIORITY, &threadId);
		if (result != 0)
			return result;
		SceNetInternal_9DBD2F60(threadId, 1, 0);
	}
	if ((*databases)->Get(database) == NULL) {
		result = SceActivityDb_FBB82B37()->OpenDatabase(path, database);
		if (result != 0)
			return result;
	}
	if (database == ACTIVITY_DB_DATABASE_ACTIVITY) {
		paf::vector<paf::string> titleIds;

		titleIds.push_back(paf::string(ACTIVITY_DB_DROPPED_TITLE_0));
		titleIds.push_back(paf::string(ACTIVITY_DB_DROPPED_TITLE_1));
		result = SceActivityDb_FBB82B37()->DeleteTitles(ACTIVITY_DB_DATABASE_ACTIVITY,
			&titleIds, NULL);
	}
	return result;
}

int SceActivityDb_B5012F40(const char *path, int flags)
{
	return ActivityDbOpen(ActivityDbGetDatabases(), path, ACTIVITY_DB_DATABASE_ACTIVITY);
}

int SceActivityDb_AD7BFF8F(void)
{
	ActivityDbDatabases **databases = ActivityDbGetDatabases();
	int result;

	if (*databases == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = SceActivityDb_FBB82B37()->CloseDatabase(ACTIVITY_DB_DATABASE_ACTIVITY);
	if (result != 0)
		return result;
	if ((*databases)->IsEmpty()) {
		SceActivityDb_7B68F69D()->Stop();
		SceActivityDb_FBB82B37()->Stop();
		delete *databases;
		*databases = NULL;
	}
	return 0;
}

int SceActivityDb_7B0C84A3(const char *path)
{
	return ActivityDbOpen(ActivityDbGetDatabases(), path, ACTIVITY_DB_DATABASE_NEWS);
}

int SceActivityDb_36B8046C(void)
{
	ActivityDbDatabases **databases = ActivityDbGetDatabases();
	int result;

	if (*databases == NULL)
		return ACTIVITY_DB_ERROR_NOT_READY;
	result = SceActivityDb_FBB82B37()->CloseDatabase(ACTIVITY_DB_DATABASE_NEWS);
	if (result != 0)
		return result;
	if ((*databases)->IsEmpty()) {
		SceActivityDb_7B68F69D()->Stop();
		SceActivityDb_FBB82B37()->Stop();
		delete *databases;
		*databases = NULL;
	}
	return 0;
}

int SceActivityDb_738D4B49(void)
{
	int result;

	result = SceActivityDb_FBB82B37()->Notify();
	if (result != 0)
		return result;
	return 0;
}
