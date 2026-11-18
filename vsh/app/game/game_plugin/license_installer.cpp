/*
 * The game plugin's installer of a license's package: the installed application's record taken
 * again with the new license's up status and its modification time now; a free trial that the
 * license makes whole has its LiveArea copied again from its game data (0x81076694..0x81077624).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameLicenseInstaller::GameLicenseInstaller(const GameInstallParam *param,
					   GameJobProgress *progress, string *errorTitleId,
					   float scale)
	: GameInstaller(param, progress, errorTitleId, scale)
{
	m_prepareFlag = false;
	m_noLicense = false;
}

GameLicenseInstaller::~GameLicenseInstaller()
{
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
}

/*
 * The application's record read by the base; a license is installed only for an installed
 * application, a free trial when its record says so (0x8107672A).
 */
int GameLicenseInstaller::Prepare(bool flag)
{
	int ret = GameInstaller::Prepare(flag);

	if (ret < 0)
		return ret;

	if (m_appInfo->size() == 0)
		return GAME_ERROR_NOT_INSTALLED;

	if (m_param->noLicense)
		m_noLicense = true;

	if (gameAppInfoPromoteAttrBit2(&m_appInfo))
		m_freeTrial = true;

	return 0;
}

GameInstallSource *GameLicenseInstaller::CreateSource(void)
{
	return new GameLicenseInstallSource(m_param, m_noLicense, m_errorTitleId);
}

/*
 * The application's appmeta folder from its record's LiveArea path, up to its title ID; the
 * license folder the package's; no source of the application's information (0x81076790).
 */
int GameLicenseInstaller::Installed(void)
{
	LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_LIVEAREA_PATH);

	if (it == m_appInfo->end())
		return GAME_ERROR_NO_VALUE;

	string liveAreaPath(LsdbValueText(it->second));
	unsigned int pos = liveAreaPath.rfind(m_titleId.c_str(), string::npos, m_titleId.length());

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	m_appmetaDir = liveAreaPath.substr(0, pos + m_titleId.length());
	m_dir = string("ur0:appmeta/") + m_titleId;
	m_licenseDir = m_param->path;
	m_infoSource = NULL;
	return 0;
}

/*
 * The free trial's application mounted with the package's license, its eboot.bin and its first
 * related application's read into the record, and its LiveArea copied by a GameLiveAreaCopier
 * (0x810769C4). Without a LiveArea path in the record, Sony's returns the key itself.
 */
int GameLicenseInstaller::CopyLiveArea(GameAppRegistration *reg, GameCancelFunc cancel,
				       void *arg)
{
	string orgPath;
	string mountPoint;
	string path;
	LocalFileStat stat;
	int ret;

	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_ORG_PATH);

	if (it == reg->info->end()) {
		ret = (int)LSDB_KEY_ORG_PATH;
	} else {
		orgPath = LsdbValueText(it->second);

		ret = gameGameDataMount(orgPath.c_str(), NULL, m_param->path, &mountPoint);
		if (ret >= 0) {
			path = mountPoint + string("/eboot.bin");
			gameAppInfoUpdateEbootParam(path, &reg->info, NULL);

			for (unsigned int i = 0; i < reg->related.size() && i < 1; i++) {
				char name[32];

				sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
				path = mountPoint + string(name);

				if (LocalFile::Getstat(path.c_str(), &stat) == 0)
					gameAppInfoUpdateEbootParam(path, &reg->related[i], NULL);
			}

			GameLiveAreaCopier *copier = new GameLiveAreaCopier(false, string(""), cancel,
									    arg, &m_progress, NULL,
									    0.45f);

			ret = GAME_ERROR_NO_INFO_MEMORY;
			if (copier != NULL) {
				ret = copier->Run(reg, &mountPoint);
				delete copier;
			}
		}
	}

	if (mountPoint.length() != 0)
		gameAppUnmount(&mountPoint);

	return ret;
}

/*
 * The record checked for its registration; a free trial made whole by the license has its
 * LiveArea copied again (0x81076D98).
 */
int GameLicenseInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret;

	if (!m_freeTrial || m_noLicense) {
		ret = gameAppPrepareRegister(&reg->info, &m_otherFlag, m_errorTitleId);
		if (ret < 0)
			return ret;
	} else {
		ret = CopyLiveArea(reg, cancel, arg);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * The record the installed application's, with its modification time now, its LiveArea path and
 * its up status from the installed license; a free trial made whole is no longer one, and a
 * game without its template.xml has bit 12 of #appconf set; the splash the application's
 * pic0.png (0x81076DCC).
 */
int GameLicenseInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	SceNpDrmLicense *license = NULL;
	SceSize licenseSize = 0;
	int ret;

	if (!m_noLicense) {
		GameLicenseInstallSource *source = (GameLicenseInstallSource *)m_source;

		license = source->m_license;
		licenseSize = source->m_licenseSize;
		if (license == NULL)
			return GAME_ERROR_NOT_READY;
	}

	reg->info = m_appInfo;

	if (m_freeTrial && !m_noLicense)
		gameAppInfoSetPromoteAttrBit2(&reg->info, false);

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetLiveAreaPath(&reg->info, &m_appmetaDir, &m_dir);
	if (ret < 0)
		return ret;

	if (m_licenseDir.length() != 0 && license != NULL && licenseSize != 0) {
		ret = gameAppInfoSetUpStatusFromLicense(license, licenseSize, &reg->info,
							(int)&m_appmetaDir, (int)m_errorTitleId,
							&m_noLicense);
		if (ret < 0)
			return ret;
	}

	reg->liveAreaState = 0;

	if (m_freeTrial && !m_noLicense) {
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

			it = reg->info->find(LSDB_KEY_ORG_PATH);
			if (it == reg->info->end())
				return GAME_ERROR_NO_VALUE;

			string orgPath(LsdbValueText(it->second));
			string dir(orgPath);
			string templatePath;

			if (upStatus == 3) {
				templatePath = dir +
					string("/sce_sys/retail/livearea/contents/template.xml");
				if (!LocalFile::Exists(templatePath.c_str()))
					templatePath =
						dir + string("/sce_sys/livearea/contents/template.xml");
			} else {
				templatePath = dir + string("/sce_sys/livearea/contents/template.xml");
			}

			gameAppInfoSetAppconfBit12(&reg->info,
						   !LocalFile::Exists(templatePath.c_str()));
		}
	}

	if (!m_noLicense) {
		string splashPath = m_dir + string("/pic0.png");

		gameLsdbValueSetText((*reg->info)[LSDB_KEY_SPLASH_PATH], splashPath);
	}

	return 0;
}
