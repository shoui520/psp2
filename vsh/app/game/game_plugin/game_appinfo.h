/*
 * game_appinfo.h: the game plugin's helpers that read an application's livespace_db
 * information. The names are ours (no record names them).
 */
#ifndef _GAME_APPINFO_H
#define _GAME_APPINFO_H

#include "../../shell/src/livespace_db/livespace_db_internal.h"

/* an application's information from livespace_db (0x81018666) */
int gameAppGetInfo(const char *titleId, LsdbAppInfoPtr *info);

/*
 * Where an application is installed (0x810111C0), from its attribute and original path: 1 and
 * 2 for gro0:, 3 for ux0:.
 */
int gameAppGetStorage(const LsdbAppInfoPtr *info, int *storage);

#endif
