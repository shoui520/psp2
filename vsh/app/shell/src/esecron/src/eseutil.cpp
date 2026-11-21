/*
 * eseutil.cpp: the background download library's helpers after its task manager (0x81110708
 * on): a block's timer, the game plugin and power held while a task runs, the Marlin licence
 * folder and the copies of a licence file. The file's name is ours (PLACEHOLDER): Sony's 0.990
 * prototype names only esehttp.cpp, eseparam.cpp and esetask.cpp in this folder.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <file/localfile.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <plugin.h>
#include <kernel/iofilemgr.h>
#include <kernel/processmgr.h>
#include <kernel/libkernel.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseutil.hpp"
#include "src/livespace_db/livespace_db_internal.h"
#include <ces/libces_ucs.h>

/* the SDK's libSceCes (libces_ucs.h) and the user side of _sceFiosKernelOverlayResolveSync */
extern "C" int sceCesUtf8ToUtf32(const uint8_t *utf8buf, uint32_t utf8max, uint32_t *convSize,
				 uint32_t *utf32);					/* 0x8138932C */
extern "C" int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath,
					       char *outPath, SceSize maxPath);		/* 0x8144DA0C */

using namespace paf;

/* (0x815447B0) where the Marlin licences are kept */
const char *bgdlLicenseFolder = "ur0:/MLN_ROOT/LICENSE/";

/* the folders of path made, each in turn (PLACEHOLDER name) */
static inline int bgdlCreateFolders(const char *path)
{
	int ret;
	const char *next;
	const char *slash;

	ret = 0;
	if (path != NULL && *path != '\0') {
		next = path + 1;
		while ((slash = sce_paf_strchr(next, '/')) != NULL) {
			next = slash + 1;
			if (slash[-1] != ':') {
				string folder(path, slash - path);

				ret = Dir::Create(folder.c_str(), 6);
			}
		}
	}
	return ret;
}

/* (0x81110708) */
void BgdlScopeTimer::Report(void)
{
	m_elapsed = m_end - m_start;
	m_elapsedMs = m_elapsed / 1000;
}

/* (0x8111072E) */
int BgdlGamePluginLock::Lock(void)
{
	int ret = -1;

	if (m_locked != true) {
		GamePluginInterface *game =
			(GamePluginInterface *)Plugin::Find("game_plugin")->GetInterface(1);

		if (game == NULL)
			return BGDL_ERROR_NO_GAME_PLUGIN;
		ret = ((int (*)(void))game->func[BGDL_GAME_PLUGIN_FUNC_LOCK])();
		if (ret == 0)
			m_locked = true;
	}
	return ret;
}

/* (0x8111076A) */
BgdlGamePluginLock::BgdlGamePluginLock(bool lock)
{
	m_locked = false;
	if (lock == true) {
		GamePluginInterface *game =
			(GamePluginInterface *)Plugin::Find("game_plugin")->GetInterface(1);

		if (game != NULL && ((int (*)(void))game->func[BGDL_GAME_PLUGIN_FUNC_LOCK])() == 0)
			m_locked = true;
	}
}

/* (0x811107A0) */
BgdlGamePluginLock::~BgdlGamePluginLock()
{
	if (m_locked) {
		GamePluginInterface *game =
			(GamePluginInterface *)Plugin::Find("game_plugin")->GetInterface(1);

		if (game != NULL)
			((void (*)(void))game->func[BGDL_GAME_PLUGIN_FUNC_UNLOCK])();
		m_locked = false;
	}
}

/* (0x811107D0) */
BgdlPowerLock::BgdlPowerLock(bool lock, int arg)
{
	m_locked = false;
	m_arg = arg;
	if (lock == true) {
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		m_locked = true;
	}
}

/* (0x811107F0) */
BgdlPowerLock::~BgdlPowerLock()
{
	if (m_locked == true) {
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		m_locked = false;
	}
}

/* (0x81110808) */
void BgdlLicenseFolder::Bgdl_81110808(void)
{
	m_active = true;
}

/* (0x8111080E) */
void BgdlLicenseFolder::Bgdl_8111080E(void)
{
	if (m_active == true)
		m_active = false;
}

/* (0x8111081C) */
int BgdlLicenseFolder::Bgdl_8111081C(void)
{
	int ret = 0;

	if (Dir::IsExist(bgdlLicenseFolder) != 1)
		ret = bgdlCreateFolders(bgdlLicenseFolder);
	return ret;
}

