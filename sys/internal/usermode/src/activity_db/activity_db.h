/*
 * activity_db.h: SceActivityDb's plain exports (vs0:sys/external/activity_db.suprx). The
 * library's NIDs reproduce with no known salt, so these link names are SceActivityDb_<NID>.
 * Most exports are methods of its C++ objects (the database access object, Sony's object name
 * SceLsdbDbAcc, the query and sync handles, the daemons): those are declared with their classes
 * in activity_db_internal.h, under their C++ link names.
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

/* deletes the database at path and its journal, and its error mark ("-err") when errorMark */
void SceActivityDb_1281238F(const char *path, bool errorMark);

/* removes the file at path; one that cannot be removed is renamed aside */
void SceActivityDb_8EE00DC5(const char *path);

/* marks the database at path damaged ("-err" holds result) when result says so */
int SceActivityDb_17420D1D(const char *path, int result);

/* the result the database at path was marked damaged with, else 0 (and the mark removed) */
int SceActivityDb_C3CAF7DA(const char *path);

/* deletes the error mark ("-err") of the database at path */
void SceActivityDb_B9A3FDCC(const char *path);

/* runs count SQL statements on the connection */
int SceActivityDb_D3277649(ActivityDbConnection *connection, const char *const *sql, int count);

/* what ActivityDbAccess::AddEndCallback calls when the transaction ends (0: committed) */
typedef void (*ActivityDbEndCallback)(int result);

/* the activity daemon (its mutex "SceLsdbActDaemon"), made at its first use */
typedef struct ActivityDbDaemon ActivityDbDaemon;
ActivityDbDaemon *SceActivityDb_FBB82B37(void);

/* opens (or makes) the activity database at path with flags, through the library's one instance */
int SceActivityDb_B5012F40(const char *path, int flags);

/* the system's title ID of an NP title ID (the title itself when it has none) */
const char *SceActivityDb_F138D4E5(const char *npTitleId);
const char *SceActivityDb_575DA6A6(const char *titleId);

/* the system title this build posts as (its NP title ID's system title ID); the kit builds have none */
const char *SceActivityDb_E6C23731(void);

/* whether this is a PS TV whose vs0 has the system title this build posts as */
int SceActivityDb_6D8000C1(void);

#ifdef __cplusplus
}
#endif

#endif
