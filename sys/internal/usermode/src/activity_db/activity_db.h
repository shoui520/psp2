/*
 * activity_db.h: SceActivityDb's exports (vs0:sys/external/activity_db.suprx) as their callers
 * use them. The library's NIDs reproduce with no known salt, so the link names are
 * SceActivityDb_<NID>. Most exports are methods of its database access object (Sony's object
 * name SceLsdbDbAcc), called with the object first.
 */
#ifndef ACTIVITY_DB_H
#define ACTIVITY_DB_H

#include <scetypes.h>

#ifdef __cplusplus
namespace dbutil {
class Connection;
}
typedef dbutil::Connection ActivityDbConnection;
extern "C" {
#else
typedef struct ActivityDbConnection ActivityDbConnection;
#endif

/* the database access object; opaque to its callers */
typedef struct ActivityDbAccess ActivityDbAccess;

/* takes the connection for a transaction (nested calls count); NULL when none is open */
ActivityDbConnection *SceActivityDb_82B45E74(ActivityDbAccess *access);

/* ends the transaction taken by SceActivityDb_82B45E74: commits when result is 0, else rolls back */
int SceActivityDb_11D77056(ActivityDbAccess *access, int result);

/* what a database is rebuilt for */
typedef struct ActivityDbRebuildParam {
	int state;
	int version;		/* the schema version */
	int language;		/* the system language */
	int region;
} ActivityDbRebuildParam;

/* rebuilds the database inside the transaction taken by SceActivityDb_82B45E74 */
int SceActivityDb_BF4343E0(ActivityDbAccess *access, const ActivityDbRebuildParam *param);

/* marks the database at path rebuilt */
int SceActivityDb_B9A3FDCC(const char *path);

/* runs count SQL statements on the connection */
int SceActivityDb_D3277649(ActivityDbConnection *connection, const char *const *sql, int count);

/* opens (or makes) the activity database at path with flags, through the library's one instance */
int SceActivityDb_B5012F40(const char *path, int flags);

#ifdef __cplusplus
}
#endif

#endif
