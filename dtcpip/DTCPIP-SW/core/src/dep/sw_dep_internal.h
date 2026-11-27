/*
 * Internal data structures and cross-file prototypes for the DTCPIP-SW "sw dep:" stream
 * pipeline . All sizes and field offsets
 * match the 345,584-byte SwDepCore allocated by swDepInitLib.
 */

#ifndef SW_DEP_INTERNAL_H
#define SW_DEP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>

struct SwDepCore;
struct SwDepStream;
struct SwDepProc;
struct SwDepMsgMgr;

struct SwDepEvent {
	SceKernelLwCondWork cond;	/* 0x00 (16 B) */
	SceKernelLwMutexWork mutex;	/* 0x10 (32 B) */
	int signaled;			/* 0x30 (4 B, struct padded to 56 B) */
};

struct SwDepThread {
	int (*entry)(void *user);	/* 0x00 */
	SceUID threadId;		/* 0x04 */
	void *user;			/* 0x08 */
};

struct SwDepWorker {
	struct SwDepThread thread;	/* 0x00 (12 B) */
	unsigned int pad;		/* 0x0C (4 B) */
	struct SwDepEvent event;	/* 0x10 (56 B) */
	unsigned char gap[32];		/* 0x48 (32 B -> 0x68) */
	int stopRequested;		/* 0x68 */
	int running;			/* 0x6C */
	int stopped;			/* 0x70 */
	int (*hasWork)(void *ctx);	/* 0x74 */
	int (*isDone)(void *ctx);	/* 0x78 */
	void (*process)(void *ctx);	/* 0x7C */
	void (*onExit)(void *ctx);	/* 0x80 */
	void *ctx;			/* 0x84 (total 0x88 = 136 B) */
};

struct SwDepMsg {
	unsigned char type;		/* 0x00: 1=hdr, 2..6=data, 7=eos */
	unsigned char queueId;		/* 0x01 */
	unsigned char cci;		/* 0x02 */
	unsigned char reservedType;	/* 0x03 */
	void *data;			/* 0x04 */
	unsigned int bufLen;		/* 0x08 */
	unsigned int len;		/* 0x0C */
	unsigned int pktInfo;		/* 0x10 */
	unsigned int flags;		/* 0x14 */
	int seqNo;			/* 0x18 */
	struct SwDepMsg *next;		/* 0x1C (total 32 B) */
};

struct SwDepMsgQueue {
	struct SwDepMsg *head;		/* 0x00 */
	struct SwDepMsg *tail;		/* 0x04 */
};

struct SwDepMsgPool {
	unsigned char msgType;		/* 0x00 */
	unsigned char reservedByte;	/* 0x01 */
	unsigned char useLock;		/* 0x02 */
	unsigned char wakeProcIdx;	/* 0x03 */
	unsigned int count;		/* 0x04 */
	unsigned char *baseBuf;		/* 0x08 */
	unsigned int itemSize;		/* 0x0C */
	struct SwDepMsgQueue freeQueue;	/* 0x10 (8 B) */
	int (*onAlloc)(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg); /* 0x18 */
	void *onAllocArg;		/* 0x1C */
	void (*onFree)(struct SwDepMsgMgr *mgr, void *arg, struct SwDepMsg *msg); /* 0x20 */
	void *onFreeArg;		/* 0x24 */
	unsigned int nextSeqNo;		/* 0x28 (total 44 B) */
};

struct SwDepMsgMgr {
	struct SwDepStream *stream;	/* 0x000 */
	unsigned int reservedHead;	/* 0x004 */
	SceKernelLwMutexWork mutex;	/* 0x008 (32 B) */
	struct SwDepMsg msgs[41];	/* 0x028 (1312 B) */
	struct SwDepMsgPool pools[10];	/* 0x548 (440 B) */
	unsigned int poolCount;		/* 0x700 */
	unsigned int reservedTail;	/* 0x704 (total 1800 B = 0x708) */
};

struct SwDepProcVtable {
	const char *(*getName)(struct SwDepProc *proc);
	int (*init)(struct SwDepProc *proc);
	void (*fini)(struct SwDepProc *proc);
	int (*open)(struct SwDepProc *proc, unsigned char dstQueueId, void *config);
	void (*close)(struct SwDepProc *proc);
	void (*push)(struct SwDepProc *proc, struct SwDepMsg *msg);
	void (*run)(struct SwDepProc *proc);
	int (*ctrl)(struct SwDepProc *proc, int cmd, void *arg);
	void (*dump)(struct SwDepProc *proc);
};