/* (0x811108C4) */
int Bgdl_811108C4(const char *src, const char *dst, bool overwrite)
{
	int ret = -1;
	SceInt64 size;
	SceInt64 copied = 0;
	common::SharedPtr<File> out;
	common::SharedPtr<File> in;

	if (src == NULL || dst == NULL)
		goto done;
	size = LocalFile::GetFileSize(src);
	if (size <= 0)
		goto done;
	in = LocalFile::Open(src, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret != 0)
		goto done;
	{
		BgdlFileCloser inCloser(in);

		out = LocalFile::Open(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);
		if (ret != 0)
			goto done;

		BgdlFileCloser outCloser(out);
		common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

		if (buf.get() == NULL || buf->GetSize() == 0) {
			ret = BGDL_ERROR_NO_MEMORY;
			goto done;
		}
		while (size > copied) {
			int read = in->Read(buf->GetAddr(), buf->GetSize());
			int written;

			if (read <= 0)
				break;
			written = out->Write(buf->GetAddr(), read);
			if (written <= 0)
				break;
			copied += written;
		}
		ret = size <= copied ? 0 : -1;
	}
done:
	if (ret != 0 && dst != NULL && copied != 0)
		LocalFile::RemoveFile(dst);
	return ret;
}

/* (0x8111102A) */
int BgdlLicenseFolder::Bgdl_8111102A(const char *file, const char *contentId)
{
	int ret = 0;
	string path(bgdlLicenseFolder);

	if (file != NULL && contentId != NULL) {
		ret = 0;
		if (Dir::IsExist(bgdlLicenseFolder) != 1)
			ret = bgdlCreateFolders(bgdlLicenseFolder);
		if (ret == 0) {
			string name(contentId);

			name += ".MNL";
			path.append(name);
			ret = Bgdl_811108C4(file, path.c_str(), true);
			if (ret != 0)
				goto done;
		}
	}
done:
	return ret;
}

/* (0x81111220) */
int Bgdl_81111220(const char *file, const char *contentId)
{
	int ret;

	if (file == NULL || contentId == NULL)
		return BGDL_ERROR_INVALID_ARGUMENT;

	string path("ux0:/MLN_ROOT/LICENSE/");

	if (Dir::IsExist(path.c_str()) != 1) {
		ret = bgdlCreateFolders(path.c_str());
		if (ret != 0)
			return ret;
	}

	string name(contentId);

	name += ".MNL";
	path.append(name);
	if (Dir::IsExist(file) == false) {
		ret = BGDL_ERROR_NO_LICENSE_FILE;
		if (Dir::IsExist(path.c_str()) == 1)
			ret = 0;
		return ret;
	}
	ret = Bgdl_811108C4(file, path.c_str(), true);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x81111432) */
int BgdlLicenseFolder::Bgdl_81111432(const char *contentId, bool remove)
{
	int ret = 0;
	string path(bgdlLicenseFolder);

	if (contentId != NULL) {
		string name(contentId);

		name += ".MNL";
		path.append(name);
		ret = Bgdl_81111220(path.c_str(), contentId);
		if (ret == 0 && remove == true)
			LocalFile::RemoveFile(path.c_str());
	}
	return ret;
}

/* (0x811115BA) */
GamePluginInterface *Bgdl_811115BA(void)
{
	return (GamePluginInterface *)Plugin::Find("game_plugin")->GetInterface(1);
}

/* (0x811115D0) */
string Bgdl_811115D0(string dir, int number)
{
	char buf[16] = "";
	string path;

	sce_paf_snprintf(buf, sizeof(buf) - 1, "%04d", number);
	path = dir;
	path += "/";
	path += buf;
	return path;
}

/* (0x81111724) */
int Bgdl_81111724(const char *path, int reserved, SceUInt64 size)
{
	int ret = -1;
	SceUInt64 written = 0;
	common::SharedPtr<File> file;

	if (path == NULL || size == written)
		goto done;
	file = LocalFile::Open(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);
	if (ret != 0)
		goto done;
	{
		BgdlFileCloser closer(file);
		common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

		if (buf.get() == NULL || buf->GetSize() == 0) {
			ret = BGDL_ERROR_NO_MEMORY;
			goto done;
		}
		sce_paf_memset(buf->GetAddr(), 0, buf->GetSize());
		while (written < size) {
			SceUInt64 left = size - written;
			int ret2;

			if (left >= buf->GetSize())
				left = buf->GetSize();
			ret2 = file->Write(buf->GetAddr(), (size_t)left);
			if (ret2 <= 0)
				break;
			written += ret2;
		}
		ret = written == size ? 0 : -1;
	}
done:
	return ret;
}

