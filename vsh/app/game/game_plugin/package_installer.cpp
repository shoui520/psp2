/*
 * The game plugin's installers' base for a package folder of GameInstallExParam: the folder made
 * ready, its source made and the package installed by it, the record built from the source and
 * registered, the source's install completed and the record updated, by stages
 * (0x81095248..0x81095EB8). The file's name and the functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePackageInstaller::GamePackageInstaller(const char *path, const SceNpDrmInstallInfo *info,
					   GameJobProgress *progress, string *errorTitleId,
					   float scale)
	: m_path(path), m_info(info), m_source(NULL), m_infoSource(NULL),
	  m_progress(*progress, scale), m_flag(false), m_prepareFlag(true), m_otherFlag(true),
	  m_titleId(info->info.titleId), m_errorTitleId(errorTitleId)
{
	m_noLicense = string(((const GameInstallExParam *)m_path)->mountPoint).length() != 0;
	m_weights[0] = 0.05f;
	m_weights[1] = 0.1f;
	m_weights[2] = 0.3f;
	m_weights[3] = 0.45f;
	m_weights[4] = 0.05f;
	m_weights[5] = 0.05f;
}

GamePackageInstaller::~GamePackageInstaller()
{
	if (m_infoSource != NULL)
		delete m_infoSource;
	if (m_source != NULL)
		delete m_source;
}

/*
 * The install: the folder made ready, its source made and the package installed by it; an
 * install the installer reports as not to be registered completed as the other kind. The record
 * built, finished and registered when registerApp is set, the source's install completed, then
 * the registered record updated (0x81095422).
 */
int GamePackageInstaller::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			      void *arg)
{
	int ret = 0;

	ret = Prepare(m_prepareFlag);
	if (ret < 0)
		return ret;

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	SetProgress(0);

	m_source = CreateSource(&ret);
	if (ret < 0)
		return ret;
	if (m_source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir;
	string installPath;

	{
		string path(m_path);

		ret = m_source->Install(&path, &m_titleId, &dir, &installPath, &m_progress, 0, 0,
					m_weights[1]);
		if (ret < 0) {
			if (ret == GAME_ERROR_GC_QUIET) {
				ret = m_source->Complete(&dir, &installPath, 0, 0, 0, 1.0f);
				if (ret < 0)
					return ret;
				return 0;
			}
			return ret;
		}
	}

	SetProgress(1);

	ret = Installed(&dir, &installPath);
	if (ret < 0) {
		if (cancel != NULL && cancel(arg) != 0)
			return GAME_ERROR_ABORTED;

		if (ret == GAME_NPDRM_ERROR_EXPIRED || ret == GAME_LICENSE_ERROR_NOT_FOUND) {
			Cleanup();
			return ret;
		}

		int other;

		{
			string path(installPath);

			other = SetOtherInfo(registerApp, reg, &dir, &path);
		}
		if (other < 0 && other != GAME_ERROR_GC_QUIET)
			return ret;

		if (other == GAME_ERROR_GC_QUIET) {
			if (m_source != NULL) {
				string none1((const char *)NULL);
				string none2((const char *)NULL);

				ret = m_source->Complete(&none1, &none2, 0, 0, 0, 1.0f);
				if (ret < 0)
					return ret;
			}
			return 0;
		}
	}

	if (cancel != NULL && cancel(arg) != 0) {
		Cleanup();
		return GAME_ERROR_ABORTED;
	}

	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	bool replace = false;
	bool notifyTopmenu = !param->imported;
	bool registered = false;

	if (m_infoSource != NULL) {
		ret = SetInfo(registerApp, reg);
		if (ret < 0) {
			Cleanup();
			return ret;
		}

		SetProgress(2);

		ret = Finish(reg, cancel, arg);
		if (ret < 0) {
			Cleanup();
			return ret;
		}

		if (registerApp) {
			LsdbAppInfo::iterator end = reg->info->end();
			LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

			if (it == end)
				return GAME_ERROR_BAD_DATA;

			string category(LsdbValueText(it->second));
			int mode = 0;

			if (m_flag)
				mode = 1;
			if (m_flag && gameCategoryIsPatch(category))
				mode = 2;

			reg->otherFlag = !param->noOtherFlag;

			ret = gameAppRegister(reg, mode, m_otherFlag, notifyTopmenu, &replace);
			if (ret < 0) {
				Cleanup();
				return ret;
			}
			registered = true;
		}

		Cleanup();
		SetProgress(3);
	}

	if (m_source != NULL) {
		ret = m_source->Complete(&dir, &installPath, 0, 0, 0, 1.0f);
		if (ret < 0) {
			if (registered && notifyTopmenu)
				gameAppRegisterEnd(&reg->info);
			return ret;
		}
	}

	SetProgress(4);

	if (registered) {
		if (m_noLicense)
			replace = true;

		ret = gameAppUpdate(&reg->info, replace, notifyTopmenu);
		if (ret < 0)
			return ret;
	}

	SetProgress(5);
	return 0;
}

/*
 * The record built from the information source: its LiveArea path, the license's #_up_status
 * for a game's package that is not an update, its short title and entry, and the two appconf
 * bits cleared (0x81095DA4).
 */
int GamePackageInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret;

	if (m_infoSource == NULL)
		return 0;

	ret = m_infoSource->SetInfo(reg);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetLiveAreaPath(&reg->info, &m_appDir, &m_dir);
	if (ret < 0)
		return ret;

	if (m_info->info.contentType == GAME_CONTENT_TYPE_GD &&
	    (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) == 0) {
		ret = gameAppInfoSetUpStatusFromLicense((SceNpDrmLicense *)m_info->license,
							m_info->licenseSize, &reg->info,
							(int)&m_dir, (int)m_errorTitleId,
							&m_noLicense);
		if (ret < 0)
			return ret;
	}

	gameAppInfoGetStitle(&reg->info, &reg->entry->otherText);

	ret = gameAppEntryFill(reg, &m_appDir, &m_dir);
	if (ret < 0)
		return ret;

	gameAppInfoSetAppconfBit10(&reg->info, false);
	gameAppInfoSetAppconfBit11(&reg->info, false);
	return 0;
}
