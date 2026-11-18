/*
 * What an interrupted installation leaves behind: applications whose package was not put
 * together again, ICV files no PFS entry names any more, and the split package
 * (sce_sys/package/) concatenated back into sce_sys/package.bin. The file's and every name
 * here are ours (no record names them); the strings are Sony's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <dir/dir.h>
#include <file/file.h>
#include <file/localfile.h>
#include <common/shared_ptr.h>
#include <memory/pool_allocator.h>

#include "game_plugin.h"
#include "install_cleanup.h"

using namespace paf;

/* the errors (values only) */
#define GAME_ERROR_INVALID_PACKAGE	-2146430693	/* 0x8010111B */
#define GAME_ERROR_PACKAGE_TOO_LARGE	-2146430658	/* 0x8010113E */

/* the PFS devctl that lists the NIDs of the files a PFS image holds (value only) */
#define GAME_PFS_DEVCTL_GET_NIDS	0x4812

/* an application's keystone (sce_sys/keystone) */
#define GAME_KEYSTONE_SIZE		0x60

/* where gameAppGetStorage says an application is installed */
#define GAME_STORAGE_GRO0		1
#define GAME_STORAGE_GRO0_OTHER		2
#define GAME_STORAGE_UX0		3

/* the largest NID list read */
#define GAME_PFS_NIDS_SIZE		0x10000

/* a package file: its header and its table of entries */
#define GAME_PACKAGE_MAGIC		0x676B707F
#define GAME_PACKAGE_COMP_MAGIC		0x504D437F
#define GAME_PACKAGE_FILES		3
#define GAME_PACKAGE_ENTRIES		7
#define GAME_PACKAGE_COMP_ENTRIES	4
#define GAME_PACKAGE_COMP_FIRST		3
#define GAME_PACKAGE_ALIGN		16

/* body.bin's size is counted in blocks of this many bytes */
#define GAME_PACKAGE_BODY_BLOCK		32

struct GamePackageEntry {
	SceUInt32 index;
	SceUInt32 reserved;
	SceUInt32 offset;
	SceUInt32 size;
};

/* package.bin's header; the table of its entries follows it */
struct GamePackageHeader {
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt32 count;
	SceUInt32 size;
};

/* the split package's comp.bin: its header, its table, then its data, size bytes in all */
struct GamePackageComp {
	GamePackageHeader header;
	GamePackageEntry entry[GAME_PACKAGE_COMP_ENTRIES];
};

/* the files of a split package, in package.bin's order; comp.bin's entries name the last four */
static const char *s_gamePackageFiles[GAME_PACKAGE_ENTRIES] = {
	"stat.bin",
	"head.bin",
	"body.bin",
	"inst.bin",
	"temp.bin",
	"work.bin",
	"tail.bin",
};

/* whether an application in dir has neither its split package nor its package.bin */
static int gameHasBrokenPackage(const char *dir)
{
	Dir folder;
	int ret;

	if (folder.Open(dir) < 0)
		return 0;

	DirEnt entry;

	do {
		ret = folder.Read(&entry);
		if (ret == 0 && entry.type == DirEnt::Type_Dir && entry.name != "sce_sys" &&
		    entry.name != "sce_pfs") {
			LocalFileStat stat;
			string path = string(dir) + string("/") + entry.name +
				      string("/sce_sys/package/comp.bin");
			int err;

			ret = LocalFile::Getstat(path.c_str(), &stat);
			path = path = string(dir) + string("/") + entry.name +
				      string("/sce_sys/package.bin");
			err = LocalFile::Getstat(path.c_str(), &stat);
			if (ret != 0 && err != 0)
				return 1;
		}
	} while (ret == 0);
	folder.Close();
	return 0;
}

