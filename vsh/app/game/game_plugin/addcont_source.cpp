/*
 * The game plugin's source of an additional content's package folder: the package moved (or, on
 * a game card, copied) to <drive>temp/promote, its content put in
 * <drive>addcont/<title ID>/<entitlement ID> (through its PFS mount when the folder is new), its
 * license written to <drive>license/addcont, and, for a license with a period, the period noted
 * in <drive>addcont/info once installed (0x810875F0..0x8108A08A). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "install_cleanup.h"
#include "license_updater.h"
#include "promoter.h"

using namespace paf;

/* the storages gameTitleGetStorage tells: a game card, the memory card or the internal one */
#define GAME_ADDCONT_STORAGE_CARD	1
#define GAME_ADDCONT_STORAGE_UX0_LAST	3

GameAddcontSource::GameAddcontSource(const char *path, const SceNpDrmInstallInfo *info,
				     LsdbAppInfoPtr *appInfo, string *errorTitleId)
	: m_path(path), m_info(info), m_appInfo(appInfo), m_errorTitleId(errorTitleId),
	  m_infoDir(""), m_contentId(""), m_startTime(0), m_expireTime(0)
{
	gameRemoveDirectory("ux0:tem", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:tem", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/game", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/game", NULL, NULL, NULL, true);
}

GameAddcontSource::~GameAddcontSource()
{
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/game", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/game", NULL, NULL, NULL, true);
}

/* every entry of from moved into to (0x810877A8) */
static int gameAddcontMoveEntries(const string &from, const string &to, string *detail)
{
	Dir dir;
	int ret;

	ret = dir.Open(from.c_str());
	if (ret < 0)
		return ret;

	DirEnt entry;
	string src;
	string dst;

	do {
		ret = dir.Read(&entry);
		if (ret != 0)
			break;

		if (entry.name[0] != '.') {
			src = from + string("/") + entry.name;
			dst = to + string("/") + entry.name;

			ret = LocalFile::RenameFile(src.c_str(), dst.c_str());
			if (ret < 0) {
				dir.Close();
				gamePathGetMountedDrive(dst, detail);
				return ret;
			}
		}
	} while (ret == 0);

	dir.Close();
	return 0;
}

/* every entry of from but "." moved into to, an installed content's into the temporary folder
 * (0x81087B46) */
int GameAddcontSource::MoveToTemp(const string *from, const string *to, string *detail)
{
	Dir dir;
	int ret;

	ret = dir.Open(from->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*from, detail);
		return ret;
	}

	do {
		DirEnt entry;

		ret = dir.Read(&entry);
		if (ret != 0 || entry.name == ".")
			continue;

		string src = *from + string("/") + entry.name;
		string dst = *to + string("/") + entry.name;

		ret = LocalFile::RenameFile(src.c_str(), dst.c_str());
		if (ret < 0) {
			dir.Close();
			gamePathGetMountedDrive(*to, detail);
			return ret;
		}
	} while (ret == 0);

	dir.Close();
	return 0;
}

/* the package at path cleaned up for ux0:addcont, then moved to promoteDir (0x81087F00) */
int GameAddcontSource::MovePackage(const string *path, const string *promoteDir, string *detail)
{
	int ret;

	gameRemoveDirectory(promoteDir->c_str(), NULL, NULL, NULL, true);

	string addcontDir = string("ux0:addcont/") + string(m_info->info.titleId);

	ret = gameCleanupInstall(addcontDir.c_str(), m_info, path->c_str(), m_appInfo, detail);
	if (ret < 0)
		return ret;

	string::size_type pos = promoteDir->rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	string parent = promoteDir->substr(0, pos);

	if (!Dir::IsExist(parent.c_str())) {
		ret = Dir::CreateRecursive(parent.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(parent, detail);
			return ret;
		}
	}

	ret = LocalFile::RenameFile(path->c_str(), promoteDir->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*path, detail);
		return ret;
	}

	return 0;
}

