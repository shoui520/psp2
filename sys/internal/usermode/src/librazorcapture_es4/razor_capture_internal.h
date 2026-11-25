/*
 * SceRazorCapture internal declarations and shared data structures.
 */

#ifndef RAZOR_CAPTURE_INTERNAL_H
#define RAZOR_CAPTURE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <kernel/iofilemgr.h>

typedef struct SceGxmProgram SceGxmProgram;
typedef struct SceGxmProgramParameter SceGxmProgramParameter;

/* External imports */
int sceClibPrintf(const char *fmt, ...);
void *sceClibMemset(void *dst, int c, SceSize len);
void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
char *sceClibStrncpy(char *dst, const char *src, SceSize len);
SceSize sceClibStrnlen(const char *s, SceSize maxlen);
int scePowerGetGpuClockFrequency(void);
int scePowerGetUsingWireless(void);
int sceKernelGetProcessId(void);
SceInt32 sceGpuRegisterSalvage(void);
SceInt32 sceGpuSignalWaitLockup(void);
SceInt32 sceGpuUnregisterSalvage(SceUID pid);
void *sceGxmGetTopContextInternal(void);
void sceGxmSetInitializeParamInternal(SceUInt32 magic, const void *callbacks);
int SceGpuEs4User_CCEE0336(void *topContext, SceUInt32 id, void *outInfo);

/* Page bitmap block (520 bytes: 1 byte inUse + 512 bytes bits + 7 bytes pad) */
typedef struct RazorPageBlock {
	uint8_t inUse;
	uint8_t bits[512];
	uint8_t pad[7];
} RazorPageBlock;

/* Tracked GXM memory mapping (16 bytes) */
typedef struct RazorMemMapping {
	uint32_t handle;
	uint32_t cpuAddr;
	uint32_t gpuAddr;
	uint32_t size;
} RazorMemMapping;

/* Buffered file writer (8200 bytes = 0x2008) */
typedef struct RazorFileWriter {
	SceUID fd;
	uint8_t buffer[8192];
	uint32_t used;
} RazorFileWriter;

/* Capture file object (~122400 bytes) */
typedef struct RazorCaptureFile {
	RazorFileWriter writer;          /* +0x0000 (8200 bytes) */
	RazorPageBlock *pageDir[256];    /* +0x2008 (1024 bytes) */
	RazorPageBlock pagePool[33];     /* +0x2408 (17160 bytes) */
	uint32_t mappingCount;           /* +0x6710 */
	uint32_t mappingPad;             /* +0x6714 */
	RazorMemMapping mappings[6000];  /* +0x6718 (96000 bytes) */
	uint64_t errorFlags;             /* +0x1de18 (122392) */
} RazorCaptureFile;

/* 32-entry ring set for deduplicating shader programs */
typedef struct RazorProgramCache {
	int32_t nextIdx;
	int32_t count;
	uint32_t entries[32];
} RazorProgramCache;

/* Buffer range list (for tracking mapped buffers in GXM) */
typedef struct RazorRangeEntry {
	uint32_t start;
	uint32_t end;
} RazorRangeEntry;

typedef struct RazorRangeList {
	uint32_t count;
	uint32_t pad;
	RazorRangeEntry entries[1024];
} RazorRangeList;

/* SGX hardware texture descriptor (6 words) */
typedef struct SgxTextureWord {
	uint32_t words[6];
} SgxTextureWord;

/* SGX render target state (12 words) */
typedef struct SgxRtState {
	uint32_t flags;
	uint32_t words[6];
	uint32_t sentinel;
	uint32_t extra[4];
} SgxRtState;

/* SGX TAG/ISP state (120 bytes = 30 dwords) */
typedef struct SgxTagState {
	uint32_t words[30];
} SgxTagState;

/* PDS program descriptor */
typedef struct SgxPdsProgram {
	uint32_t *codePtr;
	uint32_t gpuAddr;
	uint32_t dataSize;
	uint32_t codeWords;
} SgxPdsProgram;

