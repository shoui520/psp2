/*
 * reg_registry.c: the reg library ("SceRegBlockAPI"): the registries open
 * from their .ireg/.dreg files. Every entry takes the library mutex and
 * holds off suspend while it touches the files.
 * All names unknown unless noted.
 */

#include <iofilemgr/iofilemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/utils.h>
#include "reg_internal.h"
#include <sysmem/suspend.h>

#define REG_COPY_SIZE	0x2800	/* the buffer a registry file is copied through */
#define REG_ZERO_SIZE	0x4000	/* the zeroes new registry files are written with */

SceUID g_regMutex;
int g_regRegistryOpened;
int g_regInitialized;
RegRegistry *g_regRegistryList;
RegRegistry *g_regRegistryTail;
SceSha1Context g_regSha1Context;		/* the digest of an export or import */
RegExportHeader g_regImportHeader;
int g_regRegistryNum;
RegExportHeader g_regExportHeader;

/* bring the library up: its lists, its mutex and its heap */
int regInit(void)
{
	SceUID mutex;
	int state;

	state = regPlatformLock();
	g_regRegistryOpened = 0;
	g_regCategoryTail = NULL;
	g_regInitialized = 0;
	g_regRegistryNum = 0;
	g_regRegistryList = NULL;
	g_regRegistryTail = NULL;
	g_regCategoryNum = 0;
	g_regCategoryList = NULL;
	g_regMutex = -1;

	mutex = sceKernelCreateMutex("SceRegBlockAPI", 0, 0, NULL);
	if (mutex < 0) {
		regPlatformUnlock(state);
		return REG_ERROR_MUTEX;
	}
	g_regMutex = mutex;

	if (regMemInit(0, 0x10000) <= 0) {
		sceKernelDeleteMutex(g_regMutex);
		regPlatformUnlock(state);
		return REG_ERROR_HEAP;
	}

	regPlatformInit();
	g_regInitialized = 1;
	regPlatformUnlock(state);

	return 0;
}