struct SwDepProc {
	const struct SwDepProcVtable *vtable;	/* 0x00 */
	struct SwDepStream *stream;		/* 0x04 */
	unsigned char dstQueueId;		/* 0x08 */
	unsigned char reservedAlign[3];		/* 0x09 */
	unsigned int index;			/* 0x0C */
	int isReady;				/* 0x10 */
	int isEos;				/* 0x14 */
	struct SwDepProc *nextProc;		/* 0x18 (total 28 B) */
};

/* AES crypto engine request ring entry (48 B) */
struct SwDepAesReq {
	unsigned char *src;		/* 0x00 */
	unsigned char *dst;		/* 0x04 */
	unsigned int len;		/* 0x08 */
	unsigned char padding;		/* 0x0C */
	unsigned char reservedAlign[3];	/* 0x0D */
	unsigned char tmpSrcBuf[16];	/* 0x10 */
	unsigned char tmpDstBuf[16];	/* 0x20 (total 48 B) */
};

struct SwDepAesReqRing {
	struct SwDepAesReq items[16];	/* 0x000 (768 B) */
	struct SwDepAesReq *head;	/* 0x300 */
	struct SwDepAesReq *tail;	/* 0x304 */
	struct SwDepAesReq *end;	/* 0x308 */
	unsigned int count;		/* 0x30C (total 784 B = 0x310) */
};

struct SwDepAesAddReq {
	struct SwDepAesProc *owner;	/* 0x00 */
	unsigned char cci;		/* 0x04 */
	unsigned char reservedAlign[3];	/* 0x05 */
	int isEosMsg;			/* 0x08 */
	int isEos;			/* 0x0C */
	int isHdrActive;		/* 0x10 */
	unsigned char hdrBuf[64];	/* 0x14 */
	unsigned char *hdrSrc;		/* 0x54 */
	unsigned int hdrSrcLen;		/* 0x58 */
	struct SwDepAesReq *curReq;	/* 0x5C */
	struct SwDepMsg *srcMsg;	/* 0x60 */
	unsigned char *src;		/* 0x64 */
	unsigned int srcLen;		/* 0x68 */
	struct SwDepMsg *dstMsg;	/* 0x6C */
	unsigned char *dst;		/* 0x70 */
	unsigned int dstLen;		/* 0x74 */
	struct SwDepMsg *nextDstMsg;	/* 0x78 */
	unsigned char *tmpSrc;		/* 0x7C */
	unsigned int tmpSrcLen;		/* 0x80 */
	unsigned char *tmpDst;		/* 0x84 */
	unsigned int tmpDstLen;		/* 0x88 */
	unsigned int maxLen;		/* 0x8C */
	unsigned int srcLimit;		/* 0x90 */
	unsigned int dstLimit;		/* 0x94 */
	unsigned int chunkLen;		/* 0x98 (total 156 B = 0x9C) */
};

struct SwDepAesEndReq {
	struct SwDepAesProc *owner;	/* 0x00 */
	struct SwDepMsg *srcMsg;	/* 0x04 */
	unsigned int srcLen;		/* 0x08 */
	struct SwDepMsg *dstMsg;	/* 0x0C */
	unsigned char *dst;		/* 0x10 */
	unsigned int dstLen;		/* 0x14 */
	unsigned int totalOut;		/* 0x18 */
	struct SwDepAesReq *req;	/* 0x1C */
	unsigned char *reqDst;		/* 0x20 */
	unsigned int reqLen;		/* 0x24 */
	unsigned char reqPadding;	/* 0x28 */
	unsigned char reservedAlign[3];	/* 0x29 */
	int toTmpDst;			/* 0x2C */
	int markFirst;			/* 0x30 (total 52 B = 0x34) */
};

struct SwDepAesHw {
	int isEncrypt;			/* 0x00 */
	unsigned char enabled;		/* 0x04 */
	unsigned char key[16];		/* 0x05 */
	unsigned char baseIv[16];	/* 0x15 */
	unsigned char curIv[16];	/* 0x25 */
	unsigned char reservedAlign[3];	/* 0x35 */
	int (*notify)(struct SwDepAesHw *hw, int count, void *ctx); /* 0x38 */
	void *notifyCtx;		/* 0x3C (total 64 B = 0x40) */
};

