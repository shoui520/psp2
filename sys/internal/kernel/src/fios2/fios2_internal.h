/*
 * SceFios2Kernel: the kernel side of FIOS2 overlays.
 *
 * An overlay redirects a destination path to a source path (SDK
 * sceFiosOverlayAdd). The kernel keeps the system-wide overlays in its own
 * table and each process's overlays in its process local storage; path
 * resolution walks them in order. Directory handles list a directory with
 * the overlays merged in.
 */

#ifndef FIOS2_INTERNAL_H
#define FIOS2_INTERNAL_H

#include <scetypes.h>
#include <iofilemgr/iofilemgr.h>
#include <acmgr/acmgr.h>
#include <processmgr/processmgr.h>
#include <rtc/rtc.h>
#include <threadmgr/threadmgr.h>
#include <fios2/fios2.h>
#include <sysmem/compiler.h>
#include <sysmem/sysclib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the SDK's error codes (libfios2 reference) */
#define SCE_FIOS_ERROR_UNIMPLEMENTED		0x80820000
#define SCE_FIOS_ERROR_CANT_ALLOCATE_DH		0x80820003
#define SCE_FIOS_ERROR_BAD_PATH			0x80820005
#define SCE_FIOS_ERROR_BAD_PTR			0x80820006
#define SCE_FIOS_ERROR_BAD_SIZE			0x80820008
#define SCE_FIOS_ERROR_BAD_DH			0x8082000C
#define SCE_FIOS_ERROR_BAD_ALIGNMENT		0x8082000D
#define SCE_FIOS_ERROR_NOT_A_DIRECTORY		0x8082000F
#define SCE_FIOS_ERROR_EOF			0x80820010
#define SCE_FIOS_ERROR_ACCESS			0x80820013
#define SCE_FIOS_ERROR_READ_ONLY		0x80820015
#define SCE_FIOS_ERROR_PATH_TOO_LONG		0x80820018
#define SCE_FIOS_ERROR_TOO_MANY_OVERLAYS	0x80820019
#define SCE_FIOS_ERROR_BAD_OVERLAY		0x8082001A
#define SCE_FIOS_ERROR_BAD_ORDER		0x8082001B
#define SCE_FIOS_ERROR_BAD_RESOLVE_TYPE		0x80820020

#define SCE_KERNEL_ERROR_NO_MEMORY		0x8002710B
#define SCE_ERROR_ERRNO_ENOENT			0x80010002
#define SCE_ERROR_ERRNO_ENODEV			0x80010013
#define SCE_ERROR_ERRNO_ENOTDIR			0x80010014
#define SCE_ERROR_ERRNO_EICV			0x8001008F

#define SCE_FIOS_OVERLAY_ORDER_USER_LAST	127	/* SDK SceFiosOverlayOrder */
#define SCE_FIOS_OVERLAY_MAX_OVERLAYS		64	/* SDK SceFiosOverlayLimits */

#define FIOS_OVERLAY_MAX		32	/* system-wide overlays */
#define FIOS_PROCESS_OVERLAY_MAX	128	/* a process's overlays */
#define FIOS_DH_MAX			64	/* a process's open directory handles */
#define FIOS_PATH_MAX			1024

/* the overlay tables are shared by readers and changed by one writer */
typedef struct SceFiosKernelRwlock {		/* name unknown */
	SceUID mutex;
	SceUID readCond;
	SceUID writeCond;
	int readWaiters;
	int readers;
	int writeWaiters;
	int writer;
} SceFiosKernelRwlock;

void sceFiosKernelRwlockInit(SceFiosKernelRwlock *lock);
void sceFiosKernelRwlockTerm(SceFiosKernelRwlock *lock);
void sceFiosKernelRwlockReadLock(SceFiosKernelRwlock *lock);
void sceFiosKernelRwlockReadUnlock(SceFiosKernelRwlock *lock);
void sceFiosKernelRwlockWriteLock(SceFiosKernelRwlock *lock);
void sceFiosKernelRwlockWriteUnlock(SceFiosKernelRwlock *lock);

/* a process's overlays and directory handles, in its process local storage */
typedef struct SceFiosKernelProcessStorage {	/* name unknown */
	int generation;			/* the system-wide overlays it has merged */
	int count;
	SceFiosOverlay overlays[FIOS_PROCESS_OVERLAY_MAX];
	void *dh[FIOS_DH_MAX];		/* in the process's heap; -1 while one opens */
} SceFiosKernelProcessStorage;

/* an overlay table: the system-wide one or a process's */
typedef struct SceFiosKernelOverlayTable {	/* name unknown */
	int max;
	int *count;
	SceFiosOverlay *overlays;
} SceFiosKernelOverlayTable;

