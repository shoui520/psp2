/*
 * An application's license files: a license written into its folder, and the free-trial license
 * moved into ux0:license/app/<title ID>/, or removed (0x8103E944..0x8103FB34). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* where the applications' licenses are, by title ID */
#define GAME_LICENSE_APP_DIR	"ux0:license/app/"

/* the size of a license's file name */
#define GAME_RIF_NAME_SIZE	48

/* the content types whose licenses are written: applications (21, 22) and themes (31) */
#define GAME_LICENSE_CONTENT_APP		21
#define GAME_LICENSE_CONTENT_APP_ADDCONT	22
#define GAME_LICENSE_CONTENT_THEME	31

/* the license file's flag set when the free-trial license moves over one already there */
#define GAME_LICENSE_FLAG_ACTIVATED	1

/*
 * PLACEHOLDER: a flag set in a 128-byte license file, at its offset 124, under the license
 * files' lock (0x810A53CC, in another file of the shell).
 */
int gameLicenseFileSetFlag(common::SharedPtr<File> file, SceUInt32 flag);

/*
 * buf written as the file name in the folder dir, made first if it is not there; path is left
 * the file's path. On a failure detail is left the drive of dir or path; on a failed write the
 * file is removed (0x8103E944).
 */
int gameFileWriteToDir(const void *buf, SceSize size, const string *dir, const char *name,
		       string *path, string *detail)
{
	int ret = 0;

	gameDebugDump("rif data", buf, size);

	if (!Dir::IsExist(dir->c_str())) {
		ret = Dir::CreateRecursive(dir->c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(*dir, detail);
			return ret;
		}
	}

	*path = *dir + string("/") + string(name);

	common::SharedPtr<File> file =
		LocalFile::Open(path->c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (ret < 0) {
		gamePathGetMountedDrive(*path, detail);
		return ret;
	}

	ret = file->Write(buf, size);
	if (ret < 0) {
		file->Close();
		LocalFile::RemoveFile(path->c_str());
		gamePathGetMountedDrive(*path, detail);
		return ret;
	}

	file->Close();

	return 0;
}

/*
 * An application's or a theme's license written into the folder dir under the name SceNpDrm
 * gives it; path is left its path (0x8103ECEE).
 */
int gameLicenseWrite(SceUInt32 contentType, const void *license, SceSize licenseSize,
		     const string *dir, string *path, string *detail, int arg6, int arg7)
{
	gameDebugDump("rif data", license, licenseSize);

	switch (contentType) {
	case GAME_LICENSE_CONTENT_APP:
	case GAME_LICENSE_CONTENT_APP_ADDCONT:
	case GAME_LICENSE_CONTENT_THEME:
		break;
	default:
		return GAME_ERROR_BAD_STORAGE;
	}

	string name;
	char rifName[GAME_RIF_NAME_SIZE];

	sce_paf_memset(rifName, 0, sizeof(rifName));

	int ret = sceNpDrmGetRifNameForInstall(rifName, license, 1);

	if (ret < 0)
		return ret;

	name.assign(rifName, sce_paf_strlen(rifName));

	ret = gameFileWriteToDir(license, licenseSize, dir, name.c_str(), path, detail);
	if (ret < 0)
		return ret;

	return 0;
}

/* The user's free-trial license of the application titleId removed (0x8103EDDC). */
int gameFttRemove(const string &titleId)
{
	string fttName;
	int ret = gameNpDrmGetFttName(&fttName);

	if (ret < 0)
		return ret;

	string path = string(GAME_LICENSE_APP_DIR) + titleId + string("/") + fttName;

	if (LocalFile::Exists(path.c_str())) {
		ret = LocalFile::RemoveFile(path.c_str());
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* The license file at path marked activated (0x8103F100). */
int gameLicenseActivate(const string *path)
{
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path->c_str(), SCE_O_RDWR, 0, &ret);

	if (ret < 0)
		return ret;

	ret = gameLicenseFileSetFlag(file, GAME_LICENSE_FLAG_ACTIVATED);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The free-trial license at path moved into the application titleId's license folder, over the
 * one there. It is marked activated first when the one it replaces was, or when the folder has
 * the application's own license (0x8103F2B8).
 */
int gameFttInstall(const string *path, const string &titleId)
{
	string fttName;
	int ret = gameNpDrmGetFttName(&fttName);

	if (ret < 0)
		return ret;

	string dir = string(GAME_LICENSE_APP_DIR) + titleId;
	string fttPath = dir + string("/") + fttName;
	bool activate = false;

	if (Dir::IsExist(dir.c_str())) {
		if (LocalFile::Exists(fttPath.c_str())) {
			GameFttInfo info;

			sce_paf_memset(&info, 0, sizeof(info));

			ret = gamePluginFttRead(titleId.c_str(), &info);
			if (ret < 0)
				return ret;

			if (info.flag)
				activate = true;

			ret = LocalFile::RemoveFile(fttPath.c_str());
			if (ret < 0)
				return ret;
		}
	} else {
		ret = Dir::CreateRecursive(dir.c_str(), 6);
		if (ret < 0)
			return ret;
	}

	if (!activate) {
		string rifName;

		ret = gameNpDrmGetRifName(&rifName);
		if (ret < 0)
			return ret;

		string rifPath = string(GAME_LICENSE_APP_DIR) + titleId + string("/") + rifName;

		if (LocalFile::Exists(rifPath.c_str()))
			activate = true;
	}

	if (activate) {
		ret = gameLicenseActivate(path);
		if (ret < 0)
			return ret;
	}

	ret = LocalFile::RenameFile(path->c_str(), fttPath.c_str());
	if (ret < 0)
		return ret;

	return ret;
}
