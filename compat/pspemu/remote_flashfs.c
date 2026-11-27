/*
 * remote_flashfs.c: virtual flash0:/flash2: filesystem backed by the
 * decrypted PSP flash0 RAM image and SceCompat act.dat reader.
 */
#include "host_common.h"
#include <string.h>

#if FW_VERSION < 0x03650000
#define FLASHFS_IMAGE_SIZE          0x00922880U
#else
#define FLASHFS_IMAGE_SIZE          0x00922940U
#endif
#define FLASHFS_MIPS_IMAGE_BASE     0x8B000000U
#define FLASHFS_MAX_FILES           187
#define FLASHFS_MAX_DIRS            11
#define FLASHFS_MAX_OPEN_FILES      128
#define FLASHFS_MAX_OPEN_DIRS       32
#define FLASHFS_FD_BASE             100
#define FLASHFS_ACTDAT_FD           500
#define FLASHFS_ACTDAT_SIZE         4152U
#define FLASHFS_DFD_BASE            1100
#define FLASHFS_WHITELIST_COUNT     141
#define FLASHFS_INVALID_INDEX       187
#define FLASHFS_PRX_TRIGGER_INDEX   156

#define FLASHFS_ERROR_NOENT         ((int)0x80010002)
#define FLASHFS_ERROR_BADF          ((int)0x80010009)
#define FLASHFS_ERROR_EMFILE        ((int)0x80010010)
#define FLASHFS_ERROR_UNSUPPORTED   ((int)0x80020321)

typedef struct FlashImageEntry {
	uint32_t nameGuestAddr;
	uint32_t dataGuestAddr;
	uint32_t fileSize;
} FlashImageEntry;

typedef struct FlashFileEntry {
	const char *path;
	const uint8_t *data;
	uint32_t size;
} FlashFileEntry;

typedef struct FlashOpenFile {
	const FlashFileEntry *entry;
	int inUse;
	uint64_t offset;
} FlashOpenFile;

typedef struct FlashDirEntry {
	char path[24];
	uint32_t pathLen;
} FlashDirEntry;

typedef struct FlashOpenDir {
	const FlashDirEntry *dir;
	int nextFileIndex;
	int inUse;
	uint32_t prefixLen;
} FlashOpenDir;

static uint8_t g_flashAllowedIndices[FLASHFS_WHITELIST_COUNT] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 23, 24, 25, 26, 27, 30, 31, 34, 35, 60, 61, 62, 63, 64, 65, 66,
	67, 68, 69, 70, 71, 72, 73, 76, 78, 81, 83, 84, 85, 86, 87, 88, 89, 90,
	91, 92, 93, 94, 98, 100, 102, 103, 104, 105, 106, 107, 108, 109, 110,
	111, 112, 113, 114, 116, 117, 118, 122, 123, 124, 125, 126, 128, 129,
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158,
	159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
	173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186
};

int g_flashDirCount;
FlashFileEntry g_flashFiles[FLASHFS_MAX_FILES];
char g_flashPathScratch[256];
FlashOpenDir g_flashOpenDirs[FLASHFS_MAX_OPEN_DIRS];
uint8_t g_flashImageCopy[FLASHFS_IMAGE_SIZE];

uint8_t *g_flashGuestWork;
uint64_t g_flashActDatOffset;
int g_flashActDatOpen;
uint32_t g_flashActDatSize;
FlashDirEntry g_flashDirs[FLASHFS_MAX_DIRS];
FlashOpenFile g_flashOpenFiles[FLASHFS_MAX_OPEN_FILES];
uint8_t *g_flashImagePtr;

/* 0x8100b45c */
static int __attribute__((noinline, noclone)) flashfsCloseFileSlot(SceUID fd)
{
	unsigned int slot = (unsigned int)(fd - FLASHFS_FD_BASE);

	if (slot >= FLASHFS_MAX_OPEN_FILES)
		return FLASHFS_ERROR_BADF;
	if (g_flashOpenFiles[slot].entry == NULL || g_flashOpenFiles[slot].inUse != 1)
		return FLASHFS_ERROR_BADF;

	g_flashOpenFiles[slot].entry = NULL;
	g_flashOpenFiles[slot].inUse = 0;
	g_flashOpenFiles[slot].offset = 0;
	return 0;
}

/* 0x8100b494 */
static int flashfsInitOp(void *arg)
{
	(void)arg;
	return 0;
}

/* 0x8100b498 */
static int flashfsExitOp(void *arg)
{
	(void)arg;
	return 0;
}

