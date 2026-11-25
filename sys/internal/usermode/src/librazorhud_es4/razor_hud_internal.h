/*
 * Internal definitions for SceRazorHud (librazorhud_es4).
 */
#ifndef RAZOR_HUD_INTERNAL_H
#define RAZOR_HUD_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <fw.h>
#include <razor_hud.h>

/* Error codes (0x8008C0xx = SceRazorHud / SceRazorGpu, 0x800100xx = errno) */
#define HUD_ERR_NOT_INITIALIZED   ((int)0x8008C000)
#define HUD_ERR_INVALID_PARAM     ((int)0x8008C001)
#define HUD_ERR_WRONG_MODE        ((int)0x8008C002)
#define HUD_ERR_NOT_STARTED       ((int)0x8008C003)
#define HUD_ERR_ALREADY_STARTED   ((int)0x8008C004)
#define HUD_ERR_INTERNAL          ((int)0x8008C005)
#define HUD_ERR_OVERFLOW          ((int)0x8008C006)
#define HUD_ERR_UNALIGNED         ((int)0x8008C007)
#define HUD_ERR_CONFIG_NOT_FOUND  ((int)0x8008C008)
#define HUD_ERR_NULL_POINTER      ((int)0x8008C009)

#define HUD_ERR_ENOMEM            ((int)0x8001000C)
#define HUD_ERR_EINVAL            ((int)0x80010016)

#define SCE_SYSMODULE_PERF        0x002FU
#define HUD_PA_KEY_MAGIC_1        0x7A209348U
#define HUD_PA_KEY_MAGIC_2        0x7A209237U
#define HUD_TRACE_BOOKMARK_FRAME  0xB0BAFE77U

/* SceDateTime is provided by <scetypes.h> */

/* SceGxm opaque/basic types */
typedef struct SceGxmContext SceGxmContext;
typedef struct SceGxmRenderTarget SceGxmRenderTarget;
typedef struct SceGxmShaderPatcher SceGxmShaderPatcher;
typedef struct SceGxmRegisteredProgram SceGxmRegisteredProgram;
typedef SceGxmRegisteredProgram *SceGxmShaderPatcherId;
typedef struct SceGxmProgram SceGxmProgram;
typedef struct SceGxmProgramParameter SceGxmProgramParameter;
typedef struct SceGxmVertexProgram SceGxmVertexProgram;
typedef struct SceGxmFragmentProgram SceGxmFragmentProgram;
typedef struct SceGxmSyncObject SceGxmSyncObject;
typedef struct SceGxmDepthStencilSurface SceGxmDepthStencilSurface;

typedef struct SceGxmNotification {
	volatile uint32_t *address;
	uint32_t value;
} SceGxmNotification;

typedef struct SceGxmValidRegion {
	uint32_t xMax;
	uint32_t yMax;
} SceGxmValidRegion;

typedef struct SceGxmTexture {
	uint32_t words[4];
} SceGxmTexture;

typedef struct SceGxmColorSurface {
	uint32_t pbeSidebandWord;
	uint32_t pbeEmitWords[6];
	uint32_t outputRegisterSize;
	SceGxmTexture backgroundTex;
} SceGxmColorSurface;

typedef struct SceGxmContextParams {
	void *hostMem;
	uint32_t hostMemSize;
	void *vdmRingBufferMem;
	uint32_t vdmRingBufferMemSize;
	void *vertexRingBufferMem;
	uint32_t vertexRingBufferMemSize;
	void *fragmentRingBufferMem;
	uint32_t fragmentRingBufferMemSize;
	void *fragmentUsseRingBufferMem;
	uint32_t fragmentUsseRingBufferMemSize;
	uint32_t fragmentUsseRingBufferOffset;
} SceGxmContextParams;

typedef void *(*SceGxmShaderPatcherHostAllocCallback)(void *userData, uint32_t size);
typedef void (*SceGxmShaderPatcherHostFreeCallback)(void *userData, void *mem);
typedef void *(*SceGxmShaderPatcherBufferAllocCallback)(void *userData, uint32_t size);
typedef void (*SceGxmShaderPatcherBufferFreeCallback)(void *userData, void *mem);
typedef void *(*SceGxmShaderPatcherUsseAllocCallback)(void *userData, uint32_t size, uint32_t *usseOffset);
typedef void (*SceGxmShaderPatcherUsseFreeCallback)(void *userData, void *mem);

typedef struct SceGxmShaderPatcherParams {
	void *userData;
	SceGxmShaderPatcherHostAllocCallback hostAllocCallback;
	SceGxmShaderPatcherHostFreeCallback hostFreeCallback;
	SceGxmShaderPatcherBufferAllocCallback bufferAllocCallback;
	SceGxmShaderPatcherBufferFreeCallback bufferFreeCallback;
	void *bufferMem;
	uint32_t bufferMemSize;
	SceGxmShaderPatcherUsseAllocCallback vertexUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback vertexUsseFreeCallback;
	void *vertexUsseMem;
	uint32_t vertexUsseMemSize;
	uint32_t vertexUsseOffset;
	SceGxmShaderPatcherUsseAllocCallback fragmentUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback fragmentUsseFreeCallback;
	void *fragmentUsseMem;
	uint32_t fragmentUsseMemSize;
	uint32_t fragmentUsseOffset;
} SceGxmShaderPatcherParams;

