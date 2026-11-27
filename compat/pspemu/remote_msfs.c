/*
 * remote_msfs.c: Kermit Memory Stick (ms0:) remote filesystem server,
 * path normalization, music0: virtual directory, app-param parsing,
 * write/truncate tracking, and wildcard directory filters for ScePspemu.
 */
#include "host_common.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MSFS_CHANNEL_ID             3
#define MSFS_VIRTUAL_MUSIC_DFD      4096
#define MSFS_FAKE_PBP_WRITE_FD      4
#define MSFS_MAX_WRITE_TRACKED      16
#define MSFS_MAX_TRUNC_TRACKED      16
#define MSFS_MAX_MUSIC_ENTRIES      1024
#define MSFS_MAX_DIR_FILTERS        32
#define MSFS_MAX_UNIQUE_HASHES      256
#define MSFS_MAX_PATH_COMPONENTS    1024
#define MSFS_DEVCTL_GET_DEVINFO     0x02425818U
#define MSFS_IOCTL_SET_DIR_FILTER   0x02415050U
#define MSFS_MIN_FREE_SPACE_MARGIN  0x001A8000ULL

int g_compatMsfsSeekFlag;

typedef struct MsfsWriteTrack {
	SceUID fd;
	int    hasWritten;
} MsfsWriteTrack;

typedef struct MsfsTruncTrack {
	SceUID fd;
	char   path[256];
	int    pad;
	SceOff maxSize;
} MsfsTruncTrack;

typedef struct MsfsMusicEntry {
	char        subDir[4];
	char        origName[256];
	int         origNameSum;
	int         displayNameSum;
	int         pad;
	SceIoDirent dirent;
} MsfsMusicEntry;

typedef struct MsfsDirFilter {
	SceUID dfd;
	char   pattern[256];
} MsfsDirFilter;

typedef struct MsfsDevctlInfo {
	uint32_t maxClusters;
	uint32_t freeClusters;
	uint32_t freeClustersCopy;
	uint32_t sectorSize;
	uint32_t sectorsPerCluster;
} MsfsDevctlInfo;

typedef struct MsfsDirentPrivate {
	uint32_t size;
	char     shortName[16];
	char     longName[1024];
} MsfsDirentPrivate;

/* Initialized module state */
int  g_msfsPendingWriteOpenSlot = -1;
char g_msfsMusicSubdirPath[12] = "music0:  ";
int  g_msfsPendingWriteCloseSlot = -1;

/* Shared paths exported via host_common.h */
char g_msfsRootPrefix[16];
char g_originalPath[1024];
char g_selfPath[1024];

/* BSS state */
int                  g_msfsMusicCount;
char                 g_msfsTempDirPath[256];
SceIoDevInfo         g_msfsCachedDevInfo;
int                  g_msfsFakePbpWritePending;
SceUID               g_msfsDeferredCloseFd;
SceCesUcsProfileSheet g_msfsSjisProfileSheet;
SceUID               g_msfsCachedEbootFd;
MsfsMusicEntry       g_msfsMusicEntries[MSFS_MAX_MUSIC_ENTRIES];
MsfsWriteTrack       g_msfsWriteTracked[MSFS_MAX_WRITE_TRACKED];
char                 g_msfsWritePaths[MSFS_MAX_WRITE_TRACKED][256];
int                  g_msfsUniqueHashCount;
char                 g_msfsPbootSuffix[12];
char                 g_msfsEbootSuffix[12];
int                  g_msfsReadDelayMs;
int                  g_msfsMusicRootKind;
int                  g_msfsMusicDreadDotState;
char                 g_msfsDeleteDirPath[256];
char                 g_msfsHiddenEbootName[16];
MsfsTruncTrack       g_msfsTruncTracked[MSFS_MAX_TRUNC_TRACKED];
char                 g_msfsAppParamBuf[1024];
int                  g_msfsSkipSizeCheckForKernel;
char                 g_msfsWriteLogPath[256];
char                 g_msfsRenameBasename[256];
int                  g_msfsDevInfoValid;
int                  g_msfsBootWriteAllowed;
char                 g_msfsScratchPath[256];
char                 g_msfsNormPathBuf[4096];
char                 g_msfsResolvedPathBuf[4096];
int                  g_msfsMusicScanned;
int                  g_msfsMaxUniqueHashes;
char                 g_msfsOldTempDirPath[256];
char                 g_msfsUtf8ConvBuf[4096];
char                *g_msfsPathComponents[MSFS_MAX_PATH_COMPONENTS];
int                  g_msfsMaxOpenFileSize;
char                 g_msfsHiddenMenuInfoName[16];
char                 g_msfsRenameDstPathBuf[4096];
int                  g_msfsCacheDevInfo;
uint8_t              g_msfsUniqueHashes[MSFS_MAX_UNIQUE_HASHES][16];
int                  g_msfsMusicReadIndex;
int                  g_msfsDeferCloseEnabled;
int                  g_msfsFileCacheEnabled;
char                 g_msfsRenameParentDir[4096];
int                  g_msfsAllowMusicRead;
MsfsDirFilter        g_msfsDirFilters[MSFS_MAX_DIR_FILTERS];

static int msfsDirFilterInit(void);
static int msfsDirFilterRegister(SceUID dfd, const char *pattern);
static int msfsDirFilterRemove(SceUID dfd);
static int msfsDirFilterMatch(SceUID dfd, const char *name);
static int msfsCloseInternal(SceUID fd);

static int msfsInitOp(void *arg);
static int msfsExit(void *arg);
static SceUID msfsOpen(const char *path, int flags, SceMode mode);
static int msfsClose(SceUID fd);
static SceSSize msfsRead(SceUID fd, void *buf, SceSize size);
static SceSSize msfsWrite(SceUID fd, const void *buf, SceSize size);
static SceOff msfsLseek(SceUID fd, SceOff offset, int whence);
static int msfsIoctl(SceUID fd, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen);
static int msfsRemove(const char *path);
static int msfsMkdir(const char *path, SceMode mode);
static int msfsRmdir(const char *path);
static SceUID msfsDopen(const char *path);
static int msfsDclose(SceUID dfd);
static int msfsDread(SceUID dfd, SceIoDirent *dir);
static int msfsGetstat(const char *path, SceIoStat *stat);
static int msfsChstat(const char *path, const SceIoStat *stat, int bits);
static int msfsRename(const char *oldPath, const char *newPath);
static int msfsChdir(const char *path);
static int msfsDevctl(const char *dev, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen);
static int msfsPostOp(void);

const PspemuFsOps g_msfsOps = {
	msfsInitOp,
	msfsExit,
	msfsOpen,
	msfsClose,
	msfsRead,
	msfsWrite,
	msfsLseek,
	msfsIoctl,
	msfsRemove,
	msfsMkdir,
	msfsRmdir,
	msfsDopen,
	msfsDclose,
	msfsDread,
	msfsGetstat,
	msfsChstat,
	msfsRename,
	msfsChdir,
	msfsDevctl,
	msfsPostOp
};

static SceMode __attribute__((noinline, noclone)) msfsPspModeToVitaMode(SceMode mode)
{
	SceMode base = (mode & 0xF000) | 0x10000;
	if ((mode & 0x92) == 0)
		return base | 0x104;
	return base | 0x186;
}

static SceMode __attribute__((noinline, noclone)) msfsVitaModeToPspMode(SceMode mode)
{
	return (mode & 0xF000) | (((mode & 0x82) == 0) ? 0x16D : 0x1FF);
}

