/*
 * The game plugin's appmeta: an application's appmeta moved into place, the room for it checked,
 * its parameters, EBOOT parameters and images copied into it (0x81054B14..0x81055EF8). The file's
 * name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <kernel/iofilemgr.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <image/image.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the home screen's room: fewer user icons than this, and fewer than that on its first pages */
#define GAME_LSDB_MAX_USER_ICONS	500
#define GAME_LSDB_ROOM_PAGES		10
#define GAME_LSDB_MAX_ROOM_ICONS	100

/* the largest param.sfo copied into an appmeta, and how many extra ones an application has */
#define GAME_APPMETA_PARAM_MAX_SIZE	0x8000
#define GAME_APPMETA_EXTRA_PARAMS	1

/* an application's icon0.png and pic0.png as they are copied: indexed PNGs of their extent */
static GameImageFormat s_icon0Format(128, 128, 0x8000, ImageMode_index8, image::ImageFormat_Png);
static GameImageFormat s_pic0Format(960, 544, 0x96000, ImageMode_index8, image::ImageFormat_Png);

int gameAppmetaCommit(const string *tempDir, const string *titleId, bool removeTempDir)
{
	string drive;
	int ret = gamePathGetDrive(*tempDir, &drive);

	if (ret < 0)
		return ret;

	string appmetaDir = drive + string("appmeta");

	Dir::CreateRecursive(appmetaDir.c_str(), 6);
	appmetaDir.append(string("/") + *titleId);

	string newDir = drive + string("temp/appmeta/") + *titleId;
	string oldDir = drive + string("temp/appmeta/old");

	if (Dir::IsExist(appmetaDir.c_str())) {
		ret = LocalFile::RenameFile(appmetaDir.c_str(), oldDir.c_str());
		if (ret < 0)
			return ret;
	}

	ret = LocalFile::RenameFile(newDir.c_str(), appmetaDir.c_str());
	if (ret < 0)
		return ret;

	if (removeTempDir)
		gameRemoveDirectory(tempDir->c_str(), 0, 0, 0, true);

	return 0;
}

int gameAppmetaCommitNew(const string *titleId)
{
	int ret = Dir::CreateRecursive("ux0:temp/appmeta", 6);

	if (ret < 0)
		return ret;

	Dir::CreateRecursive("ux0:appmeta", 6);

	string appmetaDir = string("ux0:appmeta/") + *titleId;

	if (Dir::IsExist(appmetaDir.c_str())) {
		ret = LocalFile::RenameFile(appmetaDir.c_str(), "ux0:temp/appmeta/old");
		if (ret < 0)
			return ret;
	}

	ret = LocalFile::RenameFile("ux0:appmeta/new", appmetaDir.c_str());
	if (ret < 0)
		return ret;

	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);

	return 0;
}

int gameLsdbHasRoom(bool *hasRoom)
{
	AppInfoSClient client;

	*hasRoom = false;

	SceLsdb_D322A870(&client);

	bool userRoom = SceLsdb_4DB9BCB4(&client) < GAME_LSDB_MAX_USER_ICONS;
	int ret = SceLsdb_CF4B3684(&client, 0, GAME_LSDB_ROOM_PAGES);

	if (ret < 0) {
		SceLsdb_56D8DCF0(&client);
		return ret;
	}

	bool pageRoom = ret < GAME_LSDB_MAX_ROOM_ICONS;

	*hasRoom = userRoom && pageRoom;

	SceLsdb_56D8DCF0(&client);

	return 0;
}

int gameImageGetFileInfo(memory::HeapAllocator *allocator, const string &path,
			 GameImageInfo *info)
{
	common::SharedPtr<image::Image> img =
		image::Image::Open(path.c_str(), allocator, image::ImageFormat_None, 0, 0);

	if (img.get() == NULL)
		return image::Image::GetErrorType();

	info->width = img->GetExtent().w;
	info->height = img->GetExtent().h;
	info->mode = img->GetMode();
	info->format = img->GetFormat();

	return 0;
}

