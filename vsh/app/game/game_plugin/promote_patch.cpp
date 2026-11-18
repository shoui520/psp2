/*
 * The game plugin's patch promoter: an application's patch, promoted in the drive's temp/promote,
 * moved into its patch folder (0x8103FB34..0x81045DF6), and the checks of the game card's patch
 * against the installed one. The file's name and the functions' are ours (no record names them);
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

#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the digits of an APP_VER */
#define GAME_APP_VER_DIGITS	8

/*
 * PLACEHOLDER: whether the application of info has its LiveArea in ur0:appmeta, set in
 * hasAppmeta (0x8105159A, in another file of the shell; its name is ours).
 */
int gameAppInfoCheckAppmeta(LsdbAppInfoPtr *info, bool *hasAppmeta, int userData);

GamePatchPromoter::GamePatchPromoter(const char *path, const LsdbAppInfoPtr *sfo, bool flag1,
				     bool flag2, const GameJobProgress *progress, int userData,
				     float scale)
	: GamePromoter(path, progress, userData, scale), m_sfo(sfo), m_flag1(flag1),
	  m_flag2(flag2), m_move(false), m_drive(0)
{
	m_weights[0] = 0.3f;
	m_weights[1] = 0.3f;
	m_weights[2] = 0.35f;
	m_weights[3] = 0.05f;
}

GamePatchPromoter::~GamePatchPromoter()
{
	if (m_mountPoint.length() != 0)
		gameAppUnmount(&m_mountPoint);

	gameRemoveDirectory("grw0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ur0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ur0:appmeta/new", 0, 0, 0, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
}

/*
 * The promoted folder moved into the drive's patch folder; on ux0 the patch there is kept as
 * temp/promote/<name>_org (0x8103FC64).
 */
int GamePatchPromoter::MovePatch()
{
	string drive("");

	drive = "";
	if (m_drive == GAME_PATCH_DRIVE_GRW0)
		drive = "grw0:";
	else if (m_drive == GAME_PATCH_DRIVE_UX0)
		drive = "ux0:";
	else
		return GAME_ERROR_INVALID_ARGUMENT;

	int ret;

	string patchDir = drive + string("patch");

	if (!Dir::IsExist(patchDir.c_str()))
		Dir::Create(patchDir.c_str(), 6);

	string promoteDir = drive + string("temp/promote");

	if (m_drive == GAME_PATCH_DRIVE_UX0) {
		string patchPath = patchDir + string("/") + m_name;
		string orgPath = promoteDir + string("/") + m_name + string("_org");

		if (Dir::IsExist(patchPath.c_str())) {
			ret = LocalFile::RenameFile(patchPath.c_str(), orgPath.c_str());
			if (ret < 0)
				return ret;
		}
	}

	string promotePath = promoteDir + string("/") + m_name;
	string patchPath = patchDir + string("/") + m_name;

	ret = LocalFile::RenameFile(promotePath.c_str(), patchPath.c_str());
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The promotion, then the patch moved into place, and the application's record updated; an
 * update that fails unregisters a patch that was not an install (0x81040762).
 */
int GamePatchPromoter::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			   void *arg)
{
	m_param.registerEnd = false;

	int ret = GamePromoter::Run(reg, registerApp, cancel, arg);

	if (ret < 0)
		return ret;

	if (m_move) {
		if (m_mountPoint.length() != 0)
			gameAppUnmount(&m_mountPoint);

		ret = MovePatch();
		if (ret < 0) {
			if (registerApp && m_notifyTopmenu)
				gameAppRegisterEnd(&reg->info);
			return ret;
		}
	}

	if (registerApp && gameAppUpdate(&reg->info, m_param.replace, m_notifyTopmenu) < 0 &&
	    !m_install) {
		Plugin *applauncher = Plugin::Find("applauncher_plugin");

		if (applauncher != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)applauncher->GetInterface(1);

			if (iface != NULL && ((int (*)(string *))iface->func[1])(&m_name) < 0)
				return GAME_ERROR_UNREGISTER;
		}
	}

	SetProgress(3);

	return 0;
}

int gameAppInfoGetAppVer(const LsdbAppInfoPtr *info, SceUInt32 *version)
{
	*version = 0;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APP_VER);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	int ret = gameVersionToNumber(LsdbValueText(it->second), GAME_APP_VER_DIGITS, version);

	if (ret < 0)
		return ret;

	return 0;
}