/* PDS secondary/register state (2112 bytes: 512 words + 64-byte valid bitmap) */
typedef struct SgxRegBank {
	uint32_t regs[512];
	uint8_t valid[64];
} SgxRegBank;

/* USSE program scan result (40 bytes = 10 dwords) */
typedef struct SgxUsseProgramInfo {
	uint64_t *entryPtr;
	uint32_t gpuEntryAddr;
	uint64_t *startPtr;
	uint32_t insnCount;
	uint32_t byteSize;
	uint32_t entryOffsetInsn;
	uint32_t hasFooter;
	uint32_t hasProgramPtr;
	uint32_t footerHash;
	uint32_t programPtr;
} SgxUsseProgramInfo;

/* salvage.c */
int razorSalvageThreadEntry(SceSize args, void *argp);
int razorSalvageQueryJobInfo(uint32_t jobId, void *outInfo);
int razorSalvageStartThread(void (*callback)(void));
int razorSalvageStopThread(void);

/* capture_hooks.S */
void razorCaptureHostMarker(void);
void razorCaptureHostHookStart(void);
void razorCaptureHostHookComplete(void);

/* host_comm.c */
RazorProgramCache *razorProgramCacheInit(RazorProgramCache *cache);
void razorProgramCacheNoop(void);
RazorProgramCache *razorProgramCacheReset(RazorProgramCache *cache);
RazorProgramCache *razorProgramCacheInsert(RazorProgramCache *cache, uint32_t ptr, uint8_t *alreadyPresent);

RazorFileWriter *razorFileWriterInit(RazorFileWriter *writer);
int razorFileWriterOpen(RazorFileWriter *writer, const char *path);
int razorFileWriterClose(RazorFileWriter *writer);
int razorFileWriterFlush(RazorFileWriter *writer);
int razorFileWriterWrite(RazorFileWriter *writer, const void *buf, SceSize size);
int razorFileWriterWriteMem(RazorFileWriter *writer, const void *buf, SceSize size);

void razorHostNotifyStart(void);
int razorHostNotifyComplete(uint64_t errorFlags);
int razorHostPollTrigger(char *outFilename, int maxLen);

int razorRangeListAdd(RazorRangeList *list, uint32_t start, uint32_t size);
int razorRangeListRemove(RazorRangeList *list, uint32_t start);
int razorRangeListFindEnd(const RazorRangeList *list, uint32_t addr, uint32_t *outEnd);

int razorMarkerInit(void);
void *razorMarkerTerm(void);
int razorMarkerReset(uint32_t contextId);
int razorMarkerPush(uint32_t contextId, const char *name);
int razorMarkerGetRange(uint32_t contextId, uint32_t *outStart, uint32_t *outEnd);

/* sgx_decode.c */
uint32_t razorFloatToBits(float f);
uint32_t razorNextPow2(uint32_t v);
void *razorMemcpy(void *dst, const void *src, SceSize n);
void *razorMemset(void *dst, int c, SceSize n);
void *razorZeroInit(void *dst);
void sgxTagStateInit(void *tagState, const uint32_t *saRegs);
void sgxTagStateMerge(void *srcVoid, void *dstVoid);
uint32_t sgxDmaByteSize(const uint32_t *dmaEntry);
int sgxDmaFindReg(const uint32_t *dmaEntry, uint32_t regIdx, uint32_t *outOffset);
int sgxCalcTexLevelSize(const void *texWordsVoid, uint32_t mipCount, void *outLevelsVoid, uint32_t *outTotal);
int sgxTexGetPalette(const void *texWords, uint32_t *outGpuAddr, int *outEntryCount);
uint32_t sgxTexMaxMipLevels(const void *texWordsVoid);
uint32_t sgxCalcCubemapLayout(const void *texWordsVoid, uint32_t *outFaceOffsets, uint32_t *outTotal);
uint32_t sgxCalcTexByteSize(const void *texWords);

