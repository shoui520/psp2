#ifndef VSH_APP_SHELL_NPDRM_PACKAGE_INTERNAL_H
#define VSH_APP_SHELL_NPDRM_PACKAGE_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <libaes.h>
#include <libsha1.h>
#include <libsha256.h>
#include <kernel/iofilemgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceNpDrmPackage error codes */
#define SCE_NPDRM_PKG_ERROR_INVALID_ARG         ((SceInt32)0x80871002)
#define SCE_NPDRM_PKG_ERROR_INVALID_HEADER      ((SceInt32)0x80871003)
#define SCE_NPDRM_PKG_ERROR_NOT_DIRECTORY       ((SceInt32)0x80871005)
#define SCE_NPDRM_PKG_ERROR_UNSUPPORTED_ITEM    ((SceInt32)0x80871006)
#define SCE_NPDRM_PKG_ERROR_INVALID_HANDLE      ((SceInt32)0x80871008)
#define SCE_NPDRM_PKG_ERROR_SHORT_READ          ((SceInt32)0x8087100B)
#define SCE_NPDRM_PKG_ERROR_SHORT_WRITE         ((SceInt32)0x8087100C)
#define SCE_NPDRM_PKG_ERROR_INVALID_ITEM        ((SceInt32)0x8087100D)
#define SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA   ((SceInt32)0x8087100E)
#define SCE_NPDRM_PKG_ERROR_INVALID_ITEM_NAME   ((SceInt32)0x8087100F)
#define SCE_NPDRM_PKG_ERROR_PREFIX_MISMATCH     ((SceInt32)0x80871010)
#define SCE_NPDRM_PKG_ERROR_NAME_TOO_LONG       ((SceInt32)0x80871011)
#define SCE_NPDRM_PKG_ERROR_INVALID_DIR         ((SceInt32)0x80871012)
#define SCE_NPDRM_PKG_ERROR_PATH_TOO_LONG       ((SceInt32)0x80871013)
#define SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL    ((SceInt32)0x80871014)
#define SCE_NPDRM_PKG_ERROR_INVALID_OFFSET      ((SceInt32)0x80871015)
#define SCE_NPDRM_PKG_ERROR_UNALIGNED_OFFSET    ((SceInt32)0x80871016)
#define SCE_NPDRM_PKG_ERROR_UNALIGNED_SIZE      ((SceInt32)0x80871017)
#define SCE_NPDRM_PKG_ERROR_CORRUPT_STAT        ((SceInt32)0x80871019)
#define SCE_NPDRM_PKG_ERROR_INVALID_WORK_CTX    ((SceInt32)0x8087101A)
#define SCE_NPDRM_PKG_ERROR_SLOT_IN_USE         ((SceInt32)0x8087101B)
#define SCE_NPDRM_PKG_ERROR_INVALID_SLOT        ((SceInt32)0x8087101C)
#define SCE_NPDRM_PKG_ERROR_CONTENT_ID_MISMATCH ((SceInt32)0x8087101D)
#define SCE_NPDRM_PKG_ERROR_HEAD_TOO_LARGE      ((SceInt32)0x80871021)
#define SCE_NPDRM_PKG_ERROR_ABORTED             ((SceInt32)0x80871023)
#define SCE_NPDRM_PKG_ERROR_NO_WORK_BIN         ((SceInt32)0x80871024)
#define SCE_NPDRM_PKG_ERROR_INVALID_CERT        ((SceInt32)0x80871025)
#define SCE_NPDRM_PKG_ERROR_NO_LICENSE          ((SceInt32)0x80871400)
#define SCE_NPDRM_PKG_ERROR_LICENSE_REQUIRED    ((SceInt32)0x80871800)
#define SCE_NPDRM_PKG_ERROR_RIF_EXPIRED         ((SceInt32)0x80870004)
#define SCE_NPDRM_PKG_IO_ENOENT                 ((SceInt32)0x80010002)

#define NPDRM_PKG_MAX_SLOTS         6
#define NPDRM_PKG_HANDLE_BASE       10
#define NPDRM_PKG_STAT_MAGIC        0x5854537FU /* "\x7FSTX" */
#define NPDRM_PKG_STAT_SIZE_BASE    768U
#define NPDRM_PKG_STAT_SIZE_EXT     816U
#define NPDRM_PKG_PATH_MAX          320U
#define NPDRM_PKG_ROOT_MAX          64U
#define NPDRM_PKG_CHUNK_SLOT_SIZE   368U

