/*
 * A theme's extracted package moved into ux0:theme for its installer, the theme it replaces kept
 * in ux0:temp/promote until the move is done, and the theme's license written
 * (0x8103C120..0x8103C5CC). The file's name and the class's are ours (no record names them); the
 * strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* where a replaced theme waits until the new one is in place */
#define GAME_THEME_PROMOTE_DIR	"ux0:temp/promote"

/* 0x8103C120 */
GameThemeCopier::GameThemeCopier(SceNpDrmInstallInfo *info, string *errorTitleId)
	: m_info(info), m_errorTitleId(errorTitleId), m_moveShare(0.4f), m_licenseShare(0.6f)
{
}

/* 0x8103C158, deleting 0x8103C1A2 */
GameThemeCopier::~GameThemeCopier()
{
	gameRemoveDirectory(GAME_THEME_PROMOTE_DIR, 0, 0, 0, true);
}

/*
 * Where the theme extracted at path goes, ux0:theme/<themeId>, the replaced one's place emptied
 * (0x8103C1F2).
 */
int GameThemeCopier::Install(const string &path, const string &themeId, string *themeDir,
			     string *contentPath, GameJobProgress *progress, float share, int arg6,
			     int arg7)
{
	gameRemoveDirectory(GAME_THEME_PROMOTE_DIR, 0, 0, 0, true);

	m_themeId = themeId;
	*themeDir = path;
	*contentPath = string("ux0:theme/") + themeId;

	return 0;
}

/*
 * The theme at themeDir moved to contentPath, a theme already there moved aside first, and its
 * license written to ux0:license/theme/<themeId> (0x8103C33C).
 */
int GameThemeCopier::Commit(string *themeDir, string *contentPath, GameJobProgress *progress,
			    float share, int arg4, int arg5)
{
	int ret;

	if (Dir::IsExist(contentPath->c_str())) {
		if (!Dir::IsExist("ux0:temp")) {
			ret = Dir::CreateRecursive("ux0:temp", 6);
			if (ret < 0)
				return ret;
		}

		ret = LocalFile::RenameFile(contentPath->c_str(), GAME_THEME_PROMOTE_DIR);
		if (ret < 0)
			return ret;
	} else if (!Dir::IsExist("ux0:theme")) {
		ret = Dir::CreateRecursive("ux0:theme", 6);
		if (ret < 0)
			return ret;
	}

	ret = LocalFile::RenameFile(themeDir->c_str(), contentPath->c_str());
	if (ret < 0)
		return ret;

	if (progress != NULL)
		progress->Add(share * m_moveShare * 100.0f);

	string licenseDir = string("ux0:license/theme/") + m_themeId;
	string licensePath;

	ret = gameLicenseWrite(m_info->info.contentType, m_info->license, m_info->licenseSize,
			       &licenseDir, &licensePath, m_errorTitleId);
	if (ret < 0)
		return ret;

	if (progress != NULL)
		progress->Add(share * m_licenseShare * 100.0f);

	return 0;
}
