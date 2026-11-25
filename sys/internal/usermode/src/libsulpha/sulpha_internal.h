/*
 * Internal types, structures, and prototypes for SceSulpha.
 */
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <scetypes.h>
#include <sulpha.h>

#define SULPHA_ERROR_INTERNAL		-2142437118	/* 0x804D0102 */
#define SULPHA_ERROR_IO			-2142437117	/* 0x804D0103 */
#define SULPHA_ERROR_TRANSPORT		-2142437116	/* 0x804D0104 */
#define SULPHA_ERROR_UNKNOWN_AGENT	-2142437113	/* 0x804D0107 */

#define SULPHA_MAX_AGENTS		32
#define SULPHA_AGENT_VERSION		0x00010002U	/* 65538 */
#define SULPHA_ALLOC_MAGIC_LIVE		0x45455245U	/* 'EREE' / 1162167621 */
#define SULPHA_ALLOC_MAGIC_MEASURE	0x50505050U	/* 'PPPP' / 1347440720 */
#define SULPHA_STREAM_BUF_SIZE		0x10000U
#define SULPHA_FILE_BUF_SIZE		0x2000U
#define SULPHA_FILE_WRITE_CHUNK		0x40000U
#define SULPHA_FILE_MAGIC		0x504C5553U	/* "SULP" */
#define SULPHA_FILE_VERSION		1000U
#define SULPHA_DEFAULT_PORT		4444U
#define SULPHA_DEFAULT_BOOKMARKS	128U

enum SulphaFieldType {
	SULPHA_FIELD_UINT8	= 1,
	SULPHA_FIELD_UINT16	= 2,
	SULPHA_FIELD_UINT32	= 3,
	SULPHA_FIELD_UINT64	= 4,
	SULPHA_FIELD_FLOAT32	= 7,
	SULPHA_FIELD_FLOAT64	= 8,
	SULPHA_FIELD_INT8	= 0x81,
	SULPHA_FIELD_INT16	= 0x82,
	SULPHA_FIELD_INT32	= 0x83,
	SULPHA_FIELD_INT64	= 0x84,
	SULPHA_FIELD_NODE_REF	= 0x8D,
	SULPHA_FIELD_STRING	= 0x8E,
	SULPHA_FIELD_BYTES	= 0xFE,
	SULPHA_FIELD_TIMESTAMP	= 0xFF,
};

enum SulphaProtoState {
	SULPHA_PROTO_IDLE	= 1,
	SULPHA_PROTO_CONNECTED	= 2,
	SULPHA_PROTO_HELLO	= 3,
	SULPHA_PROTO_STREAMING	= 4,
	SULPHA_PROTO_CLOSING	= 5,
	SULPHA_PROTO_STOPPING	= 6,
	SULPHA_PROTO_STOPPED	= 7,
	SULPHA_PROTO_ERROR	= 8,
};

enum SulphaBerState {
	SULPHA_BER_INIT		= 0,
	SULPHA_BER_OK		= 1,
	SULPHA_BER_ERROR	= 2,
	SULPHA_BER_EOF		= 3,
	SULPHA_BER_OVERFLOW	= 4,
};

typedef struct SulphaServer SulphaServer;
typedef struct SulphaAgent SulphaAgent;
typedef struct SulphaProtocol SulphaProtocol;
typedef struct SulphaAllocator SulphaAllocator;
typedef struct SulphaBerStream SulphaBerStream;
typedef struct SulphaBerReader SulphaBerReader;

typedef void *(*SulphaAllocFn)(SulphaAllocator *alloc, SceUInt32 size, SceUInt32 align);

struct SulphaAllocator {
	SceUInt32 magic;
	SceUInt32 totalSize;
	SceUIntPtr current;
	SceUIntPtr end;
	SceUIntPtr base;
	SulphaAllocFn alloc;
};
_Static_assert(sizeof(SulphaAllocator) == 24, "SulphaAllocator size");

typedef struct SulphaBerMarker {
	SceUInt16 prevOffset;
	SceUInt16 prevLen;
} SulphaBerMarker;
_Static_assert(sizeof(SulphaBerMarker) == 4, "SulphaBerMarker size");

