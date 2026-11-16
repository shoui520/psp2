/*
 * SceLibFios2: the FIOS2 runtime (the SDK's fios2.h interface): its scheduler, chunks,
 * I/O filters (the PSARC dearchiver and the cache), handle tables and the platform layer
 * over SceIofilemgr and SceThreadmgr. Sony built it with SNC in C++. Sony's names come from
 * the SDK headers and the library's messages; the rest are ours.
 */
#ifndef FIOS2_INTERNAL_H
#define FIOS2_INTERNAL_H

#define SCE_FIOS_LINKAGE
#include <scetypes.h>
#include <stdarg.h>
#include <sceerror.h>
#include <time.h>
#include <rtc.h>
#include <kernel.h>
#include <fios2.h>
#include <libkernel/libkernel.h>
#include "wws_platform.h"

/*
 * The library's own string and memory routines (0x81017930..): it defines sceClibMemcpy,
 * sceClibMemset, sceClibMemcmp, sceClibStrcmp and the others itself (kernel/libkernel.h's
 * prototypes) rather than importing SceLibKernel's, and the SDK's deflate.o, which calls
 * sceClibMemcpy, binds to its copy. SNC does not expand them inline.
 */
size_t fiosStrlen(const char *s);
int fiosVsnprintf(char *dst, size_t size, const char *fmt, va_list ap);
int fiosSnprintf(char *dst, size_t size, const char *fmt, ...);

/* the parameters sceFiosInitialize was given (Sony's name, from a message) */
extern SceFiosParams g_FiosParams;

/* what an op does (Sony's names, from sceFiosDebugDumpOp) */
enum FiosOpCommand {
	OP_DELETE, OP_DH_CLOSE, OP_DH_OPEN, OP_DH_READ, OP_DH_READ_BULK, OP_DIRECTORY_CREATE,
	OP_DIRECTORY_DELETE, OP_DIRECTORY_EXISTS, OP_EXISTS, OP_FH_CLOSE, OP_FH_OPEN,
	OP_FH_PREADV, OP_FH_PWRITEV, OP_FH_READV, OP_FH_STAT, OP_FH_SYNC, OP_FH_TRUNCATE,
	OP_FH_WRITEV, OP_FILE_DELETE, OP_FILE_EXISTS, OP_FILE_GET_SIZE, OP_PATH_READ,
	OP_PATH_WRITE, OP_PREFETCH_FH, OP_PREFETCH_FH_RANGE, OP_PREFETCH_FILE,
	OP_PREFETCH_FILE_RANGE, OP_RENAME, OP_RESOLVE, OP_STAT, OP_TRUNCATE, OP_ARCHIVE_QUERY,
	OP_ARCHIVE_MOUNT, OP_ARCHIVE_UNMOUNT, OP_CH_STAT, OP_DEVCTL, OP_IOCTL, OP_SYNC
};

/* what a chunk does (Sony's names, from the chunk dump) */
enum FiosChunkCommand {
	CHUNK_NULL, CHUNK_ROOT, CHUNK_PARENT, CHUNK_CHILD, CHUNK_CALLBACK, CHUNK_BUFFER,
	CHUNK_FH_OPEN, CHUNK_FH_READ, CHUNK_FH_WRITE, CHUNK_FH_BARRIER, CHUNK_FH_SYNC,
	CHUNK_FH_CLOSE, CHUNK_DH_OPEN, CHUNK_DH_READ, CHUNK_DH_READ_BULK, CHUNK_DH_CLOSE,
	CHUNK_DIR_EXISTS, CHUNK_DIR_CREATE, CHUNK_DIR_DELETE, CHUNK_FILE_EXISTS,
	CHUNK_FILE_DELETE, CHUNK_FILE_SIZE, CHUNK_DELETE, CHUNK_EXISTS, CHUNK_STAT,
	CHUNK_TRUNCATE, CHUNK_PREFETCH, CHUNK_RESOLVE, CHUNK_RENAME, CHUNK_ARCHIVE_QUERY,
	CHUNK_ARCHIVE_MOUNT, CHUNK_ARCHIVE_UNMOUNT, CHUNK_CHSTAT, CHUNK_DEVCTL, CHUNK_IOCTL,
	CHUNK_SYNC
};

/* "CHUNKID_INVALID": an op without a root chunk */
#define CHUNKID_INVALID		0xffff