typedef struct SceGxmVertexAttribute {
	uint16_t streamIndex;
	uint16_t offset;
	uint8_t format;
	uint8_t componentCount;
	uint16_t regIndex;
} SceGxmVertexAttribute;

typedef struct SceGxmVertexStream {
	uint16_t stride;
	uint16_t indexSource;
} SceGxmVertexStream;

typedef struct SceGxmBlendInfo {
	uint8_t colorMask;
	uint8_t colorFunc : 4;
	uint8_t alphaFunc : 4;
	uint8_t colorSrc : 4;
	uint8_t colorDst : 4;
	uint8_t alphaSrc : 4;
	uint8_t alphaDst : 4;
} SceGxmBlendInfo;

typedef struct SceGxmRenderTargetParams {
	uint32_t flags;
	uint16_t width;
	uint16_t height;
	uint16_t scenesPerFrame;
	uint16_t multisampleMode;
	uint32_t multisampleLocations;
	int32_t driverMemBlock;
} SceGxmRenderTargetParams;

typedef struct SceKernelLwMutexWork {
	uint32_t data[8];
} SceKernelLwMutexWork;

/* Imported functions */
extern void *sceClibMemset(void *s, int c, uint32_t n);
extern void *sceClibMemcpy_safe(void *dst, const void *src, uint32_t n);
extern void *sceClibMemmove(void *dst, const void *src, uint32_t n);
extern char *sceClibStrncpy(char *dst, const char *src, uint32_t n);
extern char *sceClibStrncat(char *dst, const char *src, uint32_t n);
extern uint32_t sceClibStrnlen(const char *s, uint32_t n);
extern int sceClibStrncmp(const char *lhs, const char *rhs, uint32_t n);
extern char *sceClibStrchr(const char *s, int c);
extern char *sceClibStrstr(const char *haystack, const char *needle);
extern int64_t sceClibStrtoll(const char *s, char **endptr, int base);
extern int sceClibToupper(int c);
extern uint8_t sceClibLookCtypeTable(int c);
extern int sceClibPrintf(const char *fmt, ...);
extern int sceClibSnprintf(char *dst, uint32_t n, const char *fmt, ...);
extern int sceClibVsnprintf(char *dst, uint32_t n, const char *fmt, va_list ap);

extern int sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(int fd);
extern int sceIoRead(int fd, void *buf, uint32_t size);
extern int sceIoWrite(int fd, const void *buf, uint32_t size);
extern int64_t sceIoLseek(int fd, int64_t offset, int whence);
extern int sceIoOpenAsync(const char *file, int flags, int mode, void *opt);
extern int sceIoWriteAsync(int fd, const void *buf, uint32_t size, void *opt);
extern int sceIoCloseAsync(int fd, void *opt);

extern int sceKernelWaitEvent(int evid, uint32_t bits, void *outBits, void *outInfo, uint32_t *timeout);
extern int sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name, uint32_t attr, int initCount, const void *opt);
extern int sceKernelDeleteLwMutex(SceKernelLwMutexWork *work);
extern int sceKernelLockLwMutex(SceKernelLwMutexWork *work, int lockCount, uint32_t *timeout);
extern int sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int unlockCount);
extern int sceKernelCreateThread(const char *name, int (*entry)(uint32_t, void *), int initPriority, uint32_t stackSize, uint32_t attr, int cpuAffinityMask, const void *opt);
extern int sceKernelStartThread(int thid, uint32_t arglen, void *argp);
extern int sceKernelWaitThreadEnd(int thid, int *stat, uint32_t *timeout);
extern int sceKernelDeleteThread(int thid);
extern int sceKernelDelayThread(uint32_t usec);
extern int sceKernelGetProcessId(void);
extern int sceKernelGetProcessName(char *buf, uint32_t size);
extern int sceKernelGetProcessTime(uint64_t *clock);
extern uint64_t sceKernelGetProcessTimeWide(void);
extern int sceKernelAllocMemBlock(const char *name, uint32_t type, uint32_t size, const void *opt);
extern int sceKernelFreeMemBlock(int uid);
extern int sceKernelGetMemBlockBase(int uid, void **base);
extern uint32_t SceLibKernel_9F793F84(void);

extern int sceRtcGetCurrentClockLocalTime(SceDateTime *time);
extern int sceSysmoduleLoadModule(uint16_t id);
extern int sceSysmoduleUnloadModule(uint16_t id);

extern int sceRazorCpuStartActivityMonitor(void *buf, uint32_t size);
extern int sceRazorCpuStopActivityMonitor(void);
extern int sceRazorCpuGetActivityMonitorTraceBuffer(void **outBuf);
extern int sceRazorCpuStartUserMarkerTrace(void *buf, uint32_t size);
extern int sceRazorCpuStopUserMarkerTrace(void);
extern int sceRazorCpuGetUserMarkerTraceBuffer(void **outBuf);
extern int ScePerfInternal_26A6DF2C(int key, const void *bufParams);
extern int ScePerfInternal_37D9191E(int key, const void *traceParams);

