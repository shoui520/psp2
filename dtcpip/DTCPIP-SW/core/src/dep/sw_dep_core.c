/*
 * DTCPIP-SW "sw dep:" stream core, pipeline builder, ring buffer, message queue/manager,
 * processor dispatch, and RX buffer helpers (, ,
 * ).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include "sw_dep_internal.h"

struct SwDepModeChanEntry {
	unsigned int mode;
	unsigned int chan;
};

struct SwDepInputChunk {
	void *buf;
	unsigned int len;
	unsigned int extra;
};

/* RX ring descriptor layout owned by the Wagner driver */
struct SwRxDesc {
	unsigned int descAttr;		/* 0x00 */
	unsigned int len;		/* 0x04 */
	void *buf;			/* 0x08 */
	unsigned int descExtra;		/* 0x0C */
	unsigned int writtenLen;	/* 0x10 */
	unsigned int flags;		/* 0x14 */
};

struct SwRxRingTable {
	unsigned int count;		/* 0x00 */
	struct SwRxDesc *descs;		/* 0x04 */
};

struct SwRxChannel {
	struct SwRxRingTable *table;	/* 0x00 */
	unsigned int readIdx;		/* 0x04 */
	unsigned int popIdx;		/* 0x08 */
	unsigned int writeIdx;		/* 0x0C */
	int lastPktInfo;		/* 0x10 */
	int fullFlag;			/* 0x14 */
};

/* mode-to-ring-channel table */
static const struct SwDepModeChanEntry g_swDepModeChanTable[16] = {
	{ 0x00040001u, 0u }, { 0x00080001u, 1u }, { 0x00100001u, 2u }, { 0x00200001u, 3u },
	{ 0x00400001u, 4u }, { 0x00800001u, 5u }, { 0x01000001u, 6u }, { 0x02000001u, 7u },
	{ 0x00040002u, 0u }, { 0x00080002u, 1u }, { 0x00100002u, 2u }, { 0x00200002u, 3u },
	{ 0x00400002u, 4u }, { 0x00800002u, 5u }, { 0x01000002u, 6u }, { 0x02000002u, 7u },
};

/* debug/logging globals shared with the driver */
extern uint32_t g_swWgnLogMask;
static int g_swLastFullPopIdx = -1;

/* forward declarations */
int swDepCoreInitSubsystems(struct SwDepCore *core);
int swDepCoreFiniSubsystems(struct SwDepCore *core);
int swDepStreamInit(struct SwDepStream *stream, struct SwDepCore *core, unsigned char id);
int swDepStreamFini(struct SwDepStream *stream);
int swDepStreamConfigure(struct SwDepStream *stream, const void *config);
int swDepStreamStart(struct SwDepStream *stream);
int swDepStreamStop(struct SwDepStream *stream, int forceJoin);
int swDepStreamPushHdr(struct SwDepStream *stream, const void *hdr1, const void *hdr2);
unsigned int swDepStreamPushInput(struct SwDepStream *stream, const struct SwDepInputChunk *chunks,
				  unsigned int count);
int swDepStreamKickRx(struct SwDepStream *stream);
int swDepStreamCloseSession(struct SwDepStream *stream);
int swDepStreamSetCci(struct SwDepStream *stream, unsigned char cci);
int swDepStreamSetState(struct SwDepStream *stream, int state);
int swDepStreamBroadcastCtrl(struct SwDepStream *stream, int cmd, void *arg);
int swDepStreamHasWork(void *ctx);
int swDepStreamIsDone(void *ctx);
void swDepStreamProcess(void *ctx);
void swDepStreamOnExit(void *ctx);
void swDepStreamTraceNoop(void *stream);
int swDepChanMapInit(unsigned char *chanMap);
int swDepChanMapSet(unsigned char *chanMap, unsigned char key, unsigned char val);
unsigned char swDepChanMapGet(const unsigned char *chanMap, unsigned char key);
int swDepChanMapReset(unsigned char *chanMap);
unsigned char swDepModeToChan(unsigned int mode);
int swDepRingInit(struct SwDepRingBuf *ring, struct SwDepCore *core, unsigned char id);
int swDepRingFini(struct SwDepRingBuf *ring);
int swDepRingAcquire(struct SwDepRingBuf *ring, int isWriter, unsigned char streamId);
int swDepRingRelease(struct SwDepRingBuf *ring, int isWriter);
int swDepMsgMgrInit(struct SwDepMsgMgr *mgr, struct SwDepStream *stream);
int swDepMsgMgrFini(struct SwDepMsgMgr *mgr);
unsigned char swDepMsgMgrAddPool(struct SwDepMsgMgr *mgr, unsigned char *outId,
				 unsigned char msgType, unsigned int count,
				 unsigned char *baseBuf, unsigned int itemSize,
				 unsigned char useLock);
void swDepMsgMgrSetAllocCb(struct SwDepMsgMgr *mgr, unsigned char poolId,
			   int (*cb)(struct SwDepMsgMgr *, void *, struct SwDepMsg *), void *arg);
void swDepMsgMgrSetFreeCb(struct SwDepMsgMgr *mgr, unsigned char poolId,
			  void (*cb)(struct SwDepMsgMgr *, void *, struct SwDepMsg *), void *arg);
int swDepMsgMgrCommit(struct SwDepMsgMgr *mgr);
int swDepMsgMgrReset(struct SwDepMsgMgr *mgr);
void swDepMsgMgrBindWake(struct SwDepMsgMgr *mgr, unsigned char poolId, unsigned char procIdx);
int swDepGetRxBuff(struct SwRxChannel *chan, void **outBuf, unsigned int *outLen);
int swDepRxOne(void *driverCtx, unsigned char streamId, unsigned int writtenLen,
	       int flags, int pktInfo);

/* ---- Core lifecycle & channel map ---- */

int swDepInitCore(struct SwDepCore *core)
{
	int status;

	status = swBaseInit(core, &core->driverCtx);
	swDepCoreInitSubsystems(core);
	return status;
}

int swDepFiniCore(struct SwDepCore *core)
{
	swDepCoreFiniSubsystems(core);
	swBaseExit(core->driverCtx);
	return 0;
}

int swDepCoreSetChanMap(struct SwDepCore *core, unsigned char key, unsigned char val)
{
	swDepChanMapSet(core->chanMap, key, val);
	core->chanMap[key] = val;
	return val;
}

int swDepCoreResetChanMap(struct SwDepCore *core)
{
	return swDepChanMapReset(core->chanMap);
}

int swDepCoreFiniSubsystems(struct SwDepCore *core)
{
	int i;
	int j;
	int result = 0;

	for (i = 0; i < 1; ++i)
		swDepStreamFini(&core->streams[i]);
	for (j = 0; j < 1; ++j) {
		swDepRingFini(&core->ringBufs[j]);
		result = j + 1;
	}
	return result;
}