/* Meta flags stored in NpDrmPkgMeta::metaFlags */
#define NPDRM_META_FLAG_LEGACY_PSP  0x00000001U
#define NPDRM_META_FLAG_USRDIR      0x00000002U
#define NPDRM_META_FLAG_FINALIZED   0x00000004U
#define NPDRM_META_FLAG_HW_CRYPTO   0x00000008U
#define NPDRM_META_FLAG_ABORTED     0x00000010U

/* Install open flags */
#define NPDRM_OPEN_FLAG_PREALLOC        0x00000010U
#define NPDRM_OPEN_FLAG_SKIP_BODY_HASH  0x00000020U
#define NPDRM_OPEN_FLAG_STEP_ONE_ITEM   0x00000100U
#define NPDRM_OPEN_FLAG_RESET_CURSOR    0x00000400U
#define NPDRM_OPEN_FLAG_PERF_LOG        0x00001000U
#define NPDRM_OPEN_FLAG_KEEP_FDS_OPEN   0x00002000U
#define NPDRM_OPEN_FLAG_DIRECT_IO       0x00010000U
#define NPDRM_OPEN_FLAG_SKIP_WORK_BIN   0x00020000U
#define NPDRM_OPEN_FLAG_IGNORE_LICENSE  0x00040000U

/* Chunk pipeline stages */
enum NpDrmChunkStage {
	NPDRM_STAGE_IDLE    = 0,
	NPDRM_STAGE_SHA1    = 1,
	NPDRM_STAGE_DECRYPT = 2,
	NPDRM_STAGE_SHA256  = 3,
	NPDRM_STAGE_WRITE   = 4,
	NPDRM_STAGE_DONE    = 5
};

/* Chunk flags */
#define NPDRM_CHUNK_FLAG_ACTIVE     0x00000001U
#define NPDRM_CHUNK_FLAG_EBOOT_PBP  0x00000002U

/* Hardware hash transform mode flags */
#define NPDRM_TRANSFORM_SHA256      0x00000000U
#define NPDRM_TRANSFORM_SHA1        0x40000000U

/* Forward declarations */
struct NpDrmInstallContext;
struct NpDrmPipelineState;
struct NpDrmChunkSlot;
struct NpDrmThreadPool;

typedef int (*NpDrmPipelineCallback)(struct NpDrmPipelineState *pipeline,
	SceUInt32 slotMask, void *userData);

typedef struct NpDrmFileSpec {
	const char *relativePath;
	SceSize pathLen;
	SceSize minBufferSize;
} NpDrmFileSpec;

typedef struct SceNpDrmPkgInfo {
	SceUInt32 drmType;
	SceUInt32 contentType;
	SceUInt32 packageType;
	SceUInt32 itemCount;
	SceUInt64 dataOffset;
	SceUInt64 installSize;
	SceUInt64 totalSize;
	char contentId[48];
	char titleId[12];
	SceUInt8 qaDigest[16];
	SceUInt32 padding;
} SceNpDrmPkgInfo;

typedef struct SceNpDrmInstallInfo {
	SceNpDrmPkgInfo pkgInfo;
	SceUInt8 licenseData[512];
	SceUInt8 instData[256];
	SceUInt8 contentDigest[32];
	SceInt32 licenseSize;
	SceInt32 instSize;
	SceInt32 contentDigestSize;
	SceUInt32 installAttr;
} SceNpDrmInstallInfo;

typedef struct SceNpDrmContentInfo {
	SceNpDrmPkgInfo pkgInfo;
	SceUInt16 appVersion;
	SceUInt16 minFwVersion;
	SceUInt8 sfoDigest[32];
	SceUInt8 iconDigest[32];
	SceUInt8 contentDigest[32];
	SceUInt8 selfDigest[32];
	SceUInt32 padding;
} SceNpDrmContentInfo;