extern int sceKernelPaRegisterShared(uint32_t magic1, uint32_t magic2);
extern int sceKernelPaUnregister(int key);
extern int sceKernelPaStartByKey(int key);
extern int sceKernelPaStopByKey(int key);
extern int sceKernelPaRemoveCounterTraceByKey(int key);
extern int sceKernelPaRemoveGpuTraceByKey(int key);
extern int sceKernelPaSetBookmarkChannelEnableByKey(int key, uint32_t type, uint32_t mask);
extern int sceKernelPaInsertBookmark(uint32_t type, uint32_t channel, uint32_t data);
extern uint32_t sceKernelPaGetTraceBufferStatus(void);
extern uint32_t sceKernelPaGetTraceBufferSize(uint32_t idx);
extern uint32_t sceKernelPaGetWritePointer(void);
extern uint32_t sceKernelPaGetIoBaseAddress(void);
extern uint64_t sceKernelPaGetTimebaseValue(void);
extern uint32_t sceKernelPaGetTimebaseFrequency(void);

extern int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context);
extern int sceGxmDestroyContext(SceGxmContext *context);
extern int sceGxmFinish(SceGxmContext *context);
extern int sceGxmNotificationWait(const SceGxmNotification *notification);
extern int sceGxmColorSurfaceSetGammaMode(SceGxmColorSurface *surface, uint32_t mode);
extern int sceGxmColorSurfaceGetClip(const SceGxmColorSurface *surface, uint32_t *xMin, uint32_t *yMin, uint32_t *xMax, uint32_t *yMax);
extern int sceGxmSetCullMode(SceGxmContext *context, uint32_t mode);
extern int sceGxmSetFrontDepthFunc(SceGxmContext *context, uint32_t func);
extern int sceGxmSetBackDepthFunc(SceGxmContext *context, uint32_t func);
extern int sceGxmSetFrontPolygonMode(SceGxmContext *context, uint32_t mode);
extern int sceGxmBeginScene(SceGxmContext *context, uint32_t flags, const SceGxmRenderTarget *rt, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, SceGxmSyncObject *fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencilSurface);
extern int sceGxmEndScene(SceGxmContext *context, const SceGxmNotification *vertexNotification, const SceGxmNotification *fragmentNotification);
extern int sceGxmSetVertexProgram(SceGxmContext *context, const SceGxmVertexProgram *vp);
extern int sceGxmSetFragmentProgram(SceGxmContext *context, const SceGxmFragmentProgram *fp);
extern int sceGxmSetVertexStream(SceGxmContext *context, uint32_t streamIndex, const void *streamData);
extern int sceGxmSetFragmentTexture(SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture);
extern int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer);
extern int sceGxmSetUniformDataF(void *uniformBuffer, const SceGxmProgramParameter *param, uint32_t componentOffset, uint32_t componentCount, const float *sourceData);
extern int sceGxmDraw(SceGxmContext *context, uint32_t primType, uint32_t indexType, const void *indexData, uint32_t indexCount);
extern int sceGxmShaderPatcherCreate(const SceGxmShaderPatcherParams *params, SceGxmShaderPatcher **patcher);
extern int sceGxmShaderPatcherDestroy(SceGxmShaderPatcher *patcher);
extern int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher *patcher, const SceGxmProgram *programHeader, SceGxmShaderPatcherId *programId);
extern int sceGxmShaderPatcherUnregisterProgram(SceGxmShaderPatcher *patcher, SceGxmShaderPatcherId programId);
extern const SceGxmProgram *sceGxmShaderPatcherGetProgramFromId(SceGxmShaderPatcherId programId);
extern int sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher *patcher, SceGxmShaderPatcherId programId, const SceGxmVertexAttribute *attributes, uint32_t attributeCount, const SceGxmVertexStream *streams, uint32_t streamCount, SceGxmVertexProgram **vertexProgram);
extern int sceGxmShaderPatcherReleaseVertexProgram(SceGxmShaderPatcher *patcher, SceGxmVertexProgram *vertexProgram);
extern int sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher *patcher, SceGxmShaderPatcherId programId, uint32_t outputFormat, uint32_t multisampleMode, const SceGxmBlendInfo *blendInfo, const SceGxmProgram *vertexProgram, SceGxmFragmentProgram **fragmentProgram);
extern int sceGxmShaderPatcherReleaseFragmentProgram(SceGxmShaderPatcher *patcher, SceGxmFragmentProgram *fragmentProgram);
extern const SceGxmProgramParameter *sceGxmProgramFindParameterByName(const SceGxmProgram *program, const char *name);
extern uint32_t sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *param);
extern int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data, uint32_t texFormat, uint32_t width, uint32_t height, uint32_t mipCount);
extern int sceGxmTextureSetMinFilter(SceGxmTexture *texture, uint32_t minFilter);
extern int sceGxmTextureSetMagFilter(SceGxmTexture *texture, uint32_t magFilter);
extern int sceGxmTextureSetMipFilter(SceGxmTexture *texture, uint32_t mipFilter);
extern int sceGxmMapMemory(void *base, uint32_t size, uint32_t attribs);
extern int sceGxmUnmapMemory(void *base);
extern int sceGxmRenderTargetGetDriverMemBlock(const SceGxmRenderTarget *rt, int *uid);
extern int sceGxmDestroyRenderTarget(SceGxmRenderTarget *rt);
extern int sceGxmIsDebugVersion(void);