/* the ICV files of an application's PFS that no file of the PFS image names any more */
static int gameRemoveStaleIcvFiles(const char *dir, const char *name, const void *keystone)
{
	string mountPoint;
	string device;
	string icvDir = string(dir) + string("/sce_pfs/icv.db");
	string path;
	Dir folder;
	DirEnt entry;
	common::SharedPtr<memory::PoolAllocator> pool;
	int ret;

	ret = gamePluginGetPool("CleanupFiles", &pool);
	if (ret < 0)
		return ret;

	SceSize size = pool->GetFreeSize();

	if (size > GAME_PFS_NIDS_SIZE)
		size = GAME_PFS_NIDS_SIZE;

	SceUInt32 *nids = (SceUInt32 *)pool->Alloc();

	if (nids == NULL) {
		ret = GAME_ERROR_NO_MEMORY;
	} else {
		ret = gamePfsMount(string(dir), string(name), keystone, &mountPoint);
		if (ret >= 0) {
			char drive[16];

			sce_paf_memset(drive, 0, sizeof(drive));
			ret = sceAppMgrGetPfsDrive(mountPoint.c_str(), drive);
			if (ret >= 0) {
				device = string(drive) + string("0:");
				sce_paf_memset(nids, 0, size);
				ret = sceIoDevctl(device.c_str(), GAME_PFS_DEVCTL_GET_NIDS, NULL, 0,
						  nids, size);
				if (ret >= 0) {
					SceUInt32 count = ret;
					int err;

					gameDebugDump("pfs nids", nids, count * sizeof(*nids));
					gamePfsUnmount(&mountPoint);
					mountPoint.clear();
					ret = folder.Open(icvDir.c_str());
					if (ret >= 0) {
						do {
							err = folder.Read(&entry);
							if (err != 0)
								break;
							if (entry.name.rfind(".icv", string::npos,
									     sce_paf_strlen(".icv")) !=
							    string::npos) {
								char hex[9];
								SceUInt32 nid;
								SceUInt32 i;

								sce_paf_strncpy(hex, entry.name.c_str(), 8);
								hex[8] = '\0';
								nid = sce_paf_strtol(hex, NULL, 16);
								for (i = 0; i < count; i++) {
									if (nids[i] == nid)
										break;
								}
								if (i == count) {
									path = icvDir + string("/") +
									       entry.name;
									err = LocalFile::RemoveFile(
										path.c_str());
								}
							}
						} while (err == 0);
						folder.Close();
						ret = 0;
					}
				}
			}
		}
	}
	if (mountPoint.length() != 0)
		gamePfsUnmount(&mountPoint);
	if (nids != NULL)
		pool->Free(nids);
	gamePluginReleasePool(&pool);
	return ret;
}

/* the entries of the split package's comp.bin, laid out from offset on */
static int gamePackageGetCompEntries(const GamePackageComp *comp, SceSize size,
				     GamePackageEntry *entries, SceUInt32 offset)
{
	SceUInt32 i;

	if (comp->header.magic != GAME_PACKAGE_COMP_MAGIC || comp->header.version != 0 ||
	    comp->header.count != GAME_PACKAGE_COMP_ENTRIES)
		return GAME_ERROR_INVALID_PACKAGE;
	for (i = 0; i < GAME_PACKAGE_COMP_ENTRIES; i++) {
		SceUInt32 entrySize;

		entries[i].reserved = 0;
		entries[i].index = i + GAME_PACKAGE_COMP_FIRST;
		entries[i].offset = offset;
		entrySize = comp->entry[i].size;
		entries[i].size = entrySize;
		offset += entrySize;
		if ((entrySize & (GAME_PACKAGE_ALIGN - 1)) != 0)
			offset += GAME_PACKAGE_ALIGN - (entrySize & (GAME_PACKAGE_ALIGN - 1));
	}
	return 0;
}

/*
 * The split package (dir: head.bin, body.bin, tail.bin and comp.bin) concatenated into one
 * package file at path, with its header and table of entries. A body.bin that is not there
 * (yet) is written as zeros, info's block count long.
 */