typedef struct NpDrmPkgMeta {
	SceNpDrmPkgInfo pkgInfo;
	SceUInt32 itemTableSize;
	SceUInt32 itemNamesEnd;
	SceUInt64 sha1LimitOffset;
	SceUInt64 tailSigOffset;
	SceUInt32 tailSigSize;
	SceUInt32 headSize;
	SceUInt64 tailOffset;
	SceUInt64 tailSize;
	SceUInt32 metaFlags;
	SceUInt32 keySlotId;
	SceUInt32 openFlags;
	SceMode openMode;
} NpDrmPkgMeta;

typedef struct NpDrmItemMeta {
	SceInt32 itemIndex;
	SceUInt32 alignPad;
	SceUInt64 dataStart;
	SceUInt64 alignedEnd;
	SceUInt64 dataSize;
	SceUInt32 nameOffset;
	SceUInt32 nameSize;
	SceUInt32 itemFlags;
	SceUInt8 isDirectory;
	SceUInt8 isExtractable;
	SceUInt8 flagPad[2];
	SceUInt8 isEbootPbp;
	SceUInt8 tailPad[7];
} NpDrmItemMeta;

typedef struct NpDrmHashState {
	SceUInt32 currentItemIndex;
	SceUInt32 indexPad;
	SceUInt64 alignReserved;
	SceSha1Context pkgSha1Ctx;
	SceUInt64 pkgHashedBytes;
	SceUInt64 pkgTotalBytes;
	SceSha256Context bodySha256Ctx;
	SceUInt64 bodyHashedBytes;
	SceUInt64 bodyTotalBytes;
	SceSha256Context fileSha256Ctx;
	SceUInt64 fileHashedBytes;
	SceUInt64 fileTotalBytes;
} NpDrmHashState;

typedef struct NpDrmStatFile {
	SceUInt8 cmac[16];
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt32 statSize;
	SceUInt32 statFlags;
	SceUInt32 headSize;
	SceUInt8 headerReserved[8];
	SceUInt8 pkgSha1Digest[20];
	SceUInt8 headSha256Digest[32];
	SceUInt8 headFileSha256Digest[32];
	SceUInt8 bodySha256Digest[32];
	SceUInt8 tailSha256Digest[32];
	NpDrmHashState hashState;
	SceUInt8 preExtPadding[104];
	SceUInt32 ebootAlignPad;
	SceUInt32 ebootItemIndex;
	SceUInt64 ebootDigestEndOffset;
	SceUInt8 ebootPartialDigest[32];
	SceUInt8 tailPadding[16];
} NpDrmStatFile;

typedef struct NpDrmIoHandle {
	SceUID *fdPtr;
	char *pathBuf;
	char *rootDir;
} NpDrmIoHandle;

typedef struct NpDrmPipelineState {
	SceUID *activeFdPtr;
	char *pathBufPtr;
	char *rootDirPtr;
	NpDrmStatFile *statFilePtr;
	struct NpDrmChunkSlot *chunks;
	SceUInt32 chunkCount;
	SceUInt32 currentChunkIndex;
	SceUInt32 forkMask;
	SceUInt32 joinMask;
	NpDrmPipelineCallback forkCb;
	NpDrmPipelineCallback joinCb;
	void *cbUserData;
} NpDrmPipelineState;

typedef struct NpDrmChunkSlot {
	SceUInt8 *buffer;
	SceUInt32 length;
	SceUInt64 pkgOffset;
	SceUInt32 targetFill;
	SceUInt32 capacity;
	SceUInt64 itemDataStart;
	SceUInt64 itemDataSize;
	SceUInt64 itemAlignedEnd;
	SceUInt32 stage;
	SceUInt32 chunkFlags;
	SceSha1Context sha1Ctx;
	SceUInt64 sha1HashedBytes;
	SceUInt64 sha1TotalBytes;
	SceSha256Context sha256Ctx;
	SceUInt64 sha256HashedBytes;
	SceUInt64 sha256TotalBytes;
	struct NpDrmChunkSlot *prevSlot;
	NpDrmPipelineState *pipeline;
	NpDrmPkgMeta *pkgMeta;
	SceUInt32 tailPad;
} NpDrmChunkSlot;

typedef struct NpDrmWriteCursor {
	const SceUInt8 *data;
	SceUInt32 length;
	SceUInt64 offset;
} NpDrmWriteCursor;