extern int sceGxmSetCallbackInternal(uint32_t type, const void *callbacks);
extern void *sceGxmGetTopContextInternal(void);
extern int sceGxmGetRenderTargetMemSizeInternal(const SceGxmRenderTargetParams *params, uint32_t *size);
extern int sceGxmCreateRenderTargetInternal(const SceGxmRenderTargetParams *params, SceGxmRenderTarget **rt);
extern int sceGxmMapVertexUsseMemoryInternal(void *base, uint32_t size, uint32_t *offset);
extern int sceGxmUnmapVertexUsseMemoryInternal(void *base);
extern int sceGxmMapFragmentUsseMemoryInternal(void *base, uint32_t size, uint32_t *offset);
extern int sceGxmUnmapFragmentUsseMemoryInternal(void *base);
extern void *SceGxmInternal_338E3186(void);

extern int SceGpuEs4User_09BB98B2(void *ctx, void *arg);
extern int SceGpuEs4User_768482F6(void *ctx, void *arg);
extern int SceGpuEs4User_C0D16CE5(void *ctx, void *arg);

/* Shared globals */
typedef struct HudManager {
	void *views[20];
	uint32_t viewCount;
	void *dataSources[10];
	uint32_t dataSourceCount;
	float orthoLeft;
	float orthoRight;
	float orthoTop;
	float orthoBottom;
	uint32_t flags;
	uint32_t hudCores;
	uint32_t hudFirmware;
	uint32_t sceneColorMode;
	void *userMarkerBuf;
	void *activityBuf;
	uint32_t activityPackets;
	uint32_t userMarkerPackets;
	uint32_t paramBufSize;
	uint32_t gxmVerMajor;
	uint32_t gxmVerMinor;
	int32_t perfMode;
	void *gxmHostMem;
	SceGxmContext *gxmContext;
	SceGxmShaderPatcher *patcher;
	SceGxmShaderPatcherId vpId;
	SceGxmShaderPatcherId fpColorId;
	SceGxmShaderPatcherId fpTexId;
	SceGxmVertexProgram *vp;
	SceGxmFragmentProgram *fpTex;
	SceGxmFragmentProgram *fpColor;
	const SceGxmProgramParameter *wvpParam;
	SceGxmRenderTarget *rt;
	uint32_t rtWidth;
	uint32_t rtHeight;
	uint32_t msaaMode;
	uint32_t clipXMin;
	uint32_t clipYMin;
	uint32_t clipXMax;
	uint32_t clipYMax;
	SceGxmNotification notify[2];
	uint32_t swapCount;
	void *cpuMarkersSrc;
	void *gpuTimelineSrc;
	void *gpuFrameSrc;
	void *cpuActSrc;
	void *gpuBufSrc;
	void *gpuTimelineWidget;
	void *gpuFrameWidget;
	void *cpuMarkersWidget;
	void *cpuUtilWidget;
	void *cpuFpsWidget;
	void *cpuCoreWidgets[4];
	void *gpuBufWidget;
	void *txtLogView;
	void *csvLogView;
	void *xmlLogView;
	char txtFilename[256];
	char csvFilename[256];
	char xmlFilename[256];
	char traceFilename[256];
	uint32_t traceCountLen;
	void *paMgr;
	uint32_t traceCounters[16];
	uint32_t counterPad;
	uint64_t traceMask;
	uint32_t traceRate;
	uint32_t traceBufSize;
	uint32_t traceStopFrame;
	void *liveSession;
	uint8_t traceNeedStart;
	uint8_t traceTriggered;
	uint8_t perfLoaded;
	uint8_t widgetsBuilt;
	char hostConfigPath[512];
	void *savedConfig;
	uint8_t displayEnabled;
	uint8_t displayPad[7];
	uint64_t paTime;
	uint32_t tbFreq;
	uint32_t tbFreqHi;
	uint64_t gpuBaseUs;
	uint64_t cpuBaseUs;
	float gpuWinStart;
	float gpuWinEnd;
	float cpuWinStart;
	float cpuWinEnd;
	float cpuScale;
	float gpuScale;
	uint32_t sampleInterval;
	uint32_t frameCounter;
	uint32_t displayFrames;
	uint32_t useCpuBase;
} HudManager;

extern void *g_hudManager;
extern uint32_t g_paIoBaseAddress;
extern uint8_t g_userMarkerBufCustom;
extern void *g_userMarkerBufPtr;
extern uint32_t g_userMarkerBufSize;
extern uint64_t g_hudLastReloadTime;

static inline __attribute__((always_inline)) double hudU64ToDouble(uint64_t v)
{
	return (double)(uint32_t)(v >> 32) * 4294967296.0 + (double)(uint32_t)v;
}

static inline __attribute__((always_inline)) float hudU64ToFloat(uint64_t v)
{
	return (float)(uint32_t)(v >> 32) * 4294967296.0f + (float)(uint32_t)v;
}

static inline __attribute__((always_inline)) float hudI64ToFloat(int64_t v)
{
	if (v < 0)
		return -hudU64ToFloat((uint64_t)(-v));
	return hudU64ToFloat((uint64_t)v);
}