struct SulphaBerReader {
	SceUInt32 end;
	SceUInt32 pos;
	SceUInt32 state;
	SceUInt8 *data;
};
_Static_assert(sizeof(SulphaBerReader) == 16, "SulphaBerReader size");

struct SulphaBerStream {
	union {
		struct {
			SceUInt32 writePos;
			SceUInt32 bodyStart;
			SceUInt32 containerOffset;
			SceUInt32 containerLen;
		};
		SulphaBerReader reader;
	};
	SceUInt32 state;
	SceUInt32 reserved;
	SceUInt32 capacity;
	SceUInt8 *buffer;
	SceUInt8 *header;
};
_Static_assert(sizeof(SulphaBerStream) == 36, "SulphaBerStream size");

typedef struct SulphaTaggedPtr {
	void *ptr;
	SceUInt32 tag;
} SulphaTaggedPtr;
_Static_assert(sizeof(SulphaTaggedPtr) == 8, "SulphaTaggedPtr size");

typedef struct SulphaFixedPool {
	volatile SulphaTaggedPtr head;
	void *base;
	SceInt32 capacity;
	SceUInt32 stride;
	SceUInt32 align;
	volatile SceInt32 usedCount;
	SceUInt32 reserved[3];
} SulphaFixedPool;
_Static_assert(sizeof(SulphaFixedPool) == 40, "SulphaFixedPool size");

typedef struct SulphaHeapBlock {
	SceUInt32 blockCount;
	struct SulphaHeapBlock *next;
} SulphaHeapBlock;

typedef struct SulphaHeap {
	SulphaHeapBlock *base;
	SulphaHeapBlock *freeHead;
	SceUInt32 reserved0;
	SceUInt32 totalBlocks;
	SceUInt32 allocCount;
	SceUInt32 reserved1;
} SulphaHeap;
_Static_assert(sizeof(SulphaHeap) == 24, "SulphaHeap size");

typedef struct SulphaQueueNode {
	struct SulphaQueueNode *volatile next;
	SceUInt32 reserved;
} SulphaQueueNode;
_Static_assert(sizeof(SulphaQueueNode) == 8, "SulphaQueueNode size");

typedef struct SulphaQueue {
	volatile SulphaTaggedPtr head;
	volatile SulphaTaggedPtr tail;
	SceUInt32 reserved;
	volatile SceInt32 count;
	SulphaQueueNode sentinel;
} SulphaQueue;
_Static_assert(sizeof(SulphaQueue) == 32, "SulphaQueue size");

typedef struct SulphaSpinLock {
	volatile SceUInt32 lock;
	SceUInt32 reserved;
} SulphaSpinLock;
_Static_assert(sizeof(SulphaSpinLock) == 8, "SulphaSpinLock size");

typedef struct SulphaTimer {
	SceUInt64 startTime;
	SceUInt64 prevTime;
	SceUInt64 curTime;
	SceUInt64 running;
} SulphaTimer;
_Static_assert(sizeof(SulphaTimer) == 32, "SulphaTimer size");

typedef struct SulphaSocket {
	SceUID fd;
	SceUInt32 owned;
	SceInt32 lastError;
} SulphaSocket;
_Static_assert(sizeof(SulphaSocket) == 12, "SulphaSocket size");

typedef struct SulphaListener {
	SulphaServer *server;
	SulphaSocket socket;
	SceUInt16 port;
	SceUInt16 reserved;
} SulphaListener;
_Static_assert(sizeof(SulphaListener) == 20, "SulphaListener size");

typedef struct SulphaFileCapture {
	SceUID fd;
	SceUInt32 enabled;
	SceUInt8 *buffer;
	SceUInt32 buffered;
	SceUInt32 capacity;
	char filename[256];
} SulphaFileCapture;
_Static_assert(sizeof(SulphaFileCapture) == 276, "SulphaFileCapture size");

typedef struct SulphaProtoStep {
	SceBool (*run)(SulphaProtocol *proto);
	const struct SulphaProtoStep *(*next)(SulphaProtocol *proto);
} SulphaProtoStep;

typedef struct SulphaEvent SulphaEvent;

