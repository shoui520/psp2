#ifndef SCE_AVPLAYER_INTERNAL_H
#define SCE_AVPLAYER_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sceavplayer.h>
#include <videodec.h>
#include <avcdec.h>
#include <audiodec.h>
#include <kernel/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SceClibMspace;

SceClibMspace sceClibMspaceCreate(void *memblock, SceSize size);
void sceClibMspaceDestroy(SceClibMspace mspace);
void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
void sceClibMspaceFree(SceClibMspace mspace, void *ptr);

void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
void *sceClibMemset(void *dst, int ch, SceSize len);
int sceClibMemcmp(const void *lhs, const void *rhs, SceSize len);
int sceClibStrncasecmp(const char *lhs, const char *rhs, SceSize len);
char *sceClibStrrchr(const char *src, int ch);
int sceClibSnprintf(char *dst, SceSize maxLen, const char *fmt, ...);
int sceClibVsnprintf(char *dst, SceSize maxLen, const char *fmt, va_list args);
int sceClibPrintf(const char *fmt, ...);

#define SCE_O_RDONLY  0x0001
#define SCE_SEEK_SET  0
#define SCE_SEEK_END  2

SceUID sceIoOpen(const char *file, int flags, SceMode mode);
int sceIoClose(SceUID fd);
int sceIoPread(SceUID fd, void *data, SceSize size, SceOff offset);
SceOff sceIoLseek(SceUID fd, SceOff offset, int whence);

SceUInt64 sceKernelGetProcessTimeWide(void);

#define SCE_SYSMODULE_MP4 0x0026

typedef struct SceAvPlayerFrameInfoEx {
	uint8_t *pData;
	uint8_t reserved[4];
	uint64_t timeStamp;
	SceAvPlayerStreamDetails details;
	uint32_t unused;
	uint32_t cropLeftOffset;
	uint32_t cropRightOffset;
	uint32_t cropTopOffset;
	uint32_t cropBottomOffset;
} SceAvPlayerFrameInfoEx;

bool sceAvPlayerGetVideoDataEx(SceAvPlayerHandle h, SceAvPlayerFrameInfoEx *videoInfo);

/* Imported SceSysmodule functions */
int32_t sceSysmoduleLoadModule(uint16_t id);
int32_t sceSysmoduleUnloadModule(uint16_t id);
int32_t sceSysmoduleIsLoaded(uint16_t id);

/* Imported SceCodecEngineUser functions */
SceUID sceCodecEngineOpenUnmapMemBlock(void *memBlock, SceSize size);
int32_t sceCodecEngineCloseUnmapMemBlock(SceUID uid);
SceUIntVAddr sceCodecEngineAllocMemoryFromUnmapMemBlock(SceUID uid, SceSize size, SceSize align);
int32_t sceCodecEngineFreeMemoryFromUnmapMemBlock(SceUID uid, SceUIntVAddr addr);

/* Imported SceThreadmgr / SceThreadmgrCoredumpTime functions */
int32_t SceThreadmgrCoredumpTime_sceKernelExitThread(int32_t status);

/* Imported SceMp4 functions */
void *SceMp4_920623C8(void);
void SceMp4_94E1305D(void *demux);
int32_t SceMp4_30E49E4D(void *demux, void *fileOps, void *fileCtx);
int32_t SceMp4_C05DFF01(void *demux);
int32_t SceMp4_D2871F1C(void *demux);
int32_t SceMp4_A870587F(void *demux);
int32_t SceMp4_6B69F900(void *demux, uint32_t trackIndex, void *streamDesc);
int32_t SceMp4_20A01112(void *demux, uint32_t trackIndex, void *auOut);
int32_t SceMp4_40C64DC0(void *demux, uint32_t trackIndex, int32_t enable);
int32_t SceMp4_7B4832FE(void *demux, uint32_t trackIndex, uint64_t timeMs, uint32_t mode);
int32_t SceMp4_32A15788(void *demux, uint32_t trackIndex, uint32_t *outOffset);
int32_t SceMp4_05474AF0(void *demux, int32_t debugLevel);
int32_t SceMp4_609E57AD(void *demux, uint32_t trackIndex, void *extraOut);
int32_t SceMp4_D5B26179(void *demux, uint32_t trackIndex, void *codecOut);
int32_t SceMp4_A7C09746(void *demux, uint32_t trackIndex, uint64_t *outPts);
int32_t SceMp4_40351E1A(void *demux);
int32_t SceMp4_96676BA0(void *demux);
int32_t SceMp4_CB91CEBC(void *demux);
int32_t SceMp4_D092A066(void *demux);
int32_t SceMp4_EFE299C9(void *demux);

