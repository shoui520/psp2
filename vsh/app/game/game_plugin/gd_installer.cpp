/*
 * The game plugin's installer of an application's package folder: the installed application's
 * record read and the update checked against it, the folder moved into place, and the record
 * built from its param.sfo (0x8106D3EC..0x810713C0). The file's name and the functions' are ours
 * (no record names them); the strings are Sony's.
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

GameGdInstaller::GameGdInstaller(const char *path, const SceNpDrmInstallInfo *info,
				 GameJobProgress *progress, string *errorTitleId, float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale),
	  m_appInfo(new LsdbAppInfo), m_patchStorage(0), m_install(true), m_badAttribute(0)
{
}

GameGdInstaller::~GameGdInstaller()
{
	UnmountApp();
}

/*
 * The installed application's record read; an installed application not running, and an update
 * package checked against it (0x8106D57C).
 */
int GameGdInstaller::Prepare(bool flag)
{
	if (m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(m_info->info.titleId, &m_appInfo);

	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (m_appInfo->size() == 0) {
		m_flag = false;
		return 0;
	}

	m_flag = true;

	ret = gameAppCheckNotRunning(m_titleId, &m_appInfo);
	if (ret < 0)
		return ret;

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE)
		return 0;

	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	GameGdUpdateCheck *check = new GameGdUpdateCheck(string(m_info->info.titleId), string(m_path),
							 !param->imported, m_errorTitleId);

	if (check == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = check->Check(&m_appInfo);
	if (ret < 0 && ret != GAME_ERROR_GC_QUIET) {
		delete check;
		return ret;
	}

	if (ret == GAME_ERROR_GC_QUIET)
		m_install = false;

	delete check;
	return 0;
}

/*
 * The package folder's source: an update's, its stages weighed again when the application is on
 * the game card and the update on ux0:, or an application's (0x8106D702).
 */
GameInstallSource *GameGdInstaller::CreateSource(int *ret)
{
	*ret = 0;

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) {
		int storage = 0;
		int err = gameAppGetStorage(&m_appInfo, &storage);

		if (err < 0) {
			*ret = err;
			return NULL;
		}

		if (storage == GAME_STORAGE_GRO0 && sce_paf_strncmp(m_path, "ux0:", 4) == 0) {
			m_weights[2] = 0.1f;
			m_weights[1] = 0.5f;
			m_weights[5] = 0.1f;
			m_weights[3] = 0.2f;
			m_weights[4] = 0.05f;
		}

		return new GameGdUpdateSource(m_info, &m_appInfo, m_errorTitleId);
	}

	const GameInstallExParam *param = (const GameInstallExParam *)m_path;

	return new GameGdAppSource(m_info, m_noLicense, string(param->mountPoint),
				   m_errorTitleId);
}

int GameGdInstaller::CheckAttribute(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *update)
{
	SceUInt8 installedBit = 0;
	SceUInt8 updateBit = 0;
	LsdbAppInfo::iterator it = (*installed)->find(LSDB_KEY_ATTRIBUTE);

	if (it != (*installed)->end())
		installedBit = (SceUInt8)gameLsdbValueInteger(it->second);

	/* Sony's code compares the update's lookup with the installed record's end */
	it = (*update)->find(LSDB_KEY_ATTRIBUTE);
	if (it != (*installed)->end())
		updateBit = (SceUInt8)gameLsdbValueInteger(it->second);

	if ((installedBit ^ updateBit) & 1)
		return GAME_ERROR_BAD_DATA;

	return 0;
}