int swDepCoreInitSubsystems(struct SwDepCore *core)
{
	int i;
	int j;

	for (i = 0; i < 1; ++i)
		swDepRingInit(&core->ringBufs[i], core, (unsigned char)i);
	for (j = 0; j < 1; ++j)
		swDepStreamInit(&core->streams[j], core, (unsigned char)j);
	return swDepChanMapInit(core->chanMap);
}

int swDepCoreReset(struct SwDepCore *core)
{
	swDepCoreFiniSubsystems(core);
	return swDepCoreInitSubsystems(core);
}

int swDepCoreConfigureStream(struct SwDepCore *core, unsigned char streamId, const void *config)
{
	return swDepStreamConfigure(&core->streams[streamId], config);
}

int swDepCoreStartStream(struct SwDepCore *core, unsigned char streamId)
{
	return swDepStreamStart(&core->streams[streamId]);
}

int swDepCoreStopStreamGraceful(struct SwDepCore *core, unsigned char streamId)
{
	return swDepStreamStop(&core->streams[streamId], 0);
}

int swDepCoreStopStreamForce(struct SwDepCore *core, unsigned char streamId)
{
	return swDepStreamStop(&core->streams[streamId], 1);
}

int swDepCorePushHdr(struct SwDepCore *core, unsigned char streamId,
		     const void *hdr1, const void *hdr2)
{
	return swDepStreamPushHdr(&core->streams[streamId], hdr1, hdr2);
}

unsigned int swDepCorePushInput(struct SwDepCore *core, unsigned char streamId,
				const struct SwDepInputChunk *chunks, unsigned int count)
{
	return swDepStreamPushInput(&core->streams[streamId], chunks, count);
}

int swDepCoreKickRx(struct SwDepCore *core, unsigned char streamId)
{
	return swDepStreamKickRx(&core->streams[streamId]);
}

int swDepCoreSetCci(struct SwDepCore *core, unsigned char streamId, unsigned char cci)
{
	return swDepStreamSetCci(&core->streams[streamId], cci);
}

/* ---- Stream lifecycle & pipeline builders ---- */

int swDepStreamInit(struct SwDepStream *stream, struct SwDepCore *core, unsigned char id)
{
	struct SwDepProc *procs[12];
	const struct SwDepProcVtable *vtables[12];
	int status;
	int i;

	memset(stream, 0, sizeof(*stream));
	stream->core = core;
	stream->streamId = id;
	stream->state = 0;
	stream->workerStarted = 0;
	swDepMutexCreate(&stream->mutex);
	swDepWorkerEventInit(&stream->worker);

	status = swDepMsgMgrInit(&stream->msgMgr, stream);
	if (status != 0) {
		swDepWorkerEventFini(&stream->worker);
		swDepMutexDelete(&stream->mutex);
		return status;
	}

	procs[0] = (struct SwDepProc *)stream->procInput;
	vtables[0] = swDepProcInputVtable();
	procs[1] = (struct SwDepProc *)stream->procRingSrc;
	vtables[1] = swDepProcRingSrcVtable();
	procs[2] = (struct SwDepProc *)stream->procAesDecHdr;
	vtables[2] = swDepProcAesDecHdrVtable();
	procs[3] = (struct SwDepProc *)stream->procAesEnc1;
	vtables[3] = swDepProcAesEnc1Vtable();
	procs[4] = (struct SwDepProc *)stream->procTsConv;
	vtables[4] = swDepProcTsConvVtable();
	procs[5] = (struct SwDepProc *)stream->procOptParseTx;
	vtables[5] = swDepProcOptParseTxVtable();
	procs[6] = (struct SwDepProc *)stream->procOptParseRx;
	vtables[6] = swDepProcOptParseRxVtable();
	procs[7] = (struct SwDepProc *)stream->procCciTag;
	vtables[7] = swDepProcCciTagVtable();
	procs[8] = (struct SwDepProc *)stream->procAesEncKey;
	vtables[8] = swDepProcAesEncKeyVtable();
	procs[9] = (struct SwDepProc *)stream->procAesEnc2;
	vtables[9] = swDepProcAesEnc2Vtable();
	procs[10] = (struct SwDepProc *)stream->procOutputDma;
	vtables[10] = swDepProcOutputDmaVtable();
	procs[11] = (struct SwDepProc *)stream->procRingSink;
	vtables[11] = swDepProcRingSinkVtable();

	for (i = 0; i < 12; ++i) {
		status = swDepProcInit(procs[i], stream, vtables[i]);
		if (status != 0) {
			while (--i >= 0)
				swDepProcFini(procs[i]);
			swDepMsgMgrFini(&stream->msgMgr);
			swDepWorkerEventFini(&stream->worker);
			swDepMutexDelete(&stream->mutex);
			return status;
		}
	}
	return 0;
}

int swDepStreamFini(struct SwDepStream *stream)
{
	int state = stream->state;

	if (state == 2 || state == 3 || state == 4)
		swDepStreamStop(stream, 1);
	swDepWorkerEventFini(&stream->worker);
	if (stream->state == 1)
		swDepStreamCloseSession(stream);
	swDepProcFini((struct SwDepProc *)stream->procRingSink);
	swDepProcFini((struct SwDepProc *)stream->procOutputDma);
	swDepProcFini((struct SwDepProc *)stream->procAesEnc2);
	swDepProcFini((struct SwDepProc *)stream->procAesEncKey);
	swDepProcFini((struct SwDepProc *)stream->procCciTag);
	swDepProcFini((struct SwDepProc *)stream->procOptParseRx);
	swDepProcFini((struct SwDepProc *)stream->procOptParseTx);
	swDepProcFini((struct SwDepProc *)stream->procTsConv);
	swDepProcFini((struct SwDepProc *)stream->procAesEnc1);
	swDepProcFini((struct SwDepProc *)stream->procAesDecHdr);
	swDepProcFini((struct SwDepProc *)stream->procRingSrc);
	swDepProcFini((struct SwDepProc *)stream->procInput);
	swDepMsgMgrFini(&stream->msgMgr);
	return swDepMutexDelete(&stream->mutex);
}

static unsigned int swDepStreamAppendProc(struct SwDepStream *stream, struct SwDepProc *proc)
{
	unsigned int idx = stream->activeProcCount;

	stream->activeProcs[idx] = proc;
	proc->index = idx;
	if (proc->dstQueueId != 255u)
		swDepMsgMgrBindWake(&proc->stream->msgMgr, proc->dstQueueId, (unsigned char)idx);
	if (idx != 0)
		stream->activeProcs[idx - 1]->nextProc = proc;
	++stream->activeProcCount;
	return idx;
}

static int swDepStreamAllocRxCallback(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg)
{
	struct SwDepStream *stream = (struct SwDepStream *)arg;
	void *buf = NULL;
	unsigned int len = 0;
	int status;

	(void)mgr;
	swDepMutexLock(&stream->mutex);
	if (stream->state == 2 || stream->state == 3)
		status = swDepGetRxBuffWrapper(stream->core->driverCtx, stream->streamId,
					       &buf, &len);
	else
		status = 1;
	swDepMutexUnlock(&stream->mutex);
	if (status != 0)
		return status;
	msg->data = buf;
	msg->bufLen = len;
	return 0;
}

