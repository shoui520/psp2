/*
 * SceLibMp4Recorder internal declarations and class layouts.
 */
#ifndef MP4REC_INTERNAL_H
#define MP4REC_INTERNAL_H

#include <mp4rec.h>
#include <scetypes.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <appmgr.h>
#include <libSceVideoExport/video_export.h>

#ifdef __cplusplus
void *operator new(size_t size, void *ptr);
extern "C" {
#endif

/* SceLibc mspace */
typedef void *SceLibcMspace;

typedef struct SceLibcMallocManagedSize {
	SceSize size;
	SceSize version;
	SceSize currentSystemSize;
	SceSize maxSystemSize;
	SceSize currentInuseSize;
	SceSize maxInuseSize;
} SceLibcMallocManagedSize;

SceLibcMspace mspace_create(void *base, size_t capacity);
int32_t mspace_destroy(SceLibcMspace msp);
void *mspace_malloc(SceLibcMspace msp, size_t size);
void *mspace_memalign(SceLibcMspace msp, size_t boundary, size_t size);
void mspace_free(SceLibcMspace msp, void *ptr);
int32_t mspace_malloc_stats(SceLibcMspace msp, SceLibcMallocManagedSize *mmsize);

/* Kernel / AppMgr / RegMgr / Clib imports */
SceUID sceKernelGetProcessId(void);
uint64_t sceKernelGetProcessTimeWide(void);
int32_t SceThreadmgrCoredumpTime_sceKernelExitThread(int32_t status);
char *sceClibStrncpy(char *dst, const char *src, size_t len);
void *sceClibMemcpy_safe(void *dst, const void *src, size_t len);
void *sceClibMemmove(void *dst, const void *src, size_t len);

int32_t sceAppMgrAppParamGetString(SceUID pid, int32_t param, char *str, SceSize len);
int32_t sceAppMgrWorkDirMountById(int32_t id, const char *titleId, char *mountPoint);
int32_t sceAppMgrUmount(const char *mountPoint);
int32_t sceAppMgrGetDevInfo(const char *dev, uint64_t *maxSize, uint64_t *freeSize);
int32_t sceAppMgrSendNotificationRequest(const void *param, int32_t flag);
int32_t sceRegMgrSystemParamGetInt(int32_t key, int32_t *outVal);

/* CodecEngine imports */
SceUID sceCodecEngineOpenUnmapMemBlock(void *pMemBlock, SceUInt32 memBlockSize);
SceInt32 sceCodecEngineCloseUnmapMemBlock(SceUID uid);
SceUIntVAddr sceCodecEngineAllocMemoryFromUnmapMemBlock(SceUID uid, SceUInt32 size, SceUInt32 alignment);
SceInt32 sceCodecEngineFreeMemoryFromUnmapMemBlock(SceUID uid, SceUIntVAddr addr);

/* SceVideoencUser / SceAvcencUser imports */
typedef struct SceVideoencInitParam {
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint32_t maxFrameRateNum;
	uint32_t maxFrameRateDen;
	uint32_t reserved[3];
} SceVideoencInitParam;

typedef struct SceVideoencUnmapMem {
	uint32_t reserved[3];
	SceUIntVAddr addr;
	uint32_t size;
} SceVideoencUnmapMem;

typedef struct SceAvcencQueryParam {
	uint32_t framePitch;
	uint32_t frameWidth;
	uint32_t frameHeight;
	uint32_t displayWidth;
	uint32_t displayHeight;
	uint32_t profile;
	uint32_t targetBitrate;
	uint32_t maxBitrate;
	uint8_t flags;
	uint8_t level;
	uint8_t pad0[2];
	uint32_t frameRateNum;
	uint32_t frameRateDen;
	uint8_t rcMode;
	uint8_t colorMatrix;
	uint8_t pad1[2];
	uint16_t reserved0;
	uint16_t reserved1;
} SceAvcencQueryParam;

typedef struct SceAvcencMemSizeInfo {
	uint32_t codecMemSize;
	uint32_t workMemSize;
	uint32_t maxAuSize;
} SceAvcencMemSizeInfo;

typedef struct SceAvcencEncoderHandle {
	uint32_t internal;
	SceUIntVAddr codecMemAddr;
	uint32_t codecMemSize;
	SceUIntVAddr workMemAddr;
	uint32_t workMemSize;
	void *contextMem;
	uint32_t contextMemSize;
	void *spsData;
	uint32_t spsSize;
	void *ppsData;
	uint32_t ppsSize;
	uint32_t reserved[2];
} SceAvcencEncoderHandle;

typedef struct SceAvcencEncodeParam {
	uint32_t reserved[3];
	uint32_t idrInterval;
	uint32_t reserved2[2];
} SceAvcencEncodeParam;

typedef struct SceAvcencPicture {
	uint32_t size;
	uint32_t pixelType;
	uint32_t framePitch;
	uint32_t frameWidth;
	uint32_t frameHeight;
	uint8_t opt[16];
	void *pPicture[2];
	uint32_t flags;
} SceAvcencPicture;

typedef struct SceAvcencAuInfo {
	uint32_t size;
	uint32_t ptsSec;
	uint32_t ptsSubNum;
	uint32_t ptsSubDen;
	uint32_t dtsSec;
	uint32_t dtsSubNum;
	uint32_t dtsSubDen;
	uint32_t esAddr;
	uint32_t esSize;
	uint8_t nalCount;
	uint8_t frameType;
	uint8_t reserved[2];
} SceAvcencAuInfo;

typedef struct SceAvcencOutput {
	uint32_t numOutput;
	uint32_t maxOutput;
	SceAvcencAuInfo **auList;
} SceAvcencOutput;

typedef struct SceAvcencNalUnit {
	uint32_t addr;
	uint32_t size;
	uint8_t nalType;
	uint8_t reserved[3];
} SceAvcencNalUnit;

int32_t sceVideoencQueryMemSize(uint32_t codecType, uint32_t flags,
	const SceVideoencInitParam *initParam, uint32_t *outMemSize);
int32_t sceVideoencInitLibraryWithUnmapMem(uint32_t codecType,
	const SceVideoencUnmapMem *unmapMem, uint32_t flags, const SceVideoencInitParam *initParam);
int32_t sceVideoencTermLibrary(uint32_t codecType);
int32_t sceAvcencQueryEncoderMemSize(uint32_t codecType, const SceAvcencQueryParam *param,
	SceAvcencMemSizeInfo *outInfo);
int32_t sceAvcencCreateEncoder(uint32_t codecType, SceAvcencEncoderHandle *handle,
	const SceAvcencQueryParam *param);
int32_t sceAvcencSetEncoderParameter(SceAvcencEncoderHandle *handle,
	const SceAvcencEncodeParam *param);
int32_t sceAvcencDeleteEncoder(SceAvcencEncoderHandle *handle);
int32_t sceAvcencEncode(SceAvcencEncoderHandle *handle, const SceAvcencPicture *pic,
	SceAvcencOutput *out);
int32_t sceAvcencEncodeStop(SceAvcencEncoderHandle *handle, SceAvcencOutput *out);
int32_t sceAvcencGetNalUnit(SceAvcencEncoderHandle *handle, int32_t index,
	SceAvcencNalUnit *outNal);
int32_t sceAvcencCsc(void *dst, const void *src);

/* SceAacencInternal imports */
typedef struct Mp4RecNaacConfig {
	int32_t numChannels;
	int32_t sampleRate;
	int32_t bitRate;
	int32_t audioObjectType;
	int32_t streamFormat;
	uint32_t enableTns;
	uint32_t enableMs;
	uint32_t disablePns;
	const int32_t *channelOrder;
	const int32_t *channelStride;
	uint32_t reserved[22];
} Mp4RecNaacConfig;

int32_t SceAacencInternal_118F588C(void);
int32_t SceAacencInternal_71BDD121(void **handleOut, void *workBuffer);
int32_t SceAacencInternal_B3004C1B(void *handle);
int32_t SceAacencInternal_C867CE87(void *handle, const Mp4RecNaacConfig *config);
int32_t SceAacencInternal_B8BEEDE6(void *handle, const void *pcmIn, uint32_t inputBytes,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes);
int32_t SceAacencInternal_30D75275(void *handle, uint8_t *outBuf, int32_t *outBytes, int32_t flag);

/* Internal error codes */
#define MP4REC_AVREC_ERR_OUT_OF_MEMORY          ((int32_t)0x80106F01)
#define MP4REC_AVREC_ERR_FATAL                  ((int32_t)0x80106F02)
#define MP4REC_AVREC_ERR_INVALID_VALUE          ((int32_t)0x80106F03)
#define MP4REC_AVREC_ERR_VENC_BUFFER_FULL       ((int32_t)0x80106F04)
#define MP4REC_AVREC_ERR_BUFFER_FULL            ((int32_t)0x80106F08)
#define MP4REC_AVREC_ERR_SAMPLE_NUM_FULL        ((int32_t)0x80106F09)
#define MP4REC_AVREC_ERR_VIDEO_BUFFER_FULL      ((int32_t)0x80106F0A)
#define MP4REC_AVREC_ERR_VIDEO_SAMPLE_NUM_FULL  ((int32_t)0x80106F0B)
#define MP4REC_AVREC_ERR_AUDIO_BUFFER_FULL      ((int32_t)0x80106F0C)
#define MP4REC_AVREC_ERR_AUDIO_SAMPLE_NUM_FULL  ((int32_t)0x80106F0D)
#define MP4REC_AVREC_ERR_VENC_INVALID_POINTER   ((int32_t)0x80106F0E)

#define MP4REC_NAAC_ERR_INVALIDARG              ((int32_t)0x80106F41)
#define MP4REC_NAAC_ERR_INVALIDHANDLE           ((int32_t)0x80106F42)
#define MP4REC_NAAC_ERR_OUTOFMEMORY             ((int32_t)0x80106F43)
#define MP4REC_NAAC_ERR_INTERNAL                ((int32_t)0x80106F44)
#define MP4REC_NAAC_ERR_FRAME_LIMIT             ((int32_t)0x80106F45)
#define MP4REC_NAAC_ERR_NOTIMPL                 ((int32_t)0x80106F46)
#define MP4REC_NAAC_ERR_FATAL                   ((int32_t)0x80106F47)

#define MP4REC_FF4_ERR_ARGS                     ((int32_t)0x80106F81)
#define MP4REC_FF4_ERR_STATE                    ((int32_t)0x80106F82)
#define MP4REC_FF4_ERR_NOMEM                    ((int32_t)0x80106F83)
#define MP4REC_FF4_ERR_FILE                     ((int32_t)0x80106F84)
#define MP4REC_FF4_ERR_NOTFOUND                 ((int32_t)0x80106F85)
#define MP4REC_FF4_ERR_OVERFLOW                 ((int32_t)0x80106F86)
#define MP4REC_FF4_ERR_FORMAT                   ((int32_t)0x80106F87)
#define MP4REC_FF4_ERR_INTERNAL                 ((int32_t)0x80106F88)
#define MP4REC_FF4_ERR_FATAL                    ((int32_t)0x80106F89)

#define MP4REC_HANDLE_MAGIC                     0x0FFFFFF0U
#define MP4REC_THR_WRITE_MAGIC                  0x54574642U /* "TWFB" */
#define MP4REC_MIN_FREE_SPACE                   (10U * 1024U * 1024U)
#define MP4REC_WRITE_BLOCK_SIZE                 0x20000U
#define MP4REC_WRITE_BLOCK_COUNT                4U

/* PNG helper (Sony scePng) */
int32_t scePngGetOutputInfo(const void *src, uint32_t insize,
	int32_t *width, int32_t *height, int32_t *outputFormat, int32_t *streamFormat);
int32_t scePngDec(void *dst, uint32_t outsize, const void *src, uint32_t insize,
	int32_t *width, int32_t *height, int32_t *format);
int32_t scePngConvertToRGBA(void *dst, const void *src,
	int32_t width, int32_t height, int32_t format);

/* Ring queue (used by BufferPool / ThrWriteFileBuf) */
typedef struct BufferQueueEntry {
	void *data;
	uint32_t size;
} BufferQueueEntry;

typedef struct BufferQueue {
	SceKernelLwMutexWork mutex;
	BufferQueueEntry *entries;
	uint32_t flags;
	uint32_t writeIndex;
	uint32_t readIndex;
	uint32_t count;
	uint32_t entrySize;
	uint32_t capacity;
} BufferQueue;

typedef void (*BufferPoolCallbackFn)(int32_t event, int32_t count, int32_t capacity, void *userArg);

typedef struct BufferPoolCallbackEntry {
	BufferPoolCallbackFn fn;
	void *userArg;
} BufferPoolCallbackEntry;

typedef struct BufferPoolCallbackList {
	uint32_t count;
	BufferPoolCallbackEntry *entries;
} BufferPoolCallbackList;

int32_t BufferQueueReset(BufferQueue *queue);
int32_t BufferQueueInit(BufferQueue *queue, uint32_t entrySize, uint32_t capacity,
	BufferQueueEntry *entries, const char *mutexName);
int32_t BufferQueueTerm(BufferQueue *queue);
int32_t BufferQueueGetFreeCount(BufferQueue *queue);
int32_t BufferQueueGetUsedCount(BufferQueue *queue);
int32_t BufferQueueGetCapacity(BufferQueue *queue);
int32_t BufferQueuePeekWrite(BufferQueue *queue, uint32_t offset, BufferQueueEntry **outEntry);
int32_t BufferQueuePeekRead(BufferQueue *queue, uint32_t offset, BufferQueueEntry **outEntry);
int32_t BufferQueueCommitWrite(BufferQueue *queue, uint32_t count);
int32_t BufferQueueCommitRead(BufferQueue *queue, uint32_t count);
int32_t BufferPoolNotifyCallbacks(BufferPoolCallbackList *cbList, int32_t event,
	int32_t count, int32_t capacity);

/* Mode table entry */
typedef struct Mp4RecModeSpec {
	uint32_t width;
	uint32_t pitch;
	uint32_t height;
	uint32_t frameBytes;
	uint32_t bitrate;
} Mp4RecModeSpec;

/* Avrec configuration (64 bytes) */
typedef struct AvrecConfig {
	uint32_t format;
	uint32_t videoCodec;
	uint32_t width;
	uint32_t pitch;
	uint32_t height;
	uint32_t targetBitrate;
	uint32_t maxBitrate;
	uint32_t frameRateCode;
	uint16_t rotation;
	uint16_t pad0;
	uint32_t audioCodec;
	uint32_t audioSampleRateCode;
	uint32_t audioBitrate;
	int32_t writeThreadAffinity;
	int32_t writeThreadPriority;
	uint32_t reserved;
	uint32_t modeOption;
} AvrecConfig;

typedef struct AvrecMemReq {
	uint32_t heapSize;
	uint32_t phyMemSize;
} AvrecMemReq;

typedef struct AvrecVideoFrameMeta {
	uint32_t dts;
	uint32_t pts;
	uint8_t frameType;
	uint8_t pad[3];
} AvrecVideoFrameMeta;

typedef struct AvrecMetadataItem {
	uint32_t type;
	uint8_t flag;
	uint8_t pad0;
	uint16_t lang;
	uint16_t encoding;
	uint16_t pad1;
	void *data;
	uint32_t dataSize;
	void *extraData;
	uint32_t extraSize;
} AvrecMetadataItem;

typedef struct AvrecThumbnailDesc {
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t format;
	void *data;
	uint32_t size;
} AvrecThumbnailDesc;

typedef struct RecorderStartParam {
	const char *filePath;
	void *allocIf;
	void *encoderPhyPtr;
	uint32_t encoderPhySize;
	uint32_t modeIndex;
	int32_t affinity;
	int32_t priority;
	uint32_t modeOption;
} RecorderStartParam;

typedef struct RecorderTermInfo {
	SceRtcTick startUtcTick;
	uint32_t reserved0;
	uint32_t reserved1;
	SceMp4RecMetadata *metadata;
	char *titleId;
} RecorderTermInfo;

typedef struct Mp4RecAvRecBufInfo {
	void *ptr;
	uint32_t size;
} Mp4RecAvRecBufInfo;

/* Utility functions */
uint32_t Mp4RecTranslateError(uint32_t err);
int32_t Mp4RecFormatDateUtf16Be(SceUInt64 tick, char *outBuf, size_t outSize);
uint32_t Mp4RecRescaleDuration(uint32_t ticks, uint32_t srcScale, uint32_t dstScale, uint32_t *outVal);
int32_t Mp4RecBuildChapterData(uint8_t *outBuf, uint32_t outSize, const uint32_t *chapters,
	uint32_t chapterCount, uint32_t totalDuration90k);
int32_t Mp4RecConvertUtf8ToUtf16Be(const char *src, uint32_t srcLen, uint16_t *dst, uint32_t dstChars);

#ifdef __cplusplus
}

