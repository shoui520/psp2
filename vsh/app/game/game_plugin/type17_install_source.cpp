/*
 * The game plugin's source of a package of content type 17, a PSP game's saved data: the folder
 * moved to ux0:pspemu/PSP/SAVEDATA/<title ID>, the old one moved aside to ux0:pspemu/temp/promote
 * first (0x810948D8..0x81094C86). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameType17InstallSource::GameType17InstallSource(const SceNpDrmInstallInfo *info,
						 string *errorTitleId)
	: m_info(info), m_errorTitleId(errorTitleId)
{
}

GameType17InstallSource::~GameType17InstallSource()
{
	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
}

int GameType17InstallSource::Install(const string *path, const string *titleId, string *dir,
				     string *installPath, GameJobProgress *progress, int cancel,
				     int cancelArg, float scale)
{
	int ret;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (m_info->info.contentType != GAME_CONTENT_TYPE_17)
		return GAME_ERROR_INVALID_ARGUMENT;

	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);

	string savedata = string("ux0:pspemu/PSP/SAVEDATA/") + *titleId;

	if (Dir::IsExist(savedata.c_str())) {
		gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);

		ret = Dir::CreateRecursive("ux0:pspemu/temp", 6);
		if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
			gamePathGetMountedDrive(string("ux0:pspemu/temp"), m_errorTitleId);
			return ret;
		}

		ret = LocalFile::RenameFile(savedata.c_str(), "ux0:pspemu/temp/promote");
		if (ret < 0) {
			gamePathGetMountedDrive(string("ux0:pspemu/temp/promote"), m_errorTitleId);
			return ret;
		}
	}

	if (!Dir::IsExist("ux0:pspemu/PSP/SAVEDATA")) {
		ret = Dir::CreateRecursive("ux0:pspemu/PSP/SAVEDATA", 6);
		if (ret < 0) {
			gamePathGetMountedDrive(string("ux0:pspemu/PSP/SAVEDATA"), m_errorTitleId);
			return ret;
		}
	}

	ret = LocalFile::RenameFile(path->c_str(), savedata.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(savedata, m_errorTitleId);
		return ret;
	}

	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	return 0;
}
