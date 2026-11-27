/*
 * The game plugin's source of a PSP game's update package with PBOOT.PBP: with the pspemu work
 * folder mounted, the games it updates moved to ux0:pspemu/temp/promote/pspemu/PSP/GAME, the
 * package's content moved into the game's folder there, then the games moved back to
 * ux0:pspemu/PSP/GAME (0x8109367C..0x81094868). The file's name and the functions' are ours (no
 * record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePspPbootInstallSource::GamePspPbootInstallSource(const SceNpDrmInstallInfo *info,
						     const string &titleId,
						     const vector<string> &titleIds,
						     string *errorTitleId)
	: m_info(info), m_titleId(titleId), m_titleIds(titleIds), m_errorTitleId(errorTitleId)
{
}

GamePspPbootInstallSource::~GamePspPbootInstallSource()
{
	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/appmeta", NULL, NULL, NULL, true);
	gamePspemuWorkUnmount(&m_workDir);
}

/*
 * The package's content moved into gameDir/titleId, once its PBOOT.PBP reads or is missing
 * (0x810937A4).
 */
int GamePspPbootInstallSource::MoveContent(const string *content, const string *gameDir,
					   const string *titleId, string *detail)
{
	int ret;

	if (!Dir::IsExist(gameDir->c_str())) {
		ret = Dir::CreateRecursive(gameDir->c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(*gameDir, detail);
			return ret;
		}
	}

	string pboot = *content + string("/PBOOT.PBP");
	LocalFileStat stat;

	ret = LocalFile::Getstat(pboot.c_str(), &stat);
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gamePathGetMountedDrive(pboot, detail);
		return ret;
	}

	string to = *gameDir + string("/") + *titleId;

	ret = gameMoveTree(content->c_str(), to.c_str(), NULL, NULL, detail);
	if (ret < 0)
		return ret;

	return 0;
}

int GamePspPbootInstallSource::Install(const string *path, const string *titleId, string *dir,
				       string *installPath, GameJobProgress *progress,
				       int cancel, int cancelArg, float scale)
{
	int ret;
	unsigned int i;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	string pboot = *path + string("/USRDIR/CONTENT/PBOOT.PBP");
	LocalFileStat stat;

	if (LocalFile::Getstat(pboot.c_str(), &stat) < 0) {
		gamePathGetMountedDrive(pboot, m_errorTitleId);
		return GAME_ERROR_INVALID_ARGUMENT;
	}

	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	if (!Dir::IsExist("ux0:pspemu/temp/promote")) {
		ret = Dir::CreateRecursive("ux0:pspemu/temp/promote/pspemu/PSP/GAME", 6);
		if (ret < 0) {
			gamePathGetMountedDrive(string("ux0:pspemu/temp/promote/pspemu/PSP/GAME"),
						m_errorTitleId);
			return ret;
		}
	}

	ret = gamePspemuWorkMount(&m_workDir);
	if (ret < 0)
		return ret;

	/* the games the update moves; one that does not move is noted and left */
	for (i = 0; i < m_titleIds.size(); i++) {
		string from = string("ux0:pspemu/PSP/GAME/") + m_titleIds[i];
		string to = string("ux0:pspemu/temp/promote/pspemu/PSP/GAME/") + m_titleIds[i];

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
			gamePathGetMountedDrive(to, m_errorTitleId);
	}

	string newDir("ux0:pspemu/temp/promote/new");

	ret = LocalFile::RenameFile(path->c_str(), newDir.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newDir, m_errorTitleId);
		return ret;
	}

	string content = newDir + string("/USRDIR/CONTENT");
	string gameDir("ux0:pspemu/temp/promote/pspemu/PSP/GAME");

	ret = MoveContent(&content, &gameDir, titleId, m_errorTitleId);
	if (ret < 0)
		return ret;

	*dir = string("ux0:pspemu/temp/promote/pspemu/PSP/GAME");
	*installPath = string("ux0:pspemu/PSP/GAME");
	return ret;
}

int GamePspPbootInstallSource::Complete(string *dir, string *installPath, int progress,
					int cancel, int cancelArg, float scale)
{
	int ret;
	unsigned int i;

	if (!Dir::IsExist(installPath->c_str())) {
		ret = Dir::CreateRecursive(installPath->c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(*installPath, m_errorTitleId);
			return ret;
		}
	}

	for (i = 0; i < m_titleIds.size(); i++) {
		string from = *dir + string("/") + m_titleIds[i];
		string to = *installPath + string("/") + m_titleIds[i];

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			gamePathGetMountedDrive(*installPath, m_errorTitleId);
			return ret;
		}
	}

	string from = *dir + string("/") + string(m_info->info.titleId);
	string to = *installPath + string("/") + string(m_info->info.titleId);

	ret = LocalFile::RenameFile(from.c_str(), to.c_str());
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gamePathGetMountedDrive(*installPath, m_errorTitleId);
		return ret;
	}

	gamePspemuWorkUnmount(&m_workDir);
	return 0;
}