static void swDepStreamFreeTxCallback(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg)
{
	struct SwDepStream *stream = (struct SwDepStream *)arg;

	(void)mgr;
	(void)msg;
	swTxCompEvent(stream->core->driverCtx, stream->streamId, 1);
	swTxAvailEvent(stream->core->driverCtx, stream->streamId);
}

static int swDepStreamAllocRingNoop(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg)
{
	(void)mgr;
	(void)arg;
	(void)msg;
	return 0;
}

static void swDepStreamFreeRingCallback(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg)
{
	struct SwDepStream *stream = (struct SwDepStream *)arg;
	struct SwDepRingBuf *ring;

	(void)mgr;
	memcpy(&ring, stream->procRingSrc + 32, sizeof(ring));
	swDepRingConsume(ring, msg->bufLen);
}

#define swDepWorkBufSize(cfg, isDecStage) \
	(((cfg)[1] == 0) ? 51200u : (((isDecStage) || (cfg)[4] != 0) ? 24576u : 48128u))

static int swDepBuildPipelineEnc1ToDma(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[0] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->inQueueId, 2, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->inQueueId,
				     swDepStreamFreeTxCallback, stream);
	}
	if (flags[10] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->outQueueId, 5, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->outQueueId,
				      swDepStreamAllocRxCallback, stream);
	}
	if (flags[9] != 0)
		stream->aesEnc2QueueId = stream->outQueueId;
	else if (flags[4] != 0)
		stream->tsConvQueueId = stream->outQueueId;
	else if (flags[3] != 0)
		stream->aesEncQueueId = stream->outQueueId;

	if (flags[3] != 0 && stream->aesEncQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->aesEncQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->hdrQueueId, 1, 2, pool, 56u, 1);
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procInput, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procInput);
	if (flags[3] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc1, stream->aesEncQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc1);
	}
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	if (flags[9] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc2, stream->aesEnc2QueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc2);
	}
	swDepProcOpen((struct SwDepProc *)stream->procOutputDma, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOutputDma);
	return 0;
}

static int swDepBuildPipelineDecToDma(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[0] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->inQueueId, 2, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->inQueueId,
				     swDepStreamFreeTxCallback, stream);
	}
	if (flags[10] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->outQueueId, 5, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->outQueueId,
				      swDepStreamAllocRxCallback, stream);
	}
	if (flags[9] != 0)
		stream->aesEnc2QueueId = stream->outQueueId;
	else if (flags[4] != 0)
		stream->tsConvQueueId = stream->outQueueId;
	else if (flags[2] != 0)
		stream->aesDecQueueId = stream->outQueueId;

	if (flags[2] != 0 && stream->aesDecQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 1);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->aesDecQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->hdrQueueId, 1, 2, pool, 56u, 1);
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procInput, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procInput);
	if (flags[2] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesDecHdr, stream->aesDecQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesDecHdr);
	}
	if (flags[6] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procOptParseRx, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOptParseRx);
	}
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	if (flags[9] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc2, stream->aesEnc2QueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc2);
	}
	swDepProcOpen((struct SwDepProc *)stream->procOutputDma, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOutputDma);
	return 0;
}

static int swDepBuildPipelineEnc1ToKeyDma(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[0] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->inQueueId, 2, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->inQueueId,
				     swDepStreamFreeTxCallback, stream);
	}
	if (flags[10] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->outQueueId, 5, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->outQueueId,
				      swDepStreamAllocRxCallback, stream);
	}
	stream->aesEnc3QueueId = stream->outQueueId;

	if (flags[3] != 0 && stream->aesEncQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->aesEncQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procInput, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procInput);
	if (flags[3] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc1, stream->aesEncQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc1);
	}
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	if (flags[5] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procOptParseTx, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOptParseTx);
	} else if (flags[7] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procCciTag, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procCciTag);
	}
	if (flags[8] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEncKey, stream->aesEnc3QueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEncKey);
	}
	swDepProcOpen((struct SwDepProc *)stream->procOutputDma, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOutputDma);
	return 0;
}

static int swDepBuildPipelineRingToDma(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[1] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->ringInQueueId, 3, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->ringInQueueId,
				     swDepStreamFreeRingCallback, stream);
	}
	if (flags[10] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->outQueueId, 5, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->outQueueId,
				      swDepStreamAllocRxCallback, stream);
	}
	if (flags[9] != 0)
		stream->aesEnc2QueueId = stream->outQueueId;
	else if (flags[4] != 0)
		stream->tsConvQueueId = stream->outQueueId;

	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procRingSrc, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procRingSrc);
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	if (flags[9] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc2, stream->aesEnc2QueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc2);
	}
	swDepProcOpen((struct SwDepProc *)stream->procOutputDma, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOutputDma);
	return 0;
}

static int swDepBuildPipelineRingToKeyDma(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[1] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->ringInQueueId, 3, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->ringInQueueId,
				     swDepStreamFreeRingCallback, stream);
	}
	if (flags[10] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->outQueueId, 5, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->outQueueId,
				      swDepStreamAllocRxCallback, stream);
	}
	stream->aesEnc3QueueId = stream->outQueueId;

	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procRingSrc, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procRingSrc);
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	if (flags[5] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procOptParseTx, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOptParseTx);
	} else if (flags[7] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procCciTag, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procCciTag);
	}
	if (flags[8] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEncKey, stream->aesEnc3QueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEncKey);
	}
	swDepProcOpen((struct SwDepProc *)stream->procOutputDma, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOutputDma);
	return 0;
}

static int swDepBuildPipelineEnc1ToRing(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[0] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->inQueueId, 2, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->inQueueId,
				     swDepStreamFreeTxCallback, stream);
	}
	if (flags[11] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->ringOutQueueId, 6, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->ringOutQueueId,
				      swDepStreamAllocRingNoop, stream);
	}
	if (flags[4] != 0)
		stream->tsConvQueueId = stream->outQueueId;
	else if (flags[3] != 0)
		stream->aesEncQueueId = stream->outQueueId;

	if (flags[3] != 0 && stream->aesEncQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->aesEncQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->hdrQueueId, 1, 2, pool, 56u, 1);
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procInput, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procInput);
	if (flags[3] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procAesEnc1, stream->aesEncQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesEnc1);
	}
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	swDepProcOpen((struct SwDepProc *)stream->procRingSink, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procRingSink);
	return 0;
}

