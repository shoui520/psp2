/*
 * The game plugin's check of an application's update against the installed application: where
 * each one is (a game card or ux0:) decides whether the update goes in, is not needed, or puts
 * the card's application back in place of a copy on ux0: (0x8104B81C..0x8104DAC4). The file's
 * name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameUpdateChecker::GameUpdateChecker(const string &path, bool replace, bool notifyTopmenu,
				     GameJobProgress *progress, string *detail)
	: m_path(path), m_replace(replace), m_reserved(false), m_notifyTopmenu(notifyTopmenu),
	  m_progress(progress), m_detail(detail)
{
	string::size_type pos = m_path.rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos != string::npos)
		m_name = m_path.substr(pos + 1, m_path.length() - pos);
}

GameUpdateChecker::~GameUpdateChecker()
{
}

/*
 * An update in ux0: (storage) over the application where installedStorage says: one installed
 * from a card loses its ur0:appmeta folder (the card's drives unmounted first when the update
 * is a card's other copy) (0x8104B97A).
 */
int GameUpdateChecker::CheckUx0(int installedStorage, int storage, const LsdbAppInfoPtr *installed,
				const LsdbAppInfoPtr *info, bool versionDiffers)
{
	string cardDir = string("gro0:app/") + m_name;

	if (storage == GAME_STORAGE_UX0) {
		switch (installedStorage) {
		case GAME_STORAGE_GRO0: {
			string metaDir = string("ur0:appmeta/") + m_name;
			int ret = gameRemoveDirectory(metaDir.c_str(), 0, 0, 0, true);

			if (ret < 0)
				return ret;
			return 0;
		}
		case GAME_STORAGE_GRO0_OTHER:
		case GAME_STORAGE_UX0:
			return 0;
		}
	} else if (storage == GAME_STORAGE_GRO0_OTHER) {
		if (installedStorage == GAME_STORAGE_GRO0) {
			gameUnmountGameDrives();

			string metaDir = string("ur0:appmeta/") + m_name;
			int ret = gameRemoveDirectory(metaDir.c_str(), 0, 0, 0, true);

			if (ret < 0)
				return ret;
			return 0;
		}

		if (installedStorage == GAME_STORAGE_GRO0_OTHER)
			return 0;
	}

	return GAME_ERROR_NO_STORAGE;
}

/*
 * The card's application put back in place of its copy on ux0: the copy and its license moved
 * aside to ux0:temp/promote, the installed record's paths turned back to gro0:, registered
 * again and the copy removed (0x8104BD92).
 */
int GameUpdateChecker::ReturnToCard(const LsdbAppInfoPtr *installed)
{
	gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
	Dir::CreateRecursive("ux0:temp/promote", 6);

	string from = string("ux0:app/") + m_name;
	string to("ux0:temp/promote/app");

	int ret = LocalFile::RenameFile(from.c_str(), to.c_str());

	if (ret < 0) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		gamePathGetMountedDrive(to, m_detail);
		return ret;
	}

	if (m_progress != NULL)
		m_progress->Add(5.0f);

	from = string("ux0:license/app/") + m_name;
	to = string("ux0:temp/promote/license");

	ret = LocalFile::RenameFile(from.c_str(), to.c_str());
	if (ret < 0) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		gamePathGetMountedDrive(to, m_detail);
		return ret;
	}

	if (m_progress != NULL)
		m_progress->Add(5.0f);

	GameAppRegistration reg;

	reg.info = *installed;
	reg.liveAreaState = 0;

	if (reg.entry.get() == NULL) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	ret = gameAppRegistrationReplacePaths(&reg, string("ux0:"), string("gro0:"));
	if (ret < 0) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		return ret;
	}

	LsdbAppInfo::iterator it = (*installed)->find(LSDB_KEY_ATTRIBUTE);
	SceUInt64 attribute = 0;

	if (it != (*installed)->end())
		attribute = gameLsdbValueInteger(it->second);
	gameLsdbValueSetInteger((**installed)[LSDB_KEY_ATTRIBUTE], attribute | 1);

	char param[4];

	param[0] = 0;
	ret = gameAppRegister(&reg, 1, false, m_notifyTopmenu, param);
	if (ret < 0) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		return ret;
	}

	if (m_progress != NULL)
		m_progress->Add(5.0f);

	GameJobProgress *progress = NULL;

	if (m_progress != NULL) {
		progress = new GameJobProgress(*m_progress, 0.75f);
		if (progress == NULL) {
			if (m_notifyTopmenu)
				gameAppRegisterEnd(&reg.info);
			return GAME_ERROR_NO_INFO_MEMORY;
		}
	}

	gameRemoveDirectory("ux0:temp/promote", 0, 0, (int)progress, true);
	delete progress;

	ret = gameAppUpdate(&reg.info, param[0], m_notifyTopmenu);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The application's copy on ux0: and its license moved aside to ux0:temp/promote; a copy or
 * license that is not there is no error (0x8104CA10).
 */
int GameUpdateChecker::MoveToTemp(const LsdbAppInfoPtr *installed)
{
	Dir::CreateRecursive("ux0:temp/promote", 6);

	string from = string("ux0:app/") + m_name;
	string to("ux0:temp/promote/app");

	int ret = LocalFile::RenameFile(from.c_str(), to.c_str());

	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		gamePathGetMountedDrive(to, m_detail);
		return ret;
	}

	from = string("ux0:license/app/") + m_name;
	to = string("ux0:temp/promote/license");

	ret = LocalFile::RenameFile(from.c_str(), to.c_str());
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		gamePathGetMountedDrive(to, m_detail);
		return ret;
	}

	return 0;
}

