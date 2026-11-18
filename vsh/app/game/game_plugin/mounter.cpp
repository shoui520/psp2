/*
 * mounter.cpp: the game plugin's mounts, counted by user: an application's game data, its
 * application data, an installation's directory, the PSP emulator's work directory and a
 * theme. A second mount of the same thing returns the first one's mount point and counts it;
 * the last unmount unmounts it. The file's name is ours, from the mounter's mutex
 * ("SceGamePluginMounter"); the function names are ours too (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <std/string.h>
#include <std/string>
#include <std/map>
#include <thread/thread.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"

using namespace paf;

#define GAME_ERROR_DRIVE_MISMATCH	-2146430695	/* 0x80101119: mounted before from the other drive */

/* the drives sceAppMgrInstallDirMount mounts an installation's directory from */
#define GAME_INSTALL_DRIVE_GRW0		500
#define GAME_INSTALL_DRIVE_UX0		501

/* the mount IDs this file mounts by */
#define GAME_MOUNT_ID_APP_DATA		110
#define GAME_MOUNT_ID_PSPEMU_WORK	202

/* where an application's or a theme's license is written for the mount to read it */
#define GAME_RIF_WORK_DIR		"ur0:temp/rif"
#define GAME_RIF_WORK_FILE		"ur0:temp/rif/work.bin"

/* a mount and the count of its users */
struct GameMount {
	GameMount(string mountPoint) : mountPoint(mountPoint), refs(1)
	{
	}

	string mountPoint;
	int refs;
};

/* a mount made with a patch and a license: an application's game data */
struct GameLicensedMount : GameMount {
	GameLicensedMount(string mountPoint, const char *patchPath, const char *rifPath)
		: GameMount(mountPoint), patchPath(patchPath), rifPath(rifPath)
	{
	}

	string patchPath;
	string rifPath;
};

/* a theme, with its path and license */
struct GameThemeMount : GameMount {
	GameThemeMount(string mountPoint, const string &themePath, const string &rifPath)
		: GameMount(mountPoint), themePath(themePath), rifPath(rifPath)
	{
	}

	string themePath;
	string rifPath;
};

/* an installation's directory and the drive it was mounted from */
struct GameInstallDirMount : GameMount {
	GameInstallDirMount(string mountPoint, int drive) : GameMount(mountPoint), drive(drive)
	{
	}

	int drive;	/* GAME_INSTALL_DRIVE_* */
};

typedef map<string, GameMount *> GameMountMap;
typedef map<string, GameLicensedMount *> GameLicensedMountMap;
typedef map<string, GameInstallDirMount *> GameInstallDirMountMap;
typedef map<string, GameThemeMount *> GameThemeMountMap;

static GameMountMap s_appDataMounts;			/* by title ID */
static GameLicensedMountMap s_gameDataMounts;		/* by application path */
static GameInstallDirMountMap s_installDirMounts;	/* by installation name */
static GameMountMap s_workDirMounts;			/* one, "pspemu" */
static GameThemeMountMap s_themeMounts;			/* by content ID */
static thread::Mutex s_mounterMutex("SceGamePluginMounter", 0);

/* The drive a mount point stands for: the key it was mounted by, else cleared (0x8101D89C). */
void gameMountGetDrive(const string &path, string *drive)
{
	drive->clear();

	GameMountMap::iterator appDataEnd = s_appDataMounts.end();

	for (GameMountMap::iterator it = s_appDataMounts.begin(); it != appDataEnd; it++) {
		GameMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == path) {
			*drive = it->first;
			return;
		}
	}

	GameLicensedMountMap::iterator gameDataEnd = s_gameDataMounts.end();

	for (GameLicensedMountMap::iterator it = s_gameDataMounts.begin(); it != gameDataEnd; it++) {
		GameLicensedMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == path) {
			*drive = it->first;
			return;
		}
	}

	GameInstallDirMountMap::iterator installDirEnd = s_installDirMounts.end();

	for (GameInstallDirMountMap::iterator it = s_installDirMounts.begin(); it != installDirEnd; it++) {
		GameInstallDirMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == path) {
			if (mount->drive == GAME_INSTALL_DRIVE_GRW0)
				*drive = "grw0:";
			else if (mount->drive == GAME_INSTALL_DRIVE_UX0)
				*drive = "ux0:";
			else
				drive->clear();
			return;
		}
	}

	GameMountMap::iterator workDirEnd = s_workDirMounts.end();

	for (GameMountMap::iterator it = s_workDirMounts.begin(); it != workDirEnd; it++) {
		GameMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == path) {
			*drive = it->first;
			return;
		}
	}

	GameThemeMountMap::iterator themeEnd = s_themeMounts.end();

	for (GameThemeMountMap::iterator it = s_themeMounts.begin(); it != themeEnd; it++) {
		GameThemeMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == path) {
			*drive = mount->themePath;
			return;
		}
	}
}