#ifdef __cplusplus
}
#endif

/* Internal core states (SCE_MVPCORE_STATE_*) */
enum AvCoreState {
	AV_STATE_ERROR        = -1,
	AV_STATE_UNINIT       = 0,
	AV_STATE_INIT         = 1,
	AV_STATE_OPENING      = 2,
	AV_STATE_READY        = 3,
	AV_STATE_PLAY         = 4,
	AV_STATE_STOP         = 5,
	AV_STATE_EOF          = 6,
	AV_STATE_PAUSE        = 7,
	AV_STATE_EOF_PAUSE    = 8,
	AV_STATE_JUMP         = 9,
	AV_STATE_TRICKMODE    = 10,
	AV_STATE_SEEK_DONE    = 11,
	AV_STATE_BUFFERING    = 12,
	AV_STATE_TRANSITION   = 13,
	AV_STATE_JUMP_PAUSE   = 16
};

/* Internal playback modes */
enum AvPlaybackMode {
	AV_PLAYMODE_NORMAL    = 1,
	AV_PLAYMODE_SLOW      = 2,
	AV_PLAYMODE_FF        = 3,
	AV_PLAYMODE_RW        = 4
};

/* Internal event types */
enum AvInternalEventType {
	AV_EVENT_RESET            = 4,
	AV_EVENT_JUMP             = 5,
	AV_EVENT_TRICKMODE        = 6,
	AV_EVENT_JUMP_DONE_WARN   = 7,
	AV_EVENT_GET_VIDEO_PTS    = 10,
	AV_EVENT_GET_AUDIO_PTS    = 11,
	AV_EVENT_DROP_VIDEO_FRAME = 14,
	AV_EVENT_CHECK_BUFFERING  = 20,
	AV_EVENT_CHANGE_STATE     = 21,
	AV_EVENT_SEND_WARNING     = 22,
	AV_EVENT_THUMBNAIL_DONE   = 30,
	AV_EVENT_CHECK_SOURCE     = 40,
	AV_EVENT_ERROR            = 255
};

/* Event sender / routing phase */
enum AvEventSender {
	AV_SENDER_DEMUX   = 1,
	AV_SENDER_DECODER = 2,
	AV_SENDER_MONITOR = 3
};

/* Base clock sync decision */
enum AvClockSyncResult {
	AV_CLOCK_SHOW_NOW     = 0,
	AV_CLOCK_WAIT         = 1,
	AV_CLOCK_DROP_LATE    = 2,
	AV_CLOCK_AUDIO_AHEAD  = 3,
	AV_CLOCK_NO_AUDIO     = 4
};

/* Atomic store helpers using SNC ARMv7 LDREX/STREX intrinsics */
static inline void avAtomicStore32(volatile uint32_t *target, uint32_t value)
{
	do {
		(void)__builtin_ldrex((volatile unsigned int *)target);
	} while (__builtin_strex(value, (volatile unsigned int *)target) != 0);
}

static inline void avAtomicStore8(volatile uint8_t *target, uint8_t value)
{
	do {
		(void)__builtin_ldrex((volatile unsigned char *)target);
	} while (__builtin_strex(value, (volatile unsigned char *)target) != 0);
}

static inline void avAtomicStore64(volatile uint64_t *target, uint64_t value)
{
	do {
		(void)__builtin_ldrexd((volatile unsigned long long *)target);
	} while (__builtin_strexd(value, (volatile unsigned long long *)target) != 0);
}

/* Global mspace allocator and logging (avplayer_api.cpp) */
extern SceClibMspace g_avCoreMspace;
extern uint8_t g_avMp4Preloaded;

void *avMspaceAlloc(uint32_t alignment, SceSize size);
void avMspaceFree(void *ptr);
void *operator new(size_t size);
void *operator new(size_t size, void *placement);
void operator delete(void *ptr) throw();

void avLogDebug(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...);
void avLogError(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...);
void avLogInfo(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...);
void avLogWarn(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...);

/* Thread trampoline */
struct AvThreadStartArg {
	void (*entry)(void *context);
	void *context;
};

