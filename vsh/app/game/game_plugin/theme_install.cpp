/*
 * A theme installed by gamePluginParamJobRun's job: from its package, or already extracted in
 * ux0:theme, then registered from its theme.xml (0x81030A6C..0x81031C1C). The file's name and the
 * class's are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* where the themes are installed */
#define GAME_THEME_DIR		"ux0:theme/"

/* the part of a content ID a theme's folder is named by, its last characters */
#define GAME_CONTENT_LABEL_LEN	16

/*
 * The installer of the theme param names, whose progress is scale of progress's (0x81030A6C).
 * Its folder is named "<title ID>-<label>".
 */
GameThemeInstaller::GameThemeInstaller(const GameJobParam *param, SceNpDrmInstallInfo *info,
				       GameJobProgress *progress, string *errorTitleId,
				       float scale)
	: m_param(param), m_info(info), m_progress(*progress, scale),
	  m_errorTitleId(errorTitleId)
{
	string titleId(info->info.titleId);
	string contentId(info->info.contentId);
	string label(contentId.length() >= GAME_CONTENT_LABEL_LEN ?
		     contentId.substr(contentId.length() - GAME_CONTENT_LABEL_LEN, string::npos) :
		     contentId);

	m_themeId = titleId + string("-") + label;

	m_weights[0] = 0.2f;
	m_weights[2] = 0.2f;
	m_weights[1] = 0.1f;
	m_weights[3] = 0.2f;
	m_weights[4] = 0.3f;
}

/* 0x81030D72, deleting 0x81030DBA */
GameThemeInstaller::~GameThemeInstaller()
{
}

/*
 * Whether the system is new enough for the theme: its package mounted with its license, and its
 * param.sfo's system version checked (0x81030E06).
 */
int GameThemeInstaller::CheckSystemVersion(void)
{
	string mountPoint;
	LsdbAppInfoPtr info;
	string sfoPath;
	int older = 1;

	int ret = gameThemeMount(m_themeId, string(m_param->path), m_info->license,
				 m_info->licenseSize, &mountPoint);

	if (ret >= 0) {
		info = LsdbAppInfoPtr(new LsdbAppInfo);

		if (info.get() == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			sfoPath = mountPoint + string(GAME_SFO_PATH);

			ret = gameSfoLoad(sfoPath, gameSfoGetAppParams(), gameSfoGetAppParamCount(),
					  &info);
			if (ret >= 0) {
				LsdbAppInfo::iterator it = info->find(LSDB_KEY_SYSTEM_VER);

				if (it == info->end()) {
					ret = 0;
				} else {
					ret = shellSystemVersionCheck(
						(SceUInt32)gameLsdbValueInteger(it->second), &older);
					if (ret == 0 && older == 1)
						ret = GAME_ERROR_SYSTEM_UPDATE;
				}
			}
		}
	}

	if (mountPoint.length() != 0) {
		int err = gameThemeUnmount(&mountPoint);

		if (err < 0)
			return err;
	}

	return ret;
}

/*
 * The theme's package copied into ux0:theme, registered from its theme.xml, and committed
 * (0x81031248).
 */
int GameThemeInstaller::InstallPackage(bool (*cancel)(void *arg), void *arg)
{
	GameThemeReader *reader = NULL;
	string themeDir;
	string contentPath;
	string path;
	LsdbTheme theme;
	common::SharedPtr<LsdbThemeInfo> themeInfo;
	GameThemeCopier *copier = NULL;
	int ret = 0;
	int err = CheckSystemVersion();

	if (err < 0)
		ret = err;

	if (ret >= 0) {
		if (cancel != NULL && cancel(arg))
			return GAME_ERROR_ABORTED;

		SetProgress(0);

		copier = new GameThemeCopier(m_info, m_errorTitleId);
		if (copier == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			path = string(m_param->path);
			ret = copier->Install(path, m_themeId, &themeDir, &contentPath, &m_progress,
					      m_weights[1], 0, 0);
		}

		if (ret >= 0 && cancel != NULL && cancel(arg))
			ret = GAME_ERROR_ABORTED;

		if (ret >= 0) {
			SetProgress(1);

			reader = new GameThemeReader(m_info, &m_themeId, &themeDir, &contentPath,
						     m_param->extracted);
			if (reader == NULL)
				ret = GAME_ERROR_NO_INFO_MEMORY;
			else
				ret = reader->Read(&theme, &themeInfo, 0, 0);
		}

		if (ret >= 0 && cancel != NULL && cancel(arg))
			ret = GAME_ERROR_ABORTED;

		if (ret >= 0) {
			SetProgress(2);
			ret = gameThemeSetInfo(&theme, &themeInfo);
		}

		if (ret >= 0) {
			delete reader;
			reader = NULL;

			SetProgress(3);
			ret = copier->Commit(&themeDir, &contentPath, &m_progress, m_weights[4], 0,
					     0);
		}

		if (ret >= 0) {
			delete copier;
			copier = NULL;
			SetProgress(4);
		}
	}

	if (copier != NULL)
		delete copier;
	if (reader != NULL)
		delete reader;
	return ret;
}

/* The theme already extracted in ux0:theme registered from its theme.xml (0x8103170C). */
int GameThemeInstaller::InstallExtracted(bool (*cancel)(void *arg), void *arg)
{
	string path(m_param->path);
	string themeDir = string(GAME_THEME_DIR) + m_themeId;
	LsdbTheme theme;
	common::SharedPtr<LsdbThemeInfo> themeInfo;
	GameThemeReader *reader = NULL;
	int ret;

	SetProgress(0);
	SetProgress(1);

	reader = new GameThemeReader(m_info, &m_themeId, &path, &themeDir, m_param->extracted);
	if (reader == NULL) {
		ret = GAME_ERROR_NO_INFO_MEMORY;
	} else {
		ret = reader->Read(&theme, &themeInfo, 0, 0);
		if (ret >= 0 && cancel != NULL && cancel(arg))
			ret = GAME_ERROR_ABORTED;

		if (ret >= 0) {
			SetProgress(2);
			ret = gameThemeSetInfo(&theme, &themeInfo);
		}

		if (ret >= 0) {
			delete reader;
			reader = NULL;
			SetProgress(3);
			SetProgress(4);
		}
	}

	if (reader != NULL)
		delete reader;
	return ret;
}

/* The theme installed, or registered when it is already extracted (0x81031B94). */
int GameThemeInstaller::Run(bool (*cancel)(void *arg), void *arg)
{
	if (m_param->extracted)
		return InstallExtracted(cancel, arg);

	return InstallPackage(cancel, arg);
}