/* An application's game data mounted, or the mount it already has counted (0x8101DBA0). */
int gameGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
		      string *mountPoint)
{
	thread::MutexLock lock(&s_mounterMutex);

	GameLicensedMountMap::iterator it = s_gameDataMounts.find(string(appPath));

	if (it != s_gameDataMounts.end()) {
		GameLicensedMount *mount = it->second;

		if (mount == NULL)
			return GAME_ERROR_NOT_READY;

		*mountPoint = mount->mountPoint;
		mount->refs++;
		return 0;
	}

	char drive[16];
	int ret = sceAppMgrGameDataMount(appPath, patchPath, rifPath, drive);

	if (ret < 0)
		return ret;

	GameLicensedMount *mount = new GameLicensedMount(string(drive), patchPath, rifPath);

	if (mount == NULL) {
		sceAppMgrUmount(drive);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	s_gameDataMounts[string(appPath)] = mount;
	*mountPoint = drive;
	return 0;
}

/*
 * An application's game data mounted, with its license written to ur0:temp/rif/work.bin
 * first when rif is given (0x8101DE16).
 */
int gameAppMount(const char *appPath, const char *patchPath, const void *rif, SceSize rifSize,
		 string *mountPoint)
{
	int ret = 0;

	gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
	Dir::CreateRecursive(GAME_RIF_WORK_DIR);

	string rifPath;

	if (rif != NULL) {
		rifPath = string(GAME_RIF_WORK_FILE);

		common::SharedPtr<File> file = LocalFile::Open(rifPath.c_str(),
							       SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC,
							       0006, &ret);

		if (ret < 0) {
			gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
			return ret;
		}

		ret = file->Write(rif, rifSize);
		if (ret < 0) {
			file->Close();
			gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
			return ret;
		}

		file->Close();
	}

	ret = gameGameDataMount(appPath, patchPath, rifPath.length() != 0 ? rifPath.c_str() : NULL,
				mountPoint);
	if (ret < 0) {
		gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
		return ret;
	}

	return 0;
}

/* An application's game data unmounted when its last user unmounts it (0x8101E0B2). */
int gameAppUnmount(string *mountPoint)
{
	thread::MutexLock lock(&s_mounterMutex);

	if (mountPoint->length() == 0)
		return 0;

	GameLicensedMountMap::iterator end = s_gameDataMounts.end();

	for (GameLicensedMountMap::iterator it = s_gameDataMounts.begin(); it != end; it++) {
		GameLicensedMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == *mountPoint) {
			if (mount->refs == 1) {
				int ret = sceAppMgrUmount(mountPoint->c_str());

				if (ret < 0)
					return ret;

				mount->refs--;
				delete mount;
				s_gameDataMounts.erase(it++);
				return 0;
			}

			mount->refs--;
			return 0;
		}
	}

	return 0;
}

/*
 * An installation's directory mounted with its keystone, from grw0: when dir is on it, else
 * from ux0:, or the mount it already has counted (0x8101E23C).
 */