/* (0x81111B96) */
int Bgdl_81111B96(const string *name, const string *base, string *out)
{
	const char *ext;

	if (name->length() == 0 || base->length() == 0)
		return -1;
	ext = sce_paf_strrchr(name->c_str(), '.');
	if (ext == NULL)
		return -1;
	*out = base->c_str();
	*out += ext;
	return 0;
}

/* (0x81111C2C) */
bool Bgdl_81111C2C(const string *path, string *name)
{
	const char *slash;

	if (path->length() == 0)
		return false;
	slash = sce_paf_strrchr(path->c_str(), '/');
	if (slash == NULL || sce_paf_strlen(slash + 1) == 0)
		return false;
	*name = slash + 1;
	return true;
}

/* (0x81111C6A) */
int Bgdl_81111C6A(const char *name, string *base)
{
	const char *dot;

	if (name == NULL)
		return -1;
	dot = sce_paf_strchr(name, '.');
	if (dot != NULL)
		base->assign(name, dot - name);
	else
		base->assign(name, sce_paf_strlen(name));
	return 0;
}

/* (0x81111CA2) the name after the path's last '/'; it is not taken when it is one character */
int Bgdl_81111CA2(const char *path, string *name)
{
	int ret = -1;
	const char *slash;

	if (path != NULL) {
		slash = sce_paf_strrchr(path, '/');
		if (slash != NULL) {
			const char *file = slash + 1;

			if (sce_paf_strlen(file + 1) != 0) {
				ret = 0;
				*name = file;
			}
		}
	}
	return ret;
}

/* (0x81111CDC) */
int Bgdl_81111CDC(const char *src, const char *folder, string *out, bool overwrite)
{
	string name;
	int ret = Bgdl_81111CA2(src, &name);

	if (ret == 0 && folder != NULL) {
		*out = folder;
		*out += "/";
		*out += name.c_str();
		ret = Bgdl_811108C4(src, out->c_str(), overwrite);
	}
	return ret;
}

/* (0x81111E5E) */
int Bgdl_81111E5E(string *path)
{
	int ret;

	if (Dir::IsDirectory(path->c_str()) == false)
		return Dir::Remove(path->c_str());
	{
		Dir dir;
		SceSize length;

		ret = dir.Open(path->c_str());
		if (ret < 0)
			return ret;
		length = path->length();
		for (;;) {
			{
				DirEnt entry;

				ret = dir.Read(&entry);
				if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
					break;
				if (ret < 0)
					return ret;
				if ((entry.name.c_str()[0] == '.' && sce_paf_strcmp(entry.name.c_str(), ".") == 0) ||
				    (entry.name.c_str()[0] == '.' && sce_paf_strcmp(entry.name.c_str(), "..") == 0))
					continue;

				string child = *path + '/' + entry.name;

				path->swap(child);
			}
			ret = Bgdl_81111E5E(path);
			path->resize(length);
			if (ret < 0)
				return ret;
		}
	}
	return Dir::Remove(path->c_str());
}

/* (0x811120A0) */
int Bgdl_811120A0(BgdlTaskPath *path, bool recursive)
{
	if (Dir::IsExist(path->m_begin) == false)
		return 0;

	string folder(path->m_begin);

	return Bgdl_81111E5E(&folder);
}

/* (0x81112112) */
int Bgdl_81112112(string *path, bool recursive)
{
	if (Dir::IsExist(path->c_str()) == false)
		return 0;
	return Bgdl_81111E5E(path);
}

/* (0x8111212C) */
int Bgdl_8111212C(const char *folder, const char *name)
{
	BgdlTaskPath path;
	int ret;

	if (path.Reserve(sce_paf_strlen(folder) + sce_paf_strlen(name) + 2) != 0)
		return BGDL_ERROR_NO_MEMORY;
	sce_paf_snprintf(path.m_begin, path.m_end - path.m_begin, "%s/%s", folder, name);
	if (Dir::IsExist(path.m_begin) == false) {
		ret = 0;
	} else {
		string file(path.m_begin);

		ret = Bgdl_81111E5E(&file);
	}
	return ret;
}

