/*
 * The game plugin's source of an application's update package: checked against the installed
 * application and its patch, then installed to <drive>patch/<title ID>, a patch that applies on
 * the installed one merged into it in temp/promote first, a game card's from ux0: copied to
 * grw0: (0x8108A81C..0x8108CD5C). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "install_cleanup.h"
#include "promoter.h"

using namespace paf;

/* the work memory gamePatchCheck reads the packages' headers with */
#define GAME_PATCH_CHECK_BUF_SIZE	0x8000

/* what gamePatchCheck says of a patch: it applies on the application, or on the installed patch */
#define GAME_PATCH_KIND_APP		1
#define GAME_PATCH_KIND_PATCH		2

GameGdUpdateSource::GameGdUpdateSource(const SceNpDrmInstallInfo *info,
				       const LsdbAppInfoPtr *appInfo, string *errorTitleId)
	: m_info(info), m_appInfo(*appInfo), m_mode(1), m_errorTitleId(errorTitleId)
{
	gameRemoveDirectory("ux0:tem", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:tem", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
}

GameGdUpdateSource::~GameGdUpdateSource()
{
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/appmeta", NULL, NULL, NULL, true);
}

/*
 * The installed patch at patchDir moved into dir as <dir>/<title ID> and the update at path moved
 * over it, dir left naming the result (0x8108A9D6).
 */
int GameGdUpdateSource::MergePatch(const string *appDir, const string *patchDir,
				   const string *path, string *dir)
{
	int ret;

	Dir::CreateRecursive(dir->c_str(), 6);

	string to = *dir + string("/") + string(m_info->info.titleId);

	ret = LocalFile::RenameFile(patchDir->c_str(), to.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*patchDir, m_errorTitleId);
		return ret;
	}

	ret = gameMoveTree(path->c_str(), to.c_str(), NULL, NULL, m_errorTitleId);
	if (ret < 0)
		return ret;

	*dir = to;
	return 0;
}

/*
 * An update on the application's drive: the title's additional content cleaned up, then the
 * update installed from path as it is, or merged into the installed patch in promoteDir
 * (0x8108AC46).
 */
int GameGdUpdateSource::MovePatch(const string *appDir, const string *patchDir,
				  const string *path, const string &promoteDir, int kind,
				  int cancel, int cancelArg)
{
	int ret;
	string addcontDir = string("ux0:addcont/") + string(m_info->info.titleId);

	ret = gameCleanupApp(addcontDir.c_str(), &m_appInfo, m_errorTitleId, NULL);
	if (ret < 0)
		return ret;

	if (kind == GAME_PATCH_KIND_APP) {
		m_installDir = *path;
	} else if (kind == GAME_PATCH_KIND_PATCH) {
		string dir(promoteDir);

		ret = MergePatch(appDir, patchDir, path, &dir);
		if (ret < 0)
			return ret;

		m_installDir = dir;
	} else {
		return GAME_ERROR_NOT_APP_PACKAGE;
	}

	return 0;
}

/*
 * The update at path checked against the application at appDir and the patch installed at
 * patchDir (when it is there): what it applies on left in kind (0x8108AE9E).
 */
static __attribute__((noinline)) int gameGdUpdateCheckPatch(const string *appDir,
							     const string *patchDir,
							     const string *path, int *kind,
							     string *detail)
{
	char *buf = new char[GAME_PATCH_CHECK_BUF_SIZE];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string app = *appDir + string("/");
	string base;
	string patch = *path + string("/");
	const char *basePath = NULL;

	if (Dir::IsExist(patchDir->c_str())) {
		base = *patchDir + string("/");
		basePath = base.c_str();
	}

	int step = 0;
	int ret = gamePatchCheck(app.c_str(), basePath, patch.c_str(), 0, buf,
				 GAME_PATCH_CHECK_BUF_SIZE, &step);

	if (ret < 0) {
		delete[] buf;

		if (step == 1)
			gamePathGetMountedDrive(*appDir, detail);
		else if (step == 2)
			gamePathGetMountedDrive(string(basePath), detail);
		else if (step == 3)
			gamePathGetMountedDrive(patch, detail);

		return ret;
	}

	*kind = ret;
	delete[] buf;
	return 0;
}

/*
 * An update for a game card's application: from grw0: installed as it is or merged into the
 * installed patch in promoteDir; from ux0: the card's additional content cleaned up and the
 * update copied to promoteDir/cp first (0x8108B2C8).
 */
int GameGdUpdateSource::CopyCardPatch(const string *appDir, const string *patchDir,
				      const string *path, const string &promoteDir, int kind,
				      GameJobProgress *progress, int cancel, int cancelArg,
				      float scale)
{
	int ret;
	string patchDrive;

	ret = gamePathGetDrive(*patchDir, &patchDrive);
	if (ret < 0)
		return ret;

	string drive;

	ret = gamePathGetDrive(*path, &drive);
	if (ret < 0)
		return ret;

	string dir(promoteDir);

	if (drive.find("grw0:", 0, sce_paf_strlen("grw0:")) != string::npos) {
		if (kind == GAME_PATCH_KIND_APP) {
			m_installDir = *path;
		} else if (kind == GAME_PATCH_KIND_PATCH) {
			ret = MergePatch(appDir, patchDir, path, &dir);
			if (ret < 0)
				return ret;

			m_installDir = dir;
		} else {
			return GAME_ERROR_NOT_APP_PACKAGE;
		}
	} else if (drive.find("ux0:", 0, sce_paf_strlen("ux0:")) != string::npos) {
		string addcontDir = string("grw0:addcont/") + string(m_info->info.titleId);

		ret = gameCleanupApp(addcontDir.c_str(), &m_appInfo, m_errorTitleId, NULL);
		if (ret < 0)
			return ret;

		GameJobProgress part(*progress, scale);

		if (kind == GAME_PATCH_KIND_APP) {
			string to = promoteDir + string("/cp");

			ret = gameCopyFileWithPool(path->c_str(), to.c_str(), cancel, cancelArg,
						   (int)&part, (int)m_errorTitleId);
			if (ret < 0)
				return ret;

			m_installDir = to;
		} else if (kind == GAME_PATCH_KIND_PATCH) {
			string to = promoteDir + string("/cp");

			ret = gameCopyFileWithPool(path->c_str(), to.c_str(), cancel, cancelArg,
						   (int)&part, (int)m_errorTitleId);
			if (ret < 0)
				return ret;

			ret = MergePatch(appDir, patchDir, &to, &dir);
			if (ret < 0)
				return ret;

			m_installDir = dir;
		} else {
			return GAME_ERROR_NOT_APP_PACKAGE;
		}
	} else {
		return GAME_ERROR_NOT_APP_PACKAGE;
	}

	return 0;
}