/* Placement new helper */

/* Abstract low-level allocator interface */
class AllocatorInterface {
public:
	virtual ~AllocatorInterface() {}
	virtual void *Memalign(size_t align, size_t size) = 0;
	virtual void *Malloc(size_t size) = 0;
	virtual void Free(void *ptr) = 0;
};

class MspaceAllocatorInterface : public AllocatorInterface {
public:
	explicit MspaceAllocatorInterface(SceLibcMspace msp) : m_mspace(msp) {}
	virtual ~MspaceAllocatorInterface() {}
	virtual void *Memalign(size_t align, size_t size) { return mspace_memalign(m_mspace, align, size); }
	virtual void *Malloc(size_t size) { return mspace_malloc(m_mspace, size); }
	virtual void Free(void *ptr) { mspace_free(m_mspace, ptr); }
	virtual int32_t GetStats() { return 0; }

	SceLibcMspace m_mspace;
};

/* Abstract memory allocator with physical memory backing info */
class MemAllocator {
public:
	virtual ~MemAllocator() {}
	virtual int32_t Reset() = 0;
	virtual int32_t Term() = 0;
	virtual void *Malloc(size_t size) = 0;
	virtual void *Memalign(size_t align, size_t size) = 0;
	virtual void Free(void *ptr) = 0;
	virtual size_t GetMaxUsed() = 0;