static int swDepBuildPipelineDecToRing(struct SwDepStream *stream, const int *flags)
{
	const unsigned int *cfg = (const unsigned int *)stream->config;
	unsigned char *pool = stream->workPool;
	unsigned int itemSize;
	int status;

	if (flags[0] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->inQueueId, 2, 16, NULL, 0, 1);
		swDepMsgMgrSetFreeCb(&stream->msgMgr, stream->inQueueId,
				     swDepStreamFreeTxCallback, stream);
	}
	if (flags[11] != 0) {
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->ringOutQueueId, 6, 16, NULL, 0, 1);
		swDepMsgMgrSetAllocCb(&stream->msgMgr, stream->ringOutQueueId,
				      swDepStreamAllocRingNoop, stream);
	}
	if (flags[4] != 0)
		stream->tsConvQueueId = stream->outQueueId;
	else if (flags[2] != 0)
		stream->aesDecQueueId = stream->outQueueId;

	if (flags[2] != 0 && stream->aesDecQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 1);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->aesDecQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	if (flags[4] != 0 && stream->tsConvQueueId == 255u) {
		itemSize = swDepWorkBufSize(cfg, 0);
		swDepMsgMgrAddPool(&stream->msgMgr, &stream->tsConvQueueId, 4, 3, pool, itemSize, 0);
		pool += 3u * itemSize;
	}
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->hdrQueueId, 1, 2, pool, 56u, 1);
	swDepMsgMgrAddPool(&stream->msgMgr, &stream->eosQueueId, 7, 1, NULL, 0, 1);
	status = swDepMsgMgrCommit(&stream->msgMgr);
	if (status != 0)
		return status;

	swDepProcOpen((struct SwDepProc *)stream->procInput, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procInput);
	swDepProcOpen((struct SwDepProc *)stream->procAesDecHdr, stream->aesDecQueueId,
		      stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procAesDecHdr);
	if (flags[6] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procOptParseRx, 255u, stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procOptParseRx);
	}
	if (flags[4] != 0) {
		swDepProcOpen((struct SwDepProc *)stream->procTsConv, stream->tsConvQueueId,
			      stream->config);
		swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procTsConv);
	}
	swDepProcOpen((struct SwDepProc *)stream->procRingSink, 255u, stream->config);
	swDepStreamAppendProc(stream, (struct SwDepProc *)stream->procRingSink);
	return 0;
}

int swDepStreamConfigure(struct SwDepStream *stream, const void *config)
{
	int flags[12];
	const unsigned int *cfg;
	unsigned int mode;
	unsigned int i;
	int status = 0;

	if (stream->state != 0 && stream->state != 1)
		return 1;
	if (stream->state == 1) {
		status = swDepStreamCloseSession(stream);
		if (status != 0)
			return status;
	}

	stream->activeProcCount = 0;
	stream->rxProcIdx = 0;
	stream->stat0 = 0;
	stream->stat1 = 0;
	memcpy(stream->config, config, sizeof(stream->config));
	memset(flags, 0, sizeof(flags));
	cfg = (const unsigned int *)stream->config;
	mode = cfg[0] & 0x03FF03FFu;

	switch (mode) {
	case 0x20001u: case 0x20004u: case 0x20008u: case 0x20010u: case 0x20020u:
	case 0x20040u: case 0x20080u: case 0x20100u: case 0x20200u:
		flags[0] = 1;
		flags[2] = 1;
		if (cfg[1] != 0)
			flags[6] = 1;
		break;
	case 0x10001u: case 0x10002u: case 0x10004u: case 0x10008u: case 0x10010u:
	case 0x10020u: case 0x10040u: case 0x10080u: case 0x10100u: case 0x10200u:
		flags[0] = 1;
		if (cfg[9] == 1u)
			flags[3] = 1;
		break;
	default:
		flags[1] = 1;
		break;
	}

	switch (mode) {
	case 0x00010002u: case 0x00040002u: case 0x00080002u: case 0x00100002u:
	case 0x00200002u: case 0x00400002u: case 0x00800002u: case 0x01000002u:
	case 0x02000002u:
		flags[10] = 1;
		flags[8] = 1;
		if (cfg[1] != 0)
			flags[5] = 1;
		else
			flags[7] = 1;
		break;
	case 0x00010001u: case 0x00020001u: case 0x00040001u: case 0x00080001u:
	case 0x00100001u: case 0x00200001u: case 0x00400001u: case 0x00800001u:
	case 0x01000001u: case 0x02000001u:
		flags[10] = 1;
		if (cfg[19] == 1u)
			flags[9] = 1;
		break;
	default:
		flags[11] = 1;
		break;
	}

	if (cfg[1] != 0 && cfg[4] != cfg[5])
		flags[4] = 1;

	stream->inQueueId = 255u;
	stream->outQueueId = 255u;
	stream->ringInQueueId = 255u;
	stream->ringOutQueueId = 255u;
	stream->aesEncQueueId = 255u;
	stream->aesDecQueueId = 255u;
	stream->tsConvQueueId = 255u;
	stream->aesEnc2QueueId = 255u;
	stream->aesEnc3QueueId = 255u;
	stream->hdrQueueId = 255u;
	stream->eosQueueId = 255u;

	switch (mode) {
	case 0x00010001u:
		status = swDepBuildPipelineEnc1ToDma(stream, flags);
		break;
	case 0x00010002u:
		status = swDepBuildPipelineEnc1ToKeyDma(stream, flags);
		break;
	case 0x00010004u: case 0x00010008u: case 0x00010010u: case 0x00010020u:
	case 0x00010040u: case 0x00010080u: case 0x00010100u: case 0x00010200u:
		status = swDepBuildPipelineEnc1ToRing(stream, flags);
		break;
	case 0x00020001u:
		status = swDepBuildPipelineDecToDma(stream, flags);
		break;
	case 0x00020004u: case 0x00020008u: case 0x00020010u: case 0x00020020u:
	case 0x00020040u: case 0x00020080u: case 0x00020100u: case 0x00020200u:
		status = swDepBuildPipelineDecToRing(stream, flags);
		break;
	case 0x00040001u: case 0x00080001u: case 0x00100001u: case 0x00200001u:
	case 0x00400001u: case 0x00800001u: case 0x01000001u: case 0x02000001u:
		status = swDepBuildPipelineRingToDma(stream, flags);
		break;
	case 0x00040002u: case 0x00080002u: case 0x00100002u: case 0x00200002u:
	case 0x00400002u: case 0x00800002u: case 0x01000002u: case 0x02000002u:
		status = swDepBuildPipelineRingToKeyDma(stream, flags);
		break;
	default:
		return 11;
	}

	if (status != 0)
		return status;
	swDepStreamSetState(stream, 1);
	stream->rxProcIdx = 0;
	for (i = 0; i < stream->activeProcCount; ++i) {
		if (stream->activeProcs[i]->dstQueueId == stream->outQueueId)
			stream->rxProcIdx = i;
	}
	return status;
}

int swDepStreamStart(struct SwDepStream *stream)
{
	unsigned int i;
	int status;

	if (stream->state != 1)
		return 1;
	for (i = 0; i < stream->activeProcCount; ++i) {
		if (swDepProcIsReadyStub(stream->activeProcs[i], 1) == 0)
			return 1;
	}
	stream->stat0 = 0;
	swDepStreamSetState(stream, 2);
	status = swDepWorkerStart(&stream->worker, "", swDepStreamHasWork, swDepStreamIsDone,
				  swDepStreamProcess, swDepStreamOnExit, stream);
	if (status != 0)
		stream->state = 1;
	else
		stream->workerStarted = 1;
	return status;
}