/* hud_mem.c */
void *hudMemFontSingleton(void);
int hudMemFontInit(void *fontObj);
int hudMemFontTerm(void *fontObj);
void *hudMemFontGetTexture(void *fontObj, int idx);
int hudMemInit(void);
int hudMemTerm(void);
void *hudPoolAlloc(uint32_t poolIdx, uint32_t size, uint32_t align);
void hudPoolFree(uint32_t poolIdx, void *ptr);
void *hudGetVdmRingMem(void);
void *hudGetVertexRingMem(void);
void *hudGetFragmentRingMem(void);
void *hudGetFragmentUsseRingMem(void);
uint32_t hudGetVdmRingSize(void);
uint32_t hudGetVertexRingSize(void);
uint32_t hudGetFragmentRingSize(void);
uint32_t hudGetFragmentUsseRingSize(void);
uint32_t hudGetFragmentUsseRingOffset(void);
void *hudGetPatcherBufMem(void);
void *hudGetPatcherVertexUsseMem(void);
void *hudGetPatcherFragmentUsseMem(void);
uint32_t hudGetPatcherBufSize(void);
uint32_t hudGetPatcherVertexUsseSize(void);
uint32_t hudGetPatcherFragmentUsseSize(void);
uint32_t hudGetPatcherVertexUsseOffset(void);
uint32_t hudGetPatcherFragmentUsseOffset(void);
void *hudNew(uint32_t size);
void *hudNewAligned(uint32_t size, uint32_t align);
void *hudNewArray(uint32_t size);
void hudDelete(void *ptr);
void hudDeleteArray(void *ptr);

/* hud_io.c */
void *hudAsyncFileInit(void *obj);
void *hudAsyncFileDestroy(void *obj);
void hudAsyncFileSetUseFile(void *obj, uint8_t useFile);
char *hudAsyncFileSetFilename(void *obj, const char *name);
const char *hudAsyncFileGetFilename(void *obj);
int hudAsyncFileEnsureOpen(void *obj);
int hudAsyncFileFlushAndClose(void *obj);
int hudAsyncFileReopen(void *obj);
int hudAsyncFilePrintf(void *obj, const char *fmt, ...);
void *hudViewBaseDtor(void *obj);
void *hudViewBaseDeletingDtor(void *obj);
void *hudViewBaseCtor(void *obj);
void hudViewSetEnabled(void *obj, uint8_t enabled);
void hudViewSetDataSource(void *obj, void *src);
void *hudLogBaseDtor(void *obj);
void *hudLogBaseDeletingDtor(void *obj);
void *hudLogBaseCtor(void *obj);
void hudLogSetUseFile(void *obj, uint8_t useFile);
char *hudLogSetFilename(void *obj, const char *name);
void *hudCsvLogDtor(void *obj);
void *hudCsvLogDeletingDtor(void *obj);
void hudCsvLogRender(void *obj);
void *hudCsvLogCtor(void *obj);
void *hudTxtLogDtor(void *obj);
void *hudTxtLogDeletingDtor(void *obj);
void hudTxtLogRender(void *obj);
void *hudTxtLogCtor(void *obj);
void *hudXmlLogDtor(void *obj);
void *hudXmlLogDeletingDtor(void *obj);
void hudXmlLogRender(void *obj);
void *hudXmlLogCtor(void *obj);

/* hud_settings.c */
int hudMgrGetPerfMode(void *mgr);
int hudMgrSetPerfMode(void *mgr, int mode);
void hudMgrSetParamBufSize(void *mgr, uint32_t val);
void hudMgrSetGxmVersions(void *mgr, uint32_t texVA, uint32_t texVB);
void hudMgrGetGxmVersions(void *mgr, uint32_t *texVA, uint32_t *texVB);
void hudMgrSetTraceConfig(void *mgr, const void *counters, uint64_t mask, uint32_t rate, uint32_t bufSize, uint32_t stopFrame);
void hudMgrGetTraceConfig(void *mgr, void *counters, uint64_t *mask, uint32_t *rate, uint32_t *bufSize, uint32_t *stopFrame);
char *hudMgrSetTxtFilename(void *mgr, const char *name);
char *hudMgrGetTxtFilename(void *mgr, char *dst, int size);
char *hudMgrSetCsvFilename(void *mgr, const char *name);
char *hudMgrGetCsvFilename(void *mgr, char *dst, int size);
char *hudMgrSetXmlFilename(void *mgr, const char *name);
char *hudMgrGetXmlFilename(void *mgr, char *dst, int size);
void hudMgrSetRenderTargetSize(void *mgr, uint32_t w, uint32_t h);
void hudMgrGetRenderTargetSize(void *mgr, uint32_t *w, uint32_t *h);
void hudMgrSetCoreCounts(void *mgr, uint32_t cores, uint32_t firmware);
void hudMgrGetCoreCounts(void *mgr, uint32_t *cores, uint32_t *firmware);
int hudMgrPollHostTool(void *mgr);
void hudMgrOnRingOverflow(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD);
void hudMgrOnRingInit(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD);
void hudMgrOnRingUpdate(void *mgr, int ringArgA, int ringArgB, int ringArgC, int ringArgD);
int hudMgrTraceTrigger(void *mgr);
int hudMgrTraceRingBufferSize(void *mgr);
int hudMgrTraceRingBufferUsed(void *mgr);
char *hudConfigSetBaseFilename(char *cfg, const char *base);
void *hudConfigCtor(void *cfg);
void *hudConfigDisableAll(void *cfg);
int hudConfigLookupMetricsGroup(uint32_t group, void *dst);
void hudConfigSaveFromManager(void *cfg, void *mgr);
int hudConfigApplyToManager(void *cfg, void *mgr);
void hudConfigApplyLine(void *cfg, uint32_t lineNo, const char *key, const char *val, const char *valUpper);
int hudConfigLoadFile(void *cfg, const char *path);