/* 0x8100b49c */
static int flashfsClose(SceUID fd)
{
	if ((unsigned int)(fd - FLASHFS_FD_BASE) <= (FLASHFS_MAX_FILES - 1))
		return flashfsCloseFileSlot(fd);
	if (fd != FLASHFS_ACTDAT_FD || g_flashActDatOpen != 1)
		return FLASHFS_ERROR_BADF;

	g_flashActDatOpen = 0;
	g_flashActDatOffset = 0;
	return 0;
}

/* 0x8100b4d8 */
static SceSSize flashfsWrite(SceUID fd, const void *buf, SceSize size)
{
	(void)fd;
	(void)buf;
	(void)size;
	return FLASHFS_ERROR_BADF;
}

/* 0x8100b4e0 */
static int flashfsRemove(const char *path)
{
	(void)path;
	return FLASHFS_ERROR_BADF;
}

/* 0x8100b4e8 */
static int flashfsMkdir(const char *path, SceMode mode)
{
	(void)path;
	(void)mode;
	return FLASHFS_ERROR_BADF;
}

/* 0x8100b4f0 */
static int flashfsRmdir(const char *path)
{
	(void)path;
	return FLASHFS_ERROR_BADF;
}

/* 0x8100b4f8 */
static int flashfsDclose(SceUID dfd)
{
	unsigned int slot = (unsigned int)(dfd - FLASHFS_DFD_BASE);

	if (slot >= FLASHFS_MAX_DIRS)
		return FLASHFS_ERROR_BADF;
	if (g_flashOpenDirs[slot].dir == NULL || g_flashOpenDirs[slot].inUse != 1)
		return FLASHFS_ERROR_BADF;

	g_flashOpenDirs[slot].inUse = 0;
	g_flashOpenDirs[slot].nextFileIndex = 0;
	g_flashOpenDirs[slot].prefixLen = 0;
	return 0;
}

/* 0x8100b52c */
static int flashfsChstat(const char *path, const SceIoStat *stat, int bits)
{
	(void)path;
	(void)stat;
	(void)bits;
	return FLASHFS_ERROR_UNSUPPORTED;
}

/* 0x8100b538 */
static int flashfsRename(const char *oldPath, const char *newPath)
{
	(void)oldPath;
	(void)newPath;
	return FLASHFS_ERROR_UNSUPPORTED;
}

/* 0x8100b544 */
static int flashfsChdir(const char *path)
{
	(void)path;
	return FLASHFS_ERROR_UNSUPPORTED;
}

/* 0x8100b550 */
static int flashfsSyncToGuest(void)
{
	uint8_t *dst = g_flashGuestWork;

	memcpy(dst, g_flashImagePtr, FLASHFS_IMAGE_SIZE);
	if (pspemuIsCachedCdramAddress((uintptr_t)dst) != 0)
		sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, dst, FLASHFS_IMAGE_SIZE);
	return 0;
}

/* 0x8100b590 */
static int __attribute__((noinline, noclone)) flashfsParseDevice(const char *path)
{
	if (strncmp(path, "flash0:", 7) == 0)
		return 0;
	if (strncmp(path, "flash1:", 7) == 0)
		return 1;
	if (strncmp(path, "flash2:", 7) == 0)
		return 2;
	return -1;
}

static const PspemuFsOps g_flashfsOps;

/* 0x8100b610 */
static int flashfsThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	compatFsLoop(4, &g_flashfsOps);
}

/* 0x8100b624 */
static const FlashDirEntry *flashfsFindDir(const char *relPath)
{
	int index;

	for (index = 0; index < FLASHFS_MAX_DIRS; index++) {
		if (strncmp(g_flashDirs[index].path, relPath, 22) == 0)
			return &g_flashDirs[index];
	}
	return NULL;
}

/* 0x8100b650 */
static FlashImageEntry *flashfsFindImageEntry(const char *relPath)
{
	FlashImageEntry *entries = (FlashImageEntry *)(void *)g_flashImagePtr;
	int index;

	for (index = 0; index < FLASHFS_MAX_FILES; index++) {
		const char *entryName;

		if (entries[index].dataGuestAddr == 0)
			break;
		entryName = (const char *)(g_flashImagePtr + (entries[index].nameGuestAddr - FLASHFS_MIPS_IMAGE_BASE));
		if (strncmp(entryName, relPath, 256) == 0)
			return &entries[index];
	}
	return NULL;
}