int swDepStreamStop(struct SwDepStream *stream, int forceJoin)
{
	int state = stream->state;

	if (forceJoin != 0) {
		if (stream->workerStarted != 0) {
			swDepWorkerStop(&stream->worker);
			stream->workerStarted = 0;
			return 0;
		}
		return stream->workerStarted;
	}
	if (state == 2) {
		swDepStreamSetState(stream, 3);
		swDepProcPushEos(stream->activeProcs[0]);
		return swDepStreamWake(stream);
	}
	return stream->state;
}

int swDepStreamPushHdr(struct SwDepStream *stream, const void *hdr1, const void *hdr2)
{
	struct SwDepMsg *msg;
	unsigned char *dst;

	msg = swDepMsgMgrAlloc(&stream->msgMgr, stream->hdrQueueId);
	dst = (unsigned char *)msg->data;
	memcpy(dst, hdr1, 40);
	memcpy(dst + 40, hdr2, 14);
	msg->len = 56;
	swDepProcPush(stream->activeProcs[0], msg);
	swDepStreamTraceNoop(stream);
	return 0;
}

unsigned int swDepStreamPushInput(struct SwDepStream *stream, const struct SwDepInputChunk *chunks,
				  unsigned int count)
{
	struct SwDepMsg *msg;
	unsigned int i;
	unsigned int len;

	for (i = 0; i < count; ++i) {
		msg = swDepMsgMgrAlloc(&stream->msgMgr, stream->inQueueId);
		if (msg == NULL)
			break;
		len = chunks[i].len;
		msg->data = chunks[i].buf;
		msg->bufLen = len;
		msg->len = len;
		swDepProcPush(stream->activeProcs[0], msg);
	}
	if (i != 0)
		swDepStreamWake(stream);
	return i;
}

int swDepStreamKickRx(struct SwDepStream *stream)
{
	stream->activeProcs[stream->rxProcIdx]->isReady = 1;
	return swDepStreamWake(stream);
}

int swDepStreamWake(struct SwDepStream *stream)
{
	if (stream->workerStarted != 0)
		return swDepWorkerWake(&stream->worker);
	return 0;
}

int swDepStreamSetDone(struct SwDepStream *stream)
{
	return swDepStreamSetState(stream, 4);
}

int swDepStreamHasWork(void *ctx)
{
	struct SwDepStream *stream = (struct SwDepStream *)ctx;
	unsigned int i;

	for (i = 0; i < stream->activeProcCount; ++i) {
		if (stream->activeProcs[i]->isReady != 0)
			return 1;
	}
	return 0;
}

int swDepStreamIsDone(void *ctx)
{
	struct SwDepStream *stream = (struct SwDepStream *)ctx;

	return stream->state == 4;
}

void swDepStreamProcess(void *ctx)
{
	struct SwDepStream *stream = (struct SwDepStream *)ctx;
	unsigned int i;
	int ran;

	do {
		ran = 0;
		for (i = 0; i < stream->activeProcCount; ++i) {
			struct SwDepProc *proc = stream->activeProcs[i];

			if (proc->isReady != 0) {
				swDepProcRun(proc);
				ran = 1;
			}
		}
	} while (ran != 0);
}

int swDepStreamCloseSession(struct SwDepStream *stream)
{
	if (stream->state == 0)
		return 1;
	swDepMsgMgrReset(&stream->msgMgr);
	stream->activeProcCount = 0;
	swDepStreamSetState(stream, 0);
	return 0;
}

void swDepStreamOnExit(void *ctx)
{
	struct SwDepStream *stream = (struct SwDepStream *)ctx;

	swStopCompCallback(stream->core->driverCtx, stream->streamId);
	swDepStreamCloseSession(stream);
}

int swDepStreamNotifyError(struct SwDepStream *stream, int errorBits)
{
	return swSetStreamStatusWrap(stream->core->driverCtx, stream->streamId, (uint32_t)errorBits);
}

void swDepStreamGetKey(struct SwDepStream *stream, unsigned char emi, int forceNext,
		       void *outKeyInfo, void *outPcpHeader)
{
	swGetEncParamWrap(stream->core->driverCtx, stream->streamId, emi, forceNext,
			  outKeyInfo, outPcpHeader);
	swDepStreamTraceNoop(stream);
}

int swDepStreamNotifyCci(struct SwDepStream *stream, void *cciDesc, unsigned char cci,
			 unsigned char prevCci)
{
	swSetCciExt(stream->core->driverCtx, stream->streamId,
		    (const unsigned char *)cciDesc + 4, cci, prevCci);
	return 0;
}

void swDepStreamNoop(void)
{
}

int swDepStreamSetCci(struct SwDepStream *stream, unsigned char cci)
{
	unsigned char val = cci;

	swDepStreamBroadcastCtrl(stream, 1, &val);
	return 0;
}

int swDepStreamRxOne(struct SwDepStream *stream, void *buf, unsigned int len,
		     int flags, int pktInfo)
{
	return swDepRxOneWrapper(stream->core->driverCtx, stream->streamId, buf, len,
				 flags, pktInfo);
}

int swDepStreamAcquireRing(struct SwDepStream *stream, unsigned int mode, int isWriter,
			   struct SwDepRingBuf **outRing)
{
	unsigned char chan;
	unsigned char mapped;
	struct SwDepRingBuf *ring;
	int status;

	chan = swDepModeToChan(mode);
	if (chan == 1u)
		return 11;
	mapped = swDepChanMapGet(stream->core->chanMap, chan);
	ring = &stream->core->ringBufs[mapped];
	status = swDepRingAcquire(ring, isWriter, stream->streamId);
	if (status != 0)
		return status;
	*outRing = ring;
	return 0;
}

int swDepStreamReleaseRing(struct SwDepStream *stream, int isWriter,
			   struct SwDepRingBuf *ring)
{
	(void)stream;
	return swDepRingRelease(ring, isWriter);
}

int swDepStreamBroadcastCtrl(struct SwDepStream *stream, int cmd, void *arg)
{
	unsigned int i;
	int status;

	for (i = 0; i < stream->activeProcCount; ++i) {
		status = swDepProcCtrl(stream->activeProcs[i], cmd, arg);
		if (status != 0)
			return status;
	}
	return 0;
}

int swDepStreamDump(struct SwDepStream *stream)
{
	unsigned int i;

	swMwTrace("sw dep:==============================================\n");
	swMwTrace("sw dep:stream %d:\n", stream->streamId);
	swMwTrace("sw dep:state  %d:\n", stream->state);
	for (i = 0; i < stream->activeProcCount; ++i)
		swDepProcDump(stream->activeProcs[i]);
	return swMwTrace("sw dep:==============================================\n");
}

void swDepStreamTraceNoop(void *stream)
{
	(void)stream;
}

int swDepStreamSetState(struct SwDepStream *stream, int state)
{
	stream->state = state;
	return state;
}

/* ---- Channel map & ring buffer ---- */