/* hud_live.c */
int hudMgrLiveGetMetricsGroup(void *mgr);
int hudMgrLiveSetMetricsGroup(void *mgr, int internal, uint32_t group);
int hudMgrLiveStart(void *mgr, int internal);
int hudMgrLiveIsRunning(void *mgr);
int hudMgrLiveStop(void *mgr, int internal);
int hudMgrLiveSetBuffer(void *mgr, int internal, void *buf, uint32_t size, void *results);
int hudMgrLiveDump(void *mgr, int internal, const char *path);
void hudMgrGxmFinish(void *mgr);
int hudMgrTraceSetFilename(void *mgr, const char *filename, uint32_t countLen);
char *hudMgrTraceGetFilename(void *mgr, char *dst, int size, uint32_t *countLen);
int hudMgrSetDisplayEnabled(void *mgr, uint8_t enabled);
int hudMgrSetDisplayFrameCount(void *mgr, int count);
int hudMgrGetDisplayFrameCount(void *mgr);
int hudMgrSetSceneColorMode(void *mgr, int mode);
int hudMgrGetSceneColorMode(void *mgr);
int hudMgrTraceSetMetricsGroup(void *mgr, int group);
int hudLiveThreadEntry(uint32_t arglen, void *argp);
int hudLiveStartPa(void *state, void *paMgr);
void *hudLiveCtor(void *obj, void *paMgr);
void *hudLiveDtor(void *obj);
int hudLiveGetMetricsGroup(void *obj);
int hudLiveSetMetricsGroup(void *obj, uint32_t group);
int hudLiveIsRunning(void *obj);
int hudLiveStart(void *obj);
int hudLiveStop(void *obj);
int hudLiveSetBuffer(void *obj, void *buf, uint32_t size, void *results);
int hudLiveGetCounters(void *obj, void *dst);
int hudLiveDump(void *obj, const char *path);

/* hud_pa.c */
uint32_t hudGpuGetClockFreq(void);
int hudGpuSetPerfCounters(const void *counters);
int hudGpuSetTraceEnable(int enable);
void *hudPaDtor(void *pa);
int hudPaSetup(void *pa);
int hudPaStart(void *pa);
int hudPaInsertBookmark(void *pa, uint32_t data);
int hudPaStop(void *pa);
int hudPaWritePmonFile(void *pa, uint32_t gpuFreq, const char *path);
int hudPaWriteNextPmon(void *pa, uint32_t gpuFreq);
int hudPaTeardown(void *pa);
int hudPaIsSetup(void *pa);
int hudPaIsRunning(void *pa);
char *hudPaSetFilename(void *pa, const char *name, uint32_t countLen);
void *hudPaCtor(void *pa, void *mgr);
void hudPaSetConfig(void *pa, const uint32_t *counters, uint32_t maskLo, uint32_t maskHi, uint32_t rate);
uint64_t hudPaGetMask(void *pa);
void hudPaSetBuffer(void *pa, int usePool, uint32_t size);
int hudPaSetTraceFlags(void *pa, int mode, int enableCounter, int enableGpu);
void hudPaGetBufferInfo(void *pa, uint32_t *outInfo);
int hudPaGetTraceUsed(void *pa);
void *hudPaDecoderCtor(void *dec);
void hudPaDecoderInit(void *dec, void *pa, uint32_t group, uint32_t gpuFreq, uint32_t tbFreq, uint32_t samplePeriod);
void hudPaDecoderSetGpuFreq(void *dec, uint32_t gpuFreq);
void hudPaDecoderProcessSample(void *dec, void *job, uint64_t timestamp, uint8_t type, uint8_t core, uint16_t cycles);
void hudPaDecoderEmitJob(void *dec, int isEnd, void *outCtx, void *job);
int hudPaDecoderProcessCounters(void *dec, void *outCtx);
int hudPaDecoderProcessBlock(void *dec, const uint16_t *block, void *outCtx);
int hudPaDecoderDrain(void *dec, const uint32_t *bufInfo, int flushFinal, uint32_t writePtr, void *outBuf, uint32_t outSize, uint32_t *outOffset, uint32_t *outEntries, uint32_t *outOverflows);

