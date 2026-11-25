/*
 * Internal definitions for SceVoice (libvoice).
 */

#ifndef VOICE_INTERNAL_H
#define VOICE_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <scetypes.h>
#include <voice.h>
#include <audioin.h>
#include <audioout.h>
#include <audiodec.h>
#include <audioenc.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <kernel/processmgr.h>

#define VOICE_MAX_PORTS              64
#define VOICE_MAX_EDGES              128
#define VOICE_MAX_DECODERS           7
#define VOICE_MAX_ENCODERS           1
#define VOICE_MAX_MIX_SLOTS          22
#define VOICE_UNBOUND_SLOT           0xBAADu
#define VOICE_NO_OWNER_PORT          0xF00Du
#define VOICE_INVALID_SLOT           0xBEEFu
#define VOICE_RING_POISON_ADDR       0xBAADF00Du
#define VOICE_EVENT_QUIT             0x80000000u

#define VOICE_AUDIODEC_TYPE_CELP     0x1006u
#define VOICE_AUDIOENC_TYPE_CELP     0x2006u
#define VOICE_BITRATE_12200          12200

#define VOICE_CMD_INIT               0
#define VOICE_CMD_END                1
#define VOICE_CMD_START              2
#define VOICE_CMD_STOP               3
#define VOICE_CMD_ATTACH             4
#define VOICE_CMD_DETACH             5
#define VOICE_CMD_CREATE_PORT        8
#define VOICE_CMD_UPDATE_PORT        9
#define VOICE_CMD_CONNECT_PORT       10
#define VOICE_CMD_DISCONNECT_PORT    11
#define VOICE_CMD_DELETE_PORT        12
#define VOICE_CMD_SET_MUTE_ALL       15
#define VOICE_CMD_SET_MUTE           16
#define VOICE_CMD_GET_MUTE           17
#define VOICE_CMD_SET_VOLUME         18
#define VOICE_CMD_GET_VOLUME         19
#define VOICE_CMD_SET_BITRATE        20
#define VOICE_CMD_GET_BITRATE        21
#define VOICE_CMD_SET_PORT_ATTR      22
#define VOICE_CMD_GET_PORT_ATTR      23
#define VOICE_CMD_RESET_PORT         24
#define VOICE_CMD_PAUSE_PORT         25
#define VOICE_CMD_RESUME_PORT        26
#define VOICE_CMD_PAUSE_PORT_ALL     27
#define VOICE_CMD_RESUME_PORT_ALL    28
#define VOICE_CMD_GET_RESOURCE_INFO  29
#define VOICE_CMD_CHECK_TOPOLOGY     30

#define VOICE_ERROR_INVALID_CMD      ((int)0x80010016u)
#define VOICE_FACILITY_MASK          0x804E0000u

/* Generic intrusive doubly-linked list used throughout SysVoice */
typedef struct VoiceListNode VoiceListNode;
typedef struct VoiceList VoiceList;

struct VoiceListNode {
	VoiceListNode *prev;
	VoiceListNode *next;
	VoiceList *owner;
};

struct VoiceList {
	VoiceListNode *head;
	VoiceListNode *tail;
	uint32_t count;
};

/* 16-byte ring buffer control block */
typedef struct VoiceRingBuffer {
	uint16_t writeIdx;
	uint16_t writeCount;
	uint16_t readIdx;
	uint16_t readCount;
	uint8_t *buffer;
	uint16_t capacity;
	uint16_t reserved;
} VoiceRingBuffer;

/* Shared userspace port descriptor (128 bytes per port) */
typedef struct VoiceSharedPort {
	VoiceRingBuffer dataRing;
	VoiceRingBuffer indexRing;
	uint16_t portId;
	uint16_t frameSize;
	SceVoicePortType portType;
	SceVoicePortState portState;
	int32_t pcmDataType;
	uint32_t partialBytes;
	uint8_t partialBuf[76];
} VoiceSharedPort;

typedef struct VoiceSharedPortTable {
	uint32_t maxPorts;
	uint32_t reserved[3];
	VoiceSharedPort ports[VOICE_MAX_PORTS];
} VoiceSharedPortTable;

/* LibVoiceInstance (userspace client state, 96 bytes) */
typedef struct LibVoiceInstance {
	uint32_t instanceId;
	int32_t dataLockCount;
	SceKernelLwMutexWork dataMutex;
	const char *dataLockOwner;
	int32_t userLockCount;
	SceKernelLwMutexWork userMutex;
	const char *userLockOwner;
	VoiceSharedPortTable *sharedPortTable;
	uint8_t attached;
	uint8_t reserved[7];
} LibVoiceInstance;

