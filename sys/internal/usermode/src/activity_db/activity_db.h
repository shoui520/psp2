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
class Function;
}
typedef dbutil::Connection ActivityDbConnection;
typedef dbutil::Function ActivityDbFunction;
extern "C" {
#else
typedef struct ActivityDbConnection ActivityDbConnection;
typedef struct ActivityDbFunction ActivityDbFunction;
#endif

/* the database access object; opaque to its callers */
typedef struct ActivityDbAccess ActivityDbAccess;

/* the access object's size: callers allocate it and construct it with SceActivityDb_B2EF1EE0 */
#define ACTIVITY_DB_ACCESS_SIZE	64

/* constructs and destroys an access object (its mutex "SceLsdbDbAcc"); both return access */
ActivityDbAccess *SceActivityDb_B2EF1EE0(ActivityDbAccess *access);
ActivityDbAccess *SceActivityDb_6107381D(ActivityDbAccess *access);

/* takes the connection for a transaction (nested calls count); NULL when none is open */
ActivityDbConnection *SceActivityDb_82B45E74(ActivityDbAccess *access);

/* ends the transaction taken by SceActivityDb_82B45E74: commits when result is 0, else rolls back */
int SceActivityDb_11D77056(ActivityDbAccess *access, int result);

/* a decimal text's value (a time the database keeps as text) */
SceInt64 SceActivityDb_ADFC2C4C(const char *text);

/* a time's text (an rtc tick) in buf of size bytes; returns buf */
char *SceActivityDb_9A8944FC(const SceUInt64 *time, char *buf, int size);

/* what a database is rebuilt for */
typedef struct ActivityDbRebuildParam {
	int state;
	int version;		/* the schema version */
	int language;		/* the system language */
	int region;
} ActivityDbRebuildParam;

/* brings a database's tables up from version (0: a new database) */
typedef int (*ActivityDbUpgradeCallback)(ActivityDbConnection *connection, int version);

/* the database's open flags and its error codes (the last eight: the database was rebuilt) */
#define ACTIVITY_DB_OPEN_FLAGS		0x10000006
#define ACTIVITY_DB_ERROR_REBUILT_0	((int)0x801010F0)
#define ACTIVITY_DB_ERROR_REBUILT_1	((int)0x801010F1)
#define ACTIVITY_DB_ERROR_REBUILT_2	((int)0x801010F2)
#define ACTIVITY_DB_ERROR_REBUILT_3	((int)0x801010F3)
#define ACTIVITY_DB_ERROR_REBUILT_4	((int)0x801010F4)
#define ACTIVITY_DB_ERROR_REBUILT_5	((int)0x801010F5)
#define ACTIVITY_DB_ERROR_REBUILT_6	((int)0x801010FE)
#define ACTIVITY_DB_ERROR_REBUILT_7	((int)0x801010FF)

/*
 * Opens the database at path (":memory:" for one in memory) for param's state (a negative
 * state or version is refused), calling upgrade to bring its tables up.
 */
int SceActivityDb_AFB99B8D(ActivityDbAccess *access, const char *path, int flags,
	const struct ActivityDbRebuildParam *param, SceBool keepOld,
	ActivityDbUpgradeCallback upgrade);

/* closes the database */
int SceActivityDb_3D89266C(ActivityDbAccess *access);

/* deletes the database files at path (and its journal); marks it rebuilt first when rebuilt */
void SceActivityDb_1281238F(const char *path, SceBool rebuilt);

/* rebuilds the database inside the transaction taken by SceActivityDb_82B45E74 */
int SceActivityDb_BF4343E0(ActivityDbAccess *access, const ActivityDbRebuildParam *param);

/* reads the database's parameters */
int SceActivityDb_EA953F36(ActivityDbAccess *access, ActivityDbRebuildParam *param);

/* sets the database's parameters that param gives (those not negative) */
int SceActivityDb_7A345D77(ActivityDbAccess *access, ActivityDbRebuildParam *param);

/* marks the database at path rebuilt */
int SceActivityDb_B9A3FDCC(const char *path);

/* registers an SQL function on the database, and unregisters it */
int SceActivityDb_76F10DA7(ActivityDbAccess *access, const ActivityDbFunction *function);
int SceActivityDb_94516607(ActivityDbAccess *access, const ActivityDbFunction *function);

/* runs count SQL statements on the connection */
int SceActivityDb_D3277649(ActivityDbConnection *connection, const char *const *sql, int count);

/* opens (or makes) the activity database at path with flags, through the library's one instance */
int SceActivityDb_B5012F40(const char *path, int flags);

#ifdef __cplusplus
}
#endif

#endif