/* hud_data.c */
void *hudDataSrcBaseDtor(void *obj);
void hudDataSrcBaseReset(void *obj);
void *hudDataSrcBaseDeletingDtor(void *obj);
void *hudDataSrcBaseCtor(void *obj);
void *hudGpuBufSrcDtor(void *obj);
void *hudGpuBufSrcDeletingDtor(void *obj);
void hudGpuBufSrcUpdate(void *obj);
void *hudGpuBufSrcCtor(void *obj);
void hudGpuBufSrcOnOverflow(void *obj, int ringArgA, int ringArgB, int ringArgC, uint8_t ringArgD);
void hudGpuBufSrcOnInit(void *obj, uint32_t ringArgA, uint32_t ringArgB, uint32_t ringArgC, uint32_t ringArgD);
void hudGpuBufSrcOnUpdate(void *obj, uint32_t ringArgA, uint32_t ringArgB, uint32_t ringArgC, uint32_t ringArgD);
void hudGpuBufSrcSetParamBufSize(void *obj, uint32_t val);
void hudGpuBufSrcSetGpuTimeline(void *obj, void *tl);
void *hudGpuFrameSrcDtor(void *obj);
void *hudGpuFrameSrcDeletingDtor(void *obj);
void hudGpuFrameSrcUpdate(void *obj);
void *hudGpuFrameSrcCtor(void *obj);
void hudGpuFrameSrcSetGpuTimeline(void *obj, void *tl);
void *hudSeriesSrcDtor(void *obj);
void *hudSeriesSrcDeletingDtor(void *obj);
void *hudSeriesSrcCtor(void *obj);
void hudSeriesSrcPush(void *obj, uint32_t series, const float *val);
float *hudSeriesSrcGetSample(void *obj, uint32_t series, uint32_t idx);
uint32_t hudSeriesSrcGetCount(void *obj, uint32_t series);
uint32_t hudSeriesSrcGetSeriesCount(void *obj);
uint32_t hudSeriesSrcGetCapacity(void *obj);
void hudSeriesSrcSetColor(void *obj, uint32_t series, uint32_t color);
uint32_t hudSeriesSrcGetColor(void *obj, uint32_t series);
void hudSeriesSrcSetRange(void *obj, float minVal, float maxVal);
float *hudSeriesSrcGetMinPtr(void *obj);
float *hudSeriesSrcGetMaxPtr(void *obj);
void hudSeriesSrcSetAutoScale(void *obj, uint8_t autoMin, uint8_t autoMax, uint32_t window);
void hudSeriesSrcSetCapacity(void *obj, uint32_t cap);
void hudSeriesSrcSetSeriesCount(void *obj, uint32_t count);
void hudSeriesSrcSetStep(void *obj, float step);
float hudSeriesSrcGetStep(void *obj);
void hudCpuActSrcUpdate(void *obj);
void *hudCpuActSrcCtor(void *obj);
void hudCpuActSrcSetCapacity(void *obj, uint32_t cap);
void *hudCpuActSrcGetCoreSeries(void *obj, uint32_t core);
void *hudCpuActSrcGetTotalSeries(void *obj);
void *hudCpuActSrcGetFpsSeries(void *obj);
void *hudCpuActSrcDtor(void *obj);
void *hudCpuActSrcDeletingDtor(void *obj);
void *hudCpuMarkersSrcDtor(void *obj);
void *hudCpuMarkersSrcDeletingDtor(void *obj);
void *hudCpuMarkersSrcCtor(void *obj, void *timing, uint32_t maxPackets);
void hudSortU32(uint32_t *arr, uint32_t lo, uint32_t hi);
int hudCpuMarkersSrcCollect(void *obj);
int hudCpuMarkersSrcUpdate(void *obj);
void *hudTimelineSrcDtor(void *obj);
void *hudTimelineSrcDeletingDtor(void *obj);
void *hudTimelineSrcCtor(void *obj);
void hudTimelineSrcSetMinSpan(void *obj, float val);
void hudTimelineSrcSetTrackCount(void *obj, uint32_t count);
uint32_t hudTimelineSrcGetTrackCount(void *obj);
uint32_t hudTimelineSrcGetMarkerCount(void *obj);
uint32_t hudTimelineSrcGetEventCount(void *obj, uint32_t track);
void *hudTimelineSrcGetMarker(void *obj, uint32_t idx);
void *hudTimelineSrcGetEvent(void *obj, uint32_t track, uint32_t idx);
void *hudTimelineSrcAddMarker(void *obj, float t, uint32_t color);
void *hudTimelineSrcBeginEvent(void *obj, uint32_t track, float t, uint32_t color);
void hudTimelineSrcEndEvent(void *obj, uint32_t track, float t, uint32_t color);
void hudTimelineSrcClearMarkers(void *obj);
void hudTimelineSrcClearTracks(void *obj);
void hudTimelineSrcGetTimeWindow(void *obj, float *outStart, float *outEnd);
void hudTimelineSrcSetTimeWindow(void *obj, float start, float end);
void hudTimelineSrcReset(void *obj);
void *hudGpuTimelineSrcDtor(void *obj);
void *hudGpuTimelineSrcDeletingDtor(void *obj);
void *hudGpuTimelineSrcCtor(void *obj, void *timing);
void hudGpuTimelineNull1(void);
void hudGpuTimelineNull2(void);
void hudGpuTimelineNull4(void);
void hudGpuTimelineNull3(void);
void hudGpuTimelineSrcSetCores(void *obj, uint32_t cores, uint32_t fw);
void hudGpuTimelineSrcSetColorMode(void *obj, uint32_t mode);
void hudGpuTimelineSrcSetLogEnabled(void *obj, uint8_t enabled);
uint32_t hudGpuTimelineSrcGetKickCount(void *obj);
void *hudGpuTimelineSrcGetKick(void *obj, uint32_t idx);
void hudGpuTimelineSrcProcess(void *obj);
uint32_t hudGpuTimelineSrcGetFrameSeq(void *obj);
float hudGpuTimelineSrcSumVertexMs(void *obj);
float hudGpuTimelineSrcSumFragmentMs(void *obj);
float hudGpuTimelineSrcGetFrameMs(void *obj);
uint32_t hudGpuTimelineSrcGetParamBufferUsed(void *obj);
void hudGpuTimelineSrcUpdate(void *obj);
float hudCpuMarkersGetWindowScale(void *obj);
float hudTimelineGetWindowScale(void *obj);
float hudGpuTimelineGetWindowScale(void *obj);