/* Aligned 96-byte command packet passed between LibVoice and SysVoice */
typedef struct VoiceCommand {
	uint32_t commandId;
	uint32_t instanceId;
	uint32_t reserved[6];
	union {
		struct {
			SceVoiceInitParam initParam;
			SceVoiceVersion version;
			LibVoiceInstance *libInstance;
		} init;
		struct {
			SceVoiceStartParam startParam;
			uint32_t flags;
		} start;
		struct {
			void *privateBase;
			void *sharedBase;
			SceUID containerUid;
		} attach;
		struct {
			uint32_t portId;
			uint32_t reservedPort[7];
			SceVoicePortParam param;
		} portOp;
		struct {
			uint32_t iPortId;
			uint32_t oPortId;
		} connect;
		struct {
			uint8_t muted;
		} muteAll;
		struct {
			uint32_t portId;
			uint8_t muted;
		} mute;
		struct {
			uint32_t portId;
			float volume;
		} volume;
		struct {
			uint32_t portId;
			uint32_t bitrate;
		} bitrate;
		struct {
			uint32_t portId;
			SceVoicePortAttr attr;
			int32_t setValue;
			int32_t getValue;
		} portAttr;
		struct {
			SceVoiceResourceInfo *info;
		} resourceInfo;
		uint32_t rawWords[16];
	} payload;
} VoiceCommand __attribute__((aligned(32)));

/* Port timing / sizing metrics returned by format validator */
typedef struct VoicePortFormatMetrics {
	uint16_t frameDurationMs;
	uint16_t bufferBytes;
	uint16_t frameSize;
	uint16_t pcmFrameSize;
} VoicePortFormatMetrics;

/* Edge linking an input port to an output port (16 bytes) */
typedef struct VoiceEdge {
	VoiceListNode node;
	uint16_t iPortId;
	uint16_t oPortId;
} VoiceEdge;

/* Internal port object (352 bytes) */
typedef struct VoicePort {
	VoiceListNode node;
	uint32_t reservedAlign;
	VoiceRingBuffer sharedRing;
	VoiceRingBuffer internalRing;
	VoiceList edgeList;
	uint32_t reservedList;
	VoiceRingBuffer indexRing;
	void *mixInputBuf;
	uint32_t sampleRate;
	float resampleHistory[40];
	uint16_t portId;
	uint16_t reservedId;
	uint32_t reservedParam;
	SceVoicePortParam param;
	uint32_t reservedTiming;
	float idleDuration;
	float activeDuration;
	uint32_t reservedStats;
	uint32_t deviceMagic;
	uint8_t vadSpeechActive;
	uint8_t audioInActive;
	uint16_t reservedFlags;
	SceVoicePortState state;
	uint16_t frameDurationMs;
	uint16_t bufferBytes;
	uint16_t frameSize;
	uint16_t pcmFrameSize;
	uint16_t codecSlotId;
	uint16_t sharedOwnerPortId;
	uint16_t edgeCount;
	uint8_t paused;
	uint8_t reservedTail[29];
} VoicePort;

/* Resource limits per SysVoiceInstance (28 bytes) */
typedef struct VoiceResourceLimits {
	uint16_t maxTotalPort;
	uint16_t maxEncoders;
	uint16_t maxDecoders;
	uint16_t maxDevicePort;
	uint16_t maxPcmPort;
	uint16_t maxEdges;
	uint16_t maxInVoicePort;
	uint16_t maxOutVoicePort;
	uint32_t maxPcmMemBytes;
	uint32_t privateMemSize;
	uint32_t totalMemSize;
} VoiceResourceLimits;

/* SysVoiceInstance (per-client service context, 108 bytes) */
typedef struct SysVoiceInstance {
	LibVoiceInstance *libInstance;
	SceVoiceVersion version;
	uint16_t portCount;
	uint16_t edgeCount;
	uint16_t inDeviceCount;
	uint16_t outDeviceCount;
	int32_t appType;
	SceUID containerUid;
	uint32_t pcmMemUsed;
	uint32_t pcmPortCount;
	uint32_t inVoiceCount;
	uint32_t outVoiceCount;
	void *sharedMemBase;
	void *privateMemBase;
	void *sharedMemStart;
	uint8_t muteAll;
	uint8_t attached;
	uint8_t pausedAll;
	uint8_t reservedState;
	VoiceResourceLimits limits;
	VoiceList inputPortList;
	VoiceList outputPortList;
} SysVoiceInstance;

/* Memory pool block descriptor (28 bytes) */
typedef struct VoiceMemBlock {
	VoiceListNode node;
	uint32_t size;
	uint32_t inUse;
	void *address;
	uint32_t reservedTag;
} VoiceMemBlock;