/*
 * A path as the library keeps it: a hash of the normalized path, then the path itself in
 * the g_FiosParams.pathMax bytes behind it, or a pointer to the caller's path when the last
 * of those bytes is set ("pointer-type paths").
 */
struct FiosPath {
	uint8_t hash[16];
	char path[4];

	char *GetPath() { return path[g_FiosParams.pathMax - 1] ? *(char **)path : path; }
};

static inline const char *fiosPathGet(FiosPath *pPath)
{
	return pPath != NULL ? pPath->GetPath() : NULL;
}

/* an op flag the SDK does not name: a Sync call's op, its caller waits on it */
#define FIOS_OPFLAG_SYNC	0x100

/* an op's internal flags */
#define FIOS_OP_CANCELLED	0x0002	/* sceFiosOpCancel */
#define FIOS_OP_UPDATES_FH	0x0004	/* its end moves the file handle's offset and size */
#define FIOS_OP_PRIVATE_FH	0x0008	/* a path op that works through a file handle of its own */
#define FIOS_OP_OWNS_HANDLE	0x0010	/* its h is released when it fails */

/* an op: unit of the op table, the path behind it */
struct FiosOp {
	SceFiosOp h;
	uint16_t what;			/* OP_* */
	uint8_t bufferCount;
	bool deleted;			/* sceFiosOpDelete has run its delete callback */
	SceFiosHandle fh;		/* the file or directory handle it works on */
	int16_t internalFlags;		/* FIOS_OP_* */
	uint16_t rootChunk;		/* CHUNKID_INVALID before it is scheduled */
	SceFiosOffset offset;
	SceFiosBuffer buffers[8];
	SceFiosSize actCount;
	union {
		struct {
			const char *pPath2;	/* rename's new path, mount's archive */
			void *pArg;
		};
		int32_t nativeMode;		/* a new file's or directory's mode */
		int cbit;			/* sceFiosChangeStat's fields to change */
		int cmd;			/* a devctl's or ioctl's command */
		int flag;			/* sceFiosSync's flag */
		SceFiosSize length;		/* truncate's new length */
	};
	int err;
	volatile int done;
	uint32_t reserved1[2];
	SceFiosOpAttr attr;
	FiosPath path;
};

/* a file handle: unit of the FH table */
struct FiosFH {
	SceFiosFH h;
	uint32_t reserved0[2];
	SceFiosNativeFH nativeFH;	/* SCE_FIOS_NATIVEFH_INVALID while it is not open */
	uint32_t reserved1[4];
	SceFiosOffset offset;
	SceFiosSize size;
	SceFiosOpenParams openParams;
	FiosPath path;
};

/* a directory handle: unit of the DH table */
struct FiosDH {
	SceFiosDH h;
	uint32_t reserved0[2];
	SceFiosNativeDH nativeDH;
	uint32_t reserved1[6];
	void *pBuf;			/* the caller's buffer for the entries */
	size_t bufSize;
	uint32_t reserved2[4];
	FiosPath path;
};

/*
 * allocator.cpp: fixed-size units in a caller's buffer, the units first and a bitmap of the
 * allocated ones (most significant bit first) at the buffer's end, which AtomicToggleBits
 * changes 64 bits at a time with ldrexd/strexd. The ops, file and directory handles and
 * chunks are allocated from one each.
 */
class Allocator {
public:
	const char *m_pName;		/* in the leak warning: "op", "filehandle", ... */
	volatile uint32_t m_allocated;	/* units allocated */
	volatile uint32_t m_maxAllocated; /* the most units allocated at once */
	uint32_t m_lastIndex;		/* the last unit allocated: the next search starts after it */
	uint32_t m_count;		/* units in the buffer */
	volatile uint32_t m_generation;	/* times the search wrapped, 1 to 0x7ffe */
	size_t m_unitSize;		/* a unit's size, a multiple of m_alignment */
	size_t m_alignment;
	uint8_t *m_pBase;		/* the first unit */
	uint8_t *m_pBitmap;		/* a bit per unit */
	bool m_initialized;

	int Init(SceFiosBuffer buffer, uint32_t maxCount, size_t unitSize, size_t alignment);
	void Term();
	bool IsAllocated(const void *p, uint32_t count) const;