/* what a resolution looks at (name unknown) */
typedef struct SceFiosKernelResolveParam {
	SceUID pid;
	int resolveFlag;		/* 1: a writable overlay is not followed */
	SceSize maxPath;
	uint8_t minOrder;
	uint8_t maxOrder;
} SceFiosKernelResolveParam;

/* the arguments sceKernelExtendKernelStackWide passes on (name unknown) */
typedef struct SceFiosKernelResolveArgs {
	SceFiosKernelResolveParam *param;
	const char *inPath;
	char *outPath;
} SceFiosKernelResolveArgs;

/* the thread state in the TLS slot */
typedef struct SceFiosKernelThreadState {	/* name unknown */
	unsigned int reserved : 31;
	unsigned int overlayDisabled : 1;	/* the thread sees only orders 128 and up */
} SceFiosKernelThreadState;

/* fixed-size blocks handed out from a caller's buffer, a bitmap behind them */
typedef struct SceFiosKernelStorage {		/* name unknown */
	const char *name;
	SceSize used;			/* blocks */
	int last;			/* the last block handed out */
	SceSize count;
	int generation;
	SceSize blockSize;
	SceSize alignment;
	void *blocks;
	uint8_t *bitmap;
	int initialized;
} SceFiosKernelStorage;

/* a directory listing entry */
typedef struct SceFiosKernelDHEntry {		/* name unknown */
	SceIoDirent dirent;
	struct SceFiosKernelDHEntry *next;
	int reserved;
} SceFiosKernelDHEntry;

#define SCE_FIOS_KERNEL_DH_MAGIC	0x6f766468	/* "hdvo" */

/* a directory handle: the directory itself and the entries overlays add */
typedef struct SceFiosKernelDH {		/* name unknown */
	int magic;
	SceUID pid;
	SceUID fd;			/* the directory, or -1 */
	SceFiosKernelDHEntry *head;
	SceFiosKernelDHEntry *tail;
	SceFiosKernelDHEntry *next;	/* the next entry to read */
	char path[FIOS_PATH_MAX];
} SceFiosKernelDH;

/* what building a directory handle works with (name unknown) */
typedef struct SceFiosKernelDHBuild {
	SceFiosKernelStorage *storage;	/* NULL: only the size is computed */
	void *platform;
	SceIoDirent *dirent;
	SceIoStat *stat;
	char **path;
	char **resolved;
	SceUID pid;
	uint8_t minOrder;
	uint8_t maxOrder;
} SceFiosKernelDHBuild;

extern int g_fiosPlsKey;
extern int g_fiosTlsSlot;
extern SceFiosKernelRwlock g_fiosOverlayLock;

/* path.cpp */
size_t sceFiosKernelPathNormalize(char *dst, const char *src, size_t dstSize, int style);
size_t sceFiosKernelPathNormalizeDefault(char *dst, const char *src, size_t dstSize);

/* overlay.cpp */
SceFiosKernelOverlayTable sceFiosKernelOverlayGetTable(SceUID pid);
int sceFiosKernelOverlayAddInternal(SceUID pid, const SceFiosOverlay *overlay, SceFiosOverlayID *outId);
int sceFiosKernelOverlayRemoveInternal(SceUID pid, SceFiosOverlayID id);
void sceFiosKernelOverlayRemoveAll(SceUID pid);
int sceFiosKernelOverlayGetInfoInternal(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay);
int sceFiosKernelOverlayModifyInternal(SceUID pid, SceFiosOverlayID id, const SceFiosOverlay *overlay);
int sceFiosKernelOverlayGetListInternal(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
					SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount);

/* resolve.cpp */
int sceFiosKernelOverlayResolve(SceFiosKernelResolveParam *param, int index, char *out, char *path);
int sceFiosKernelOverlayResolveCallback(void *argument);

/* dh.cpp */
int sceFiosKernelDHReadInternal(SceFiosKernelDH *dh, SceIoDirent *dirent, SceIoStat *stat,
				unsigned int minOrder, unsigned int maxOrder, void *platform);
int sceFiosKernelDHRead(SceFiosKernelDH *dh, SceIoDirent *dirent, SceIoStat *stat, void *platform);
int sceFiosKernelDHOpenInternal(SceUID pid, SceFiosKernelDH **outDH, const char *path,
				unsigned int minOrder, unsigned int maxOrder, void *buffer,
				SceSize bufferSize, void *platform);
int sceFiosKernelDHClose(SceFiosKernelDH *dh);
int sceFiosKernelDHFree(SceUID pid, SceFiosKernelDH *dh);

/* kernel.cpp: SceFios2KernelForDriver, declared in fios2/fios2.h */

