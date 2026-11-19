/*
 * The game plugin's source of a PSP game's package (or a PocketStation application's): with the
 * pspemu work folder mounted, the game's installed folder and its game data folder moved to
 * ux0:pspemu/temp/promote/pspemu/PSP/GAME (ps1emu for a PocketStation application) with the
 * package's content, then moved back to ux0:pspemu/PSP/GAME (ux0:ps1emu) and the license written
 * to ux0:pspemu/PSP/LICENSE, noted in addcont/info when it has a period (0x8108ED68..0x8109360C).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <plugin.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "../../shell/plugins.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "license_updater.h"
#include "promoter.h"

using namespace paf;

GamePspInstallSource::GamePspInstallSource(const SceNpDrmInstallInfo *info,
					   const string &titleId, string *errorTitleId)
	: m_info(info), m_titleId(titleId), m_ebootSig(false),
	  m_errorTitleId(errorTitleId), m_addcontInfoDir(""), m_pocketStation(false)
{
}

GamePspInstallSource::~GamePspInstallSource()
{
	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/appmeta", NULL, NULL, NULL, true);
	gamePspemuWorkUnmount(&m_workDir);
}

/*
 * A PSP game without an EBOOT signature (category MG): the installed game and its game data
 * moved to the promote folder, then the package's content moved over them; GAME_ERROR_GC_QUIET
 * for a game not installed yet (0x8108EF34).
 */
int GamePspInstallSource::MoveMgGame(const string *path, const string &titleId,
				     const string &promoteDir, const LsdbAppInfoPtr *sfo,
				     vector<string> *moved, string *detail)
{
	int ret;
	string gameDir("ux0:pspemu/temp/promote/pspemu/PSP/GAME");

	if (!Dir::IsExist(gameDir.c_str())) {
		ret = Dir::CreateRecursive(gameDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(gameDir, detail);
			return ret;
		}
	}

	moved->push_back(titleId);

	string installed = string("ux0:pspemu/PSP/GAME/") + titleId;
	string promoted = gameDir + string("/") + titleId;

	if (Dir::IsExist(installed.c_str())) {
		ret = LocalFile::RenameFile(installed.c_str(), promoted.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(promoted, detail);
			return ret;
		}
	} else if (!Dir::IsExist(promoted.c_str())) {
		ret = Dir::CreateRecursive(promoted.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(promoted, detail);
			return ret;
		}
	}

	string newDir("ux0:pspemu/temp/promote/new");

	ret = LocalFile::RenameFile(path->c_str(), newDir.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newDir, detail);
		return ret;
	}

	string content = newDir + string("/USRDIR/CONTENT");

	ret = gameMoveTree(content.c_str(), promoted.c_str(), NULL, NULL, detail);
	if (ret < 0)
		return ret;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(titleId.c_str(), &info);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (info->size() == 0)
		return GAME_ERROR_GC_QUIET;

	string gameDataId;

	ret = gamePspGetGameDataId(&info, &gameDataId);
	if (ret < 0)
		return ret;

	if (gameDataId.length() != 0) {
		string from = string("ux0:pspemu/PSP/GAME/") + gameDataId;
		string to = gameDir + string("/") + gameDataId;

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			gamePathGetMountedDrive(from, detail);
			return ret;
		}
		if (ret == 0)
			moved->push_back(gameDataId);
	}

	return 0;
}

/*
 * A PSP game with an EBOOT signature (category EG or ME): the installed game and the game data
 * its param.sfo names moved to the promote folder, then the package's content moved into the
 * game's folder there (0x8108FE50).
 */