	uint32_t m_phyMemSize;
	void *m_phyMemBase;
	uint8_t m_initialized;
	uint8_t m_pad[3];
};

class MspaceMemAllocator : public MemAllocator {
public:
	MspaceMemAllocator();
	virtual ~MspaceMemAllocator();
	virtual int32_t Reset();
	virtual int32_t Term();
	virtual void *Malloc(size_t size);
	virtual void *Memalign(size_t align, size_t size);
	virtual void Free(void *ptr);
	virtual size_t GetMaxUsed();

	int32_t Init(size_t heapSize, void *heapBase, uint32_t phySize, void *phyBase);

	size_t m_heapSize;
	void *m_heapBase;
	SceLibcMspace m_mspace;
};

class ExternalMemAllocator : public MemAllocator {
public:
	ExternalMemAllocator();
	virtual ~ExternalMemAllocator();
	virtual int32_t Reset();
	virtual int32_t Term();
	virtual void *Malloc(size_t size);
	virtual void *Memalign(size_t align, size_t size);
	virtual void Free(void *ptr);
	virtual size_t GetMaxUsed();

	int32_t Init(AllocatorInterface *allocIf, uint32_t phySize, void *phyBase);
	AllocatorInterface *GetInterface();

	AllocatorInterface *m_allocIf;
};