/*
 * An update on a game card (storage) over the application where installedStorage says: the
 * same card's application already promoted needs nothing (GAME_ERROR_GC_QUIET); one installed
 * on ux0: is replaced by the card's when replacing is allowed and it is not running
 * (0x8104CD60).
 */
int GameUpdateChecker::CheckGro0(int installedStorage, int storage, const LsdbAppInfoPtr *installed,
				 const LsdbAppInfoPtr *info, bool versionDiffers)
{
	int ret;

	if (storage == GAME_STORAGE_GRO0) {
		switch (installedStorage) {
		case GAME_STORAGE_GRO0: {
			if (versionDiffers)
				return 0;
			if (gameAppInfoParentalDiffers(info, installed))
				return 0;
			if (m_replace)
				return 0;

			string metaDir = string("ur0:appmeta/") + m_name;

			if (Dir::IsExist(metaDir.c_str()))
				return GAME_ERROR_GC_QUIET;
			return 0;
		}
		case GAME_STORAGE_GRO0_OTHER:
			return GAME_ERROR_STORAGE_CONFLICT;
		case GAME_STORAGE_UX0:
			return GAME_ERROR_INSTALLED_ON_UX0;
		default:
			return GAME_ERROR_NO_STORAGE;
		}
	}

	if (storage != GAME_STORAGE_GRO0_OTHER)
		return GAME_ERROR_NO_STORAGE;

	switch (installedStorage) {
	case GAME_STORAGE_GRO0:
		return GAME_ERROR_STORAGE_CONFLICT;
	case GAME_STORAGE_GRO0_OTHER: {
		int kind;

		ret = gameAppInfoGetCategoryKind(info, &kind);
		if (ret < 0)
			return ret;

		if (kind == 1) {
			ret = gameAttributeCompare(installed, info);
			if (ret < 0)
				return ret;
		}

		LocalFileStat stat;

		if (LocalFile::Getstat("ux0:", &stat) == 0) {
			string sfoPath = string("ux0:appmeta/") + m_name + string("/param.sfo");

			if (LocalFile::Getstat(sfoPath.c_str(), &stat) < 0)
				return 0;
		}

		if (versionDiffers || gameAppInfoParentalDiffers(info, installed) || m_replace)
			return 0;

		string metaDir = string("ur0:appmeta/") + m_name;
		string ux0MetaDir = string("ux0:appmeta/") + m_name;

		if (Dir::IsExist(metaDir.c_str()) &&
		    (!Dir::IsExist("ux0:") || Dir::IsExist(ux0MetaDir.c_str())))
			return GAME_ERROR_GC_QUIET;
		return 0;
	}
	case GAME_STORAGE_UX0:
		if (!m_replace)
			return GAME_ERROR_INSTALL_TITLE;

		if (gameAppCheckNotRunning(m_name, installed) < 0)
			return GAME_ERROR_GC_PLAIN;

		if (gameAppInfoParentalDiffers(info, installed)) {
			ret = MoveToTemp(installed);
			if (ret >= 0)
				return 0;
		} else {
			ret = ReturnToCard(installed);
			if (ret >= 0)
				return GAME_ERROR_GC_QUIET;
		}
		break;
	default:
		return GAME_ERROR_NO_STORAGE;
	}

	return ret;
}

/*
 * The update at m_path (its record info) checked against the installed application's record:
 * nothing to check without an installed record or outside gro0:app, ux0:app and ux0:appmeta
 * (0x8104D4CA).
 */
int GameUpdateChecker::Check(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *info)
{
	if ((*installed)->size() == 0)
		return 0;

	if (m_path.find("gro0:app/") == string::npos && m_path.find("ux0:app/") == string::npos &&
	    m_path.find("ux0:appmeta/") == string::npos)
		return 0;

	if (m_name.length() == 0)
		return GAME_ERROR_INVALID_ARGUMENT;

	LsdbAppInfo::iterator end = (*info)->end();
	SceUInt64 attribute = 0;
	int storage;
	string::size_type pos = m_path.find("appmeta/");

	if (pos == string::npos) {
		LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ATTRIBUTE);

		if (it != end)
			attribute = gameLsdbValueInteger(it->second);

		storage = gamePathGetStorage(m_path, attribute);
		if (storage == 0)
			return GAME_ERROR_INVALID_ARGUMENT;
	} else {
		string appPath(m_path);

		appPath.replace(pos, sizeof("appmeta/") - 1, "app/", 0, sizeof("app/") - 1);

		storage = GAME_STORAGE_GRO0_OTHER;
		if (Dir::IsExist(appPath.c_str()))
			storage = GAME_STORAGE_UX0;
	}

	int installedStorage = 0;
	int ret = gameAppGetStorage(installed, &installedStorage);

	if (ret < 0)
		return ret;

	string version;

	version.clear();

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APP_VER);

	if (it != end)
		version = LsdbValueText(it->second);

	string installedVersion;

	installedVersion.clear();
	it = (*installed)->find(LSDB_KEY_APP_VER);
	if (it != (*installed)->end())
		installedVersion = LsdbValueText(it->second);

	bool same = version == installedVersion;
	bool versionDiffers = !same;

	if (m_path.find("gro0:") != string::npos) {
		ret = CheckGro0(installedStorage, storage, installed, info, versionDiffers);
		if (ret < 0)
			return ret;
	} else if (m_path.find("ux0:") != string::npos) {
		ret = CheckUx0(installedStorage, storage, installed, info, versionDiffers);
		if (ret < 0)
			return ret;
	}

	return 0;
}