/* 0x8100b694 */
static void flashfsSwapImageEntries(const char *firstPath, const char *secondPath)
{
	FlashImageEntry *second = flashfsFindImageEntry(secondPath);
	FlashImageEntry *first;

	if (second != NULL) {
		first = flashfsFindImageEntry(firstPath);
		if (first != NULL) {
			uint32_t savedName = second->nameGuestAddr;
			second->nameGuestAddr = first->nameGuestAddr;
			first->nameGuestAddr = savedName;
		}
	}
}

/* 0x8100b6b4 */
static SceUID flashfsDopen(const char *path)
{
	const FlashDirEntry *dir;
	size_t pathLen;
	int slot;

	if (flashfsParseDevice(path) != 0)
		return FLASHFS_ERROR_BADF;

	dir = flashfsFindDir(path + 7);
	if (dir == NULL || remotePeripheralIsBusy() == 0)
		return FLASHFS_ERROR_NOENT;

	pathLen = strlen(path);
	for (slot = 0; slot < FLASHFS_MAX_OPEN_DIRS; slot++) {
		if (g_flashOpenDirs[slot].inUse == 0) {
			g_flashOpenDirs[slot].dir = dir;
			g_flashOpenDirs[slot].nextFileIndex = 0;
			g_flashOpenDirs[slot].inUse = 1;
			g_flashOpenDirs[slot].prefixLen = (uint32_t)(pathLen - 7);
			return FLASHFS_DFD_BASE + slot;
		}
	}
	return FLASHFS_ERROR_EMFILE;
}

/* 0x8100b71c */
static SceSSize flashfsRead(SceUID fd, void *buf, SceSize size)
{
	int delayPerByte = pspemuCompatGetParam(23);

	if (delayPerByte > 0)
		sceKernelDelayThread((SceUInt)(size * (uint32_t)delayPerByte));

	if ((unsigned int)(fd - FLASHFS_FD_BASE) <= (FLASHFS_MAX_FILES - 1)) {
		unsigned int slot = (unsigned int)(fd - FLASHFS_FD_BASE);
		FlashOpenFile *file;
		uint32_t fileSize;

		if (slot >= FLASHFS_MAX_OPEN_FILES)
			return FLASHFS_ERROR_BADF;
		file = &g_flashOpenFiles[slot];
		if (file->entry == NULL || file->inUse != 1)
			return FLASHFS_ERROR_BADF;

		fileSize = file->entry->size;
		if (size > fileSize || (int64_t)(file->offset + size) > (int64_t)fileSize) {
			if ((int64_t)file->offset >= (int64_t)fileSize)
				return 0;
			size = (SceSize)(fileSize - (uint32_t)file->offset);
		}
		if (size == 0)
			return 0;

		memcpy(buf, file->entry->data + (uint32_t)file->offset, size);
		file->offset += size;
		return (SceSSize)size;
	}

	if (fd != FLASHFS_ACTDAT_FD || g_flashActDatOpen != 1)
		return FLASHFS_ERROR_BADF;

	if (size > g_flashActDatSize || (int64_t)(g_flashActDatOffset + size) > (int64_t)g_flashActDatSize) {
		if ((int64_t)g_flashActDatOffset >= (int64_t)g_flashActDatSize)
			return 0;
		size = (SceSize)(g_flashActDatSize - (uint32_t)g_flashActDatOffset);
	}
	if (size == 0)
		return 0;

	sceCompatDatRead(0, (unsigned int)g_flashActDatOffset, buf, size);
	g_flashActDatOffset += size;
	return (SceSSize)size;
}