int gameGcPatchGetAppVer(const string &titleId, SceUInt32 *version)
{
	*version = 0;

	string path = string("gro0:patch") + string("/");

	path.append(titleId + string("/"));
	path.append(string("sce_sys//param.sfo"));

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameSfoLoad(string(path.c_str()), gameSfoGetAppVerParams(),
			      gameSfoGetAppVerParamCount(), &info);

	if (ret < 0)
		return ret;

	ret = gameAppInfoGetAppVer(&info, version);
	if (ret < 0)
		return ret;

	return 0;
}

bool gameUx0PatchExists(const string &titleId)
{
	string path("ux0:patch");

	path.append(string("/") + titleId);
	path.append(string("/sce_sys/param.sfo"));

	return Dir::IsExist(path.c_str());
}

bool gameGrw0PatchExists(const string &titleId)
{
	string path("grw0:patch");

	path.append(string("/") + titleId);
	path.append(string("/sce_sys/param.sfo"));

	return Dir::IsExist(path.c_str());
}

bool gameGcPatchExists(const string &titleId)
{
	string path("gro0:patch");

	path.append(string("/") + titleId);
	path.append(string("/sce_sys/param.sfo"));

	return Dir::IsExist(path.c_str());
}

bool gameAppIsUpdateNeeded(const LsdbAppInfoPtr *info, const string &titleId, const int *storage)
{
	if (!gameGcPatchExists(titleId))
		return false;

	if (*storage == 1)
		return !gameGrw0PatchExists(titleId);

	if (*storage == 2) {
		if (!gameUx0PatchExists(titleId))
			return true;

		SceUInt32 gcVersion = 0;

		if (gameGcPatchGetAppVer(titleId, &gcVersion) >= 0) {
			SceUInt32 version = 0;

			if (gameAppInfoGetAppVer(info, &version) >= 0 && gcVersion > version)
				return true;
		}
	}

	return false;
}

bool gamePatchExists(const string &titleId)
{
	return gameUx0PatchExists(titleId);
}

int GamePatchPromoter::CopyCardPatch(GameJobProgress *progress)
{
	string drive("");

	drive = "";
	if (m_drive == GAME_PATCH_DRIVE_GRW0)
		drive = "grw0:";
	else if (m_drive == GAME_PATCH_DRIVE_UX0)
		drive = "ux0:";
	else
		return GAME_ERROR_INVALID_ARGUMENT;

	int ret;
	string tempDir = drive + string("temp");

	if (Dir::IsExist(tempDir.c_str())) {
		ret = Dir::RemoveRecursive(tempDir.c_str());
		if (ret < 0)
			return ret;
	}

	string promoteDir = drive + string("temp/promote");

	ret = Dir::CreateRecursive(promoteDir.c_str(), 6);
	if (ret < 0)
		return ret;

	string cardPath = string("gro0:patch") + string("/") + m_name;
	string promotePath = promoteDir + string("/") + m_name;

	ret = gameCopyFileWithPool(cardPath.c_str(), promotePath.c_str(), 0, 0, (int)progress,
				   m_userData);
	if (ret < 0)
		return ret;

	return 0;
}