static unsigned int __attribute__((noinline, noclone)) msfsVitaModeToPspAttr(SceMode mode)
{
	if ((mode & 0x1000) != 0)
		return 0x10U;
	if ((mode & 0x2000) == 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return 0U;
}

static int __attribute__((noinline, noclone)) msfsPspFlagsToVitaFlags(int flags)
{
	return flags & 0x01000F03;
}

static int __attribute__((noinline, noclone)) msfsConvertStatLocalToUtc(SceIoStat *stat)
{
	remotefsConvertLocalToUtc(&stat->st_atime);
	remotefsConvertLocalToUtc(&stat->st_ctime);
	return remotefsConvertLocalToUtc(&stat->st_mtime);
}

static int __attribute__((noinline, noclone)) msfsConvertStatUtcToLocal(SceIoStat *stat)
{
	remotefsConvertUtcToLocal(&stat->st_atime);
	remotefsConvertUtcToLocal(&stat->st_ctime);
	return remotefsConvertUtcToLocal(&stat->st_mtime);
}

static int msfsExit(void *arg)
{
	(void)arg;
	return 0;
}

static int msfsChdir(const char *path)
{
	(void)path;
	return -1;
}

static int __attribute__((noinline, noclone)) msfsIsUx0PspemuPath(const char *path)
{
	char prefix[12];
	prefix[11] = '\0';
	memcpy(prefix, path, 11);
	return memcmp(prefix, "ux0:pspemu/", 12) == 0;
}

static char *msfsBuildTempMusicPath(char *outBuf, const char *path)
{
	char *slash = strrchr(path, '/');
	if (slash != NULL && (slash - path) > 4) {
		strncpy(g_msfsScratchPath, g_msfsTempDirPath, 0x100U);
		strncpy(&g_msfsScratchPath[15], slash, 0xF1U);
		return strncpy(outBuf, g_msfsScratchPath, 0x100U);
	}
	return slash;
}

static int __attribute__((noinline, noclone)) msfsNormalizeBackslashes(char *path)
{
	int len = (int)strlen(path);
	for (int i = 0; i < len; i++) {
		if (path[i] == '\\')
			path[i] = '/';
	}
	return len;
}

static int __attribute__((noinline, noclone)) msfsGetAppParamString(const char *key, char *outBuf, size_t outSize)
{
	size_t keyLen = strlen(key);
	int pos = 0;

	for (;;) {
		int endPos = pos;
		int atEnd = (g_msfsAppParamBuf[pos] == '\0');
		if (g_msfsAppParamBuf[pos] != '\0' && g_msfsAppParamBuf[pos] != '&') {
			while (++endPos != 1024) {
				char ch = g_msfsAppParamBuf[endPos];
				if (ch == '&' || ch == '\0')
					break;
			}
			if (endPos == 1024)
				return -1;
		}
		if (g_msfsAppParamBuf[pos + keyLen] == '=' &&
		    strncmp(key, &g_msfsAppParamBuf[pos], keyLen) == 0) {
			size_t valLen = (size_t)(endPos - (pos + (int)keyLen + 1));
			if (valLen >= outSize)
				valLen = outSize - 1;
			memcpy(outBuf, &g_msfsAppParamBuf[pos + keyLen + 1], valLen);
			outBuf[valLen] = '\0';
			return (int)valLen;
		}
		if (atEnd || endPos == 1023)
			return -1;
		pos = endPos + 1;
	}
}

static int __attribute__((noinline, noclone)) msfsGetAppParamInt(const char *key, int *outVal)
{
	char numBuf[16];
	int len = msfsGetAppParamString(key, numBuf, sizeof(numBuf));
	if (len >= 0) {
		int val = 0;
		for (int i = 0; i < len; i++)
			val = (val * 10) + (numBuf[i] - '0');
		*outVal = val;
	}
	return 0;
}

static int __attribute__((noinline, noclone)) msfsGetPocketStationAppParam(void)
{
	int val = 0;
	if (msfsGetAppParamInt("pocketstation", &val) >= 0)
		return val;
	return 0;
}

static int __attribute__((noinline, noclone)) msfsComparePathCaseInsensitive(const char *path, const char *refStr, int len)
{
	char foldBuf[20];
	int minCh;
	int maxCh;
	char delta;

	if (refStr[len - 2] <= 'Z') {
		minCh = 'a';
		maxCh = 'z';
		delta = (char)('A' - 'a');
	} else {
		minCh = 'A';
		maxCh = 'Z';
		delta = (char)('a' - 'A');
	}

	for (int i = 0; i < len; i++) {
		char ch = path[i];
		if (ch >= minCh && ch <= maxCh)
			foldBuf[i] = (char)(ch + delta);
		else if (ch == '\\')
			foldBuf[i] = '/';
		else
			foldBuf[i] = ch;
	}
	return memcmp(foldBuf, refStr, (size_t)len);
}

static int msfsDevctl(const char *dev, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen)
{
	SceIoDevInfo devInfo;
	int ret;

	(void)dev;
	(void)inData;
	(void)inLen;

	if (cmd != MSFS_DEVCTL_GET_DEVINFO || outLen != sizeof(MsfsDevctlInfo))
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (g_msfsCacheDevInfo != 0 && g_msfsDevInfoValid != 0) {
		devInfo = g_msfsCachedDevInfo;
		ret = 0;
	} else {
		ret = sceCompatGetDevInf(&devInfo);
		if (ret != 0)
			return ret;
	}

	MsfsDevctlInfo *outInfo = (MsfsDevctlInfo *)outData;
	uint32_t clusterSize = (uint32_t)devInfo.clusterSize;
	outInfo->maxClusters = (uint32_t)((uint64_t)devInfo.maxSize / (uint64_t)clusterSize);
	uint32_t freeClusters = (uint32_t)((uint64_t)devInfo.freeSize / (uint64_t)clusterSize);
	outInfo->freeClusters = freeClusters;
	outInfo->freeClustersCopy = freeClusters;
	outInfo->sectorSize = 512U;
	outInfo->sectorsPerCluster = clusterSize >> 9;
	return 0;
}

static void __attribute__((noinline, noclone)) msfsRemoveDirectoryContents(const char *rootPath)
{
	SceIoDirent dirent;

	strncpy(g_msfsDeleteDirPath, rootPath, sizeof(g_msfsDeleteDirPath));
	for (;;) {
		SceUID dfd = sceIoDopen(g_msfsDeleteDirPath);
		if (dfd < 0)
			break;

		int descended = 0;
		while (sceIoDread(dfd, &dirent) > 0) {
			SceMode type = dirent.d_stat.st_mode & 0xF000;
			if (type == 0x2000) {
				snprintf(g_msfsScratchPath, sizeof(g_msfsScratchPath), "%s/%s", g_msfsDeleteDirPath, dirent.d_name);
				sceIoRemove(g_msfsScratchPath);
			} else if (type == 0x1000 &&
			           memcmp(dirent.d_name, ".", 2) != 0 &&
			           memcmp(dirent.d_name, "..", 3) != 0) {
				snprintf(g_msfsScratchPath, sizeof(g_msfsScratchPath), "%s/%s", g_msfsDeleteDirPath, dirent.d_name);
				if (sceIoRmdir(g_msfsScratchPath) < 0) {
					sceIoDclose(dfd);
					strncpy(g_msfsDeleteDirPath, g_msfsScratchPath, sizeof(g_msfsDeleteDirPath));
					descended = 1;
					break;
				}
			}
		}

		if (descended)
			continue;

		sceIoDclose(dfd);
		if (strncmp(g_msfsDeleteDirPath, rootPath, sizeof(g_msfsDeleteDirPath)) == 0)
			break;
		char *slash = strrchr(g_msfsDeleteDirPath, '/');
		if (slash == NULL)
			break;
		*slash = '\0';
	}
}

static int msfsThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	compatFsLoop(MSFS_CHANNEL_ID, &g_msfsOps);
	return 0;
}

static int msfsDclose(SceUID dfd)
{
	msfsDirFilterRemove(dfd);
	if (dfd == MSFS_VIRTUAL_MUSIC_DFD)
		return 0;
	return sceIoDclose(dfd);
}

static int msfsIoctl(SceUID fd, unsigned int cmd, void *inData, SceSize inLen, void *outData, SceSize outLen)
{
	(void)outData;
	(void)outLen;

	if (cmd != MSFS_IOCTL_SET_DIR_FILTER || inLen != 4U)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	const char *pattern = (const char *)remotefsTranslateGuestAddr(*(const uint32_t *)inData, 1, 0x4000U);
	if (msfsDirFilterRegister(fd, pattern) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return 0;
}

static int msfsInitOp(void *arg)
{
	(void)arg;
	msfsDirFilterInit();
	return 0;
}

static int __attribute__((noinline, noclone)) msfsIsTruncTrackedFd(SceUID fd)
{
	for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
		if (g_msfsTruncTracked[i].fd == fd)
			return 1;
	}
	return 0;
}