/* (0x811121FE) */
int Bgdl_811121FE(const char *path, int mode)
{
	const char *next;
	const char *slash;

	if (path != NULL && *path != '\0') {
		next = path + 1;
		while ((slash = sce_paf_strchr(next, '/')) != NULL) {
			string folder(path, slash - path);

			Dir::Create(folder.c_str(), mode);
			next = slash + 1;
		}
	}
	return Dir::Create(path, mode);
}

/* (0x81112284) */
bool Bgdl_81112284(const char *path)
{
	if (path == NULL)
		return false;

	LocalFileStat stat;

	return LocalFile::Getstat(path, &stat) == 0;
}

/* (0x81112310) */
SceUInt64 Bgdl_81112310(SceUInt64 size, SceUInt32 cluster)
{
	if (size == 0 || cluster == 0)
		return 0;
	return (size + (cluster - 1)) / cluster * cluster;
}

/* what devctl 0x3001 gives for a device (PLACEHOLDER names) */
struct BgdlDevInfo {
	SceUInt64 max;
	SceUInt64 free;
	SceUInt32 clusterSize;
	int reserved;
};

/* (0x8111234C) */
int Bgdl_8111234C(const char *device, BgdlStorageInfo *info)
{
	BgdlDevInfo dev;
	int ret;

	if (Bgdl_81112284(device) == true) {
		ret = sceIoDevctl(device, 0x3001, NULL, 0, &dev, sizeof(dev));
		if (ret >= 0) {
			info->max = dev.max;
			info->free = dev.free;
			info->clusterSize = dev.clusterSize;
		}
	} else {
		ret = BGDL_ERROR_NO_DEVICE;
	}
	return ret;
}

/* (0x811123BA) */
int Bgdl_811123BA(LsdbQueryCondition *conditions, vector<string> *titleIds)
{
	AppInfoQClient *client = new AppInfoQClient;
	int ret;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL)
		return BGDL_ERROR_NO_MEMORY;
	ret = SceLsdb_E90B81CD(client);
	if (ret >= 0) {
		ret = SceLsdb_76B75713(client, conditions, titleIds, 0);
		if (ret >= 0)
			ret = 0;
	}
	operator delete(SceLsdb_E2101B03(client));
	return ret;
}

/* (0x8111240E) */
int Bgdl_8111240E(const char *path, SceUInt64 size)
{
	int ret = 0;
	LocalFileStat stat;
	BgdlScopeTimer timer("truncate");

	if (path == NULL || LocalFile::Getstat(path, &stat) != 0) {
		common::SharedPtr<File> file =
			LocalFile::Open(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);

		if (ret != 0)
			return ret;

		BgdlFileCloser closer(file);
	}
	ret = path != NULL ? LocalFile::Getstat(path, &stat) : -1;
	if (ret != 0)
		return ret;
	stat.st_size = size;
	ret = LocalFile::Chstat(path, &stat, 4);
	if (ret == 0) {
		LocalFileStat check;

		if (path != NULL)
			LocalFile::Getstat(path, &check);
	}
	return ret;
}

/* (0x81112816) */
int Bgdl_81112816(const char *path, SceInt64 size, LocalFileStat *out)
{
	LocalFileStat stat;
	int ret;

	ret = LocalFile::Getstat(path, &stat);
	if (ret != 0)
		return ret;
	if (out != NULL)
		*out = stat;
	if (size == stat.st_size)
		return 0;
	if (size > 0) {
		stat.st_size = size;
		return LocalFile::Chstat(path, &stat, 4);
	}
	return BGDL_ERROR_NO_GAME_FOLDER;
}

/* (0x8111296E) */
int Bgdl_8111296E(const char *folder, SceUInt64 size, int first, int *count)
{
	BgdlScopeTimer timer("truncateFileFAT32");
	int number = 0;
	char name[64] = "";
	int ret;

	if (folder == NULL) {
		ret = BGDL_ERROR_NULL_FOLDER;
	} else {
		do {
			name[0] = '\0';
			sce_paf_snprintf(name, sizeof(name) - 1, "f_%04d_%02d.pkg", first, number);

			string path(folder);

			path += "/";
			path += name;
			if (size >= 0xFFFFFFFF) {
				ret = Bgdl_8111240E(path.c_str(), 0xFFFFFFFF);
				if (ret == 0)
					size -= 0xFFFFFFFF;
			} else {
				ret = Bgdl_8111240E(path.c_str(), size);
				size = 0;
			}
			if (ret != 0)
				break;
			number++;
		} while (size != 0);
		if (ret == 0 && count != NULL)
			*count = number;
	}
	return ret;
}

