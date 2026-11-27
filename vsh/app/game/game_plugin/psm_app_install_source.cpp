/*
 * The game plugin's source of a PlayStation Mobile application's package: installed to
 * ux0:psm/<title ID>, the old one moved to ux0:temp/promote/old first, and the license the
 * package carries in runtime/sce_sys/package/temp.bin written to ux0:license/app/<title ID>
 * (0x8108521C..0x81085DF0). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
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

GamePsmAppInstallSource::GamePsmAppInstallSource(const GameInstallParam *param,
						 string *errorTitleId)
	: m_param(param), m_errorTitleId(errorTitleId)
{
}

GamePsmAppInstallSource::~GamePsmAppInstallSource()
{
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);
	gameRemoveDirectory("ur0:temp/promote", NULL, NULL, NULL, true);
}

int GamePsmAppInstallSource::Install(const string *path, const string *titleId, string *dir,
				     string *installPath, GameJobProgress *progress, int cancel,
				     int cancelArg, float scale)
{
	gameRemoveDirectory("ux0:temp/promote", NULL, NULL, NULL, true);

	*dir = *path;
	*installPath = string("ux0:psm/") + *titleId;
	return 0;
}

/*
 * The license at path read into a block of the plugin's pool and written into dir under the name
 * SceNpDrm gives it; a license larger than the block returns its size (0x8108540E).
 */
__attribute__((noinline)) int GamePsmAppInstallSource::WriteLicense(const string *path,
								    const string *dir,
								    string *licensePath,
								    string *detail)
{
	int ret = 0;
	common::SharedPtr<File> file;
	common::SharedPtr<memory::PoolAllocator> pool;

	ret = gamePluginGetPool("PSMobileRifWrite", &pool);

	if (ret < 0)
		return ret;

	SceSize poolSize = pool->GetFreeSize();
	void *buf = pool->Alloc();
	char rifName[GAME_RIF_NAME_SIZE];

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
				if (ret < 0) {
					file->Close();
				} else {
					file->Close();
					sce_paf_memset(rifName, 0, sizeof(rifName));
					ret = sceNpDrmGetRifNameForInstall(rifName, buf, 1);
					if (ret >= 0) {
						ret = gameFileWriteToDir(buf, (SceSize)size, dir,
									 rifName, licensePath, detail);
						if (ret >= 0)
							ret = 0;
					}
				}
			}
		}
	}

	if (buf != NULL)
		pool->Free(buf);

	gamePluginReleasePool(&pool);
	return ret;
}

int GamePsmAppInstallSource::Complete(string *dir, string *installPath, int progress,
				      int cancel, int cancelArg, float scale)
{
	int ret;

	Dir::CreateRecursive("ux0:temp/promote", 6);

	string oldDir("ux0:temp/promote/old");

	if (Dir::IsExist(installPath->c_str())) {
		ret = LocalFile::RenameFile(installPath->c_str(), oldDir.c_str());
		if (ret < 0)
			return ret;
	}

	Dir::CreateRecursive("ux0:psm", 6);

	string from = *dir + string("/") + string(m_param->titleId);

	ret = LocalFile::RenameFile(from.c_str(), installPath->c_str());
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gamePathGetMountedDrive(*dir, m_errorTitleId);
		return ret;
	}

	string tempPath = *installPath + string("/runtime/sce_sys/package/temp.bin");

	if (LocalFile::Exists(tempPath.c_str())) {
		string licenseDir = string("ux0:license/app/") + string(m_param->titleId);
		string licenseFile = *installPath + string("/runtime/sce_sys/package/temp.bin");
		string licensePath;

		ret = WriteLicense(&licenseFile, &licenseDir, &licensePath, m_errorTitleId);
		if (ret < 0)
			return ret;
	}

	return 0;
}