struct SwDepAesProc {
	int state;			/* 0x000 */
	int isEncrypt;			/* 0x004 */
	int isPcp;			/* 0x008 */
	struct SwDepProc *proc;		/* 0x00C */
	SceKernelLwMutexWork mutex;	/* 0x010 (32 B) */
	struct SwDepMsgQueue srcQueue;	/* 0x030 */
	struct SwDepMsgQueue reqSrcQ;	/* 0x038 */
	struct SwDepMsgQueue reqDstQ;	/* 0x040 */
	unsigned char dstQueueId;	/* 0x048 */
	unsigned char reservedAlign[3];	/* 0x049 */
	struct SwDepAesReqRing ring;	/* 0x04C (784 B -> 0x35C) */
	struct SwDepAesAddReq addReq;	/* 0x35C (156 B -> 0x3F8) */
	struct SwDepAesEndReq endReq;	/* 0x3F8 (52 B -> 0x42C) */
	struct SwDepAesHw hw;		/* 0x42C (64 B -> 0x46C) */
	unsigned char hdrInfo[56];	/* 0x46C (56 B -> 0x4A4) */
	unsigned int reqCount;		/* 0x4A4 */
	unsigned int notifyCount;	/* 0x4A8 */
	unsigned int reservedTail;	/* 0x4AC (total 1200 B = 0x4B0) */
};

struct SwDepRingBuf {
	unsigned char id;		/* 0x0000 */
	unsigned char reservedHead[7];	/* 0x0001 */
	SceKernelLwMutexWork mutex;	/* 0x0008 (32 B) */
	unsigned int reservedMid;	/* 0x0028 */
	unsigned char writerStream;	/* 0x002C */
	unsigned char readerStream;	/* 0x002D */
	unsigned char reservedAlign[2];	/* 0x002E */
	int writerActive;		/* 0x0030 */
	int readerActive;		/* 0x0034 */
	int eosFlag;			/* 0x0038 */
	unsigned char storage[24576];	/* 0x003C (24576 B -> 0x603C) */
	unsigned int capacity;		/* 0x603C (36096) */
	unsigned int writeCount;	/* 0x6040 */
	unsigned int readCount;		/* 0x6044 */
	unsigned char *readPtr;		/* 0x6048 */
	unsigned char *commitPtr;	/* 0x604C */
	unsigned char *writePtr;	/* 0x6050 */
	unsigned char *endPtr;		/* 0x6054 (total 24664 B = 0x6058) */
};

struct SwDepStream {
	unsigned char streamId;		/* 0x0000 */
	unsigned char inQueueId;	/* 0x0001 */
	unsigned char outQueueId;	/* 0x0002 */
	unsigned char ringInQueueId;	/* 0x0003 */
	unsigned char ringOutQueueId;	/* 0x0004 */
	unsigned char aesDecQueueId;	/* 0x0005 */
	unsigned char aesEncQueueId;	/* 0x0006 */
	unsigned char tsConvQueueId;	/* 0x0007 */
	unsigned char aesEnc2QueueId;	/* 0x0008 */
	unsigned char aesEnc3QueueId;	/* 0x0009 */
	unsigned char eosQueueId;	/* 0x000A */
	unsigned char hdrQueueId;	/* 0x000B */
	struct SwDepCore *core;		/* 0x000C */
	SceKernelLwMutexWork mutex;	/* 0x0010 (32 B) */
	int state;			/* 0x0030 */
	unsigned int rxProcIdx;		/* 0x0034 */
	struct SwDepMsgMgr msgMgr;	/* 0x0038 (1800 B -> 0x0740) */
	unsigned char procInput[36];		/* 0x0740 (1856) */
	unsigned char procRingSrc[36];		/* 0x0764 (1892) */
	unsigned char procAesDecHdr[1232];	/* 0x0788 (1928) */
	unsigned char procAesEnc1[1232];	/* 0x0C58 (3160) */
	unsigned char procTsConv[504];		/* 0x1128 (4392) */
	unsigned char procOptParseTx[2348];	/* 0x1320 (4896) */
	unsigned char procOptParseRx[2356];	/* 0x1C4C (7244) */
	unsigned char procCciTag[40];		/* 0x2580 (9600) */
	unsigned char procAesEncKey[1264];	/* 0x25A8 (9640) */
	unsigned char procAesEnc2[1232];	/* 0x2A98 (10904) */
	unsigned char procOutputDma[36];	/* 0x2F68 (12136) */
	unsigned char procRingSink[60];		/* 0x2F8C (12172) */
	struct SwDepProc *activeProcs[16];	/* 0x2FC8 (12232) */
	unsigned int activeProcCount;		/* 0x3008 (12296) */
	unsigned int reservedAlign;		/* 0x300C (12300) */
	struct SwDepWorker worker;		/* 0x3010 (12304, 136 B) */
	int workerStarted;			/* 0x3098 (12440) */
	unsigned char config[0x474];		/* 0x309C (12444, 1140 B) */
	unsigned char workPool[307312];		/* 0x3510 (13584 -> 0x4E580) */
	unsigned int stat0;			/* 0x4E580 */
	unsigned int stat1;			/* 0x4E584 (total 320904 B = 0x4E588) */
};

