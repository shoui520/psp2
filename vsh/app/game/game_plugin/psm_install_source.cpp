/*
 * The game plugin's source of a PlayStation Mobile game's package: its RO and RW folders built in
 * ux0:temp/promote/<title ID> and moved to ux0:psm/<title ID>. A game already installed is moved
 * to ux0:temp/promote/old first, and its license, RW folders and (for a Unity game) save data
 * carried over to the new one; a new game gets an empty RW folder with its content ID. The
 * license is written from the package's work.bin, and a Unity game's runtime license from its
 * temp.bin (0x8108CD5C..0x8108ECF8). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <memory/pool_allocator.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the package's CONTENTS folders that become the game's RO folders */
static const char *s_roDirs[] = {
	"Application",
};

/* the old game's RW folders the new one keeps */
static const char *s_rwDirs[] = {
	"Documents",
	"System",
};

/* the users whose save data folders a Unity game's update carries over */
#define GAME_PSM_SAVEDATA_USERS		64

GamePsmInstallSource::GamePsmInstallSource(const SceNpDrmInstallInfo *info,
					   const LsdbAppInfoPtr *appInfo, string *errorTitleId)
	: m_info(info), m_appInfo(*appInfo), m_errorTitleId(errorTitleId)
{
}

GamePsmInstallSource::~GamePsmInstallSource()
{
	gameRemoveDirectory("grw0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:appmeta/new", NULL, NULL, NULL, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", NULL, NULL, NULL, true);
	gameRemoveDirectory("ux0:temp/appmeta", NULL, NULL, NULL, true);
}

int GamePsmInstallSource::Install(const string *path, const string *titleId, string *dir,
				  string *installPath, GameJobProgress *progress, int cancel,
				  int cancelArg, float scale)
{
	m_pkgPath = *path;

	if (m_info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	if (m_info->info.contentType != GAME_CONTENT_TYPE_PSM &&
	    m_info->info.contentType != GAME_CONTENT_TYPE_PSM_UNITY)
		return GAME_ERROR_INVALID_ARGUMENT;

	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);

	m_titleId = *titleId;
	*dir = *path + string("/contents");
	*installPath = string("ux0:psm/") + *titleId;
	return 0;
}

/*
 * The license at path read into a block of the plugin's pool and written into dir under the name
 * SceNpDrm (npDrm) or ScePsmDrm gives it; a license larger than the block returns its size
 * (0x8108D12E).
 */
__attribute__((noinline)) int GamePsmInstallSource::WriteLicense(const string *path,
								 const string *dir,
								 string *licensePath,
								 string *detail, bool npDrm)
{
	common::SharedPtr<File> file;
	string rifName;
	common::SharedPtr<memory::PoolAllocator> pool;
	int ret = gamePluginGetPool("PSMobileRifWrite", &pool);

	if (ret < 0)
		return ret;

	SceSize poolSize = pool->GetFreeSize();
	void *buf = pool->Alloc();

	if (buf == NULL) {
		ret = GAME_ERROR_NO_MEMORY;
	} else {
		file = LocalFile::Open(path->c_str(), SCE_O_RDONLY, 0, &ret);
		if (ret >= 0) {
			SceInt64 size = file->GetFileSize();

			if ((SceInt64)poolSize < size) {
				file->Close();
				ret = (int)size;
			} else {
				ret = file->Read(buf, (SceSize)size);
				if (ret >= 0) {
					file->Close();
					if (npDrm) {
						char name[GAME_RIF_NAME_SIZE];

						sce_paf_memset(name, 0, sizeof(name));
						ret = sceNpDrmGetRifNameForInstall(name, buf, 1);
						if (ret >= 0)
							rifName = name;
					} else {
						char name[GAME_RIF_NAME_SIZE];

						sce_paf_memset(name, 0, sizeof(name));
						ret = scePsmDrmGetRifNameForInstall(name, buf);
						if (ret >= 0)
							rifName = name;
					}
					if (ret >= 0) {
						ret = gameFileWriteToDir(buf, (SceSize)size, dir,
									 rifName.c_str(),
									 licensePath, detail);
						if (ret >= 0)
							ret = 0;
					}
				} else {
					file->Close();
				}
			}
		}
	}

	if (buf != NULL)
		pool->Free(buf);

	gamePluginReleasePool(&pool);
	return ret;
}

int GamePsmInstallSource::Complete(string *dir, string *installPath, int progress, int cancel,
				   int cancelArg, float scale)
{
	unsigned int i;
	bool hadOld;
	int ret;

	if (dir->length() == 0) {
		gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
		return 0;
	}

	bool unity = m_info->info.contentType == GAME_CONTENT_TYPE_PSM_UNITY;

	Dir::CreateRecursive("ux0:temp/promote", 6);

	string pkgDir("ux0:temp/promote/pkg");

	ret = LocalFile::RenameFile(m_pkgPath.c_str(), pkgDir.c_str());
	if (ret < 0)
		return ret;

	hadOld = false;
	if (Dir::IsExist(installPath->c_str())) {
		hadOld = true;
		Dir::CreateRecursive("ux0:temp/promote", 6);
		ret = LocalFile::RenameFile(installPath->c_str(), "ux0:temp/promote/old");
		if (ret < 0) {
			gamePathGetMountedDrive(*installPath, m_errorTitleId);
			return ret;
		}

		if (unity && m_appInfo->size() != 0) {
			LsdbAppInfo::iterator it = m_appInfo->find(LsdbKeyHash("INSTALL_DIR_SAVEDATA"));

			if (it != m_appInfo->end()) {
				string savedataDir(LsdbValueText(it->second));

				if (gameCheckTitleId(savedataDir.c_str()) < 0)
					return GAME_ERROR_BAD_SAVEDATA_DIR;

				for (i = 0; i < GAME_PSM_SAVEDATA_USERS; i++) {
					char user[3];

					sce_paf_snprintf(user, sizeof(user), "%02d", i);

					string savedata = string("user/") + string(user) +
							  string("/savedata/") + savedataDir;
					string from = string("ux0:") + savedata;
					string to = string("ux0:temp/promote") + string("/old/") + savedata;

					if (LocalFile::Exists(from.c_str())) {
						Dir::CreateRecursive(to.c_str(), 6);
						Dir::RemoveRecursive(to.c_str());
						ret = LocalFile::RenameFile(from.c_str(), to.c_str());
						if (ret < 0) {
							gamePathGetMountedDrive(from, m_errorTitleId);
							return ret;
						}
					}
				}
			}
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

	string newDir = string("ux0:temp/promote/") + m_titleId;
	string roDir = newDir + string("/RO");
	string rwDir = newDir + string("/RW");

	Dir::CreateRecursive(roDir.c_str(), 6);
	Dir::CreateRecursive(rwDir.c_str(), 6);

	SceUInt32 hasLicense = m_info->info.flags & GAME_PACKAGE_FLAG_CARD;

	if (hadOld) {
		if (hasLicense) {
			string licenseDir = roDir + string("/License");
			LocalFileStat stat;
			string workBin = pkgDir + string("/sce_sys/package/work.bin");

			if (LocalFile::Getstat(workBin.c_str(), &stat) == 0) {
				string licenseFile = pkgDir + string("/sce_sys/package/work.bin");
				string licensePath;

				ret = WriteLicense(&licenseFile, &licenseDir, &licensePath,
						   m_errorTitleId, false);
				if (ret < 0)
					return ret;
			} else {
				string oldLicense("ux0:temp/promote/old/RO/License");

				ret = LocalFile::RenameFile(oldLicense.c_str(), licenseDir.c_str());
				if (ret < 0) {
					gamePathGetMountedDrive(*dir, m_errorTitleId);
					return ret;
				}
			}
		}

		for (i = 0; i < 2; i++) {
			string from = string("ux0:temp/promote/old/RW/") + string(s_rwDirs[i]);
			string to = rwDir + string("/") + string(s_rwDirs[i]);

			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0) {
				gamePathGetMountedDrive(*dir, m_errorTitleId);
				return ret;
			}
		}

		string tempDir = rwDir + string("/Temp");

		Dir::CreateRecursive(tempDir.c_str(), 6);
	} else {
		if (hasLicense) {
			string licenseFile = pkgDir + string("/sce_sys/package/work.bin");
			string licensePath;
			string licenseDir = roDir + string("/License");

			ret = WriteLicense(&licenseFile, &licenseDir, &licensePath, m_errorTitleId,
					   false);
			if (ret < 0)
				return ret;
		}

		string subDir = rwDir + string("/Documents");

		Dir::CreateRecursive(subDir.c_str(), 6);
		subDir = rwDir + string("/Temp");
		Dir::CreateRecursive(subDir.c_str(), 6);
		subDir = rwDir + string("/System");
		Dir::CreateRecursive(subDir.c_str(), 6);

		string contentIdPath = rwDir + string("/System/content_id");

		ret = gameFileWrite(contentIdPath, m_info->info.contentId,
				    sizeof(m_info->info.contentId), m_errorTitleId);
		if (ret < 0) {
			gamePathGetMountedDrive(contentIdPath, m_errorTitleId);
			return ret;
		}
	}

	string from;
	string to;

	for (i = 0; i < 1; i++) {
		from = pkgDir + string("/CONTENTS/") + string(s_roDirs[i]);
		to = roDir + string("/") + string(s_roDirs[i]);

		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			gamePathGetMountedDrive(from, m_errorTitleId);
			return ret;
		}
	}

	if (unity) {
		string runtimeFrom = pkgDir + string("/contents/") + string("runtime");
		string runtimeTo = newDir + string("/") + string("runtime");

		ret = LocalFile::RenameFile(runtimeFrom.c_str(), runtimeTo.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(runtimeFrom, m_errorTitleId);
			return ret;
		}
	}

	ret = LocalFile::RenameFile(newDir.c_str(), installPath->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newDir, m_errorTitleId);
		return ret;
	}

	if (unity) {
		LocalFileStat stat;
		string tempBin = *installPath + string("/runtime/sce_sys/package/temp.bin");

		ret = LocalFile::Getstat(tempBin.c_str(), &stat);
		if (ret < 0) {
			gamePathGetMountedDrive(tempBin, m_errorTitleId);
			return ret;
		}

		string licenseDir = string("ux0:license/app/") + m_titleId;
		string licenseFile = *installPath + string("/runtime/sce_sys/package/temp.bin");
		string licensePath;

		ret = WriteLicense(&licenseFile, &licenseDir, &licensePath, m_errorTitleId, true);
		if (ret < 0)
			return ret;
	}

	return 0;
}
