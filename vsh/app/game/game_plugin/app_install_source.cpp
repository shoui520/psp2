/*
 * The game plugin's source of an application's package: its appmeta moved to ux0:appmeta/new and
 * its application data mounted while it installs; once installed, the application's old app,
 * patch, additional content and license folders, saved data and appmeta are moved aside, the
 * package's are moved into place, and its appmeta renamed to the application's
 * (0x81081F28..0x81084C9E). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
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

/* the folders of an application moved as a whole, under ux0: and the package */
static const char *const s_appDirs[] = {
	"app",
	"patch",
	"addcont",
	"license/app",
	"license/addcont",
};

GameAppInstallSource::GameAppInstallSource(const GameInstallParam *param, string *errorTitleId)
	: m_param(param), m_oldDir("ur0:temp/promote/old"), m_errorTitleId(errorTitleId),
	  m_freeTrial(false), m_hasFttLicense(false)
{
}

GameAppInstallSource::~GameAppInstallSource()
{
	gameAppDataUnmount(&m_mountPoint);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
}

int GameAppInstallSource::Install(const string *path, const string *titleId, string *dir,
				  string *installPath, GameJobProgress *progress, int reserved1,
				  int reserved2, float scale)
{
	int ret;

	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:appmeta/new", NULL, NULL, NULL, true);
	Dir::CreateRecursive("ux0:appmeta", 6);

	string from = string(m_param->path) + string("/appmeta/") + *titleId;
	string to("ux0:appmeta/new");

	ret = LocalFile::RenameFile(from.c_str(), to.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(to, m_errorTitleId);
		return ret;
	}

	ret = gameAppDataMount(string("new"), &m_mountPoint, true);
	if (ret < 0)
		return ret;

	*dir = *path;
	*installPath = "ux0:";
	return 0;
}

/* the names of the folders in path, none when it is not there (0x810823CE) */
int GameAppInstallSource::ListDirs(const string &path, vector<string> *names)
{
	Dir dir;
	DirEnt entry;
	int ret;

	if (!Dir::IsExist(path.c_str()))
		return 0;

	ret = dir.Open(path.c_str());
	if (ret < 0)
		goto out;

	for (;;) {
		ret = dir.Read(&entry);
		if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
			break;
		if (ret < 0)
			goto out;

		if (entry.type == DirEnt::Type_Dir && entry.name != "." && entry.name != "..")
			names->push_back(entry.name);
	}

	ret = 0;
out:
	dir.Close();
	return ret;
}

/*
 * The package's folders moved into place under ux0: (its free trial license activated first
 * when the old license had one), then its saved data into the user's (0x81082518).
 */
int GameAppInstallSource::MoveNew(const string &path, const string &titleId,
				  const string &userDir, string *detail)
{
	const char *const *name = s_appDirs;
	string from;
	string dir;
	string to;
	unsigned int i;
	int ret;

	for (i = 0; i < sizeof(s_appDirs) / sizeof(s_appDirs[0]); i++, name++) {
		from = path + string("/") + string(*name) + string("/") + titleId;
		dir = string("ux0:") + string(*name);

		if (!Dir::IsExist(from.c_str()))
			continue;

		bool isLicense = string(*name) == string("license/app");

		if (isLicense && m_freeTrial && m_hasFttLicense) {
			string ftt;

			ret = gameNpDrmGetFttName(&ftt);
			if (ret < 0)
				return ret;

			string license = from + string("/") + ftt;

			ret = gameLicenseActivate(&license);
			if (ret < 0)
				return ret;
		}

		if (!Dir::IsExist(dir.c_str())) {
			ret = Dir::CreateRecursive(dir.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(dir, detail);
				return ret;
			}
		}

		to = dir + string("/") + titleId;

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			gamePathGetMountedDrive(to, detail);
			return ret;
		}
	}

	vector<string> names;
	string savedataPath = path + string("/savedata");

	ret = ListDirs(savedataPath, &names);
	if (ret < 0)
		return ret;

	if (names.size() != 0) {
		dir = userDir + string("/savedata");

		if (!Dir::IsExist(dir.c_str())) {
			ret = Dir::CreateRecursive(dir.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(dir, detail);
				return ret;
			}
		}

		for (i = 0; i < names.size(); i++) {
			from = path + string("/savedata/") + names[i];
			to = dir + string("/") + names[i];

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0) {
				gamePathGetMountedDrive(to, detail);
				return ret;
			}
		}
	}

	return 0;
}