/* LwMutex wrapper */
class Mp4RecMutex {
public:
	Mp4RecMutex(int32_t *outErr, const char *name, uint8_t attrFlags);
	~Mp4RecMutex();

	int32_t Lock();
	int32_t Unlock();
	int32_t TryLock();

	SceKernelLwMutexWork m_work;
};

/* Buffer pool block descriptor (12 bytes) */
typedef struct BufferPoolBlock {
	void *data;
	uint32_t usedSize;
	uint32_t capacity;
} BufferPoolBlock;

typedef struct BufferPool {
	BufferQueue queue;
	BufferQueueEntry *entries;
	uint32_t pad;
	BufferPoolCallbackList *cbList;
	MemAllocator *allocator;
} BufferPool;

int32_t BufferPoolFreeBlocks(BufferPool *pool, int32_t count);
int32_t BufferPoolInit(BufferPool *pool, MemAllocator *allocator, uint32_t blockSize,
	uint32_t blockCount, const char *mutexName, BufferPoolCallbackList *cbList);
int32_t BufferPoolTerm(BufferPool *pool);
int32_t BufferPoolGetReadBlock(BufferPool *pool, BufferPoolBlock **outBlock);
int32_t BufferPoolReleaseReadBlock(BufferPool *pool);
int32_t BufferPoolGetWriteBlock(BufferPool *pool, BufferPoolBlock **outBlock);
int32_t BufferPoolCommitWriteBlock(BufferPool *pool);
int32_t BufferPoolGetQueuedCount(BufferPool *pool);