unsigned char swDepModeToChan(unsigned int mode)
{
	unsigned int i;

	for (i = 0; i < 16u; ++i) {
		if (g_swDepModeChanTable[i].mode == (mode & 0x03FF03FFu))
			return (unsigned char)g_swDepModeChanTable[i].chan;
	}
	return 1u;
}

int swDepChanMapInit(unsigned char *chanMap)
{
	return swDepChanMapReset(chanMap);
}

void swDepChanMapFini(unsigned char *chanMap)
{
	(void)chanMap;
}

int swDepChanMapSet(unsigned char *chanMap, unsigned char key, unsigned char val)
{
	if (key >= 8u)
		return 11;
	if (val >= 8u)
		return 11;
	chanMap[key] = val;
	return 0;
}

unsigned char swDepChanMapGet(const unsigned char *chanMap, unsigned char key)
{
	return chanMap[key];
}

int swDepChanMapReset(unsigned char *chanMap)
{
	unsigned int i;

	for (i = 0; i < 8u; ++i)
		chanMap[i] = (unsigned char)i;
	return 0;
}

int swDepRingInit(struct SwDepRingBuf *ring, struct SwDepCore *core, unsigned char id)
{
	(void)core;
	memset(ring, 0, sizeof(*ring));
	swDepMutexCreate(&ring->mutex);
	ring->id = id;
	ring->readerStream = 255u;
	ring->writerStream = 255u;
	ring->writerActive = 0;
	ring->readerActive = 0;
	ring->eosFlag = 0;
	ring->capacity = 36096u;
	ring->writeCount = 0;
	ring->commitPtr = ring->storage;
	ring->writePtr = ring->storage;
	ring->endPtr = ring->storage + ring->capacity;
	return 0;
}

int swDepRingFini(struct SwDepRingBuf *ring)
{
	return swDepMutexDelete(&ring->mutex);
}

int swDepRingPeekRead(struct SwDepRingBuf *ring, void **outPtr, unsigned int *outLen,
		      int *outEos)
{
	unsigned int toEnd;

	if (ring->eosFlag != 0 && ring->readCount == 0) {
		*outPtr = NULL;
		*outLen = 0;
		*outEos = 1;
		return 1;
	}
	toEnd = (unsigned int)(ring->endPtr - ring->readPtr);
	*outPtr = ring->readPtr;
	*outLen = (toEnd >= ring->readCount) ? ring->readCount : toEnd;
	*outEos = 0;
	return 0;
}

int swDepRingConsume(struct SwDepRingBuf *ring, unsigned int len)
{
	swDepMutexLock(&ring->mutex);
	if (ring->readCount < len) {
		swDepMutexUnlock(&ring->mutex);
		return 11;
	}
	if (ring->readPtr + len < ring->endPtr)
		ring->readPtr += len;
	else
		ring->readPtr = ring->storage + (len - (unsigned int)(ring->endPtr - ring->readPtr));
	ring->readCount -= len;
	swDepMutexUnlock(&ring->mutex);
	return 0;
}

int swDepRingPeekCommit(struct SwDepRingBuf *ring, void **outPtr, unsigned int *outLen,
			int *outEos)
{
	unsigned int toEnd;

	if (ring->eosFlag != 0 && ring->writeCount == 0) {
		*outPtr = NULL;
		*outLen = 0;
		*outEos = 1;
		return 1;
	}
	toEnd = (unsigned int)(ring->endPtr - ring->commitPtr);
	*outPtr = ring->commitPtr;
	*outLen = (toEnd >= ring->writeCount) ? ring->writeCount : toEnd;
	*outEos = 0;
	return 0;
}

int swDepRingReleaseCommit(struct SwDepRingBuf *ring, unsigned int len)
{
	swDepMutexLock(&ring->mutex);
	if (ring->writeCount < len) {
		swDepMutexUnlock(&ring->mutex);
		return 11;
	}
	if (ring->commitPtr + len < ring->endPtr)
		ring->commitPtr += len;
	else
		ring->commitPtr = ring->storage + (len - (unsigned int)(ring->endPtr - ring->commitPtr));
	ring->writeCount -= len;
	swDepMutexUnlock(&ring->mutex);
	if (ring->writerStream != 255u)
		swDepStreamNoop();
	return 0;
}

unsigned int swDepRingPeekWrite(struct SwDepRingBuf *ring, void **outPtr,
				unsigned int *outLen)
{
	unsigned int toEnd = (unsigned int)(ring->endPtr - ring->commitPtr);
	unsigned int avail;

	if (ring->eosFlag != 0) {
		*outPtr = NULL;
		*outLen = 0;
		return 0;
	}
	*outPtr = ring->commitPtr;
	avail = (toEnd >= ring->writeCount) ? ring->writeCount : toEnd;
	*outLen = avail;
	return avail;
}

int swDepRingCommit(struct SwDepRingBuf *ring, unsigned int len)
{
	swDepMutexLock(&ring->mutex);
	if (ring->eosFlag != 0 && ring->capacity - ring->writeCount < len) {
		swDepMutexUnlock(&ring->mutex);
		return 11;
	}
	if (ring->writePtr < ring->endPtr)
		ring->writePtr += len;
	else
		ring->writePtr = ring->storage + (len - (unsigned int)(ring->endPtr - ring->writePtr));
	ring->writeCount += len;
	ring->readCount += len;
	swDepMutexUnlock(&ring->mutex);
	if (ring->readerStream != 255u)
		swDepStreamNoop();
	return 0;
}

void swDepRingSetEos(struct SwDepRingBuf *ring)
{
	swDepMutexLock(&ring->mutex);
	ring->eosFlag = 0;
	swDepMutexUnlock(&ring->mutex);
	if (ring->readerStream != 255u)
		swDepStreamNoop();
}

int swDepRingAcquire(struct SwDepRingBuf *ring, int isWriter, unsigned char streamId)
{
	swDepMutexLock(&ring->mutex);
	if (isWriter != 0) {
		if (ring->writerStream != 255u)
			return 11;
		if (ring->writerActive != 0)
			return 11;
		ring->writerStream = streamId;
		ring->writerActive = 1;
	} else {
		if (ring->readerStream != 255u)
			return 11;
		if (ring->readerActive != 0)
			return 11;
		ring->readerStream = streamId;
		ring->readerActive = 1;
	}
	swDepMutexUnlock(&ring->mutex);
	return 0;
}

int swDepRingRelease(struct SwDepRingBuf *ring, int isWriter)
{
	int bothFree;

	swDepMutexLock(&ring->mutex);
	if (isWriter != 0) {
		ring->writerStream = 255u;
		bothFree = (ring->readerStream == 255u);
	} else {
		ring->readerStream = 255u;
		bothFree = (ring->writerStream == 255u);
	}
	if (bothFree) {
		ring->readPtr = ring->storage;
		ring->commitPtr = ring->storage;
		ring->writePtr = ring->storage;
		ring->eosFlag = 0;
		ring->writeCount = 0;
		ring->readCount = 0;
		ring->writerActive = 0;
		ring->readerActive = 0;
	}
	return swDepMutexUnlock(&ring->mutex);
}

