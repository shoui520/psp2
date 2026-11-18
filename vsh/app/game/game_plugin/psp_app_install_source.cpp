/*
 * The game plugin's source of a PSP game's package: with the pspemu work folder mounted, the
 * games' old folders and licenses under ux0:pspemu moved to ux0:pspemu/temp/promote (or, for a
 * package without its game data, the game data on ux0 moved into the package), then once
 * installed the package's moved into ux0:pspemu (0x81085DF0..0x81087580). The file's name and
 * the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePspAppInstallSource::GamePspAppInstallSource(const GameInstallParam *param,
						 const vector<string> *titleIds,
						 const string *gameDataId, string *errorTitleId)
	: m_param(param), m_titleIds(titleIds), m_gameDataId(gameDataId),
	  m_errorTitleId(errorTitleId)
{
}

GamePspAppInstallSource::~GamePspAppInstallSource()
{
	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	gamePspemuWorkUnmount(&m_mountPoint);
}

/*
 * The entries of fromDir of the given type that path also holds moved to toDir, which is made
 * when it is not there (0x81085EDE).
 */
int GamePspAppInstallSource::MoveOld(const string *path, const string &fromDir,
				     const string &toDir, int type, string *detail)
{
	int ret;

	if (!Dir::IsExist(toDir.c_str())) {
		ret = Dir::CreateRecursive(toDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(toDir, detail);
			return ret;
		}
	}

	Dir dir;

	ret = dir.Open(path->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*path, detail);
		return ret;
	}

	DirEnt entry;

	do {
		ret = dir.Read(&entry);
		if (ret != 0)
			break;

		if (entry.name[0] != '.' && entry.type == type) {
			string from = fromDir + string("/") + entry.name;
			string to = toDir + string("/") + entry.name;

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
				gamePathGetMountedDrive(to, detail);
				return ret;
			}
		}
	} while (ret == 0);

	dir.Close();
	return 0;
}

int GamePspAppInstallSource::Install(const string *path, const string *titleId, string *dir,
				     string *installPath, GameJobProgress *progress, int reserved1,
				     int reserved2, float scale)
{
	int ret;

	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);

	ret = gamePspemuWorkMount(&m_mountPoint);
	if (ret < 0)
		return ret;

	string gameDir = *path + string("/PSP/GAME");

	ret = MoveOld(&gameDir, string("ux0:pspemu/PSP/GAME"),
		      string("ux0:pspemu/temp/promote/PSP/GAME"), DirEnt::Type_Dir, m_errorTitleId);
	if (ret < 0)
		return ret;

	string licenseDir = *path + string("/PSP/LICENSE");

	if (Dir::IsExist(licenseDir.c_str())) {
		ret = MoveOld(&licenseDir, string("ux0:pspemu/PSP/LICENSE"),
			      string("ux0:pspemu/temp/promote/PSP/LICENSE"), DirEnt::Type_File,
			      m_errorTitleId);
		if (ret < 0)
			return ret;
	}

	if (m_gameDataId->length() != 0) {
		string gameDataDir = *path + string("/PSP/GAME/") + *m_gameDataId;
		LocalFileStat stat;

		if (LocalFile::Getstat(gameDataDir.c_str(), &stat) == 0) {
			/* the games sharing it come back into the package */
			for (unsigned int i = 0; i < m_titleIds->size(); i++) {
				string from = string("ux0:pspemu/PSP/GAME/") + (*m_titleIds)[i];
				string to = *path + string("/PSP/GAME/") + (*m_titleIds)[i];

				ret = LocalFile::RenameFile(from.c_str(), to.c_str());
				if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
					gamePathGetMountedDrive(to, m_errorTitleId);
					return ret;
				}
			}
		} else {
			/* the package lacks its game data: the one on ux0 goes with it */
			string from = string("ux0:pspemu/PSP/GAME/") + *m_gameDataId;
			LocalFileStat fromStat;

			if (LocalFile::Getstat(from.c_str(), &fromStat) == 0) {
				string to = *path + string("/PSP/GAME/") + *m_gameDataId;

				ret = LocalFile::RenameFile(from.c_str(), to.c_str());
				if (ret < 0) {
					gamePathGetMountedDrive(to, m_errorTitleId);
					return ret;
				}
			}
		}
	}

	*dir = *path;
	*installPath = "ux0:";
	return 0;
}

/* the entries of fromDir of the given type moved to toDir, which is made when it is not there
 * (0x81086D06) */
int GamePspAppInstallSource::MoveNew(const string &fromDir, const string &toDir, int type,
				     string *detail)
{
	int ret;

	if (!Dir::IsExist(toDir.c_str())) {
		ret = Dir::CreateRecursive(toDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(toDir, detail);
			return ret;
		}
	}

	Dir dir;

	ret = dir.Open(fromDir.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(fromDir, detail);
		return ret;
	}

	DirEnt entry;

	do {
		ret = dir.Read(&entry);
		if (ret != 0)
			break;

		if (entry.name[0] != '.' && entry.type == type) {
			string from = fromDir + string("/") + entry.name;
			string to = toDir + string("/") + entry.name;

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
				gamePathGetMountedDrive(to, detail);
				return ret;
			}
		}
	} while (ret == 0);

	dir.Close();
	return 0;
}

int GamePspAppInstallSource::Complete(string *dir, string *installPath, int reserved1,
				      int reserved2, int reserved3, float scale)
{
	int ret;

	gamePspemuWorkUnmount(&m_mountPoint);

	string gameDir = *dir + string("/PSP/GAME");
	string gameTo = *installPath + string("pspemu/PSP/GAME");

	ret = MoveNew(gameDir, gameTo, DirEnt::Type_Dir, m_errorTitleId);
	if (ret < 0)
		return ret;

	string licenseDir = *dir + string("/PSP/LICENSE");

	if (Dir::IsExist(licenseDir.c_str())) {
		string licenseTo = *installPath + string("pspemu/PSP/LICENSE");

		ret = MoveNew(licenseDir, licenseTo, DirEnt::Type_File, m_errorTitleId);
		if (ret < 0)
			return ret;
	}

	gameRemoveDirectory(dir->c_str(), NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:pspemu/temp/promote", NULL, NULL, NULL, true);
	return 0;
}