/* 0x8100b858 */
static SceOff flashfsLseek(SceUID fd, SceOff offset, int whence)
{
	uint64_t basePos;
	uint64_t nextPos;
	uint32_t maxSize;

	if ((unsigned int)(fd - FLASHFS_FD_BASE) <= (FLASHFS_MAX_FILES - 1)) {
		unsigned int slot = (unsigned int)(fd - FLASHFS_FD_BASE);
		FlashOpenFile *file;

		if (slot >= FLASHFS_MAX_OPEN_FILES)
			return FLASHFS_ERROR_BADF;
		file = &g_flashOpenFiles[slot];
		if (file->entry == NULL || file->inUse != 1)
			return FLASHFS_ERROR_BADF;

		maxSize = file->entry->size;
		switch (whence) {
		case SCE_SEEK_SET:
			basePos = 0;
			break;
		case SCE_SEEK_CUR:
			basePos = file->offset;
			break;
		case SCE_SEEK_END:
			basePos = maxSize;
			break;
		default:
			return PSPEMU_ERROR_INVALID_ARG;
		}

		nextPos = basePos + (uint64_t)offset;
		if ((offset < 0 && nextPos > basePos) ||
		    (offset > 0 && nextPos < basePos) ||
		    nextPos > (uint64_t)maxSize) {
			return PSPEMU_ERROR_INVALID_ARG;
		}

		file->offset = nextPos;
		return (SceOff)nextPos;
	}

	if (fd != FLASHFS_ACTDAT_FD || g_flashActDatOpen != 1)
		return FLASHFS_ERROR_BADF;

	maxSize = g_flashActDatSize;
	switch (whence) {
	case SCE_SEEK_SET:
		basePos = 0;
		break;
	case SCE_SEEK_CUR:
		basePos = g_flashActDatOffset;
		break;
	case SCE_SEEK_END:
		basePos = maxSize;
		break;
	default:
		return PSPEMU_ERROR_INVALID_ARG;
	}

	nextPos = basePos + (uint64_t)offset;
	if ((offset < 0 && nextPos > basePos) ||
	    (offset > 0 && nextPos < basePos) ||
	    nextPos > (uint64_t)maxSize) {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	g_flashActDatOffset = nextPos;
	return (SceOff)nextPos;
}

/* 0x8100b990 */
static const FlashFileEntry *flashfsLookupFile(const char *relPath)
{
	int fileIndex;
	int allowIndex;

	for (fileIndex = 0; fileIndex < FLASHFS_MAX_FILES; fileIndex++) {
		if (g_flashFiles[fileIndex].data == NULL)
			return NULL;
		if (strncmp(g_flashFiles[fileIndex].path, relPath, 256) == 0)
			break;
	}
	if (fileIndex == FLASHFS_MAX_FILES)
		return NULL;

	for (allowIndex = 0; allowIndex < FLASHFS_WHITELIST_COUNT; allowIndex++) {
		if (g_flashAllowedIndices[allowIndex] == (uint8_t)fileIndex)
			return &g_flashFiles[fileIndex];
	}

	pspemuRecordTitleListFlags(1);
	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static const SceDateTime g_flashDefaultTime = { 2010, 10, 7, 15, 30, 28, 0 };

/* 0x8100b9f8 */
static int __attribute__((noinline, noclone)) flashfsFillStat(const char *path, SceIoStat *stat)
{
	const FlashFileEntry *entry;

	memset(stat, 0, sizeof(*stat));
	entry = flashfsLookupFile(path + 7);
	if (entry == NULL)
		return FLASHFS_ERROR_NOENT;

	stat->st_mode = 0x11FF;
	stat->st_attr = 0;
	stat->st_size = entry->size;
	stat->st_ctime = g_flashDefaultTime;
	stat->st_atime = g_flashDefaultTime;
	stat->st_mtime = g_flashDefaultTime;
	return 0;
}

/* 0x8100ba6c */
static int flashfsGetstat(const char *path, SceIoStat *stat)
{
	int dev = flashfsParseDevice(path);

	if (dev == 0)
		return flashfsFillStat(path, stat);

	if (dev == 2) {
		if (strncmp(path, "flash2:/act.dat", 15) != 0)
			return FLASHFS_ERROR_NOENT;

		memset(stat, 0, sizeof(*stat));
		stat->st_mode = 0x11FF;
		stat->st_size = FLASHFS_ACTDAT_SIZE;
		stat->st_ctime = g_flashDefaultTime;
		stat->st_atime = g_flashDefaultTime;
		stat->st_mtime = g_flashDefaultTime;
		return 0;
	}

	return FLASHFS_ERROR_UNSUPPORTED;
}

/* 0x8100bb54 */
static int flashfsDread(SceUID dfd, SceIoDirent *dir)
{
	unsigned int slot = (unsigned int)(dfd - FLASHFS_DFD_BASE);
	FlashOpenDir *openDir;
	int fileIndex;

	if (dir != NULL)
		memset(dir, 0, sizeof(*dir));

	if (slot >= FLASHFS_MAX_DIRS)
		return FLASHFS_ERROR_BADF;

	openDir = &g_flashOpenDirs[slot];
	if (openDir->dir == NULL || openDir->inUse != 1) {
		dir->d_name[0] = '\0';
		return FLASHFS_ERROR_BADF;
	}

	for (fileIndex = openDir->nextFileIndex; fileIndex < FLASHFS_MAX_FILES; fileIndex++) {
		const char *entryPath = g_flashFiles[fileIndex].path;
		uint32_t dirLen = openDir->dir->pathLen;
		uint32_t pos;

		if (g_flashFiles[fileIndex].data == NULL)
			break;
		if (memcmp(entryPath, openDir->dir->path, dirLen) != 0 || dirLen > 255U)
			continue;

		for (pos = dirLen; pos < 256U; pos++) {
			if (entryPath[pos] == '\0') {
				flashfsFillStat(entryPath, &dir->d_stat);
				strncpy(dir->d_name, entryPath + openDir->prefixLen, sizeof(dir->d_name));
				openDir->nextFileIndex = fileIndex + 1;
				return fileIndex + 1;
			}
			if (entryPath[pos] == '/')
				break;
		}
	}

	dir->d_name[0] = '\0';
	return 0;
}

/* 0x8100bc34 */
static SceUID flashfsOpen(const char *path, int flags, SceMode mode)
{
	int dev = flashfsParseDevice(path);
	const FlashFileEntry *entry;
	int slot;

	(void)flags;
	(void)mode;

	if (dev == 0) {
		entry = flashfsLookupFile(path + 7);
		if (entry == NULL)
			return FLASHFS_ERROR_NOENT;

		if (entry == &g_flashFiles[FLASHFS_PRX_TRIGGER_INDEX])
			remoteMsfsSetBootAllowed(1);

		for (slot = 0; slot < FLASHFS_MAX_OPEN_FILES; slot++) {
			if (g_flashOpenFiles[slot].inUse == 0) {
				g_flashOpenFiles[slot].entry = entry;
				g_flashOpenFiles[slot].inUse = 1;
				g_flashOpenFiles[slot].offset = 0;
				return FLASHFS_FD_BASE + slot;
			}
		}
		return FLASHFS_ERROR_EMFILE;
	}

	if (dev == 2 && strncmp(path, "flash2:/act.dat", 15) == 0) {
		if (g_flashActDatOpen != 0)
			return FLASHFS_ERROR_EMFILE;

		g_flashActDatOpen = 1;
		g_flashActDatSize = FLASHFS_ACTDAT_SIZE;
		g_flashActDatOffset = 0;
		return FLASHFS_ACTDAT_FD;
	}

	return FLASHFS_ERROR_NOENT;
}

/* 0x8100bd28 */
int remoteFlashfsClearGuest(void)
{
	uint8_t *dst = g_flashGuestWork;

	memset(dst, 0, FLASHFS_IMAGE_SIZE);
	if (pspemuIsCachedCdramAddress((uintptr_t)dst) != 0)
		sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, dst, FLASHFS_IMAGE_SIZE);
	return 0;
}

/* 0x8100bd60 */
int remoteFlashfsReset(void)
{
	int index;

	for (index = FLASHFS_FD_BASE; index < FLASHFS_FD_BASE + FLASHFS_MAX_OPEN_FILES; index++)
		flashfsCloseFileSlot(index);

	for (index = 0; index < FLASHFS_MAX_OPEN_DIRS; index++) {
		if (g_flashOpenDirs[index].dir != NULL && g_flashOpenDirs[index].inUse == 1) {
			g_flashOpenDirs[index].inUse = 0;
			g_flashOpenDirs[index].nextFileIndex = 0;
			g_flashOpenDirs[index].prefixLen = 0;
		}
	}

	if (g_flashActDatOpen == 1) {
		g_flashActDatOpen = 0;
		g_flashActDatOffset = 0;
	}

	flashfsSyncToGuest();
	return 0;
}

static inline void flashfsRevokeAllowedIndex(uint8_t fileIndex)
{
	int pos;

	for (pos = 0; pos < FLASHFS_WHITELIST_COUNT; pos++) {
		if (g_flashAllowedIndices[pos] == fileIndex) {
			g_flashAllowedIndices[pos] = FLASHFS_INVALID_INDEX;
			return;
		}
	}
}

/* 0x8100bdc0 */
int remoteFlashfsInit(void *workBuf)
{
	static const uint8_t restrictedIndices[] = {
		25, 34, 35, 64, 65, 67, 70, 71, 72, 73,
		86, 87, 88, 89, 90, 91, 92, 93, 94,
		102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
		136, 138, 141, 142, 143, 144,
		151, 152, 153, 154, 155, 156, 157, 158,
		161, 165, 166, 167,
		122, 123, 124, 125, 126,
		140, 160, 164
	};
	FlashImageEntry *imageEntries;
	int swapFlags;
	int index;
	SceUID threadId;

	g_flashGuestWork = (uint8_t *)workBuf;
	g_flashImagePtr = g_flashImageCopy;
	memcpy(g_flashImageCopy, workBuf, FLASHFS_IMAGE_SIZE);
	memset(g_flashFiles, 0, sizeof(g_flashFiles));
	memset(g_flashDirs, 0, sizeof(g_flashDirs));

	if (pspemuIsPs1Mode() != 1)
		flashfsRevokeAllowedIndex(98);

	if (pspemuCompatIsRestrictedBootTitle() != 0) {
		size_t revIdx;

		for (revIdx = 0; revIdx < sizeof(restrictedIndices); revIdx++)
			flashfsRevokeAllowedIndex(restrictedIndices[revIdx]);
	}

	swapFlags = pspemuCompatGetParam(14);
	if (swapFlags != -1) {
		if ((swapFlags & 1) != 0)
			flashfsSwapImageEntries("/kd/ge.prx", "/kd/kermit_peripheral.prx");
		if ((swapFlags & 2) != 0)
			flashfsSwapImageEntries("/kd/ge.prx", "/kd/ge_2.prx");
		if ((swapFlags & 4) != 0)
			flashfsSwapImageEntries("/kd/kermit_me_wrapper.prx", "/kd/kermit_me_wrapper_2.prx");
	}

	imageEntries = (FlashImageEntry *)(void *)g_flashImagePtr;
	for (index = 0; index < FLASHFS_MAX_FILES; index++) {
		if (imageEntries[index].dataGuestAddr == 0)
			break;

		g_flashFiles[index].path = (const char *)(g_flashImagePtr + (imageEntries[index].nameGuestAddr - FLASHFS_MIPS_IMAGE_BASE));
		g_flashFiles[index].data = g_flashImagePtr + (imageEntries[index].dataGuestAddr - FLASHFS_MIPS_IMAGE_BASE);
		g_flashFiles[index].size = imageEntries[index].fileSize;

		if (g_flashDirCount < FLASHFS_MAX_DIRS) {
			int pos = 0;

			strncpy(g_flashPathScratch, g_flashFiles[index].path, sizeof(g_flashPathScratch));
			while (pos <= 255 && g_flashPathScratch[pos] != '\0') {
				if (g_flashPathScratch[pos] == '/') {
					char savedChar = g_flashPathScratch[pos + 1];

					g_flashPathScratch[pos + 1] = '\0';
					if (flashfsFindDir(g_flashPathScratch) == NULL) {
						size_t dirLen = strlen(g_flashPathScratch);

						g_flashDirs[g_flashDirCount].pathLen = (uint32_t)dirLen;
						strncpy(g_flashDirs[g_flashDirCount].path, g_flashPathScratch, dirLen);
						g_flashDirCount++;
					}
					g_flashPathScratch[pos + 1] = savedChar;
					if (g_flashDirCount >= FLASHFS_MAX_DIRS)
						break;
				}
				pos++;
			}
		}
	}

	flashfsSyncToGuest();
	memset(g_flashOpenFiles, 0, sizeof(g_flashOpenFiles));
	memset(g_flashOpenDirs, 0, sizeof(g_flashOpenDirs));
	g_flashActDatOffset = 0;
	g_flashActDatOpen = 0;
	g_flashActDatSize = 0;

	threadId = sceKernelCreateThread(
		"ScePspemuRemoteFlashfs",
		flashfsThread,
		96,
		0x1000,
		0,
		SCE_KERNEL_CPU_MASK_USER_0,
		NULL);
	if (threadId < 0)
		return threadId;
	return sceKernelStartThread(threadId, 0, NULL);
}

static const PspemuFsOps g_flashfsOps = {
	.init = flashfsInitOp,
	.exit = flashfsExitOp,
	.open = flashfsOpen,
	.close = flashfsClose,
	.read = flashfsRead,
	.write = flashfsWrite,
	.lseek = flashfsLseek,
	.ioctl = NULL,
	.remove = flashfsRemove,
	.mkdir = flashfsMkdir,
	.rmdir = flashfsRmdir,
	.dopen = flashfsDopen,
	.dclose = flashfsDclose,
	.dread = flashfsDread,
	.getstat = flashfsGetstat,
	.chstat = flashfsChstat,
	.rename = flashfsRename,
	.chdir = flashfsChdir,
	.devctl = NULL,
	.postOp = NULL,
};