	/* the units index to index + count all allocated */
	bool IsAllocatedIndex(uint32_t index, uint32_t count) const
	{
		uint32_t total = m_count;
		uint32_t end;

		if (!m_initialized || count > total || index >= total)
			return false;
		end = count + index;
		if (end > total)
			return false;
		for (; index < end; index++)
			if (!(m_pBitmap[index >> 3] & (1 << (7 - (index & 7)))))
				return false;
		return true;
	}
	bool AtomicToggleBits(uint32_t index, uint32_t count, int set);
	int Allocate(uint32_t count);
	void Free(uint32_t index, uint32_t count);
	uint32_t GetLastIndex() const;
};

/*
 * handletable.cpp: the op, file and directory handles. A handle is its unit's index, the
 * table's number (1 to 3) and the allocator's generation; the unit starts with it.
 */
#define FIOS_HANDLE_INDEX(h)		((h) & 0x3fff)
#define FIOS_HANDLE_TABLE(h)		(((uint16_t)(h) >> 14) - 1)

/*
 * A pass-through file handle (sceFiosFilenoToFH, or a Sync open before sceFiosInitialize):
 * a native file descriptor in sync.cpp's table, the FH table's number and all of the
 * generation bits set.
 */
#define FIOS_PASSTHRU_FH_MAX		256
#define FIOS_PASSTHRU_FH(index)		(((index) & 0x3fff) | 0x7fff8000)

static inline bool fiosIsPassthruFH(SceFiosFH fh)
{
	return ((fh >> 16) & 0x7fff) == 0x7fff && FIOS_HANDLE_TABLE(fh) == 1;
}

class HandleTable : public Allocator {
public:
	int Init(SceFiosBuffer buffer, size_t alignment, size_t unitSize);
	int Term();
	SceFiosHandle Allocate();
	void *Lookup(SceFiosHandle h);
	void Free(SceFiosHandle h);
	void *GetUnit(SceFiosHandle h);
	uint32_t GetAll(SceFiosHandle *pHandles, uint32_t max);
};

/* the op, file handle and directory handle tables */
extern HandleTable s_handleTables[3];
extern HandleTable *const s_pOpTable;
extern HandleTable *const s_pFHTable;
extern HandleTable *const s_pDHTable;

/* api.cpp: the library's state that lives from module_start to module_stop */
void fiosLibraryInit();
void fiosLibraryTerm();

/* the library's locks (api.cpp) */
extern Wws::Platform::Rwlock s_defaultOpAttrLock;	/* "SceFiosDefaultOpAttr" */
extern Wws::Platform::Mutex s_opWaitMutex;		/* "SceFiosOpWait" */
extern Wws::Platform::Cond s_opWaitCond;
extern Wws::Platform::Mutex s_chunkMutex;		/* "SceFiosChunk", recursive */
extern Wws::Platform::Rwlock s_ioFilterLock;		/* "SceFiosIOFilter" */
extern Wws::Platform::Mutex s_dhMutex;			/* "SceFiosDirHandle" */
extern int s_overlayCount;				/* the process's overlays at start */
extern Allocator s_chunkAllocator;			/* the chunk storage */

/* op.cpp: an op's life */
FiosOp *fiosOpAllocate(const SceFiosOpAttr *pAttr, uint16_t what);
SceFiosOp *fiosGetCallbackOp();
SceFiosOp fiosOpCallback(FiosOp *pOp, SceFiosOpEvent event);
SceFiosOp fiosOpCompleteWithError(FiosOp *pOp, int err);
SceFiosOp fiosOpComplete(FiosOp *pOp);
SceFiosOp fiosOpSubmit(FiosOp *pOp);
SceFiosOp fiosOpSubmitPath(FiosOp *pOp, const char *pPath, bool resolve);
int fiosOpWait(FiosOp *pOp);
int fiosOpWaitUntil(FiosOp *pOp, SceFiosTime deadline);
void fiosOpDelete(SceFiosOp op, FiosOp *pOp);
SceFiosOp fiosOpSubmitPathExists(const SceFiosOpAttr *pAttr, const char *pPath, bool *pOutExists,
				 uint16_t what);
SceFiosOp fiosOpSubmitFH(const SceFiosOpAttr *pAttr, SceFiosFH fh, uint16_t what);
SceFiosOp fiosOpSubmitFHIO(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer *pIov,
			   int iovcnt, SceFiosOffset offset, FiosOpCommand what);
SceFiosOffset fiosFHTell(SceFiosFH fh);
SceFiosOp fiosOpSubmitPathWithMode(const SceFiosOpAttr *pAttr, const char *pPath, uint16_t what,
				   bool resolve, int32_t nativeMode);