/* (0x81112BD8) */
int Bgdl_81112BD8(const char *folder, SceUInt64 oldSize, SceUInt64 newSize, int first, int count)
{
	char name[64] = "";
	BgdlScopeTimer timer("update_truncateFAT32");
	int ret;

	if (folder == NULL) {
		ret = BGDL_ERROR_NULL_FOLDER;
	} else if (oldSize == 0 && newSize != 0) {
		int parts = 0;

		ret = Bgdl_8111296E(folder, newSize, first, &parts);
	} else if (newSize <= oldSize) {
		ret = BGDL_ERROR_NULL_FOLDER;
	} else {
		SceUInt64 grow = newSize - oldSize;
		int part = grow / 0xFFFFFFFF;
		SceUInt64 last = grow % 0xFFFFFFFF;

		name[0] = '\0';
		sce_paf_snprintf(name, sizeof(name) - 1, "f_%04d_%02d.pkg", first, part);

		string path(folder);

		path += "/";
		path += name;

		LocalFileStat stat;

		ret = Bgdl_81112816(path.c_str(), last, &stat);
		if (ret == 0 && stat.st_size == 0xFFFFFFFF && part + 1 <= count) {
			name[0] = '\0';
			sce_paf_snprintf(name, sizeof(name) - 1, "f_%04d_%02d.pkg", first, part + 1);
			path = folder;
			path += "/";
			path += name;
			LocalFile::RemoveFile(path.c_str());
		}
	}
	return ret;
}

/* (0x81112FAA) */
int Bgdl_81112FAA(const char *folder, SceUInt64 size, int first)
{
	int ret = 0;
	BgdlScopeTimer timer("removeFileFAT32");
	char name[64] = "";

	if (folder == NULL || size == 0) {
		ret = BGDL_ERROR_NULL_FOLDER;
	} else {
		int parts = (int)(size / 0xFFFFFFFF) + (size % 0xFFFFFFFF != 0 ? 1 : 0);
		int i;

		for (i = 0; i < parts; i++) {
			name[0] = '\0';
			sce_paf_snprintf(name, sizeof(name) - 1, "f_%04d_%02d.pkg", first, i);

			string path(folder);

			path += "/";
			path += name;

			LocalFileStat stat;

			if (LocalFile::Getstat(path.c_str(), &stat) == 0)
				LocalFile::RemoveFile(path.c_str());
		}
	}
	return ret;
}

/* (0x8111322E) */
int Bgdl_8111322E(const char *file, const char *folder, string *path)
{
	string work(folder);
	int ret;

	work += "/sce_sys/package/work.bin";
	ret = Bgdl_811108C4(file, work.c_str(), true);
	if (ret == 0 && path != NULL)
		*path = work.c_str();
	return ret;
}

/* (0x81113336) */
int Bgdl_81113336(const char *folder)
{
	int ret = 0;

	if (folder != NULL) {
		string work(folder);

		work += "/sce_sys/package/work.bin";
		ret = Dir::IsExist(work.c_str());
	}
	return ret;
}

/* (0x81113400) a PSP title's folder: 0 EBOOT.PBP, 1 PARAM.PBP, 2 PBOOT.PBP in USRDIR/CONTENT */
int Bgdl_81113400(const char *folder)
{
	if (folder == NULL)
		return -1;

	string content(folder);

	content += "/USRDIR/CONTENT";

	string path;

	path = content;
	path += "/";
	path += "EBOOT.PBP";
	if (Dir::IsExist(path.c_str()) == 1)
		return 0;
	path = content;
	path += "/";
	path += "PARAM.PBP";
	if (Dir::IsExist(path.c_str()) == 1)
		return 1;
	path = content;
	path += "/";
	path += "PBOOT.PBP";
	if (Dir::IsExist(path.c_str()) == 1)
		return 2;
	return -1;
}