int GamePspInstallSource::MoveEgMeGame(const string *path, const string &titleId,
				       const string &promoteDir, const LsdbAppInfoPtr *sfo,
				       vector<string> *moved, string *detail)
{
	int ret;

	moved->push_back(titleId);

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(titleId.c_str(), &info);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	string gameDir("ux0:pspemu/temp/promote/pspemu/PSP/GAME");

	if (!Dir::IsExist(gameDir.c_str())) {
		ret = Dir::CreateRecursive(gameDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(gameDir, detail);
			return ret;
		}
	}

	string installed = string("ux0:pspemu/PSP/GAME/") + titleId;

	if (Dir::IsExist(installed.c_str())) {
		string to = gameDir + string("/") + titleId;

		ret = LocalFile::RenameFile(installed.c_str(), to.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(to, detail);
			return ret;
		}
	}

	string gameDataId;

	ret = gamePspGetGameDataId(sfo, &gameDataId);
	if (ret < 0)
		return ret;

	if (gameDataId.length() != 0) {
		string from = string("ux0:pspemu/PSP/GAME/") + gameDataId;

		if (Dir::IsExist(from.c_str())) {
			string to = gameDir + string("/") + gameDataId;

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0) {
				gamePathGetMountedDrive(to, detail);
				return ret;
			}
			moved->push_back(gameDataId);
		}
	}

	string newDir("ux0:pspemu/temp/promote/new");

	ret = LocalFile::RenameFile(path->c_str(), newDir.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newDir, detail);
		return ret;
	}

	/* Sony's two branches on the installed record do the same */
	if (info->size() == 0) {
		string promoted = gameDir + string("/") + titleId;

		if (!Dir::IsExist(promoted.c_str())) {
			ret = Dir::CreateRecursive(promoted.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(promoted, detail);
				return ret;
			}
		}

		string content = newDir + string("/USRDIR/CONTENT");

		ret = gameMoveTree(content.c_str(), promoted.c_str(), NULL, NULL, detail);
		if (ret < 0)
			return ret;
	} else {
		string promoted = gameDir + string("/") + titleId;

		if (!Dir::IsExist(promoted.c_str())) {
			ret = Dir::CreateRecursive(promoted.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(promoted, detail);
				return ret;
			}
		}

		string content = newDir + string("/USRDIR/CONTENT");

		ret = gameMoveTree(content.c_str(), promoted.c_str(), NULL, NULL, detail);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * A PocketStation application: the installed one copied to the promote folder and removed, then
 * the package's texture.enc moved into its folder there (0x810912DA).
 */
int GamePspInstallSource::MovePocketStation(const string *path, const string &titleId,
					    const string &promoteDir, vector<string> *moved,
					    string *detail)
{
	int ret;
	string ps1Dir("ux0:pspemu/temp/promote/ps1emu");

	if (!Dir::IsExist(ps1Dir.c_str())) {
		ret = Dir::CreateRecursive(ps1Dir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(ps1Dir, detail);
			return ret;
		}
	}

	string installed = string("ux0:ps1emu/") + titleId;

	if (Dir::IsExist(installed.c_str())) {
		string old("ux0:pspemu/temp/promote/old");

		ret = gameCopyFileWithPool(installed.c_str(), old.c_str(), 0, 0, 0, 0);
		if (ret < 0) {
			gamePathGetMountedDrive(old, detail);
			return ret;
		}

		ret = gameRemoveDirectory(installed.c_str(), NULL, NULL, NULL, true);
		if (ret < 0) {
			gamePathGetMountedDrive(installed, detail);
			return ret;
		}
	}

	string newDir("ux0:pspemu/temp/promote/new");

	ret = LocalFile::RenameFile(path->c_str(), newDir.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newDir, detail);
		return ret;
	}

	string promoted = ps1Dir + string("/") + titleId;

	if (!Dir::IsExist(promoted.c_str())) {
		ret = Dir::CreateRecursive(promoted.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(promoted, detail);
			return ret;
		}
	}

	string texture = newDir + string("/USRDIR/CONTENT") + string("/texture.enc");

	if (LocalFile::Exists(texture.c_str())) {
		string to = promoted + string("/texture.enc");

		ret = LocalFile::RenameFile(texture.c_str(), to.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(to, detail);
			return ret;
		}
	}

	return 0;
}

/* the package at path moved to the promote folder by its kind (0x81091BC4) */
int GamePspInstallSource::MovePackage(const string *path, const SceNpDrmInstallInfo *info,
				      vector<string> *moved)
{
	int ret;

	if (m_pocketStation) {
		ret = MovePocketStation(path, string(info->info.titleId),
					string("ux0:pspemu/temp/promote"), moved, m_errorTitleId);
		if (ret < 0)
			return ret;
		return 0;
	}

	string content = *path + string("/USRDIR/CONTENT");
	string bootPath;

	ret = gamePspGetBootPath(content, &bootPath);
	if (ret < 0) {
		gamePathGetMountedDrive(content, m_errorTitleId);
		return ret;
	}

	LsdbAppInfoPtr sfo(new LsdbAppInfo);

	ret = gamePbpLoadSfo(&bootPath, gameSfoGetPspCheckParams(), gameSfoGetCheckParamCount(),
			     &sfo);
	if (ret < 0) {
		gamePathGetMountedDrive(bootPath, m_errorTitleId);
		return ret;
	}

	LsdbAppInfo::iterator it = sfo->find(LSDB_KEY_CATEGORY);

	if (it == sfo->end())
		return GAME_ERROR_BAD_DATA;

	string category(LsdbValueText(it->second));

	if (category == "EG" || category == "ME") {
		m_ebootSig = true;
		ret = MoveEgMeGame(path, string(info->info.titleId),
				   string("ux0:pspemu/temp/promote"), &sfo, moved, m_errorTitleId);
		if (ret < 0)
			return ret;
		return 0;
	}

	if (category == "MG") {
		m_ebootSig = false;
		ret = MoveMgGame(path, string(info->info.titleId),
				 string("ux0:pspemu/temp/promote"), &sfo, moved, m_errorTitleId);
		if (ret < 0)
			return ret;

		if (Dir::IsExist("grw0:temp/plus"))
			Dir::RemoveRecursive("grw0:temp/plus");
		if (Dir::IsExist("ux0:temp/plus"))
			Dir::RemoveRecursive("ux0:temp/plus");

		m_addcontInfoDir = string("ux0:") + string("addcont/info");
		return 0;
	}

	return GAME_ERROR_NOT_APP_PACKAGE;
}