static int gamePackageConcatenate(const string &dir, const string &path,
				  const char *const *files, int count, const GamePackageInfo *info,
				  void *buf, SceSize bufSize)
{
	int ret = 0;
	common::SharedPtr<File> in;
	common::SharedPtr<File> out;
	char padding[GAME_PACKAGE_ALIGN];
	GamePackageHeader header;
	GamePackageEntry entries[GAME_PACKAGE_ENTRIES];
	SceUInt32 i;

	sce_paf_memset(padding, 0, sizeof(padding));
	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memset(entries, 0, sizeof(entries));

	SceUInt32 mod = 0;
	SceUInt32 offset = sizeof(header) + sizeof(entries);
	SceUInt32 dataOffset = offset;

	for (i = 0; i < GAME_PACKAGE_FILES; i++) {
		SceUInt32 start = offset;

		if (mod != 0)
			start = GAME_PACKAGE_ALIGN - mod + offset;

		string file = dir + string("/") + string(files[i]);
		int size = LocalFile::GetFileSize(file.c_str());

		if (size < 0) {
			ret = size;
			if (size != GAME_PAF_ERROR_NOT_FOUND ||
			    sce_paf_strncmp(files[i], "body.bin", sizeof("body.bin")) != 0)
				return ret;
			size = info->bodyBlocks * GAME_PACKAGE_BODY_BLOCK;
		}
		entries[i].index = i;
		offset = start + size;
		entries[i].reserved = 0;
		entries[i].offset = start;
		entries[i].size = size;
		mod = offset & (GAME_PACKAGE_ALIGN - 1);
	}

	out = LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);
	if (ret < 0)
		return ret;
	ret = out->Write(&header, sizeof(header));
	if (ret < 0 || (ret = out->Write(entries, sizeof(GamePackageEntry) * count)) < 0) {
		out->Close();
		return ret;
	}

	SceUInt32 pos = dataOffset;
	SceInt64 chunkSize = bufSize;

	mod = 0;
	for (i = 0; i < GAME_PACKAGE_FILES; i++) {
		if (mod != 0) {
			SceUInt32 pad = GAME_PACKAGE_ALIGN - mod;

			if (pad != 0) {
				ret = out->Write(padding, pad);
				if (ret < 0) {
					out->Close();
					if (in.get() != NULL)
						in->Close();
					return ret;
				}
				pos += pad;
			}
		}

		string file = dir + string("/") + string(files[i]);

		in = LocalFile::Open(file.c_str(), SCE_O_RDONLY, 0, &ret);
		if (ret >= 0) {
			SceInt64 remain = entries[i].size;
			SceInt64 chunk = chunkSize;

			while (remain > 0) {
				if (remain < chunkSize)
					chunk = remain;
				ret = in->Read(buf, chunk);
				if (ret < 0) {
					out->Close();
					in->Close();
					return ret;
				}
				ret = out->Write(buf, chunk);
				if (ret < 0) {
					out->Close();
					in->Close();
					return ret;
				}
				pos += chunk;
				remain -= chunk;
			}
		} else {
			if (ret != GAME_PAF_ERROR_NOT_FOUND ||
			    sce_paf_strncmp(files[i], "body.bin", sizeof("body.bin")) != 0) {
				out->Close();
				return ret;
			}

			SceInt64 chunk = chunkSize;
			SceInt64 remain = entries[i].size;

			sce_paf_memset(buf, 0, bufSize);
			while (remain > 0) {
				if (remain < chunkSize)
					chunk = remain;
				ret = out->Write(buf, chunk);
				if (ret < 0) {
					out->Close();
					in->Close();
					return ret;
				}
				pos += chunk;
				remain -= chunk;
			}
		}
		mod = pos & (GAME_PACKAGE_ALIGN - 1);
	}
	if ((pos & (GAME_PACKAGE_ALIGN - 1)) != 0) {
		SceUInt32 pad = GAME_PACKAGE_ALIGN - (pos & (GAME_PACKAGE_ALIGN - 1));

		if (pad != 0) {
			ret = out->Write(padding, pad);
			if (ret < 0) {
				out->Close();
				if (in.get() != NULL)
					in->Close();
				return ret;
			}
			pos += pad;
		}
	}

	string compPath = dir + string("/comp.bin");

	in = LocalFile::Open(compPath.c_str(), SCE_O_RDONLY, 0, &ret);
	if (ret < 0) {
		out->Close();
		return ret;
	}

	int compSize = in->GetFileSize();

	ret = compSize;
	if (compSize < 0) {
		in->Close();
		out->Close();
		return ret;
	}
	if ((SceSize)compSize > bufSize) {
		in->Close();
		out->Close();
		return GAME_ERROR_PACKAGE_TOO_LARGE;
	}
	ret = in->Read(buf, bufSize);
	if (ret < 0) {
		in->Close();
		out->Close();
		return ret;
	}
	in->Close();

	const GamePackageComp *comp = (const GamePackageComp *)buf;

	ret = gamePackageGetCompEntries(comp, bufSize, &entries[GAME_PACKAGE_FILES], pos);
	if (ret < 0) {
		out->Close();
		return ret;
	}
	ret = out->Write(comp + 1, comp->header.size - sizeof(*comp));
	if (ret < 0) {
		out->Close();
		return ret;
	}
	mod = (comp->header.size - sizeof(*comp)) & (GAME_PACKAGE_ALIGN - 1);
	if (mod != 0) {
		SceUInt32 pad = GAME_PACKAGE_ALIGN - mod;

		if (pad != 0) {
			ret = out->Write(padding, pad);
			if (ret < 0) {
				out->Close();
				return ret;
			}
		}
	}
	ret = out->Seek(0, SCE_SEEK_SET);
	if (ret < 0) {
		out->Close();
		return ret;
	}

	SceUInt32 size = entries[GAME_PACKAGE_ENTRIES - 1].offset +
			 entries[GAME_PACKAGE_ENTRIES - 1].size;

	if ((size & (GAME_PACKAGE_ALIGN - 1)) != 0)
		size += GAME_PACKAGE_ALIGN - (size & (GAME_PACKAGE_ALIGN - 1));
	header.magic = GAME_PACKAGE_MAGIC;
	header.version = 0;
	header.count = count;
	header.size = size;
	ret = out->Write(&header, sizeof(header));
	if (ret < 0) {
		out->Close();
		return ret;
	}
	ret = out->Write(entries, sizeof(entries));
	if (ret < 0) {
		out->Close();
		return ret;
	}
	out->Close();
	return 0;
}