int32_t avThreadTrampoline(SceSize argSize, void *argBlock);

/* Generic doubly-linked list matching Sony's 0Stl_Handler_Mutex container */
template <typename T>
struct AvListNode {
	AvListNode<T> *prev;
	T value;
	AvListNode<T> *next;
};

template <typename T>
struct AvList;

template <typename T>
struct AvListIterator {
	AvList<T> *owner;
	AvListNode<T> *node;
	uint32_t index;
};

template <typename T>
struct AvList {
	AvListNode<T> *head;
	AvListNode<T> *tail;
	uint32_t count;
	uint32_t byteSize;
	uint32_t ownIterators;
	AvListIterator<T> *beginIter;
	AvListIterator<T> *endIter;
	AvListNode<T> *sentinel;
	SceKernelLwMutexWork mutex;

	void init()
	{
		head = NULL;
		tail = NULL;
		count = 0;
		byteSize = 0;
		ownIterators = 0;
		beginIter = NULL;
		endIter = NULL;
		SceKernelLwMutexOptParam opt;
		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		sceKernelCreateLwMutex(&mutex, "0Stl_Handler_Mutex ", 0, 0, &opt);
		T emptyValue;
		sceClibMemset(&emptyValue, 0, sizeof(emptyValue));
		AvListNode<T> *node = (AvListNode<T> *)avMspaceAlloc(8, sizeof(AvListNode<T>));
		if (node != NULL) {
			node->prev = NULL;
			node->next = NULL;
			sceClibMemcpy_safe(&node->value, &emptyValue, sizeof(T));
		}
		sentinel = node;
	}

	void destroy()
	{
		clear();
		if (ownIterators == 0) {
			if (beginIter != NULL)
				avMspaceFree(beginIter);
			if (endIter != NULL)
				avMspaceFree(endIter);
			if (sentinel != NULL)
				avMspaceFree(sentinel);
		}
		sceKernelDeleteLwMutex(&mutex);
	}

	void pushBack(const T &item)
	{
		sceKernelLockLwMutex(&mutex, 1, NULL);
		AvListNode<T> *node = (AvListNode<T> *)avMspaceAlloc(8, sizeof(AvListNode<T>));
		if (node != NULL) {
			node->prev = NULL;
			node->next = NULL;
			sceClibMemcpy_safe(&node->value, &item, sizeof(T));
		}
		if (tail != NULL) {
			tail->next = node;
			node->prev = tail;
		} else {
			head = node;
		}
		tail = node;
		count++;
		byteSize += sizeof(AvListNode<T>);
		sceKernelUnlockLwMutex(&mutex, 1);
	}

	void popBack()
	{
		sceKernelLockLwMutex(&mutex, 1, NULL);
		if (count != 0 && tail != NULL) {
			AvListNode<T> *victim = tail;
			AvListNode<T> *prevNode = victim->prev;
			if (prevNode != NULL)
				prevNode->next = NULL;
			else
				head = NULL;
			avMspaceFree(victim);
			tail = prevNode;
			count--;
			byteSize -= sizeof(AvListNode<T>);
		}
		sceKernelUnlockLwMutex(&mutex, 1);
	}

	void eraseAt(uint32_t index)
	{
		sceKernelLockLwMutex(&mutex, 1, NULL);
		if (count != 0 && head != NULL && index < count) {
			AvListNode<T> *cur = head;
			for (uint32_t i = 0; i < index && cur != NULL; i++)
				cur = cur->next;
			if (cur != NULL) {
				AvListNode<T> *prevNode = cur->prev;
				AvListNode<T> *nextNode = cur->next;
				if (cur == head) {
					head = nextNode;
					if (nextNode != NULL)
						nextNode->prev = NULL;
					if (count == 2)
						tail = nextNode;
				} else if (prevNode != NULL) {
					prevNode->next = nextNode;
				}
				if (nextNode != NULL)
					nextNode->prev = prevNode;
				else
					tail = prevNode;
				avMspaceFree(cur);
				count--;
				byteSize -= sizeof(AvListNode<T>);
			}
		}
		sceKernelUnlockLwMutex(&mutex, 1);
	}

	void clear()
	{
		while (count != 0)
			popBack();
	}

	T &at(uint32_t index) const
	{
		if (index >= count || head == NULL)
			return sentinel->value;
		AvListNode<T> *cur = head;
		for (uint32_t i = 0; i < index && cur->next != NULL; i++)
			cur = cur->next;
		return cur->value;
	}
};