/* ---- Message queue & message manager ---- */

int swDepMsgDump(const char *prefix, const struct SwDepMsg *msg)
{
	if (msg != NULL)
		return swMwTrace("sw dep:%s%p (msgtype=%d seqno=%d mqueueid=%d len=%d buflen=%d)\n",
				   prefix, msg, msg->type, msg->seqNo, msg->queueId,
				   msg->len, msg->bufLen);
	return swMwTrace("sw dep:%s(nil)\n", prefix);
}

int swDepMsgQueueInit(struct SwDepMsgQueue *q)
{
	q->head = NULL;
	q->tail = NULL;
	return 0;
}

int swDepMsgQueueFini(struct SwDepMsgQueue *q)
{
	q->head = NULL;
	q->tail = NULL;
	return 0;
}

struct SwDepMsg *swDepMsgQueuePop(struct SwDepMsgQueue *q)
{
	struct SwDepMsg *msg = q->head;

	if (msg != NULL) {
		q->head = msg->next;
		if (q->tail == msg)
			q->tail = NULL;
		msg->next = NULL;
	}
	return msg;
}

struct SwDepMsg *swDepMsgQueuePushTail(struct SwDepMsgQueue *q, struct SwDepMsg *msg)
{
	if (q->tail != NULL)
		q->tail->next = msg;
	q->tail = msg;
	if (q->head == NULL)
		q->head = msg;
	return q->head;
}

struct SwDepMsg *swDepMsgQueuePushHead(struct SwDepMsgQueue *q, struct SwDepMsg *msg)
{
	if (q->head != NULL)
		msg->next = q->head;
	q->head = msg;
	if (q->tail == NULL)
		q->tail = msg;
	return q->tail;
}

struct SwDepMsg *swDepMsgQueuePeek(struct SwDepMsgQueue *q)
{
	return q->head;
}

unsigned int swDepMsgQueueCount(const struct SwDepMsgQueue *q)
{
	unsigned int count = 0;
	const struct SwDepMsg *cur;

	for (cur = q->head; cur != NULL; cur = cur->next)
		++count;
	return count;
}

void swDepMsgQueueDumpNoop(void)
{
}

int swDepMsgMgrInit(struct SwDepMsgMgr *mgr, struct SwDepStream *stream)
{
	memset(mgr, 0, sizeof(*mgr));
	swDepMutexCreate(&mgr->mutex);
	mgr->stream = stream;
	mgr->poolCount = 0;
	return 0;
}

int swDepMsgMgrFini(struct SwDepMsgMgr *mgr)
{
	return swDepMutexDelete(&mgr->mutex);
}

unsigned char swDepMsgMgrAddPool(struct SwDepMsgMgr *mgr, unsigned char *outId,
				 unsigned char msgType, unsigned int count,
				 unsigned char *baseBuf, unsigned int itemSize,
				 unsigned char useLock)
{
	unsigned char id = (unsigned char)mgr->poolCount;
	struct SwDepMsgPool *pool = &mgr->pools[id];

	pool->msgType = msgType;
	pool->useLock = useLock;
	pool->count = count;
	pool->baseBuf = baseBuf;
	pool->itemSize = itemSize;
	swDepMsgQueueInit(&pool->freeQueue);
	pool->onAlloc = NULL;
	pool->onAllocArg = NULL;
	pool->onFree = NULL;
	pool->onFreeArg = NULL;
	pool->nextSeqNo = 0;
	++mgr->poolCount;
	*outId = id;
	return id;
}

void swDepMsgMgrSetAllocCb(struct SwDepMsgMgr *mgr, unsigned char poolId,
			   int (*cb)(struct SwDepMsgMgr *, void *, struct SwDepMsg *), void *arg)
{
	struct SwDepMsgPool *pool = &mgr->pools[poolId];

	pool->onAlloc = cb;
	pool->onAllocArg = arg;
}

void swDepMsgMgrSetFreeCb(struct SwDepMsgMgr *mgr, unsigned char poolId,
			  void (*cb)(struct SwDepMsgMgr *, void *, struct SwDepMsg *), void *arg)
{
	struct SwDepMsgPool *pool = &mgr->pools[poolId];

	pool->onFree = cb;
	pool->onFreeArg = arg;
}

int swDepMsgMgrCommit(struct SwDepMsgMgr *mgr)
{
	unsigned int total = 0;
	unsigned int i;
	unsigned int j;
	unsigned int offset;

	for (i = 0; i < mgr->poolCount; ++i)
		total += mgr->pools[i].count;
	if (total > 41u)
		return 7;

	memset(mgr->msgs, 0, sizeof(mgr->msgs));
	offset = 0;
	for (i = 0; i < mgr->poolCount; ++i) {
		struct SwDepMsgPool *pool = &mgr->pools[i];
		struct SwDepMsg *items = &mgr->msgs[offset];
		unsigned char *buf = pool->baseBuf;

		for (j = 0; j < pool->count; ++j) {
			items[j].type = pool->msgType;
			items[j].queueId = (unsigned char)i;
			items[j].seqNo = -1;
			items[j].next = NULL;
			if (buf != NULL) {
				items[j].data = buf;
				items[j].bufLen = pool->itemSize;
				buf += pool->itemSize;
			}
		}
		if (buf != NULL && pool->count > 1u) {
			for (j = 0; j < pool->count / 2u; ++j) {
				unsigned int opp = pool->count - j - 1u;
				void *tmp = items[j].data;

				items[j].data = items[opp].data;
				items[opp].data = tmp;
			}
		}
		for (j = 0; j < pool->count; ++j)
			swDepMsgQueuePushTail(&pool->freeQueue, &items[j]);
		offset += pool->count;
	}
	return 0;
}

int swDepMsgMgrReset(struct SwDepMsgMgr *mgr)
{
	unsigned int i;

	memset(mgr->msgs, 0, sizeof(mgr->msgs));
	for (i = 0; i < mgr->poolCount; ++i)
		mgr->pools[i].wakeProcIdx = 255u;
	mgr->poolCount = 0;
	return 0;
}

void swDepMsgMgrBindWake(struct SwDepMsgMgr *mgr, unsigned char poolId, unsigned char procIdx)
{
	mgr->pools[poolId].wakeProcIdx = procIdx;
}

struct SwDepMsg *swDepMsgMgrAlloc(struct SwDepMsgMgr *mgr, unsigned char poolId)
{
	struct SwDepMsgPool *pool = &mgr->pools[poolId];
	struct SwDepMsg *msg;

	if (pool->useLock != 0)
		swDepMutexLock(&mgr->mutex);
	msg = swDepMsgQueuePop(&pool->freeQueue);
	if (pool->useLock != 0)
		swDepMutexUnlock(&mgr->mutex);
	if (msg == NULL)
		return NULL;

	msg->len = 0;
	msg->pktInfo = 0;
	msg->cci = 255u;
	msg->seqNo = (int)pool->nextSeqNo++;
	msg->flags = 0;

	if (pool->onAlloc == NULL || pool->onAlloc(mgr, pool->onAllocArg, msg) == 0)
		return msg;

