/*
 * The game plugin's installer of an application's package: the new application's app data and
 * its game data mounted, the package's gc_param.sfo read, the application's source made from
 * its ux0:app or gro0:app folder, and its record built with its sizes not counted yet
 * (0x81073E90..0x81076694). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "game_appsize.h"
#include "promoter.h"

using namespace paf;

GameAppInstaller::GameAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
				   string *errorTitleId, float scale)
	: GameInstaller(param, progress, errorTitleId, scale)
{
	m_prepareFlag = true;
}

GameAppInstaller::~GameAppInstaller()
{
}

GameInstallSource *GameAppInstaller::CreateSource(void)
{
	return new GameAppInstallSource(m_param, m_errorTitleId);
}

int GameAppInstaller::ReadAppInfo(SceNpDrmInstallInfo *info)
{
	string path = string(m_param->path) + string("/app/") + string(m_param->titleId);

	return gameDirGetStat(path, info);
}

bool GameAppInstaller::HasFttLicense(const string *licenseDir)
{
	string name;

	if (gameNpDrmGetFttName(&name) < 0)
		return false;

	string path(*licenseDir);

	path += "/";
	path.append(name);

	return LocalFile::Exists(path.c_str());
}

/*
 * The new application's app data mounted and the package's gc_param.sfo read; with its app
 * folder, its game data mounted with its patch and license (a free trial when only that license
 * is there), else the game card's application; the source made from what is mounted
 * (0x81074348).
 */
