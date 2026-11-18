/*
 * The game plugin's application promoter: an application's directory promoted by the promoter
 * its path asks for, run on the caller's registration (0x81046D04..0x810475B2). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
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

/* the size of a directory's path, with its terminator */
#define GAME_APP_PATH_SIZE	128

GameAppPromoter::GameAppPromoter(const char *path, bool flag, GameAppRegistration *reg,
				 bool (*isStopRequested)(void), void *arg, int unused,
				 void (*progressFunc)(void *arg, float percent), void *progressArg)
	: m_result(GAME_ERROR_NOT_STARTED), m_reg(reg), m_registerApp(flag), m_option(false),
	  m_installed(false), m_promoted(false), m_otherFlag(true), m_replaced(false),
	  m_cancel((GameCancelFunc)isStopRequested), m_arg(arg), m_percent(0.0f),
	  m_progress(&m_percent, progressFunc, progressArg)
{
	const char *slash = sce_paf_strrchr(path, '/');

	if (slash == NULL) {
		m_result = GAME_ERROR_NO_DRIVE;
		return;
	}

	m_name = slash + 1;

	int ret;

	if (path != NULL && path[0] != '\0') {
		SceSize length = gameStrnlen(path, GAME_APP_PATH_SIZE);

		if (length <= GAME_APP_PATH_SIZE - 1) {
			if (path[length - 1] == '/')
				ret = GAME_ERROR_INVALID_ARGUMENT;
			else
				ret = 0;
		} else {
			ret = GAME_ERROR_INVALID_ARGUMENT;
		}
	} else {
		ret = GAME_ERROR_INVALID_ARGUMENT;
	}

	m_result = ret;
	if (ret < 0)
		return;

	m_path = path;
}

GameAppPromoter::GameAppPromoter(const GamePromoteParam *param, bool flag,
				 GameAppRegistration *reg, bool (*cancel)(void *arg), void *arg)
	: m_result(0), m_sfo(param->appInfo), m_reg(reg), m_registerApp(flag), m_option(false),
	  m_installed(false), m_otherFlag(true), m_cancel((GameCancelFunc)cancel), m_arg(arg),
	  m_percent(0.0f), m_progress(&m_percent, param->progressFunc, param->progressArg)
{
	const char *path = param->path;
	const char *slash = sce_paf_strrchr(path, '/');

	if (slash == NULL) {
		m_result = GAME_ERROR_NO_DRIVE;
		return;
	}

	m_name = slash + 1;

	int ret;

	if (path != NULL) {
		if (path[0] != '\0') {
			if (gameStrnlen(path, GAME_APP_PATH_SIZE) <= GAME_APP_PATH_SIZE - 1) {
				if (param->appInfo.get() != NULL)
					ret = 0;
				else
					ret = GAME_ERROR_INVALID_ARGUMENT;
			} else {
				ret = GAME_ERROR_INVALID_ARGUMENT;
			}
		} else {
			ret = GAME_ERROR_INVALID_ARGUMENT;
		}
	} else {
		ret = GAME_ERROR_INVALID_ARGUMENT;
	}

	m_result = ret;
	if (ret < 0)
		return;

	m_path = path;
	m_option = param->option;
}

GameAppPromoter::~GameAppPromoter()
{
}

bool GameAppPromoter::IsAppPath(const string *path)
{
	return path->find("gro0:app/") != string::npos ||
	       path->find("vs0:app/") != string::npos ||
	       path->find("ux0:app/") != string::npos ||
	       path->find("pd0:app/") != string::npos;
}

int GameAppPromoter::CreatePromoter(const string *path, const LsdbAppInfoPtr *sfo, bool option,
				    bool otherFlag, GamePromoter **promoter,
				    GameJobProgress *progress, string *errorTitleId)
{
	GamePromoter *created;

	if (path->find("ux0:pspemu/PSP/GAME/") != string::npos) {
		*promoter = created = new GamePspPromoter(path->c_str(), NULL, otherFlag, progress,
							  (int)errorTitleId, 1.0f);
	} else if (path->find("ux0:appmeta/") != string::npos) {
		*promoter = created = new GameAppmetaPromoter(path->c_str(), progress,
							      (int)errorTitleId, 1.0f);
	} else if (path->find("ux0:psm/") != string::npos) {
		*promoter = created = new GamePsmPromoter(path->c_str(), otherFlag, progress,
							  (int)errorTitleId, 1.0f);
	} else if (IsAppPath(path) == true) {
		*promoter = created = new GamePatchPromoter(path->c_str(), sfo, option, otherFlag,
							    progress, (int)errorTitleId, 1.0f);
	} else {
		return GAME_ERROR_NOT_APP_PACKAGE;
	}

	if (created == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

int GameAppPromoter::Run()
{
	if (m_result < 0)
		return m_result;

	if (m_reg->info.get() == NULL || m_reg->entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	GamePromoter *promoter = NULL;
	int ret = CreatePromoter(&m_path, &m_sfo, m_option, m_otherFlag, &promoter, &m_progress,
				 &m_errorTitleId);

	if (ret < 0)
		return ret;

	ret = promoter->Run(m_reg, m_registerApp, m_cancel, m_arg);
	m_promoted = promoter->m_install;

	if (ret < 0) {
		if (promoter != NULL)
			delete promoter;

		if (ret != GAME_ERROR_GC_QUIET && ret != GAME_ERROR_GC_REPLACED)
			return ret;

		m_replaced = false;
		if (ret == GAME_ERROR_GC_REPLACED)
			m_replaced = true;
		m_installed = true;

		m_progress.Set(100.0f);

		return 0;
	}

	if (promoter != NULL)
		delete promoter;

	m_progress.Set(100.0f);

	return 0;
}