	if (pool->useLock != 0)
		swDepMutexLock(&mgr->mutex);
	swDepMsgQueuePushHead(&pool->freeQueue, msg);
	if (pool->useLock != 0)
		swDepMutexUnlock(&mgr->mutex);
	return NULL;
}

int swDepMsgMgrFree(struct SwDepMsgMgr *mgr, struct SwDepMsg *msg)
{
	struct SwDepMsgPool *pool = &mgr->pools[msg->queueId];
	struct SwDepMsg *prevHead;

	if (pool->onFree != NULL)
		pool->onFree(mgr, pool->onFreeArg, msg);
	if (pool->useLock != 0)
		swDepMutexLock(&mgr->mutex);
	msg->seqNo = -1;
	prevHead = swDepMsgQueuePeek(&pool->freeQueue);
	swDepMsgQueuePushTail(&pool->freeQueue, msg);
	if (pool->useLock != 0)
		swDepMutexUnlock(&mgr->mutex);
	if (prevHead == NULL && pool->wakeProcIdx != 255u)
		mgr->stream->activeProcs[pool->wakeProcIdx]->isReady = 1;
	return pool->wakeProcIdx;
}

int swDepMsgMgrDumpPool(struct SwDepMsgMgr *mgr, const char *prefix, unsigned char poolId)
{
	(void)mgr;
	return swMwTrace("sw dep:%smqueue_id %d\n", prefix, poolId);
}

int swDepMsgMgrCheckPools(struct SwDepMsgMgr *mgr)
{
	unsigned int i;

	for (i = 0; i < mgr->poolCount; ++i)
		;
	return 1;
}

/* ---- Base processor vtable dispatch ---- */

const char *swDepProcGetName(struct SwDepProc *proc)
{
	return proc->vtable->getName(proc);
}

int swDepProcInit(struct SwDepProc *proc, struct SwDepStream *stream,
		  const struct SwDepProcVtable *vtable)
{
	memset(proc, 0, sizeof(proc->vtable));
	proc->index = 0;
	proc->stream = stream;
	proc->vtable = vtable;
	proc->nextProc = NULL;
	return proc->vtable->init(proc);
}

void swDepProcFini(struct SwDepProc *proc)
{
	proc->vtable->fini(proc);
}

int swDepProcOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	int status;

	proc->isReady = 1;
	proc->isEos = 0;
	proc->dstQueueId = dstQueueId;
	status = proc->vtable->open(proc, dstQueueId, config);
	return (status != 0) ? status : 0;
}

void swDepProcClose(struct SwDepProc *proc)
{
	proc->vtable->close(proc);
}

int swDepProcPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	proc->vtable->push(proc, msg);
	proc->isReady = 1;
	return 1;
}

int swDepProcRun(struct SwDepProc *proc)
{
	if (proc->isReady == 0)
		return 0;
	proc->isReady = 0;
	proc->vtable->run(proc);
	return 1;
}

int swDepProcCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	return proc->vtable->ctrl(proc, cmd, arg);
}

void swDepProcDump(struct SwDepProc *proc)
{
	proc->vtable->dump(proc);
}

int swDepProcPushEos(struct SwDepProc *proc)
{
	struct SwDepMsg *msg;

	msg = swDepMsgMgrAlloc(&proc->stream->msgMgr, proc->stream->eosQueueId);
	return swDepProcPush(proc, msg);
}

void swDepProcDumpHeader(struct SwDepProc *proc)
{
	swMwTrace("sw dep:------------------------------------\n");
	swMwTrace("sw dep:[proc %d] %-10s\n", proc->index, swDepProcGetName(proc));
	swMwTrace("sw dep:is_ready         = %d\n", proc->isReady);
	swMwTrace("sw dep:is_eos           = %d\n", proc->isEos);
}

int swDepProcIsReadyStub(struct SwDepProc *proc, int arg)
{
	(void)proc;
	(void)arg;
	return 1;
}

/* ---- RX buffer wrappers ---- */

int swDepGetRxBuffWrapper(void *driverCtx, unsigned char streamId, void **outBuf,
			  unsigned int *outLen)
{
	struct SwRxChannel *chan =
		(struct SwRxChannel *)((unsigned char *)driverCtx + 192u * streamId + 112u);

	return swDepGetRxBuff(chan, outBuf, outLen);
}

int swDepRxOneWrapper(void *driverCtx, unsigned char streamId, void *buf,
		      unsigned int len, int flags, int pktInfo)
{
	(void)buf;
	return swDepRxOne(driverCtx, streamId, len, flags, pktInfo);
}

int swDepGetRxBuff(struct SwRxChannel *chan, void **outBuf, unsigned int *outLen)
{
	unsigned int popIdx = chan->popIdx;
	struct SwRxDesc *desc;
	unsigned int nextIdx;

	if (popIdx == chan->readIdx) {
		chan->fullFlag = 1;
		if ((int)popIdx != g_swLastFullPopIdx && (g_swWgnLogMask & 0x10u) != 0) {
			g_swLastFullPopIdx = (int)popIdx;
			swMwTrace("sw: get_rxbuff: full pp=%d\n", popIdx);
		}
		return 6;
	}
	desc = &chan->table->descs[popIdx];
	*outBuf = desc->buf;
	*outLen = desc->len;
	nextIdx = popIdx + 1u;
	if (nextIdx == chan->table->count)
		nextIdx = 0;
	chan->popIdx = nextIdx;
	return 0;
}

int swDepRxOne(void *driverCtx, unsigned char streamId, unsigned int writtenLen,
	       int flags, int pktInfo)
{
	struct SwRxChannel *chan =
		(struct SwRxChannel *)((unsigned char *)driverCtx + 192u * streamId + 112u);
	unsigned int idx;
	struct SwRxDesc *desc;
	unsigned int descFlags;
	int notify = 0;
	void *parentCtx;

	if ((g_swWgnLogMask & 0x10u) != 0)
		swMwTrace("sw: rx_one: p=%d w=%d r=%d\n",
			    chan->popIdx, chan->writeIdx, chan->readIdx);
	idx = chan->writeIdx;
	chan->writeIdx = idx + 1u;
	if (chan->writeIdx == chan->table->count)
		chan->writeIdx = 0;
	desc = &chan->table->descs[idx];
	desc->writtenLen = writtenLen;
	descFlags = desc->flags & 0x10100000u;
	if ((desc->flags & 0x10000000u) != 0)
		notify = 1;
	if ((flags & 0x20000000) != 0) {
		descFlags |= 0x20000000u;
		notify = 1;
	}
	if ((flags & 0x08000000) != 0) {
		if (pktInfo != chan->lastPktInfo)
			descFlags |= 0x08000000u;
		notify = 1;
	}
	desc->flags = descFlags;
	if (notify != 0) {
		memcpy(&parentCtx, (unsigned char *)driverCtx + 4, sizeof(parentCtx));
		return swRxCompCallback(parentCtx, streamId, (uint32_t)pktInfo);
	}
	return 0;
}
