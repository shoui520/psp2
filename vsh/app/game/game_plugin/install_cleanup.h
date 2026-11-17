/*
 * install_cleanup.h: what an interrupted installation leaves behind, cleaned up. The names are
 * ours (no record names them).
 */
#ifndef _INSTALL_CLEANUP_H
#define _INSTALL_CLEANUP_H

#include <std/string>

#include "game_plugin.h"
#include "game_appinfo.h"

int gameCleanupApp(const char *path, const LsdbAppInfoPtr *info, paf::string *detail,
		   const void *keystone);
int gameCleanupAddcont(void);
int gameCleanupInstall(const char *path, const GameInstallPackage *package, const char *appDir,
		       const LsdbAppInfoPtr *info, paf::string *detail);

#endif
