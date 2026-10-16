/*
 * SceSqlite: SQLite 3.6.23.1, built from the unmodified upstream amalgamation in
 * contrib/sqlite-3.6.23.1 with the options Sony's build records in its
 * compile-option list (sqlite3_compileoption_get).
 */

#define SQLITE_THREADSAFE			1
#define SQLITE_TEMP_STORE			1
#define SQLITE_ENABLE_MEMORY_MANAGEMENT		1
#define SQLITE_OS_OTHER				1	/* Sony supplies the VFS (os_psp2.c) */
#define HAVE_LOCALTIME_R			1	/* libc_psp2.c */

/* SCE: use this module's decimal conversion, not SNC's _Stoul macro. */
#include <stdlib.h>
#undef atoi

#include "contrib/sqlite-3.6.23.1/sqlite3.c"
