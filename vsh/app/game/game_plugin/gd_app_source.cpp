/*
 * The game plugin's source of an application's package: installed to ux0:app/<title ID>, the
 * old folder moved to ux0:temp/promote first, then the application's license written to
 * ux0:license/app/<title ID> and its FTT removed, or, for an application without a license, its
 * FTT installed (0x8108A0FC..0x8108A81C). The file's name and the functions' are ours (no record
 * names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameGdAppSource::GameGdAppSource(const SceNpDrmInstallInfo *info, bool noLicense,
				 const string &mountPoint, string *errorTitleId)
	: m_info(info), m_noLicense(noLicense), m_mountPoint(mountPoint),
	  m_errorTitleId(errorTitleId)
{
}

GameGdAppSource::~GameGdAppSource()
{
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/appmeta", NULL, NULL, NULL, true);
}

int GameGdAppSource::Install(const string *path, const string *titleId, string *dir,
			     string *installPath, GameJobProgress *progress, int reserved1,
			     int reserved2, float scale)
{
	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (m_info->info.contentType != GAME_CONTENT_TYPE_GD)
		return GAME_ERROR_INVALID_ARGUMENT;

	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);

	m_titleId = *titleId;
	*dir = *path;
	*installPath = string("ux0:app/") + *titleId;
	return 0;
}

int GameGdAppSource::Complete(string *dir, string *installPath, int reserved1, int reserved2,
			      int reserved3, float scale)
{
	int ret;

	if (dir->length() == 0) {
		gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
		return 0;
	}

	if (Dir::IsExist(installPath->c_str())) {
		if (!Dir::IsExist("ux0:temp")) {
			ret = Dir::CreateRecursive("ux0:temp", 6);
			if (ret < 0)
				return ret;
		}

		ret = LocalFile::RenameFile(installPath->c_str(), "ux0:temp/promote");
		if (ret < 0)
			return ret;
	} else {
		string::size_type pos = installPath->rfind("/", string::npos, sce_paf_strlen("/"));

		if (pos == string::npos)
			return GAME_ERROR_NO_DRIVE;

		string parent = installPath->substr(0, pos);

		if (!Dir::IsExist(parent.c_str())) {
			ret = Dir::CreateRecursive(parent.c_str(), 6);
			if (ret < 0)
				return ret;
		}
	}

	ret = LocalFile::RenameFile(dir->c_str(), installPath->c_str());
	if (ret < 0)
		return ret;

	if (m_noLicense) {
		ret = gameFttInstall(&m_mountPoint, m_titleId);
		if (ret < 0)
			return ret;
		return 0;
	}

	string licenseDir = string("ux0:license/app/") + m_titleId;
	string licensePath;

	ret = gameLicenseWrite(m_info->info.contentType, m_info->license, m_info->licenseSize,
			       &licenseDir, &licensePath, m_errorTitleId);
	if (ret < 0)
		return ret;

	gameFttRemove(m_titleId);
	return 0;
}