/* Core event structures */
struct AvEvent {
	int32_t type;
	uint32_t reserved;
	int64_t payload;
};

struct AvAckEvent {
	int32_t sender;
	uint32_t reserved;
	int32_t eventType;
	uint32_t reservedEvent;
	int64_t timeStamp;
	int32_t status;
	uint32_t reservedStatus;
};

/* Stream descriptor (136 bytes, matches SceMp4 stream info) */
struct AvStreamDescriptor {
	int32_t streamType;
	uint32_t trackId;
	uint32_t codecFourcc;
	uint32_t codecProfile;
	uint64_t duration;
	uint64_t startTime;
	uint32_t bitRate;
	uint32_t maxBitRate;
	uint32_t timeScale;
	uint32_t sampleCount;
	uint32_t maxAuSize;
	uint32_t avgAuSize;
	uint32_t flags;
	uint32_t encryptionType;
	uint8_t extraConfig[8];
	uint8_t languageCode[4];
	uint16_t channelCount;
	uint16_t bitsPerSample;
	uint32_t sampleRate;
	uint32_t samplesPerFrame;
	uint32_t audioObjectType;
	uint32_t audioReserved;
	uint32_t width;
	uint32_t height;
	uint32_t cropLeft;
	uint32_t cropTop;
	uint16_t frameRateNum;
	uint16_t frameRateDen;
	uint32_t videoReserved;
	int64_t aspectRatioScaled;
	uint8_t fontSize;
	uint8_t textFlags[7];
};

/* Access unit descriptor (56 bytes) */
struct AvAccessUnit {
	uint32_t trackIndex;
	uint32_t size;
	uint8_t isSyncSample;
	uint8_t reservedSync[7];
	uint64_t pts;
	uint64_t dts;
	uint32_t duration;
	uint32_t sampleFlags;
	uint8_t isEof;
	uint8_t isEmpty;
	uint8_t isLoopBoundary;
	uint8_t reservedFlags[5];
	uint8_t *buffer;
	uint32_t capacity;
};

/* Decoded output frame descriptor (56 bytes) */
struct AvDecodedFrame {
	uint8_t *buffer;
	uint32_t bufferIndex;
	uint64_t pts;
	uint32_t resetFlag;
	uint32_t width;
	uint32_t heightOrSize;
	uint32_t cropLeftOrReadOffset;
	uint32_t cropRightOffset;
	uint32_t cropTopOffset;
	uint32_t cropBottomOffset;
	uint32_t reservedTail[3];
};

/* Forward declarations */
class AvAuFifo;
class AvOutputFifo;
class AvBaseClock;
class AvStateMachine;
class AvSourceFilter;
class AvDecoder;
class AvCoreDecoder;
class AvCoreDemux;
class AvController;

/* Stream slot owned by AvController (32 bytes) */
struct AvStreamSlot {
	uint8_t enabled;
	uint8_t active;
	uint8_t reserved[2];
	AvStreamDescriptor *descriptor;
	AvCoreDecoder *coreDecoder;
	AvAuFifo *auFifo;
	void *auBufferMem;
	AvOutputFifo *outputFifo;
	AvDecodedFrame *currentFrame;
	void *outputBufferMem;
};

/* AvAuFifo (136 bytes) */
class AvAuFifo {
public:
	AvList<AvAccessUnit *> queue;
	SceUID condMutex;
	SceUID condVar;
	SceClibMspace mspace;
	uint8_t *poolBase;
	uint32_t poolSize;
	uint32_t freeBytes;
	uint32_t highWaterMark;
	uint32_t lowWaterMark;
	uint32_t maxAuSize;
	uint32_t waitingFlag;
	uint32_t abortFlag;
	uint32_t fifoFlags;
	uint8_t isFull;
	uint8_t reservedPad[3];
	int32_t debugLevel;

	int32_t init(uint32_t size, void *buffer, const char *name);
	int32_t allocateAu(AvAccessUnit *au, uint32_t size);
	int32_t pushAu(AvAccessUnit *au);
	AvAccessUnit *peekAu();
	int32_t popAndFreeAu(AvAccessUnit *au);
	int32_t flush(void (*freeFunc)(void *));
	int32_t getCount();
	int32_t destroy();
	int32_t isBufferFull();
	void signalAbort();
	void clearAbort();
};