/* usse_decode.c */
void *sgxRtStateInit(void *rtState, const uint32_t *words);
uint32_t sgxRtCalcByteSize(const uint32_t *rtState);
void *sgxDepthStateInit(void *depthState, const void *src);
int sgxDepthIsDepthEnabled(const uint32_t **depthState, int isStore);
int sgxDepthIsStencilEnabled(const uint32_t **depthState, int isStore);
uint32_t sgxDepthGetDepthAddr(const uint32_t **depthState, int isStore);
uint32_t sgxDepthGetStencilAddr(const uint32_t **depthState, int isStore);
uint32_t sgxDepthCalcDepthSize(const uint32_t **depthState, int isStore);
uint32_t sgxDepthCalcStencilSize(const uint32_t **depthState, int isStore);
const uint32_t *sgxLookupTexFormat(uint32_t fmt, int isExtended);
int sgxCalcYuvLayout(uint32_t width, uint32_t height, uint32_t stride, uint32_t strideHeight,
		     uint8_t useStride, uint8_t planes, uint32_t *outPlanes, int *outSize1, int *outSize2);
int sgxIsSwizzledTexType(int texType);
int sgxScanUsseProgram(SgxUsseProgramInfo *outInfo, uint64_t *entryPtr, uint32_t gpuEntryAddr,
		       int minBytes, int maxBytes, uint8_t checkFooter);
void sgxFindUsseTexSamples(const SgxUsseProgramInfo *info, int *outOffsets, int isSecondPass);
int sgxUsseGetFooter(const SgxUsseProgramInfo *info, uint32_t *outHash, uint8_t *outHasProg, uint32_t *outProg);

/* capture_file.c */
void razorCaptureFileReset(RazorCaptureFile *file);
RazorCaptureFile *razorCaptureFileInit(RazorCaptureFile *file);
uint64_t razorCaptureFileOpen(RazorCaptureFile *file, const char *filename, const void *initData);
int razorCaptureFileClose(RazorCaptureFile *file);
int razorCaptureFileWriteFrameInfo(RazorCaptureFile *file, uint32_t argB, uint32_t argC);
int razorCaptureFileWriteJobHeader(RazorCaptureFile *file, const void *jobMeta, const void *regs, const uint32_t *kickInfo);
uint64_t razorCaptureFileFlushMemory(RazorCaptureFile *file, int captureAll);
void razorCaptureFileClearPages(RazorCaptureFile *file);
int razorCaptureFileWriteSync(RazorCaptureFile *file);
void razorCaptureFileMarkRange(RazorCaptureFile *file, uint32_t startAddr, uint32_t endAddr);
int razorCaptureFileAddMapping(RazorCaptureFile *file, uint32_t handle, uint32_t cpuAddr, uint32_t gpuAddr, uint32_t size);
int razorCaptureFileRemoveMapping(RazorCaptureFile *file, uint32_t handle, uint32_t cpuAddr);
int razorCaptureFileIsMappingsFull(const RazorCaptureFile *file);
uint32_t razorCaptureFileGpuToCpu(const RazorCaptureFile *file, uint32_t gpuAddr);
int razorCaptureFileFindCpuMapping(const RazorCaptureFile *file, uint32_t cpuAddr, uint32_t *outRemaining);

/* analyse.c */
uint64_t razorWriteMemoryDump(RazorPageBlock **pageDir, RazorFileWriter *writer);

/* capture.c */
void razorCaptureGetInitCallbacks(SceUInt32 *magic, void *callbacks);
void razorCapturePreInit(void);
void razorCaptureSetTriggerImpl(SceUInt32 startFrame, SceUInt32 endFrame,
				const char *filename, const char *funcName, SceBool fromHost);
void razorCaptureSetTriggerNextFrameImpl(const char *filename, const char *funcName, SceBool fromHost);
SceBool razorCaptureIsCapturingImpl(void);
void razorCaptureSetCaptureAllMemoryImpl(SceBool enable);
void razorCaptureSetCaptureBeforeKickImpl(SceBool enable);
void razorCaptureEnableSalvageImpl(const char *filename);

#endif /* RAZOR_CAPTURE_INTERNAL_H */