/* Memory pool allocator (68 bytes) */
typedef struct VoiceMemPool {
	uint8_t isPrivate;
	uint8_t initialized;
	uint16_t reservedFlags;
	uint32_t reservedMeta[2];
	uint32_t allocCount;
	uint32_t freeCount;
	uint32_t usedBytes;
	uint32_t freeBytes;
	VoiceList availBlocks;
	VoiceList usedBlocks;
	VoiceList freeBlocks;
	const char *name;
} VoiceMemPool;

/* CELP codec info structs matching SceAudioenc / SceAudiodec */
typedef struct VoiceCelpEncInfo {
	uint32_t size;
	uint32_t excitationMode;
	uint32_t samplingRate;
	uint32_t bitRate;
} VoiceCelpEncInfo;

typedef struct VoiceCelpDecInfo {
	uint32_t size;
	uint32_t excitationMode;
	uint32_t samplingRate;
	uint32_t bitRate;
	uint32_t lostFlag;
	uint32_t reserved[2];
} VoiceCelpDecInfo;

/* Encoder slot (128 bytes) */
typedef struct VoiceEncoderSlot {
	VoiceListNode node;
	uint16_t slotIndex;
	uint16_t portId;
	void *pcmBuf;
	VoiceCelpEncInfo celpInfo;
	uint8_t reservedPad[44];
	SceAudioencCtrl ctrl;
	uint32_t reservedTail;
} VoiceEncoderSlot;

/* Decoder slot (224 bytes) */
typedef struct VoiceDecoderSlot {
	VoiceListNode node;
	uint16_t slotIndex;
	uint16_t portId;
	void *pcmBuf;
	VoiceCelpDecInfo celpInfo;
	SceAudiodecCtrl ctrl;
	uint8_t lastFrame[136];
} VoiceDecoderSlot;

/* Decoder pool (5376 bytes) */
typedef struct VoiceDecoderPool {
	uint8_t inputScratch[1024];
	uint8_t outputScratch[2560];
	VoiceList activeList;
	VoiceList freeList;
	uint32_t reservedLists[2];
	VoiceDecoderSlot slots[VOICE_MAX_DECODERS];
	uint8_t pcmPassthrough[192];
} VoiceDecoderPool;

/* Encoder pool (3840 bytes) */
typedef struct VoiceEncoderPool {
	uint8_t outputScratch[1024];
	uint8_t inputScratch[2560];
	VoiceList activeList;
	VoiceList freeList;
	uint32_t reservedLists[2];
	VoiceEncoderSlot slots[VOICE_MAX_ENCODERS];
	uint8_t pcmPassthrough[96];
} VoiceEncoderPool;

/* Mixer state */
typedef struct VoiceMixer {
	void *inputCacheBuf;
	void *workBuf;
	uint16_t cachedPortIds[VOICE_MAX_MIX_SLOTS];
	uint16_t cachedCount;
} VoiceMixer;

/* VAD hangover state machine */
typedef struct VoiceVadHangoverConfig {
	float releaseTime;
	float attackTime;
	float frameStep;
} VoiceVadHangoverConfig;

typedef struct VoiceVadHangoverState {
	float elapsed;
	uint8_t active;
	uint8_t reserved[3];
} VoiceVadHangoverState;

/* Spectral noise gate / VAD filter (2104 bytes) */
typedef struct VoiceVadFilter {
	float alphaSignal;
	float alphaNoise;
	float snrThresholdDb;
	float levelThresholdDb;
	VoiceVadHangoverConfig hangoverCfg;
	uint32_t reservedCfg;
	float workMemory[512];
	float *workPtr;
	VoiceVadHangoverState hangoverState;
} VoiceVadFilter;

/* Common audio device control block */
typedef int (*VoiceAudioIoFunc)(int port, void *buf);

typedef struct VoiceDeviceIoBlock {
	SceUID threadId;
	int audioPort;
	SceUID eventUid;
	uint8_t isOutput;
	uint8_t reservedIo[3];
	void *pendingBuf;
	VoiceAudioIoFunc ioFunc;
	void *workBuf;
} VoiceDeviceIoBlock;

/* AudioIn device slot (2160 bytes) */
typedef struct VoiceAudioInDevice {
	VoiceListNode node;
	uint16_t slotIndex;
	uint16_t portId;
	int32_t grain;
	int32_t freq;
	uint32_t pingPong;
	VoiceDeviceIoBlock io;
	VoiceVadFilter vad;
} VoiceAudioInDevice;