struct SulphaProtocol {
	SceUInt32 state;
	SulphaServer *server;
	SulphaSocket socket;
	SulphaFileCapture *fileCapture;
	const SulphaProtoStep *rxStep;
	const SulphaProtoStep *txStep;
	SceUInt32 rxOffset;
	SceUInt32 rxRemaining;
	SceUInt32 txOffset;
	SceUInt32 txRemaining;
	SceUInt32 txMaxBatch;
	SulphaBerStream rxStream;
	SulphaBerStream txStream;
	SceUInt8 *txBuf;
	SulphaEvent *pendingEvent;
};
_Static_assert(sizeof(SulphaProtocol) == 132, "SulphaProtocol size");

typedef struct SulphaFieldDef {
	SceUInt8 id;
	SceUInt8 type;
	SceUInt16 count;
	const char *name;
	const char *desc;
	const char *unit;
} SulphaFieldDef;
_Static_assert(sizeof(SulphaFieldDef) == 16, "SulphaFieldDef size");

typedef struct SulphaExtFieldDef {
	SceUInt8 id;
	SceUInt8 type;
	SceUInt16 count;
	const char *name;
	const char *desc;
	const char *unit;
	SceUInt32 minVal;
	SceUInt32 maxVal;
	SceUInt32 defaultVal;
	SceUInt32 reserved;
} SulphaExtFieldDef;
_Static_assert(sizeof(SulphaExtFieldDef) == 32, "SulphaExtFieldDef size");

typedef struct SulphaEventDef {
	SceUInt8 id;
	SceUInt8 fieldCount;
	SceUInt16 flagsAndBlocks;
	const char *name;
	const char *desc;
	void *fields;
} SulphaEventDef;
_Static_assert(sizeof(SulphaEventDef) == 16, "SulphaEventDef size");

typedef struct SulphaEnumValueDef {
	SceInt32 value;
	const char *name;
	const char *desc;
} SulphaEnumValueDef;
_Static_assert(sizeof(SulphaEnumValueDef) == 12, "SulphaEnumValueDef size");

typedef struct SulphaEnumDef {
	SceUInt8 id;
	SceInt8 valueCount;
	SceUInt16 reserved;
	const char *name;
	const char *desc;
	const SulphaEnumValueDef *values;
} SulphaEnumDef;
_Static_assert(sizeof(SulphaEnumDef) == 16, "SulphaEnumDef size");

typedef struct SulphaAgentDef {
	const char *name;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	SceUInt32 versionBuild;
	const char *desc;
	SceUInt32 uuid[4];
	SceUInt32 typeId;
	SceUInt8 eventDefCount;
	SceUInt8 subEventDefCount;
	SceUInt8 enumDefCount;
	SceUInt8 reserved;
	SulphaEventDef *eventDefs;
	SulphaEventDef *subEventDefs;
	SulphaEnumDef *enumDefs;
} SulphaAgentDef;
_Static_assert(sizeof(SulphaAgentDef) == 52, "SulphaAgentDef size");

typedef struct SulphaAgentMemReq {
	SceUInt32 userDataSize;
	SceUInt32 defCopySize;
	SceUInt32 heapSize;
	SceUInt32 eventCount;
	SceUInt32 nodeCount;
} SulphaAgentMemReq;
_Static_assert(sizeof(SulphaAgentMemReq) == 20, "SulphaAgentMemReq size");

typedef struct SulphaAgentConfig SulphaAgentConfig;

typedef struct SulphaAgentCallbacks {
	SceInt32 (*getDef)(void *userData, const SulphaAgentDef **outDef);
	SceInt32 (*getMemReq)(const SulphaAgentConfig *cfg, SulphaAgentMemReq *outReq);
	SceInt32 (*init)(SulphaAgent *agent, const SulphaAgentConfig *cfg, void *userData, SceUInt32 userDataSize);
	void (*onStart)(SulphaAgent *agent, void *userData);
	SceInt32 (*shutdown)(SulphaAgent *agent, void *userData);
	void (*onStarted)(SulphaAgent *agent, void *userData);
	void (*onCommand)(SulphaAgent *agent, void *userData, SulphaBerReader *reader);
} SulphaAgentCallbacks;