/* chunk.cpp */
int fiosChunkInit(SceFiosBuffer storage);
void fiosChunkTerm();
void fiosChunkCancel(uint16_t chunk, bool cancel);
void fiosChunkReschedule(uint16_t chunkID, SceFiosTime deadline, SceFiosPriority priority);
void fiosScheduleTree(uint16_t chunkID, int depth, bool reschedule);
uint16_t fiosChunkGetParent(uint16_t chunkID, int index);
uint16_t fiosChunkGetChild(uint16_t chunkID, int index);

/*
 * A chunk: one or more 8-byte units of g_FiosParams.chunkStorage, its ID the first unit's
 * index. The fields past the type are the ones the chunk dump reads.
 */
#define FIOS_CHUNK_MAGIC	0x2b	/* the low six bits of an allocated chunk's first byte */

struct FiosChunk {
	int8_t magic;
	uint8_t scheduled : 1;
	uint8_t prepared : 1;
	uint8_t completed : 1;
	uint8_t cancelled : 1;
	uint8_t reservedFlags0 : 2;
	uint8_t executing : 1;
	uint8_t reservedFlags1 : 1;
	uint8_t type;			/* CHUNK_* */
	uint8_t reserved0[21];
	union {
		SceFiosOp op;		/* CHUNK_ROOT's op */
		SceFiosHandle h;	/* the file or directory handle it works on */
	};
	uint32_t reserved1;
	SceFiosOffset offset;
	SceFiosSize reqCount;
	SceFiosSize *pActCount;
};

/* debug.cpp: the dumps the SDK does not export */
char *fiosDebugDumpChunk(uint16_t chunkID, char *pBuffer, size_t bufferSize);
char *fiosDebugDumpIOFilter(SceFiosIOFilterCallback pFilterCallback, void *pFilterContext,
			    char *pBuffer, size_t bufferSize);

/* a cache entry as the cache's messages print it */
struct FiosCacheEntry {
	SceFiosDate accessDate;
	SceFiosDate modificationDate;
	uint64_t reserved0;
	SceFiosOffset offset;
	uint32_t length;
	uint32_t reserved1[7];
};

char *fiosDebugDumpCacheEntry(FiosCacheEntry entry, char *pBuffer, size_t bufferSize);

/* dearchiver.cpp */
void fiosDearchiverSetThreadCount(uint8_t count);
void fiosDearchiverTerm();
uint8_t fiosDearchiverSetDecompressorThreadCount(uint8_t threadCount);
uint8_t fiosDearchiverGetDecompressorThreadCount();

/* scheduler.cpp: every scheduler */
int fiosSchedulersInit();
void fiosSchedulersTerm();
bool fiosSchedulersIsIdle();
void fiosSchedulersWake();
void fiosSchedulersGetStatistics(struct FiosStatistics *pStats);
void fiosSchedulersResetStatistics();

/* the I/O filters and overlays */
int fiosIOFiltersCall(int command, void *pArgs);
int fiosIOFilterAdd(int index, SceFiosIOFilterCallback pFilterCallback, void *pFilterContext);
int fiosIOFilterGetInfo(int index, SceFiosIOFilterCallback *pOutFilterCallback,
			void **pOutFilterContext);
int fiosIOFilterRemove(int index);
int fiosOverlayCopy(const SceFiosOverlay *pOverlay, SceFiosOverlay *pOut);

/*
 * An overlay as the system keeps it (sceFiosOverlayGetInfoForProcess02): the SDK's
 * SceFiosOverlay, its reserved bytes holding the paths' lengths and the owning process.
 */
struct FiosProcessOverlay {
	uint8_t type;
	uint8_t order;
	uint16_t dstLength;
	uint16_t srcLength;
	uint16_t reserved;
	SceUID pid;
	SceFiosOverlayID id;
	char dst[SCE_FIOS_OVERLAY_POINT_MAX];
	char src[SCE_FIOS_OVERLAY_POINT_MAX];
};
void fiosIOFiltersTerm();
void fiosOverlaysTerm(SceUID pid);

/* what sceFiosStatisticsGet reports: each storage's units, then each scheduler's */
struct FiosStorageStatistics {
	uint32_t current;
	uint32_t highWater;
	uint32_t maxAvailable;
	uint32_t bytesPerUnit;
};

struct FiosStatistics {
	FiosStorageStatistics op, fh, dh, chunk;
	FiosStorageStatistics scheduler[3];
};