static int __attribute__((noinline, noclone)) msfsCloseInternal(SceUID fd)
{
	SceIoStat stat;
	int ret = sceIoClose(fd);

	if (fd == g_msfsCachedEbootFd) {
		iocacheRequestClose();
		g_msfsCachedEbootFd = 0;
	} else if (g_msfsFileCacheEnabled != 0 && iocacheFileIsCached(fd) != 0) {
		iocacheFileClose(fd);
	}

	for (int i = 0; i < MSFS_MAX_WRITE_TRACKED; i++) {
		if (g_msfsWriteTracked[i].fd == fd) {
			if (g_msfsWriteTracked[i].hasWritten != 0) {
				remoteLowPriorityEnqueuePromote(g_msfsWritePaths[i]);
				if (g_msfsCacheDevInfo != 0)
					g_msfsDevInfoValid = (sceCompatGetDevInf(&g_msfsCachedDevInfo) >= 0);
			}
			memset(&g_msfsWriteTracked[i], 0, sizeof(g_msfsWriteTracked[i]));
			g_msfsPendingWriteCloseSlot = i;
			break;
		}
	}

	if (ret == 0 && fd != 0 && msfsIsTruncTrackedFd(fd) != 0) {
		MsfsTruncTrack *entry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == fd) {
				entry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (entry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		memset(&stat, 0, sizeof(stat));
		stat.st_size = entry->maxSize;
		if (sceIoChstat(entry->path, &stat, 4) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		MsfsTruncTrack *clearEntry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == fd) {
				clearEntry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (clearEntry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		memset(clearEntry, 0, sizeof(*clearEntry));
	}

	return ret;
}

static SceOff msfsLseek(SceUID fd, SceOff offset, int whence)
{
	int seekMode = pspemuCompatGetParam(9);
	if (seekMode == 0) {
		if (offset == 0x7C4C20)
			g_compatMsfsSeekFlag = 1;
		else if (offset == ((SceOff)565 << 9))
			g_compatMsfsSeekFlag = 0;
	} else if (seekMode == 1) {
		if (offset == 0xC96340)
			g_compatMsfsSeekFlag = 1;
		else if (offset == ((SceOff)567 << 9))
			g_compatMsfsSeekFlag = 0;
	}

	if (fd != 0 && msfsIsTruncTrackedFd(fd) != 0) {
		MsfsTruncTrack *entry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == fd) {
				entry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (entry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		if (whence == SCE_SEEK_END) {
			whence = SCE_SEEK_SET;
			offset += entry->maxSize;
			if (offset < 0)
				offset = 0;
		}
	}

	SceOff pos = sceIoLseek(fd, offset, whence);
	if (fd == g_msfsCachedEbootFd) {
		if (pos > 0)
			iocachePrefetchRange(pos, 0U);
	} else if (g_msfsFileCacheEnabled != 0 && iocacheFileIsCached(fd) != 0) {
		iocacheFileLseek(fd, offset, whence);
	}
	return pos;
}

static SceSSize msfsRead(SceUID fd, void *buf, SceSize size)
{
	if (g_msfsReadDelayMs != 0 && remoteNetIsThrottleActive() != 0 && fd == g_msfsCachedEbootFd)
		sceKernelDelayThread((SceUInt)(1000 * g_msfsReadDelayMs));

	if (fd != 0 && msfsIsTruncTrackedFd(fd) != 0) {
		MsfsTruncTrack *entry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == fd) {
				entry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (entry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		SceOff curPos = sceIoLseek(fd, 0, SCE_SEEK_CUR);
		if (curPos < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		if (curPos >= entry->maxSize)
			size = 0U;
		else if (curPos + (SceOff)size > entry->maxSize)
			size = (SceSize)(entry->maxSize - curPos);
	}

	if (fd == g_msfsCachedEbootFd) {
		SceOff curPos = sceIoLseek(fd, 0, SCE_SEEK_CUR);
		iocachePrefetchRange(curPos, size);
		return iocacheRead(fd, buf, size);
	}
	if (g_msfsFileCacheEnabled != 0 && iocacheFileIsCached(fd) != 0)
		return iocacheFileRead(fd, buf, size);
	return sceIoRead(fd, buf, size);
}

static void __attribute__((noinline, noclone)) msfsFormatMusicDisplayName(MsfsMusicEntry *entry, unsigned int dupIndex)
{
	if (dupIndex > 666U)
		return;

	strncpy(g_msfsScratchPath, entry->origName, sizeof(g_msfsScratchPath));
	char *dot = strrchr(g_msfsScratchPath, '.');
	if (dot == NULL)
		return;

	*dot = '\0';
	const char *ext = dot + 1;
	if (dupIndex != 0U)
		snprintf(entry->dirent.d_name, sizeof(entry->dirent.d_name), "%s(%d).%s", g_msfsScratchPath, dupIndex, ext);
	else
		snprintf(entry->dirent.d_name, sizeof(entry->dirent.d_name), "%s..%s", g_msfsScratchPath, ext);

	int sum = 0;
	for (const unsigned char *p = (const unsigned char *)entry->dirent.d_name; *p != 0; p++) {
		int ch = *p;
		if ((unsigned int)(ch - 'A') <= 25U)
			ch += 32;
		sum += ch;
	}
	entry->displayNameSum = sum;
}

static int __attribute__((noinline, noclone)) msfsScanMusicDir(SceUID rootDfd)
{
	SceIoDirent rootEntry;
	SceIoDirent fileEntry;
	int scannedSubdirs = 0;

	g_msfsMusicCount = 0;
	memset(&rootEntry, 0xFF, sizeof(rootEntry));
	memset(g_msfsMusicEntries, 0, sizeof(g_msfsMusicEntries));

	while (sceIoDread(rootDfd, &rootEntry) > 0) {
		unsigned char firstCh = (unsigned char)rootEntry.d_name[0];
		unsigned char secondCh = (unsigned char)rootEntry.d_name[1];
		int firstValid = ((unsigned int)(firstCh - '0') <= 9U) || ((unsigned int)(firstCh - 'a') <= 25U);
		int secondValid = ((unsigned int)(secondCh - '0') <= 9U) || ((unsigned int)(secondCh - 'a') <= 25U);

		if (firstValid && secondValid && rootEntry.d_name[2] == '\0') {
			memset(&fileEntry, 0, sizeof(fileEntry));
			g_msfsMusicSubdirPath[7] = (char)firstCh;
			g_msfsMusicSubdirPath[8] = (char)secondCh;
			SceUID subDfd = sceIoDopen(g_msfsMusicSubdirPath);
			if (subDfd > 0) {
				int foundFile = 0;
				while (sceIoDread(subDfd, &fileEntry) > 0) {
					if ((fileEntry.d_stat.st_mode & 0xF000) == 0x2000) {
						MsfsMusicEntry *slot = &g_msfsMusicEntries[g_msfsMusicCount];
						slot->subDir[0] = rootEntry.d_name[0];
						slot->subDir[1] = rootEntry.d_name[1];
						slot->subDir[2] = rootEntry.d_name[2];
						slot->subDir[3] = '\0';
						memcpy(slot->origName, fileEntry.d_name, sizeof(slot->origName));

						int sum = 0;
						for (const unsigned char *p = (const unsigned char *)slot->origName; *p != 0; p++) {
							int ch = *p;
							if ((unsigned int)(ch - 'A') <= 25U)
								ch += 32;
							sum += ch;
						}
						slot->origNameSum = sum;
						memcpy(&slot->dirent, &fileEntry, sizeof(slot->dirent));
						g_msfsMusicCount++;
						if (g_msfsMusicCount == MSFS_MAX_MUSIC_ENTRIES) {
							foundFile = 1;
							break;
						}
						foundFile = 1;
					}
				}
				sceIoDclose(subDfd);
				if (foundFile)
					scannedSubdirs++;
			}
		}

		if (g_msfsMusicCount == MSFS_MAX_MUSIC_ENTRIES)
			break;
		if (scannedSubdirs > 665)
			break;
	}

	for (int i = 0; i < g_msfsMusicCount; i++) {
		int dupCount = g_msfsMusicEntries[i].displayNameSum;
		if (dupCount != 0)
			continue;

		for (int j = i + 1; j < g_msfsMusicCount; j++) {
			if (g_msfsMusicEntries[i].origNameSum == g_msfsMusicEntries[j].origNameSum &&
			    strncmp(g_msfsMusicEntries[i].origName, g_msfsMusicEntries[j].origName, sizeof(g_msfsMusicEntries[i].origName)) == 0) {
				if (dupCount == 0) {
					msfsFormatMusicDisplayName(&g_msfsMusicEntries[i], 1U);
					dupCount = 2;
				} else {
					dupCount++;
				}
				msfsFormatMusicDisplayName(&g_msfsMusicEntries[j], (unsigned int)dupCount);
			}
		}
		if (dupCount == 0)
			msfsFormatMusicDisplayName(&g_msfsMusicEntries[i], 0U);
	}

	g_msfsMusicScanned = 1;
	return 1;
}

static int msfsResolveMusicReadPath(char *outBuf, const char *path)
{
	const char *slash = strrchr(path, '/');
	if (slash == NULL || (slash - path) <= 4)
		return (int)(uintptr_t)slash;

	snprintf(outBuf, 0xFFBU, "music0:%s", slash + 1);
	char *name = outBuf + 7;
	char *dot = strrchr(name, '.');
	if (dot == NULL || (dot - name) <= 1) {
		*name = '\0';
		return (dot != NULL) ? (int)(dot - name) : 0;
	}

	if (g_msfsMusicScanned == 0) {
		SceUID dfd = sceIoDopen("music0:");
		if (dfd >= 0) {
			msfsScanMusicDir(dfd);
			sceIoDclose(dfd);
		}
	}

	int sum = 0;
	for (const unsigned char *p = (const unsigned char *)name; *p != 0; p++) {
		int ch = *p;
		if ((unsigned int)(ch - 'A') <= 25U)
			ch += 32;
		sum += ch;
	}

	for (int i = 0; i < g_msfsMusicCount; i++) {
		if (sum == g_msfsMusicEntries[i].displayNameSum) {
			const char *cand = g_msfsMusicEntries[i].dirent.d_name;
			int k;
			for (k = 0; k < 256; k++) {
				unsigned char a = (unsigned char)cand[k];
				unsigned char b = (unsigned char)name[k];
				if ((unsigned int)(a - 'A') <= 25U)
					a = (unsigned char)(a + 32);
				if ((unsigned int)(b - 'A') <= 25U)
					b = (unsigned char)(b + 32);
				if (a != b)
					break;
				if (a == 0)
					return snprintf(name, 0x100U, "%s/%s", g_msfsMusicEntries[i].subDir, g_msfsMusicEntries[i].origName);
			}
		}
	}

	if ((unsigned int)((unsigned char)name[0] - '0') <= 9U &&
	    (unsigned int)((unsigned char)name[1] - '0') <= 9U &&
	    (unsigned int)((unsigned char)name[2] - '0') <= 9U &&
	    (unsigned int)((unsigned char)name[3] - '0') <= 9U &&
	    name[4] == '.') {
		unsigned int idx = (unsigned int)((name[0] - '0') * 1000 +
		                                  (name[1] - '0') * 100 +
		                                  (name[2] - '0') * 10 +
		                                  (name[3] - '0'));
		if (idx < (unsigned int)g_msfsMusicCount)
			return snprintf(name, 0x100U, "%s/%s", g_msfsMusicEntries[idx].subDir, g_msfsMusicEntries[idx].origName);
	}

	return (int)(unsigned char)name[3];
}

static int __attribute__((noinline, noclone)) msfsIsPbpWriteAllowedByCompat(void)
{
	return pspemuCompatHasFlag(0x400U) != 0;
}

static int msfsClose(SceUID fd)
{
	if ((g_msfsBootWriteAllowed != 0 || msfsIsPbpWriteAllowedByCompat() != 0) && fd == MSFS_FAKE_PBP_WRITE_FD)
		return 0;
	if (g_msfsDeferCloseEnabled == 0)
		return msfsCloseInternal(fd);
	g_msfsDeferredCloseFd = fd;
	return 0;
}

static SceSSize msfsWrite(SceUID fd, const void *buf, SceSize size)
{
	SceIoDevInfo devInfo;

	if ((g_msfsBootWriteAllowed != 0 || msfsIsPbpWriteAllowedByCompat() != 0) && fd == MSFS_FAKE_PBP_WRITE_FD)
		return (SceSSize)size;

	for (int i = 0; i < MSFS_MAX_WRITE_TRACKED; i++) {
		if (g_msfsWriteTracked[i].fd == fd) {
			if (remoteLowPriorityIsPromotablePath(g_msfsWritePaths[i]) != 0) {
				if (sceCompatGetDevInf(&devInfo) != 0 ||
				    (uint64_t)devInfo.freeSize < (uint64_t)size + MSFS_MIN_FREE_SPACE_MARGIN) {
					return PSPEMU_ERROR_NO_SPACE;
				}
				g_msfsWriteTracked[i].hasWritten = 1;
			}
			break;
		}
	}

	SceSSize written = sceIoWrite(fd, buf, size);
	if (written >= 0 && fd != 0 && msfsIsTruncTrackedFd(fd) != 0) {
		MsfsTruncTrack *entry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == fd) {
				entry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (entry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		SceOff curPos = sceIoLseek(fd, 0, SCE_SEEK_CUR);
		if (curPos < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		if (entry->maxSize < curPos)
			entry->maxSize = curPos;
	}
	return written;
}

static int __attribute__((noinline, noclone)) msfsCheckProtectedPbpPath(const char *path)
{
	int len = (int)strlen(path);
	if (len <= 9)
		return 0;

	const char *tail = &path[len - 10];
	if (msfsComparePathCaseInsensitive(tail, "/EBOOT.PBP", 10) != 0 &&
	    msfsComparePathCaseInsensitive(tail, "/PBOOT.PBP", 10) != 0 &&
	    msfsComparePathCaseInsensitive(tail, ":EBOOT.PBP", 10) != 0 &&
	    msfsComparePathCaseInsensitive(tail, ":PBOOT.PBP", 10) != 0) {
		return 0;
	}

	if (g_msfsBootWriteAllowed == 0 && msfsIsPbpWriteAllowedByCompat() == 0)
		return PSPEMU_ERROR_ACCESS_DENIED;
	if (msfsComparePathCaseInsensitive(path, "/PSP/GAME/", 10) != 0)
		return PSPEMU_ERROR_ACCESS_DENIED;

	g_msfsFakePbpWritePending = 1;
	return 0;
}

static int msfsDread(SceUID dfd, SceIoDirent *dir)
{
	SceIoDirent localDir;
	uint32_t guestPriv = (uint32_t)(uintptr_t)dir->d_private;
	int ret = 0;

	memcpy(&localDir, dir, sizeof(localDir));
	localDir.d_private = NULL;

	if (dfd == MSFS_VIRTUAL_MUSIC_DFD) {
		if ((unsigned int)g_msfsMusicDreadDotState <= 1U) {
			memset(dir, 0, sizeof(*dir));
			dir->d_private = (void *)(uintptr_t)guestPriv;
			dir->d_stat.st_mode = 0x11FF;
			dir->d_stat.st_attr = 0x10;
			dir->d_stat.st_ctime.year = 2012;
			dir->d_stat.st_ctime.month = 11;
			dir->d_stat.st_ctime.day = 8;
			dir->d_stat.st_ctime.hour = 21;
			dir->d_stat.st_ctime.minute = 14;
			dir->d_stat.st_ctime.second = 52;
			dir->d_stat.st_atime = dir->d_stat.st_ctime;
			dir->d_stat.st_mtime = dir->d_stat.st_ctime;
			dir->d_name[0] = '.';
			dir->d_name[1] = (g_msfsMusicDreadDotState == 0) ? '\0' : '.';

			if (guestPriv != 0U) {
				MsfsDirentPrivate *priv = (MsfsDirentPrivate *)remotefsTranslateGuestAddr(guestPriv, 3, sizeof(MsfsDirentPrivate));
				if (priv->size == sizeof(MsfsDirentPrivate)) {
					memset(priv->shortName, 0, 13);
					strncpy(priv->longName, dir->d_name, sizeof(priv->longName));
					remotefsFlushGuestAddr(priv, sizeof(*priv));
				}
			}
			g_msfsMusicDreadDotState++;
			return 1;
		}

		if ((unsigned int)g_msfsMusicReadIndex < (unsigned int)g_msfsMusicCount) {
			ret = 1;
			memcpy(&localDir, &g_msfsMusicEntries[g_msfsMusicReadIndex].dirent, sizeof(localDir));
			g_msfsMusicReadIndex++;
		}
	} else {
		for (;;) {
			ret = sceIoDread(dfd, &localDir);
			if (ret <= 0)
				break;
			if (msfsDirFilterMatch(dfd, localDir.d_name) != 0 &&
			    msfsComparePathCaseInsensitive(localDir.d_name, &g_msfsHiddenEbootName[1], 14) != 0) {
				break;
			}
		}
	}

	msfsConvertStatUtcToLocal(&localDir.d_stat);
	memcpy(dir, &localDir, sizeof(localDir));
	if (ret > 0) {
		dir->d_stat.st_mode = msfsVitaModeToPspMode(localDir.d_stat.st_mode);
		dir->d_stat.st_attr = msfsVitaModeToPspAttr(localDir.d_stat.st_mode);
		if (guestPriv != 0U) {
			MsfsDirentPrivate *priv = (MsfsDirentPrivate *)remotefsTranslateGuestAddr(guestPriv, 3, sizeof(MsfsDirentPrivate));
			if (priv->size == sizeof(MsfsDirentPrivate)) {
				memset(priv->shortName, 0, 13);
				if (pspemuCompatHasFlag(0x80000U) != 0 && strlen(dir->d_name) <= 12U)
					strncpy(priv->shortName, dir->d_name, 13U);
				if (dfd == MSFS_VIRTUAL_MUSIC_DFD) {
					int entryIdx = g_msfsMusicReadIndex - 1;
					if ((unsigned int)entryIdx < (unsigned int)g_msfsMusicCount) {
						char *dot = strrchr(g_msfsMusicEntries[entryIdx].origName, '.');
						if (dot != NULL) {
							char extUpper[4] = { 0, 0, 0, 0 };
							for (int k = 0; k < 3; k++) {
								unsigned char ch = (unsigned char)dot[k + 1];
								if (ch == 0)
									break;
								if ((unsigned int)(ch - 'a') <= 25U)
									ch = (unsigned char)(ch - 32);
								extUpper[k] = (char)ch;
							}
							snprintf(priv->shortName, 13U, "%04d.%s", entryIdx, extUpper);
						}
					}
				}
				strncpy(priv->longName, dir->d_name, sizeof(priv->longName));
				remotefsFlushGuestAddr(priv, sizeof(*priv));
			}
		}
	}

	dir->d_private = (void *)(uintptr_t)guestPriv;
	return ret;
}

static int __attribute__((noinline, noclone)) msfsRewriteMusicPath(void (*rewriteFn)(char *, const char *))
{
	char *rel = &g_msfsResolvedPathBuf[4];
	if (*rel == '/' || *rel == '\\')
		rel++;

	if (msfsComparePathCaseInsensitive(rel, "MUSIC", 5) == 0 &&
	    (rel[5] == '\\' || rel[5] == '/' || rel[5] == '\0')) {
		if (g_msfsMusicRootKind == 2)
			return 0;
		g_msfsMusicRootKind = 1;
		msfsNormalizeBackslashes(g_msfsResolvedPathBuf);
		if (rel[5] == '\0') {
			rel[6] = '\0';
			rel[5] = '/';
		}
		rewriteFn(g_msfsResolvedPathBuf, rel);
		return 1;
	}

	if (msfsComparePathCaseInsensitive(rel, "PSP/MUSIC", 9) == 0 &&
	    (rel[9] == '\\' || rel[9] == '/' || rel[9] == '\0')) {
		if (g_msfsMusicRootKind == 1)
			return 0;
		g_msfsMusicRootKind = 2;
		strncpy(rel, rel + 4, 0xFFBU);
		msfsNormalizeBackslashes(g_msfsResolvedPathBuf);
		if (rel[5] == '\0') {
			rel[6] = '\0';
			rel[5] = '/';
		}
		rewriteFn(g_msfsResolvedPathBuf, rel);
		return 1;
	}

	return 2;
}

static int __attribute__((noinline, noclone)) msfsRejectBarePspDir(void)
{
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	if (msfsComparePathCaseInsensitive(rel, "PSP", 3) == 0 && rel[3] == '\0')
		return PSPEMU_ERROR_ACCESS_DENIED;
	return 0;
}

static int __attribute__((noinline, noclone)) msfsNormalizePath(const char *path, int allowTrailingSlash)
{
	strncpy(g_msfsNormPathBuf, path, sizeof(g_msfsNormPathBuf));
	g_msfsNormPathBuf[sizeof(g_msfsNormPathBuf) - 1] = '\0';
	msfsNormalizeBackslashes(g_msfsNormPathBuf);
	memset(g_msfsPathComponents, 0, sizeof(g_msfsPathComponents));

	char *p = g_msfsNormPathBuf;
	int depth = 0;
	int atStart = 1;

	while (*p != '\0') {
		if (!atStart) {
			atStart = (*p == '/');
			p++;
			continue;
		}
		if (*p == '/') {
			if (p == g_msfsNormPathBuf) {
				p++;
				continue;
			}
			return PSPEMU_ERROR_INVALID_ARG;
		}
		if (*p == '.') {
			if (p[1] == '\0')
				return PSPEMU_ERROR_INVALID_ARG;
			if (p[1] == '/') {
				char *dst = p;
				char ch;
				while ((ch = dst[2]) != '\0')
					*dst++ = ch;
				*dst = '\0';
				continue;
			}
			if (p[1] == '.') {
				if (p[2] == '\0')
					return PSPEMU_ERROR_INVALID_ARG;
				if (p[2] != '/') {
					p++;
					atStart = 0;
					continue;
				}
				if (depth == 0)
					return PSPEMU_ERROR_NO_ENTRY;
				char *prev = g_msfsPathComponents[--depth];
				if (prev == NULL)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				char *src = p + 3;
				if (src <= prev)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				char *dst = prev;
				while (*src != '\0')
					*dst++ = *src++;
				*dst = '\0';
				p = prev;
				continue;
			}
			p++;
			atStart = 0;
			continue;
		}
		g_msfsPathComponents[depth++] = p;
		if (depth >= MSFS_MAX_PATH_COMPONENTS)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		atStart = 0;
		p++;
	}

	g_msfsPathComponents[depth] = p;

	for (int i = depth; i > 0; i--) {
		char *start = g_msfsPathComponents[i - 1];
		char *end = g_msfsPathComponents[i];
		char *tail = end - 1;

		if (start < tail) {
			if (*tail == '/') {
				tail = end - 2;
				if (start > tail)
					continue;
			}
			if (*tail == ' ' || *tail == '.') {
				int stripCount = 0;
				for (;;) {
					stripCount++;
					if (tail - 1 < start)
						break;
					char prevCh = *(tail - 1);
					if (prevCh != '.' && prevCh != ' ')
						break;
					tail--;
				}
				char *src = tail + stripCount;
				char *dst = tail;
				while (*src != '\0')
					*dst++ = *src++;
				*dst = '\0';
			}
		}
	}

	if (g_msfsNormPathBuf[0] != '\0') {
		if (g_msfsNormPathBuf[strlen(g_msfsNormPathBuf) - 1] == '/' && !allowTrailingSlash)
			return PSPEMU_ERROR_INVALID_ARG;
	}
	return 0;
}

static int __attribute__((noinline, noclone)) msfsCheckForbiddenPrefix(void)
{
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	if (msfsComparePathCaseInsensitive(rel, "bgdl", 4) == 0 && (rel[4] == '\0' || rel[4] == '/'))
		return PSPEMU_ERROR_NO_ENTRY;
	if (msfsComparePathCaseInsensitive(rel, "temp/promote", 12) == 0 && (rel[12] == '\0' || rel[12] == '/'))
		return PSPEMU_ERROR_NO_ENTRY;
	return 0;
}

static int __attribute__((noinline, noclone)) msfsCheckForbiddenSuffix(const char *suffix)
{
	int len = (int)strlen(g_msfsNormPathBuf);
	if (len <= 14)
		return 0;
	if (msfsComparePathCaseInsensitive(&g_msfsNormPathBuf[len - 15], suffix, 15) == 0)
		return PSPEMU_ERROR_NO_ENTRY;
	return 0;
}

static int __attribute__((noinline, noclone)) msfsCheckGameDirWrite(void)
{
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	if (msfsComparePathCaseInsensitive(rel, "PSP/GAME", 8) == 0 && (rel[8] == '\0' || rel[8] == '/')) {
		pspemuRecordTitleListFlags(2U);
		return 0;
	}
	return 1;
}

static int __attribute__((noinline, noclone)) msfsCheckVhblDirAccess(void)
{
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	if (msfsComparePathCaseInsensitive(rel, "PSP/VHBL", 8) == 0 && (rel[8] == '\0' || rel[8] == '/')) {
		pspemuRecordTitleListFlags(4U);
		return 0;
	}
	return 1;
}

static int __attribute__((noinline, noclone)) msfsIsTempDir(void)
{
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	return (msfsComparePathCaseInsensitive(rel, "temp", 4) == 0 && rel[4] == '\0');
}

static char *__attribute__((noinline, noclone)) msfsExtractBasename(char *outBuf, const char *path)
{
	int len = (int)strlen(path);
	if (len > 255)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	int basePos = 0;
	for (int i = 0; i < len; i++) {
		if (path[i] == '/')
			basePos = i + 1;
	}
	char *res = strncpy(outBuf, &path[basePos], 0x100U);
	outBuf[255] = '\0';
	return res;
}

static int __attribute__((noinline, noclone)) msfsTrackUniqueFileHash(const char *path)
{
	SceIoStat stat;
	uint8_t digest[16];
	char baseName[256];

	if (g_msfsMaxUniqueHashes != 0 && sceIoGetstat(path, &stat) >= 0 && (stat.st_mode & 0x2000) != 0) {
		size_t pathLen = strlen(path);
		msfsExtractBasename(baseName, path);
		if (strncmp(baseName, "PARAM.SFO", sizeof(baseName)) != 0 &&
		    strncmp(baseName, "ICON0.PNG", sizeof(baseName)) != 0) {
			sceMd5Digest(path, (SceSize)pathLen, digest);
			int found = 0;
			for (int i = 0; i < g_msfsUniqueHashCount; i++) {
				if (memcmp(digest, g_msfsUniqueHashes[i], sizeof(digest)) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				int idx = g_msfsUniqueHashCount++;
				memcpy(g_msfsUniqueHashes[idx], digest, sizeof(digest));
				if (g_msfsUniqueHashCount > g_msfsMaxUniqueHashes || g_msfsUniqueHashCount > 255)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
		}
	}
	return 1;
}

static int __attribute__((noinline, noclone)) msfsConvertAndValidatePath(const char *path, int allowTrailingSlash)
{
	SceCesMbcsUcsContext ctx;
	uint32_t charLen;
	uint32_t utf8Len;
	int remain = (int)strlen(path);
	const char *p = path;
	int isValidUtf8 = 1;

	while (remain > 0) {
		if (sceCesUtf8CheckCode((const uint8_t *)p, (uint32_t)remain, &charLen) < 0) {
			isValidUtf8 = 0;
			break;
		}
		remain -= (int)charLen;
		p += charLen;
	}

	int ret;
	if (isValidUtf8) {
		if (path == NULL)
			return -1;
		ret = msfsNormalizePath(path, allowTrailingSlash);
		if (ret < 0)
			return ret;
	} else {
		memset(g_msfsUtf8ConvBuf, 0, sizeof(g_msfsUtf8ConvBuf));
		SceCesSJisUcsProfile *profile = sceCesUcsProfileInitSJis1997Cp932(&g_msfsSjisProfileSheet);
		sceCesMbcsUcsContextInit(&ctx, sceCesGetMbcsUcsProfile(profile));
		size_t srcLen = strlen(path);
		if (sceCesMbcsStrGetUtf8Len(&ctx, (const uint8_t *)path, (uint32_t)srcLen, &charLen, &utf8Len) < 0 ||
		    sceCesMbcsStrToUtf8Str(&ctx, (const uint8_t *)path, (uint32_t)srcLen, &charLen,
		                           (uint8_t *)g_msfsUtf8ConvBuf, sizeof(g_msfsUtf8ConvBuf), &utf8Len) < 0) {
			return PSPEMU_ERROR_INVALID_ARG;
		}
		ret = msfsNormalizePath(g_msfsUtf8ConvBuf, allowTrailingSlash);
		if (ret < 0)
			return ret;
	}

	ret = msfsCheckForbiddenSuffix(g_msfsHiddenEbootName);
	if (ret < 0)
		return ret;
	ret = msfsCheckForbiddenPrefix();
	if (ret < 0)
		return ret;
	ret = msfsCheckForbiddenSuffix(g_msfsHiddenMenuInfoName);
	return (ret < 0) ? ret : 0;
}

static SceUID msfsOpen(const char *path, int flags, SceMode mode)
{
	SceIoStat stat;
	(void)mode;

	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	msfsCheckVhblDirAccess();
	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, rel);

	int isNotOriginalPbp = strncmp(path, g_originalPath + 4, 30U);
	SceMode vitaMode = msfsPspModeToVitaMode(0644);
	int vitaFlags = msfsPspFlagsToVitaFlags(flags);
	int truncRequested = vitaFlags & 0x400;
	if (truncRequested != 0)
		vitaFlags &= ~0x400;

	int musicMatch;
	if ((vitaFlags & 2) != 0) {
		ret = msfsCheckProtectedPbpPath(g_msfsResolvedPathBuf);
		if (ret < 0)
			return ret;
		if (g_msfsFakePbpWritePending == 1) {
			g_msfsFakePbpWritePending = 0;
			return MSFS_FAKE_PBP_WRITE_FD;
		}
		musicMatch = msfsRewriteMusicPath((void (*)(char *, const char *))msfsBuildTempMusicPath);
		if (musicMatch == 0)
			return PSPEMU_ERROR_NO_ENTRY;
	} else {
		musicMatch = msfsRewriteMusicPath((void (*)(char *, const char *))msfsResolveMusicReadPath);
		if (musicMatch == 0)
			return PSPEMU_ERROR_NO_ENTRY;
	}

	SceUID fd;
	if (isNotOriginalPbp != 0) {
		if ((g_msfsAllowMusicRead == 0 || musicMatch != 1) &&
		    g_msfsMaxOpenFileSize != 0 &&
		    (g_msfsSkipSizeCheckForKernel == 0 || (flags & 0x40000000) == 0) &&
		    sceIoGetstat(g_msfsResolvedPathBuf, &stat) >= 0 &&
		    (stat.st_mode & 0x2000) != 0 &&
		    stat.st_size > (SceOff)(unsigned int)g_msfsMaxOpenFileSize) {
			pspemuRecordTitleListFlags(32U);
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		}
		msfsTrackUniqueFileHash(g_msfsResolvedPathBuf);
		fd = sceIoOpen(g_msfsResolvedPathBuf, vitaFlags, vitaMode);
		if (g_msfsFileCacheEnabled != 0) {
			if (fd <= 0)
				return fd;
			if (iocacheFileCanCache(g_msfsResolvedPathBuf, vitaFlags, vitaMode) != 0 &&
			    iocacheFileLoad(g_msfsResolvedPathBuf, vitaFlags, vitaMode, fd) < 0) {
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
		}
	} else {
		msfsTrackUniqueFileHash(g_msfsResolvedPathBuf);
		fd = sceIoOpen(g_msfsResolvedPathBuf, vitaFlags, vitaMode);
		if (g_msfsCachedEbootFd == 0) {
			if (fd > 0) {
				g_msfsCachedEbootFd = fd;
				int retries = 301;
				while (iocacheOpenBackingFile(g_msfsResolvedPathBuf, vitaFlags, vitaMode) < 0) {
					if (--retries == 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
					sceKernelDelayThread(100000U);
				}
			}
		} else if (g_msfsFileCacheEnabled != 0) {
			if (fd <= 0)
				return fd;
			if (iocacheFileCanCache(g_msfsResolvedPathBuf, vitaFlags, vitaMode) != 0 &&
			    iocacheFileLoad(g_msfsResolvedPathBuf, vitaFlags, vitaMode, fd) < 0) {
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
		}
	}

	if ((vitaFlags & 2) != 0) {
		if (fd <= 0)
			return fd;
		if (sceClibStrncasecmp(g_msfsResolvedPathBuf, "ms0:PSP/SAVEDATA/", 17U) != 0) {
			int slot = -1;
			for (int i = 0; i < MSFS_MAX_WRITE_TRACKED; i++) {
				if (g_msfsWriteTracked[i].fd == 0) {
					slot = i;
					break;
				}
			}
			if (slot < 0)
				return PSPEMU_ERROR_TOO_MANY_FILES;
			g_msfsWriteTracked[slot].fd = fd;
			g_msfsWriteTracked[slot].hasWritten = 0;
			strncpy(g_msfsWritePaths[slot], g_msfsResolvedPathBuf, 0x100U);
			g_msfsPendingWriteOpenSlot = slot;
		}
	}

	if (truncRequested != 0 && fd > 0) {
		MsfsTruncTrack *entry = NULL;
		for (int i = 0; i < MSFS_MAX_TRUNC_TRACKED; i++) {
			if (g_msfsTruncTracked[i].fd == 0) {
				entry = &g_msfsTruncTracked[i];
				break;
			}
		}
		if (entry == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		entry->fd = fd;
		strncpy(entry->path, g_msfsResolvedPathBuf, sizeof(entry->path));
	}

	return fd;
}

static int msfsGetstat(const char *path, SceIoStat *stat)
{
	SceIoStat localStat;
	size_t len = strlen(path);
	if (len != 0U && path[len - 1] == '/')
		return PSPEMU_ERROR_INVALID_ARG;

	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	if (msfsRewriteMusicPath((void (*)(char *, const char *))msfsResolveMusicReadPath) == 0)
		return PSPEMU_ERROR_NO_ENTRY;

	localStat.st_mode = 0;
	ret = sceIoGetstat(g_msfsResolvedPathBuf, &localStat);
	if (ret == 0) {
		SceMode pspMode = msfsVitaModeToPspMode(localStat.st_mode);
		localStat.st_attr = msfsVitaModeToPspAttr(localStat.st_mode);
		localStat.st_mode = pspMode;
		msfsConvertStatUtcToLocal(&localStat);
		memcpy(stat, &localStat, sizeof(localStat));
	}
	return ret;
}

static int msfsChstat(const char *path, const SceIoStat *stat, int bits)
{
	SceIoStat localStat;
	SceIoStat checkStat;
	size_t len = strlen(path);
	if (len != 0U && path[len - 1] == '/')
		return PSPEMU_ERROR_INVALID_ARG;

	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	memcpy(&localStat, stat, sizeof(localStat));

	if ((bits & 1) != 0)
		localStat.st_mode = msfsPspModeToVitaMode(stat->st_mode);
	if ((bits & 2) != 0) {
		bits &= ~2;
		if (bits == 0)
			return sceIoGetstat(g_msfsResolvedPathBuf, &checkStat);
	}

	msfsConvertStatLocalToUtc(&localStat);
	return sceIoChstat(g_msfsResolvedPathBuf, &localStat, bits);
}

static int msfsRename(const char *oldPath, const char *newPath)
{
	int ret = msfsConvertAndValidatePath(oldPath, 0);
	if (ret < 0)
		return ret;
	ret = msfsRejectBarePspDir();
	if (ret < 0)
		return ret;

	int oldIsTemp = msfsIsTempDir();
	msfsCheckVhblDirAccess();
	if (g_msfsBootWriteAllowed == 0 && msfsIsPbpWriteAllowedByCompat() == 0 && msfsCheckGameDirWrite() == 0)
		return PSPEMU_ERROR_ACCESS_DENIED;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	int normLen = (int)strlen(g_msfsNormPathBuf);
	strncpy(g_msfsRenameParentDir, g_msfsNormPathBuf, sizeof(g_msfsRenameParentDir));
	if (normLen >= (int)sizeof(g_msfsRenameParentDir))
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_msfsRenameParentDir[sizeof(g_msfsRenameParentDir) - 1] = '\0';
	int lastSlash = 0;
	for (int i = 0; i < normLen; i++) {
		if (g_msfsNormPathBuf[i] == '/')
			lastSlash = i;
	}
	g_msfsRenameParentDir[lastSlash] = '\0';

	if (msfsComparePathCaseInsensitive(newPath, "bgdl", 4) == 0 && newPath[4] == '\0') {
		strncpy(g_msfsNormPathBuf, newPath, sizeof(g_msfsNormPathBuf));
	} else {
		ret = msfsConvertAndValidatePath(newPath, 0);
		if (ret < 0)
			return ret;
	}

	msfsExtractBasename(g_msfsRenameBasename, g_msfsNormPathBuf);
	snprintf(g_msfsNormPathBuf, sizeof(g_msfsNormPathBuf), "%s/%s", g_msfsRenameParentDir, g_msfsRenameBasename);

	ret = msfsRejectBarePspDir();
	if (ret < 0)
		return ret;
	int newIsTemp = msfsIsTempDir();
	ret = msfsCheckForbiddenPrefix();
	if (ret < 0)
		return ret;

	msfsCheckVhblDirAccess();
	if (g_msfsBootWriteAllowed == 0 && msfsIsPbpWriteAllowedByCompat() == 0 && msfsCheckGameDirWrite() == 0)
		return PSPEMU_ERROR_ACCESS_DENIED;

	snprintf(g_msfsRenameDstPathBuf, sizeof(g_msfsRenameDstPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	ret = msfsCheckProtectedPbpPath(g_msfsResolvedPathBuf);
	if (ret < 0)
		return ret;
	ret = msfsCheckProtectedPbpPath(g_msfsRenameDstPathBuf);
	if (ret < 0)
		return ret;

	msfsTrackUniqueFileHash(g_msfsResolvedPathBuf);
	msfsTrackUniqueFileHash(g_msfsRenameDstPathBuf);
	if (newIsTemp || oldIsTemp) {
		msfsRemoveDirectoryContents("ms0:temp/promote");
		sceIoRmdir("ms0:temp/promote");
	}

	int renameRet = sceIoRename(g_msfsResolvedPathBuf, g_msfsRenameDstPathBuf);
	if (g_msfsRenameDstPathBuf[4] == '/')
		strncpy(&g_msfsRenameDstPathBuf[4], &g_msfsRenameDstPathBuf[5], 0x100U);
	if (remoteLowPriorityIsPromotablePath(g_msfsRenameDstPathBuf) != 0)
		remoteLowPriorityEnqueuePromote(g_msfsRenameDstPathBuf);
	return renameRet;
}

static int msfsMkdir(const char *path, SceMode mode)
{
	(void)mode;
	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	msfsCheckVhblDirAccess();
	if (g_msfsBootWriteAllowed == 0 && msfsIsPbpWriteAllowedByCompat() == 0 && msfsCheckGameDirWrite() == 0)
		return PSPEMU_ERROR_ACCESS_DENIED;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	return sceIoMkdir(g_msfsResolvedPathBuf, msfsPspModeToVitaMode(0755));
}

static int msfsRemove(const char *path)
{
	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	ret = msfsCheckProtectedPbpPath(g_msfsResolvedPathBuf);
	if (ret < 0)
		return ret;

	ret = sceIoRemove(g_msfsResolvedPathBuf);
	if (g_msfsCacheDevInfo != 0)
		g_msfsDevInfoValid = (sceCompatGetDevInf(&g_msfsCachedDevInfo) >= 0);
	return ret;
}

static int msfsRmdir(const char *path)
{
	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	return sceIoRmdir(g_msfsResolvedPathBuf);
}

static SceUID msfsDopen(const char *path)
{
	int ret = msfsConvertAndValidatePath(path, 1);
	if (ret < 0)
		return ret;

	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, g_msfsNormPathBuf);
	int musicMatch = msfsRewriteMusicPath((void (*)(char *, const char *))msfsResolveMusicReadPath);
	if (musicMatch == 0)
		return PSPEMU_ERROR_KERNEL_NO_ENTRY;

	SceUID dfd = sceIoDopen(g_msfsResolvedPathBuf);
	if (dfd >= 0 && musicMatch == 1) {
		if (g_msfsMusicScanned == 0)
			msfsScanMusicDir(dfd);
		sceIoDclose(dfd);
		g_msfsMusicReadIndex = 0;
		g_msfsMusicDreadDotState = 0;
		return MSFS_VIRTUAL_MUSIC_DFD;
	}
	return dfd;
}

static int __attribute__((noinline, noclone)) msfsValidateNormalizedPath(const char *path)
{
	if (path == NULL)
		return -1;
	int ret = msfsNormalizePath(path, 0);
	if (ret < 0)
		return ret;
	if (strncmp(g_msfsNormPathBuf, path, sizeof(g_msfsNormPathBuf)) != 0)
		return PSPEMU_ERROR_INVALID_ARG;
	return 0;
}

static int msfsPostOp(void)
{
	if (g_msfsDeferCloseEnabled != 0 && g_msfsDeferredCloseFd != 0) {
		msfsCloseInternal(g_msfsDeferredCloseFd);
		g_msfsDeferredCloseFd = 0;
	}

	if ((unsigned int)g_msfsPendingWriteOpenSlot < MSFS_MAX_WRITE_TRACKED) {
		snprintf(g_msfsWriteLogPath, sizeof(g_msfsWriteLogPath), "%s/w%02d.bin", g_msfsTempDirPath, g_msfsPendingWriteOpenSlot);
		SceUID fd = sceIoOpen(g_msfsWriteLogPath, 0x203, 0x186);
		if (fd > 0) {
			const char *writePath = g_msfsWritePaths[g_msfsPendingWriteOpenSlot];
			sceIoWrite(fd, writePath, strlen(writePath) + 1U);
			sceIoClose(fd);
		}
	}
	g_msfsPendingWriteOpenSlot = -1;

	if ((unsigned int)g_msfsPendingWriteCloseSlot < MSFS_MAX_WRITE_TRACKED) {
		snprintf(g_msfsWriteLogPath, sizeof(g_msfsWriteLogPath), "%s/w%02d.bin", g_msfsTempDirPath, g_msfsPendingWriteCloseSlot);
		SceUID fd = sceIoOpen(g_msfsWriteLogPath, 0x403, 0x186);
		if (fd >= 0)
			sceIoClose(fd);
	}
	g_msfsPendingWriteCloseSlot = -1;
	return 0;
}

int remoteMsfsSetBootAllowed(int allowed)
{
	g_msfsBootWriteAllowed = allowed;
	return allowed;
}

int remoteMsfsCheckMusicPath(const char *path, int flags)
{
	if (g_msfsAllowMusicRead == 0)
		return -1;

	int ret = msfsConvertAndValidatePath(path, 0);
	if (ret < 0)
		return ret;

	const char *rel = (g_msfsNormPathBuf[0] == '/') ? &g_msfsNormPathBuf[1] : g_msfsNormPathBuf;
	snprintf(g_msfsResolvedPathBuf, sizeof(g_msfsResolvedPathBuf), "%s%s", g_msfsRootPrefix, rel);

	int musicMatch;
	if ((flags & 2) != 0)
		musicMatch = msfsRewriteMusicPath((void (*)(char *, const char *))msfsBuildTempMusicPath);
	else
		musicMatch = msfsRewriteMusicPath((void (*)(char *, const char *))msfsResolveMusicReadPath);

	if (musicMatch == 0)
		return PSPEMU_ERROR_NO_ENTRY;
	return (musicMatch == 1) ? 0 : -1;
}

int remoteMsfsCloseCachedEboot(void)
{
	if (g_msfsCachedEbootFd != 0)
		return msfsCloseInternal(g_msfsCachedEbootFd);
	return 0;
}

void *remoteMsfsSuspendReset(void)
{
	g_msfsMusicScanned = 0;
	g_msfsMusicCount = 0;
	g_msfsMusicReadIndex = 0;
	return memset(g_msfsMusicEntries, 0, sizeof(g_msfsMusicEntries));
}

int remoteMsfsParseAppParam(char *discId, uint32_t *parentalLevel, char *gamedataId,
                            char *appVer, uint32_t *bootable, int *isPs1Mode, int *isPocsLaunch)
{
	const char *tokens[9];
	char mountPoint[16];
	char category[4];

	memcpy(g_msfsPbootSuffix, "/PBOOT.PBP", sizeof("/PBOOT.PBP"));
	memcpy(g_msfsEbootSuffix, "/EBOOT.PBP", sizeof("/EBOOT.PBP"));
	memcpy(g_msfsHiddenEbootName, "/__sce_ebootpbp", sizeof("/__sce_ebootpbp"));
	memcpy(g_msfsHiddenMenuInfoName, "/__sce_menuinfo", sizeof("/__sce_menuinfo"));
	g_msfsRootPrefix[0] = '\0';

	int ret = sceAppMgrGetAppParam(g_msfsAppParamBuf);
	if (ret != 0)
		return ret;

	int paramLen = (int)strlen(g_msfsAppParamBuf);
	if ((unsigned int)paramLen >= sizeof(g_msfsAppParamBuf) || paramLen <= 0)
		return PSPEMU_ERROR_INVALID_ARG;

	tokens[0] = g_msfsAppParamBuf;
	int ampCount = 0;
	for (int i = 0; i < paramLen; i++) {
		if (g_msfsAppParamBuf[i] == '&') {
			ampCount++;
			if (ampCount <= 8)
				tokens[ampCount] = &g_msfsAppParamBuf[i + 1];
		}
	}

	if ((unsigned int)(ampCount - 7) > 1U ||
	    memcmp(tokens[0], "originalpath=", 13U) != 0 ||
	    memcmp(tokens[1], "selfpath=", 9U) != 0 ||
	    memcmp(tokens[2], "discid=", 7U) != 0 ||
	    memcmp(tokens[3], "parentallevel=", 14U) != 0 ||
	    memcmp(tokens[4], "gamedataid=", 11U) != 0 ||
	    memcmp(tokens[5], "appver=", 7U) != 0 ||
	    memcmp(tokens[6], "bootable=", 9U) != 0 ||
	    memcmp(tokens[7], "category=", 9U) != 0 ||
	    (ampCount == 8 && memcmp(tokens[8], "pocketstation=", 14U) != 0)) {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	memset(g_selfPath, 0, sizeof(g_selfPath));
	memset(discId, 0, 16U);
	*parentalLevel = 0U;
	memset(gamedataId, 0, 16U);
	memset(appVer, 0, 8U);
	*bootable = 0U;

	if (msfsGetAppParamString("originalpath", g_originalPath, sizeof(g_originalPath)) < 0 ||
	    msfsGetAppParamString("selfpath", g_selfPath, sizeof(g_selfPath)) < 0 ||
	    msfsGetAppParamString("discid", discId, 16U) < 0 ||
	    msfsGetAppParamInt("parentallevel", (int *)parentalLevel) < 0 ||
	    msfsGetAppParamString("gamedataid", gamedataId, 16U) < 0 ||
	    msfsGetAppParamString("appver", appVer, 7U) < 0 ||
	    msfsGetAppParamInt("bootable", (int *)bootable) < 0 ||
	    msfsGetAppParamString("category", category, 3U) < 0) {
		return PSPEMU_ERROR_INVALID_ARG;
	}

	*isPocsLaunch = msfsGetPocketStationAppParam();
	*isPs1Mode = (category[0] == 'M' && category[1] == 'E' && category[2] == '\0');

	memset(mountPoint, 0, sizeof(mountPoint));
	int mountRet = sceAppMgrWorkDirMount(202U, mountPoint);
	if (mountRet < 0)
		return mountRet;

	strncpy(g_msfsRootPrefix, mountPoint, sizeof(g_msfsRootPrefix));
	if (g_originalPath[0] != '\0') {
		ret = msfsValidateNormalizedPath(g_originalPath);
		if (ret < 0)
			return ret;
		if (!msfsIsUx0PspemuPath(g_originalPath))
			return PSPEMU_ERROR_INVALID_ARG;
		snprintf(g_originalPath, sizeof(g_originalPath), "%s%s%s", mountPoint, &g_originalPath[11], g_msfsEbootSuffix);
	}

	if (g_selfPath[0] == '\0')
		return PSPEMU_ERROR_INVALID_ARG;
	ret = msfsValidateNormalizedPath(g_selfPath);
	if (ret < 0)
		return ret;
	if (!msfsIsUx0PspemuPath(g_selfPath))
		return PSPEMU_ERROR_INVALID_ARG;
	snprintf(g_selfPath, sizeof(g_selfPath), "%s%s", mountPoint, &g_selfPath[11]);

	if (discId[0] != '\0') {
		ret = msfsValidateNormalizedPath(discId);
		if (ret < 0)
			return ret;
	}
	return mountRet;
}

int remoteMsfsCheckPocketStationParam(void)
{
	if (sceAppMgrGetAppParam(g_msfsAppParamBuf) != 0)
		return 0;
	return msfsGetPocketStationAppParam();
}

int remoteMsfsPreInit(void)
{
	SceIoStat stat;
	if (pspemuSettingsGetEmuListFlag() == 0) {
		if (sceIoGetstat("ur0:emu_list", &stat) >= 0) {
			if ((stat.st_mode & 0x1000) != 0) {
				msfsRemoveDirectoryContents("ur0:emu_list");
				sceIoRmdir("ur0:emu_list");
			} else {
				sceIoRemove("ur0:emu_list");
			}
		}
		pspemuSettingsSetEmuListFlag();
	}
	return 0;
}

int remoteMsfsInit(void)
{
	int readDelay = pspemuCompatGetParam(2);
	if (readDelay != -1)
		g_msfsReadDelayMs = readDelay;
	if (pspemuCompatHasFlag(2U) != 0)
		g_msfsDeferCloseEnabled = 1;
	if (pspemuCompatHasFlag(0x2000U) != 0)
		g_msfsCacheDevInfo = 1;

	int fileCacheSlotSize = pspemuCompatGetParam(18);
	if (fileCacheSlotSize == -1)
		fileCacheSlotSize = 0;
	else
		g_msfsFileCacheEnabled = 1;

	int maxFileParam = pspemuCompatGetParam(31);
	if (maxFileParam == -1) {
		if (pspemuCompatIsRestrictedBootTitle() != 0)
			g_msfsMaxOpenFileSize = 0x01800000;
		else if (pspemuIsPs1Mode() == 1)
			g_msfsMaxOpenFileSize = 0x00040000;
	} else {
		g_msfsMaxOpenFileSize = maxFileParam & 0x3FFFFFFF;
		if (maxFileParam < 0)
			g_msfsSkipSizeCheckForKernel = 1;
		if ((maxFileParam & 0x40000000) != 0)
			g_msfsAllowMusicRead = 1;
	}

	int maxHashes = pspemuCompatGetParam(4);
	if (maxHashes != -1)
		g_msfsMaxUniqueHashes = maxHashes;

	SceUID thid = sceKernelCreateThread("ScePspemuRemoteMsfs", msfsThread, 96, 0x1000U, 0, 0x10000, NULL);
	if (thid < 0)
		return thid;
	int ret = sceKernelStartThread(thid, 0, NULL);
	if (ret != 0)
		return ret;

	memset(g_msfsTruncTracked, 0, sizeof(g_msfsTruncTracked));
	if (g_msfsFileCacheEnabled != 0) {
		ret = iocacheInit(0x04000000U);
		if (ret < 0)
			return ret;
		ret = iocacheFileInit(0x04000000U, (SceSize)fileCacheSlotSize);
		if (ret < 0)
			return ret;
	} else {
		ret = iocacheInit(0x08000000U);
		if (ret < 0)
			return ret;
	}

	memcpy(g_msfsOldTempDirPath, "ms0:pspemu_temp", sizeof("ms0:pspemu_temp"));
	msfsRemoveDirectoryContents(g_msfsOldTempDirPath);
	sceIoRmdir(g_msfsOldTempDirPath);

	memcpy(g_msfsTempDirPath, "ux0:temp/pspemu", sizeof("ux0:temp/pspemu"));
	if (sceIoMkdir(g_msfsTempDirPath, msfsPspModeToVitaMode(0666)) == PSPEMU_ERROR_ALREADY_EXISTS) {
		for (int i = 0; i < MSFS_MAX_WRITE_TRACKED; i++) {
			snprintf(g_msfsWriteLogPath, sizeof(g_msfsWriteLogPath), "%s/w%02d.bin", g_msfsTempDirPath, i);
			SceUID fd = sceIoOpen(g_msfsWriteLogPath, 1, 0);
			if (fd >= 0) {
				SceSSize readLen = sceIoRead(fd, g_msfsWritePaths[0], 0x100U);
				sceIoClose(fd);
				if (pspemuValidatePathPrefix(g_msfsWritePaths[0], (unsigned int)readLen, 256U, "ms0:") != 0 &&
				    msfsConvertAndValidatePath(g_msfsWritePaths[0], 0) == 0) {
					sceIoRemove(g_msfsNormPathBuf);
				}
			}
		}
		msfsRemoveDirectoryContents(g_msfsTempDirPath);
	}

	memset(g_msfsWriteTracked, 0, sizeof(g_msfsWriteTracked));
	g_msfsMusicRootKind = 0;
	if (pspemuIsPs1Mode() == 0) {
		ret = sceAppUtilMusicMount();
		if (ret != 0)
			return ret;
		if (pspemuSettingsIsClearMediaEnabled() == 1) {
			msfsRemoveDirectoryContents("ms0:/PSP/MUSIC");
			msfsRemoveDirectoryContents("ms0:/MUSIC");
			msfsRemoveDirectoryContents("ms0:/PSP/PHOTO");
			msfsRemoveDirectoryContents("ms0:/PICTURE");
			msfsRemoveDirectoryContents("ms0:/PSP/SCREENSHOT");
			msfsRemoveDirectoryContents("ms0:/VIDEO");
			msfsRemoveDirectoryContents("ms0:/PSP/VIDEO");
			msfsRemoveDirectoryContents("ms0:/PSP/THEME");
		}
	}

	g_msfsMusicCount = 0;
	g_msfsMusicReadIndex = 0;
	if (g_msfsCacheDevInfo != 0) {
		ret = sceCompatGetDevInf(&g_msfsCachedDevInfo);
		g_msfsDevInfoValid = (ret >= 0);
		return ret;
	}
	return 0;
}

static int __attribute__((noinline, noclone)) msfsDirFilterInit(void)
{
	for (int i = 0; i < MSFS_MAX_DIR_FILTERS; i++)
		g_msfsDirFilters[i].dfd = -1;
	return 0;
}

static int __attribute__((noinline, noclone)) msfsDirFilterRegister(SceUID dfd, const char *pattern)
{
	for (int i = 0; i < MSFS_MAX_DIR_FILTERS; i++) {
		if (g_msfsDirFilters[i].dfd == -1) {
			g_msfsDirFilters[i].dfd = dfd;
			strncpy(g_msfsDirFilters[i].pattern, pattern, sizeof(g_msfsDirFilters[i].pattern));
			return 0;
		}
	}
	return -1;
}

static int __attribute__((noinline, noclone)) msfsDirFilterRemove(SceUID dfd)
{
	for (int i = 0; i < MSFS_MAX_DIR_FILTERS; i++) {
		if (g_msfsDirFilters[i].dfd == dfd) {
			g_msfsDirFilters[i].dfd = -1;
			return 0;
		}
	}
	return -1;
}

static int __attribute__((noinline, noclone)) msfsDirFilterMatch(SceUID dfd, const char *name)
{
	int slot = -1;
	for (int i = 0; i < MSFS_MAX_DIR_FILTERS; i++) {
		if (g_msfsDirFilters[i].dfd == dfd) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return 1;

	const char *pattern = g_msfsDirFilters[slot].pattern;
	int patIdx = 0;
	int nameIdx = 0;
	int starActive = 0;
	int starPatIdx = -1;

	extern const short *_Touptab;

	for (;;) {
		char patCh = (char)_Touptab[(unsigned char)pattern[patIdx]];
		char nameCh = (char)_Touptab[(unsigned char)name[nameIdx]];

		if (patCh == '*') {
			patIdx++;
			starActive = 1;
			starPatIdx = patIdx;
			if (pattern[patIdx] == '\0')
				return 1;
		} else {
			if (patCh == '?' || patCh == nameCh) {
				patIdx++;
				nameIdx++;
			} else {
				nameIdx++;
				if (starPatIdx == -1 || !starActive)
					return 0;
				starActive = 1;
			}
			if (patCh == '\0')
				return (nameCh == '\0');
		}
		if (nameCh == '\0')
			return 0;
	}
}