int gameImageGetInfo(memory::HeapAllocator *allocator, void *buf, size_t size,
		     GameImageInfo *info)
{
	common::SharedPtr<image::Image> img =
		image::Image::Open(buf, size, allocator, image::ImageFormat_None);

	if (img.get() == NULL)
		return image::Image::GetErrorType();

	info->width = img->GetExtent().w;
	info->height = img->GetExtent().h;
	info->mode = img->GetMode();
	info->format = img->GetFormat();

	return 0;
}

int gameAppmetaCheckSpace(bool check, SceUInt64 size, string *detail)
{
	if (!check)
		return 0;

	SceUInt64 maxSize = 0;
	SceUInt64 freeSize = 0;
	int ret = gameDeviceGetSpace("ux0:", &maxSize, &freeSize);

	if (ret < 0 && ret != SCE_ERROR_ERRNO_ENODEV) {
		gamePathGetMountedDrive(string("ux0:"), detail);
		return ret;
	}

	if (freeSize < size)
		return GAME_ERROR_NO_APPMETA_SPACE;

	return 0;
}

int gameUr0CheckFreeSpace(SceUInt64 size, string *detail)
{
	SceUInt64 maxSize = 0;
	SceUInt64 freeSize = 0;
	int ret = gameDeviceGetSpace("ur0:", &maxSize, &freeSize);

	if (ret < 0) {
		gamePathGetMountedDrive(string("ur0:"), detail);
		return ret;
	}

	if (freeSize < size)
		return GAME_ERROR_NO_UR0_SPACE;

	return 0;
}

int gameAppmetaCopyParams(const string &srcDir, const string &dstDir, string *detail)
{
	string src = srcDir + string("/sce_sys/param.sfo");
	string dst = dstDir + string("/param.sfo");
	LocalFileStat stat;
	int ret = LocalFile::Getstat(src.c_str(), &stat);

	if (ret < 0) {
		gamePathGetMountedDrive(src, detail);
		return ret;
	}

	if (stat.st_size > GAME_APPMETA_PARAM_MAX_SIZE)
		return GAME_ERROR_BAD_PARAM_SIZE;

	ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, (int)detail);
	if (ret < 0)
		return ret;

	for (unsigned int i = 0; i < GAME_APPMETA_EXTRA_PARAMS; i++) {
		char name[32];

		sce_paf_snprintf(name, sizeof(name), "/sce_sys/param%u.sfo", i + 2);
		src = srcDir + string(name);

		if (LocalFile::Getstat(src.c_str(), &stat) != 0)
			continue;

		if (stat.st_size > GAME_APPMETA_PARAM_MAX_SIZE)
			return GAME_ERROR_BAD_PARAM_SIZE;

		sce_paf_snprintf(name, sizeof(name), "/param%u.sfo", i + 2);
		dst = dstDir + string(name);

		ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, (int)detail);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int gameImageConvert(const string *src, const string *dst, const GameImageFormat *format,
		     string *detail)
{
	LocalFileStat stat;

	sce_paf_memset(&stat, 0, sizeof(stat));

	if (LocalFile::Getstat(src->c_str(), &stat) < 0)
		return 0;

	if (stat.st_size > format->maxSize)
		return GAME_ERROR_BAD_PARAM_SIZE;

	GameImageInfo info;
	int ret = gameImageGetFileInfo(NULL, src->c_str(), &info);

	if (ret < 0)
		return ret;

	if (info.width != format->width || info.height != format->height ||
	    info.mode != format->mode || info.format != format->format)
		return GAME_ERROR_BAD_PARAM_SIZE;

	ret = gameCopyFileWithPool(src->c_str(), dst->c_str(), 0, 0, 0, (int)detail);
	if (ret < 0)
		return ret;

	return 0;
}

const GameImageFormat *gameIcon0Format(void)
{
	return &s_icon0Format;
}

const GameImageFormat *gamePic0Format(void)
{
	return &s_pic0Format;
}

int gameAppmetaWriteEbootParam(const string *ebootPath, const string *paramPath, string *detail)
{
	GameEbootParam *param = new GameEbootParam();

	if (param == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = param->Save(*ebootPath, *paramPath);

	if (ret < 0) {
		delete param;
		gamePathGetMountedDrive(*ebootPath, detail);
		return ret;
	}

	delete param;

	return 0;
}