/*
 * A profiling message: the system time, the facility and the calling thread, then the
 * caller's text, printed when g_FiosParams.profiling has the mask's bit.
 */
#define FIOS_TRACE(mask, facility, fmt, ...)						\
	do {										\
		if (g_FiosParams.profiling & (mask)) {					\
			char fiosTraceTime[16];						\
			Wws::Platform::FormatSystemTime(fiosTraceTime, sizeof(fiosTraceTime)); \
			sceFiosPrintf("%s fios2/%-5s 0x%08llx " fmt "\n", fiosTraceTime,	\
				      facility,						\
				      (long long)Wws::Platform::GetCurrentThreadId(),	\
				      __VA_ARGS__);					\
		}									\
	} while (0)

/* SceAppMgr's and SceFios2User's calls (driver_us) */
extern "C" {
int sceAppMgrIsGameProgram(void);
int sceFiosOverlayAddForProcess02(SceUID pid, const SceFiosOverlay *pOverlay,
				  SceFiosOverlayID *pOutId);
int sceFiosOverlayRemoveForProcess02(SceUID pid, SceFiosOverlayID id);
int sceFiosOverlayGetInfoForProcess02(SceUID pid, SceFiosOverlayID id,
				      SceFiosOverlay *pOutOverlay);
int sceFiosOverlayModifyForProcess02(SceUID pid, SceFiosOverlayID id,
				     const SceFiosOverlay *pOverlay);
int sceFiosOverlayGetList02(SceUID pid, SceUInt8 minOrder, SceUInt8 maxOrder,
			    SceFiosOverlayID *pOutIds, SceSize maxIds, SceSize *pActualIds);
int sceFiosOverlayResolveWithRangeSync02(SceUID pid, int resolveFlag, const char *pInPath,
					 char *pOutPath, SceSize maxPath, SceUInt8 minOrder,
					 SceUInt8 maxOrder);
int sceFiosOverlayGetRecommendedScheduler02(int avail, const char *pPartiallyResolvedPath);
}

/* platform.cpp: SceIofilemgr's calls in FIOS2's terms */
int fiosPlatformTranslateError(int err);
int fiosPlatformOpenFlags(uint32_t flags);
SceIoMode fiosPlatformMode(SceIoMode mode);
int fiosPlatformOpen(SceFiosNativeFH *pFd, const char *pPath, uint32_t flags, SceIoMode mode);
SceOff fiosPlatformLseek(SceFiosNativeFH fd, SceOff offset, int whence);
int64_t fiosPlatformRead(SceFiosNativeFH fd, void *pBuf, SceSize size);
int64_t fiosPlatformWrite(SceFiosNativeFH fd, const void *pBuf, SceSize size);
int64_t fiosPlatformPread(SceFiosNativeFH fd, void *pBuf, SceSize size, SceOff offset);
int64_t fiosPlatformPwrite(SceFiosNativeFH fd, const void *pBuf, SceSize size, SceOff offset);
int fiosPlatformClose(SceFiosNativeFH fd);
int fiosPlatformGetstatByFd(SceFiosNativeFH fd, SceIoStat *pStat);
int fiosPlatformSyncByFd(SceFiosNativeFH fd);
int fiosPlatformFlushByFd(SceFiosNativeFH fd);
int fiosPlatformGetDevice(char *pDevice, const char *pPath, size_t size);
int fiosPlatformSync(const char *pPath, int flag);
int fiosPlatformRemove(const char *pPath);
int fiosPlatformRename(const char *pOldPath, const char *pNewPath);
int fiosPlatformTruncate(const char *pPath, SceOff size);
int fiosPlatformMkdir(const char *pPath, SceIoMode mode);
int fiosPlatformRmdir(const char *pPath);
int fiosPlatformDevctl(const char *pDevice, int cmd, const void *pArg, SceSize argSize,
		       void *pBuf, SceSize bufSize);
int fiosPlatformIoctl(SceFiosNativeFH fd, int cmd, const void *pArg, SceSize argSize,
		      void *pBuf, SceSize bufSize);