typedef struct NpDrmJobWorker {
	NpDrmPipelineState *pipeline;
	SceUInt32 workerIndex;
	SceInt32 lastResult;
	SceInt32 stagePhase;
	SceUID threadId;
	SceUInt32 alignPad;
	SceUInt64 startClocks;
} NpDrmJobWorker;

typedef struct NpDrmThreadPool {
	SceUInt32 totalSize;
	SceUInt32 workerCount;
	SceUID forkEventFlag;
	SceUID joinEventFlag;
	SceUID workMemBlockId;
	SceUID heapMemBlockId;
	SceUInt32 modeFlags;
	SceUInt32 alignPad;
	SceUInt64 stageClocks[4];
	NpDrmJobWorker workers[1];
} NpDrmThreadPool;

typedef struct NpDrmInstallContext {
	SceInt32 slotIndex;
	NpDrmThreadPool *threadPool;
	char rootDir[NPDRM_PKG_ROOT_MAX];
	SceSize rootDirLen;
	SceUInt32 rootDirPad;
	NpDrmStatFile statFile;
	NpDrmPkgMeta pkgMeta;
	char prefixFilter[NPDRM_PKG_ROOT_MAX];
	SceSize prefixFilterLen;
	SceUInt32 prefixPad;
	NpDrmItemMeta currentItem;
	SceUID activeFd;
	SceUID bodyFd;
	SceUID headFd;
	NpDrmPipelineState pipeline;
	union {
		struct {
			char name[256];
			SceSize nameLen;
		} itemName;
		SceUInt8 tailSignature[384];
		char rifContentId[48];
	} scratch;
	char workPath[NPDRM_PKG_PATH_MAX];
	SceSize workPathLen;
	SceUInt64 maxWrittenOffset;
	SceUInt64 currentCursor;
	SceUInt8 ctxTailPad[40];
} NpDrmInstallContext;

typedef struct NpDrmInstallPerfStats {
	SceUInt64 itemOpenUs;
	SceUInt64 decryptUs;
	SceUInt64 fileCreateUs;
	SceUInt64 fileWriteUs;
	SceUInt64 fileCloseUs;
	SceUInt64 sha256Us;
	SceUInt64 sha1Us;
	SceUInt64 bodyStatUs;
	SceUInt64 totalWriteUs;
	SceUInt64 headPhaseUs;
	SceUInt64 contentPhaseUs;
	SceUInt64 tailPhaseUs;
	SceUInt64 lastStampUs;
	SceUInt64 enabled;
} NpDrmInstallPerfStats;

typedef struct NpDrmTlvPointers {
	const SceUInt8 *drmType;
	const SceUInt8 *contentType;
	const SceUInt8 *packageType;
	const SceUInt8 *installSize;
	const SceUInt8 *makePkgExe;
	const SceUInt8 *titleId;
	const SceUInt8 *appVersion;
	const SceUInt8 *digestTable;
	const SceUInt8 *minFwVersion;
	const SceUInt8 *itemNamesEnd;
	const SceUInt8 *paramDigest;
	const SceUInt8 *selfDigestInfo;
	const SceUInt8 *contentDigestInfo;
	const SceUInt8 *installAttrTable;
} NpDrmTlvPointers;

typedef struct NpDrmTransformState {
	SceUInt32 hashWords[8];
	SceUInt64 bitCount;
} NpDrmTransformState;

/* External kernel/SDK imports used by SceNpDrmPackage */
void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
void sceAesEncrypt1(const SceAesContext *ctx, const void *src, void *dst);
void sceAesDecrypt1(const SceAesContext *ctx, const void *src, void *dst);
int vshSysconIsIduMode(void);
int sceNpDrmPackageIsGameExist(void);

/* Shared globals between npdrm_install_api.cpp and npdrm_pkg_unpack.cpp */
extern NpDrmInstallPerfStats g_npDrmInstallPerf;
extern SceUInt8 g_npDrmHwAccelActive;

extern const NpDrmFileSpec g_npDrmHeadBinSpec;
extern const NpDrmFileSpec g_npDrmInstBinSpec;
extern const NpDrmFileSpec g_npDrmTempBinSpec;
extern const NpDrmFileSpec g_npDrmWorkBinSpec;