/* the package at path, already on the game card, cleaned up for grw0:addcont (0x810881A0) */
int GameAddcontSource::CleanupCardPackage(const string *path, int cancel, int cancelArg,
					  GameJobProgress *progress, string *detail)
{
	int ret;

	string addcontDir = string("grw0:addcont/") + string(m_info->info.titleId);

	ret = gameCleanupInstall(addcontDir.c_str(), m_info, path->c_str(), m_appInfo, detail);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * ux0:temp emptied, then the package at path moved to ux0:temp/promote and removed with it
 * (0x81088322)
 */
int GameAddcontSource::RemovePackage(const string *path, int cancel, int cancelArg,
				     GameJobProgress *progress, string *detail)
{
	int ret;
	string temp("ux0:temp");

	if (Dir::IsExist(temp.c_str())) {
		Dir dir;

		ret = dir.Open(temp.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(temp, detail);
			return ret;
		}

		do {
			DirEnt entry;

			ret = dir.Read(&entry);
			if (ret == 0) {
				if (entry.type == DirEnt::Type_Dir) {
					if (sce_paf_strncmp(entry.name.c_str(), ".", 1) == 0)
						continue;

					string sub = temp + string("/") + entry.name;

					ret = gameRemoveDirectoryContents(sub.c_str(),
									  (GameCancelFunc)cancel,
									  (void *)cancelArg, progress,
									  true);
					if (ret < 0) {
						gamePathGetMountedDrive(temp, detail);
						dir.Close();
						return ret;
					}
				} else {
					string file = temp + string("/") + entry.name;

					ret = LocalFile::RemoveFile(file.c_str());
					if (ret < 0) {
						gamePathGetMountedDrive(temp, detail);
						dir.Close();
						return ret;
					}
				}
			}
		} while (ret == 0);

		dir.Close();
	}

	if (Dir::IsExist("ux0:temp/promote")) {
		ret = gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
		if (ret < 0) {
			gamePathGetMountedDrive(string("ux0:temp/promote"), detail);
			return ret;
		}
	}

	ret = LocalFile::RenameFile(path->c_str(), "ux0:temp/promote");
	if (ret < 0) {
		gamePathGetMountedDrive(string("ux0:temp/promote"), detail);
		return ret;
	}

	ret = gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	if (ret < 0) {
		gamePathGetMountedDrive(string("ux0:temp/promote"), detail);
		return ret;
	}

	return ret;
}

/*
 * The package at path, on the memory card, cleaned up for grw0:addcont, copied to promoteDir on
 * the game card and removed (0x810888C8)
 */
int GameAddcontSource::CopyPackage(const string *path, const string *promoteDir, int cancel,
				   int cancelArg, GameJobProgress *progress, string *detail)
{
	int ret;

	gameRemoveDirectory(promoteDir->c_str(), NULL, NULL, NULL, true);

	string addcontDir = string("grw0:addcont/") + string(m_info->info.titleId);

	ret = gameCleanupInstall(addcontDir.c_str(), m_info, path->c_str(), m_appInfo, detail);
	if (ret < 0)
		return ret;

	string::size_type pos = promoteDir->rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	/* Sony's length: the path's length less the separator's position, plus one */
	string parent = promoteDir->substr(0, promoteDir->length() - pos + 1);

	if (!Dir::IsExist(parent.c_str())) {
		ret = Dir::CreateRecursive(parent.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(parent, detail);
			return ret;
		}
	}

	ret = gameCopyFileWithPool(path->c_str(), promoteDir->c_str(), cancel, cancelArg,
				   (int)progress, (int)detail);
	if (ret < 0)
		return ret;

	ret = RemovePackage(path, cancel, cancelArg, progress, detail);
	if (ret < 0)
		return ret;

	return 0;
}

int GameAddcontSource::Install(const string *path, const string *titleId, string *dir,
			       string *installPath, GameJobProgress *progress, int cancel,
			       int cancelArg, float scale)
{
	int ret;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (m_info->info.contentType != GAME_CONTENT_TYPE_ADDCONT)
		return GAME_ERROR_INVALID_ARGUMENT;

	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/game", NULL, NULL, NULL, true);
	gameRemoveDirectory("grw0:temp/game", NULL, NULL, NULL, true);

	int storage = ((const GameInstallExParam *)m_path)->storage;

	if (storage == 0) {
		ret = gameTitleGetStorage(m_info->info.titleId, &storage);
		if (ret < 0)
			return ret;
	}

	string drive;
	string promoteDir;

	switch (storage) {
	case GAME_ADDCONT_STORAGE_CARD:
		if (!vshSblQafMgrIsAllowFakeACInstall() && !(m_info->info.flags & GAME_PACKAGE_FLAG_CARD))
			return GAME_ERROR_NOT_CARD_PACKAGE;

		GameJobProgress part(*progress, scale);

		if (path->find("ux0:", 0, sce_paf_strlen("ux0:")) != string::npos) {
			drive = "grw0:";
			promoteDir = string("grw0:temp/promote");

			ret = CopyPackage(path, &promoteDir, cancel, cancelArg, &part,
					  m_errorTitleId);
			if (ret < 0)
				return ret;
		} else if (path->find("grw0:", 0, sce_paf_strlen("grw0:")) != string::npos) {
			drive = "grw0:";
			promoteDir = *path;

			ret = CleanupCardPackage(path, cancel, cancelArg, &part, m_errorTitleId);
			if (ret < 0)
				return ret;
		} else {
			return GAME_ERROR_NO_DRIVE;
		}
		break;

	case 2:
	case GAME_ADDCONT_STORAGE_UX0_LAST:
		drive = "ux0:";
		promoteDir = string("ux0:temp/promote");

		ret = MovePackage(path, &promoteDir, NULL);
		if (ret < 0)
			return ret;
		break;

	default:
		return GAME_ERROR_BAD_STORAGE;
	}

	string contentDir = drive + string("addcont/") + *titleId + string("/") +
			    string(m_info->info.entitlementId);

	if (Dir::IsExist(contentDir.c_str())) {
		/* the content installed: its files go to the temporary folder first */
		string tempDir("ux0:temp/game");

		if (storage == GAME_ADDCONT_STORAGE_CARD)
			tempDir = "grw0:temp/game";

		tempDir += "/";
		tempDir += m_info->info.entitlementId;

		if (!Dir::IsExist(tempDir.c_str())) {
			ret = Dir::CreateRecursive(tempDir.c_str(), 6);
			if (ret < 0) {
				gamePathGetMountedDrive(string(tempDir.c_str()), m_errorTitleId);
				return ret;
			}
		}

		ret = MoveToTemp(&contentDir, &tempDir, m_errorTitleId);
		if (ret < 0)
			return ret;
	} else {
		/* a new content: its folder made under its PFS mount */
		string mountPoint;

		ret = gamePfsMount(promoteDir, string(m_info->info.titleId), m_info->keystone,
				   &mountPoint);
		if (ret < 0) {
			gamePathGetMountedDrive(promoteDir, m_errorTitleId);
			return ret;
		}

		string newDir = mountPoint + string(m_info->info.entitlementId);

		if (!Dir::IsExist(newDir.c_str())) {
			ret = Dir::CreateRecursive(newDir.c_str(), 0x106);
			if (ret < 0) {
				gamePfsUnmount(&mountPoint);
				gamePathGetMountedDrive(newDir, m_errorTitleId);
				return ret;
			}

			ret = sceAppMgrAcidDirSet(mountPoint.c_str(), m_info->info.entitlementId);
			if (ret < 0) {
				gameRemoveDirectory(newDir.c_str(), NULL, NULL, NULL, true);
				gamePfsUnmount(&mountPoint);
				gamePathGetMountedDrive(mountPoint, m_errorTitleId);
				return ret;
			}
		}

		ret = gamePfsUnmount(&mountPoint);
		if (ret < 0) {
			gamePathGetMountedDrive(mountPoint, m_errorTitleId);
			return ret;
		}
	}

	ret = gameAddcontMoveEntries(promoteDir, contentDir, m_errorTitleId);
	if (ret < 0)
		return ret;

	string licenseDir = drive + string("license/addcont/") + *titleId + string("/") +
			    string(m_info->info.entitlementId);
	string licensePath;

	ret = gameLicenseWrite(m_info->info.contentType, m_info->license, m_info->licenseSize,
			       &licenseDir, &licensePath, m_errorTitleId);
	if (ret < 0)
		return ret;

	if (Dir::IsExist("grw0:temp/plus"))
		Dir::RemoveRecursive("grw0:temp/plus");
	if (Dir::IsExist("ux0:temp/plus"))
		Dir::RemoveRecursive("ux0:temp/plus");

	char contentId[48];
	SceUInt64 startTime;
	SceUInt64 expireTime;

	sce_paf_memset(contentId, 0, sizeof(contentId));
	sce_paf_memset(&startTime, 0, sizeof(startTime));
	sce_paf_memset(&expireTime, 0, sizeof(expireTime));

	GameRifInfo rif(0);

	if (rif.GetPeriod((SceNpDrmLicense *)m_info->license, m_info->licenseSize, contentId,
			  &startTime, &expireTime) == 0 &&
	    expireTime != 0x7FFFFFFFFFFFFFFFULL) {
		/* a license with a period: noted in addcont/info once installed */
		m_infoDir = drive + string("addcont/info");
		m_contentId = string(contentId);
		m_startTime = startTime;
		m_expireTime = expireTime;
	}

	return 0;
}

int GameAddcontSource::Complete(string *dir, string *installPath, int reserved1, int reserved2,
				int reserved3, float scale)
{
	if (m_infoDir.length() != 0 &&
	    (Dir::IsExist(m_infoDir.c_str()) || Dir::CreateRecursive(m_infoDir.c_str(), 6) >= 0) &&
	    gameAddcontInfoPrepare(m_infoDir.c_str()) >= 0) {
		GameAddcontLicenseInfo info;

		sce_paf_memset(&info, 0, sizeof(info));
		sce_paf_memcpy(info.contentId, m_contentId.c_str(), sizeof(info.contentId));
		/* Sony stores the start time over the two flag words */
		info.licenseFlags = (int)m_startTime;
		info.skuFlags = (int)(m_startTime >> 32);
		info.expireTime = m_expireTime;
		gameAddcontInfoWrite(m_infoDir.c_str(), &info);
	}

	return 0;
}
