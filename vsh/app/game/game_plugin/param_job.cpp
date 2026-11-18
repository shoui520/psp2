/*
 * A package folder installed with the shell locked (gamePluginParamJobRun's job): its install
 * information read and checked, then its installer run (0x810305E8..0x81030A6C). The file's
 * name and the class's are ours (no record names them); GetInstallInfo is Sony's name in 0.990's
 * messages ("GamePlugin::GetInstallInfo(%s) ret=%#x").
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the work memory NpDrm reads the install information with */
#define GAME_INSTALL_INFO_BUF_SIZE	0x8000

/* the part of the param digest the debug dump shows */
#define GAME_PARAM_DIGEST_DUMP_SIZE	32

/* The install information of the package folder at path (0x810305E8). */
int GameParamJob::GetInstallInfo(const string &path, SceNpDrmInstallInfo *info)
{
	char *buf = new char[GAME_INSTALL_INFO_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir(path);

	if (dir[dir.length() - 1] != '/')
		dir.append(string("/"));

	sce_paf_memset(info, 0, sizeof(*info));

	int ret = gameNpDrmGetInstallInfo(info, sizeof(*info), dir.c_str(), buf,
					  GAME_INSTALL_INFO_BUF_SIZE);

	if (ret < 0) {
		delete[] buf;

		if (ret == SCE_ERROR_ERRNO_ENOENT)
			return GAME_ERROR_NO_INSTALL_INFO;

		return ret;
	}

	delete[] buf;

	gameDebugDump("sceNpDrmInstallInfo.license", info->license, sizeof(info->license));
	gameDebugDump("sceNpDrmInstallInfo.keystone", info->keystone, sizeof(info->keystone));
	gameDebugDump("sceNpDrmInstallInfo.param_digest", info->param_digest,
		      GAME_PARAM_DIGEST_DUMP_SIZE);

	return ret;
}

/* Whether the parameters name an application's package folder (0x8103079E). */
int GameParamJob::Check(void)
{
	if (m_param.path[0] == '\0' ||
	    gameStrnlen(m_param.path, sizeof(m_param.path)) >= sizeof(m_param.path) ||
	    !Dir::IsExist(m_param.path))
		return GAME_ERROR_INVALID_ARGUMENT;

	int ret = GetInstallInfo(string(m_param.path), &m_info);

	if (ret < 0)
		return ret;

	if (m_info.info.contentType != GAME_CONTENT_TYPE_APP)
		return GAME_ERROR_NOT_APP_PACKAGE;

	return 0;
}

/* The job made from param, its folder checked (0x8103084C). */
GameParamJob::GameParamJob(const GameJobParam *param, bool (*cancel)(void *arg), void *arg)
	: m_param(), m_cancel(cancel), m_cancelArg(arg), m_result(0), m_percent(0.0f),
	  m_progress(&m_percent, param->progressFunc, param->progressArg)
{
	sce_paf_memcpy(&m_param, param, sizeof(m_param));
	sce_paf_memset(&m_info, 0, sizeof(m_info));

	m_result = Check();
}

/* 0x810308E0, deleting 0x81030914 */
GameParamJob::~GameParamJob()
{
}

/* The folder installed, the progress brought to 100% (0x8103094C). */
int GameParamJob::Run(void)
{
	int ret = m_result;

	if (ret < 0)
		return ret;

	GameThemeInstaller *installer = new GameThemeInstaller(&m_param, &m_info, &m_progress,
							       &m_errorTitleId, 1.0f);

	if (installer == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = installer->Run(m_cancel, m_cancelArg);
	if (ret < 0) {
		delete installer;
		return ret;
	}

	delete installer;

	float *percent = m_progress.percent;

	if (percent != NULL && !(*percent > 100.0f)) {
		*percent = 100.0f;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, 100.0f);
		m_progress.added = 0;
	}

	return 0;
}
