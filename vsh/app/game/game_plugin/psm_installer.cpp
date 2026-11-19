/*
 * The game plugin's installer of a PlayStation Mobile game's package folder: its app.info read
 * by a GamePsmAppReader, as its promotion reads it, and the record built from it
 * (0x810713C0..0x81071890). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
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

GamePsmInstaller::GamePsmInstaller(const char *path, const SceNpDrmInstallInfo *info,
				   GameJobProgress *progress, string *errorTitleId, float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale),
	  m_reader(m_titleId, errorTitleId), m_appInfo(new LsdbAppInfo), m_reserved(0)
{
	m_prepareFlag = false;
	m_weights[0] = 0.1f;
	m_weights[1] = 0.2f;
	m_weights[4] = 0.1f;
	m_weights[2] = 0.4f;
	m_weights[3] = 0.25f;
	m_weights[5] = 0.05f;
}

GamePsmInstaller::~GamePsmInstaller()
{
}

/* the app.info made, and whether the game is installed already (0x81071570) */
int GamePsmInstaller::Prepare(bool flag)
{
	int ret = m_reader.Prepare(flag, &m_appInfo, &m_flag);

	if (ret < 0)
		return ret;

	return 0;
}

GameInstallSource *GamePsmInstaller::CreateSource(int *ret)
{
	*ret = 0;
	return new GamePsmInstallSource(m_info, &m_appInfo, m_errorTitleId);
}

/* the source of the installed folder, from its Application/app.info (0x810715B0) */
int GamePsmInstaller::Installed(string *dir, string *path)
{
	string appInfoPath = *dir + string("/Application/app.info");
	int ret = m_reader.CreateSource(appInfoPath, *dir, *path, &m_infoSource);

	if (ret < 0)
		return ret;

	return 0;
}

/* the app.info and the source deleted (0x810716F2) */
int GamePsmInstaller::Cleanup(void)
{
	m_reader.Cleanup(&m_infoSource);
	return 0;
}

/*
 * The record built: the app.info's information, the install time kept from the old record, the
 * change time now, the package's content ID, then the source's (0x81071706).
 */
int GamePsmInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret = m_reader.SetInfo(registerApp, &m_appInfo, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_appInfo, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_CONTENT_ID], m_info->info.contentId);

	ret = m_infoSource->SetInfo(reg);
	if (ret < 0)
		return ret;

	return 0;
}

/* the LiveArea written, with a quarter of the install's progress (0x810717FA) */
int GamePsmInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret = m_reader.Finish(reg, &m_progress, cancel, arg, 0.25f);

	if (ret < 0)
		return ret;

	return 0;
}