int fiosPlatformChstat(const char *pPath, const SceFiosStat *pStat, unsigned int cbit);
int fiosPlatformGetstat(const char *pPath, SceIoStat *pStat);
int fiosPlatformInit();
int fiosPlatformTerm();
void fiosPlatformIdle();
int fiosPlatformDopen(SceFiosNativeDH *pFd, const char *pPath);
int fiosPlatformDread(SceFiosNativeDH fd, SceIoDirent *pDirent);
int fiosPlatformDclose(SceFiosNativeDH fd);
SceFiosDate fiosPlatformDateTimeToDate(const SceDateTime *pTime);
int fiosPlatformOverlayAdd(SceUID pid, const SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutId);
int fiosPlatformOverlayRemove(SceUID pid, SceFiosOverlayID id);
int fiosPlatformOverlayGetInfo(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *pOutOverlay);
int fiosPlatformOverlayModify(SceUID pid, SceFiosOverlayID id, const SceFiosOverlay *pOverlay);
int fiosPlatformOverlayGetList(SceUID pid, uint8_t minOrder, uint8_t maxOrder,
			       SceFiosOverlayID *pOutIds, SceSize maxIds, SceSize *pActualIds);
int fiosPlatformOverlayResolve(SceUID pid, int resolveFlag, const char *pInPath, char *pOutPath,
			       SceSize maxPath, uint8_t minOrder, uint8_t maxOrder);

/* a decompression job of the dearchiver's codecs */
#define FIOS_ZLIB_JOB_DONE	0x1
#define FIOS_ZLIB_JOB_VALID	0x4

typedef struct FiosZlibJob {
	struct FiosZlibJob *next;	/* in the codec's queue */
	uint32_t codec;			/* FIOS_CODEC_ZLIB */
	uint16_t flags;
	uint16_t reserved0;
	uint32_t reserved1;
	const void *pSrc;
	void *pDst;
	void *pCopyDst;			/* where the window of the output goes, or NULL */
	void (*pCallback)(struct FiosZlibJob *job);	/* NULL: the waiters are woken */
	uint32_t reserved2;
	uint32_t size;			/* the output buffer's size, then the output's */
	uint32_t copyOffset;		/* the window copied to pCopyDst */
	uint32_t copyLength;
	int result;
} FiosZlibJob;

/* a dearchiver codec */
#define FIOS_CODEC_ZLIB		0x7a6c6962	/* "zlib" */

typedef struct FiosCodec {
	int (*init)(uint8_t threadCount);
	void (*term)();
	void (*setThreadCount)(uint8_t threadCount);
	FiosZlibJob *(*submit)(FiosZlibJob *job);
	bool (*isDone)(FiosZlibJob *job);
	int (*wait)(FiosZlibJob *job, uint32_t *pSize);
	uint32_t codec;
	uint32_t maxInput;
	uint32_t maxOutput;
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t blockSize;
} FiosCodec;

extern FiosCodec g_fiosZlibCodec;


/* the byte range an I/O filter's cache command is about */
struct FiosCacheRange {
	const char *pPath;
	SceFiosOffset offset;
	SceFiosSize length;
};

/* the cache commands fiosIOFiltersCall passes to the filters */
#define FIOS_FILTER_CACHE_CONTAINS	3
#define FIOS_FILTER_CACHE_FLUSH		4

/* exports the SDK's headers no longer declare */
extern "C" {
SceFiosOp sceFiosCachePrefetchFile(const SceFiosOpAttr *pAttr, const char *pPath);
SceFiosOp sceFiosCachePrefetchFileRange(const SceFiosOpAttr *pAttr, const char *pPath,
					SceFiosOffset offset, SceFiosSize length);
SceFiosOp sceFiosCachePrefetchFH(const SceFiosOpAttr *pAttr, SceFiosFH fh);
SceFiosOp sceFiosCachePrefetchFHRange(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				      SceFiosOffset offset, SceFiosSize length);
void sceFiosCacheFlushSync();
int sceFiosCachePrefetchFileSync(const SceFiosOpAttr *pAttr, const char *pPath);
int sceFiosCachePrefetchFileRangeSync(const SceFiosOpAttr *pAttr, const char *pPath,
				      SceFiosOffset offset, SceFiosSize length);
int sceFiosCachePrefetchFHSync(const SceFiosOpAttr *pAttr, SceFiosFH fh);
int sceFiosCachePrefetchFHRangeSync(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				    SceFiosOffset offset, SceFiosSize length);
SceFiosNativeFH sceFiosFHToFileno(SceFiosFH fh);
SceFiosFH sceFiosFilenoToFH(SceFiosNativeFH fd);
int sceFiosDeallocatePassthruFH(SceFiosFH fh);
}

/* sync.cpp: a SceIoStat in FIOS2's terms */
void fiosStatFromIoStat(const SceIoStat *pIoStat, SceFiosStat *pStat);

#endif