/* hud_render.c */
void *hudPatcherHostAlloc(void *userData, uint32_t size);
void hudPatcherHostFree(void *userData, void *mem);
void *hudMgrCtor(void *mgr);
void *hudMgrDtor(void *mgr);
uint64_t hudMgrRefreshTime(void);
void hudMgrSetFlags(void *mgr, uint32_t flags);
uint32_t hudMgrGetFlags(void *mgr);
int hudMgrStartCpuPerf(void *mgr);
int hudMgrSetUserMarkerBuffer(void *buf, uint32_t size);
void hudMgrLayoutWidgets(void *mgr, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax);
void hudMgrDestroyWidgets(void *mgr);
void hudMgrRebuildWidgets(void *mgr);
void hudMgrDestroyShaders(void *mgr);
int hudMgrCreateShaders(void *mgr);
void hudMgrStopAll(void *mgr, int unloadPerf);
int hudMgrCreateRenderTarget(void *mgr);
void hudMgrOnSwap(void *mgr, const SceGxmColorSurface *colorSurface, SceGxmSyncObject *fragmentSyncObject);
void *hudWidgetBaseDtor(void *obj);
void hudWidgetBaseRender(void *obj, void *drawCtx);
void *hudWidgetBaseDeletingDtor(void *obj);
void *hudWidgetBaseCtor(void *obj);
void hudWidgetSetTop(void *obj, float y);
void hudWidgetSetRect(void *obj, float x, float y, float w, float h);
char *hudWidgetSetTitle(void *obj, const char *title);
void hudWidgetSetTitleStyle(void *obj, uint32_t style, float fontSize);
void hudWidgetAddRect(void *obj, float posXA, float posYA, float posXB, float posYB, uint32_t color);
void hudWidgetAddLine(void *obj, float posXA, float posYA, float posXB, float posYB, uint32_t color);
int hudWidgetAddText(void *obj, float x, float y, const char *fmt, ...);
void hudWidgetDrawFrame(void *obj);
void *hudLineGraphDtor(void *obj);
void *hudLineGraphDeletingDtor(void *obj);
void *hudLineGraphCtor(void *obj);
void hudLineGraphBuild(void *obj);
void *hudBarGraphDtor(void *obj);
void *hudBarGraphDeletingDtor(void *obj);
void *hudBarGraphCtor(void *obj, uint32_t mode);
void hudBarGraphBuild(void *obj);
void *hudTimelineWidgetDtor(void *obj);
void *hudTimelineWidgetDeletingDtor(void *obj);
void *hudTimelineWidgetCtor(void *obj);
void hudTimelineWidgetSetAutoHeight(void *obj, uint32_t mode);
void hudTimelineWidgetSetRowHeight(void *obj, float h);
void hudTimelineWidgetBuild(void *obj);

/* hud_markers.c / hud_hooks.S */
void hudThreadFilterEnableAll(void *filter, uint8_t enable);
int hudThreadFilterIsEnabled(void *filter, int threadId);
int hudThreadFilterSetThread(void *filter, int threadId, uint8_t enable);
void *hudThreadFilterGetInstance(void);
void hudHostToolMarker(void);
void hudHostToolSetStatus(uint64_t status);
void hudHostToolNotifyDone(uint64_t status);
int hudHostToolPollStart(void *outPath, uint32_t maxLen);
int hudHostToolPollTrigger(void);
int hudHostToolPollCancel(void);
void hudHostToolHook(void);
void *hudMemmoveWrap(void *dst, const void *src, uint32_t n);
void *hudMemsetWrap(void *dst, int c, uint32_t n);

/* librazorhud_es4.c helpers */
uint64_t hudGetProcessTimeAndStore(void);
uint32_t hudFindParamResourceIndex(const SceGxmProgram *prog, const char *name);
const SceGxmProgramParameter *hudFindProgramParam(SceGxmRegisteredProgram *progId, const char *name);
const char *hudFormatInt64Commas(int64_t val);
int hudGuardAcquire(uint32_t *guard);
void hudGuardRelease(uint32_t *guard);
int hudReloadConfigPath(const char *path);
int hudLoadDefaultConfig(void *cfg);
int hudLiveSetMetricsGroupInternal(int internal, uint32_t metrics);
int hudLiveStartInternal(int internal);
int hudLiveStopInternal(int internal);
int hudLiveSetBufferInternal(int internal, void *buffer, uint32_t size, void *results);
void hudGxmFinishInternal(void);

#endif /* RAZOR_HUD_INTERNAL_H */