struct SulphaAgentConfig {
	SceUInt32 agentTypeId;
	SceUInt32 reserved0;
	SceUInt32 version;
	const SulphaAgentCallbacks *callbacks;
};

typedef struct SulphaAgentsConfig {
	SceUInt32 count;
	const SulphaAgentConfig **agents;
} SulphaAgentsConfig;

typedef struct SulphaNode {
	SulphaAgent *agent;
	SceUInt32 nodeId;
	SceUInt32 parentId;
} SulphaNode;
_Static_assert(sizeof(SulphaNode) == 12, "SulphaNode size");

typedef struct SulphaEventEncoder {
	const SulphaEventDef *eventDef;
	SulphaBerStream stream;
	SulphaBerMarker bodyMarker;
	SulphaBerMarker fieldsMarker;
	SulphaBerMarker msgMarker;
	SceUInt8 fieldCount;
	SceUInt8 pad[3];
} SulphaEventEncoder;
_Static_assert(sizeof(SulphaEventEncoder) == 56, "SulphaEventEncoder size");

struct SulphaEvent {
	SulphaQueueNode queueNode;
	SulphaAgent *agent;
	SceUInt32 dataSize;
	SceUInt32 reserved;
	void *payload;
};
_Static_assert(sizeof(SulphaEvent) == 24, "SulphaEvent size");

struct SulphaAgent {
	SulphaFixedPool nodePool;
	SulphaFixedPool eventPool;
	SulphaFixedPool encoderPool;
	SulphaHeap heap;
	SulphaServer *server;
	const SulphaAgentCallbacks *callbacks;
	SulphaAgentDef *def;
	void *userData;
	SceUInt32 agentTypeId;
	SceUInt8 disabled;
	SceUInt8 capturing;
	SceUInt8 slotId;
	SceUInt8 reserved;
};
_Static_assert(sizeof(SulphaAgent) == 168, "SulphaAgent size");

struct SulphaServer {
	SulphaTimer timer;
	SulphaQueue eventQueue;
	SulphaFixedPool bookmarkPool;
	SulphaSpinLock heapLock;
	SulphaFileCapture fileCapture;
	SulphaProtocol protocol;
	SulphaListener listener;
	SceSulphaCallback notifyCallback;
	SceUInt8 agentCount;
	SceUInt8 pad[7];
	SulphaAgent agents[SULPHA_MAX_AGENTS];
};
_Static_assert(sizeof(SulphaServer) == 5928, "SulphaServer size");

typedef struct SulphaDeci4pState {
	SceUID threadId;
	SceUID callbackId;
	SceUID socketId;
	SceUID recvEvFlag;
	SceUID doneEvFlag;
	volatile SceInt32 availBytes;
	void *readBuf;
	SceInt32 readSize;
	SceInt32 readResult;
	SceInt32 readError;
	volatile SceUInt8 flags;
	SceUInt8 pad[3];
} SulphaDeci4pState;
_Static_assert(sizeof(SulphaDeci4pState) == 44, "SulphaDeci4pState size");

/* Globals */
extern volatile SceUInt32 g_sulphaNextNodeId;
extern volatile SceInt32 g_sulphaInitRefCount;
extern SulphaDeci4pState g_sulphaDeci4p;
extern SceInt32 g_sulphaDeci4pInitState;
extern volatile SceUInt32 g_sulphaUpdateLock;
extern SulphaServer *g_sulphaServer;
extern SceInt32 g_sulphaLogLevel;

/* ARMv7 exclusive load/store primitives (sulpha_atomic.S) */
SceUInt32 sulphaAtomicLoad32(const volatile void *ptr);
SceUInt32 sulphaAtomicStore32(SceUInt32 val, volatile void *ptr);
SceUInt64 sulphaAtomicLoad64(const volatile void *ptr);
SceUInt32 sulphaAtomicStore64(SceUInt64 val, volatile void *ptr);

/* Inline atomic helpers */
static inline SceUInt64 sulphaPackTaggedPtr(void *ptr, SceUInt32 tag)
{
	return ((SceUInt64)tag << 32) | (SceUInt32)(SceUIntPtr)ptr;
}