int gamePfsMount(const string &dir, const string &name, const void *keystone, string *mountPoint)
{
	int drive = GAME_INSTALL_DRIVE_UX0;

	if (dir.find("grw0:") != string::npos)
		drive = GAME_INSTALL_DRIVE_GRW0;

	GameInstallDirMountMap::iterator it = s_installDirMounts.find(name);

	if (it != s_installDirMounts.end()) {
		GameInstallDirMount *mount = it->second;

		if (mount == NULL)
			return GAME_ERROR_NOT_READY;

		if (mount->drive == drive) {
			*mountPoint = mount->mountPoint;
			mount->refs++;
			return 0;
		}
		return GAME_ERROR_DRIVE_MISMATCH;
	}

	char drivePath[16];
	int ret = sceAppMgrInstallDirMount(drive, name.c_str(), keystone, drivePath);

	if (ret < 0)
		return ret;

	GameInstallDirMount *mount = new GameInstallDirMount(string(drivePath), drive);

	if (mount == NULL) {
		sceAppMgrUmount(drivePath);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	s_installDirMounts[name] = mount;
	*mountPoint = drivePath;
	return 0;
}

/* An installation's directory unmounted when its last user unmounts it (0x8101E434). */
int gamePfsUnmount(string *mountPoint)
{
	if (mountPoint->length() == 0)
		return 0;

	GameInstallDirMountMap::iterator end = s_installDirMounts.end();

	for (GameInstallDirMountMap::iterator it = s_installDirMounts.begin(); it != end; it++) {
		GameInstallDirMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == *mountPoint) {
			if (mount->refs == 1) {
				int ret = sceAppMgrUmount(mountPoint->c_str());

				if (ret < 0)
					return ret;

				mount->refs--;
				delete mount;
				s_installDirMounts.erase(it++);
				mountPoint->clear();
				return 0;
			}

			mount->refs--;
			mountPoint->clear();
			return 0;
		}
	}

	return 0;
}

/*
 * An application's data (mount ID 110) mounted, or the mount it already has counted (0x8101E56E);
 * reserved is not read (its callers pass 0 or 1).
 */
int gameAppDataMount(const string &titleId, string *mountPoint, bool reserved)
{
	thread::MutexLock lock(&s_mounterMutex);
	ScePfsRndDriveId drive;

	sce_paf_memset(&drive, 0, sizeof(drive));

	int ret = sceAppMgrAppDataMountById(GAME_MOUNT_ID_APP_DATA, titleId.c_str(), &drive);

	if (ret < 0) {
		if (ret == APPMGR_ERROR_ALREADY_MOUNTED) {
			GameMountMap::iterator it = s_appDataMounts.find(titleId);

			if (it == s_appDataMounts.end())
				return ret;

			GameMount *mount = it->second;

			if (mount == NULL)
				return GAME_ERROR_NOT_READY;

			*mountPoint = mount->mountPoint;
			mount->refs++;
			return 0;
		}
		return ret;
	}

	*mountPoint = drive.drive_id;

	GameMount *mount = new GameMount(*mountPoint);

	if (mount == NULL) {
		sceAppMgrUmount(drive.drive_id);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	s_appDataMounts[titleId] = mount;
	return 0;
}

/* An application's data unmounted when its last user unmounts it (0x8101E6F6). */
int gameAppDataUnmount(string *mountPoint)
{
	thread::MutexLock lock(&s_mounterMutex);

	if (mountPoint->length() == 0)
		return 0;

	GameMountMap::iterator end = s_appDataMounts.end();

	for (GameMountMap::iterator it = s_appDataMounts.begin(); it != end; it++) {
		GameMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == *mountPoint) {
			if (mount->refs == 1) {
				int ret = sceAppMgrUmount(mountPoint->c_str());

				if (ret < 0)
					return ret;

				mount->refs--;
				delete mount;
				s_appDataMounts.erase(it++);
				return 0;
			}

			mount->refs--;
			return 0;
		}
	}

	return 0;
}

/*
 * The PSP emulator's work directory (mount ID 202) mounted, or the mount it already has
 * counted (0x8101E84C).
 */
