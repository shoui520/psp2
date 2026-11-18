/*
 * The game plugin's installer of a package folder of content type 17: the installers' base does
 * the install, once the title is not the foreground application (0x81073D34..0x81073E90). The
 * file's name and the functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameType17Installer::GameType17Installer(const char *path, const SceNpDrmInstallInfo *info,
					 GameJobProgress *progress, string *errorTitleId,
					 float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale)
{
}

GameType17Installer::~GameType17Installer()
{
}

/* the title not the foreground application (0x81073D90) */
int GameType17Installer::Prepare(bool flag)
{
	int ret = gameAppCheckNotForeground(string(m_info->info.titleId));

	if (ret < 0)
		return ret;

	return 0;
}

GameInstallSource *GameType17Installer::CreateSource(int *ret)
{
	*ret = 0;
	return new GameType17InstallSource(m_info, m_errorTitleId);
}