static inline SceUInt32 sulphaAtomicInc32(volatile SceInt32 *ptr)
{
	SceInt32 oldVal, newVal;
	do {
		oldVal = *ptr;
		newVal = oldVal + 1;
	} while ((SceInt32)sulphaAtomicLoad32(ptr) != oldVal ||
		 sulphaAtomicStore32((SceUInt32)newVal, ptr) != 0);
	return (SceUInt32)newVal;
}

static inline void sulphaAtomicDec32(volatile SceInt32 *ptr)
{
	SceInt32 oldVal, newVal;
	do {
		oldVal = *ptr;
		newVal = oldVal - 1;
	} while ((SceInt32)sulphaAtomicLoad32(ptr) != oldVal ||
		 sulphaAtomicStore32((SceUInt32)newVal, ptr) != 0);
}

/* sulpha_agent.c */
SceBool sulphaEncodeFieldList(SulphaBerStream *stream, const SulphaEventDef *eventDef);
SceInt32 sulphaAgentInit(SulphaAgent *agent, SulphaServer *server,
			 const SulphaAgentConfig *cfg, SulphaAllocator *alloc);
SceInt32 sulphaAgentShutdown(SulphaAgent *agent);
SceInt32 sulphaAgentHandleMessage(SulphaAgent *agent, SulphaBerStream *stream, SceUInt8 msgType);
SceInt32 sulphaAgentNodeNew(SulphaNode **outNode, SulphaAgent *agent, const SulphaNode *parent);
SceInt32 sulphaAgentNodeDelete(SulphaAgent *agent, SulphaNode *node, SceUInt8 eventId);
SceInt32 sulphaAgentEventNew(SulphaAgent *agent, SulphaEvent **outEvent, SceUInt8 eventId);
SceInt32 sulphaAgentEventAdd(SulphaEvent *event, SceUInt8 fieldId, const void *data, SceUInt32 size);
SceInt32 sulphaAgentEventDelete(SulphaAgent *agent, SulphaEvent *event, SceBool force);
SceInt32 sulphaAgentEventReport(SulphaAgent *agent, SulphaEvent *event, const SulphaNode *node);
void *sulphaBumpAlloc(SulphaAllocator *alloc, SceUInt32 size, SceUInt32 align);
SulphaAllocator *sulphaBumpAllocInit(SulphaAllocator *alloc, void *buf, SceUInt32 size);