int GamePatchPromoter::MountCardPatch()
{
	if (!m_move)
		return 0;

	float part = 0.5f;

	if (m_install)
		part = 0.225f;

	GameJobProgress *progress = new GameJobProgress(m_progress, part);

	if (progress == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_mountPoint.length() != 0)
		gameAppUnmount(&m_mountPoint);

	int ret = CopyCardPatch(progress);

	if (m_install)
		progress->Add(part * 100.0f);

	delete progress;

	if (ret < 0)
		return ret;

	string appPath = string("gro0:app/") + m_name;
	string drive("");

	drive = "";
	if (m_drive == GAME_PATCH_DRIVE_GRW0)
		drive = "grw0:";
	else if (m_drive == GAME_PATCH_DRIVE_UX0)
		drive = "ux0:";
	else
		return GAME_ERROR_INVALID_ARGUMENT;

	string promotePath = drive + string("temp/promote") + string("/") + m_name;

	ret = gameGameDataMount(appPath.c_str(), promotePath.c_str(), NULL, &m_mountPoint);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The patch's param.sfo read into info. A vs0: application's is read where it is; a game card's
 * application gets its patch folder by its attribute (ux0: or grw0:), or the card's own patch
 * when that is newer; then the application is mounted with its patch, and the sfo read from the
 * mount (0x81042EA0).
 */
int GamePatchPromoter::ReadSfo(LsdbAppInfoPtr *info)
{
	string patchPath;
	string sfoPath;
	string::size_type pos = m_path.find("app/");
	int ret;

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	if (m_path.find("vs0:") != string::npos) {
		sfoPath = string("vs0:app/") + m_name + string("/sce_sys/param.sfo");
	} else {
		if (m_path.find("gro0:app/") != string::npos) {
			sfoPath = m_path + string("/sce_sys/param.sfo");

			ret = gameSfoLoad(string(sfoPath.c_str()), gameSfoGetAppVerParams(),
					  gameSfoGetAppVerParamCount(), info);
			if (ret < 0) {
				gamePathGetMountedDrive(sfoPath, (string *)m_userData);
				return ret;
			}

			SceUInt64 attribute = 0;
			LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);

			if (it != (*info)->end())
				attribute = gameLsdbValueInteger(it->second);

			if (attribute & 1) {
				m_drive = GAME_PATCH_DRIVE_UX0;
				patchPath = string("ux0:patch/") + m_name;
			} else {
				m_drive = GAME_PATCH_DRIVE_GRW0;
				patchPath = string("grw0:patch/") + m_name;
			}

			if (gameGcPatchExists(m_name) && m_drive == GAME_PATCH_DRIVE_UX0) {
				LocalFileStat stat;

				if (LocalFile::Getstat("ux0:", &stat) < 0) {
					gamePathGetMountedDrive(string("ux0:"), (string *)m_userData);
					return GAME_ERROR_NO_UX0;
				}
			}

			if (gameAppIsUpdateNeeded(&m_info, m_name, &m_drive) && !g_gameRepromoting) {
				m_move = true;
				patchPath = string("gro0:patch/") + m_name;
			}

			(*info)->clear();
		} else if (m_path.find("pd0:app/") != string::npos) {
			patchPath.assign(NULL, 0);
		} else {
			string path(m_path);

			path.replace(pos, 4, "patch/", 0, 6);
			patchPath = path;
		}

		if (patchPath.length() != 0 && Dir::IsExist(patchPath.c_str()))
			ret = gameGameDataMount(m_path.c_str(), patchPath.c_str(), NULL,
						&m_mountPoint);
		else
			ret = gameGameDataMount(m_path.c_str(), NULL, NULL, &m_mountPoint);

		if (ret < 0)
			return ret;

		sfoPath = m_mountPoint + string("/sce_sys/param.sfo");
	}

	ret = gameSfoLoad(string(sfoPath.c_str()), gameSfoGetCheckParams(),
			  gameSfoGetCheckParamCount(), info);
	if (ret < 0) {
		gamePathGetMountedDrive(sfoPath, (string *)m_userData);
		return ret;
	}

	return 0;
}

/*
 * A game card application's record brought up to date when its patch was already in place: the
 * #_promote_attr's manual bit, the #_up_status from the card's license, the LiveArea in
 * ur0:appmeta and the appinst bit, then the record registered again (0x81042154).
 */