/* api.cpp: what the syscalls call */
int sceFiosKernelOverlayAdd(SceFiosOverlay *overlay, SceFiosOverlayID *outId);
int sceFiosKernelOverlayRemove(SceFiosOverlayID id);
int sceFiosKernelOverlayGetInfo(SceFiosOverlayID id, SceFiosOverlay *outOverlay);
int sceFiosKernelOverlayModify(SceFiosOverlayID id, SceFiosOverlay *overlay);
int sceFiosKernelOverlayAddForProcess(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outId);
int sceFiosKernelOverlayRemoveForProcess(SceUID pid, SceFiosOverlayID id);
int sceFiosKernelOverlayGetInfoForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay);
int sceFiosKernelOverlayModifyForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *overlay);
int sceFiosKernelOverlayGetList(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount);
int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				    SceSize maxPath);
int sceFiosKernelOverlayResolveWithRangeSync(SceUID pid, int resolveFlag, const char *inPath,
					     char *outPath, SceSize maxPath, uint8_t minOrder,
					     uint8_t maxOrder);
int sceFiosKernelOverlayDHOpenSync(SceFiosKernelDH **outDH, const char *path, uint8_t minOrder,
				   uint8_t maxOrder);
int sceFiosKernelOverlayDHReadSync(SceFiosKernelDH *dh, SceIoDirent *dirent);
int sceFiosKernelOverlayDHCloseSync(SceFiosKernelDH *dh);
int sceFiosKernelOverlayDHStatSync(SceFiosKernelDH *dh, SceIoStat *stat);
int sceFiosKernelOverlayDHChstatSync(SceFiosKernelDH *dh, SceIoStat *stat, int bits);
int sceFiosKernelOverlayDHSyncSync(SceFiosKernelDH *dh, int flag);
int sceFiosKernelOverlayThreadSetDisabled(int disabled);
int sceFiosKernelOverlayThreadIsDisabled(void);
int sceFiosKernelOverlayGetRecommendedScheduler(int avail, const char *path);

/* storage.cpp */
int sceFiosKernelStorageInit(SceFiosKernelStorage *storage, void *buffer, SceSize size,
			     SceSize maxBlocks, SceSize elementSize, SceSize alignment);
int sceFiosKernelStorageAllocBlocks(SceFiosKernelStorage *storage, SceSize count);
void sceFiosKernelStorageFreeBlocks(SceFiosKernelStorage *storage, SceSize index, SceSize count);

/* platform.cpp: the file system under FIOS (iofilemgr in the kernel) */
int sceFiosKernelPlatformGetstat(const char *path, SceIoStat *stat);
void *sceFiosKernelPlatformOpen(void);
void sceFiosKernelPlatformClose(void *platform);
int sceFiosKernelPlatformDopen(SceUID *fd, const char *path, void *platform);
int sceFiosKernelPlatformDread(SceUID fd, SceIoDirent *dirent, void *platform);
int sceFiosKernelPlatformDclose(SceUID fd);
SceUInt64 sceFiosKernelPlatformDateTimeToTime(const SceDateTime *time);

/* the caller may do anything with any overlay */
static inline int fiosCallerIsPrivileged(SceUID caller)	/* name unknown */
{
	return sceSblACMgrIsSystemProgram(caller) || sceSblACMgrIsKernelProgram(caller) ||
	       SceThreadmgrForDriver_332E127C() >= 0x40;
}

/* a path buffer in the caller's process heap */
static inline char *fiosPathAlloc(void)			/* name unknown */
{
	char *path = (char *)sceKernelAllocOwnProcessHeap(FIOS_PATH_MAX, NULL);

	if (path != NULL)
		path[0] = '\0';
	return path;
}

static inline void fiosPathFree(char *path)		/* name unknown */
{
	path[0] = '\0';
	SceProcessmgrForDriver_FC2A424E(path);
}

/* a block range of storage for size bytes */
static inline void *fiosStorageAlloc(SceFiosKernelStorage *storage, SceSize size)	/* name unknown */
{
	int index = sceFiosKernelStorageAllocBlocks(storage,
		((size + storage->blockSize - 1) & -storage->blockSize) / storage->blockSize);

	if (index == -1)
		return NULL;
	return (char *)storage->blocks + index * storage->blockSize;
}

#ifdef __cplusplus
}
#endif

/* Sony's 4.00 build calls the checked string copies, the destination's size unknown */
#if FW_VERSION >= 0x04000000
#define strlcpy(dst, src, size)	__strlcpy_chk(dst, src, size, (size_t)-1)
#define strlcat(dst, src, size)	__strlcat_chk(dst, src, size, (size_t)-1)
#endif

#endif /* FIOS2_INTERNAL_H */
