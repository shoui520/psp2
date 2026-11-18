/*
 * The game plugin's installer of an application's package folder: the installed application's
 * record read and the update checked against it, the folder moved into place, and the record
 * built from its param.sfo (0x8106D3EC..0x810713C0). The file's name and the functions' are ours
 * (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameGdInstaller::GameGdInstaller(const char *path, const SceNpDrmInstallInfo *info,
				 GameJobProgress *progress, string *errorTitleId, float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale),
	  m_appInfo(new LsdbAppInfo), m_otherWord(0), m_install(true), m_lastWord(0)
{
}

GameGdInstaller::~GameGdInstaller()
{
	UnmountApp();
}

/*
 * The installed application's record read; an installed application not running, and an update
 * package checked against it (0x8106D57C).
 */
int GameGdInstaller::Prepare(bool flag)
{
	if (m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(m_info->info.titleId, &m_appInfo);

	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (m_appInfo->size() == 0) {
		m_flag = false;
		return 0;
	}

	m_flag = true;

	ret = gameAppCheckNotRunning(m_titleId, &m_appInfo);
	if (ret < 0)
		return ret;

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE)
		return 0;

	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	GameGdUpdateCheck *check = new GameGdUpdateCheck(string(m_info->info.titleId), string(m_path),
							 !param->imported, m_errorTitleId);

	if (check == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = check->Check(&m_appInfo);
	if (ret < 0 && ret != GAME_ERROR_GC_QUIET) {
		delete check;
		return ret;
	}

	if (ret == GAME_ERROR_GC_QUIET)
		m_install = false;

	delete check;
	return 0;
}

/*
 * The package folder's source: an update's, its stages weighed again when the application is on
 * the game card and the update on ux0:, or an application's (0x8106D702).
 */
GameInstallSource *GameGdInstaller::CreateSource(int *ret)
{
	*ret = 0;

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) {
		int storage = 0;
		int err = gameAppGetStorage(&m_appInfo, &storage);

		if (err < 0) {
			*ret = err;
			return NULL;
		}

		if (storage == GAME_STORAGE_GRO0 && sce_paf_strncmp(m_path, "ux0:", 4) == 0) {
			m_weights[2] = 0.1f;
			m_weights[1] = 0.5f;
			m_weights[5] = 0.1f;
			m_weights[3] = 0.2f;
			m_weights[4] = 0.05f;
		}

		return new GameGdUpdateSource(m_info, &m_appInfo, m_errorTitleId);
	}

	const GameInstallExParam *param = (const GameInstallExParam *)m_path;

	return new GameGdAppSource(m_info, m_noLicense, string((const char *)param->moreFields),
				   m_errorTitleId);
}

int GameGdInstaller::CheckAttribute(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *update)
{
	SceUInt8 installedBit = 0;
	SceUInt8 updateBit = 0;
	LsdbAppInfo::iterator it = (*installed)->find(LSDB_KEY_ATTRIBUTE);

	if (it != (*installed)->end())
		installedBit = (SceUInt8)gameLsdbValueInteger(it->second);

	/* Sony's code compares the update's lookup with the installed record's end */
	it = (*update)->find(LSDB_KEY_ATTRIBUTE);
	if (it != (*installed)->end())
		updateBit = (SceUInt8)gameLsdbValueInteger(it->second);

	if ((installedBit ^ updateBit) & 1)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

/* the application unmounted, and the source deleted (0x8106FAD6) */
int GameGdInstaller::Cleanup(void)
{
	UnmountApp();

	if (m_infoSource != NULL)
		delete m_infoSource;
	m_infoSource = NULL;

	return 0;
}
