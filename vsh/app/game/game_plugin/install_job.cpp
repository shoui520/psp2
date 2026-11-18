/*
 * The game plugin's install job: a package of GameInstallParam, its parameters checked and the
 * promoter of its kind run on the job's registration (0x81045DF6..0x8104639C). The file's name
 * and the functions' are ours (no record names them).
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

/* the kinds a package can be of */
#define GAME_INSTALL_KINDS	4

int GameInstallJob::CheckParam()
{
	const char *slash = sce_paf_strrchr(m_param.path, '/');

	if (slash == NULL)
		return GAME_ERROR_NO_DRIVE;

	m_name = slash + 1;

	if (m_reg.info.get() == NULL || m_reg.entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_param.path[0] != '\0' &&
	    gameStrnlen(m_param.path, sizeof(m_param.path)) <= sizeof(m_param.path) - 1 &&
	    m_param.titleId[0] != '\0' &&
	    gameStrnlen(m_param.titleId, sizeof(m_param.titleId)) <= sizeof(m_param.titleId) - 1 &&
	    gameCheckTitleId(m_param.titleId) >= 0 && m_param.kind < GAME_INSTALL_KINDS)
		return 0;

	return GAME_ERROR_NO_INFO_MEMORY;
}

GameInstallJob::GameInstallJob(const GameInstallParam *param, bool flag,
			       bool (*isStopRequested)(void), int arg4)
	: m_result(0), m_param(), m_flag(flag), m_otherFlag(true),
	  m_isStopRequested(isStopRequested), m_arg4(arg4), m_percent(0.0f),
	  m_progress(&m_percent, param->progressFunc, param->progressArg)
{
	sce_paf_memcpy(&m_param, param, sizeof(m_param));
	m_result = CheckParam();
}

GameInstallJob::~GameInstallJob()
{
}

int GameInstallJob::CreatePromoter(const GameInstallParam *param, GamePromoterIf **promoter,
				   GameJobProgress *progress, string *errorTitleId)
{
	GamePromoterIf *created;

	switch (param->kind) {
	case GAME_INSTALL_KIND_APP:
		*promoter = created = new GameAppInstaller(param, progress, errorTitleId);
		break;
	case GAME_INSTALL_KIND_PSP:
		*promoter = created = new GamePspAppInstaller(param, progress, errorTitleId);
		break;
	case GAME_INSTALL_KIND_LICENSE:
		*promoter = created = new GameLicenseInstaller(param, progress, errorTitleId);
		break;
	case GAME_INSTALL_KIND_PSM:
		*promoter = created = new GamePsmAppInstaller(param, progress, errorTitleId);
		break;
	default:
		return GAME_ERROR_NOT_APP_PACKAGE;
	}

	if (created == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

int GameInstallJob::Run()
{
	if (m_result < 0)
		return m_result;

	GamePromoterIf *promoter = NULL;
	int ret = CreatePromoter(&m_param, &promoter, &m_progress, &m_errorTitleId);

	if (ret < 0)
		return ret;

	ret = promoter->Run(&m_reg, m_flag, (GameCancelFunc)m_isStopRequested, (void *)m_arg4);
	if (ret < 0) {
		if (promoter != NULL)
			delete promoter;
		return ret;
	}

	if (promoter != NULL)
		delete promoter;

	if (m_progress.percent != NULL && !(100.0f < *m_progress.percent)) {
		*m_progress.percent = 100.0f;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, *m_progress.percent);
		m_progress.added = 0;
	}

	return 0;
}