/* Async threaded file writer (208 bytes) */
typedef void (*LowSpaceCallbackFn)(uint64_t freeSpace);

typedef struct ThrWriteFileBuf {
	SceKernelLwMutexWork dataMutex;
	SceKernelLwMutexWork apiMutex;
	uint32_t magic;
	uint32_t pad0;
	BufferPool pool;
	uint32_t initFlags;
	SceUID fd;
	SceUID threadId;
	SceUID threadEvf;
	SceUID ackEvf;
	uint32_t maxBlocks;
	int32_t lastError;
	MemAllocator *allocator;
	int32_t threadPriority;
	int32_t threadAffinity;
	LowSpaceCallbackFn lowSpaceCb;
	uint32_t pad1;
	uint64_t lowSpaceThreshold;
} ThrWriteFileBuf;

int32_t ThrWriteFileBufTerm(ThrWriteFileBuf *ctx);
int32_t ThrWriteFileBufInit(ThrWriteFileBuf *ctx, MemAllocator *allocator, SceUID fd,
	int32_t priority, int32_t affinity);
int32_t ThrWriteFileBufAcquire(ThrWriteFileBuf *ctx, void **outBuf, uint32_t *outSize);
int32_t ThrWriteFileBufSubmit(ThrWriteFileBuf *ctx, void *buf, uint32_t size, uint32_t *outWritten);
int32_t ThrWriteFileBufFlush(ThrWriteFileBuf *ctx);
int32_t ThrWriteFileBufSetLowSpaceCallback(ThrWriteFileBuf *ctx, void *unused,
	uint64_t threshold, LowSpaceCallbackFn cb);