/* (0x811137F6) */
int Bgdl_811137F6(bool mount)
{
	const char *folder = "grw0:fgdl";
	int ret = 0;

	if (Dir::IsExist(folder) == 1)
		Dir::RemoveRecursive(folder);
	if (mount == true) {
		const char *next = folder + 1;
		const char *slash;

		while ((slash = sce_paf_strchr(next, '/')) != NULL) {
			string parent(folder, slash - folder);

			Dir::Create(parent.c_str(), 6);
			next = slash + 1;
		}
		ret = Dir::Create(folder, 6);
	}
	return ret;
}

/* (0x81113890) */
void Bgdl_81113890(int type)
{
	sceKernelPowerTick(type);
}

/* (0x81113898) */
int Bgdl_81113898(const LsdbAppInfoPtr *info, int *storage)
{
	GamePluginInterface *game = Bgdl_811115BA();

	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	return ((int (*)(const LsdbAppInfoPtr *, int *))game->func[GAME_PLUGIN_FUNC_GET_STORAGE])(
		info, storage);
}

/* (0x811138CC) */
int Bgdl_811138CC(const LsdbAppInfoPtr *info, int *kind)
{
	GamePluginInterface *game = Bgdl_811115BA();

	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	return ((int (*)(const LsdbAppInfoPtr *, int *))
		game->func[GAME_PLUGIN_FUNC_GET_CATEGORY_KIND])(info, kind);
}

/* (0x81113900) */
bool Bgdl_81113900(const LsdbAppInfoPtr *info, string *titleId)
{
	LsdbAppInfo *app = info->get();
	LsdbAppInfo::iterator it = app->find(LSDB_KEY_TITLE_ID);

	if (it == app->end())
		return false;
	*titleId = LsdbValueText(it->second);
	return titleId->length() != 0;
}

/* (0x81113984) 1: the path, resolved, is in a download folder */
int Bgdl_81113984(const char *path)
{
	if (path == NULL)
		return 0;

	SceSize len = sce_paf_strlen(path);
	uint32_t convSize;
	uint32_t utf16Len;
	uint32_t utf32;

	if (sceCesUtf8ToUtf32((const uint8_t *)path, len, &convSize, &utf32) != 0 ||
	    (convSize == 3 && utf32 == 0xFEFF))
		return 0;

	SceCesUcsContext ctx;

	sceClibMemset(&ctx, 0, sizeof(ctx));
	if (sceCesUcsContextInit(&ctx) != 0 ||
	    sceCesUtf8StrGetUtf16Len(&ctx, (const uint8_t *)path, len, &convSize, &utf16Len) != 0)
		return 0;

	SceUID pid = sceKernelGetProcessId();

	if (pid < 0)
		return 0;

	char resolved[256] = "";

	if (sceFiosKernelOverlayResolveSync(pid, 0, path, resolved, sizeof(resolved) - 1) != 0)
		return 0;

	string real(resolved);
	char dir[64] = "";

	if (sceFiosKernelOverlayResolveSync(pid, 0, "ux0:bgdl/t", dir, sizeof(dir) - 1) != 0)
		return 0;
	if (real.find(dir) == 0)
		return 1;
	if (sceFiosKernelOverlayResolveSync(pid, 0, "ux0:pspemu/bgdl", dir, sizeof(dir) - 1) != 0)
		return 0;
	if (real.find(dir) == 0)
		return 1;
	if (sceFiosKernelOverlayResolveSync(pid, 0, "grw0:fgdl", dir, sizeof(dir) - 1) != 0)
		return 0;
	if (real.find(dir) == 0)
		return 1;
	return 0;
}

/* (0x81113C72) */
string Bgdl_81113C72(SceUInt32 id, bool make)
{
	string folder("ux0:pspemu/bgdl");
	char name[24] = "";

	sce_paf_snprintf(name, sizeof(name) - 1, "/%08x", id);
	folder += name;
	if (make == true) {
		if (Dir::IsExist(folder.c_str()) == 1)
			Dir::RemoveRecursive(folder.c_str());

		const char *path = folder.c_str();

		if (path != NULL && *path != '\0') {
			const char *next = path + 1;
			const char *slash;

			while ((slash = sce_paf_strchr(next, '/')) != NULL) {
				string parent(path, slash - path);

				Dir::Create(parent.c_str(), 6);
				next = slash + 1;
			}
		}
		if (Dir::Create(path, 6) != 0)
			folder = "";
	}
	return folder;
}

