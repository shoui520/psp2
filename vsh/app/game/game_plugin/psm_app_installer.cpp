/*
 * The game plugin's installer of a PlayStation Mobile application's package: its app.info read
 * and its folder promoted into ux0:psm by a GamePsmAppReader, the record's times and content ID
 * set (0x81077624..0x8107832C). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
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

GamePsmAppInstaller::GamePsmAppInstaller(const GameInstallParam *param,
					 GameJobProgress *progress, string *errorTitleId,
					 float scale)
	: GameInstaller(param, progress, errorTitleId, scale),
	  m_reader(string(param->titleId), (int)errorTitleId), m_info(new LsdbAppInfo)
{
	m_prepareFlag = true;
	m_weights[0] = 0.1f;
	m_weights[1] = 0.1f;
	m_weights[2] = 0.3f;
	m_weights[4] = 0.1f;
	m_weights[3] = 0.35f;
	m_weights[5] = 0.05f;
}

GamePsmAppInstaller::~GamePsmAppInstaller()
{
}

/* the app.info made, and whether the application is installed already (0x81077808) */
int GamePsmAppInstaller::Prepare(bool flag)
{
	int ret = m_reader.Prepare(flag, &m_info, &m_installed);

	if (ret < 0)
		return ret;

	return 0;
}

GameInstallSource *GamePsmAppInstaller::CreateSource(void)
{
	return new GamePsmAppInstallSource(m_param, m_errorTitleId);
}

/* the source of the installed folder, from its RO/Application/app.info (0x81077824) */
int GamePsmAppInstaller::Installed(void)
{
	string appInfoPath = string(m_param->path) + string("/") + string(m_param->titleId) +
			     string("/RO/Application/app.info");
	string dir = string(m_param->path) + string("/") + string(m_param->titleId);
	string path = string("ux0:psm/") + string(m_param->titleId);

	int ret = m_reader.CreateSource(appInfoPath, dir, path, &m_infoSource);

	if (ret < 0)
		return ret;

	return 0;
}

/* the app.info and the source deleted (0x81077ED8) */
int GamePsmAppInstaller::Cleanup(void)
{
	m_reader.Cleanup(&m_infoSource);
	return 0;
}

/*
 * The record built: the app.info's information, the install time kept from the old record, the
 * change time now, the content ID from RW/System/content_id, then the source's; an installed
 * application of another version keeps what the record keeps (0x81077EEC).
 */
int GamePsmAppInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret = m_reader.SetInfo(registerApp, &m_info, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_info, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	string path = m_param->path + string("/") + string(m_param->titleId) +
		      string("/RW/System/content_id");

	m_reader.ReadContentId(path, &reg->info);

	ret = m_infoSource->SetInfo(reg);
	if (ret < 0)
		return ret;

	if (m_installed && gameAppVersionDiffers(&m_info, &reg->info)) {
		m_versionChanged = true;

		ret = gameAppInfoCopyKept(&m_info, &reg->info);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* the LiveArea written, its share of the install's progress (0x8107828C) */
int GamePsmAppInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret = m_reader.Finish(reg, &m_progress, cancel, arg, 0.35f);

	if (ret < 0)
		return ret;

	return 0;
}