int32_t ThrWriteFileBufThreadMain(SceSize args, void *argp);

/* Buffered file wrapper embedded in Mp4Muxer (256 bytes) */
class BufferedFileWriter {
public:
	BufferedFileWriter();
	~BufferedFileWriter();

	int32_t Term();
	int32_t Init(SceUID fd, MemAllocator *allocator, int32_t affinity, int32_t priority);
	int32_t Flush();
	int32_t SetLowSpaceCallback(void *unused, uint64_t threshold, LowSpaceCallbackFn cb);
	int32_t DirectWriteWithSpaceCheck(SceUID fd, const void *buf, uint32_t unused,
		uint32_t size, uint32_t unused2, uint64_t *outWritten);

	void *m_curBuf;
	uint32_t m_curPos;
	SceUID m_fd;
	uint32_t m_pad0;
	uint64_t m_fileOffset;
	uint64_t m_fileSize;
	MemAllocator *m_allocator;
	uint32_t m_pad1;
	ThrWriteFileBuf m_thrWriter;
	LowSpaceCallbackFn m_directLowSpaceCb;
	uint32_t m_pad2;
	uint64_t m_directLowSpaceThreshold;
};

/* Sample ring buffer & iterator */
typedef struct SampleSpan {
	void *data;
	uint32_t size;
} SampleSpan;

class SampleIterator {
public:
	SampleIterator(SampleSpan *spans, uint32_t count);
	~SampleIterator();
	SampleSpan *Next();

	SampleSpan *m_spans;
	uint32_t m_count;
	uint32_t m_index;
};

class SampleRingBuffer {
public:
	SampleRingBuffer();
	~SampleRingBuffer();

	static uint32_t CalcWorkMemSize(uint32_t dataSize, uint32_t maxSamples);
	int32_t Term(MemAllocator *allocator);
	int32_t Init(uint32_t dataSize, uint32_t maxSamples, MemAllocator *allocator);
	int32_t Push(const void *data, size_t size);
	uint32_t GetTotalPushed();
	int32_t MarkChunkAt(uint32_t sampleIndex);
	int32_t MarkChunkNow();
	bool HasReadyChunk();
	SampleIterator GetReadyChunkIterator();
	int32_t ConsumeReadyChunk();
	SampleIterator GetAllSamplesIterator();
	uint32_t GetFreeSpace();

	uint32_t m_reserved;
	uint32_t m_dataCapacity;
	uint32_t m_maxSamples;
	uint8_t *m_dataBase;
	uint8_t *m_writePtr;
	SampleSpan *m_spans;
	uint32_t m_spanCount;
	uint32_t *m_chunkCounts;
	uint32_t m_chunkCount;
	uint32_t m_lastMarkedSample;
	uint32_t m_totalPushed;
	uint32_t m_prevMarkedTotal;
	uint32_t m_defaultLimit;
};