int GameAppInstaller::Installed(void)
{
	int ret = gameAppDataMount(string("new"), &m_appDataMountPoint, true);

	if (ret < 0) {
		gamePathGetMountedDrive(m_appDataMountPoint, m_errorTitleId);
		return ret;
	}

	string sfoPath;

	m_sfoInfo = LsdbAppInfoPtr(new LsdbAppInfo);
	if (m_sfoInfo.get() == NULL) {
		gameAppDataUnmount(&m_appDataMountPoint);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	sfoPath = m_appDataMountPoint + string("/gc_param.sfo");

	GameSfoReader *reader = new GameSfoReader(&m_sfoInfo);

	if (reader == NULL) {
		gameAppDataUnmount(&m_appDataMountPoint);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	reader->Load(sfoPath);
	delete reader;

	string dir = string(m_param->path) + string("/");

	m_appmetaDir = string(m_param->path) + string("/appmeta/") + string(m_param->titleId);
	m_dir = string("ur0:appmeta/") + string(m_param->titleId);

	string appDir = string(m_param->path) + string("/app/") + string(m_param->titleId);
	string path;
	string appPath;

	if (Dir::IsExist(appDir.c_str())) {
		LocalFileStat stat;
		string patchDir = string(m_param->path) + string("/patch/") + string(m_param->titleId);
		const char *patch = NULL;

		if (LocalFile::Getstat(patchDir.c_str(), &stat) == 0)
			patch = patchDir.c_str();

		SceUInt64 accountId = 0;

		ret = gameNpGetAccountId(&accountId);
		if (ret < 0) {
			gameAppDataUnmount(&m_appDataMountPoint);
			return ret;
		}

		string licenseDir =
			string(m_param->path) + string("/license/app/") + string(m_param->titleId);
		string rifPath;

		ret = gameRifGetPath(licenseDir, &accountId, &rifPath, NULL);
		if (ret < 0) {
			gameAppDataUnmount(&m_appDataMountPoint);
			gamePathGetMountedDrive(licenseDir, m_errorTitleId);
			return ret;
		}

		ret = gameGameDataMount(appDir.c_str(), patch, rifPath.c_str(), &m_gameMountPoint);
		if (ret < 0) {
			gameAppDataUnmount(&m_appDataMountPoint);
			if (ret == GAME_LICENSE_ERROR_NOT_FOUND && HasFttLicense(&licenseDir))
				m_freeTrial = true;
		}

		path = string("ux0:app/") + string(m_param->titleId);
		appPath = string("ux0:app/") + string(m_param->titleId);

		if (m_gameMountPoint.length() != 0)
			m_licenseDir = string(m_param->path) + string("/license/app/") +
				       string(m_param->titleId);
	} else {
		path = string("gro0:app/") + string(m_param->titleId);
		appPath = string("gro0:app/") + string(m_param->titleId);
	}

	GameFolderSourceParam param(dir.c_str(), path.c_str(), m_param->titleId,
				    m_appDataMountPoint.c_str(), m_dir.c_str(), appPath.c_str());
	GameAppmetaSource *source = new GameAppmetaSource(param);

	if (source == NULL) {
		GameAppInstaller::Cleanup();
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	if (m_freeTrial) {
		source->m_installed = true;

		SceNpDrmInstallInfo info;

		sce_paf_memset(&info, 0, sizeof(info));

		ret = ReadAppInfo(&info);
		if (ret < 0) {
			GameAppInstaller::Cleanup();
			delete source;
			return ret;
		}

		ret = source->SetDigest(info.param_digest, info.param_digest_size);
		if (ret < 0) {
			GameAppInstaller::Cleanup();
			delete source;
			return ret;
		}

		if (m_source != NULL)
			((GameAppInstallSource *)m_source)->SetFreeTrial(true);
	}

	m_infoSource = source;
	return 0;
}

/* the game data and the app data unmounted (0x81075AE6) */
int GameAppInstaller::Cleanup(void)
{
	if (m_gameMountPoint.length() != 0) {
		gameAppUnmount(&m_gameMountPoint);
		m_gameMountPoint.clear();
	}

	gameAppDataUnmount(&m_appDataMountPoint);
	return 0;
}

/*
 * The record completed: the parental level from the gc_param.sfo, the ebootparam.bin of the
 * application and of its first related one; then room for its icon, unless its storage has none
 * to give (0x81075B10).
 */
int GameAppInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret;

	if (m_sfoInfo.get() != NULL && m_sfoInfo->size() != 0) {
		ret = gameAppInfoCopyParental(&m_sfoInfo, &reg->info);
		if (ret < 0)
			return ret;
	}

	string path = m_appDataMountPoint + string("/ebootparam.bin");

	ret = gameAppInfoLoadEbootParam(path, &reg->info, m_errorTitleId);
	if (ret < 0) {
		gamePathGetMountedDrive(path, m_errorTitleId);
		return ret;
	}

	LocalFileStat stat;

	for (unsigned int i = 0; i < reg->related.size() && i < 1; i++) {
		char name[32];

		sce_paf_snprintf(name, sizeof(name), "/ebootparam%u.bin", i + 2);
		path = m_appDataMountPoint + string(name);

		if (LocalFile::Getstat(path.c_str(), &stat) == 0)
			gameAppInfoLoadEbootParam(path, &reg->related[i], NULL);
	}

	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_TITLE_ID);

	if (it == reg->info->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int storage = 0;

	ret = gameAppGetStorage(&reg->info, &storage);
	if (ret < 0)
		return ret;

	bool hasRoom = false;

	ret = gameLsdbHasRoom(&hasRoom);
	if (ret < 0)
		return ret;

	if (!hasRoom) {
		switch (storage) {
		case GAME_STORAGE_GRO0_OTHER:
			return GAME_ERROR_TOO_MANY_ICONS;
		case GAME_STORAGE_UX0:
			break;
		default:
			return GAME_ERROR_NO_STORAGE;
		}
	}

	return 0;
}

/*
 * The record built; an installed application of another version keeps what the record keeps;
 * an installed application's #appsize then has its patch's and its additional contents' sizes
 * not counted (0x81075F68).
 */
int GameAppInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret = GameInstaller::SetInfo(registerApp, reg);

	if (ret < 0)
		return ret;

	if (m_installed && gameAppVersionDiffers(&m_appInfo, &reg->info)) {
		m_versionChanged = true;

		ret = gameAppInfoCopyKept(&m_appInfo, &reg->info);
		if (ret < 0)
			return ret;
	}

	if (!m_installed)
		return 0;

	{
		string titleId(m_param->titleId);
		LsdbAppInfoPtr info(reg->info);
		AppInfoQClient client;
		SceUInt64 bits = 0;
		bool found = false;

		SceLsdb_A6D8772C(&client);

		if (SceLsdb_E90B81CD(&client) == 0) {
			LsdbValue value;

			if (SceLsdb_65ECEF39(&client, &titleId, LsdbKeyHasher<8>::Hash("#appsize"),
					     &value) == 0 &&
			    ((SceUInt64)gameLsdbValueInteger(value) >> 61) == 0) {
				bits = (SceUInt64)gameLsdbValueInteger(value);
				found = true;
			}
		}

		SceLsdb_E2101B03(&client);

		if (found) {
			bits |= GAME_APPSIZE_PATCH_MASK << GAME_APPSIZE_PATCH_SHIFT;
			gameLsdbValueSetInteger((*info)[LsdbKeyHasher<8>::Hash("#appsize")],
						(SceInt64)(bits & GAME_APPSIZE_MAX));
		}
	}

	{
		string titleId(m_param->titleId);
		LsdbAppInfoPtr info(reg->info);
		AppInfoQClient client;
		SceUInt64 bits = 0;
		bool found = false;

		SceLsdb_A6D8772C(&client);

		if (SceLsdb_E90B81CD(&client) == 0) {
			LsdbValue value;

			if (SceLsdb_65ECEF39(&client, &titleId, LsdbKeyHasher<8>::Hash("#appsize"),
					     &value) == 0 &&
			    ((SceUInt64)gameLsdbValueInteger(value) >> 61) == 0) {
				bits = (SceUInt64)gameLsdbValueInteger(value);
				found = true;
			}
		}

		SceLsdb_E2101B03(&client);

		if (found) {
			bits |= GAME_APPSIZE_ADDCONT_MASK << GAME_APPSIZE_ADDCONT_SHIFT;
			gameLsdbValueSetInteger((*info)[LsdbKeyHasher<8>::Hash("#appsize")],
						(SceInt64)bits);
		}
	}

	return 0;
}