int GamePatchPromoter::CheckInstalled(LsdbAppInfoPtr *installed, bool flag)
{
	if (m_path.find("gro0:") == string::npos)
		return 0;

	string manualPath = m_mountPoint + string("/sce_sys/manual/001.png");
	LocalFileStat stat;

	if (LocalFile::Getstat(manualPath.c_str(), &stat) == 0) {
		SceUInt32 promoteAttr = 0;
		LsdbAppInfo::iterator it = (*installed)->find(LSDB_KEY_PROMOTE_ATTR);

		if (it != (*installed)->end())
			promoteAttr = (SceUInt32)gameLsdbValueInteger(it->second);
		gameLsdbValueSetInteger((**installed)[LSDB_KEY_PROMOTE_ATTR], promoteAttr | 2);
	}

	string licensePath = string("gro0:license/app/") + m_name;
	string appmetaPath = string("ur0:appmeta/") + m_name;
	bool noLicense = false;
	int ret = gameAppInfoSetUpStatus(installed, &licensePath, &appmetaPath, m_userData,
					 &noLicense);

	if (ret < 0)
		return ret;

	bool hasAppmeta = false;

	ret = gameAppInfoCheckAppmeta(installed, &hasAppmeta, m_userData);
	if (ret < 0)
		return ret;

	gameAppInfoSetAppconfBit10(installed, gameDirHasSubDirs(string("gro0:appinst")));

	GameAppRegistration reg;

	if (reg.entry.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	reg.info = *installed;
	reg.liveAreaState = 0;

	if (hasAppmeta) {
		string liveAreaPath = string("ur0:appmeta/") + m_name + string("/livearea/contents");

		gameLsdbValueSetText((**installed)[LSDB_KEY_LIVEAREA_PATH], liveAreaPath);
	}

	bool replace = false;

	ret = gameAppRegister(&reg, 1, hasAppmeta, true, &replace);
	if (ret < 0)
		return ret;

	ret = gameAppUpdate(&reg.info, replace, true);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The folder made ready: the installed application's record read, the patch's param.sfo read,
 * and when an application is installed the patch checked against it; then the game card's
 * patch copied and mounted when m_move is set (0x81043B1C).
 */
int GamePatchPromoter::Prepare(bool flag)
{
	if (m_info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = gameAppGetInfo(m_name.c_str(), &m_info);

	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = ReadSfo(&info);
	if (ret < 0)
		return ret;

	if (m_move) {
		m_weights[1] = 0.2f;
		m_weights[0] = 0.5f;
		m_weights[3] = 0.05f;
		m_weights[2] = 0.25f;
	}

	if (m_info->size() != 0) {
		m_install = true;

		GameJobProgress *progress = NULL;

		if (m_move) {
			progress = new GameJobProgress(m_progress, 0.15f);
			if (progress == NULL)
				return GAME_ERROR_NO_INFO_MEMORY;
		}

		GameUpdateChecker *checker = new GameUpdateChecker(m_path, m_flag1, true,
			progress != NULL ? progress : &m_progress, (string *)m_userData);

		if (checker == NULL) {
			delete progress;
			return GAME_ERROR_NO_INFO_MEMORY;
		}

		if (m_sfo->get() != NULL && (*m_sfo)->size() != 0) {
			ret = gameAppInfoCopyParental(m_sfo, &info);
			if (ret < 0) {
				delete progress;
				delete checker;
				return ret;
			}
		}

		ret = checker->Check(&m_info, &info);
		if (ret < 0) {
			delete progress;
			delete checker;

			if (ret != GAME_ERROR_GC_QUIET)
				return ret;

			int err = ret;

			ret = CheckInstalled(&m_info, flag);
			if (ret < 0)
				return ret;

			if (!m_move)
				return err;

			ret = MountCardPatch();
			if (ret < 0)
				return ret;

			m_param.updateApp = true;

			return 0;
		}

		if (progress != NULL) {
			progress->Add(100.0f);
			delete progress;
		}

		delete checker;
	} else {
		m_install = false;
	}

	ret = MountCardPatch();
	if (ret < 0)
		return ret;

	m_param.updateApp = true;

	return 0;
}

/*
 * The source of the promoted application's information: the folder read is the mount point when
 * there is one; the LiveArea's folder and the license folder are set by the drive the
 * application is on (0x8104415A).
 */
int GamePatchPromoter::CreateSource(GamePromoteSource **source)
{
	string dir(m_path);

	if (m_mountPoint.length() != 0)
		dir = m_mountPoint;

	string sysDir = dir + string("/sce_sys");
	string sfoPath = sysDir + string("/param.sfo");

	if (m_path.find("gro0:") != string::npos) {
		LsdbAppInfoPtr info(new LsdbAppInfo);

		if (info.get() == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		int ret = gameSfoLoad(sfoPath, gameSfoGetAppVerParams(),
				      gameSfoGetAppVerParamCount(), &info);
		if (ret < 0) {
			gamePathGetMountedDrive(sfoPath, (string *)m_userData);
			return ret;
		}

		SceUInt64 attribute = 0;
		LsdbAppInfo::iterator it = info->find(LSDB_KEY_ATTRIBUTE);

		if (it != info->end())
			attribute = gameLsdbValueInteger(it->second);

		if (attribute & 1) {
			m_appDir = m_path;
			m_dir = string("ur0:appmeta/") + m_name;
		} else {
			m_appDir = m_path;
			m_dir = string("ur0:appmeta/") + m_name;
		}

		m_licensePath = string("gro0:license/app/") + m_name;
	} else if (m_path.find("pd0:") != string::npos) {
		m_appDir = m_path;
		m_dir = string("ur0:appmeta/") + m_name;
		m_licensePath = string("pd0:license/app/") + m_name;
	} else if (m_path.find("vs0:") != string::npos) {
		m_appDir = m_path;
		m_dir = string("vs0:app/") + m_name;
	} else if (m_path.find("ux0:") != string::npos) {
		m_appDir = dir;
		m_dir = string("ur0:appmeta/") + m_name;
		m_licensePath = string("ux0:license/app/") + m_name;
	}

	GameAppSourceParam param(dir.c_str(), m_path.c_str(), m_name.c_str(), sysDir.c_str(),
				 m_dir.c_str(), m_info);

	*source = new GameAppSource(param);
	if (*source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

/*
 * The record built by the promoter, the kept values of the installed record added for an update;
 * a game card's game gets the appinst bit, and an application without a LiveArea type whose
 * template.xml is missing the bit that says so (0x8104531A).
 */
int GamePatchPromoter::SetInfo(GamePromoteSource *source, bool registerApp,
			       GameAppRegistration *reg)
{
	int ret = GamePromoter::SetInfo(source, registerApp, reg);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = reg->info->end();

	if (m_install && m_param.updateApp) {
		ret = gameAppInfoCopyKept(&m_info, &reg->info);
		if (ret < 0)
			return ret;
	}

	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));

	if (category == "gd" || category == "gp") {
		if (LocalFile::Exists("gro0:")) {
			string titleId = gameCardGetTitleId();

			if (titleId == m_name.c_str()) {
				bool appinst = gameDirHasSubDirs(string("gro0:appinst"));

				gameAppInfoSetAppconfBit10(&reg->info, appinst);
			}
		}
	}

	it = reg->info->find(LSDB_KEY_LAREA_TYPE);
	if (it == end || (int)gameLsdbValueInteger(it->second) == 0) {
		int upStatus = 0;

		it = reg->info->find(LSDB_KEY_UP_STATUS);
		if (it != end)
			upStatus = (int)gameLsdbValueInteger(it->second);

		string templatePath;

		if (upStatus == 3) {
			templatePath = m_path +
				       string("/sce_sys/retail/livearea/contents/template.xml");
			if (!LocalFile::Exists(templatePath.c_str()))
				templatePath = m_path +
					       string("/sce_sys/livearea/contents/template.xml");
		} else {
			templatePath = m_path + string("/sce_sys/livearea/contents/template.xml");
		}

		if (!LocalFile::Exists(templatePath.c_str()))
			gameAppInfoSetAppconfBit12(&reg->info, true);
	}

	return 0;
}

/*
 * What is left once the record is built: the parental values an update keeps, the EBOOT
 * parameters of eboot.bin and of the first related application's eboot2.bin, and the LiveArea
 * brought into ur0:appmeta when m_flag2 is set (0x810458A0).
 */
int GamePatchPromoter::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret;

	if (m_sfo->get() != NULL && (*m_sfo)->size() != 0) {
		ret = gameAppInfoCopyParental(m_sfo, &reg->info);
		if (ret < 0)
			return ret;
	}

	string dir(m_path);

	if (m_mountPoint.length() != 0)
		dir = m_mountPoint;

	string ebootPath = dir + string("/eboot.bin");

	gameAppInfoUpdateEbootParam(ebootPath, &reg->info, NULL);

	LocalFileStat stat;
	char name[32];

	for (unsigned int i = 0; i < reg->related.size() && i < 1; i++) {
		sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
		ebootPath = dir + string(name);

		if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0)
			gameAppInfoUpdateEbootParam(ebootPath, &reg->related[i], NULL);
	}

	if (m_flag2) {
		LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

		if (it == reg->info->end())
			return GAME_ERROR_NO_VALUE;

		string category(LsdbValueText(it->second));
		GameLiveAreaCopier *copier = new GameLiveAreaCopier(false, string(""), cancel, arg,
								    &m_progress, 0, 0.4f);

		if (copier == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = copier->Run(reg, &dir);
		delete copier;
		if (ret < 0)
			return ret;
	}

	return 0;
}