int GameGdInstaller::MountApp(const string *dir, const string *path, string *mountPoint,
			      int *patchStorage)
{
	if (!(m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE)) {
		LsdbAppInfoPtr sfo(new LsdbAppInfo);

		if (sfo.get() == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		string sfoPath = *dir + string("/sce_sys/param.sfo");
		int ret = gameSfoLoad(string(sfoPath.c_str()), gameSfoGetAppVerParams(),
				      gameSfoGetAppVerParamCount(), &sfo);

		if (ret < 0) {
			gamePathGetMountedDrive(sfoPath, m_errorTitleId);
			return ret;
		}

		SceUInt8 attribute = 0;
		LsdbAppInfo::iterator it = sfo->find(LSDB_KEY_ATTRIBUTE);

		if (it != sfo->end())
			attribute = (SceUInt8)gameLsdbValueInteger(it->second);

		string drive;

		ret = gamePathGetDataDrive(string(*path), (attribute & 1) != 0, &drive);
		if (ret < 0)
			return ret;

		string patchPath = drive + string("patch/") + m_titleId;
		const char *patch = NULL;

		if (Dir::IsExist(patchPath.c_str()))
			patch = patchPath.c_str();

		ret = gameAppMount(dir->c_str(), patch, m_info->license, m_info->licenseSize,
				   mountPoint);
		if (ret < 0)
			return ret;

		return 0;
	}

	LsdbAppInfoPtr sfo(new LsdbAppInfo);

	if (sfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string sfoPath = *dir + string("/sce_sys/param.sfo");
	int ret = gameSfoLoad(string(sfoPath.c_str()), gameSfoGetAppVerParams(),
			      gameSfoGetAppVerParamCount(), &sfo);

	if (ret < 0) {
		gamePathGetMountedDrive(sfoPath, m_errorTitleId);
		return ret;
	}

	LsdbAppInfo::iterator it = sfo->find(LSDB_KEY_CATEGORY);

	if (it == sfo->end())
		return GAME_ERROR_BAD_DATA;

	string category(LsdbValueText(it->second));

	it = m_appInfo->find(LSDB_KEY_ORG_PATH);
	if (it == m_appInfo->end())
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));
	SceUInt64 attribute = 0;

	it = m_appInfo->find(LSDB_KEY_ATTRIBUTE);
	if (it != m_appInfo->end())
		attribute = gameLsdbValueInteger(it->second);

	m_patchStorage = gamePatchPathGetStorage(orgPath, attribute, category);
	if (m_patchStorage == 0)
		return GAME_ERROR_BAD_STORAGE;

	ret = CheckAttribute(&m_appInfo, &sfo);
	if (ret < 0) {
		m_badAttribute = 1;
		return ret;
	}

	ret = gameAppMount(orgPath.c_str(), dir->c_str(), NULL, 0, mountPoint);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The installed folder mounted, unless the caller did; its LiveArea's folder and paths by where
 * the application is installed, and its source made, with the param.sfo's digest for a folder
 * the caller mounted (0x8106E7B4).
 */
int GameGdInstaller::Installed(string *dir, string *path)
{
	if (!m_install)
		return 0;

	if (m_noLicense) {
		m_mountPoint = *dir;
	} else {
		int ret = MountApp(dir, path, &m_mountPoint, &m_patchStorage);

		if (ret < 0)
			return ret;
	}

	string appPath;
	string promotePath;
	string titleId(m_info->info.titleId);

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) {
		LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_ORG_PATH);

		if (it == m_appInfo->end())
			return GAME_ERROR_NO_VALUE;

		string orgPath(LsdbValueText(it->second));

		if (orgPath.find("gro0:") != string::npos) {
			SceUInt8 attribute = 0;

			it = m_appInfo->find(LSDB_KEY_ATTRIBUTE);
			if (it != m_appInfo->end())
				attribute = (SceUInt8)gameLsdbValueInteger(it->second);

			if (attribute & 1)
				m_dir = string("ur0:appmeta/") + titleId;
			else
				m_dir = string("ur0:appmeta/") + titleId;

			appPath = string("gro0:app/") + titleId;
			promotePath = string("gro0:app/") + titleId;
		} else if (orgPath.find("pd0:") != string::npos) {
			m_dir = string("ur0:appmeta/") + titleId;
			appPath = string("pd0:app/") + titleId;
			promotePath = string("pd0:app/") + titleId;
		} else {
			m_dir = string("ur0:appmeta/") + titleId;
			appPath = string("ux0:app/") + titleId;
			promotePath = string("ux0:app/") + titleId;
		}
	} else {
		m_dir = string("ur0:appmeta/") + titleId;
		promotePath = *path;
		appPath = *path;
	}

	m_appDir = m_mountPoint;

	string sysDir = m_mountPoint + string("/sce_sys");
	GameFolderSourceParam param(m_mountPoint.c_str(), promotePath.c_str(), titleId.c_str(),
				    sysDir.c_str(), m_dir.c_str(), appPath.c_str(), m_appInfo);
	GameAppSource *source = new GameAppSource(param);

	if (source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_noLicense) {
		source->m_installed = true;

		int ret = source->SetDigest(m_info->param_digest, m_info->param_digest_size);

		if (ret < 0) {
			delete source;
			return ret;
		}
	}

	m_infoSource = source;
	return 0;
}

/* the application unmounted, and the source deleted (0x8106FAD6) */
int GameGdInstaller::Cleanup(void)
{
	UnmountApp();

	if (m_infoSource != NULL)
		delete m_infoSource;
	m_infoSource = NULL;

	return 0;
}

/*
 * The record built: the install time kept from the installed application's record, the change
 * time now, the shell's keys kept; a LiveArea without template.xml marked, and an update's
 * #appsize patch field set to not counted (0x8106FB20).
 */
int GameGdInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret = GamePackageInstaller::SetInfo(registerApp, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_appInfo, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	if (m_appInfo.get() != NULL) {
		ret = gameAppInfoCopyShellKeys(&m_appInfo, &reg->info);
		if (ret < 0)
			return ret;
	}

	LsdbAppInfo::iterator end = reg->info->end();
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_LAREA_TYPE);

	if ((it == end || (int)gameLsdbValueInteger(it->second) == 0) &&
	    !(m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE)) {
		int upStatus = 0;

		it = reg->info->find(LSDB_KEY_UP_STATUS);
		if (it != reg->info->end())
			upStatus = (int)gameLsdbValueInteger(it->second);

		string path;

		if (upStatus == 3) {
			path = string(m_path) + string("/sce_sys/retail/livearea/contents/template.xml");
			if (!LocalFile::Exists(path.c_str()))
				path = string(m_path) + string("/sce_sys/livearea/contents/template.xml");
		} else {
			path = string(m_path) + string("/sce_sys/livearea/contents/template.xml");
		}

		if (!LocalFile::Exists(path.c_str()))
			gameAppInfoSetAppconfBit12(&reg->info, true);
	}

	if (!(m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE))
		return 0;

	string titleId(m_info->info.titleId);
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

	return 0;
}

/*
 * The other step: the application unmounted, its LiveArea and patch folders removed (where the
 * patch is installed decides the patch folder) and the original application promoted again in
 * their place. A promotion done returns GAME_ERROR_GC_QUIET; an update whose attribute did not
 * match returns -1 (0x81070364).
 */
int GameGdInstaller::SetOtherInfo(bool registerApp, GameAppRegistration *reg, string *dir,
				  string *path)
{
	Cleanup();

	LsdbAppInfo::iterator end = m_appInfo->end();
	LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_ORG_PATH);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));
	SceUInt64 attribute = 0;

	it = m_appInfo->find(LSDB_KEY_ATTRIBUTE);
	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	if ((m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) && m_badAttribute == 1)
		return -1;

	string appmetaDir;
	string patchDir;

	if (orgPath.find("gro0:") != string::npos) {
		if (attribute & 1) {
			appmetaDir = string("ur0:appmeta/") + m_titleId;
			patchDir = string("ux0:patch/") + m_titleId;
		} else {
			appmetaDir = string("ur0:appmeta/") + m_titleId;
			patchDir = string("grw0:patch/") + m_titleId;
		}
	}

	if (orgPath.find("pd0:") != string::npos) {
		appmetaDir = string("ur0:appmeta/") + m_titleId;
		patchDir = string("ux0:patch/") + m_titleId;
	} else {
		appmetaDir = string("ur0:appmeta/") + m_titleId;
		patchDir = string("ux0:patch/") + m_titleId;
	}

	gameRemoveDirectory(appmetaDir.c_str(), NULL, NULL, NULL, true);
	gameRemoveDirectory(patchDir.c_str(), NULL, NULL, NULL, true);

	GameAppPromoter *promoter =
		new GameAppPromoter(orgPath.c_str(), registerApp, reg, NULL, NULL, 0, NULL, NULL);

	if (promoter == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	promoter->m_option = true;

	int ret = promoter->Run();

	if (ret < 0) {
		delete promoter;
		return ret;
	}

	delete promoter;
	return GAME_ERROR_GC_QUIET;
}

/*
 * The eboot parameters read from the mounted application; a patch package of a game only
 * registered, an update's appmeta or the application's LiveArea written (0x81070E60).
 */
int GameGdInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	string ebootPath = m_mountPoint + string("/eboot.bin");

	if (m_noLicense)
		ebootPath.assign(NULL, 0);

	gameAppInfoUpdateEbootParam(ebootPath, &reg->info, NULL);

	LocalFileStat stat;
	char name[32];

	for (unsigned int i = 0; i < reg->related.size() && i < 1; i++) {
		sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
		ebootPath = m_mountPoint + string(name);

		if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0)
			gameAppInfoUpdateEbootParam(ebootPath, &reg->related[i], NULL);
	}

	if (m_flag) {
		bool patch = false;
		LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_CATEGORY);

		if (it != m_appInfo->end()) {
			string category(LsdbValueText(it->second));

			patch = category == "gp" || category == "gpc" || category == "gpd";
		}

		bool game = m_info->info.contentType == GAME_CONTENT_TYPE_GD &&
			    !(m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE);

		if (patch && game)
			return 0;
	}

	GameAppmetaWriterIf *writer;

	if (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) {
		writer = new GameGdUpdateAppmetaWriter(cancel, arg, &m_progress, NULL, 0.45f);
	} else {
		const GameInstallExParam *param = (const GameInstallExParam *)m_path;

		writer = new GameLiveAreaCopier(m_noLicense, string(param->mountPoint),
						cancel, arg, &m_progress, NULL, 0.45f);
	}

	if (writer == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = writer->Run(reg, &m_mountPoint);

	if (ret < 0) {
		delete writer;
		return ret;
	}

	delete writer;
	return 0;
}