/*
 * An application's split package put back together as sce_sys/package.bin: concatenated in
 * ur0:temp/promote, the split package removed, the result copied in and renamed.
 */
static int gamePackageRebuild(const char *appDir, const GamePackageInfo *info, void *buf,
			      SceSize bufSize)
{
	string from;
	string to;
	int ret;

	Dir::CreateRecursive("ur0:temp/promote", 6);

	string tempPath("ur0:temp/promote/__package.bin");

	LocalFile::RemoveFile(tempPath.c_str());

	string packageDir = string(appDir) + string("/sce_sys/package");

	ret = gamePackageConcatenate(packageDir, tempPath, s_gamePackageFiles, GAME_PACKAGE_ENTRIES,
				     info, buf, bufSize);
	if (ret >= 0) {
		from = string(appDir) + string("/sce_sys/package");
		to = string(appDir) + string("/sce_sys/__package");
		gameRemoveDirectory(to.c_str(), 0, 0, 0, true);
		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret >= 0) {
			ret = gameRemoveDirectory(to.c_str(), 0, 0, 0, true);
			if (ret >= 0) {
				from = string(appDir) + string("/sce_sys/__package.bin");
				ret = gameCopyFile(tempPath.c_str(), from.c_str(), 0, 0, buf,
						   bufSize, 0, 0);
				if (ret >= 0) {
					to = string(appDir) + string("/sce_sys/package.bin");
					ret = LocalFile::RenameFile(from.c_str(), to.c_str());
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}
	if (tempPath.length() != 0)
		LocalFile::RemoveFile(tempPath.c_str());
	return ret;
}

/* an application's package.bin rebuilt from its split package, with a block of the work pool */
static int gamePackageRepair(const char *appDir)
{
	int ret;

	if (appDir == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	common::SharedPtr<memory::PoolAllocator> pool;

	ret = gamePluginGetPool("CleanupFiles", &pool);
	if (ret < 0)
		return ret;

	SceSize size = pool->GetFreeSize();
	void *buf = pool->Alloc();

	if (buf == NULL) {
		ret = GAME_ERROR_NO_MEMORY;
	} else {
		GamePackageInfo info;

		sce_paf_memset(&info, 0, sizeof(info));

		string path = string(appDir) + string("/");

		ret = gamePackageReadInfo(&info, path.c_str(), buf, size);
		if (ret < 0)
			goto out;
		ret = gamePackageRebuild(appDir, &info, buf, size);
		if (ret < 0)
			goto out;
		ret = 0;
	}
out:
	if (buf != NULL)
		pool->Free(buf);
	gamePluginReleasePool(&pool);
	return ret;
}

/*
 * The package of an installation that did not finish, concatenated from all of its files into
 * path: a missing work.bin is taken from temp.bin, a missing body.bin written as zeros. Its
 * header and table are written first, with their final values.
 */
static int gamePackageConcatenateInstalling(const string &dir, const string &path,
					    const char *const *files, SceUInt32 count,
					    const GamePackageInfo *info, void *buf, SceSize bufSize)
{
	int ret = 0;
	common::SharedPtr<File> in;
	common::SharedPtr<File> out;
	char padding[GAME_PACKAGE_ALIGN];
	GamePackageHeader header;
	GamePackageEntry entries[count];
	SceUInt32 i;

	sce_paf_memset(padding, 0, sizeof(padding));

	SceUInt32 offset = sizeof(header) + sizeof(GamePackageEntry) * count;
	SceUInt32 mod = offset & (GAME_PACKAGE_ALIGN - 1);

	for (i = 0; i < count; i++) {
		if (mod != 0)
			offset += GAME_PACKAGE_ALIGN - mod;

		string file = dir + string("/") + string(files[i]);
		int size = LocalFile::GetFileSize(file.c_str());

		if (size == GAME_PAF_ERROR_NOT_FOUND) {
			ret = size;
			if (sce_paf_strncmp(files[i], "work.bin", sizeof("work.bin")) == 0) {
				file = dir + string("/temp.bin");
				size = LocalFile::GetFileSize(file.c_str());
				if (size < 0)
					return size;
			} else if (sce_paf_strncmp(files[i], "body.bin", sizeof("body.bin")) == 0) {
				size = info->bodyBlocks * GAME_PACKAGE_BODY_BLOCK;
			} else {
				return ret;
			}
		} else if (size < 0) {
			return size;
		}
		entries[i].offset = offset;
		offset += size;
		entries[i].index = i;
		entries[i].reserved = 0;
		entries[i].size = size;
		mod = offset & (GAME_PACKAGE_ALIGN - 1);
	}
	if (mod != 0)
		offset += GAME_PACKAGE_ALIGN - mod;

	header.magic = GAME_PACKAGE_MAGIC;
	header.version = 0;
	header.count = count;
	header.size = offset;

	out = LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);
	if (ret < 0)
		return ret;
	ret = out->Write(&header, sizeof(header));
	if (ret < 0) {
		out->Close();
		return ret;
	}
	ret = out->Write(entries, sizeof(GamePackageEntry) * count);
	if (ret < 0) {
		out->Close();
		return ret;
	}

	SceUInt32 pos = 0;
	SceInt64 chunkSize = bufSize;
	SceInt64 remain;
	SceInt64 chunk;

	mod = 0;
	for (i = 0; i < count; i++) {
		if (mod != 0) {
			SceUInt32 pad = GAME_PACKAGE_ALIGN - mod;

			if (pad != 0) {
				ret = out->Write(padding, pad);
				if (ret < 0) {
					out->Close();
					if (in.get() != NULL)
						in->Close();
					return ret;
				}
				pos += pad;
			}
		}

		string file = dir + string("/") + string(files[i]);

		in = LocalFile::Open(file.c_str(), SCE_O_RDONLY, 0, &ret);
		if (ret != GAME_PAF_ERROR_NOT_FOUND) {
			if (ret < 0) {
				out->Close();
				return ret;
			}
		} else if (sce_paf_strncmp(files[i], "work.bin", sizeof("work.bin")) == 0) {
			file = dir + string("/temp.bin");
			in = LocalFile::Open(file.c_str(), SCE_O_RDONLY, 0, &ret);
			if (ret < 0) {
				out->Close();
				return ret;
			}
		} else {
			if (sce_paf_strncmp(files[i], "body.bin", sizeof("body.bin")) != 0) {
				out->Close();
				return ret;
			}
			remain = entries[i].size;
			chunk = chunkSize;
			sce_paf_memset(buf, 0, bufSize);
			while (remain > 0) {
				if (remain < chunkSize)
					chunk = remain;
				ret = out->Write(buf, chunk);
				if (ret < 0) {
					out->Close();
					in->Close();
					return ret;
				}
				pos += chunk;
				remain -= chunk;
			}
			goto next;
		}

		remain = entries[i].size;
		chunk = chunkSize;
		while (remain > 0) {
			if (remain < chunkSize)
				chunk = remain;
			ret = in->Read(buf, chunk);
			if (ret < 0) {
				out->Close();
				in->Close();
				return ret;
			}
			ret = out->Write(buf, chunk);
			if (ret < 0) {
				out->Close();
				in->Close();
				return ret;
			}
			pos += chunk;
			remain -= chunk;
		}
next:
		mod = pos & (GAME_PACKAGE_ALIGN - 1);
	}
	if (mod != 0) {
		ret = out->Write(padding, GAME_PACKAGE_ALIGN - mod);
		if (ret < 0) {
			out->Close();
			if (in.get() != NULL)
				in->Close();
			return ret;
		}
	}
	out->Close();
	in->Close();
	return 0;
}

/* gamePackageRebuild for the package of an installation that did not finish */
static int gamePackageRebuildInstalling(const char *appDir, const GamePackageInfo *info,
					void *buf, SceSize bufSize)
{
	string from;
	string to;
	int ret;

	Dir::CreateRecursive("ur0:temp/promote", 6);

	string tempPath("ur0:temp/promote/__package.bin");

	LocalFile::RemoveFile(tempPath.c_str());

	string packageDir = string(appDir) + string("/sce_sys/package");

	ret = gamePackageConcatenateInstalling(packageDir, tempPath, s_gamePackageFiles,
					       GAME_PACKAGE_ENTRIES, info, buf, bufSize);
	if (ret >= 0) {
		from = string(appDir) + string("/sce_sys/package");
		to = string(appDir) + string("/sce_sys/__package");
		gameRemoveDirectory(to.c_str(), 0, 0, 0, true);
		ret = LocalFile::RenameFile(from.c_str(), to.c_str());
		if (ret >= 0) {
			ret = gameRemoveDirectory(to.c_str(), 0, 0, 0, true);
			if (ret >= 0) {
				from = string(appDir) + string("/sce_sys/__package.bin");
				ret = gameCopyFile(tempPath.c_str(), from.c_str(), 0, 0, buf,
						   bufSize, 0, 0);
				if (ret >= 0) {
					to = string(appDir) + string("/sce_sys/package.bin");
					ret = LocalFile::RenameFile(from.c_str(), to.c_str());
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}
	if (tempPath.length() != 0)
		LocalFile::RemoveFile(tempPath.c_str());
	return ret;
}

/* gamePackageRepair for the package of an installation that did not finish */
static int gamePackageRepairInstalling(const char *appDir)
{
	int ret;

	if (appDir == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	common::SharedPtr<memory::PoolAllocator> pool;

	ret = gamePluginGetPool("CleanupFiles", &pool);
	if (ret < 0)
		return ret;

	SceSize size = pool->GetFreeSize();
	void *buf = pool->Alloc();

	if (buf == NULL) {
		ret = GAME_ERROR_NO_MEMORY;
	} else {
		GamePackageInfo info;

		sce_paf_memset(&info, 0, sizeof(info));

		string path = string(appDir) + string("/");

		ret = gamePackageReadInfo(&info, path.c_str(), buf, size);
		if (ret < 0)
			goto out;
		ret = gamePackageRebuildInstalling(appDir, &info, buf, size);
		if (ret < 0)
			goto out;
		ret = 0;
	}
out:
	if (buf != NULL)
		pool->Free(buf);
	gamePluginReleasePool(&pool);
	return ret;
}

/*
 * What an interrupted installation left in an application's folder (path) cleaned up: stale
 * ICV files removed with the application's keystone (read from the installed application
 * when keystone is NULL), and every subfolder's split package concatenated back.
 */
int gameCleanupApp(const char *path, const LsdbAppInfoPtr *info, string *detail,
		   const void *keystone)
{
	int ret;
	char keystoneBuf[GAME_KEYSTONE_SIZE];

	if (path == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;
	ret = 0;
	if (!Dir::IsExist(path))
		return 0;

	string dir(path);
	string::size_type sep = dir.rfind("/", string::npos, sce_paf_strlen("/"));
	string name = dir.substr(sep + 1, dir.length() - sep);

	if (keystone == NULL) {
		int storage;

		ret = gameAppGetStorage(info, &storage);
		if (ret < 0)
			return ret;

		string appPath;

		if (storage == GAME_STORAGE_GRO0 || storage == GAME_STORAGE_GRO0_OTHER)
			appPath = string("gro0:app/") + name;
		else if (storage == GAME_STORAGE_UX0)
			appPath = string("ux0:app/") + name;
		else
			return GAME_ERROR_INVALID_ARGUMENT;

		string mountPoint;

		ret = gameAppMount(appPath.c_str(), "invalid:", 0, 0, &mountPoint);
		if (ret < 0)
			return ret;
		keystone = keystoneBuf;

		string keystonePath = appPath + string("/sce_sys/keystone");
		common::SharedPtr<File> file =
			LocalFile::Open(keystonePath.c_str(), SCE_O_RDONLY, 0, &ret);

		if (ret < 0) {
			gameAppUnmount(&mountPoint);
			return ret;
		}
		ret = file->Read(keystoneBuf, sizeof(keystoneBuf));
		if (ret != sizeof(keystoneBuf)) {
			gameAppUnmount(&mountPoint);
			file->Close();
			return ret;
		}
		file->Close();
		gameAppUnmount(&mountPoint);
	}

	if (gameHasBrokenPackage(path)) {
		ret = gameRemoveStaleIcvFiles(path, name.c_str(), keystone);
		if (ret < 0) {
			gameDebugDump("keystone", keystone, GAME_KEYSTONE_SIZE);
			return ret;
		}
	}

	Dir folder;

	ret = folder.Open(path);
	if (ret < 0)
		return ret;

	DirEnt entry;
	string appDir;
	string packagePath;

	for (;;) {
		ret = folder.Read(&entry);
		if (ret == 0 && entry.type == DirEnt::Type_Dir && entry.name != "sce_sys" &&
		    entry.name != "sce_pfs") {
			appDir = string(path) + string("/") + entry.name;
			packagePath = appDir + string("/sce_sys/package/comp.bin");

			LocalFileStat compStat;

			ret = LocalFile::Getstat(packagePath.c_str(), &compStat);
			if (ret == 0) {
				ret = gamePackageRepair(appDir.c_str());
				if (ret < 0) {
					folder.Close();
					return ret;
				}
			} else {
				if (ret != GAME_PAF_ERROR_NOT_FOUND) {
					folder.Close();
					return ret;
				}

				LocalFileStat stat;

				packagePath = appDir + string("/sce_sys/package.bin");
				ret = LocalFile::Getstat(packagePath.c_str(), &stat);
				if (ret == GAME_PAF_ERROR_NOT_FOUND) {
					ret = gamePackageRepairInstalling(appDir.c_str());
					if (ret < 0) {
						folder.Close();
						return ret;
					}
				}
			}
		}
		if (ret != 0)
			break;
	}
	folder.Close();
	return 0;
}

/* what interrupted installations of additional content left cleaned up (interface entry 38) */
int gameCleanupAddcont(void)
{
	vector<string> titles;
	int ret;
	SceUInt32 i;

	ret = gamePluginGetAddcontTitles(&titles);
	if (ret < 0)
		return ret;
	for (i = 0; i < titles.size(); i++) {
		gameRemoveDirectory("ux0:tem", 0, 0, 0, true);
		gameRemoveDirectory("grw0:tem", 0, 0, 0, true);
		gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
		gameRemoveDirectory("grw0:temp/promote", 0, 0, 0, true);

		LsdbAppInfoPtr info(new LsdbAppInfo);

		ret = gameAppGetInfo(titles[i].c_str(), &info);
		if (ret < 0 && ret != LSDB_ERROR_NOT_FOUND)
			return ret;

		string path = string("grw0:addcont/") + titles[i];

		ret = gameCleanupApp(path.c_str(), &info, NULL, NULL);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/*
 * What an interrupted installation left cleaned up: the application's folder (path), then,
 * for additional content, the package in appDir concatenated back. On failure, detail is
 * given what gamePathGetMountedDrive finds for the folder that failed.
 */
int gameCleanupInstall(const char *path, const SceNpDrmInstallInfo *package, const char *appDir,
		       const LsdbAppInfoPtr *info, string *detail)
{
	int ret;

	ret = gameCleanupApp(path, info, detail, package->keystone);
	if (ret < 0) {
		gamePathGetMountedDrive(string(path), detail);
		return ret;
	}

	string packagePath = string(appDir) + string("/sce_sys/package.bin");
	LocalFileStat stat;

	if (LocalFile::Getstat(packagePath.c_str(), &stat) != GAME_PAF_ERROR_NOT_FOUND)
		return 0;
	if (package != NULL && appDir != NULL) {
		if (package->info.contentType == GAME_CONTENT_TYPE_ADDCONT) {
			common::SharedPtr<memory::PoolAllocator> pool;

			ret = gamePluginGetPool("ConcatenateInstallingAc", &pool);
			if (ret >= 0) {
				SceSize size = pool->GetFreeSize();
				void *buf = pool->Alloc();

				if (buf == NULL) {
					ret = GAME_ERROR_NO_MEMORY;
				} else {
					ret = gamePackageRebuildInstalling(appDir, &package->info,
									   buf, size);
					if (ret >= 0)
						ret = 0;
				}
				if (buf != NULL)
					pool->Free(buf);
				gamePluginReleasePool(&pool);
			}
			if (ret < 0)
				goto fail;
		}
		return 0;
	}
	ret = GAME_ERROR_INVALID_ARGUMENT;
fail:
	gamePathGetMountedDrive(string(appDir), detail);
	return ret;
}