/* sulpha_codec.c */
SceUInt32 sulphaBerReadLength(const SulphaBerReader *reader);
SceBool sulphaBerWriteTagValue(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len);
SulphaBerReader *sulphaBerReaderCopy(SulphaBerReader *dst, const SulphaBerReader *src);
SceBool sulphaBerReaderNext(SulphaBerReader *reader);
SceUInt32 sulphaBerReaderEnter(SulphaBerReader *reader);
SceBool sulphaBerReadUInt8(SulphaBerReader *reader, SceUInt8 expectedTag, SceUInt8 *outVal);
SceBool sulphaBerReadBytes(SulphaBerReader *reader, SceUInt8 expectedTag, const SceUInt8 **outPtr, SceUInt32 *outLen);
SulphaBerStream *sulphaBerResetBuffer(SulphaBerStream *stream, void *buf, SceUInt32 capacity);
SceBool sulphaBerWriteRaw(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len);
SceBool sulphaBerWriteUInt16(SulphaBerStream *stream, SceUInt16 val);
SceBool sulphaBerWriteUInt32(SulphaBerStream *stream, SceUInt32 val);
SceBool sulphaBerWriteTaggedUInt16(SulphaBerStream *stream, SceUInt8 tag, SceUInt16 val);
SceBool sulphaBerWriteTaggedInt16(SulphaBerStream *stream, SceUInt8 tag, SceInt16 val);
SceBool sulphaBerWriteTaggedUInt32(SulphaBerStream *stream, SceUInt8 tag, SceUInt32 val);
SceBool sulphaBerWriteTaggedInt32(SulphaBerStream *stream, SceUInt8 tag, SceInt32 val);
SceBool sulphaBerWriteTaggedUInt64(SulphaBerStream *stream, SceUInt8 tag, SceUInt64 val);
SceBool sulphaBerWriteTaggedUInt8(SulphaBerStream *stream, SceUInt8 tag, SceUInt8 val);
SceBool sulphaBerWriteTaggedInt8(SulphaBerStream *stream, SceUInt8 tag, SceInt8 val);
SceBool sulphaBerWriteTaggedFloat32(SulphaBerStream *stream, SceUInt8 tag, float val);
SceBool sulphaBerWriteTaggedBytes(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len);
SceBool sulphaBerBeginContainer(SulphaBerStream *stream, SceUInt8 tag, SceUInt8 flags, SulphaBerMarker *marker);
SceBool sulphaBerEndContainer(SulphaBerStream *stream, const SulphaBerMarker *marker);
void *sulphaBerAllocStream(SulphaBerStream *stream, SulphaAllocator *alloc, SceUInt32 capacity);
SulphaBerStream *sulphaBerInitStream(SulphaBerStream *stream, void *buf, SceUInt32 capacity);
SulphaBerStream *sulphaBerResetStream(SulphaBerStream *stream);
SceUInt8 *sulphaBerGetBuffer(const SulphaBerStream *stream);
SceUInt16 sulphaBerGetPayloadLength(const SulphaBerStream *stream);
SceUInt8 sulphaBerGetAgentId(const SulphaBerStream *stream);
SulphaBerStream *sulphaBerSetAgentId(SulphaBerStream *stream, SceUInt8 agentId);
SulphaBerStream *sulphaBerSetHeader(SulphaBerStream *stream, SceUInt8 flags, SceUInt8 agentId);
SceUInt32 sulphaBerFinalize(SulphaBerStream *stream);
SulphaBerStream *sulphaBerGetReader(SulphaBerStream *stream, SulphaBerReader **outReader);
SulphaBerStream *sulphaBerGetWriter(SulphaBerStream *stream, SulphaBerStream **outWriter);
SceInt32 sulphaBerBeginRead(SulphaBerStream *stream, SceUInt8 *outMsgType);

/* sulpha_pool.c */
SceInt32 sulphaPoolGetNeededMemory(SceInt32 count, SceUInt32 elemSize, SceUInt32 align, SceUInt32 *outSize);
SceInt32 sulphaPoolInit(SulphaFixedPool *pool, void *buf, SceInt32 count, SceUInt32 elemSize, SceUInt32 align);
SceInt32 sulphaPoolShutdown(SulphaFixedPool *pool);
SceInt32 sulphaPoolIsValid(const SulphaFixedPool *pool);
void *sulphaPoolAlloc(SulphaFixedPool *pool);
void sulphaPoolFree(SulphaFixedPool *pool, void *ptr);
SceUInt32 sulphaHeapGetNeededMemory(SceUInt32 size, SceUInt32 *outSize);
void sulphaHeapInit(SulphaHeap *heap, void *buf, SceUInt32 size);
void *sulphaHeapAlloc(SulphaHeap *heap, SceUInt32 size);
void sulphaHeapFree(SulphaHeap *heap, void *ptr);

/* sulpha_server.c */
const SulphaProtoStep *sulphaProtoCheckTxReady(SulphaProtocol *proto);
const SulphaProtoStep *sulphaProtoFinishTx(SulphaProtocol *proto);
const SulphaProtoStep *sulphaProtoResetRecvHeader(SulphaProtocol *proto);
SceUInt32 sulphaProtoSetState(SulphaProtocol *proto, SceUInt32 newState);
SulphaQueueNode *sulphaQueuePopA(SulphaQueue *queue);
SulphaQueueNode *sulphaQueuePopB(SulphaQueue *queue);
const SulphaProtoStep *sulphaProtoParseHeader(SulphaProtocol *proto);
SceBool sulphaProtoFlushTx(SulphaProtocol *proto);
SceBool sulphaProtoSendBuf(SulphaProtocol *proto);
SceBool sulphaProtoRecvBuf(SulphaProtocol *proto);
SceBool sulphaProtoDrainQueue(SulphaProtocol *proto);
void sulphaProtoInit(SulphaProtocol *proto, SulphaServer *server, SulphaAllocator *alloc);
void sulphaProtoCloseSocket(SulphaProtocol *proto);
void sulphaProtoReset(SulphaProtocol *proto, SceBool flush);
SceInt32 sulphaProtoUpdate(SulphaProtocol *proto);
SceInt32 sulphaProtoAccept(SulphaProtocol *proto, SulphaListener *listener);
SceInt32 sulphaProtoStartFileCapture(SulphaProtocol *proto, SulphaFileCapture *fc,
				     const SceUInt8 *agentIds, SceUInt8 agentCount);