/* AAC encoder wrapper (12 bytes) */
class AacEncoder {
public:
	static int32_t QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req);
	AacEncoder();
	virtual ~AacEncoder();

	int32_t Term(MemAllocator *allocator);
	int32_t Init(const AvrecConfig *cfg, MemAllocator *allocator, uint32_t *outMaxAuSize);
	int32_t Encode(const void *pcm, uint32_t pcmBytes, void *outBuf, uint32_t *inOutSize, int32_t isFlush);
	int32_t Flush();

	void *m_handle;
	void *m_workBuf;
};

/* AVC encoder wrapper (108 bytes) */
class AvcEncoder {
public:
	static int32_t BuildParams(const AvrecConfig *cfg, SceVideoencInitParam *videoInit,
		SceAvcencQueryParam *queryParam, uint32_t *outLibMemSize, SceAvcencMemSizeInfo *outEncMem);
	static int32_t QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req);

	AvcEncoder();
	virtual ~AvcEncoder();

	int32_t Term(MemAllocator *allocator);
	int32_t Init(const AvrecConfig *cfg, MemAllocator *allocator, uint32_t *outMaxAuSize);
	int32_t Encode(const void *yuv, uint32_t yuvSize, void *outBuf, uint32_t *inOutSize,
		AvrecVideoFrameMeta *outMeta, int32_t *inOutMode);
	int32_t GetParameterSets(void **outSps, uint32_t *outSpsSize, void **outPps, uint32_t *outPpsSize);
	int32_t GetMaxGopFrames();
	uint32_t GetMaxFrameBytes();

	uint32_t m_reserved0;
	uint32_t m_initFlags;
	uint32_t m_width;
	uint32_t m_pitch;
	uint32_t m_height;
	SceUID m_unmapUid;
	SceUIntVAddr m_libMemAddr;
	SceUIntVAddr m_codecMemAddr;
	SceUIntVAddr m_workMemAddr;
	void *m_contextMem;
	SceAvcencEncoderHandle m_encHandle;
	uint8_t m_flag96;
	uint8_t m_pad97[3];
	uint32_t m_framesSinceIdr;
	uint32_t m_encodedCount;
};

/* Forward declaration of FF4 handle */
typedef void *Ff4SessionHandle;
typedef void *Ff4WriterHandle;

/* MP4 muxer wrapper (560 bytes) */
class Mp4Muxer {
public:
	static int32_t QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req);
	Mp4Muxer();
	virtual ~Mp4Muxer();

	static int32_t MapFf4Error(int32_t ff4Err);
	int32_t Init(const AvrecConfig *cfg, const char *filePath, MemAllocator *allocator);
	int32_t WriteVideoSamples(SampleIterator *it);
	int32_t WriteAudioSamples(SampleIterator *it);
	int32_t Finalize(const uint16_t *creationDate);
	int32_t Term();
	int32_t FlushInterleaved();
	int32_t AddVideoAu(const void *data, size_t size, const AvrecVideoFrameMeta *meta);
	int32_t AddAudioAu(const void *data, size_t size);
	int32_t SetAvcConfig(const void *sps, uint32_t spsSize, const void *pps, uint32_t ppsSize);
	int32_t AddMetadata(const AvrecMetadataItem *item);
	int32_t PrepareThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count, int32_t flag);
	int32_t WriteThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count);

	MemAllocator *m_allocator;
	uint32_t m_pad8;
	BufferedFileWriter *m_fileWriterPtr;
	void *m_ff4Callbacks[14];
	Ff4WriterHandle m_ff4Writer;
	Ff4SessionHandle m_ff4Session;
	uint64_t m_currentOffset;
	uint64_t m_mdatStartOffset;
	uint64_t m_mdatDataOffset;
	uint32_t m_videoTrackId;
	uint32_t m_audioTrackId;
	uint32_t m_videoSampleDescId;
	uint32_t m_audioSampleDescId;
	SampleRingBuffer m_audioBuf;
	SampleRingBuffer m_videoBuf;
	uint32_t m_lastWrittenStream;
	uint32_t m_pad228;
	SceKernelLwMutexWork m_mutex;
	uint8_t m_mutexCreated;
	uint8_t m_pad265[3];
	uint32_t m_videoChunkScale;
	uint32_t m_audioChunkScale;
	uint32_t m_pad276;
	BufferedFileWriter m_fileWriter;
	SceUID m_fd;
	uint32_t m_avcProfileLevel;
	uint32_t m_reserved552;
	uint32_t m_reserved556;
};

