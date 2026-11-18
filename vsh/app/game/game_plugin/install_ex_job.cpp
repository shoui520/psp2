/*
 * The game plugin's second install job: a package folder of GameInstallExParam, its install
 * information read and the promoter of its content type run on the job's registration
 * (0x8104639C..0x81046D04). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the buffer sceNpDrm reads a folder's install information with */
#define GAME_INSTALL_INFO_BUF_SIZE	0x8000

/* the part of the param digest the debug dump shows */
#define GAME_PARAM_DIGEST_DUMP_SIZE	32

/* The install information of the package folder at path (0x8104639C). */
int gameDirReadStat(const string &path, SceNpDrmInstallInfo *stat)
{
	char *buf = new char[GAME_INSTALL_INFO_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir(path);

	if (dir.c_str()[dir.length() - 1] != '/')
		dir.append(string("/"));

	int ret = gameNpDrmGetInstallInfo(stat, sizeof(*stat), dir.c_str(), buf,
					  GAME_INSTALL_INFO_BUF_SIZE);

	if (ret < 0) {
		delete[] buf;

		if (ret == SCE_ERROR_ERRNO_ENOENT)
			return GAME_ERROR_NO_INSTALL_INFO;

		return ret;
	}

	delete[] buf;

	gameDebugDump("sceNpDrmInstallInfo.license", stat->license, sizeof(stat->license));
	gameDebugDump("sceNpDrmInstallInfo.keystone", stat->keystone, sizeof(stat->keystone));
	gameDebugDump("sceNpDrmInstallInfo.param_digest", stat->param_digest,
		      GAME_PARAM_DIGEST_DUMP_SIZE);

	return 0;
}

int GameInstallExJob::CheckParam()
{
	if (m_reg.info.get() == NULL || m_reg.entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_param.path[0] == '\0' ||
	    gameStrnlen(m_param.path, sizeof(m_param.path)) > sizeof(m_param.path) - 1)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_readInfo) {
		int ret = gameDirReadStat(string(m_param.path), &m_info);

		if (ret < 0)
			return ret;
	}

	return 0;
}

GameInstallExJob::GameInstallExJob(const GameInstallExParam *param, bool flag, int arg3,
				   int arg4)
	: m_result(0), m_param(), m_readInfo(true), m_info(), m_percent(0.0f),
	  m_progress(&m_percent, param->progressFunc, param->progressArg), m_flag(flag),
	  m_otherFlag(true), m_flag2(false), m_arg3(arg3), m_arg4(arg4)
{
	sce_paf_memcpy(&m_param, param, sizeof(m_param));
	sce_paf_memset(&m_info, 0, sizeof(m_info));
	m_result = CheckParam();
}

GameInstallExJob::~GameInstallExJob()
{
}

int GameInstallExJob::CreatePromoter(const char *path, const SceNpDrmInstallInfo *info,
				     GamePromoterIf **promoter, GameJobProgress *progress,
				     string *errorTitleId)
{
	GamePromoterIf *created;

	if (gameTypeIsSpecial(info->info.contentType)) {
		{
			string pbootPath = string(path) + string("/USRDIR/CONTENT/PBOOT.PBP");
			LocalFileStat stat;

			if (LocalFile::Getstat(pbootPath.c_str(), &stat) != 0)
				*promoter = new GamePspInstaller(path, info, progress,
								 errorTitleId);
			else
				*promoter = new GamePspPbootInstaller(path, info, progress,
								      errorTitleId);
		}

		created = *promoter;
	} else {
		switch (info->info.contentType) {
		case GAME_CONTENT_TYPE_17:
			*promoter = created = new GameType17Installer(path, info, progress,
								      errorTitleId);
			break;
		case GAME_CONTENT_TYPE_GD:
			*promoter = created = new GameGdInstaller(path, info, progress,
								  errorTitleId);
			break;
		case GAME_CONTENT_TYPE_ADDCONT:
			*promoter = created = new GameAcInstaller(path, info, progress,
								  errorTitleId);
			break;
		case GAME_CONTENT_TYPE_PSM:
		case GAME_CONTENT_TYPE_PSM_UNITY:
			*promoter = created = new GamePsmInstaller(path, info, progress,
								   errorTitleId);
			break;
		default:
			return GAME_ERROR_NOT_APP_PACKAGE;
		}
	}

	if (created == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

int GameInstallExJob::Run()
{
	if (m_result < 0)
		return m_result;

	GamePromoterIf *promoter = NULL;
	int ret = CreatePromoter(m_param.path, &m_info, &promoter, &m_progress, &m_errorTitleId);

	if (ret < 0)
		return ret;

	ret = promoter->Run(&m_reg, m_flag, (GameCancelFunc)m_arg3, (void *)m_arg4);
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