/* remove the files of the registry param names */
int regRemoveRegistry(void *param)
{
	const char *index;
	const char *data;
	int state;
	int ret;

	state = regPlatformLock();
	if (regPlatformCheckPointer(param) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}

	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	SceKernelSuspendForDriver_4DF40893(0);
	switch (*(SceUInt32 *)param & ~0x80) {
	case 1:
		index = "/registry/system.ireg";
		data = "/registry/system.dreg";
		break;
	case 2:
		index = "/registry/backup.ireg";
		data = "/registry/backup.dreg";
		break;
	case 3:
		index = "/registry/temp.ireg";
		data = "/registry/temp.dreg";
		break;
	default:
		index = NULL;
		data = NULL;
		break;
	}
	regFileRemove(index);
	regFileRemove(data);
	SceKernelSuspendForDriver_2BB92967(0);

	ret = sceKernelUnlockMutex(g_regMutex, 1) == 0 ? 0 : REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* write a modified registry's index image back, with its digest renewed */
int regFlushIndex(int registry)
{
	SceUInt32 digest[5] = { 0 };
	RegIndexImage *image;
	RegRegistry *reg;
	int written;
	SceUID fd;
	int size;
	int ret;
	int i;

	SceKernelSuspendForDriver_4DF40893(0);
	reg = regGetRegistry(registry);
	if (reg == NULL) {
		ret = REG_ERROR_NOT_FOUND;
		goto out;
	}

	ret = 0;
	fd = -1;
	if (regIsRegistryModified(registry) == 0) {
		image = reg->image;
		size = image->categorySize + image->bitmapSize + REG_INDEX_HEADER_SIZE;
		memset(image->digest, 0, sizeof(image->digest));
		ret = SceKernelUtilsForDriver_87DC7F2F(image, size, digest);
		if (ret >= 0) {
			for (i = 0; i < 5; i++)
				image->digest[i] = digest[i];
			fd = regOpenRegistryFile(reg, REG_FILE_INDEX);
			if (fd == -1) {
				ret = REG_ERROR_NOT_FOUND;
			} else {
				ret = regFileSeek(fd, 0, SCE_SEEK_SET);
				if (ret == 0) {
					ret = regFileWrite(image, 1, size, fd, &written);
					if (ret == 0)
						regClearRegistryModified(registry);
				}
			}
		}
	}
	regCloseRegistryFile(reg, fd, REG_FILE_INDEX);

out:
	SceKernelSuspendForDriver_2BB92967(0);

	return ret;
}

/* load a registry from its files, or count one more open of it */
int regLoadRegistry(const SceUInt32 *param, const char *indexPath, const char *dataPath,
		    int *registry)
{
	SceUInt32 digest[5] = { 0 };
	SceUInt32 saved[5];
	SceUInt32 size[2];
	RegCategoryEntry *entry;
	RegIndexImage *image;
	RegRegistry *reg;
	RegRegistry *r;
	SceUID indexFd;
	SceUID dataFd;
	RegRegistry *next;
	SceUInt32 n;
	SceUInt32 i;
	int index;
	int len;
	int ret;

	if (g_regRegistryList != NULL) {
		reg = g_regRegistryList;
		for (index = 0; index < g_regRegistryNum; index++) {
			if (reg->id == *param) {
				*registry = index;
				break;
			}
			reg = reg->next;
		}
		if (index >= g_regRegistryNum)
			reg = NULL;
		/* as the original: a registry not found is not caught */
		reg->openCount++;
		return 0;
	}

	if (regFileGetSize(indexPath, size) != 0)
		return REG_ERROR_INDEX_SIZE;

	image = regMemAlloc(size[0]);
	if (image == NULL)
		return REG_ERROR_NO_MEMORY_HEAP;

	reg = NULL;
	indexFd = regFileOpen(indexPath);
	ret = indexFd;
	if (indexFd < 0)
		goto free;

	dataFd = 0;
	reg = regMemAlloc(sizeof(*reg));
	if (reg == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto close;
	}

	memset(reg, 0, sizeof(*reg));
	if (regFileRead(image, 1, size[0], indexFd, (int *)&size[0]) != 0) {
		ret = REG_ERROR_COPY_READ;
		goto close;
	}

	for (i = 0; i < 5; i++)
		saved[i] = image->digest[i];
	memset(image->digest, 0, sizeof(image->digest));
	ret = SceKernelUtilsForDriver_87DC7F2F(image, size[0], digest);
	if (ret < 0)
		goto close;
	if (memcmp(digest, saved, sizeof(digest)) != 0) {
		ret = REG_ERROR_BAD_DIGEST;
		goto close;
	}
	for (i = 0; i < 5; i++)
		image->digest[i] = saved[i];

	entry = (RegCategoryEntry *)((char *)image + image->categoryOffset);
	n = image->categorySize / sizeof(RegCategoryEntry);
	for (i = 0; i < n; i++)
		entry[i].cache = NULL;

	dataFd = regFileOpen(dataPath);
	ret = dataFd;
	if (dataFd < 0) {
		dataFd = 0;
		goto close;
	}

	reg->state = 0;
	reg->modified = 0;
	reg->openCount = 0;
	reg->image = image;
	reg->next = NULL;
	reg->fd[REG_FILE_INDEX] = indexFd;
	reg->fd[REG_FILE_DATA] = dataFd;
	reg->id = *param;
	reg->imageSize = size[0];
	reg->dataSize = image->dataSize;

	len = strnlen(indexPath, REG_PATH_LEN);
	if (strncmp(indexPath, dataPath, len - 5) != 0) {
		ret = REG_ERROR_NOT_FOUND;
		goto close;
	}
	strncpy(reg->path, indexPath, len - 5);
	reg->path[len - 5] = '\0';

	/* take the place of a closed registry, or join the end of the list */
	index = g_regRegistryNum;
	if (g_regRegistryNum >= 1) {
		r = g_regRegistryList;
		for (i = 0; i < g_regRegistryNum; i++) {
			if (r->state != 0)
				break;
			r = r->next;
		}
		if (i < g_regRegistryNum) {
			next = r->next;
			memcpy(r, reg, sizeof(*reg));
			r->next = next;
			index = i;
			goto added;
		}
	}
	if (g_regRegistryList != NULL)
		g_regRegistryTail->next = reg;
	else
		g_regRegistryList = reg;
	g_regRegistryNum++;
	g_regRegistryTail = reg;

added:
	*registry = index;
	image = reg->image;
	if (image->version == REG_INDEX_VERSION_OLD) {
		entry = (RegCategoryEntry *)((char *)image + image->categoryOffset);
		n = image->categorySize / sizeof(RegCategoryEntry);
		for (i = 0; i < n; i++) {
			if (entry[i].name[0] != '\0' && entry[i].parent == 0)
				entry[i].parent = REG_CATEGORY_NONE;
		}
		image->version = REG_INDEX_VERSION;
		regSetRegistryModified(index);
		regFlushIndex(*registry);
	}

	return 0;

close:
	if (indexFd != 0)
		regFileClose(indexFd);
	if (dataFd != 0)
		regFileClose(dataFd);
free:
	memset(image, 0, size[0]);
	regMemFree(image);
	if (reg != NULL) {
		memset(reg, 0, sizeof(*reg));
		regMemFree(reg);
	}

	return ret;
}

/* lay out new, empty registry files: the index header, bitmap and category table,
 * then a zeroed data file; 0 when both are made */
static inline __attribute__((always_inline)) int regMakeRegistryFiles(const RegMgrRegistryParam *param,
						const char *index, const char *data)
{
	RegCategoryEntry template;
	RegIndexImage header;
	RegCategoryEntry *table;
	RegIndexImage *image;
	SceUInt32 digest[5];
	SceUInt32 total;
	SceUInt32 left;
	SceUInt32 n;
	int written;
	int headerWritten;
	SceUID fd;
	void *buf;
	int err;
	int ret;
	int i;

	memset(&header, 0, sizeof(header));
	header.version = REG_INDEX_VERSION;
	header.magic[1] = 'I';
	header.magic[2] = 'R';
	header.magic[3] = 'F';
	header.dataSize = param->size << 17;
	header.blockSize = param->blockSize << 9;
	header.categoryNum = header.dataSize / header.blockSize;
	header.freeBlockNum = header.categoryNum;
	header.bitmapSize = header.categoryNum >> 3;
	header.bitmapOffset = sizeof(header);
	header.categorySize = header.categoryNum * sizeof(RegCategoryEntry);
	header.categoryOffset = header.bitmapSize + sizeof(header);

	fd = regFileCreate(index);
	if (fd < 0)
		return REG_ERROR_CREATE;

	ret = 0;
	err = regFileWrite(&header, 1, sizeof(header), fd, &headerWritten);
	if (err != 0)
		goto close;

	buf = regMemAlloc(REG_ZERO_SIZE);
	if (buf == NULL) {
		err = REG_ERROR_NO_MEMORY_HEAP;
		goto close;
	}
	memset(buf, 0, REG_ZERO_SIZE);
	for (left = header.bitmapSize; left != 0; left -= written) {
		n = left < REG_ZERO_SIZE ? left : REG_ZERO_SIZE;
		err = regFileWrite(buf, 1, n, fd, &written);
		if (err != 0) {
			memset(buf, 0, REG_ZERO_SIZE);
			regMemFree(buf);
			goto close;
		}
	}
	memset(buf, 0, REG_ZERO_SIZE);
	regMemFree(buf);

	memset(&template, 0, sizeof(template));
	template.slot = REG_CATEGORY_NONE;
	memset(template.block, 0xff, 9);
	table = regMemAlloc(header.categorySize);
	if (table == NULL) {
		err = REG_ERROR_NO_MEMORY_HEAP;
		goto close;
	}
	for (i = 0; i < header.categoryNum; i++)
		table[i] = template;
	err = regFileWrite(table, 1, header.categorySize, fd, &written);
	memset(table, 0, header.categorySize);
	regMemFree(table);
	if (err != 0)
		goto close;

	/* the digest of the whole index */
	total = header.bitmapSize + headerWritten + header.categorySize;
	image = regMemAlloc(total);
	if (image == NULL) {
		err = REG_ERROR_NO_MEMORY_HEAP;
		goto close;
	}
	ret = regFileSeek(fd, 0, SCE_SEEK_SET);
	if (ret != 0) {
		ret = REG_ERROR_COPY_READ;
	} else {
		ret = regFileRead(image, 1, total, fd, &written);
		if (ret == 0) {
			memset(header.digest, 0, sizeof(header.digest));
			ret = SceKernelUtilsForDriver_87DC7F2F(image, total, digest);
			if (ret >= 0) {
				for (i = 0; i < 5; i++)
					header.digest[i] = image->digest[i] = digest[i];
				ret = regFileSeek(fd, 0, SCE_SEEK_SET);
				if (ret == 0)
					ret = regFileWrite(image, 1, total, fd, &written);
			}
		}
	}
	memset(image, 0, total);
	regMemFree(image);
	if (ret != 0) {
		if (regFileClose(fd) == 0)
			return ret;
		err = REG_ERROR_COPY_CLOSE;
		goto close;
	}
	if (regFileClose(fd) != 0)
		return err;

	/* the data file, zeroed */
	fd = regFileCreate(data);
	if (fd < 0)
		return REG_ERROR_CREATE;
	buf = regMemAlloc(REG_ZERO_SIZE);
	if (buf == NULL) {
		regFileClose(fd);
		return REG_ERROR_NO_MEMORY_HEAP;
	}
	memset(buf, 0, REG_ZERO_SIZE);
	for (left = header.dataSize; left != 0; left -= written) {
		n = left < REG_ZERO_SIZE ? left : REG_ZERO_SIZE;
		ret = regFileWrite(buf, 1, n, fd, &written);
		if (ret != 0) {
			memset(buf, 0, REG_ZERO_SIZE);
			regMemFree(buf);
			regFileClose(fd);
			return ret;
		}
	}
	memset(buf, 0, REG_ZERO_SIZE);
	regMemFree(buf);
	if (regFileClose(fd) != 0)
		return REG_ERROR_COPY_CLOSE;

	return 0;

close:
	if (ret == 0)
		ret = err;
	regFileClose(fd);

	return ret;
}

/* open the registry param names, made first when missing with mode 1 */
int regOpenRegistry(void *param, int mode, int *handle)
{
	const RegMgrRegistryParam *p = param;
	SceUInt32 size[2];
	const char *index;
	const char *data;
	RegRegistry *reg;
	int registry;
	int state;
	int ret;

	registry = -1;
	state = regPlatformLock();
	if (regPlatformCheckPointer(param) == NULL || regPlatformCheckPointer(handle) == NULL) {
		regPlatformUnlock(state);
		return REG_ERROR_BAD_PATH;
	}
	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	switch (p->mode & ~REG_PARAM_CLOSE_FILES) {
	case 1:
		index = "/registry/system.ireg";
		data = "/registry/system.dreg";
		break;
	case 2:
		index = "/registry/backup.ireg";
		data = "/registry/backup.dreg";
		break;
	case 3:
		index = "/registry/temp.ireg";
		data = "/registry/temp.dreg";
		break;
	default:
		index = NULL;
		data = NULL;
		break;
	}

	regMakeDirectory();
	if (regFileGetSize("/registry", size) != 0) {
		ret = REG_ERROR_NO_DIRECTORY;
		goto unlock;
	}
	if (regFileGetSize(index, size) == 0 && regFileGetSize(data, size) == 0)
		ret = 0;
	else
		ret = REG_ERROR_NO_FILES;

	if (mode == 1) {
		if (ret != 0) {
			SceKernelSuspendForDriver_4DF40893(0);
			ret = regMakeRegistryFiles(p, index, data);
			if (ret != 0) {
				regFileRemove(index);
				regFileRemove(data);
				SceKernelSuspendForDriver_2BB92967(0);
				goto unlock;
			}
			SceKernelSuspendForDriver_2BB92967(0);
			g_regRegistryOpened = 1;
		}
	} else if (mode == 2) {
		if (ret != 0)
			goto unlock;
	} else {
		ret = REG_ERROR_BAD_OPEN;
		goto unlock;
	}

	ret = regLoadRegistry(param, index, data, &registry);
	if (ret == 0)
		*handle = registry;

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	if ((p->mode & REG_PARAM_CLOSE_FILES) && registry != -1 && g_regRegistryList != NULL) {
		reg = regGetRegistry(registry);
		if (reg != NULL) {
			regFileClose(reg->fd[REG_FILE_INDEX]);
			regFileClose(reg->fd[REG_FILE_DATA]);
			reg->fd[REG_FILE_INDEX] = -1;
			reg->fd[REG_FILE_DATA] = -1;
		}
	}
	regPlatformUnlock(state);

	return ret;
}

/* close a registry: with its last open, its categories and files go */
int regCloseRegistry(int registry)
{
	RegCategoryEntry *entry;
	RegCategory *next;
	RegCategory *cat;
	RegRegistry *prev;
	RegRegistry *reg;
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	if (regGetRegistry(registry) == NULL) {
		ret = REG_ERROR_NOT_FOUND;
		goto unlock;
	}

	ret = regFlushIndex(registry);
	if (ret != 0)
		goto unlock;

	prev = NULL;
	reg = g_regRegistryList;
	for (i = 0; i < g_regRegistryNum; i++) {
		if (i == registry)
			break;
		prev = reg;
		reg = reg->next;
	}

	if (reg->openCount != 0) {
		reg->openCount--;
		goto unlock;
	}

	if (g_regCategoryList != NULL) {
		cat = g_regCategoryList;
		for (i = 0; i < g_regCategoryNum; i++) {
			entry = (RegCategoryEntry *)cat->handle;
			next = cat->next;
			if (entry->cache != NULL && entry->cache->data != NULL) {
				memset(entry->cache->data, 0, entry->blockNum * REG_BLOCK_SIZE);
				regMemFree(entry->cache->data);
			}
			if (entry->cache != NULL) {
				memset(entry->cache, 0, sizeof(*entry->cache));
				regMemFree(entry->cache);
				entry->cache = NULL;
			}
			memset(cat, 0, sizeof(*cat));
			regMemFree(cat);
			cat = next;
		}
	}
	g_regCategoryTail = NULL;
	g_regRegistryOpened = 0;
	g_regCategoryNum = 0;
	g_regCategoryList = NULL;

	reg->state = 1;
	for (i = 0; i < 2; i++) {
		if (reg->fd[i] == -1)
			continue;
		ret = regFileClose(reg->fd[i]);
		if (ret != 0)
			goto unlock;
		reg->fd[i] = -1;
	}

	if (--g_regRegistryNum == 0)
		g_regRegistryList = NULL;
	else if (prev != NULL)
		prev->next = reg->next;
	else
		g_regRegistryList = reg->next;

	memset(reg->image, 0, reg->imageSize);
	regMemFree(reg->image);
	memset(reg, 0, sizeof(*reg));
	regMemFree(reg);

unlock:
	if (sceKernelUnlockMutex(g_regMutex, 1) != 0)
		ret = REG_ERROR_UNLOCK;
	regPlatformUnlock(state);

	return ret;
}

/* write a modified category's data back, its blocks in runs */
int regFlushCategory(int handle)
{
	RegBlockRun run[REG_CATEGORY_BLOCK_MAX - 1] = { { 0 } };
	RegCategoryEntry *entry;
	RegRegistry *reg;
	SceUInt8 *data;
	int written;
	int runNum;
	int start;
	SceUID fd;
	int len;
	int off;
	int ret;
	int i;

	SceKernelSuspendForDriver_4DF40893(0);
	if (regFindCategory(handle) == 0) {
		ret = REG_ERROR_NOT_FOUND;
		goto out;
	}

	ret = 0;
	if (regIsCategoryModified(handle) != 0)
		goto out;

	entry = (RegCategoryEntry *)handle;
	reg = entry->registry;
	if (entry->cache == NULL) {
		ret = REG_ERROR_NO_CACHE;
		if (reg == NULL)
			goto out;
		fd = -1;
		goto close;
	}

	data = (SceUInt8 *)entry->cache->data;
	fd = regOpenRegistryFile(reg, REG_FILE_DATA);
	if (fd == -1) {
		ret = REG_ERROR_NOT_FOUND;
		goto close;
	}

	ret = regFileSeek(fd, 0, SCE_SEEK_SET);
	if (ret != 0)
		goto close;

	runNum = 0;
	start = 0;
	len = 1;
	for (i = 0; i < REG_CATEGORY_BLOCK_MAX - 1; i++) {
		if (entry->block[i + 1] == entry->block[i] + 1) {
			len++;
			continue;
		}
		if (entry->block[start] != REG_CATEGORY_NONE) {
			run[runNum].block = entry->block[start];
			run[runNum].num = len;
			runNum++;
		}
		start += len;
		len = 1;
	}

	off = 0;
	for (i = 0; i < runNum; i++) {
		ret = regFileSeek(fd, run[i].block * REG_BLOCK_SIZE, SCE_SEEK_SET);
		if (ret != 0)
			goto close;
		ret = regFileWrite(data + off * REG_BLOCK_SIZE, 1, run[i].num * REG_BLOCK_SIZE, fd,
				   &written);
		if (ret != 0)
			goto close;
		off += run[i].num;
	}
	regClearCategoryModified(handle);

close:
	regCloseRegistryFile(reg, fd, REG_FILE_DATA);
out:
	SceKernelSuspendForDriver_2BB92967(0);

	return ret;
}

/* the registry by its number */
RegRegistry *regGetRegistry(int registry)
{
	RegRegistry *reg;
	int i;

	reg = g_regRegistryList;
	if (reg == NULL)
		return NULL;

	for (i = 0; i < g_regRegistryNum; i++) {
		if (i == registry)
			return reg;
		reg = reg->next;
	}

	return NULL;
}

/* copy the system registry files over the backup ones */
int regBackupRegistry(void)
{
	const char *path[4] = { NULL };
	SceUID src;
	SceUID dst;
	void *buf;
	int locked;
	int state;
	int ret;
	int len;
	int i;

	ret = 0;
	locked = 0;
	state = regPlatformLock();
	ret = sceKernelLockMutex(g_regMutex, 1, NULL);
	if (ret != 0) {
		locked = -1;
		ret = REG_ERROR_LOCK;
		goto out;
	}

	SceKernelSuspendForDriver_4DF40893(0);
	path[0] = "/registry/system.ireg";
	path[1] = "/registry/system.dreg";
	path[2] = "/registry/backup.ireg";
	path[3] = "/registry/backup.dreg";

	buf = regMemAlloc(REG_COPY_SIZE);
	if (buf == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto out;
	}

	for (i = 0; i < 2; i++) {
		src = regFileOpen(path[i]);
		ret = src;
		if (src < 0)
			break;

		dst = regFileCreate(path[i + 2]);
		ret = dst;
		if (dst < 0) {
			dst = -1;
			goto close;
		}

		do {
			regFileRead(buf, 1, REG_COPY_SIZE, src, &ret);
			len = ret;
			if (ret < 0) {
				ret = REG_ERROR_COPY_READ;
				goto close;
			}
			regFileWrite(buf, 1, len, dst, &ret);
			if (ret < 0 || ret != len) {
				ret = REG_ERROR_COPY_WRITE;
				goto close;
			}
		} while (ret >= REG_COPY_SIZE);

		ret = regFileClose(src);
		if (ret != 0) {
			ret = REG_ERROR_COPY_CLOSE;
			goto close;
		}
		if (regFileClose(dst) != 0) {
			ret = REG_ERROR_COPY_CLOSE;
			src = -1;
			goto close;
		}
		ret = 0;
		continue;

close:
		if (src != -1)
			regFileClose(src);
		if (dst != -1)
			regFileClose(dst);
		break;
	}

	memset(buf, 0, REG_COPY_SIZE);
	regMemFree(buf);

out:
	SceKernelSuspendForDriver_2BB92967(0);
	if (locked != -1)
		sceKernelUnlockMutex(g_regMutex, 1);
	regPlatformUnlock(state);

	return ret;
}

/* put the backup registry files in place of the system ones */
int regRestoreRegistry(void)
{
	const char *path[6] = { NULL };
	int renamed[2] = { 0 };
	int state;
	int ret;
	int i;

	state = regPlatformLock();
	if (sceKernelLockMutex(g_regMutex, 1, NULL) != 0) {
		regPlatformUnlock(state);
		return REG_ERROR_LOCK;
	}

	SceKernelSuspendForDriver_4DF40893(0);
	path[0] = "/registry/system.ireg";
	path[1] = "/registry/system.dreg";
	path[2] = "/registry/backup.ireg";
	path[3] = "/registry/backup.dreg";
	path[4] = "/registry/temp.ireg";
	path[5] = "/registry/temp.dreg";

	for (i = 0; i < 2; i++) {
		regFileRemove(path[i + 4]);
		if (regFileRename(path[i], path[i + 4]) == 0)
			renamed[i]++;
		if (regFileRename(path[i + 2], path[i]) != 0) {
			if (renamed[0] != 0) {
				regFileRemove(path[0]);
				regFileRename(path[4], path[0]);
			}
			if (renamed[1] != 0) {
				regFileRemove(path[1]);
				regFileRename(path[5], path[1]);
			}
			ret = REG_ERROR_RESTORE;
			goto out;
		}
	}
	regFileRemove("/registry/temp.ireg");
	regFileRemove("/registry/temp.dreg");
	ret = 0;

out:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelUnlockMutex(g_regMutex, 1);
	regPlatformUnlock(state);

	return ret;
}

static const char g_regExportMagic[12] = "*DATA-BASE*";

/* write the backup registry files to path, behind a header with their digest */
int regExportRegistry(const char *path)
{
	const char *src[3] = { NULL };
	SceUID out;
	SceUID in;
	void *buf;
	int locked;
	int block;
	int state;
	int ret;
	int len;
	int i;

	ret = 0;
	state = regPlatformLock();
	ret = sceKernelLockMutex(g_regMutex, 1, NULL);
	if (ret != 0) {
		locked = -1;
		ret = REG_ERROR_LOCK;
		goto out;
	}
	locked = 0;
	if (regPlatformCheckPointer(path) == NULL) {
		ret = REG_ERROR_BAD_PATH;
		goto out;
	}

	SceKernelSuspendForDriver_4DF40893(0);
	src[0] = "/registry/backup.ireg";
	src[1] = "/registry/backup.dreg";
	buf = regMemAlloc(REG_COPY_SIZE);
	if (buf == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto out;
	}

	out = regFileCreateFullPath(path);
	ret = out;
	if (out < 0)
		goto free;

	memset(&g_regExportHeader, 0, sizeof(g_regExportHeader));
	g_regExportHeader.headerSize = sizeof(g_regExportHeader);
	memcpy(g_regExportHeader.magic, g_regExportMagic, sizeof(g_regExportMagic));
	regFileWrite(&g_regExportHeader, 1, sizeof(g_regExportHeader), out, &ret);
	if (ret != sizeof(g_regExportHeader)) {
		ret = REG_ERROR_COPY_WRITE;
		goto close_out;
	}

	for (i = 0; i < 2; i++) {
		in = regFileOpen(src[i]);
		ret = in;
		if (in < 0)
			goto close_out;

		for (block = 0;; block++) {
			regFileRead(buf, 1, REG_COPY_SIZE, in, &ret);
			len = ret;
			if (ret < 0) {
				ret = REG_ERROR_COPY_READ;
				goto close_in;
			}
			regFileWrite(buf, 1, len, out, &ret);
			if (ret < 0 || ret != len) {
				ret = REG_ERROR_COPY_WRITE;
				goto close_in;
			}
			g_regExportHeader.fileSize[i] += ret;
			if (block == 0) {
				ret = SceKernelUtilsForDriver_E4390FFA(&g_regSha1Context);
				if (ret < 0)
					goto close_in;
			}
			ret = SceKernelUtilsForDriver_478A6F3C(&g_regSha1Context, buf, len);
			if (ret < 0)
				goto close_in;
			if (len < REG_COPY_SIZE)
				break;
		}

		ret = regFileClose(in);
		if (ret != 0) {
			ret = REG_ERROR_COPY_CLOSE;
			goto close_in;
		}
	}

	ret = SceKernelUtilsForDriver_48F24106(&g_regSha1Context, g_regExportHeader.digest);
	if (ret >= 0) {
		ret = regFileSeek(out, 0, SCE_SEEK_SET);
		if (ret == 0) {
			regFileWrite(&g_regExportHeader, 1, sizeof(g_regExportHeader), out, &ret);
			if (ret != sizeof(g_regExportHeader)) {
				ret = REG_ERROR_COPY_WRITE;
			} else {
				ret = 0;
				regFileRemove("/registry/backup.ireg");
				regFileRemove("/registry/backup.dreg");
			}
		}
	}
	goto close_out;

close_in:
	if (in != -1)
		regFileClose(in);
close_out:
	if (out != -1)
		regFileClose(out);
free:
	memset(buf, 0, REG_COPY_SIZE);
	regMemFree(buf);
out:
	SceKernelSuspendForDriver_2BB92967(0);
	if (locked != -1)
		sceKernelUnlockMutex(g_regMutex, 1);
	regPlatformUnlock(state);

	return ret;
}

/* read an exported registry from path into the backup registry files */
int regImportRegistry(const char *path)
{
	SceUInt8 digest[0x14];
	const char *dst[2] = { NULL };
	void *buf = NULL;
	SceUID out;
	SceUID in;
	int locked;
	int block;
	int state;
	int left;
	int ret;
	int got;
	int i;

	ret = 0;
	state = regPlatformLock();
	ret = sceKernelLockMutex(g_regMutex, 1, NULL);
	if (ret != 0) {
		locked = -1;
		ret = REG_ERROR_LOCK;
		goto out;
	}
	locked = 0;
	if (regPlatformCheckPointer(path) == NULL) {
		ret = REG_ERROR_BAD_PATH;
		goto out;
	}

	SceKernelSuspendForDriver_4DF40893(0);
	dst[0] = "/registry/backup.ireg";
	dst[1] = "/registry/backup.dreg";
	buf = regMemAlloc(REG_COPY_SIZE);
	if (buf == NULL) {
		ret = REG_ERROR_NO_MEMORY_HEAP;
		goto out;
	}

	in = regFileOpenFullPath(path);
	ret = in;
	if (in < 0)
		goto out;

	regFileRead(&g_regImportHeader, 1, sizeof(g_regImportHeader), in, &ret);
	if (ret != sizeof(g_regImportHeader)) {
		ret = REG_ERROR_COPY_READ;
		goto close_in;
	}
	if (g_regImportHeader.headerSize != sizeof(g_regImportHeader) ||
	    memcmp(&g_regImportHeader, g_regExportMagic, sizeof(g_regExportMagic)) != 0) {
		ret = REG_ERROR_BAD_EXPORT;
		goto close_in;
	}

	for (i = 0; i < 2; i++) {
		left = g_regImportHeader.fileSize[i];
		if (left <= 0) {
			ret = REG_ERROR_BAD_EXPORT;
			goto close_in;
		}

		out = regFileCreate(dst[i]);
		ret = out;
		if (out < 0)
			goto close_in;

		for (block = 0;; block++) {
			regFileRead(buf, 1, left < REG_COPY_SIZE ? left : REG_COPY_SIZE, in, &ret);
			got = ret;
			if (ret < 0) {
				ret = REG_ERROR_COPY_READ;
				goto close_out;
			}
			regFileWrite(buf, 1, got, out, &ret);
			if (ret < 0 || ret != got) {
				ret = REG_ERROR_COPY_WRITE;
				goto close_out;
			}
			left -= ret;
			if (block == 0) {
				ret = SceKernelUtilsForDriver_E4390FFA(&g_regSha1Context);
				if (ret < 0)
					goto close_out;
			}
			ret = SceKernelUtilsForDriver_478A6F3C(&g_regSha1Context, buf, got);
			if (ret < 0)
				goto close_out;
			if (left == 0)
				break;
			if (left < 0) {
				ret = REG_ERROR_COPY_READ;
				goto close_out;
			}
		}

		ret = regFileClose(out);
		if (ret != 0) {
			ret = REG_ERROR_COPY_CLOSE;
			goto close_out;
		}
	}

	ret = SceKernelUtilsForDriver_48F24106(&g_regSha1Context, digest);
	if (ret >= 0)
		ret = memcmp(g_regImportHeader.digest, digest, sizeof(digest)) == 0 ?
		      0 : REG_ERROR_EXPORT_DIGEST;
	goto close_in;

close_out:
	if (out != -1)
		regFileClose(out);
close_in:
	if (in != -1)
		regFileClose(in);
out:
	if (ret != 0) {
		regFileRemove(dst[0]);
		regFileRemove(dst[1]);
	}
	if (buf != NULL) {
		memset(buf, 0, REG_COPY_SIZE);
		regMemFree(buf);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	if (locked != -1)
		sceKernelUnlockMutex(g_regMutex, 1);
	regPlatformUnlock(state);

	return ret;
}

/* the registry's file, opened for the call when the registry has it closed */
SceUID regOpenRegistryFile(RegRegistry *reg, int file)
{
	SceUID fd;
	int len;

	fd = reg->fd[file];
	if (fd != -1)
		return fd;

	len = strnlen(reg->path, REG_PATH_LEN);
	strncat(reg->path, file == REG_FILE_DATA ? ".dreg" : ".ireg", REG_PATH_LEN - 1 - len);
	fd = regFileOpen(reg->path);
	reg->path[len] = '\0';

	return fd < 0 ? -1 : fd;
}

/* close a file regOpenRegistryFile opened for the call */
void regCloseRegistryFile(RegRegistry *reg, SceUID fd, int file)
{
	if (reg != NULL && reg->fd[file] == -1)
		regFileClose(fd);
}