/* Avrec coordinator (100 bytes) */
class Avrec {
public:
	Avrec();
	virtual ~Avrec();

	static int32_t QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req);
	int32_t Cleanup();
	int32_t ValidateConfig(const AvrecConfig *cfg);
	int32_t Init(const char *filePath, const AvrecConfig *cfg, MemAllocator *allocator);
	int32_t FlushVideoOnce();
	int32_t FlushAudioOnce();
	int32_t StopEncoders();
	int32_t FinalizeFile(const uint16_t *creationDate);
	int32_t AddVideoFrame(const void *yuv, uint32_t size, uint8_t *outDropped);
	int32_t AddAudioFrame(const void *pcm, uint32_t size);
	int32_t AddMetadata(const AvrecMetadataItem *item);
	int32_t PrepareThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count, int32_t flag);
	int32_t WriteThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count);
	static void OnLowFreeSpace(uint64_t freeSpace);

	AvrecConfig m_config;
	AvcEncoder *m_avcEnc;
	AacEncoder *m_aacEnc;
	Mp4Muxer *m_muxer;
	MemAllocator *m_allocator;
	uint32_t m_videoAuBufSize;
	void *m_videoAuBuf;
	uint32_t m_audioAuBufSize;
	void *m_audioAuBuf;
};

/* High-level Recorder (248 bytes) */
class Recorder {
public:
	explicit Recorder(uint32_t ctrlSize);
	virtual ~Recorder();

	int32_t CheckFreeSpace(uint32_t extraBytes);
	int32_t Prepare();
	int32_t BuildConfig(uint32_t modeIndex, uint32_t modeOption, AvrecConfig *outCfg);
	int32_t Start(const RecorderStartParam *param);
	int32_t Pause();
	int32_t Stop();
	int32_t Finalize(const RecorderTermInfo *info, AllocatorInterface *allocIf);
	int32_t Unprepare();
	int32_t WaitIdle();
	uint32_t GetDurationMs();
	int32_t TakeAsyncError();
	int32_t AddAudioSample(const void *pcm, uint32_t size);
	void AddAudioSampleNoReturn(const void *pcm, uint32_t size);
	int32_t AddVideoSample(const void *yuv, uint32_t size, uint8_t *outDropped);
	void AddVideoSampleNoReturn(const void *yuv, uint32_t size, uint8_t *outDropped);
	int32_t QueryMemSize(uint32_t modeIndex, uint32_t modeOption, AvrecMemReq *outReq);

	Avrec m_avrec;
	ExternalMemAllocator m_allocator;
	uint32_t m_pad124;
	Mp4RecMutex m_stateMutex;
	Mp4RecMutex m_infoMutex;
	uint64_t m_startTime;
	uint32_t m_recordedMs;
	int32_t m_asyncError;
	uint32_t m_reserved208;
	uint32_t m_reserved212;
	uint32_t m_videoDurationTicks;
	uint32_t m_audioDurationTicks;
	int32_t m_state;
	uint32_t m_ctrlSize;
	int32_t m_lastDevCheckErr;
	int32_t m_stateMutexErr;
	int32_t m_infoMutexErr;
	uint32_t m_pad244;
};

/* Top-level recorder handle stored in SceMp4RecCtrl::handle (616 bytes) */
typedef struct Mp4RecHandle {
	Recorder *recorder;
	MspaceAllocatorInterface *allocIf;
	RecorderStartParam startParam;
	int32_t state;
	char tmpFilePath[SCE_MP4REC_PATH_MAX];
	SceLibcMspace mspace;
	Mp4RecAvRecBufInfo *avRecBuf;
	uint32_t magic;
	SceRtcTick startUtcTick;
	Mp4RecMutex *apiMutex;
	char mountPoint[16];
	char titleId[20];
} Mp4RecHandle;

#endif /* __cplusplus */

#endif /* MP4REC_INTERNAL_H */