int GameGdUpdateSource::Install(const string *path, const string *titleId, string *dir,
				string *installPath, GameJobProgress *progress, int cancel,
				int cancelArg, float scale)
{
	int ret;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (m_info->info.contentType != GAME_CONTENT_TYPE_GD &&
	    (m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE) != 0)
		return GAME_ERROR_INVALID_ARGUMENT;

	LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_TITLE_ID);

	if (it == m_appInfo->end())
		return GAME_ERROR_NO_VALUE;

	if (*titleId != LsdbValueText(it->second))
		return GAME_ERROR_NOT_INSTALLED;

	int storage = 0;

	ret = gameAppGetStorage(&m_appInfo, &storage);
	if (ret < 0)
		return ret;

	switch (storage) {
	case GAME_STORAGE_GRO0: {
		if (path->find("grw0:", 0, sce_paf_strlen("grw0:")) == string::npos &&
		    path->find("ux0:", 0, sce_paf_strlen("ux0:")) == string::npos)
			return GAME_ERROR_NO_DRIVE;

		string appDir = string("gro0:app/") + string(m_info->info.titleId);
		string patchDir = string("grw0:patch/") + string(m_info->info.titleId);

		*installPath = string("grw0:patch/") + *titleId;

		int kind = GAME_PATCH_KIND_APP;

		ret = gameGdUpdateCheckPatch(&appDir, &patchDir, path, &kind, m_errorTitleId);
		if (ret < 0)
			return ret;

		ret = CopyCardPatch(&appDir, &patchDir, path, string("grw0:temp/promote"), kind,
				    progress, cancel, cancelArg, scale);
		if (ret < 0)
			return ret;
		break;
	}
	case GAME_STORAGE_GRO0_OTHER: {
		string appDir = string("gro0:app/") + string(m_info->info.titleId);
		string patchDir = string("ux0:patch/") + string(m_info->info.titleId);

		*installPath = string("ux0:patch/") + *titleId;

		int kind = GAME_PATCH_KIND_APP;

		ret = gameGdUpdateCheckPatch(&appDir, &patchDir, path, &kind, m_errorTitleId);
		if (ret < 0)
			return ret;

		ret = MovePatch(&appDir, &patchDir, path, string("ux0:temp/promote"), kind, 0, 0);
		if (ret < 0)
			return ret;
		break;
	}
	case GAME_STORAGE_UX0: {
		string appDir = string("ux0:app/") + string(m_info->info.titleId);
		string patchDir = string("ux0:patch/") + string(m_info->info.titleId);

		*installPath = string("ux0:patch/") + *titleId;

		int kind = GAME_PATCH_KIND_APP;

		ret = gameGdUpdateCheckPatch(&appDir, &patchDir, path, &kind, m_errorTitleId);
		if (ret < 0)
			return ret;

		ret = MovePatch(&appDir, &patchDir, path, string("ux0:temp/promote"), kind, 0, 0);
		if (ret < 0)
			return ret;
		break;
	}
	default:
		return GAME_ERROR_BAD_STORAGE;
	}

	*dir = m_installDir;
	return 0;
}

int GameGdUpdateSource::Complete(string *dir, string *installPath, int progress, int cancel,
				 int cancelArg, float scale)
{
	int ret;

	if (dir->length() == 0) {
		gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
		return 0;
	}

	if (Dir::IsExist(installPath->c_str())) {
		/* the installed patch moves to <drive>temp/promote/old */
		string old;

		ret = gamePathGetDrive(*installPath, &old);
		if (ret < 0)
			return ret;

		old += "temp/promote";

		if (!Dir::IsExist(old.c_str())) {
			ret = Dir::CreateRecursive(old.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(old, m_errorTitleId);
				return ret;
			}
		}

		old += "/old";

		ret = LocalFile::RenameFile(installPath->c_str(), old.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(old, m_errorTitleId);
			return ret;
		}
	} else {
		string::size_type pos = installPath->rfind("/", string::npos, sce_paf_strlen("/"));

		if (pos == string::npos)
			return GAME_ERROR_NO_DRIVE;

		string parent = installPath->substr(0, pos);

		if (!Dir::IsExist(parent.c_str())) {
			ret = Dir::CreateRecursive(parent.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(parent, m_errorTitleId);
				return ret;
			}
		}
	}

	ret = LocalFile::RenameFile(dir->c_str(), installPath->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*installPath, m_errorTitleId);
		return ret;
	}

	return 0;
}
