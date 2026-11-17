/*
 * savedata_backup.h: the local save data backups (savedata_backup.cpp). Every name here is ours
 * (no record names them).
 */
#ifndef _SHELL_SAVEDATA_BACKUP_H
#define _SHELL_SAVEDATA_BACKUP_H

#include <scetypes.h>
#include <std/string>
#include <file/localfile.h>

/* a title's backup: its file's status and its number */
typedef struct ShellSavedataBackupInfo {
	paf::LocalFileStat stat;
	int number;
	int reserved;
} ShellSavedataBackupInfo;

typedef char ShellSavedataBackupInfoSizeCheck[sizeof(ShellSavedataBackupInfo) == 0x60 ? 1 : -1];

/* the current user's backup of a title: 0, or the backup of another account's (0x81006FBC) */
int shellSavedataBackupGetInfo(const paf::string &titleId, ShellSavedataBackupInfo *info);
/* a title's backup removed: the current user's, every user's, or one user's */
int shellSavedataBackupDeleteCurrentUser(const paf::string &titleId);		/* (0x81007C62) */
int shellSavedataBackupDeleteAll(const paf::string &titleId);			/* (0x81007DDA) */
int shellSavedataBackupDeleteUser(const paf::string &titleId, int user);	/* (0x81007EA2) */

#endif