/* AvBaseClock (68 bytes) */
class AvBaseClock {
public:
	uint64_t audioPts;
	uint64_t videoPts;
	uint64_t systemStartUs;
	uint64_t baseMediaTime;
	uint64_t lastMediaTime;
	uint64_t pauseOffset;
	int32_t playbackSpeed;
	uint32_t clockFlags;
	uint8_t hasAudioClock;
	uint8_t hasVideoClock;
	uint8_t isRunning;
	uint8_t isPaused;
	int32_t debugLevel;

	void initBaseClock();
	void updateVideoPts(uint64_t pts);
	void updateAudioPts(uint64_t pts);
	uint64_t getVideoPts();
	uint64_t getAudioPts();
	int32_t evaluateVideoSync(uint64_t framePts);
	void setPlaybackSpeed(int32_t speed);
	void resumeClock();
	void resetClock();
	uint64_t getCurrentMediaTime90k();
	int32_t getPlaybackSpeed();
	uint64_t getCurrentMediaTimeMs();
};

/* AvOutputFifo (184 bytes) */
class AvOutputFifo {
public:
	AvList<AvDecodedFrame *> readyQueue;
	AvList<AvDecodedFrame *> freeQueue;
	SceUID condMutex;
	SceUID condVar;
	uint8_t *poolBase;
	uint32_t poolSize;
	uint32_t slotSize;
	uint32_t slotCount;
	uint32_t maxHoldCount;
	uint32_t allocatedCount;
	uint32_t waitingFlag;
	uint32_t abortFlag;
	uint8_t isFull;
	uint8_t reservedPad[3];
	int32_t debugLevel;
	uint32_t reservedTail[3];

	int32_t init(uint32_t totalSize, uint32_t frameSize, uint32_t maxSlots, void *buffer, const char *name, int32_t dbgLevel);
	uint8_t *acquireBuffer(uint32_t *outIndex);
	int32_t pushFrame(AvDecodedFrame *frame);
	AvDecodedFrame *peekOrPopReady(uint32_t popFlag);
	AvDecodedFrame *popReadyNoWait();
	AvDecodedFrame *popReady();
	AvDecodedFrame *releaseOldest(uint32_t forceFlag);
	int32_t getReadyCount();
	int32_t destroy();
	void signalAbort();
	void clearAbort();
	int32_t isBufferFull();
};

/* AvSourceFilter base class (virtual interface) */
class AvSourceFilter {
public:
	virtual ~AvSourceFilter();
	virtual int32_t init(const char *path, SceAvPlayerFileReplacement *fileOps, int32_t debugLevel);
	virtual int32_t open();
	virtual int32_t close();
	virtual int32_t getStreamCount();
	virtual int32_t getStreamInfo(uint32_t trackIndex, AvStreamDescriptor *outDesc);
	virtual int32_t enableStream(uint32_t trackIndex, int32_t enable);
	virtual int32_t readAccessUnit(uint32_t trackIndex, AvAccessUnit *outAu);
	virtual int32_t seekToTime(uint32_t trackIndex, uint64_t timeMs, uint32_t mode);
	virtual int32_t getTrackOffset(uint32_t trackIndex, uint32_t *outOffset);
	virtual int32_t getCodecConfig(uint32_t trackIndex, void *outConfig);
	virtual int32_t getExtraData(uint32_t trackIndex, void *outExtra);
	virtual int64_t getNextSyncPts(uint32_t trackIndex);
	virtual int64_t getDuration();
	virtual void setDebugLevel(int32_t level);
	virtual void resetState();
	virtual void clearFlags();
	virtual int32_t getSourceType();
	virtual int32_t isSeekable();
	virtual int32_t getEncryptionInfo();
	virtual int32_t shutdown();
};

/* AvMp4SourceFilter : public AvSourceFilter */
class AvMp4SourceFilter : public AvSourceFilter {
public:
	void *mp4Demux;
	int32_t debugLevel;
	SceAvPlayerFileReplacement fileOps;
	SceUID defaultFileFd;
	uint64_t defaultFileSize;
	char sourcePath[64];