/* AudioOut device slot (64 bytes) */
typedef struct VoiceAudioOutDevice {
	VoiceListNode node;
	uint16_t slotIndex;
	uint16_t portId;
	int32_t grain;
	int32_t freq;
	uint32_t pingPong;
	VoiceDeviceIoBlock io;
	void *stereoBuf;
	uint32_t reservedTail;
} VoiceAudioOutDevice;

/* AudioIn device pool */
typedef struct VoiceAudioInPool {
	VoiceList activeList;
	VoiceList freeList;
	uint32_t reservedHeader[2];
	VoiceAudioInDevice devices[1];
} VoiceAudioInPool;

/* AudioOut device pool */
typedef struct VoiceAudioOutPool {
	VoiceList activeList;
	VoiceList freeList;
	uint32_t reservedHeader[2];
	VoiceAudioOutDevice devices[1];
} VoiceAudioOutPool;

/* Event dispatcher */
typedef struct VoiceEventDispatcher {
	SceUID msgPipeUid;
	void *userData;
	SceVoiceEventCallback callback;
	SceUID threadId;
	SceUID sendPipeUid;
	uint8_t running;
	uint8_t reserved[3];
} VoiceEventDispatcher;

/* Global SysVoice singleton */
typedef struct SysVoice {
	uint32_t instanceCount;
	SysVoiceInstance instance;
	uint8_t padAfterInstance[16];
	VoicePort ports[VOICE_MAX_PORTS];
	VoiceList freePortList;
	uint32_t reservedPortList;
	VoiceEdge edges[VOICE_MAX_EDGES];
	SysVoiceInstance *activeInstance;
	uint32_t reservedActive;
	SceKernelLwMutexWork globalMutex;
	int32_t globalLockCount;
	const char *globalLockOwner;
	VoiceMemPool sharedMemPool;
	VoiceMemPool privateMemPool;
	SceUID internalMemBlockUid;
	int32_t threadPriority;
	int32_t threadAffinityMask;
	SceUID updateThreadId;
	SceUID updateEventFlag;
	uint8_t updateRunning;
	uint8_t reservedUpdate;
	uint16_t updateTickCount;
	VoiceAudioInPool audioInPool;
	uint8_t padBeforeDecoders[28];
	VoiceDecoderPool decoderPool;
	VoiceMixer mixer;
	uint8_t padBeforeEncoders[200];
	VoiceEncoderPool encoderPool;
	VoiceAudioOutPool audioOutPool;
	VoiceList freeEdgeList;
	VoiceEventDispatcher eventDispatcher;
	uint8_t padTail[124];
} SysVoice;

_Static_assert(offsetof(SysVoice, ports) == 128, "SysVoice.ports offset");
_Static_assert(offsetof(SysVoice, freePortList) == 22656, "SysVoice.freePortList offset");
_Static_assert(offsetof(SysVoice, edges) == 22672, "SysVoice.edges offset");
_Static_assert(offsetof(SysVoice, activeInstance) == 24720, "SysVoice.activeInstance offset");
_Static_assert(offsetof(SysVoice, globalMutex) == 24728, "SysVoice.globalMutex offset");
_Static_assert(offsetof(SysVoice, sharedMemPool) == 24768, "SysVoice.sharedMemPool offset");
_Static_assert(offsetof(SysVoice, privateMemPool) == 24836, "SysVoice.privateMemPool offset");
_Static_assert(offsetof(SysVoice, updateRunning) == 24924, "SysVoice.updateRunning offset");
_Static_assert(offsetof(SysVoice, audioInPool) == 24928, "SysVoice.audioInPool offset");
_Static_assert(offsetof(SysVoice, decoderPool) == 27136, "SysVoice.decoderPool offset");
_Static_assert(offsetof(SysVoice, mixer) == 32512, "SysVoice.mixer offset");
_Static_assert(offsetof(SysVoice, encoderPool) == 32768, "SysVoice.encoderPool offset");
_Static_assert(offsetof(SysVoice, audioOutPool) == 36608, "SysVoice.audioOutPool offset");
_Static_assert(offsetof(SysVoice, freeEdgeList) == 36704, "SysVoice.freeEdgeList offset");
_Static_assert(offsetof(SysVoice, eventDispatcher) == 36716, "SysVoice.eventDispatcher offset");
_Static_assert(sizeof(SysVoice) == 36864, "SysVoice size");

extern LibVoiceInstance *gLibVoiceInstance;
extern LibVoiceInstance gLibVoiceInstanceStorage;
extern uint32_t gVoiceStartReserved;
extern SysVoice gSysVoice;
extern int16_t gVadSampleScratch[256];