int GamePspInstallSource::Install(const string *path, const string *titleId, string *dir,
				  string *installPath, GameJobProgress *progress, int reserved1,
				  int reserved2, float scale)
{
	int ret;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (!gameTypeIsSpecial(m_info->info.contentType))
		return GAME_ERROR_INVALID_ARGUMENT;

	m_pocketStation = gameIsPocketStation(string(m_info->info.contentId));

	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	if (!Dir::IsExist("ux0:pspemu/temp/promote")) {
		ret = Dir::CreateRecursive("ux0:pspemu/temp/promote", 6);
		if (ret < 0)
			return ret;
	}

	ret = gamePspemuWorkMount(&m_workDir);
	if (ret < 0)
		return ret;

	ret = MovePackage(path, m_info, &m_moved);
	if (ret < 0) {
		if (ret != GAME_ERROR_GC_QUIET)
			return ret;
	} else {
		ret = 0;
	}

	if (m_pocketStation) {
		*dir = string("ux0:pspemu/temp/promote/ps1emu");
		*installPath = string("ux0:ps1emu");
	} else {
		*dir = string("ux0:pspemu/temp/promote/pspemu/PSP/GAME");
		*installPath = string("ux0:pspemu/PSP/GAME");
	}

	return ret;
}

/* livearea_plugin told of a PocketStation application's license */
static void NotifyLiveArea(void)
{
	Plugin *livearea = Plugin::Find("livearea_plugin");

	if (livearea != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)livearea->GetInterface(1);

		if (iface != NULL)
			iface->func[LIVEAREA_PLUGIN_FUNC_LICENSE_ADDED]();
	}
}

int GamePspInstallSource::Complete(string *dir, string *installPath, int reserved1,
				   int reserved2, int reserved3, float scale)
{
	int ret = 0;
	unsigned int i;

	if (!Dir::IsExist(installPath->c_str())) {
		ret = Dir::CreateRecursive(installPath->c_str(), 6);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < m_moved.size(); i++) {
		string from = *dir + string("/") + m_moved[i];
		string to = *installPath + string("/") + m_moved[i];

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
			return ret;
	}

	if (m_pocketStation) {
		string titleId(m_info->info.titleId);
		string from = *dir + string("/") + titleId;
		string to = *installPath + string("/") + titleId;

		ret = gameCopyFileWithPool(from.c_str(), to.c_str(), 0, 0, 0, 0);
		if (ret < 0)
			return ret;
	}

	if (m_info->info.headerFields[0] != 2)
		return 0;

	string licenseDir("ux0:pspemu/PSP/LICENSE");

	if (m_info->licenseSize == 0) {
		string path = licenseDir;

		if (!Dir::IsExist(path.c_str())) {
			ret = Dir::CreateRecursive(path.c_str(), 6);
			if (ret < 0)
				return ret;
		}

		path.append(string("/") + string(m_info->info.contentId) + string(".rif"));

		common::SharedPtr<File> file = LocalFile::Open(path.c_str(),
							       SCE_O_RDWR | SCE_O_CREAT |
							       SCE_O_TRUNC, 6, &ret);
		if (ret < 0)
			return ret;
		file->Close();

		if (m_pocketStation)
			NotifyLiveArea();
	} else {
		string licensePath;

		if ((int)m_info->licenseSize <= 0)
			return GAME_ERROR_BAD_LICENSE;

		string name = string(m_info->info.contentId) + string(".rif");

		ret = gameFileWriteToDir(m_info->license, m_info->licenseSize, &licenseDir,
					 name.c_str(), &licensePath, m_errorTitleId);
		if (ret < 0)
			return ret;

		if (m_pocketStation)
			NotifyLiveArea();

		if (m_addcontInfoDir.length() != 0) {
			char contentId[48];

			sce_paf_memset(contentId, 0, sizeof(contentId));

			SceUInt64 startTime = 0;
			SceUInt64 expireTime = 0;
			GameRifInfo rif(0);

			ret = rif.GetPeriod((SceNpDrmLicense *)m_info->license, m_info->licenseSize,
					    contentId, &startTime, &expireTime);
			if (ret == 0) {
				string cid(contentId);

				if (expireTime == 0x7FFFFFFFFFFFFFFFULL)
					return 0;

				if ((Dir::IsExist(m_addcontInfoDir.c_str()) ||
				     Dir::CreateRecursive(m_addcontInfoDir.c_str(), 6) >= 0) &&
				    gamePspAddcontInfoPrepare(m_addcontInfoDir.c_str()) >= 0) {
					GameAddcontLicenseInfo info;

					sce_paf_memset(&info, 0, sizeof(info));
					sce_paf_memcpy(info.contentId, cid.c_str(),
						       sizeof(info.contentId));
					info.startTime = startTime;
					info.expireTime = expireTime;
					gamePspAddcontInfoWrite(m_addcontInfoDir.c_str(), &info);
				}
				ret = 0;
			}
		}
	}
	return 0;
}
