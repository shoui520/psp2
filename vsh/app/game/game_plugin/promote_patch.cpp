/*
 * The game plugin's patch promoter: an application's patch, promoted in the drive's temp/promote,
 * moved into its patch folder (0x8103FB34..0x8104639C), and the checks of the game card's patch
 * against the installed one. The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
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
			progress != NULL ? progress : &m_progress, m_userData);

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
