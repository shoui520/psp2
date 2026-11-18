/*
 * The game plugin's installer of an additional content's package folder: its application's record
 * read, the install held while the running application uses the application's additional
 * contents, and the additional contents' field of the #appsize set to not counted
 * (0x8106D0B0..0x8106D3EC). The file's name and the functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <thread/thread.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "game_appsize.h"
#include "promoter.h"

using namespace paf;

/* how many times, half a second apart, the install waits for the running application */
#define GAME_AC_WAIT_TRIES	30

GameAcInstaller::GameAcInstaller(const char *path, const SceNpDrmInstallInfo *info,
				 GameJobProgress *progress, string *errorTitleId, float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale),
	  m_appInfo(new LsdbAppInfo)
{
	m_weights[2] = 0.0f;
	m_weights[3] = 0.0f;
	m_weights[0] = 0.1f;
	m_weights[1] = 0.75f;
	m_weights[4] = 0.1f;
	m_weights[5] = 0.05f;
}

GameAcInstaller::~GameAcInstaller()
{
}

/*
 * The application's record read, and the install held, up to 15 seconds, while the running
 * application is not in its install period and its record names the application (0x8106D1EA).
 */
int GameAcInstaller::Prepare(bool flag)
{
	if (m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(m_info->info.titleId, &m_appInfo);

	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	int tries = 0;
	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));

	if (sceAppMgrGetStatusByAppId(-3, &status) != 0)
		return 0;

	if ((int)vshAppMgrCheckContentInstallPeriod(status.processId) < 0) {
		while (sceAppMgrGetStatusByAppId(-3, &status) == 0 &&
		       gameAppRecordNamesTitle(m_titleId.c_str(), status.appName) != 0) {
			thread::Sleep(500);
			if (++tries > GAME_AC_WAIT_TRIES)
				return GAME_ERROR_APP_RUNNING;
		}
	}

	return 0;
}

/* the additional content's source (0x8106D2A2) */
GameInstallSource *GameAcInstaller::CreateSource(int *ret)
{
	*ret = 0;

	return new GameAddcontSource(m_path, m_info, &m_appInfo, m_errorTitleId);
}

/* the #appsize's additional contents' field not counted, when the application has one (0x8106D2CE) */
int GameAcInstaller::Installed(string *dir, string *path)
{
	string titleId(m_info->info.titleId);

	if (m_appInfo->find(LSDB_KEY_APPSIZE) != m_appInfo->end())
		gameAppSizeSetAddcont(&titleId, GAME_APPSIZE_ADDCONT_MASK);

	return 0;
}