/* libnpdrm user syscall wrappers (npdrm_syscalls.cpp) */
int sceNpDrmPackageCheck(const void *buf, SceSize size, const void *digest, SceUInt32 identifier);
int sceNpDrmPackageDecrypt(void *buf, SceSize size, SceOff offset, SceUInt32 identifier);
int sceNpDrmPackageTransform(void *buf, SceSize size, NpDrmTransformState *state, SceUInt32 flags);
int sceNpDrmPackageInstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen);
int sceNpDrmPackageInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen);
int sceNpDrmPackageUninstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen);
int sceNpDrmPackageUninstallFinished(SceUInt32 result, const char *msg, SceSize msgLen);
int sceNpDrmSaveDataInstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen);
int sceNpDrmSaveDataInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen);
int sceNpDrmSaveDataFormatStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen);
int sceNpDrmSaveDataFormatFinished(SceUInt32 result, const char *msg, SceSize msgLen);
int sceNpDrmCheckActData(int *actType, int *versionFlag, SceUInt64 *accountId,
	SceUInt64 *startTime, SceUInt64 *expirationTime);
int sceNpDrmRemoveActData(SceUInt64 *accountId);
int sceNpDrmGetRifInfo(const void *license, SceSize licenseSize, int checkSign,
	char *contentId, SceUInt64 *accountId, int *licenseVersion, int *drmType,
	int *flags, SceUInt32 *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime,
	SceUInt64 *licenseExtFlags);
int sceNpDrmGetRifName(char *rifName, SceUInt64 accountId);
int sceNpDrmGetFixedRifName(char *rifName, SceUInt64 accountId);
int sceNpDrmGetRifNameForInstall(char *rifName, const void *license, SceUInt32 isFixed);
int sceNpDrmCheckDrmReset(const void *inAddr, SceSize size, SceUInt64 accountId, int *isReset);
int sceNpDrmPresetRifProvisionalFlag(void *license);

/* Public SceNpDrmInstall API (npdrm_install_api.cpp) */
int sceNpDrmInstallOpen(const char *dirPath, SceUInt32 flags, SceMode mode, void *workCtx);
int sceNpDrmInstallClose(int handle);
int sceNpDrmInstallResume(int handle, void *headBuf, SceSize bufSize);
int sceNpDrmInstallWrite(int handle, const void *buf, SceSize size);
SceOff sceNpDrmInstallLseek(int handle, SceOff offset, int whence);
int sceNpDrmInstallGetPkgInfo(SceNpDrmPkgInfo *outInfo, const void *buf, SceSize size);
int sceNpDrmInstallGetPkgInfoFromDir(SceNpDrmPkgInfo *outInfo, const char *dirPath,
	void *headBuf, SceSize bufSize);
int sceNpDrmInstallGetInstallInfo(SceNpDrmInstallInfo *outInfo, SceSize outSize,
	const char *dirPath, void *headBuf, SceSize bufSize);
int sceNpDrmInstallGetContentInfo(SceNpDrmContentInfo *outInfo, SceSize outSize,
	const char *dirPath, void *headBuf, SceSize bufSize);
int sceNpDrmInstallSetThreadMode(int handle, void *workMem, SceSize workSize,
	void *chunkHeap, SceSize heapSize, SceSize chunkSize, SceUInt32 numJobs, SceUInt32 flags);

/* Internal metadata & crypto helpers (npdrm_pkg_meta.cpp) */
void npDrmStatComputeCmac(SceUInt8 *outMac, const SceUInt8 *input, SceSize length, SceUInt32 keySlotId);
void npDrmStatEncryptCbc(SceUInt8 *buffer, SceSize length, SceUInt32 keySlotId);
void npDrmStatDecryptCbc(SceUInt8 *buffer, SceSize length, SceUInt32 keySlotId);
int npDrmParsePkgHeaderTlv(SceNpDrmPkgInfo *outInfo, const SceUInt8 *headerBuf, SceSize headerSize,
	NpDrmTlvPointers *outTlvs, SceUInt32 checkFlags);