struct SwDepCore {
	void *driverCtx;			/* 0x00000 */
	unsigned int reservedAlign;		/* 0x00004 */
	struct SwDepStream streams[1];		/* 0x00008 (320904 B -> 0x4E590) */
	struct SwDepRingBuf ringBufs[1];	/* 0x4E590 (24664 B -> 0x545E8) */
	unsigned char chanMap[8];		/* 0x545E8 (8 B -> 0x545F0) */
};

/* OSAL / crypto helpers from sw_dep_vita.c */
int swDepWorkerEventInit(struct SwDepWorker *worker);
int swDepWorkerEventFini(struct SwDepWorker *worker);
int swDepWorkerStart(struct SwDepWorker *worker, const char *unusedName,
		     int (*hasWork)(void *), int (*isDone)(void *),
		     void (*process)(void *), void (*onExit)(void *), void *ctx);
int swDepWorkerStop(struct SwDepWorker *worker);
int swDepWorkerWake(struct SwDepWorker *worker);
int swDepMutexCreate(SceKernelLwMutexWork *mutex);
int swDepMutexDelete(SceKernelLwMutexWork *mutex);
int swDepMutexLock(SceKernelLwMutexWork *mutex);
int swDepMutexUnlock(SceKernelLwMutexWork *mutex);
int swDepAes128CbcEncrypt(const unsigned char *src, unsigned char *dst, unsigned int len,
			  const void *key, const void *iv);
int swDepAes128CbcDecrypt(const unsigned char *src, unsigned char *dst, unsigned int len,
			  const void *key, void *iv);
int swMwTrace(const char *format, ...);

/* Wagner driver hooks */
int swBaseInit(void *depCore, void **outInfo);
void swBaseExit(void *driverCtx);
int swSetStreamStatusWrap(void *driverCtx, unsigned char streamId, uint32_t errorBits);
void swStopCompCallback(void *driverCtx, unsigned char streamId);
void swSetCciExt(void *driverCtx, unsigned char streamId, const unsigned char *cciDesc,
		 unsigned char cci, unsigned char prevCci);
void swGetEncParamWrap(void *driverCtx, int streamId, int emi,
		       int forceNext, void *outKeyInfo, void *outPcpHeader);
void swTxCompEvent(void *driverCtx, unsigned char streamId, int count);
void swTxAvailEvent(void *driverCtx, unsigned char streamId);
int swRxCompCallback(void *driverCtx, unsigned char streamId, uint32_t pktInfo);

/* sw_opt_parser.c hooks */
int swOptSectionInit(void *section);
void swOptSectionFini(void *section);
void swOptSectionReset(void *section);
int swOptParseResultInit(void *parser);
void swOptParseResultFini(void *parser);
int sw_opt_section_put(void *section, const unsigned char *packet, unsigned int packetLen,
		       int rewriteFlag, int *outComplete, unsigned char **outData,
		       unsigned int *outLen);
unsigned char swOptCciToEemi(unsigned char cci, unsigned char epn);
int swOptCheckEemiCompatibility(unsigned char oldEemi, unsigned char newEemi);
int sw_opt_parser_parsepat(void *parser, const unsigned char *section, unsigned int sectionLen);
int sw_opt_parser_parsepmt(void *parser, unsigned char *section, unsigned int sectionLen);

/* sw_dep_core.c exports used by sw_dep_procs.c */
int swDepStreamWake(struct SwDepStream *stream);
int swDepStreamSetDone(struct SwDepStream *stream);
int swDepStreamNotifyError(struct SwDepStream *stream, int errorBits);
void swDepStreamGetKey(struct SwDepStream *stream, unsigned char emi, int forceNext,
		       void *outKeyInfo, void *outPcpHeader);
