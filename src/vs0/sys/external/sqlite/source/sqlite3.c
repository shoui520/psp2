/* SQLite 3.7.6.3 with the VSH build configuration and PAF libc bindings. */

#include <psp2/kernel/threadmgr.h>
#include <stdint.h>

#define SQLITE_THREADSAFE			1
#define SQLITE_MUTEX_APPDEF			1
#define SQLITE_TEMP_STORE			1
#define SQLITE_DEFAULT_PAGE_SIZE		4096
#define SQLITE_DEFAULT_CACHE_SIZE		16
#define SQLITE_DEFAULT_FILE_FORMAT		4
#define SQLITE_ENABLE_UNLOCK_NOTIFY		1
#define SQLITE_ENABLE_COLUMN_METADATA		1
#define SQLITE_ENABLE_MEMSYS5			1
#define SQLITE_ENABLE_ATOMIC_WRITE		1
#define SQLITE_ENABLE_UPDATE_DELETE_LIMIT		1
#define SQLITE_OMIT_COMPILEOPTION_DIAGS		1
#define SQLITE_OMIT_BUILTIN_TEST			1
#define SQLITE_OMIT_UTF16				1
#define SQLITE_OMIT_COMPLETE			1
#define SQLITE_OMIT_DEPRECATED			1
#define SQLITE_OMIT_GET_TABLE			1
#define SQLITE_OMIT_ANALYZE			1
#define SQLITE_OMIT_REINDEX			1
#define SQLITE_OMIT_VACUUM			1
#define SQLITE_OMIT_AUTOVACUUM			1
#define SQLITE_OMIT_AUTOMATIC_INDEX		1
#define SQLITE_OMIT_VIRTUALTABLE			1
#define SQLITE_OMIT_WAL				1
#define SQLITE_OMIT_EXPLAIN			1
#define SQLITE_OMIT_TRACE			1
#define SQLITE_OMIT_AUTHORIZATION			1
#define SQLITE_OMIT_LOOKASIDE			1
#define SQLITE_OMIT_TCL_VARIABLE			1
#define SQLITE_OMIT_SCHEMA_PRAGMAS		1
#define SQLITE_OS_OTHER				1
#define HAVE_LOCALTIME_R				1
#define HAVE_USLEEP				1

#define malloc		sce_paf_malloc
#define realloc		ScePafStdc_0ADEA3AD
#define free		sce_paf_free
#define memcpy		sce_paf_memcpy
#define memset		sce_paf_memset
#define memcmp		sce_paf_memcmp
#define strcmp		sce_paf_strcmp
#define strncmp		sce_paf_strncmp
#define strlen		sce_paf_strlen
#define strchr		sceClibStrchr
#define strrchr		sceClibStrrchr

#include "sqlite-config/sqlite3.c"

/* SCE: the default mutex backend uses PSP2 lightweight mutex work areas. */
struct sqlite3_mutex {
	SceKernelLwMutexWork work;
};

static sqlite3_mutex g_staticMutexes[6];
static int g_mutexInitialized;
static const char *const g_mutexNames[] = {
	"SceSqliteFast",
	"SceSqliteRecursive",
	"SceSqliteStaticMaster",
	"SceSqliteStaticMem",
	"SceSqliteStaticOpen",
	"SceSqliteStaticPrng",
	"SceSqliteStaticLru",
	"SceSqliteStaticPmem",
};

static int psp2MutexInit(void)
{
	int index, result;

	if (!g_mutexInitialized) {
		memset(g_staticMutexes, 0, sizeof(g_staticMutexes));
		for (index = 0; index < 6; index++) {
			result = sceKernelCreateLwMutex(&g_staticMutexes[index].work,
						      g_mutexNames[index + 2], 0x2000,
						      0, NULL);
			if (result < 0)
				return SQLITE_NOMEM;
		}
		g_mutexInitialized = 1;
	}
	return SQLITE_OK;
}

static int psp2MutexEnd(void)
{
	int index;

	if (g_mutexInitialized) {
		for (index = 0; index < 6; index++) {
			if (sceKernelDeleteLwMutex(&g_staticMutexes[index].work) < 0)
				return SQLITE_NOMEM;
		}
		g_mutexInitialized = 0;
	}
	return SQLITE_OK;
}

static sqlite3_mutex *psp2MutexAlloc(int id)
{
	sqlite3_mutex *mutex;
	const char *name;
	int result;

	if (id >= SQLITE_MUTEX_STATIC_MASTER)
		return &g_staticMutexes[id - SQLITE_MUTEX_STATIC_MASTER];
	mutex = sqlite3Malloc(sizeof(*mutex));
	if (mutex) {
		memset(mutex, 0, sizeof(*mutex));
		name = (unsigned int)id < ArraySize(g_mutexNames) ?
			g_mutexNames[id] : "SceSqlite";
		result = sceKernelCreateLwMutex(&mutex->work, name,
					      id == SQLITE_MUTEX_RECURSIVE ? 0x2002 : 0x2000,
					      0, NULL);
		if (result < 0) {
			sqlite3_free(mutex);
			mutex = NULL;
		}
	}
	return mutex;
}

static void psp2MutexFree(sqlite3_mutex *mutex)
{
	if (mutex) {
		sceKernelDeleteLwMutex(&mutex->work);
		sqlite3_free(mutex);
	}
}

static void psp2MutexEnter(sqlite3_mutex *mutex)
{
	if (mutex)
		sceKernelLockLwMutex(&mutex->work, 1, NULL);
}

static int psp2MutexTry(sqlite3_mutex *mutex)
{
	if (mutex && sceKernelTryLockLwMutex(&mutex->work, 1) < 0)
		return SQLITE_BUSY;
	return SQLITE_OK;
}

static void psp2MutexLeave(sqlite3_mutex *mutex)
{
	if (mutex)
		sceKernelUnlockLwMutex(&mutex->work, 1);
}

SQLITE_PRIVATE const sqlite3_mutex_methods *sqlite3DefaultMutex(void)
{
	static const sqlite3_mutex_methods methods = {
		psp2MutexInit,
		psp2MutexEnd,
		psp2MutexAlloc,
		psp2MutexFree,
		psp2MutexEnter,
		psp2MutexTry,
		psp2MutexLeave,
		NULL,
		NULL,
	};

	return &methods;
}
