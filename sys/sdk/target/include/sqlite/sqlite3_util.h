/*
 * SceSqlite extension (SDK sqlite/sqlite3_util.h): the allocator SQLite's
 * malloc/realloc/free go to.
 */

#ifndef _SCE_SQLITE3_UTIL_H
#define _SCE_SQLITE3_UTIL_H

#include <stddef.h>

typedef struct SceSqliteMallocMethods {
	void *(*fMalloc)(size_t);
	void *(*fRealloc)(void *, size_t);
	void (*fFree)(void *);
} SceSqliteMallocMethods;

int sceSqliteConfigMallocMethods(SceSqliteMallocMethods *mf);

#endif /* _SCE_SQLITE3_UTIL_H */