	AvMp4SourceFilter();
	virtual ~AvMp4SourceFilter();
	virtual int32_t init(const char *path, SceAvPlayerFileReplacement *fileOps, int32_t debugLevel);
	virtual int32_t open();
	virtual int32_t close();
	virtual int32_t getStreamCount();
	virtual int32_t getStreamInfo(uint32_t trackIndex, AvStreamDescriptor *outDesc);
	virtual int32_t enableStream(uint32_t trackIndex, int32_t enable);
	virtual int32_t readAccessUnit(uint32_t trackIndex, AvAccessUnit *outAu);
	virtual int32_t seekToTime(uint32_t trackIndex, uint64_t timeMs, uint32_t mode);
	virtual int32_t getTrackOffset(uint32_t trackIndex, uint32_t *outOffset);
	virtual int32_t getCodecConfig(uint32_t trackIndex, void *outConfig);
	virtual int32_t getExtraData(uint32_t trackIndex, void *outExtra);
	virtual int64_t getNextSyncPts(uint32_t trackIndex);
	virtual int64_t getDuration();
	virtual void setDebugLevel(int32_t level);
	virtual void resetState();
	virtual void clearFlags();
	virtual int32_t getSourceType();
	virtual int32_t isSeekable();
	virtual int32_t getEncryptionInfo();
	virtual int32_t shutdown();
};

/* AvDecoder base class (152 bytes) */
class AvDecoder {
public:
	int32_t streamType;
	uint32_t decoderState;
	uint32_t codecFlags;
	AvStreamDescriptor descriptor;

	AvDecoder();
	virtual ~AvDecoder();
	virtual int32_t init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t debugLevel);
	virtual int32_t decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput);
	virtual int32_t reset();
	virtual int32_t shutdown();
	virtual int32_t stop();
	virtual int32_t flush(AvDecodedFrame *outFrame);
	virtual int32_t getInfo(void *outInfo);
};

/* AvAacDecoder : public AvDecoder */
class AvAacDecoder : public AvDecoder {
public:
	SceAvPlayerMemAllocator memAlloc;
	int32_t debugLevel;
	SceAudiodecCtrl audioCtrl;
	SceAudiodecInfoAac aacInfo;
	void *contextMem;
	SceUID unmapUid;
	SceUIntVAddr vaContext;
	uint32_t contextSize;
	uint8_t isInitialized;
	uint8_t reservedPad[3];

	AvAacDecoder();
	virtual ~AvAacDecoder();
	virtual int32_t init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t debugLevel);
	virtual int32_t decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput);
	virtual int32_t reset();
	virtual int32_t shutdown();
	virtual int32_t stop();
};

/* AvAvcDecoder : public AvDecoder */
class AvAvcDecoder : public AvDecoder {
public:
	SceAvPlayerMemAllocator memAlloc;
	int32_t debugLevel;
	SceVideodecCtrl videodecCtrl;
	SceAvcdecCtrl avcdecCtrl;
	void *libMem;
	void *frameMem;
	SceUID unmapUid;
	SceUIntVAddr vaContext;
	uint32_t contextSize;
	uint32_t frameWidth;
	uint32_t frameHeight;
	uint32_t alignedWidth;
	uint32_t alignedHeight;
	uint8_t isInitialized;
	uint8_t libInitialized;
	uint8_t reservedPad[2];

	AvAvcDecoder();
	virtual ~AvAvcDecoder();
	virtual int32_t init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t debugLevel);
	virtual int32_t decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput);
	virtual int32_t reset();
	virtual int32_t shutdown();
	virtual int32_t stop();
	virtual int32_t flush(AvDecodedFrame *outFrame);
};

/* AvCoreDecoder (128 bytes) */
class AvCoreDecoder {
public:
	volatile uint32_t stopFlag;
	volatile uint32_t eventPendingFlag;
	uint32_t decodeMode;
	uint32_t reservedAlign;
	volatile uint64_t discardUntilPts;
	AvAuFifo *inputFifo;
	AvOutputFifo *outputFifo;
	SceUID threadId;
	int32_t playbackMode;
	SceUID lockMutex;
	AvStateMachine *stateMachine;
	AvDecoder *decoder;
	int32_t debugLevel;
	AvList<AvEvent> eventQueue;
	uint32_t highFrameRateFlag;