/*
 * The application's old folders under ux0: moved to uxOldDir (whether its license folder holds
 * the free trial license noted), the saved data the package replaces moved there from the
 * user's, and its appmeta (0x8108345C).
 */
int GameAppInstallSource::MoveOld(const string &oldDir, const string &uxOldDir,
				  const string &path, const string &titleId,
				  const string &userDir, string *detail)
{
	unsigned int i;
	int ret;

	if (!Dir::IsExist(oldDir.c_str())) {
		ret = Dir::CreateRecursive(oldDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(oldDir, detail);
			return ret;
		}
	}

	if (!Dir::IsExist(uxOldDir.c_str())) {
		ret = Dir::CreateRecursive(uxOldDir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(uxOldDir, detail);
			return ret;
		}
	}

	const char *const *name = s_appDirs;
	string from;
	string dir;
	string to;

	i = 0;
	do {
		from = string("ux0:") + string(*name) + string("/") + titleId;
		dir = uxOldDir + string("/") + string(*name);

		if (Dir::IsExist(from.c_str())) {
			bool isLicense = string(*name) == string("license/app");

			if (isLicense && m_freeTrial) {
				string rif;

				ret = gameNpDrmGetRifName(&rif);
				if (ret < 0)
					return ret;

				string rifPath = from + string("/") + rif;

				if (LocalFile::Exists(rifPath.c_str()))
					m_hasFttLicense = true;
			}

			if (!Dir::IsExist(dir.c_str())) {
				ret = Dir::CreateRecursive(dir.c_str(), 6);
				if (ret < 0) {
					gamePathGetMountedDrive(dir, detail);
					return ret;
				}
			}

			to = dir + string("/") + titleId;

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0) {
				gamePathGetMountedDrive(to, detail);
				return ret;
			}
		}

		i++;
		name++;
	} while (i < sizeof(s_appDirs) / sizeof(s_appDirs[0]));

	vector<string> names;
	string savedataPath = path + string("/savedata");

	ret = ListDirs(savedataPath, &names);
	if (ret < 0)
		return ret;

	if (names.size() != 0) {
		dir = uxOldDir + string("/savedata");

		if (!Dir::IsExist(dir.c_str())) {
			ret = Dir::CreateRecursive(dir.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(dir, detail);
				return ret;
			}
		}

		for (i = 0; i < names.size(); i++) {
			from = userDir + string("/savedata/") + names[i];
			to = dir + string("/") + names[i];

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
				gamePathGetMountedDrive(from, detail);
				return ret;
			}
		}
	}

	from = string("ux0:appmeta/") + titleId;
	dir = uxOldDir + string("/appmeta");

	if (!Dir::IsExist(dir.c_str())) {
		ret = Dir::CreateRecursive(dir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(dir, detail);
			return ret;
		}
	}

	to = dir + string("/") + titleId;

	ret = LocalFile::RenameFile(from.c_str(), to.c_str());
	if (ret >= 0 || ret == GAME_PAF_ERROR_NOT_FOUND)
		return 0;

	gamePathGetMountedDrive(to, detail);
	return ret;
}

int GameAppInstallSource::Complete(string *dir, string *installPath, int reserved1,
				   int reserved2, int reserved3, float scale)
{
	string titleId(m_param->titleId);
	string appmetaPath;
	char userDir[32];
	int ret;

	ret = gameAppDataUnmount(&m_mountPoint);
	if (ret < 0)
		return ret;

	ret = sceAppMgrGetUserDirPath(GAME_USER_DIR_PARTITION, userDir, sizeof(userDir));
	if (ret < 0)
		return ret;

	ret = MoveOld(m_oldDir, string("ux0:temp/promote/old"), string(m_param->path), titleId,
		      string(userDir), m_errorTitleId);
	if (ret < 0)
		return ret;

	ret = MoveNew(string(m_param->path), titleId, string(userDir), m_errorTitleId);
	if (ret < 0)
		return ret;

	appmetaPath = string("ux0:appmeta/") + titleId;

	ret = LocalFile::RenameFile("ux0:appmeta/new", appmetaPath.c_str());
	if (ret >= 0 || ret == GAME_PAF_ERROR_NOT_FOUND) {
		gameRemoveDirectory(m_param->path, NULL, NULL, NULL, true);
		gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
		gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
		return 0;
	}

	gamePathGetMountedDrive(appmetaPath, m_errorTitleId);
	return ret;
}

void GameAppInstallSource::SetFreeTrial(bool freeTrial)
{
	m_freeTrial = freeTrial;
}