int npDrmParsePkgInfo(SceNpDrmPkgInfo *outInfo, const void *headerBuf, SceSize headerSize);
int npDrmValidateItemEntry(const SceUInt8 *rawEntry, SceUInt64 dataAreaSize);
int npDrmParsePkgMeta(NpDrmPkgMeta *outMeta, const void *headerBuf, SceSize headerSize, SceUInt32 slotId);
int npDrmPopulateInstallMeta(SceNpDrmInstallInfo *outInfo, const SceUInt8 *headerBuf,
	const SceUInt8 *extHeaderBuf, const NpDrmTlvPointers *tlvs);
int npDrmParseInstallInfo(SceNpDrmInstallInfo *outInfo, const void *headerBuf, SceSize headerSize);
int npDrmParseContentInfo(SceNpDrmContentInfo *outInfo, const void *headerBuf, SceSize headerSize);
SceUInt32 npDrmGetItemEntryOffset(SceUInt32 itemIndex, const NpDrmPkgMeta *pkgMeta);
int npDrmDecodeItemMeta(NpDrmItemMeta *outItem, SceUInt32 itemIndex, const SceUInt8 *rawEntry,
	SceSize entrySize, const NpDrmPkgMeta *pkgMeta);
int npDrmDecryptPkgRegion(void *buf, SceSize size, SceUInt64 pkgOffset,
	const NpDrmPkgMeta *pkgMeta, SceUInt8 useHwAccel);
int npDrmVerifyTailSignature(const void *tailSig, SceSize sigSize,
	const SceUInt8 *sha1Digest, const NpDrmPkgMeta *pkgMeta);
int npDrmDecryptWorkBin(void *workBuf, SceSize workSize);

/* Internal unpacking & I/O helpers (npdrm_pkg_unpack.cpp) */
int npDrmHashChunkSha256(NpDrmChunkSlot *chunk, SceSha256Context *sha256Ctx, SceUInt64 *hashedOut);
int npDrmProcessPkgHeader(NpDrmInstallContext *ctx, const void *headerBuf, SceSize headerSize,
	SceUInt8 isResume);
int npDrmFinalizeHeadBin(NpDrmIoHandle *ioHandle, NpDrmHashState *hashState,
	NpDrmStatFile *statFile, const NpDrmPkgMeta *pkgMeta);
int npDrmWriteHeadSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor);
int npDrmReadItemMetaFromHead(NpDrmItemMeta *outItem, char *outNameBuf, SceUInt32 itemIndex,
	NpDrmIoHandle *tempIo, NpDrmIoHandle *headIo, const NpDrmPkgMeta *pkgMeta);
int npDrmOpenNextItem(NpDrmIoHandle *ioHandle, NpDrmHashState *hashState, NpDrmItemMeta *outItem,
	char *outNameBuf, const char *prefixFilter, SceSize prefixFilterLen,
	NpDrmIoHandle *headIo, const NpDrmPkgMeta *pkgMeta);
int npDrmAdvancePipeline(NpDrmPipelineState *pipeline, NpDrmHashState *hashState);
int npDrmProcessItemData(NpDrmPipelineState *pipeline, NpDrmWriteCursor *cursor,
	const NpDrmItemMeta *itemMeta, const NpDrmPkgMeta *pkgMeta,
	NpDrmIoHandle *ioHandle, NpDrmHashState *hashState);
int npDrmWriteContentSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor);
int npDrmWriteTailSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor);
int npDrmDispatchWriteCursor(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor);
int npDrmLoadAndVerifyStat(NpDrmStatFile *outStat, NpDrmIoHandle *ioHandle,
	const NpDrmPkgMeta *pkgMeta);
int npDrmReadDirFile(const char *dirPath, void *outBuf, SceSize bufSize, const NpDrmFileSpec *spec);
int npDrmWriteDirFile(const char *dirPath, const void *inBuf, SceSize bufSize, const NpDrmFileSpec *spec);

/* Internal pipeline step helper (npdrm_install_api.cpp) */
int npDrmExecuteChunkStage(NpDrmPipelineState *pipeline, SceUInt32 chunkIndex);
SceSize npDrmGetChunkSlotSize(void);

#ifdef __cplusplus
}
#endif

#endif /* VSH_APP_SHELL_NPDRM_PACKAGE_INTERNAL_H */