	void initCore();
	void setInputAndDecoder(AvAuFifo *fifo, AvDecoder *dec);
	int32_t initDecoder(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t dbgLevel);
	int32_t startVideo(int32_t priority, const char *threadName, uint8_t highFps);
	int32_t startAudio(int32_t priority, const char *threadName);
	int32_t addNewEvent(int32_t eventType, int64_t payload);
	int32_t processEvent();
	void decodeThread();
	int32_t reset();
	int32_t stop();
	int32_t shutdown();
	int32_t jump(int64_t targetPts);
	int32_t setPlaybackMode(int32_t mode);
	int32_t getDiscardInformation(int64_t pts);
	int32_t setDiscardTarget(uint64_t targetPts);
	int32_t flushTillNextSyncSample();
};

/* AvCoreDemux (328 bytes) */
class AvCoreDemux {
public:
	volatile uint32_t stopFlag;
	volatile uint32_t eventPendingFlag;
	SceUID threadId;
	AvSourceFilter *sourceFilter;
	int32_t debugLevel;
	int32_t basePriority;
	AvStateMachine *stateMachine;
	AvController *controller;
	AvList<AvEvent> eventQueue;
	AvList<AvAuFifo *> auFifoList;
	int32_t playbackMode;
	int32_t sourceStatus;
	int32_t eofReached;
	int32_t loopEnabled;
	AvList<int32_t> trackEnableList;
	AvList<AvStreamDescriptor *> streamDescList;

	int32_t init(AvStateMachine *sm, AvController *ctrl, int32_t priority, int32_t dbgLevel);
	int32_t checkSource(const char *path, SceAvPlayerFileReplacement *fileOps, int32_t isRetry);
	int32_t start();
	int32_t addNewEvent(int32_t eventType, int64_t payload);
	int32_t processEvent();
	void demuxThread();
	int32_t jump(int64_t targetPts);
	int32_t stop();
	int32_t shutdown();
	int32_t setStreamFifo(uint32_t trackIndex, AvAuFifo *fifo);
	int32_t disableStream(uint32_t trackIndex);
	int32_t setPlaybackMode(int32_t mode);
	int32_t getTrackOffset(uint32_t trackIndex);
	int32_t getSourceType();
	int32_t discardAu(uint32_t trackIndex);
	int32_t flushAllAUs();
};

/* State machine callback type */
typedef int32_t (*AvFlowMgrCallback)(AvController *controller, AvEvent *event, int32_t sender, void *outData);

/* AvStateMachine : public AvBaseClock (312 bytes) */
class AvStateMachine : public AvBaseClock {
public:
	uint32_t reservedAlign;
	AvFlowMgrCallback flowCallback;
	volatile uint32_t stopFlag;
	volatile uint8_t currentState;
	volatile uint8_t previousState;
	uint8_t reservedStatePad[2];
	int32_t debugLevel;
	SceAvPlayerMemAllocator *memAlloc;
	int32_t basePriority;
	AvController *controller;
	volatile uint32_t pendingAckCount;
	SceKernelLwMutexWork stateMutex;
	SceKernelLwMutexWork eventMutex;
	SceUID threadId;
	uint32_t reservedThreadPad;
	AvList<AvEvent> userEventQueue;
	AvList<AvAckEvent> ackEventQueue;
	void *playerInstance;
	int32_t syncCheckFlag;

	int32_t init(void *player, SceAvPlayerMemAllocator *alloc, int32_t priority, int32_t dbgLevel, AvController *ctrl, AvFlowMgrCallback cb);
	int32_t start();
	int32_t stop();
	int32_t shutdown();
	int32_t isStateTransitionValid(int8_t nextState);
	int32_t setState(int8_t nextState);
	volatile uint8_t *getStatePtr();
	int8_t getPreviousState();
	int32_t postUserEvent(int32_t eventType, int64_t payload);
	void postAckEvent(int32_t sender, int32_t eventType, int64_t timeStamp, int32_t status);
	int32_t mapStateCode(int32_t stateCode);
	int32_t processUserEvent();
	int32_t processAckEvent();
	int32_t monitorBuffering();
	int32_t monitorAvSync();
	void controllerThread();
};

