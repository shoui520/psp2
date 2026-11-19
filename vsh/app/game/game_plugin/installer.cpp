/*
 * The game plugin's installers' base for a package of GameInstallParam: the installed
 * application's record read, the package installed by its source, the record built from the
 * source and registered, the source's install completed and the record updated, by stages
 * (0x81095EB8..0x81096D1A). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameInstaller::GameInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     string *errorTitleId, float scale)
	: m_param(param), m_appInfo(new LsdbAppInfo), m_source(NULL), m_infoSource(NULL),
	  m_progress(*progress, scale), m_installed(false), m_prepareFlag(true),
	  m_otherFlag(true), m_errorTitleId(errorTitleId), m_freeTrial(false),
	  m_versionChanged(false)
{
	m_weights[0] = 0.05f;
	m_weights[1] = 0.1f;
	m_weights[2] = 0.3f;
	m_weights[3] = 0.45f;
	m_weights[4] = 0.05f;
	m_weights[5] = 0.05f;
}

GameInstaller::~GameInstaller()
{
	if (m_infoSource != NULL)
		delete m_infoSource;
	if (m_source != NULL)
		delete m_source;
}

/*
 * The install: the installed application's record read, the package installed by its source,
 * the record built, finished and registered when registerApp is set, the source's install
 * completed, then the registered record updated (0x81096114).
 */
int GameInstaller::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
		       void *arg)
{
	int ret;

	m_titleId = m_param->titleId;

	ret = Prepare(m_prepareFlag);
	if (ret < 0)
		return ret;

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	SetProgress(0);

	m_source = CreateSource();
	if (m_source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir;
	string installPath;

	{
		string path(m_param->path);

		ret = m_source->Install(&path, &m_titleId, &dir, &installPath, &m_progress, 0, 0,
					m_weights[1]);
		if (ret < 0)
			return ret;
	}

	SetProgress(1);

	ret = Installed();
	if (ret < 0)
		return ret;

	if (cancel != NULL && cancel(arg) != 0) {
		Cleanup();
		return GAME_ERROR_ABORTED;
	}

	ret = SetInfo(registerApp, reg);
	if (ret < 0) {
		Cleanup();
		return ret;
	}

	SetProgress(2);

	if (cancel != NULL && cancel(arg) != 0) {
		Cleanup();
		return GAME_ERROR_ABORTED;
	}

	ret = Finish(reg, cancel, arg);
	if (ret < 0) {
		Cleanup();
		return ret;
	}

	bool replace = false;
	bool notifyTopmenu = !m_param->flag2;

	if (registerApp) {
		reg->otherFlag = !m_param->flag1;

		int mode = 0;

		if (m_installed)
			mode = 1;
		if (m_installed && m_versionChanged)
			mode = 2;

		ret = gameAppRegister(reg, mode, m_otherFlag, notifyTopmenu, &replace);
		if (ret < 0) {
			Cleanup();
			return ret;
		}
	}

	SetProgress(3);

	ret = Cleanup();
	if (ret < 0) {
		if (registerApp && notifyTopmenu)
			gameAppRegisterEnd(&reg->info);
		return ret;
	}

	if (m_source != NULL) {
		ret = m_source->Complete(&dir, &installPath, 0, 0, 0, 1.0f);
		if (ret < 0) {
			if (registerApp && notifyTopmenu)
				gameAppRegisterEnd(&reg->info);
			return ret;
		}
	}

	SetProgress(4);

	if (registerApp) {
		ret = gameAppUpdate(&reg->info, replace, notifyTopmenu);
		if (ret < 0)
			return ret;
	}

	SetProgress(5);
	return 0;
}

/* the installed application's record read, and whether it has one (0x8109675E) */
int GameInstaller::Prepare(bool flag)
{
	if (m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(m_titleId.c_str(), &m_appInfo);

	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (m_appInfo->size() == 0)
		m_installed = false;
	else
		m_installed = true;
	return 0;
}

/* the information source deleted (0x810967A4) */
int GameInstaller::Cleanup(void)
{
	if (m_infoSource != NULL)
		delete m_infoSource;
	m_infoSource = NULL;
	return 0;
}

/*
 * The record built from the information source: the installed application's install time, the
 * modification time now, its LiveArea path, its #_up_status from the license folder, its short
 * title and entry; a game without its template.xml in ux0:appmeta/new has bit 12 of #appconf
 * set (0x810967B8).
 */
int GameInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret;

	if (m_infoSource == NULL)
		return 0;

	ret = m_infoSource->SetInfo(reg);
	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_appInfo, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetLiveAreaPath(&reg->info, &m_appmetaDir, &m_dir);
	if (ret < 0)
		return ret;

	if (m_licenseDir.length() != 0) {
		ret = gameAppInfoSetUpStatus(&reg->info, &m_licenseDir, &m_dir, (int)m_errorTitleId,
					     &m_freeTrial);
		if (ret < 0)
			return ret;
	}

	gameAppInfoGetStitle(&reg->info, &reg->entry->otherText);

	ret = gameAppEntryFill(reg, &m_appmetaDir, &m_dir);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = reg->info->end();
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));
	int lareaType = 0;

	it = reg->info->find(LSDB_KEY_LAREA_TYPE);
	if (it != end)
		lareaType = (int)gameLsdbValueInteger(it->second);

	if (gameCategoryIsGame(category.c_str()) && lareaType == 0) {
		int upStatus = 0;

		it = reg->info->find(LSDB_KEY_UP_STATUS);
		if (it != end)
			upStatus = (int)gameLsdbValueInteger(it->second);

		string dir("ux0:appmeta/new");
		string templatePath;

		if (upStatus == 3) {
			templatePath = dir + string("/retail/livearea/contents/template.xml");
			if (!LocalFile::Exists(templatePath.c_str()))
				templatePath = dir + string("/livearea/contents/template.xml");
		} else {
			templatePath = dir + string("/livearea/contents/template.xml");
		}

		if (!LocalFile::Exists(templatePath.c_str()))
			gameAppInfoSetAppconfBit12(&reg->info, true);
	}

	return 0;
}