SceBool sulphaWriteMsgHeader(SulphaBerStream *stream, SulphaServer *server,
			     SceUInt32 errCode, const char *errMsg, SceUInt64 *outTimestamp);
SceBool sulphaWriteSimpleResponse(SulphaBerStream *stream, SulphaServer *server,
				  SceUInt32 errCode, SceUInt8 respTag);
const SulphaProtoStep *sulphaProtoHandleMessage(SulphaProtocol *proto);
SceBool sulphaWriteBookmarkMsg(SulphaBerStream *stream, SulphaServer *server,
			       const SceSulphaUTF8 *name, SceInt32 id);
void *sulphaMeasureAlloc(SulphaAllocator *alloc, SceUInt32 size, SceUInt32 align);
SceInt32 sulphaServerGetNeededMemory(const SceSulphaConfig *config, SceUInt32 *outSize);
SceInt32 sulphaServerInit(SulphaServer *server, const SceSulphaConfig *config, SulphaAllocator *alloc);
SceInt32 sulphaServerShutdown(SulphaServer *server);
SceInt32 sulphaServerUpdate(SulphaServer *server);
SceInt32 sulphaServerFileConnect(SulphaServer *server, const char *filename);
SceInt32 sulphaServerFileDisconnect(SulphaServer *server);
SceInt32 sulphaServerIsFileConnected(const SulphaServer *server, SceBool *outConnected);
SceInt32 sulphaServerAgentsGetNeededMemory(const SulphaServer *server, const SulphaAgentsConfig *cfg,
					   SceUInt32 *outSize);
SceInt32 sulphaServerAgentsRegister(SulphaServer *server, const SulphaAgentsConfig *cfg,
				    SulphaAllocator *alloc, SceUInt64 *outHandles);
SceInt32 sulphaServerAgentsUnregister(SulphaServer *server, const SceUInt64 *handles, SceUInt32 count);
SceBool sulphaServerIsStreaming(const SulphaServer *server);
SceUInt32 sulphaServerGetActiveAgentIds(const SulphaServer *server, SceUInt8 *outIds, SceUInt32 maxCount);
SceInt32 sulphaServerDispatchMessage(SulphaServer *server, SulphaBerStream *stream, SceUInt8 msgType);
SceInt32 sulphaServerSetBookmark(SulphaServer *server, const SceSulphaUTF8 *name, SceInt32 id);
SceInt32 sulphaServerFreeBookmark(SulphaServer *server, SulphaEvent *event);
const char *sulphaProtocolErrorString(SceUInt32 errCode);
SceBool sulphaVformatString(char *buf, SceInt32 size, const char *fmt, va_list args);
SceBool sulphaFormatString(char *buf, SceInt32 size, const char *fmt, ...);