/* AvController (424 bytes) */
class AvController {
public:
	AvList<AvStreamSlot *> streamList;
	AvCoreDemux *demux;
	uint32_t reservedDemuxPad;
	AvList<AvStreamDescriptor *> streamDescList;
	uint8_t loopFlagPending;
	uint8_t clockSynced;
	uint8_t reservedSyncPad[2];
	int32_t loopingEnabled;
	volatile uint32_t videoLoopPending;
	volatile uint32_t audioLoopPending;
	volatile uint32_t loopBothReady;
	int32_t activeAudioStream;
	SceUID streamMutex;
	int32_t playbackMode;
	volatile uint32_t trickSpeed;
	uint8_t jumpCompletePending;
	uint8_t reservedJumpPad[3];
	AvStateMachine *stateMachine;
	SceAvPlayerMemAllocator memAlloc;
	SceAvPlayerEventReplacement eventCb;
	SceAvPlayerFileReplacement fileOps;
	AvDecoder *audioDecoder;
	AvDecoder *videoDecoder;
	char sourcePath[64];
	int32_t sourceId;
	int32_t basePriority;
	int32_t numOutputVideoBuffers;
	uint32_t reservedBufPad;
	AvList<AvDecoder *> decoderList;
	int32_t debugLevel;
	uint8_t highFrameRateFlag;
	uint8_t reservedFpsPad[3];
	uint32_t timedTextAuBufSize;
	uint32_t audioAuBufSize;
	uint32_t videoAuBufSize;

	int32_t init(SceAvPlayerInitData *initData, void *playerInstance);
	int32_t setDemuxVideoBufferSize(uint32_t size);
	int32_t addSource(const char *path);
	int32_t getStreamCount();
	int32_t getStreamInfo(uint32_t streamIndex, SceAvPlayerStreamInfo *outInfo);
	int32_t registerVideoDecoder(AvDecoder *dec);
	int32_t registerAudioDecoder(AvDecoder *dec);
	int32_t hasDecoderForType(int32_t streamType);
	AvDecoder *findDecoderForType(int32_t streamType);
	void *avMemAlign(uint32_t alignment, SceSize size);
	void *avGraphicsMemAlign(uint32_t alignment, SceSize size);
	void avMemFree(void *ptr);
	void avGraphicsFree(void *ptr);
	int32_t enableFrameOutput(AvStreamSlot *slot);
	int32_t enableAuInput(AvStreamSlot *slot, uint32_t trackOffset);
	int32_t enableDecoder(AvStreamSlot *slot, uint32_t trackOffset);
	int32_t enableStream(uint32_t streamIndex);
	int32_t disableStream(uint32_t streamIndex);
	int32_t isStreamEnabled(uint32_t streamIndex);
	int32_t changeFlowState(int8_t nextState);
	int32_t stop();
	int32_t start();
	int32_t jumpInternal(uint64_t targetPts90k);
	int32_t pause();
	int32_t resume();
	int32_t sendTimedTextEvent(uint64_t videoTimeMs);
	int32_t fetchFrame(int32_t streamType, uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame);
	int32_t syncLoopAudio(uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame);
	void drainInactiveAudio(uint32_t *activeAudioIdx, uint32_t *enabledAudioIdx);
	int32_t getAudioData(SceAvPlayerFrameInfo *outFrame);
	int32_t syncLoopVideo(uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame);
	int32_t getVideoData(SceAvPlayerFrameInfo *outFrame);
	int32_t getVideoDataEx(SceAvPlayerFrameInfoEx *outFrame);
	int32_t isActive();
	int32_t isEof();
	int32_t finalize();
	int32_t setLooping(uint8_t loopFlag);
	uint64_t getCurrentTime();
	int32_t classifyTrickSpeed(int32_t speed, int32_t *outMode, int32_t *outClampedSpeed, uint8_t *outNeedJump);
	int32_t setTrickSpeed(int32_t speed);
	int32_t jumpToTime(int64_t timeMs);
	int32_t jumpTimeOffset(int64_t offsetMs);
	int32_t handleFlowEvent(AvEvent *event, int32_t sender, void *outData);
};

/* Top-level player handle (152 bytes) */
struct SceAvPlayerInstance {
	AvController *controller;
	SceAvPlayerEventReplacement userEventCb;
	SceAvPlayerMemAllocator userMemAlloc;
	SceAvPlayerFileReplacement userFileOps;
	int32_t debugLevel;
	uint32_t basePriority;
	int32_t numOutputVideoFrameBuffers;
	bool autoStart;
	uint8_t reservedAuto[3];
	const char *defaultLanguage;
	void *coreHeapMem;
	SceUID apiMutex;
	int32_t sourceCount;
	int32_t activeFlag;
};

#endif /* SCE_AVPLAYER_INTERNAL_H */