int gamePspemuWorkMount(string *mountPoint)
{
	ScePfsRndDriveId drive;

	sce_paf_memset(&drive, 0, sizeof(drive));

	int ret = sceAppMgrWorkDirMount(GAME_MOUNT_ID_PSPEMU_WORK, &drive);

	if (ret < 0) {
		if (ret == APPMGR_ERROR_ALREADY_MOUNTED) {
			GameMountMap::iterator it = s_workDirMounts.begin();

			if (it != s_workDirMounts.end()) {
				GameMount *mount = it->second;

				if (mount == NULL)
					return GAME_ERROR_NOT_READY;

				*mountPoint = mount->mountPoint;
				mount->refs++;
				return 0;
			}
		}
		return ret;
	}

	*mountPoint = drive.drive_id;

	GameMount *mount = new GameMount(*mountPoint);

	if (mount == NULL) {
		sceAppMgrUmount(drive.drive_id);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	s_workDirMounts[string("pspemu")] = mount;
	return 0;
}

/* The PSP emulator's work directory unmounted when its last user unmounts it (0x8101E9B6). */
int gamePspemuWorkUnmount(string *mountPoint)
{
	if (mountPoint->length() == 0)
		return 0;

	GameMountMap::iterator end = s_workDirMounts.end();

	for (GameMountMap::iterator it = s_workDirMounts.begin(); it != end; it++) {
		GameMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == *mountPoint) {
			if (mount->refs == 1) {
				int ret = sceAppMgrUmount(mountPoint->c_str());

				if (ret < 0)
					return ret;

				mount->refs--;
				delete mount;
				s_workDirMounts.erase(it++);
				mountPoint->clear();
				return 0;
			}

			mount->refs--;
			return 0;
		}
	}

	return 0;
}

/* A theme mounted, or the mount it already has counted (0x8101EAE2). */
int gameThemeDataMount(const string &contentId, const string &themePath, const string &rifPath,
		       string *mountPoint)
{
	thread::MutexLock lock(&s_mounterMutex);
	GameThemeMount *mount;
	GameThemeMountMap::iterator it = s_themeMounts.find(contentId);

	if (it != s_themeMounts.end()) {
		mount = it->second;
		if (mount == NULL)
			return GAME_ERROR_NOT_READY;

		mount->refs++;
	} else {
		char drive[16];

		sce_paf_memset(drive, 0, sizeof(drive));

		int ret = sceAppMgrThemeDataMount(contentId.c_str(),
						  themePath.length() != 0 ? themePath.c_str() : NULL,
						  rifPath.length() != 0 ? rifPath.c_str() : NULL,
						  drive);

		if (ret < 0)
			return ret;

		mount = new GameThemeMount(string(drive), themePath, rifPath);
		if (mount == NULL) {
			sceAppMgrUmount(drive);
			return GAME_ERROR_NO_INFO_MEMORY;
		}

		s_themeMounts[contentId] = mount;
	}

	*mountPoint = mount->mountPoint;
	return 0;
}

/* A theme mounted, with its license written to ur0:temp/rif/work.bin first when rif is given (0x8101ECBA). */
int gameThemeMount(const string &contentId, const string &themePath, const void *rif,
		   SceSize rifSize, string *mountPoint)
{
	int ret = 0;

	gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
	Dir::CreateRecursive(GAME_RIF_WORK_DIR);

	string rifPath;

	if (rif != NULL) {
		rifPath = string(GAME_RIF_WORK_FILE);

		common::SharedPtr<File> file = LocalFile::Open(rifPath.c_str(),
							       SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC,
							       0006, &ret);

		if (ret < 0) {
			gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
			return ret;
		}

		ret = file->Write(rif, rifSize);
		if (ret < 0) {
			file->Close();
			gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
			return ret;
		}

		file->Close();
	}

	ret = gameThemeDataMount(contentId, themePath, rifPath, mountPoint);
	if (ret < 0) {
		gameRemoveDirectory(GAME_RIF_WORK_DIR, 0, 0, 0, true);
		return ret;
	}

	return 0;
}

/* A theme unmounted when its last user unmounts it (0x8101EF4E). */
int gameThemeUnmount(string *mountPoint)
{
	thread::MutexLock lock(&s_mounterMutex);

	if (mountPoint->length() == 0)
		return 0;

	GameThemeMountMap::iterator end = s_themeMounts.end();

	for (GameThemeMountMap::iterator it = s_themeMounts.begin(); it != end; it++) {
		GameThemeMount *mount = it->second;

		if (mount != NULL && mount->mountPoint == *mountPoint) {
			if (mount->refs == 1) {
				int ret = sceAppMgrUmount(mountPoint->c_str());

				if (ret < 0)
					return ret;

				mount->refs--;
				delete mount;
				s_themeMounts.erase(it);
			} else {
				mount->refs--;
			}
			break;
		}
	}

	return 0;
}