/* sulpha_platform.c */
SceSize sulphaStrlen(const char *s);
char *sulphaStrncpy(char *dst, const char *src, SceSize len);
void *sulphaMemset(void *dst, int ch, SceSize len);
void *sulphaMemcpy(void *dst, const void *src, SceSize len);
void sulphaListenerInit(SulphaListener *listener, const SceSulphaConfig *config, SulphaServer *server);
void sulphaListenerClose(SulphaListener *listener);
SceInt32 sulphaListenerStart(SulphaListener *listener);
SceBool sulphaListenerHasPending(SulphaListener *listener);
SceInt32 sulphaListenerAccept(SulphaListener *listener, SulphaSocket *outSocket);
SceInt32 sulphaDeci4pInit(void);
SceInt32 sulphaDeci4pShutdown(void);
void sulphaSocketInit(SulphaSocket *sock);
void sulphaSocketShutdown(SulphaSocket *sock);
SceInt32 sulphaSocketOpen(SulphaSocket *sock);
SceInt32 sulphaSocketClose(SulphaSocket *sock);
SceInt16 sulphaSocketAccept(const SulphaSocket *listenerSock, SulphaSocket *clientSock);
SceBool sulphaSocketIsReadable(const SulphaSocket *sock);
SceBool sulphaSocketIsOpen(const SulphaSocket *sock);
void sulphaSocketSetNonBlocking(SulphaSocket *sock, SceInt32 nonBlock);
SceInt32 sulphaSocketSend(SulphaSocket *sock, const void *buf, SceSize size);
SceInt32 sulphaSocketRecv(SulphaSocket *sock, void *buf, SceInt32 size);
SceInt32 sulphaSocketGetError(const SulphaSocket *sock);
SceInt32 sulphaSocketBind(SulphaSocket *sock, SceUInt16 port);
void sulphaFileCaptureInit(SulphaFileCapture *fc, SulphaAllocator *alloc);
void sulphaFileCaptureShutdown(SulphaFileCapture *fc);
SceInt32 sulphaFileCaptureOpen(SulphaFileCapture *fc, const char *filename);
SceInt32 sulphaFileCaptureClose(SulphaFileCapture *fc);
SceInt32 sulphaFileCaptureWrite(SulphaFileCapture *fc, const void *buf, SceUInt32 size);
void *sulphaClibMemcpy(void *dst, const void *src, SceSize len);
void *sulphaClibMemset(void *dst, int ch, SceSize len);
char *sulphaClibStrncpy(char *dst, const char *src, SceSize len);
SceSize sulphaClibStrnlen(const char *s);
SceInt32 sulphaClibVsnprintf(char *dst, SceSize size, const char *fmt, va_list args);
SceInt32 sulphaPlatformInit(void);
SceInt32 sulphaPlatformShutdown(void);
void sulphaSpinLockInit(SulphaSpinLock *lock, const char *name);
void sulphaSpinLockDestroy(SulphaSpinLock *lock);
void sulphaSpinLockAcquire(SulphaSpinLock *lock);
void sulphaSpinLockRelease(SulphaSpinLock *lock);
void sulphaTimerInit(SulphaTimer *timer);
void sulphaTimerReset(SulphaTimer *timer);
SceInt32 sulphaTimerStart(SulphaTimer *timer);
SceInt32 sulphaTimerStop(SulphaTimer *timer);
SceUInt64 sulphaTimerGetTimestamp(SulphaTimer *timer);

/* Additional SceSulpha exports */
int sceSulphaAgentsGetNeededMemory(const SulphaAgentsConfig *config, SceUInt32 *sizeInBytes);
int sceSulphaAgentsRegister(const SulphaAgentsConfig *config, void *buffer,
			    SceUInt32 sizeInBytes, SceUInt64 *handles);
int sceSulphaAgentsUnregister(const SceUInt64 *handles, SceUInt32 count);
int sceSulphaGetAgent(SceUInt64 handle, SulphaAgent **outAgent, void **outUserData);
int sceSulphaNodeNew(SulphaNode **outNode, SulphaAgent *agent, const SulphaNode *parent);
int sceSulphaNodeDelete(SulphaNode *node, SceUInt8 eventId);
int sceSulphaEventNew(SulphaEvent **outEvent, SulphaAgent *agent, SceUInt8 eventId);
int sceSulphaEventDelete(SulphaEvent *event);
int sceSulphaEventAdd(SulphaEvent *event, SceUInt8 fieldId, const void *data, SceUInt32 size);
int sceSulphaEventReport(SulphaEvent *event, const SulphaNode *node);
int sceSulphaGetTimestamp(SceUInt64 *timestamp);
SceUInt32 sceSulphaLogSetLevel(SceUInt32 level);
void sceSulphaLogHandler(const char *file, SceInt32 line, SceInt32 level,
			 const char *module, const char *fmt, va_list args);