/* Intrusive doubly-linked list helpers (inlined to avoid extra symbols) */
static inline __attribute__((always_inline)) void voiceListInit(VoiceList *list)
{
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

static inline __attribute__((always_inline)) void voiceListPushFront(VoiceList *list, VoiceListNode *node)
{
	VoiceListNode *head = list->head;
	if (head != NULL) {
		head->prev = node;
	}
	node->next = head;
	node->prev = NULL;
	list->head = node;
	if (list->tail == NULL) {
		list->tail = node;
	}
	node->owner = list;
	list->count++;
}

static inline __attribute__((always_inline)) void voiceListPushBack(VoiceList *list, VoiceListNode *node)
{
	VoiceListNode *tail = list->tail;
	if (tail != NULL) {
		tail->next = node;
	}
	node->prev = tail;
	node->next = NULL;
	list->tail = node;
	if (list->head == NULL) {
		list->head = node;
	}
	node->owner = list;
	list->count++;
}

static inline __attribute__((always_inline)) VoiceListNode *voiceListPopFront(VoiceList *list)
{
	VoiceListNode *node = list->head;
	if (node != NULL) {
		VoiceListNode *next = node->next;
		list->head = next;
		if (next != NULL) {
			next->prev = NULL;
		}
		if (node == list->tail) {
			list->tail = NULL;
		}
		list->count--;
		node->prev = NULL;
		node->next = NULL;
		node->owner = NULL;
	}
	return node;
}

static inline __attribute__((always_inline)) void voiceListRemove(VoiceList *list, VoiceListNode *node)
{
	VoiceListNode *prev = node->prev;
	VoiceListNode *next = node->next;
	if (prev != NULL) {
		prev->next = next;
	} else {
		list->head = next;
	}
	if (next != NULL) {
		next->prev = prev;
	} else {
		list->tail = prev;
	}
	node->prev = NULL;
	node->next = NULL;
	node->owner = NULL;
	list->count--;
}

/* Kernel syscall declaration not present in user headers */
int sceKernelIsGameBudget(void);

/* libvoice.c */
int voiceSendCommand(VoiceCommand *cmd);
void voiceSharedPortInit(VoiceSharedPort *sharedPort, const VoicePort *port, intptr_t addrDelta);
void voiceSharedPortTerm(VoiceSharedPort *sharedPort);
void voiceSharedPortReset(VoiceSharedPort *sharedPort);
void voiceSharedPortWriteIndexedFrame(VoiceSharedPort *sharedPort, const void *src, uint32_t size,
                                      uint16_t *frameOffset, uint16_t indexOffset);
int voiceSharedPortWrite(VoiceSharedPort *sharedPort, const void *data, uint32_t *size, int16_t frameGaps);
int voiceSharedPortRead(VoiceSharedPort *sharedPort, void *data, uint32_t *size);
int libVoiceInitUserLock(LibVoiceInstance *inst);
int libVoiceTermUserLock(LibVoiceInstance *inst);
__attribute__((noinline)) int libVoiceLockUser(LibVoiceInstance *inst, const char *owner);
__attribute__((noinline)) int libVoiceUnlockUser(LibVoiceInstance *inst);
int libVoiceInitDataLock(LibVoiceInstance *inst, uint32_t instanceId);
int libVoiceTermDataLock(LibVoiceInstance *inst);
__attribute__((noinline)) int libVoiceLockData(LibVoiceInstance *inst, const char *owner);
__attribute__((noinline)) int libVoiceUnlockData(LibVoiceInstance *inst);
int libVoiceGetPortInfo(LibVoiceInstance *inst, uint32_t portId, SceVoiceBasePortInfo *info);
int libVoiceWriteToPort(LibVoiceInstance *inst, uint32_t portId, const void *data, uint32_t *size, int16_t frameGaps);
int libVoiceReadFromPort(LibVoiceInstance *inst, uint32_t portId, void *data, uint32_t *size);

/* voice_sysvoice.c */
int sysVoiceDispatchCommand(SysVoice *sys, VoiceCommand *cmd);
SysVoiceInstance *sysVoiceGetInstance(SysVoice *sys, uint32_t instanceId);
VoicePort *sysVoiceGetPort(SysVoice *sys, uint32_t portId);
int sysVoiceGetBitRateMetrics(SysVoice *sys, uint32_t bitrate, uint16_t *frameSize,
                              uint16_t *pcmFrameSize, uint16_t *frameDurationMs);
int sysVoiceValidatePortParam(SysVoice *sys, const SceVoicePortParam *param, VoicePortFormatMetrics *metrics);
int sysVoiceInitGlobalLock(SysVoice *sys);
int sysVoiceTermGlobalLock(SysVoice *sys);
__attribute__((noinline)) int sysVoiceLockGlobal(SysVoice *sys, const char *owner);
__attribute__((noinline)) int sysVoiceUnlockGlobal(SysVoice *sys);
void sysVoiceInitResourceLimits(VoiceResourceLimits *limits, int32_t appType);
void *sysVoiceAllocMem(SysVoice *sys, uint32_t size, const char *tag, int isPrivate);
void sysVoiceFreeMem(SysVoice *sys, void *ptr, int isPrivate);
int sysVoiceUpdateThreadEntry(SceSize args, void *argp);
void sysVoiceStaticInit(void);
int sysVoiceAttachInstance(SysVoice *sys, SysVoiceInstance *inst);
int sysVoiceHandleAttach(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceDetachInstance(SysVoice *sys, SysVoiceInstance *inst);
int sysVoiceHandleDetach(SysVoice *sys);

/* voice_ringbuf.c */
void voiceRingInit(VoiceRingBuffer *ring, void *buffer, uint16_t capacity);
void voiceRingTerm(VoiceRingBuffer *ring);
void voiceRingReset(VoiceRingBuffer *ring);
uint32_t voiceRingWriteAt(VoiceRingBuffer *ring, uint32_t offset, const void *src, uint32_t size);
uint32_t voiceRingRead(VoiceRingBuffer *ring, void *dst, uint32_t size);
uint32_t voiceRingWrite(VoiceRingBuffer *ring, const void *src, uint32_t size);

/* voice_codec.c */
void voiceEncoderSlotInit(VoiceEncoderSlot *slot, uint16_t slotIndex);
int voiceEncoderSlotCreate(VoiceEncoderSlot *slot, VoicePort *port);
int voiceEncoderSlotDelete(VoiceEncoderSlot *slot, VoicePort *port);
void *voiceEncoderSlotAllocBuf(VoiceEncoderSlot *slot);
void voiceEncoderSlotFreeBuf(VoiceEncoderSlot *slot);
uint32_t voiceEncoderPassThrough(VoiceEncoderSlot *slot, VoicePort *port, const void *src, void *dst);
uint32_t voiceEncoderEncodeFrame(VoiceEncoderSlot *slot, VoicePort *port, const void *src, void *dst);
void voiceDecoderSlotInit(VoiceDecoderSlot *slot, uint16_t slotIndex);
int voiceDecoderSlotCreate(VoiceDecoderSlot *slot, VoicePort *port);
int voiceDecoderSlotDelete(VoiceDecoderSlot *slot, VoicePort *port);
void *voiceDecoderSlotAllocBuf(VoiceDecoderSlot *slot);
void voiceDecoderSlotFreeBuf(VoiceDecoderSlot *slot);
uint32_t voiceDecoderDecodeFrame(VoiceDecoderSlot *slot, VoicePort *port, void *src, int16_t *dst, uint8_t lostFlag);
uint32_t voiceDecoderScalePcm(VoiceDecoderSlot *slot, VoicePort *port, const int16_t *src, int16_t *dst);
int sysVoiceHandleSetMuteAll(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleSetMute(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleGetMute(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleSetVolume(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleGetVolume(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleSetBitRate(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleGetBitRate(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleSetPortAttr(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleGetPortAttr(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleGetResourceInfo(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleCheckTopology(SysVoice *sys, VoiceCommand *cmd);

/* voice_device.c */
int voiceAudioInIoWrapper(int port, void *buf);
int voiceAudioOutIoWrapper(int port, void *buf);
int voiceDeviceThreadEntry(SceSize args, void *argp);
void voiceDeviceInit(VoiceAudioInDevice *dev, uint16_t slotIndex, int32_t freq, int32_t grain);
int voiceDeviceBindPort(VoiceAudioInDevice *dev, const VoicePort *port);
void voiceDeviceUnbindPort(VoiceAudioInDevice *dev);
int voiceDeviceStopIo(VoiceAudioInDevice *dev);
int voiceDeviceStartIo(VoiceAudioInDevice *dev);
int voiceDeviceTransfer(VoiceAudioInDevice *dev, int audioPort, void *buf);
int voiceAudioInOpen(VoiceAudioInDevice *dev);
int voiceAudioInClose(VoiceAudioInDevice *dev);
int voiceAudioInProcess(VoiceAudioInDevice *dev, VoicePort *port);
int voiceAudioOutOpen(VoiceAudioOutDevice *dev);
int voiceAudioOutClose(VoiceAudioOutDevice *dev);
int voiceAudioOutProcess(VoiceAudioOutDevice *dev, VoicePort *port);

/* voice_dsp.c */
int voiceRealFft(float *data, int length);
float voiceExpf(float x);
float voiceLogf(float x);
float voiceLog10f(float x);
float voiceSinf(float x);
float voiceCosf(float x);
float voiceSqrtf(float x);

/* voice_service.c */
int sysVoiceInstanceLockData(SysVoiceInstance *inst, const char *owner);
int sysVoiceInstanceUnlockData(SysVoiceInstance *inst);
int sysVoiceInstanceInit(SysVoiceInstance *inst, const VoiceCommand *cmd, uint32_t instanceId);
int sysVoiceInstanceTerm(SysVoiceInstance *inst);
int sysVoiceInstanceReservePort(SysVoiceInstance *inst, const SceVoicePortParam *param,
                                VoicePortFormatMetrics *metrics);
int sysVoiceInstanceReleasePort(SysVoiceInstance *inst, const VoicePort *port);
void sysVoiceInstanceNoop(void);
VoiceSharedPort *sysVoiceInstanceGetSharedPort(SysVoiceInstance *inst, uint32_t portId);
void sysVoiceInstanceAddPort(SysVoiceInstance *inst, VoicePort *port);
void sysVoiceInstanceRemovePort(SysVoiceInstance *inst, VoicePort *port);
void sysVoiceInstanceDeduplicateOutputs(SysVoiceInstance *inst);
int sysVoiceInstanceConnectPorts(SysVoiceInstance *inst, VoicePort *iPort, VoicePort *oPort);
int sysVoiceInstanceDisconnectPorts(SysVoiceInstance *inst, VoicePort *iPort, VoicePort *oPort);
void *sysVoiceInstanceAllocSharedTable(SysVoiceInstance *inst);
void sysVoiceInstanceFreeSharedTable(SysVoiceInstance *inst);
void sysVoiceInstanceBroadcastEncoded(SysVoiceInstance *inst, const VoicePort *srcPort,
                                      const void *encodedBuf, uint32_t encodedSize);
int16_t sysVoiceInstanceCollectDataReady(SysVoiceInstance *inst, SceVoiceEventData *eventData);
int sysVoiceProcessAudioInPorts(SysVoiceInstance *inst);
void sysVoiceProcessInputPorts(SysVoiceInstance *inst);
void sysVoiceProcessMixPorts(SysVoiceInstance *inst);
void sysVoiceProcessOutputPorts(SysVoiceInstance *inst);
void sysVoiceProcessAudioOutPorts(SysVoiceInstance *inst);

/* voice_mempool.c */
void voiceMemPoolInit(VoiceMemPool *pool, void *baseAddr, uint32_t totalSize,
                      VoiceMemBlock *blockTable, int blockCount, uint8_t isPrivate, const char *name);
void voiceMemPoolTerm(VoiceMemPool *pool);
void *voiceMemPoolAlloc(VoiceMemPool *pool, uint32_t size);
void voiceMemPoolCoalesce(VoiceMemBlock *target, VoiceList *freeList,
                          VoiceList *availList, VoiceMemBlock *neighbor);
void voiceMemPoolInsertSorted(VoiceMemBlock *block, VoiceList *freeList);
void voiceMemPoolFree(VoiceMemPool *pool, void *addr);

/* voice_pool.c */
int voiceDecoderPoolInit(VoiceDecoderPool *pool);
int voiceDecoderPoolTerm(VoiceDecoderPool *pool);
void *voiceDecoderPoolAllocSlotBuf(VoiceDecoderPool *pool, uint32_t index);
void voiceDecoderPoolFreeSlotBuf(VoiceDecoderPool *pool, uint32_t index);
int voiceDecoderPoolRelease(VoiceDecoderPool *pool, VoicePort *port);
int voiceDecoderPoolAcquire(VoiceDecoderPool *pool, VoicePort *port, const SysVoiceInstance *inst);
void voiceDecoderPoolDecodeFrames(VoiceDecoderPool *pool, VoicePort *port, uint32_t frameCount);
int voiceEncoderPoolInit(VoiceEncoderPool *pool);
int voiceEncoderPoolTerm(VoiceEncoderPool *pool);
void *voiceEncoderPoolAllocSlotBuf(VoiceEncoderPool *pool, uint32_t index);
void voiceEncoderPoolFreeSlotBuf(VoiceEncoderPool *pool, uint32_t index);
int voiceEncoderPoolRelease(VoiceEncoderPool *pool, VoicePort *port);
int voiceEncoderPoolAcquire(VoiceEncoderPool *pool, VoicePort *port, const SysVoiceInstance *inst);
void voiceEncoderPoolEncodeFrames(VoiceEncoderPool *pool, VoicePort *port, uint32_t frameCount,
                                  SysVoiceInstance *inst);
int voiceMixerInit(VoiceMixer *mixer);
int voiceMixerTerm(VoiceMixer *mixer);
void voiceMixerResetCache(VoiceMixer *mixer);
void voiceMixerAllocWorkBufs(VoiceMixer *mixer);
void voiceMixerFreeWorkBufs(VoiceMixer *mixer);
void voiceMixerMixPort(VoiceMixer *mixer, VoicePort *outPort, uint8_t muteAll);
int voiceAudioInPoolInit(VoiceAudioInPool *pool);
void voiceAudioInPoolTerm(VoiceAudioInPool *pool);
int voiceAudioInPoolRelease(VoiceAudioInPool *pool, VoicePort *port);
int voiceAudioInPoolAcquire(VoiceAudioInPool *pool, VoicePort *port, const SysVoiceInstance *inst);
int voiceAudioInPoolProcess(VoiceAudioInPool *pool, VoicePort *port);
int voiceAudioOutPoolInit(VoiceAudioOutPool *pool);
void voiceAudioOutPoolTerm(VoiceAudioOutPool *pool);
int voiceAudioOutPoolRelease(VoiceAudioOutPool *pool, VoicePort *port);
int voiceAudioOutPoolAcquire(VoiceAudioOutPool *pool, VoicePort *port, const SysVoiceInstance *inst);
int voiceAudioOutPoolProcess(VoiceAudioOutPool *pool, VoicePort *port);

/* voice_event.c */
int voiceEventThreadEntry(SceSize args, void *argp);
int voiceEventDispatcherInit(VoiceEventDispatcher *disp, SceVoiceEventCallback callback, void *userData);
int voiceEventDispatcherSend(VoiceEventDispatcher *disp, SceVoiceEventData *eventData);
int voiceEventDispatcherTerm(VoiceEventDispatcher *disp);

/* voice_port.c */
void voicePortInit(VoicePort *port, uint16_t portId);
void voicePortConfigure(VoicePort *port, const SceVoicePortParam *param, const VoicePortFormatMetrics *metrics);
void voicePortResetIds(VoicePort *port);
void voicePortBindShared(VoicePort *port, SysVoiceInstance *inst);
void voicePortUnbindShared(VoicePort *port, SysVoiceInstance *inst);
VoiceSharedPort *voicePortSyncShared(VoicePort *port, SysVoiceInstance *inst);
int voicePortHasSameTopology(const VoicePort *portA, const VoicePort *portB);
float voicePortGetThresholdMs(const VoicePort *port);
float voicePortGetBufferedMs(const VoicePort *port);
void voicePortAllocBuffers(VoicePort *port);
int voicePortAttachResources(VoicePort *port, SysVoiceInstance *inst);
void voicePortFreeBuffers(VoicePort *port);
int voicePortDetachResources(VoicePort *port, SysVoiceInstance *inst);
int voicePortAcquireEncoder(VoicePort *port, SysVoiceInstance *inst);
int voicePortReleaseEncoder(VoicePort *port);
int voicePortRunVad(VoicePort *port, const int16_t *pcmSamples, VoiceVadFilter *vad);
int voicePortCheckAudioInAdopt(VoicePort *port);
int sysVoiceStartUpdateThread(SysVoice *sys);
int sysVoiceStopUpdateThread(SysVoice *sys);
int sysVoiceHandleInit(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleStart(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleStop(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleCreatePort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleDeletePort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleEnd(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleUpdatePort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleConnectPort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleDisconnectPort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleResetPort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandlePausePort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandlePausePortAll(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleResumePort(SysVoice *sys, VoiceCommand *cmd);
int sysVoiceHandleResumePortAll(SysVoice *sys, VoiceCommand *cmd);

/* voice_vad.c */
void *voiceMemset(void *dst, uint8_t value, uint32_t size);
void *voiceMemcpy(void *dst, const void *src, uint32_t size);
int voiceMemcheck(const uint8_t *buf, uint8_t value, uint32_t size);
void voiceVadHangoverConfigInit(VoiceVadHangoverConfig *cfg);
void voiceVadHangoverStateInit(VoiceVadHangoverState *state);
int voiceVadHangoverUpdate(const VoiceVadHangoverConfig *cfg, VoiceVadHangoverState *state,
                           const int *decisionsBegin, const int *decisionsEnd);
int voiceVadFilterInit(VoiceVadFilter *vad, float sampleRate);
int voiceVadFilterProcess(VoiceVadFilter *vad, const int16_t *samplesBegin, const int16_t *samplesEnd);
int voiceVadFilterTerm(VoiceVadFilter *vad);
int voiceFirDownsample2x(float *dst, int dstCount, const float *src, int srcCount);
int voiceFirUpsample2x(float *dst, int dstCount, const float *src, int srcCount);

#endif /* VOICE_INTERNAL_H */