int swDepStreamNotifyCci(struct SwDepStream *stream, void *cciDesc, unsigned char cci,
			 unsigned char prevCci);
void swDepStreamNoop(void);
int swDepStreamRxOne(struct SwDepStream *stream, void *buf, unsigned int len,
		     int flags, int pktInfo);
int swDepStreamAcquireRing(struct SwDepStream *stream, unsigned int mode, int isWriter,
			   struct SwDepRingBuf **outRing);
int swDepStreamReleaseRing(struct SwDepStream *stream, int isWriter,
			   struct SwDepRingBuf *ring);

/* ring buffer operations (sw_dep_core.c) */
int swDepRingPeekRead(struct SwDepRingBuf *ring, void **outPtr, unsigned int *outLen,
		      int *outEos);
int swDepRingConsume(struct SwDepRingBuf *ring, unsigned int len);
unsigned int swDepRingPeekWrite(struct SwDepRingBuf *ring, void **outPtr,
				unsigned int *outLen);
int swDepRingCommit(struct SwDepRingBuf *ring, unsigned int len);
void swDepRingSetEos(struct SwDepRingBuf *ring);

/* message queue & manager operations (sw_dep_core.c) */
int swDepMsgQueueInit(struct SwDepMsgQueue *q);
int swDepMsgQueueFini(struct SwDepMsgQueue *q);
struct SwDepMsg *swDepMsgQueuePop(struct SwDepMsgQueue *q);
struct SwDepMsg *swDepMsgQueuePushTail(struct SwDepMsgQueue *q, struct SwDepMsg *msg);
struct SwDepMsg *swDepMsgQueuePushHead(struct SwDepMsgQueue *q, struct SwDepMsg *msg);
struct SwDepMsg *swDepMsgQueuePeek(struct SwDepMsgQueue *q);
struct SwDepMsg *swDepMsgMgrAlloc(struct SwDepMsgMgr *mgr, unsigned char poolId);
int swDepMsgMgrFree(struct SwDepMsgMgr *mgr, struct SwDepMsg *msg);

/* processor dispatch (sw_dep_core.c) */
const char *swDepProcGetName(struct SwDepProc *proc);
int swDepProcInit(struct SwDepProc *proc, struct SwDepStream *stream,
		  const struct SwDepProcVtable *vtable);
void swDepProcFini(struct SwDepProc *proc);
int swDepProcOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config);
void swDepProcClose(struct SwDepProc *proc);
int swDepProcPush(struct SwDepProc *proc, struct SwDepMsg *msg);
int swDepProcRun(struct SwDepProc *proc);
int swDepProcCtrl(struct SwDepProc *proc, int cmd, void *arg);
void swDepProcDump(struct SwDepProc *proc);
int swDepProcPushEos(struct SwDepProc *proc);
void swDepProcDumpHeader(struct SwDepProc *proc);
int swDepProcIsReadyStub(struct SwDepProc *proc, int arg);

/* processor vtable getters (sw_dep_procs.c) */
const struct SwDepProcVtable *swDepProcAesEnc1Vtable(void);
const struct SwDepProcVtable *swDepProcAesEnc2Vtable(void);
const struct SwDepProcVtable *swDepProcAesDecHdrVtable(void);
const struct SwDepProcVtable *swDepProcAesEncKeyVtable(void);
const struct SwDepProcVtable *swDepProcOptParseRxVtable(void);
const struct SwDepProcVtable *swDepProcOptParseTxVtable(void);
const struct SwDepProcVtable *swDepProcCciTagVtable(void);
const struct SwDepProcVtable *swDepProcInputVtable(void);
const struct SwDepProcVtable *swDepProcOutputDmaVtable(void);
const struct SwDepProcVtable *swDepProcRingSrcVtable(void);
const struct SwDepProcVtable *swDepProcRingSinkVtable(void);
const struct SwDepProcVtable *swDepProcTsConvVtable(void);

/* RX ring helpers (sw_dep_core.c) */
int swDepGetRxBuffWrapper(void *driverCtx, unsigned char streamId, void **outBuf,
			  unsigned int *outLen);
int swDepRxOneWrapper(void *driverCtx, unsigned char streamId, void *buf,
		      unsigned int len, int flags, int pktInfo);

#endif /* SW_DEP_INTERNAL_H */