/* (0x81113DE6) */
string Bgdl_81113DE6(const char *titleId)
{
	string iconPath("");
	LsdbIconPtr icon(new LsdbIcon());

	if (icon.get() == NULL)
		return iconPath;

	AppInfoQClient *client = new AppInfoQClient;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL)
		return iconPath;
	if (SceLsdb_E90B81CD(client) >= 0) {
		int ret;

		{
			string id(titleId);

			ret = SceLsdb_7AF9008C(client, &id, &icon, false);
		}
		if (ret >= 0)
			iconPath = icon->iconPath;
	}
	operator delete(SceLsdb_E2101B03(client));
	return iconPath;
}

/* (0x81114092) */
int Bgdl_81114092(vector<string> *titles)
{
	titles->clear();

	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin == NULL)
		return BGDL_ERROR_NO_MEMORY;

	GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

	if (game == NULL)
		return BGDL_ERROR_NO_MEMORY;
	return ((int (*)(vector<string> *))game->func[BGDL_GAME_PLUGIN_FUNC_CARD_TITLES])(titles);
}

/* (0x81114128) */
int Bgdl_81114128(const string *titleId, int kind)
{
	if (titleId->length() == 0)
		return BGDL_ERROR_NULL_FOLDER;
	if (kind != 1)
		return 0;

	vector<string> titles;
	int ret = Bgdl_81114092(&titles);

	if (ret != 0) {
		ret = BGDL_ERROR_NOT_ON_CARD;
	} else {
		SceUInt32 i;

		for (i = 0; i < titles.size(); i++) {
			if (titles[i] == *titleId)
				goto found;
		}
		ret = BGDL_ERROR_NOT_ON_CARD;
	}
found:
	return ret;
}

/* (0x81114220) */
int Bgdl_81114220(const string *titleId, int kind)
{
	if (titleId->length() == 0)
		return BGDL_ERROR_NULL_FOLDER;
	if (kind <= 0 || kind > 2)
		return 0;

	vector<string> titles;
	int ret = Bgdl_81114092(&titles);

	if (ret != 0) {
		ret = BGDL_ERROR_NOT_ON_CARD;
	} else {
		SceUInt32 i;

		for (i = 0; i < titles.size(); i++) {
			if (titles[i] == *titleId)
				goto found;
		}
		ret = BGDL_ERROR_NOT_ON_CARD;
	}
found:
	return ret;
}

/* (0x8111431C) */
int Bgdl_8111431C(string *str, const char *from, const char *to)
{
	int ret = 0;

	if (str->length() != 0 && from != NULL && to != NULL) {
		const char *found = sce_paf_private_strtok(str->c_str(), from);

		if (found != NULL) {
			const char *rest = found + sce_paf_strlen(from);
			SceSize size = sce_paf_strlen(str->c_str()) + sce_paf_strlen(to) + 16;
			char *buf = new char[size];

			if (buf != NULL) {
				sce_paf_memset(buf, 0, size);
				buf[0] = '\0';
				sce_paf_strncpy(buf, str->c_str(), str->length());

				char *at = sce_paf_private_strtok(buf, from);

				if (at != NULL)
					*at = '\0';

				string out(buf);

				out += to;
				if (rest != NULL)
					out += rest;
				delete[] buf;
				*str = out;
				ret = 1;
			}
		}
	}
	return ret;
}

/* (0x811144CE) */
bool Bgdl_811144CE(const string *titleId)
{
	bool ret = false;
	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByAppId(BGDL_APP_ID_GAME, &status) == 0 && status.appName[0] != '\0' &&
	    sce_paf_strlen(status.appName) != 0)
		ret = sce_paf_strncmp(titleId->c_str(), status.appName, 31) == 0;
	return ret;
}

/* (0x81114524) */
bool Bgdl_81114524(SceUInt32 id)
{
	bool ret = false;
	EseParamCache param;
	string titleId;
	SceAppMgrAppStatus status;

	if (bgdlGetTaskParam(id, &param) == 0 && param.key107 == 6 &&
	    (param.kind == 2 || param.kind == 3) && bgdlGetTaskTitle(id, &titleId) == 0 &&
	    titleId.length() != 0 && sceAppMgrGetStatusByAppId(BGDL_APP_ID_GAME, &status) == 0 &&
	    status.appName[0] != '\0' && sce_paf_strlen(status.appName) != 0 &&
	    sce_paf_strncmp(titleId.c_str(), status.appName, 31) == 0)
		ret = true;
	return ret;
}
