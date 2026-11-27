/*
 * DTCPIP-SW "sw dep:" stream processors and AES crypto state machine
 *: the 12 pipeline processors (4 AES stages, 2 TS/PMT opt-parsers,
 * CCI tagger, input queue, output DMA sink, ringbuf source/sink, TS 188<->192 converter)
 * and the shared SwDepAesProc addreq/endreq engine.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include "sw_dep_internal.h"

/* ---- Processor-specific layouts (all start with struct SwDepProc base, 28 B) ---- */

struct SwDepProcAesBase {
	struct SwDepProc base;		/* 0x000 (28 B) */
	unsigned int reservedAlign;	/* 0x01C (4 B) */
	struct SwDepAesProc engine;	/* 0x020 (1200 B -> 0x4D0 = 1232 B) */
};

struct SwDepProcAesKey {
	struct SwDepProc base;		/* 0x000 (28 B) */
	unsigned int reservedAlign;	/* 0x01C (4 B) */
	struct SwDepAesProc engine;	/* 0x020 (1200 B -> 0x4D0) */
	unsigned char curEmi;		/* 0x4D0 (1232) */
	unsigned char pcpHeader[14];	/* 0x4D1 (1233) */
	unsigned char reservedHdr;	/* 0x4DF */
	int keyState;			/* 0x4E0 (1248) */
	unsigned int bytesEncrypted;	/* 0x4E4 (1252) */
	unsigned int pcpPayloadSize;	/* 0x4E8 (1256) */
	unsigned int reservedTail;	/* 0x4EC (total 1264 B) */
};

struct SwDepProcOptRx {
	struct SwDepProc base;		/* 0x000 (28 B) */
	struct SwDepMsgQueue queue;	/* 0x01C (28) */
	unsigned int pktSize;		/* 0x024 (36) */
	unsigned int tsOffset;		/* 0x028 (40) */
	unsigned char pktBuf[192];	/* 0x02C (44) */
	unsigned int partialLen;	/* 0x0EC (236) */
	struct SwDepMsg *heldMsg;	/* 0x0F0 (240) */
	unsigned short pmtPid;		/* 0x0F4 (244) */
	unsigned short reservedPid;	/* 0x0F6 */
	unsigned char patSec[1036];	/* 0x0F8 (248) */
	unsigned char pmtSec[1036];	/* 0x504 (1284) */
	unsigned char parser[20];	/* 0x910 (2320) */
	unsigned char curEmi;		/* 0x924 (2340) */
	unsigned char pmtEmi;		/* 0x925 (2341) */
	unsigned char reservedEmi[2];	/* 0x926 */
	int sawPmt;			/* 0x928 (2344) */
	int emiMatched;			/* 0x92C (2348) */
	unsigned int mismatchCount;	/* 0x930 (2352, total 2356 B) */
};

struct SwDepProcOptTx {
	struct SwDepProc base;		/* 0x000 (28 B) */
	struct SwDepMsgQueue queue;	/* 0x01C (28) */
	unsigned int pktSize;		/* 0x024 (36) */
	unsigned int tsOffset;		/* 0x028 (40) */
	unsigned char pktBuf[192];	/* 0x02C (44) */
	unsigned int partialLen;	/* 0x0EC (236) */
	struct SwDepMsg *heldMsg;	/* 0x0F0 (240) */
	int rewriteFlag;		/* 0x0F4 (244) */
	unsigned short pmtPid;		/* 0x0F8 (248) */
	unsigned short reservedPid;	/* 0x0FA */
	unsigned char patSec[1036];	/* 0x0FC (252) */
	unsigned char pmtSec[1036];	/* 0x508 (1288) */
	unsigned char parser[20];	/* 0x914 (2324) */
	unsigned char curEmi;		/* 0x928 (2344) */
	unsigned char outEmi;		/* 0x929 (2345) */
	unsigned char reservedEmi[2];	/* 0x92A (total 2348 B) */
};

struct SwDepProcCciTag {
	struct SwDepProc base;		/* 0x00 (28 B) */
	struct SwDepMsgQueue queue;	/* 0x1C (28) */
	unsigned char cci;		/* 0x24 (36) */
	unsigned char reservedAlign[3];	/* 0x25 (total 40 B) */
};

struct SwDepProcInput {
	struct SwDepProc base;		/* 0x00 (28 B) */
	struct SwDepMsgQueue queue;	/* 0x1C (28, total 36 B) */
};

struct SwDepProcOutputDma {
	struct SwDepProc base;		/* 0x00 (28 B) */
	int isFirstChunk;		/* 0x1C (28) */
	int emitBoundary;		/* 0x20 (32, total 36 B) */
};

struct SwDepProcRingSrc {
	struct SwDepProc base;		/* 0x00 (28 B) */
	unsigned char poolId;		/* 0x1C (28) */
	unsigned char reservedAlign[3];	/* 0x1D */
	struct SwDepRingBuf *ring;	/* 0x20 (32, total 36 B) */
};

struct SwDepProcRingSink {
	struct SwDepProc base;		/* 0x00 (28 B) */
	struct SwDepRingBuf *ring;	/* 0x1C (28) */
	struct SwDepMsgQueue queue;	/* 0x20 (32) */
	struct SwDepMsg *curMsg;	/* 0x28 (40) */
	unsigned char *curPtr;		/* 0x2C (44) */
	unsigned int curRem;		/* 0x30 (48) */
	unsigned char curCci;		/* 0x34 (52) */
	unsigned char reservedAlign[3];	/* 0x35 */
	int eosPending;			/* 0x38 (56, total 60 B) */
};

struct SwDepProcTsConv {
	struct SwDepProc base;		/* 0x000 (28 B) */
	struct SwDepMsgQueue queue;	/* 0x01C (28) */
	unsigned char dstPoolId;	/* 0x024 (36) */
	unsigned char reservedAlign[3];	/* 0x025 */
	int mode;			/* 0x028 (40): 0=188->188, 1=192->192, 2=192->188, 3=188->192 */
	unsigned int inPktSize;		/* 0x02C (44) */
	unsigned int inTsOff;		/* 0x030 (48) */
	unsigned int outPktSize;	/* 0x034 (52) */
	unsigned int outTsOff;		/* 0x038 (56) */
	int enableSync;			/* 0x03C (60) */
	struct SwDepMsg *inMsg;		/* 0x040 (64) */
	unsigned char *inPtr;		/* 0x044 (68) */
	unsigned int inRem;		/* 0x048 (72) */
	unsigned char inCci;		/* 0x04C (76) */
	unsigned char eosSeen;		/* 0x04D (77) */
	unsigned char reservedEos[2];	/* 0x04E */
	unsigned char inScratch[192];	/* 0x050 (80) */
	unsigned char *inScratchPtr;	/* 0x110 (272) */
	unsigned int inScratchLen;	/* 0x114 (276) */
	struct SwDepMsg *outMsg;	/* 0x118 (280) */
	unsigned char *outPtr;		/* 0x11C (284) */
	unsigned int outRem;		/* 0x120 (288) */
	unsigned char outScratch[192];	/* 0x124 (292) */
	unsigned char *outScratchPtr;	/* 0x1E4 (484) */
	unsigned int outScratchLen;	/* 0x1E8 (488) */
	int unsynced;			/* 0x1EC (492) */
	unsigned int syncHits;		/* 0x1F0 (496) */
	int syncSkip;			/* 0x1F4 (500, total 504 B) */
};

struct SwDepKeyInfoOut {
	int enableEncrypt;		/* 0x00 */
	unsigned char key[16];		/* 0x04 */
	unsigned char iv[16];		/* 0x14 */
	unsigned int pcpPayloadSize;	/* 0x24 */
};

/* ---- Forward declarations of shared SwDepAesProc helpers ---- */

static void swDepAesEngineInit(struct SwDepAesProc *eng, struct SwDepProc *proc,
			       int isEncrypt, int isPcp);
static int swDepAesEngineFini(struct SwDepAesProc *eng);
static int swDepAesEngineOpen(struct SwDepAesProc *eng, unsigned char dstQueueId, void *config);
static void swDepAesEngineSetKey(struct SwDepAesProc *eng, unsigned char enabled,
				 const void *key, const void *iv);
static void swDepAesEngineSetLen(struct SwDepAesProc *eng, unsigned int len);
static void swDepAesEngineResetChunk(struct SwDepAesProc *eng);
static unsigned char swDepAesEngineGetCci(const struct SwDepAesProc *eng);
static void swDepAesEngineSetCci(struct SwDepAesProc *eng, unsigned char cci);
static void swDepAesEngineResetIv(struct SwDepAesProc *eng);
static void swDepAesEnginePush(struct SwDepAesProc *eng, struct SwDepMsg *msg);
static unsigned int swDepAesEngineLastOutLen(const struct SwDepAesProc *eng);
static int swDepAesEngineState(const struct SwDepAesProc *eng);
static void swDepAesEngineStepIdle(struct SwDepAesProc *eng, int *cont);
static void swDepAesEngineStepWaitHdr(struct SwDepAesProc *eng, int *cont);
static void swDepAesEngineStepStart(struct SwDepAesProc *eng, const void *hdr, size_t hdrLen,
				    int *cont);
static void swDepAesEngineStepRun(struct SwDepAesProc *eng, int *cont);
static void swDepAesEngineStepFlush(struct SwDepAesProc *eng, int flushDst, int *cont);
static void swDepAesEngineStepEos(struct SwDepAesProc *eng, int *cont);
static void swDepAesEngineDump(struct SwDepAesProc *eng);

/* ---- 1. AES Enc 1 processor ---- */

static const char *swDepProcAesEnc1Name(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcAesEnc1Init(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineInit(&self->engine, proc, 0, 0);
	return 0;
}

static void swDepProcAesEnc1Fini(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineFini(&self->engine);
}

static int swDepProcAesEnc1Open(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	const unsigned char *cfg = (const unsigned char *)config;
	unsigned int enableFlag;
	unsigned int chunkLen;
	int status;

	status = swDepAesEngineOpen(&self->engine, dstQueueId, config);
	if (status != 0)
		return status;
	memcpy(&enableFlag, cfg + 36, sizeof(enableFlag));
	memcpy(&chunkLen, cfg + 72, sizeof(chunkLen));
	swDepAesEngineSetKey(&self->engine, enableFlag == 1u ? 1u : 0u, cfg + 40, cfg + 56);
	swDepAesEngineSetLen(&self->engine, chunkLen);
	return 0;
}

static void swDepProcAesEnc1Close(struct SwDepProc *proc)
{
	(void)proc;
}

static void swDepProcAesEnc1Push(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEnginePush(&self->engine, msg);
}

static void swDepProcAesEnc1Run(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	int cont = 1;

	do {
		switch (swDepAesEngineState(&self->engine)) {
		case 0:
			swDepAesEngineStepIdle(&self->engine, &cont);
			break;
		case 1:
			swDepAesEngineStepWaitHdr(&self->engine, &cont);
			break;
		case 2:
			swDepAesEngineStepStart(&self->engine, NULL, 0, &cont);
			break;
		case 3:
			swDepAesEngineStepRun(&self->engine, &cont);
			break;
		case 4:
			swDepAesEngineStepFlush(&self->engine, 0, &cont);
			swDepAesEngineResetIv(&self->engine);
			break;
		case 5:
			swDepAesEngineStepEos(&self->engine, &cont);
			break;
		default:
			cont = 0;
			break;
		}
	} while (cont != 0);
}

static int swDepProcAesEnc1Ctrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcAesEnc1Dump(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepProcDumpHeader(proc);
	swDepAesEngineDump(&self->engine);
	swLogPrintf("sw dep:<msgmgr mqueueid_dst>\n");
}

static const struct SwDepProcVtable g_swDepProcAesEnc1Vtable = {
	swDepProcAesEnc1Name, swDepProcAesEnc1Init, swDepProcAesEnc1Fini,
	swDepProcAesEnc1Open, swDepProcAesEnc1Close, swDepProcAesEnc1Push,
	swDepProcAesEnc1Run, swDepProcAesEnc1Ctrl, swDepProcAesEnc1Dump,
};

const struct SwDepProcVtable *swDepProcAesEnc1Vtable(void)
{
	return &g_swDepProcAesEnc1Vtable;
}

/* ---- 2. AES Enc 2 processor ---- */

static const char *swDepProcAesEnc2Name(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcAesEnc2Init(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineInit(&self->engine, proc, 1, 0);
	return 0;
}

static void swDepProcAesEnc2Fini(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineFini(&self->engine);
}

static int swDepProcAesEnc2Open(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	const unsigned char *cfg = (const unsigned char *)config;
	unsigned int enableFlag;
	unsigned int chunkLen;
	int status;

	status = swDepAesEngineOpen(&self->engine, dstQueueId, config);
	if (status != 0)
		return status;
	memcpy(&enableFlag, cfg + 76, sizeof(enableFlag));
	memcpy(&chunkLen, cfg + 112, sizeof(chunkLen));
	swDepAesEngineSetKey(&self->engine, enableFlag == 1u ? 1u : 0u, cfg + 80, cfg + 96);
	swDepAesEngineSetLen(&self->engine, chunkLen);
	return 0;
}

static void swDepProcAesEnc2Close(struct SwDepProc *proc)
{
	(void)proc;
}

static void swDepProcAesEnc2Push(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEnginePush(&self->engine, msg);
}

static void swDepProcAesEnc2Run(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	int cont = 1;

	do {
		switch (swDepAesEngineState(&self->engine)) {
		case 0:
			swDepAesEngineStepIdle(&self->engine, &cont);
			break;
		case 1:
			swDepAesEngineStepWaitHdr(&self->engine, &cont);
			break;
		case 2:
			swDepAesEngineStepStart(&self->engine, NULL, 0, &cont);
			break;
		case 3:
			swDepAesEngineStepRun(&self->engine, &cont);
			break;
		case 4:
			swDepAesEngineStepFlush(&self->engine, 0, &cont);
			swDepAesEngineResetIv(&self->engine);
			break;
		case 5:
			swDepAesEngineStepEos(&self->engine, &cont);
			break;
		default:
			cont = 0;
			break;
		}
	} while (cont != 0);
}

static int swDepProcAesEnc2Ctrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcAesEnc2Dump(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepProcDumpHeader(proc);
	swDepAesEngineDump(&self->engine);
	swLogPrintf("sw dep:<msgmgr mqueueid_dst>\n");
}

static const struct SwDepProcVtable g_swDepProcAesEnc2Vtable = {
	swDepProcAesEnc2Name, swDepProcAesEnc2Init, swDepProcAesEnc2Fini,
	swDepProcAesEnc2Open, swDepProcAesEnc2Close, swDepProcAesEnc2Push,
	swDepProcAesEnc2Run, swDepProcAesEnc2Ctrl, swDepProcAesEnc2Dump,
};

const struct SwDepProcVtable *swDepProcAesEnc2Vtable(void)
{
	return &g_swDepProcAesEnc2Vtable;
}

/* ---- 3. AES Dec + PCP header processor ---- */

static const char *swDepProcAesDecHdrName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcAesDecHdrInit(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineInit(&self->engine, proc, 0, 1);
	return 0;
}

static void swDepProcAesDecHdrFini(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEngineFini(&self->engine);
}

static int swDepProcAesDecHdrOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	int status;

	status = swDepAesEngineOpen(&self->engine, dstQueueId, config);
	return (status != 0) ? status : 0;
}

static void swDepProcAesDecHdrClose(struct SwDepProc *proc)
{
	(void)proc;
}

static void swDepProcAesDecHdrPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepAesEnginePush(&self->engine, msg);
}

static void swDepProcAesDecHdrPrepare(struct SwDepProcAesBase *self)
{
	const unsigned char *hdr = self->engine.hdrInfo;
	unsigned int enableFlag;
	unsigned int payloadLen;
	unsigned char emi;

	payloadLen = ((unsigned int)hdr[50] << 24) | ((unsigned int)hdr[51] << 16) |
		     ((unsigned int)hdr[52] << 8) | (unsigned int)hdr[53];
	emi = hdr[40] & 0x0Fu;
	memcpy(&enableFlag, hdr, sizeof(enableFlag));
	swDepAesEngineSetKey(&self->engine, enableFlag == 1u ? 1u : 0u, hdr + 4, hdr + 20);
	swDepAesEngineSetLen(&self->engine, payloadLen);
	swDepAesEngineSetCci(&self->engine, emi);
}

static void swDepProcAesDecHdrRun(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;
	int cont = 1;

	do {
		switch (swDepAesEngineState(&self->engine)) {
		case 0:
			swDepAesEngineStepIdle(&self->engine, &cont);
			break;
		case 1:
			swDepAesEngineStepWaitHdr(&self->engine, &cont);
			break;
		case 2:
			swDepProcAesDecHdrPrepare(self);
			swDepAesEngineStepStart(&self->engine, NULL, 0, &cont);
			break;
		case 3:
			swDepAesEngineStepRun(&self->engine, &cont);
			break;
		case 4:
			swDepAesEngineStepFlush(&self->engine, 0, &cont);
			swDepAesEngineResetChunk(&self->engine);
			break;
		case 5:
			swDepAesEngineStepEos(&self->engine, &cont);
			break;
		default:
			cont = 0;
			break;
		}
	} while (cont != 0);
}

static int swDepProcAesDecHdrCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcAesDecHdrDump(struct SwDepProc *proc)
{
	struct SwDepProcAesBase *self = (struct SwDepProcAesBase *)proc;

	swDepProcDumpHeader(proc);
	swDepAesEngineDump(&self->engine);
	swLogPrintf("sw dep:<msgmgr mqueueid_dst>\n");
}

static const struct SwDepProcVtable g_swDepProcAesDecHdrVtable = {
	swDepProcAesDecHdrName, swDepProcAesDecHdrInit, swDepProcAesDecHdrFini,
	swDepProcAesDecHdrOpen, swDepProcAesDecHdrClose, swDepProcAesDecHdrPush,
	swDepProcAesDecHdrRun, swDepProcAesDecHdrCtrl, swDepProcAesDecHdrDump,
};

const struct SwDepProcVtable *swDepProcAesDecHdrVtable(void)
{
	return &g_swDepProcAesDecHdrVtable;
}

/* ---- 4. AES Enc Key-Pool processor ---- */

static const char *swDepProcAesEncKeyName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcAesEncKeyInit(struct SwDepProc *proc)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;

	swDepAesEngineInit(&self->engine, proc, 1, 1);
	return 0;
}

static void swDepProcAesEncKeyFini(struct SwDepProc *proc)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;

	swDepAesEngineFini(&self->engine);
}

static int swDepProcAesEncKeyOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;
	const unsigned char *cfg = (const unsigned char *)config;
	int status;

	status = swDepAesEngineOpen(&self->engine, dstQueueId, config);
	if (status != 0)
		return status;
	self->keyState = 0;
	self->curEmi = cfg[8];
	self->bytesEncrypted = 0;
	self->pcpPayloadSize = 0;
	return 0;
}

static void swDepProcAesEncKeyClose(struct SwDepProc *proc)
{
	(void)proc;
}

static void swDepProcAesEncKeyPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;

	swDepAesEnginePush(&self->engine, msg);
}

static void swDepProcAesEncKeyPrepare(struct SwDepProcAesKey *self)
{
	unsigned char emi = swDepAesEngineGetCci(&self->engine);
	struct SwDepKeyInfoOut keyInfo;

	if (self->keyState == 1) {
		if (self->bytesEncrypted >= 0x08000000u) {
			self->keyState = 3;
			self->bytesEncrypted = 0;
		} else if (self->curEmi != emi) {
			self->keyState = 2;
		}
	}

	switch (self->keyState) {
	case 0:
		swDepStreamGetKey(self->base.stream, emi, 0, &keyInfo, self->pcpHeader);
		self->pcpPayloadSize = keyInfo.pcpPayloadSize;
		swDepAesEngineSetKey(&self->engine, keyInfo.enableEncrypt == 1 ? 1u : 0u,
				     keyInfo.key, keyInfo.iv);
		swDepAesEngineSetLen(&self->engine, self->pcpPayloadSize);
		self->keyState = 1;
		break;
	case 1:
		if (self->bytesEncrypted + self->pcpPayloadSize > 0x08000000u)
			swDepAesEngineSetLen(&self->engine, 0x08000000u - self->bytesEncrypted);
		else
			swDepAesEngineSetLen(&self->engine, self->pcpPayloadSize);
		swDepAesEngineResetIv(&self->engine);
		break;
	case 2:
		swDepStreamGetKey(self->base.stream, emi, 0, &keyInfo, self->pcpHeader);
		self->pcpPayloadSize = keyInfo.pcpPayloadSize;
		swDepAesEngineSetKey(&self->engine, keyInfo.enableEncrypt == 1 ? 1u : 0u,
				     keyInfo.key, keyInfo.iv);
		swDepAesEngineSetLen(&self->engine, self->pcpPayloadSize);
		self->curEmi = emi;
		self->keyState = 1;
		break;
	case 3:
		swDepStreamGetKey(self->base.stream, emi, 1, &keyInfo, self->pcpHeader);
		self->pcpPayloadSize = keyInfo.pcpPayloadSize;
		swDepAesEngineSetKey(&self->engine, keyInfo.enableEncrypt == 1 ? 1u : 0u,
				     keyInfo.key, keyInfo.iv);
		swDepAesEngineSetLen(&self->engine, self->pcpPayloadSize);
		self->keyState = 1;
		break;
	default:
		break;
	}
}

static void swDepProcAesEncKeyRun(struct SwDepProc *proc)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;
	int cont = 1;

	do {
		switch (swDepAesEngineState(&self->engine)) {
		case 0:
			swDepAesEngineStepIdle(&self->engine, &cont);
			break;
		case 1:
			swDepAesEngineStepWaitHdr(&self->engine, &cont);
			break;
		case 2:
			swDepProcAesEncKeyPrepare(self);
			swDepAesEngineStepStart(&self->engine, self->pcpHeader, 14, &cont);
			break;
		case 3:
			swDepAesEngineStepRun(&self->engine, &cont);
			break;
		case 4:
			swDepAesEngineStepFlush(&self->engine, 1, &cont);
			self->bytesEncrypted += swDepAesEngineLastOutLen(&self->engine);
			swDepAesEngineResetChunk(&self->engine);
			break;
		case 5:
			swDepAesEngineStepEos(&self->engine, &cont);
			break;
		default:
			cont = 0;
			break;
		}
	} while (cont != 0);
}

static int swDepProcAesEncKeyCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcAesEncKeyDump(struct SwDepProc *proc)
{
	struct SwDepProcAesKey *self = (struct SwDepProcAesKey *)proc;

	swDepProcDumpHeader(proc);
	swDepAesEngineDump(&self->engine);
	swLogPrintf("sw dep:<msgmgr mqueueid_dst>\n");
}

static const struct SwDepProcVtable g_swDepProcAesEncKeyVtable = {
	swDepProcAesEncKeyName, swDepProcAesEncKeyInit, swDepProcAesEncKeyFini,
	swDepProcAesEncKeyOpen, swDepProcAesEncKeyClose, swDepProcAesEncKeyPush,
	swDepProcAesEncKeyRun, swDepProcAesEncKeyCtrl, swDepProcAesEncKeyDump,
};

const struct SwDepProcVtable *swDepProcAesEncKeyVtable(void)
{
	return &g_swDepProcAesEncKeyVtable;
}

/* ---- 5. Opt-parser RX processor ---- */

static const char *swDepProcOptParseRxName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcOptParseRxInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcOptParseRxFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcOptParseRxOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcOptRx *self = (struct SwDepProcOptRx *)proc;
	const unsigned char *cfg = (const unsigned char *)config;
	unsigned int is192;

	(void)dstQueueId;
	swDepMsgQueueInit(&self->queue);
	self->pmtPid = 0xFFFFu;
	self->sawPmt = 0;
	self->emiMatched = 1;
	self->mismatchCount = 0;
	memcpy(&is192, cfg + 20, sizeof(is192));
	self->pktSize = (is192 != 0) ? 192u : 188u;
	self->tsOffset = (self->pktSize == 192u) ? 4u : 0u;
	self->curEmi = cfg[8];
	self->pmtEmi = cfg[8];
	self->partialLen = 0;
	self->heldMsg = NULL;
	swOptSectionInit(self->patSec);
	swOptSectionInit(self->pmtSec);
	swOptParserInit(self->parser);
	return 0;
}

static void swDepProcOptParseRxClose(struct SwDepProc *proc)
{
	struct SwDepProcOptRx *self = (struct SwDepProcOptRx *)proc;

	swOptParserFini(self->parser);
	swOptSectionFini(self->pmtSec);
	swOptSectionFini(self->patSec);
	swDepMsgQueueFini(&self->queue);
}

static void swDepProcOptParseRxPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcOptRx *self = (struct SwDepProcOptRx *)proc;

	swDepMsgQueuePushTail(&self->queue, msg);
}

static void swDepProcOptParseRxCheckBoundary(struct SwDepProcOptRx *self)
{
	if (self->sawPmt != 0) {
		if (self->emiMatched != 0) {
			self->mismatchCount = 0;
		} else if (++self->mismatchCount > 2u) {
			swDepStreamNotifyError(self->base.stream, 0x08000000);
			self->mismatchCount = 0;
		}
	} else {
		if (self->mismatchCount != 0)
			++self->mismatchCount;
		if (self->mismatchCount > 2u) {
			swDepStreamNotifyError(self->base.stream, 0x08000000);
			self->mismatchCount = 0;
		}
	}
	self->emiMatched = 0;
	self->sawPmt = 0;
}

static void swDepProcOptParseRxHandleCci(struct SwDepProcOptRx *self, void *cciDesc,
					 unsigned char cci, unsigned char epn)
{
	self->pmtEmi = swOptCciToEemi(cci, epn);
	swDepStreamNotifyCci(self->base.stream, cciDesc, self->pmtEmi, self->curEmi);
	if (swOptCheckEemiCompatibility(self->curEmi, self->pmtEmi) != 0)
		self->emiMatched = 1;
	self->sawPmt = 1;
}

static void swDepProcOptParseRxPacket(struct SwDepProcOptRx *self, unsigned char *pkt)
{
	unsigned short pid = (unsigned short)(((pkt[1] & 0x1Fu) << 8) | pkt[2]);
	int complete = 0;
	unsigned char *secData = NULL;
	unsigned int secLen = 0;
	unsigned int errBits;

	if (pid == 0) {
		if (sw_opt_section_put(self->patSec, pkt, 188u, 0, &complete,
				       &secData, &secLen) != 0) {
			swOptSectionReset(self->patSec);
			return;
		}
		if (complete != 0) {
			unsigned short parsedPid;

			if (sw_opt_parser_parsepat(self->parser, secData, secLen) != 0) {
				memcpy(&errBits, self->parser + 16, sizeof(errBits));
				swDepStreamNotifyError(self->base.stream, (int)errBits);
				return;
			}
			memcpy(&parsedPid, self->parser, sizeof(parsedPid));
			if (parsedPid != 0xFFFFu)
				self->pmtPid = parsedPid;
		}
	} else if (pid == self->pmtPid) {
		if (sw_opt_section_put(self->pmtSec, pkt, 188u, 0, &complete,
				       &secData, &secLen) != 0) {
			swOptSectionReset(self->pmtSec);
			return;
		}
		if (complete != 0) {
			int hasCci;

			if (sw_opt_parser_parsepmt(self->parser, secData, secLen) != 0) {
				memcpy(&errBits, self->parser + 16, sizeof(errBits));
				swDepStreamNotifyError(self->base.stream, (int)errBits);
				return;
			}
			memcpy(&hasCci, self->parser + 4, sizeof(hasCci));
			if (hasCci != 0)
				swDepProcOptParseRxHandleCci(self, self->parser + 10,
							     self->parser[8], self->parser[9]);
		}
	}
}

static void swDepProcOptParseRxRun(struct SwDepProc *proc)
{
	struct SwDepProcOptRx *self = (struct SwDepProcOptRx *)proc;
	struct SwDepMsg *msg;

	while ((msg = swDepMsgQueuePop(&self->queue)) != NULL) {
		if ((msg->flags & 1u) != 0)
			swDepProcOptParseRxCheckBoundary(self);
		if (msg->type >= 2u && msg->type <= 6u) {
			unsigned char *cursor;
			unsigned int rem;

			self->curEmi = msg->cci;
			if (self->partialLen != 0) {
				unsigned int need = self->pktSize - self->partialLen;
				unsigned int take = (msg->len >= need) ? need : msg->len;

				memcpy(self->pktBuf + self->partialLen, msg->data, take);
				self->partialLen += take;
				if (self->partialLen == self->pktSize) {
					swDepProcOptParseRxPacket(self, self->pktBuf + self->tsOffset);
					if (self->heldMsg != NULL) {
						unsigned int prevLen = self->partialLen - take;
						unsigned char *heldTail =
							(unsigned char *)self->heldMsg->data +
							self->heldMsg->len - prevLen;

						memcpy(heldTail, self->pktBuf, prevLen);
						memcpy(msg->data, self->pktBuf + prevLen, take);
						swDepProcPush(self->base.nextProc, self->heldMsg);
						self->heldMsg = NULL;
					}
					self->partialLen = 0;
				}
				cursor = (unsigned char *)msg->data + take;
				rem = msg->len - take;
			} else {
				cursor = (unsigned char *)msg->data;
				rem = msg->len;
			}
			while (rem >= self->pktSize) {
				swDepProcOptParseRxPacket(self, cursor + self->tsOffset);
				cursor += self->pktSize;
				rem -= self->pktSize;
			}
			if (rem != 0) {
				memcpy(self->pktBuf, cursor, rem);
				self->partialLen = rem;
				self->heldMsg = msg;
				msg = NULL;
			}
		} else if (msg->type == 7u) {
			self->base.isEos = 1;
			self->partialLen = 0;
			if (self->heldMsg != NULL) {
				swDepProcPush(self->base.nextProc, self->heldMsg);
				self->heldMsg = NULL;
			}
		}
		if (msg != NULL)
			swDepProcPush(self->base.nextProc, msg);
	}
}

static int swDepProcOptParseRxCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcOptParseRxDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
	swLogPrintf("sw dep:<mqueue>\n");
}

static const struct SwDepProcVtable g_swDepProcOptParseRxVtable = {
	swDepProcOptParseRxName, swDepProcOptParseRxInit, swDepProcOptParseRxFini,
	swDepProcOptParseRxOpen, swDepProcOptParseRxClose, swDepProcOptParseRxPush,
	swDepProcOptParseRxRun, swDepProcOptParseRxCtrl, swDepProcOptParseRxDump,
};

const struct SwDepProcVtable *swDepProcOptParseRxVtable(void)
{
	return &g_swDepProcOptParseRxVtable;
}

/* ---- 6. Opt-parser TX processor ---- */

static const char *swDepProcOptParseTxName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcOptParseTxInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcOptParseTxFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcOptParseTxOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcOptTx *self = (struct SwDepProcOptTx *)proc;
	const unsigned char *cfg = (const unsigned char *)config;
	unsigned int is192;

	(void)dstQueueId;
	swDepMsgQueueInit(&self->queue);
	self->pmtPid = 0xFFFFu;
	memcpy(&is192, cfg + 20, sizeof(is192));
	self->pktSize = (is192 != 0) ? 192u : 188u;
	self->tsOffset = (self->pktSize == 192u) ? 4u : 0u;
	self->curEmi = cfg[8];
	self->outEmi = cfg[8];
	memcpy(&self->rewriteFlag, cfg + 12, sizeof(self->rewriteFlag));
	self->partialLen = 0;
	self->heldMsg = NULL;
	swOptSectionInit(self->patSec);
	swOptSectionInit(self->pmtSec);
	swOptParserInit(self->parser);
	return 0;
}

static void swDepProcOptParseTxClose(struct SwDepProc *proc)
{
	struct SwDepProcOptTx *self = (struct SwDepProcOptTx *)proc;

	swOptParserFini(self->parser);
	swOptSectionFini(self->pmtSec);
	swOptSectionFini(self->patSec);
	swDepMsgQueueFini(&self->queue);
}

static void swDepProcOptParseTxPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcOptTx *self = (struct SwDepProcOptTx *)proc;

	swDepMsgQueuePushTail(&self->queue, msg);
}

static void swDepProcOptParseTxHandleCci(struct SwDepProcOptTx *self, void *cciDesc,
					 unsigned char cci, unsigned char epn)
{
	self->outEmi = swOptCciToEemi(cci, epn);
	swDepStreamNotifyCci(self->base.stream, cciDesc, self->outEmi, self->curEmi);
}

static void swDepProcOptParseTxPacket(struct SwDepProcOptTx *self, unsigned char *pkt)
{
	unsigned short pid = (unsigned short)(((pkt[1] & 0x1Fu) << 8) | pkt[2]);
	int complete = 0;
	unsigned char *secData = NULL;
	unsigned int secLen = 0;
	unsigned int errBits;

	if (pid == 0) {
		if (sw_opt_section_put(self->patSec, pkt, 188u, 0, &complete,
				       &secData, &secLen) != 0) {
			swOptSectionReset(self->patSec);
			return;
		}
		if (complete != 0) {
			unsigned short parsedPid;

			if (sw_opt_parser_parsepat(self->parser, secData, secLen) != 0) {
				memcpy(&errBits, self->parser + 16, sizeof(errBits));
				swDepStreamNotifyError(self->base.stream, (int)errBits);
				return;
			}
			memcpy(&parsedPid, self->parser, sizeof(parsedPid));
			if (parsedPid != 0xFFFFu)
				self->pmtPid = parsedPid;
		}
	} else if (pid == self->pmtPid) {
		if (sw_opt_section_put(self->pmtSec, pkt, 188u, self->rewriteFlag, &complete,
				       &secData, &secLen) != 0) {
			swOptSectionReset(self->pmtSec);
			return;
		}
		if (complete != 0) {
			int hasCci;

			if (sw_opt_parser_parsepmt(self->parser, secData, secLen) != 0) {
				memcpy(&errBits, self->parser + 16, sizeof(errBits));
				swDepStreamNotifyError(self->base.stream, (int)errBits);
				return;
			}
			memcpy(&hasCci, self->parser + 4, sizeof(hasCci));
			if (hasCci != 0)
				swDepProcOptParseTxHandleCci(self, self->parser + 10,
							     self->parser[8], self->parser[9]);
		}
	}
}

static void swDepProcOptParseTxRun(struct SwDepProc *proc)
{
	struct SwDepProcOptTx *self = (struct SwDepProcOptTx *)proc;
	struct SwDepMsg *msg;

	while ((msg = swDepMsgQueuePop(&self->queue)) != NULL) {
		if (msg->type >= 2u && msg->type <= 6u) {
			unsigned char *cursor;
			unsigned int rem;

			self->curEmi = self->outEmi;
			if (self->partialLen != 0) {
				unsigned int need = self->pktSize - self->partialLen;
				unsigned int take = (msg->len >= need) ? need : msg->len;

				memcpy(self->pktBuf + self->partialLen, msg->data, take);
				self->partialLen += take;
				if (self->partialLen == self->pktSize) {
					swDepProcOptParseTxPacket(self, self->pktBuf + self->tsOffset);
					if (self->heldMsg != NULL) {
						unsigned int prevLen = self->partialLen - take;
						unsigned char *heldTail =
							(unsigned char *)self->heldMsg->data +
							self->heldMsg->len - prevLen;

						memcpy(heldTail, self->pktBuf, prevLen);
						memcpy(msg->data, self->pktBuf + prevLen, take);
						swDepProcPush(self->base.nextProc, self->heldMsg);
						self->heldMsg = NULL;
					}
					self->partialLen = 0;
				}
				cursor = (unsigned char *)msg->data + take;
				rem = msg->len - take;
			} else {
				cursor = (unsigned char *)msg->data;
				rem = msg->len;
			}
			while (rem >= self->pktSize) {
				swDepProcOptParseTxPacket(self, cursor + self->tsOffset);
				cursor += self->pktSize;
				rem -= self->pktSize;
			}
			msg->cci = self->outEmi;
			if (rem != 0) {
				memcpy(self->pktBuf, cursor, rem);
				self->partialLen = rem;
				self->heldMsg = msg;
				msg = NULL;
			}
		} else {
			if (msg->type == 7u)
				self->base.isEos = 1;
			self->partialLen = 0;
			if (self->heldMsg != NULL) {
				swDepProcPush(self->base.nextProc, self->heldMsg);
				self->heldMsg = NULL;
			}
		}
		if (msg != NULL)
			swDepProcPush(self->base.nextProc, msg);
	}
}

static int swDepProcOptParseTxCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	struct SwDepProcOptTx *self = (struct SwDepProcOptTx *)proc;

	if (cmd == 1)
		self->outEmi = *(const unsigned char *)arg;
	return 0;
}

static void swDepProcOptParseTxDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
	swLogPrintf("sw dep:<mqueue>\n");
}

static const struct SwDepProcVtable g_swDepProcOptParseTxVtable = {
	swDepProcOptParseTxName, swDepProcOptParseTxInit, swDepProcOptParseTxFini,
	swDepProcOptParseTxOpen, swDepProcOptParseTxClose, swDepProcOptParseTxPush,
	swDepProcOptParseTxRun, swDepProcOptParseTxCtrl, swDepProcOptParseTxDump,
};

const struct SwDepProcVtable *swDepProcOptParseTxVtable(void)
{
	return &g_swDepProcOptParseTxVtable;
}

/* ---- 7. CCI tagger processor ---- */

static const char *swDepProcCciTagName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcCciTagInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcCciTagFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcCciTagOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcCciTag *self = (struct SwDepProcCciTag *)proc;
	const unsigned char *cfg = (const unsigned char *)config;

	(void)dstQueueId;
	swDepMsgQueueInit(&self->queue);
	self->cci = cfg[8];
	return 0;
}

static void swDepProcCciTagClose(struct SwDepProc *proc)
{
	struct SwDepProcCciTag *self = (struct SwDepProcCciTag *)proc;

	swDepMsgQueueFini(&self->queue);
}

static void swDepProcCciTagPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcCciTag *self = (struct SwDepProcCciTag *)proc;

	swDepMsgQueuePushTail(&self->queue, msg);
}

static void swDepProcCciTagRun(struct SwDepProc *proc)
{
	struct SwDepProcCciTag *self = (struct SwDepProcCciTag *)proc;
	struct SwDepMsg *msg;

	while ((msg = swDepMsgQueuePop(&self->queue)) != NULL) {
		if (msg->type >= 2u && msg->type <= 6u)
			msg->cci = self->cci;
		else if (msg->type == 7u)
			self->base.isEos = 1;
		swDepProcPush(self->base.nextProc, msg);
	}
}

static int swDepProcCciTagCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	struct SwDepProcCciTag *self = (struct SwDepProcCciTag *)proc;

	if (cmd == 1)
		self->cci = *(const unsigned char *)arg;
	return 0;
}

static void swDepProcCciTagDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
	swLogPrintf("sw dep:<mqueue>\n");
}

static const struct SwDepProcVtable g_swDepProcCciTagVtable = {
	swDepProcCciTagName, swDepProcCciTagInit, swDepProcCciTagFini,
	swDepProcCciTagOpen, swDepProcCciTagClose, swDepProcCciTagPush,
	swDepProcCciTagRun, swDepProcCciTagCtrl, swDepProcCciTagDump,
};

const struct SwDepProcVtable *swDepProcCciTagVtable(void)
{
	return &g_swDepProcCciTagVtable;
}

/* ---- 8. Input queue processor ---- */

static const char *swDepProcInputName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcInputInit(struct SwDepProc *proc)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;

	swDepMsgQueueInit(&self->queue);
	return 0;
}

static void swDepProcInputFini(struct SwDepProc *proc)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;

	swDepMsgQueueFini(&self->queue);
}

static int swDepProcInputOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;

	(void)dstQueueId;
	(void)config;
	swDepMsgQueueInit(&self->queue);
	return 0;
}

static void swDepProcInputClose(struct SwDepProc *proc)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;

	swDepMsgQueueFini(&self->queue);
}

static void swDepProcInputPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;

	swDepMutexLock(&self->base.stream->mutex);
	swDepMsgQueuePushTail(&self->queue, msg);
	swDepMutexUnlock(&self->base.stream->mutex);
}

static void swDepProcInputRun(struct SwDepProc *proc)
{
	struct SwDepProcInput *self = (struct SwDepProcInput *)proc;
	struct SwDepMsg *msg;

	swDepMutexLock(&self->base.stream->mutex);
	while ((msg = swDepMsgQueuePop(&self->queue)) != NULL) {
		swDepProcPush(self->base.nextProc, msg);
		if (msg->type == 7u)
			self->base.isEos = 1;
	}
	swDepMutexUnlock(&self->base.stream->mutex);
}

static int swDepProcInputCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcInputDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
	swLogPrintf("sw dep:<mqueue>\n");
}

static const struct SwDepProcVtable g_swDepProcInputVtable = {
	swDepProcInputName, swDepProcInputInit, swDepProcInputFini,
	swDepProcInputOpen, swDepProcInputClose, swDepProcInputPush,
	swDepProcInputRun, swDepProcInputCtrl, swDepProcInputDump,
};

const struct SwDepProcVtable *swDepProcInputVtable(void)
{
	return &g_swDepProcInputVtable;
}

/* ---- 9. Output DMA sink processor ---- */

static const char *swDepProcOutputDmaName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcOutputDmaInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcOutputDmaFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcOutputDmaOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcOutputDma *self = (struct SwDepProcOutputDma *)proc;
	unsigned int mode;

	(void)dstQueueId;
	self->isFirstChunk = 1;
	mode = *(const unsigned int *)config & 0x03FF03FFu;
	switch (mode) {
	case 0x00010002u: case 0x00040002u: case 0x00080002u: case 0x00100002u:
	case 0x00200002u: case 0x00400002u: case 0x00800002u: case 0x01000002u:
	case 0x02000002u:
		self->emitBoundary = 1;
		break;
	default:
		self->emitBoundary = 0;
		break;
	}
	self->base.isReady = 0;
	return 0;
}

static void swDepProcOutputDmaClose(struct SwDepProc *proc)
{
	(void)proc;
}

static void swDepProcOutputDmaPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcOutputDma *self = (struct SwDepProcOutputDma *)proc;

	if (msg->type == 7u) {
		swDepStreamRxOne(self->base.stream, NULL, 0, 0x20000000, 0);
		swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
		self->base.isEos = 1;
		swDepStreamSetDone(self->base.stream);
	} else if (msg->type == 5u) {
		int flags = 0;

		if (self->emitBoundary != 0) {
			if (self->isFirstChunk != 0) {
				flags = 0x00800000;
				self->isFirstChunk = 0;
			}
			if (msg->pktInfo != 0) {
				flags |= 0x08000000;
				self->isFirstChunk = 1;
			}
		}
		swDepStreamRxOne(self->base.stream, msg->data, msg->len, flags, (int)msg->pktInfo);
		swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
	}
}

static void swDepProcOutputDmaRun(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcOutputDmaCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcOutputDmaDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
}

static const struct SwDepProcVtable g_swDepProcOutputDmaVtable = {
	swDepProcOutputDmaName, swDepProcOutputDmaInit, swDepProcOutputDmaFini,
	swDepProcOutputDmaOpen, swDepProcOutputDmaClose, swDepProcOutputDmaPush,
	swDepProcOutputDmaRun, swDepProcOutputDmaCtrl, swDepProcOutputDmaDump,
};

const struct SwDepProcVtable *swDepProcOutputDmaVtable(void)
{
	return &g_swDepProcOutputDmaVtable;
}

/* ---- 10. RingBuf source processor ---- */

static const char *swDepProcRingSrcName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcRingSrcInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcRingSrcFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcRingSrcOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcRingSrc *self = (struct SwDepProcRingSrc *)proc;
	int status;

	self->poolId = dstQueueId;
	status = swDepStreamAcquireRing(self->base.stream, *(const unsigned int *)config,
					0, &self->ring);
	return (status != 0) ? status : 0;
}

static void swDepProcRingSrcClose(struct SwDepProc *proc)
{
	struct SwDepProcRingSrc *self = (struct SwDepProcRingSrc *)proc;

	swDepStreamReleaseRing(self->base.stream, 0, self->ring);
}

static void swDepProcRingSrcPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	(void)proc;
	(void)msg;
}

static void swDepProcRingSrcRun(struct SwDepProc *proc)
{
	struct SwDepProcRingSrc *self = (struct SwDepProcRingSrc *)proc;
	void *ptr = NULL;
	unsigned int avail = 0;
	int eos = 0;
	struct SwDepMsg *msg = NULL;

	swDepRingPeekRead(self->ring, &ptr, &avail, &eos);
	if (avail != 0) {
		unsigned char *cursor = (unsigned char *)ptr;
		unsigned int rem = avail;

		while (rem >= 9024u) {
			msg = swDepMsgMgrAlloc(&self->base.stream->msgMgr, self->poolId);
			if (msg == NULL)
				break;
			msg->data = cursor;
			msg->bufLen = 9024u;
			msg->len = 9024u;
			msg->cci = 255u;
			msg->pktInfo = 0;
			swDepProcPush(self->base.nextProc, msg);
			cursor += 9024u;
			rem -= 9024u;
		}
		if (msg != NULL && rem < 9024u && eos != 0) {
			struct SwDepMsg *tail =
				swDepMsgMgrAlloc(&self->base.stream->msgMgr, self->poolId);

			if (tail != NULL) {
				tail->data = cursor;
				tail->bufLen = 9024u;
				tail->len = rem;
				tail->cci = 255u;
				tail->pktInfo = 0;
				swDepProcPush(self->base.nextProc, tail);
				rem = 0;
			}
			swDepProcPushEos(proc);
		}
		if (avail != rem)
			swDepRingConsume(self->ring, avail - rem);
	}
}

static int swDepProcRingSrcCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcRingSrcDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
}

static const struct SwDepProcVtable g_swDepProcRingSrcVtable = {
	swDepProcRingSrcName, swDepProcRingSrcInit, swDepProcRingSrcFini,
	swDepProcRingSrcOpen, swDepProcRingSrcClose, swDepProcRingSrcPush,
	swDepProcRingSrcRun, swDepProcRingSrcCtrl, swDepProcRingSrcDump,
};

const struct SwDepProcVtable *swDepProcRingSrcVtable(void)
{
	return &g_swDepProcRingSrcVtable;
}

/* ---- 11. RingBuf sink processor ---- */

static const char *swDepProcRingSinkName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcRingSinkInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcRingSinkFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcRingSinkOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcRingSink *self = (struct SwDepProcRingSink *)proc;
	int status;

	(void)dstQueueId;
	swDepMsgQueueInit(&self->queue);
	status = swDepStreamAcquireRing(self->base.stream, *(const unsigned int *)config,
					1, &self->ring);
	if (status != 0)
		return status;
	self->curMsg = NULL;
	self->curPtr = NULL;
	self->curRem = 0;
	self->curCci = 255u;
	self->eosPending = 0;
	return 0;
}

static void swDepProcRingSinkClose(struct SwDepProc *proc)
{
	struct SwDepProcRingSink *self = (struct SwDepProcRingSink *)proc;

	swDepStreamReleaseRing(self->base.stream, 1, self->ring);
	swDepMsgQueueFini(&self->queue);
}

static void swDepProcRingSinkPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcRingSink *self = (struct SwDepProcRingSink *)proc;

	swDepMsgQueuePushTail(&self->queue, msg);
}

static void swDepProcRingSinkRun(struct SwDepProc *proc)
{
	struct SwDepProcRingSink *self = (struct SwDepProcRingSink *)proc;
	struct SwDepMsg *msg;
	void *dstPtr = NULL;
	unsigned int dstAvail = 0;

	if (self->curMsg == NULL) {
		msg = swDepMsgQueuePop(&self->queue);
		if (msg == NULL)
			return;
		if (msg->type == 7u) {
			self->eosPending = 1;
			swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
		} else if (msg->type >= 2u && msg->type <= 4u) {
			self->curMsg = msg;
			self->curPtr = (unsigned char *)msg->data;
			self->curRem = msg->len;
			self->curCci = msg->cci;
		} else {
			swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
		}
	}

	if (self->curRem != 0) {
		unsigned int copyLen;

		swDepRingPeekWrite(self->ring, &dstPtr, &dstAvail);
		copyLen = (self->curRem >= dstAvail) ? dstAvail : self->curRem;
		if (copyLen != 0) {
			memcpy(dstPtr, self->curPtr, copyLen);
			self->curPtr += copyLen;
			self->curRem -= copyLen;
			swDepRingCommit(self->ring, copyLen);
		}
	}

	if (self->eosPending != 0 && self->curRem == 0)
		swDepProcPushEos(proc);
}

static int swDepProcRingSinkCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcRingSinkDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
}

static const struct SwDepProcVtable g_swDepProcRingSinkVtable = {
	swDepProcRingSinkName, swDepProcRingSinkInit, swDepProcRingSinkFini,
	swDepProcRingSinkOpen, swDepProcRingSinkClose, swDepProcRingSinkPush,
	swDepProcRingSinkRun, swDepProcRingSinkCtrl, swDepProcRingSinkDump,
};

const struct SwDepProcVtable *swDepProcRingSinkVtable(void)
{
	return &g_swDepProcRingSinkVtable;
}

/* ---- 12. TS 188<->192 packetizer / sync processor ---- */

static const char *swDepProcTsConvName(struct SwDepProc *proc)
{
	(void)proc;
	return "";
}

static int swDepProcTsConvInit(struct SwDepProc *proc)
{
	(void)proc;
	return 0;
}

static void swDepProcTsConvFini(struct SwDepProc *proc)
{
	(void)proc;
}

static int swDepProcTsConvOpen(struct SwDepProc *proc, unsigned char dstQueueId, void *config)
{
	struct SwDepProcTsConv *self = (struct SwDepProcTsConv *)proc;
	const unsigned int *cfg = (const unsigned int *)config;
	const unsigned char *cfgBytes = (const unsigned char *)config;

	swDepMsgQueueInit(&self->queue);
	self->dstPoolId = dstQueueId;
	if (cfg[4] != 0) {
		if (cfg[5] != 0) {
			self->mode = 1;
			self->inPktSize = 192u;
			self->inTsOff = 4u;
			self->outPktSize = 192u;
			self->outTsOff = 4u;
		} else {
			self->mode = 2;
			self->inPktSize = 192u;
			self->inTsOff = 4u;
			self->outPktSize = 188u;
			self->outTsOff = 0u;
		}
	} else if (cfg[5] != 0) {
		self->mode = 3;
		self->inPktSize = 188u;
		self->inTsOff = 0u;
		self->outPktSize = 192u;
		self->outTsOff = 4u;
	} else {
		self->mode = 0;
		self->inPktSize = 188u;
		self->inTsOff = 0u;
		self->outPktSize = 188u;
		self->outTsOff = 0u;
	}
	self->enableSync = (int)cfg[7];
	self->inMsg = NULL;
	self->inPtr = NULL;
	self->inRem = 0;
	self->inCci = cfgBytes[8];
	self->eosSeen = 0;
	self->inScratchPtr = NULL;
	self->inScratchLen = 0;
	self->outMsg = NULL;
	self->outPtr = NULL;
	self->outRem = 0;
	self->outScratchLen = 0;
	self->unsynced = 0;
	self->syncHits = 0;
	self->syncSkip = 0;
	return 0;
}

static void swDepProcTsConvClose(struct SwDepProc *proc)
{
	struct SwDepProcTsConv *self = (struct SwDepProcTsConv *)proc;

	swDepMsgQueueFini(&self->queue);
}

static void swDepProcTsConvPush(struct SwDepProc *proc, struct SwDepMsg *msg)
{
	struct SwDepProcTsConv *self = (struct SwDepProcTsConv *)proc;

	swDepMsgQueuePushTail(&self->queue, msg);
}

static void swDepTsConvFlushOut(struct SwDepProcTsConv *self)
{
	struct SwDepMsg *msg = self->outMsg;

	msg->len = msg->bufLen - self->outRem;
	msg->cci = self->inCci;
	swDepProcPush(self->base.nextProc, msg);
	self->outMsg = NULL;
	self->outRem = 0;
}

static int swDepTsConvResync(struct SwDepProcTsConv *self, unsigned char **ptrIO,
			     unsigned int *remIO)
{
	unsigned char *cursor = *ptrIO;
	unsigned char *end = cursor + *remIO;

	if (self->syncHits != 0 && self->syncSkip != 0)
		cursor += (int)self->inPktSize - self->syncSkip;
	while (cursor < end) {
		if (self->syncHits != 0) {
			unsigned char *probe = cursor;

			if (cursor + self->inTsOff < end) {
				while (probe[self->inTsOff] == 0x47u) {
					if (++self->syncHits >= 2u) {
						self->unsynced = 0;
						self->syncSkip = 0;
						*ptrIO = probe;
						*remIO = (unsigned int)(end - probe);
						return 1;
					}
					probe += self->inPktSize;
					if (probe + self->inTsOff >= end)
						break;
				}
				if (probe + self->inTsOff < end) {
					self->syncHits = 0;
					++cursor;
				}
			}
			if (self->syncHits != 0) {
				self->syncSkip = (int)(probe - end);
				*ptrIO = end;
				*remIO = 0;
				return 1;
			}
		} else if (*cursor == 0x47u) {
			++self->syncHits;
			cursor += self->inPktSize - self->inTsOff;
		} else {
			++cursor;
		}
	}
	*ptrIO = end;
	*remIO = 0;
	return 1;
}

static struct SwDepMsg *swDepTsConvAllocOut(struct SwDepProcTsConv *self)
{
	struct SwDepMsg *msg = swDepMsgMgrAlloc(&self->base.stream->msgMgr, self->dstPoolId);

	if (msg != NULL) {
		self->outMsg = msg;
		self->outPtr = (unsigned char *)msg->data;
		self->outRem = msg->bufLen;
	}
	return msg;
}

static void swDepTsConvDrainScratch(struct SwDepProcTsConv *self)
{
	unsigned int take =
		(self->outScratchLen >= self->outRem) ? self->outRem : self->outScratchLen;

	memcpy(self->outPtr, self->outScratchPtr, take);
	self->outScratchLen -= take;
	self->outScratchPtr += take;
	self->outRem -= take;
	self->outPtr += take;
}

static int swDepTsConvEmitPacket(struct SwDepProcTsConv *self, const unsigned char *pkt)
{
	if (self->outMsg == NULL) {
		while (swDepTsConvAllocOut(self) != NULL) {
			if (self->outScratchLen != 0) {
				unsigned int take =
					(self->outScratchLen >= self->outRem) ?
						self->outRem : self->outScratchLen;

				memcpy(self->outPtr, self->outScratchPtr, take);
				self->outScratchLen -= take;
				self->outRem -= take;
				self->outPtr += take;
			}
			if (self->outRem == 0)
				swDepTsConvFlushOut(self);
			if (self->outRem != 0)
				break;
		}
		if (self->outMsg == NULL)
			return 0;
	}

	if (self->outRem < self->outPktSize) {
		if (self->mode == 3) {
			memset(self->outScratch, 0, 4);
			memcpy(self->outScratch + 4, pkt, 188);
		} else if (self->mode == 2) {
			memcpy(self->outScratch, pkt + 4, self->outPktSize);
		} else {
			memcpy(self->outScratch, pkt, self->outPktSize);
		}
		self->outScratchPtr = self->outScratch;
		self->outScratchLen = self->outPktSize;
		if (self->outRem != 0)
			swDepTsConvDrainScratch(self);
	} else {
		if (self->mode == 3) {
			memset(self->outPtr, 0, 4);
			memcpy(self->outPtr + 4, pkt, 188);
		} else if (self->mode == 2) {
			memcpy(self->outPtr, pkt + 4, self->outPktSize);
		} else {
			memcpy(self->outPtr, pkt, self->outPktSize);
		}
		self->outPtr += self->outPktSize;
		self->outRem -= self->outPktSize;
	}
	if (self->outRem == 0)
		swDepTsConvFlushOut(self);
	return 0;
}

static int swDepTsConvProcessSynced(struct SwDepProcTsConv *self, unsigned char **ptrIO,
				    unsigned int *remIO)
{
	unsigned char *cursor = *ptrIO;
	unsigned char *end = cursor + *remIO;
	int status;

	while (cursor + self->inPktSize <= end) {
		if (cursor[self->inTsOff] != 0x47u) {
			self->unsynced = 1;
			self->syncHits = 0;
			*ptrIO = cursor;
			*remIO = (unsigned int)(end - cursor);
			return 0;
		}
		status = swDepTsConvEmitPacket(self, cursor);
		if (status != 0) {
			*ptrIO = cursor;
			*remIO = (unsigned int)(end - cursor);
			return status;
		}
		cursor += self->inPktSize;
	}
	*ptrIO = cursor;
	*remIO = (unsigned int)(end - cursor);
	return 0;
}

static int swDepTsConvProcessRaw(struct SwDepProcTsConv *self, unsigned char **ptrIO,
				 unsigned int *remIO)
{
	unsigned char *cursor = *ptrIO;
	unsigned char *end = cursor + *remIO;
	int status;

	while (cursor + self->inPktSize <= end) {
		status = swDepTsConvEmitPacket(self, cursor);
		if (status != 0) {
			*ptrIO = cursor;
			*remIO = (unsigned int)(end - cursor);
			return status;
		}
		cursor += self->inPktSize;
	}
	*ptrIO = cursor;
	*remIO = (unsigned int)(end - cursor);
	return 0;
}

static int swDepTsConvProcessSpan(struct SwDepProcTsConv *self, unsigned char **ptrIO,
				  unsigned int *remIO)
{
	if (self->enableSync == 0)
		return swDepTsConvProcessRaw(self, ptrIO, remIO);
	for (;;) {
		int status = (self->unsynced != 0) ?
			swDepTsConvResync(self, ptrIO, remIO) :
			swDepTsConvProcessSynced(self, ptrIO, remIO);

		if (status != 0)
			return status;
		if (*remIO < self->inPktSize)
			return 0;
	}
}

static void swDepProcTsConvRun(struct SwDepProc *proc)
{
	struct SwDepProcTsConv *self = (struct SwDepProcTsConv *)proc;
	struct SwDepMsg *msg;

	for (;;) {
		if (self->inMsg == NULL) {
			msg = swDepMsgQueuePop(&self->queue);
			if (msg != NULL) {
				if (msg->type == 7u) {
					self->eosSeen = 1;
					swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
				} else if (msg->type >= 2u && msg->type <= 4u) {
					self->inMsg = msg;
					self->inPtr = (unsigned char *)msg->data;
					self->inRem = msg->len;
					self->inCci = msg->cci;
				} else {
					swDepMsgMgrFree(&self->base.stream->msgMgr, msg);
				}
			}
		}
		if (self->inMsg == NULL)
			break;
		if (self->inScratchLen != 0) {
			unsigned int need = self->inPktSize - self->inScratchLen;
			unsigned int take = (self->inRem >= need) ? need : self->inRem;

			memcpy(self->inScratchPtr, self->inPtr, take);
			self->inScratchPtr += take;
			self->inScratchLen += take;
			self->inPtr += take;
			self->inRem -= take;
			if (self->inScratchLen == self->inPktSize)
				swDepTsConvProcessSpan(self, &self->inScratchPtr,
						       &self->inScratchLen);
		}
		if (self->inRem != 0) {
			swDepTsConvProcessSpan(self, &self->inPtr, &self->inRem);
			if (self->inRem != 0 && self->inRem < self->inPktSize) {
				memcpy(self->inScratch, self->inPtr, self->inRem);
				self->inScratchPtr = self->inScratch;
				self->inScratchLen = self->inRem;
				self->inRem = 0;
			}
		}
		if (self->inRem == 0) {
			swDepMsgMgrFree(&self->base.stream->msgMgr, self->inMsg);
			self->inMsg = NULL;
		}
	}

	if (self->eosSeen != 0 && self->base.isEos == 0) {
		if (self->inScratchLen != 0)
			self->inScratchLen = 0;
		if (self->outMsg != NULL)
			swDepTsConvFlushOut(self);
		swDepProcPushEos(self->base.nextProc);
	}
}

static int swDepProcTsConvCtrl(struct SwDepProc *proc, int cmd, void *arg)
{
	(void)proc;
	(void)cmd;
	(void)arg;
	return 0;
}

static void swDepProcTsConvDump(struct SwDepProc *proc)
{
	swDepProcDumpHeader(proc);
}

static const struct SwDepProcVtable g_swDepProcTsConvVtable = {
	swDepProcTsConvName, swDepProcTsConvInit, swDepProcTsConvFini,
	swDepProcTsConvOpen, swDepProcTsConvClose, swDepProcTsConvPush,
	swDepProcTsConvRun, swDepProcTsConvCtrl, swDepProcTsConvDump,
};

const struct SwDepProcVtable *swDepProcTsConvVtable(void)
{
	return &g_swDepProcTsConvVtable;
}

/* ---- Shared AES engine (SwDepAesProc / addreq / endreq / hw) ---- */

static void swDepAesAddReqDump(const struct SwDepAesAddReq *add)
{
	swLogPrintf("sw dep:  <addreq>\n");
	swLogPrintf("sw dep:    is_eosmsg    = %d\n", add->isEosMsg);
	swLogPrintf("sw dep:    is_eos       = %d\n", add->isEos);
	swLogPrintf("sw dep:    is_hdr_active= %d\n", add->isHdrActive);
	swLogPrintf("sw dep:    hdr_src      = %p\n", add->hdrSrc);
	swLogPrintf("sw dep:    hdr_srclen   = %d\n", add->hdrSrcLen);
	swLogPrintf("sw dep:    src          = %p\n", add->src);
	swLogPrintf("sw dep:    srclen       = %d\n", add->srcLen);
	swLogPrintf("sw dep:    dst          = %p\n", add->dst);
	swLogPrintf("sw dep:    dstlen       = %d\n", add->dstLen);
	swLogPrintf("sw dep:    tmpsrc       = %p\n", add->tmpSrc);
	swLogPrintf("sw dep:    tmpsrclen    = %d\n", add->tmpSrcLen);
	swLogPrintf("sw dep:    tmpdst       = %p\n", add->tmpDst);
	swLogPrintf("sw dep:    tmpdstlen    = %d\n", add->tmpDstLen);
	swLogPrintf("sw dep:    maxlen       = %d\n", add->maxLen);
	swLogPrintf("sw dep:    chunklen     = %d\n", add->chunkLen);
}

static void swDepAesEndReqDump(const struct SwDepAesEndReq *end)
{
	swLogPrintf("sw dep:  <endreq>\n");
	swLogPrintf("sw dep:    srclen       = %d\n", end->srcLen);
	swLogPrintf("sw dep:    dst          = %p\n", end->dst);
	swLogPrintf("sw dep:    dstlen       = %d\n", end->dstLen);
	swLogPrintf("sw dep:    req          = %p\n", end->req);
	swLogPrintf("sw dep:    reqdst       = %p\n", end->reqDst);
	swLogPrintf("sw dep:    reqlen       = %d\n", end->reqLen);
	swLogPrintf("sw dep:    reqpadding   = %d\n", end->reqPadding);
	swLogPrintf("sw dep:    to_tmpdst    = %d\n", end->toTmpDst);
}

static int swDepAesHwNotify(struct SwDepAesHw *hw, int count, void *ctx)
{
	struct SwDepAesProc *eng = (struct SwDepAesProc *)ctx;

	(void)hw;
	swDepMutexLock(&eng->mutex);
	eng->notifyCount += (unsigned int)count;
	swDepMutexUnlock(&eng->mutex);
	return swDepStreamWake(eng->proc->stream);
}

static void swDepAesHwInit(struct SwDepAesHw *hw, int isEncrypt,
			   int (*notify)(struct SwDepAesHw *, int, void *), void *ctx)
{
	memset(hw, 0, sizeof(*hw));
	hw->isEncrypt = isEncrypt;
	hw->notify = notify;
	hw->notifyCtx = ctx;
}

static void swDepAesHwFini(struct SwDepAesHw *hw)
{
	(void)hw;
}

static int swDepAesHwOpen(struct SwDepAesHw *hw)
{
	(void)hw;
	return 0;
}

static void swDepAesHwClose(struct SwDepAesHw *hw)
{
	(void)hw;
}

static void swDepAesHwSetKey(struct SwDepAesHw *hw, unsigned char enabled,
			     const void *key, const void *iv)
{
	hw->enabled = enabled;
	memcpy(hw->key, key, 16);
	memcpy(hw->baseIv, iv, 16);
	memcpy(hw->curIv, iv, 16);
}

static void swDepAesHwResetIv(struct SwDepAesHw *hw)
{
	memcpy(hw->curIv, hw->baseIv, 16);
}

static int swDepAesHwSubmit(struct SwDepAesHw *hw, struct SwDepAesReq *req)
{
	int status = 0;

	if (hw->enabled == 1u) {
		if (hw->isEncrypt != 0) {
			status = swDepAes128CbcEncrypt(req->src, req->dst, req->len,
						       hw->key, hw->curIv);
			memcpy(hw->curIv, req->dst + req->len - 16u, 16);
		} else {
			status = swDepAes128CbcDecrypt(req->src, req->dst, req->len,
						       hw->key, hw->curIv);
			memcpy(hw->curIv, req->src + req->len - 16u, 16);
		}
	} else {
		memcpy(req->dst, req->src, req->len);
	}
	hw->notify(hw, 1, hw->notifyCtx);
	return status;
}

static void swDepAesReqRingInit(struct SwDepAesReqRing *ring)
{
	ring->head = ring->items;
	ring->tail = ring->items;
	ring->end = ring->items + 16;
	ring->count = 0;
}

static void swDepAesAddReqInit(struct SwDepAesAddReq *add, struct SwDepAesProc *owner)
{
	memset(add, 0, sizeof(*add));
	add->owner = owner;
}

static void swDepAesEndReqInit(struct SwDepAesEndReq *end, struct SwDepAesProc *owner)
{
	memset(end, 0, sizeof(*end));
	end->owner = owner;
}

static void swDepAesEngineInit(struct SwDepAesProc *eng, struct SwDepProc *proc,
			       int isEncrypt, int isPcp)
{
	memset(eng, 0, sizeof(*eng));
	swDepMutexInit(&eng->mutex);
	swDepAesHwInit(&eng->hw, isEncrypt, swDepAesHwNotify, eng);
	swDepAesReqRingInit(&eng->ring);
	swDepAesAddReqInit(&eng->addReq, eng);
	swDepAesEndReqInit(&eng->endReq, eng);
	eng->state = 0;
	eng->isEncrypt = isEncrypt;
	eng->isPcp = isPcp;
	eng->proc = proc;
}

static int swDepAesEngineFini(struct SwDepAesProc *eng)
{
	swDepAesHwFini(&eng->hw);
	return swDepMutexDestroy(&eng->mutex);
}

static int swDepAesEngineOpen(struct SwDepAesProc *eng, unsigned char dstQueueId, void *config)
{
	const unsigned char *cfg = (const unsigned char *)config;
	int status;

	eng->dstQueueId = dstQueueId;
	eng->state = 0;
	eng->reqCount = 0;
	eng->notifyCount = 0;
	swDepMsgQueueInit(&eng->srcQueue);
	swDepMsgQueueInit(&eng->reqSrcQ);
	swDepMsgQueueInit(&eng->reqDstQ);
	status = swDepAesHwOpen(&eng->hw);
	if (status != 0)
		return status;
	swDepAesReqRingInit(&eng->ring);
	eng->addReq.cci = cfg[8];
	eng->addReq.isEos = 0;
	eng->addReq.isEosMsg = 0;
	eng->addReq.isHdrActive = 0;
	eng->addReq.hdrSrc = NULL;
	eng->addReq.hdrSrcLen = 0;
	eng->addReq.curReq = NULL;
	eng->addReq.srcMsg = NULL;
	eng->addReq.src = NULL;
	eng->addReq.srcLen = 0;
	eng->addReq.dstMsg = NULL;
	eng->addReq.dst = NULL;
	eng->addReq.dstLen = 0;
	eng->addReq.nextDstMsg = NULL;
	eng->addReq.tmpSrc = NULL;
	eng->addReq.tmpSrcLen = 0;
	eng->addReq.tmpDst = NULL;
	eng->addReq.tmpDstLen = 0;
	eng->addReq.maxLen = 0;
	eng->addReq.srcLimit = 0;
	eng->addReq.dstLimit = 0;
	eng->addReq.chunkLen = 0;
	eng->endReq.srcMsg = NULL;
	eng->endReq.srcLen = 0;
	eng->endReq.dstMsg = NULL;
	eng->endReq.dst = NULL;
	eng->endReq.dstLen = 0;
	eng->endReq.totalOut = 0;
	eng->endReq.req = NULL;
	eng->endReq.reqDst = NULL;
	eng->endReq.reqLen = 0;
	eng->endReq.reqPadding = 0;
	eng->endReq.toTmpDst = 0;
	eng->endReq.markFirst = 1;
	return 0;
}

static void swDepAesEngineSetKey(struct SwDepAesProc *eng, unsigned char enabled,
				 const void *key, const void *iv)
{
	swDepAesHwSetKey(&eng->hw, enabled, key, iv);
	eng->addReq.chunkLen = 0;
}

static void swDepAesEngineSetLen(struct SwDepAesProc *eng, unsigned int len)
{
	eng->addReq.maxLen = len;
	eng->addReq.srcLimit = len;
	eng->addReq.dstLimit = len;
	if (eng->isPcp != 0) {
		unsigned int rem = len & 0xFu;

		if (rem != 0) {
			if (eng->isEncrypt != 0)
				eng->addReq.dstLimit = eng->addReq.dstLimit - rem + 16u;
			else
				eng->addReq.srcLimit = eng->addReq.srcLimit - rem + 16u;
		}
	}
	eng->addReq.chunkLen = 0;
}

static void swDepAesEngineResetChunk(struct SwDepAesProc *eng)
{
	eng->addReq.chunkLen = 0;
}

static unsigned char swDepAesEngineGetCci(const struct SwDepAesProc *eng)
{
	return eng->addReq.cci;
}

static void swDepAesEngineSetCci(struct SwDepAesProc *eng, unsigned char cci)
{
	eng->addReq.cci = cci;
}

static void swDepAesEngineResetIv(struct SwDepAesProc *eng)
{
	swDepAesHwResetIv(&eng->hw);
	eng->addReq.chunkLen = 0;
}

static void swDepAesEnginePush(struct SwDepAesProc *eng, struct SwDepMsg *msg)
{
	swDepMsgQueuePushTail(&eng->srcQueue, msg);
}

static unsigned int swDepAesEngineLastOutLen(const struct SwDepAesProc *eng)
{
	if (eng->addReq.chunkLen != 0)
		return eng->addReq.maxLen;
	return eng->addReq.chunkLen;
}

static int swDepAesEngineState(const struct SwDepAesProc *eng)
{
	return eng->state;
}

static void swDepAesEngineStepIdle(struct SwDepAesProc *eng, int *cont)
{
	if (swDepMsgQueuePeek(&eng->srcQueue) != NULL) {
		eng->state = 1;
		*cont = 1;
	} else {
		*cont = 0;
	}
}

static void swDepAesEngineStepWaitHdr(struct SwDepAesProc *eng, int *cont)
{
	struct SwDepStream *stream = eng->proc->stream;
	struct SwDepMsg *msg;

	if (eng->addReq.srcMsg != NULL && (eng->isPcp == 0 || eng->isEncrypt != 0)) {
		eng->state = 2;
		return;
	}
	msg = swDepMsgQueuePeek(&eng->srcQueue);
	if (msg == NULL) {
		if (eng->addReq.isEos != 0)
			eng->state = 5;
		else
			*cont = 0;
		return;
	}
	if (msg->type == 7u) {
		msg = swDepMsgQueuePop(&eng->srcQueue);
		eng->addReq.isEosMsg = 1;
		swDepMsgMgrFree(&stream->msgMgr, msg);
		eng->state = 2;
	} else if (msg->type == 1u) {
		msg = swDepMsgQueuePop(&eng->srcQueue);
		memcpy(eng->hdrInfo, msg->data, 56);
		swDepMsgMgrFree(&stream->msgMgr, msg);
	} else {
		eng->state = 2;
	}
}

static void swDepAesEngineStepStart(struct SwDepAesProc *eng, const void *hdr, size_t hdrLen,
				    int *cont)
{
	(void)cont;
	eng->state = 3;
	if (hdrLen != 0) {
		memcpy(eng->addReq.hdrBuf, hdr, hdrLen);
		eng->addReq.hdrSrc = eng->addReq.hdrBuf;
		eng->addReq.hdrSrcLen = (unsigned int)hdrLen;
		eng->addReq.isHdrActive = 1;
	}
	eng->endReq.markFirst = 1;
}

static void swDepAesAddReqSetDst(struct SwDepAesAddReq *add, struct SwDepMsg *msg)
{
	if (msg != NULL) {
		add->dstMsg = msg;
		add->dst = (unsigned char *)msg->data;
		add->dstLen = msg->bufLen;
		msg->cci = add->cci;
	} else {
		add->dstMsg = NULL;
		add->dst = NULL;
		add->dstLen = 0;
	}
}

static int swDepAesAddReqEnsureDst(struct SwDepAesAddReq *add)
{
	struct SwDepAesProc *eng = add->owner;
	struct SwDepStream *stream = eng->proc->stream;
	int allocated = 0;
	struct SwDepMsg *msg;

	if (add->dstMsg == NULL) {
		if (add->nextDstMsg != NULL) {
			swDepAesAddReqSetDst(add, add->nextDstMsg);
			add->nextDstMsg = NULL;
			allocated = 1;
		} else {
			msg = swDepMsgMgrAlloc(&stream->msgMgr, eng->dstQueueId);
			if (msg != NULL) {
				swDepAesAddReqSetDst(add, msg);
				swDepMsgQueuePushTail(&eng->reqDstQ, msg);
				allocated = 1;
			}
		}
	}
	if (add->nextDstMsg == NULL) {
		msg = swDepMsgMgrAlloc(&stream->msgMgr, eng->dstQueueId);
		if (msg != NULL) {
			add->nextDstMsg = msg;
			swDepMsgQueuePushTail(&eng->reqDstQ, msg);
			return 1;
		}
	}
	return allocated;
}

static int swDepAesAddReqCopyHdr(struct SwDepAesAddReq *add, int *cont)
{
	do {
		unsigned int take;

		if (add->dstMsg == NULL) {
			swDepAesAddReqEnsureDst(add);
			if (add->dstMsg == NULL) {
				*cont = 0;
				return 0;
			}
		}
		take = (add->hdrSrcLen >= add->dstLen) ? add->dstLen : add->hdrSrcLen;
		memcpy(add->dst, add->hdrSrc, take);
		add->hdrSrc += take;
		add->hdrSrcLen -= take;
		add->dst += take;
		add->dstLen -= take;
		if (add->dstLen == 0) {
			add->dstMsg->len = add->dstMsg->bufLen;
			swDepAesAddReqSetDst(add, NULL);
		}
	} while (add->hdrSrcLen != 0);

	add->dstMsg->len = add->dstMsg->bufLen - add->dstLen;
	add->isHdrActive = 0;
	*cont = 1;
	return 1;
}

static void swDepAesEndReqEmitMsg(struct SwDepAesEndReq *end, struct SwDepMsg *msg)
{
	if (end->markFirst != 0) {
		msg->flags |= 1u;
		end->markFirst = 0;
	}
	swDepProcPush(end->owner->proc->nextProc, msg);
}

static void swDepAesEndReqDrain(struct SwDepAesEndReq *end)
{
	struct SwDepAesProc *eng = end->owner;
	struct SwDepAesReqRing *ring = &eng->ring;
	struct SwDepStream *stream = eng->proc->stream;

	for (;;) {
		while (end->reqLen + end->reqPadding != 0) {
			unsigned int avail;

			if (end->srcMsg == NULL) {
				struct SwDepMsg *s = swDepMsgQueuePop(&eng->reqSrcQ);

				if (s != NULL) {
					end->srcMsg = s;
					end->srcLen = s->len;
				}
			}
			if (end->dstMsg == NULL) {
				struct SwDepMsg *d = swDepMsgQueuePop(&eng->reqDstQ);

				if (d != NULL) {
					end->dstMsg = d;
					end->dst = (unsigned char *)d->data + d->len;
					end->dstLen = d->bufLen - d->len;
				}
			}
			if (end->srcLen != 0)
				avail = (end->srcLen >= end->dstLen) ? end->dstLen : end->srcLen;
			else
				avail = end->dstLen;

			if (end->reqLen != 0) {
				unsigned int take = (avail >= end->reqLen) ? end->reqLen : avail;

				if (end->toTmpDst != 0)
					memcpy(end->dst, end->reqDst, take);
				if (end->srcLen != 0)
					end->srcLen -= take;
				end->dstLen -= take;
				end->dst += take;
				end->reqLen -= take;
				end->reqDst += take;
			} else if (end->reqPadding != 0) {
				unsigned int take = (avail >= end->reqPadding) ?
					end->reqPadding : avail;

				if (eng->isEncrypt != 0) {
					if (end->toTmpDst != 0)
						memcpy(end->dst, end->reqDst, take);
					end->dstLen -= take;
					end->dst += take;
				} else if (end->srcLen != 0) {
					end->srcLen -= take;
				}
				end->reqPadding = (unsigned char)(end->reqPadding - take);
			}

			if (end->srcLen == 0 && end->srcMsg != NULL) {
				swDepMsgMgrFree(&stream->msgMgr, end->srcMsg);
				end->srcMsg = NULL;
			}
			if (end->dstLen == 0 && end->dstMsg != NULL) {
				struct SwDepMsg *doneMsg = end->dstMsg;

				doneMsg->len = doneMsg->bufLen;
				swDepAesEndReqEmitMsg(end, doneMsg);
				end->dstMsg = NULL;
			}
			if (end->reqLen + end->reqPadding == 0) {
				--eng->reqCount;
				--ring->count;
				++ring->head;
				if (ring->head == ring->end)
					ring->head = ring->items;
			}
		}

		swDepMutexLock(&eng->mutex);
		if (eng->notifyCount != 0) {
			struct SwDepAesReq *req;

			--eng->notifyCount;
			swDepMutexUnlock(&eng->mutex);
			req = (ring->count != 0) ? ring->head : NULL;
			end->req = req;
			end->reqLen = req->len - req->padding;
			end->reqDst = req->dst;
			end->reqPadding = req->padding;
			end->toTmpDst = (req->dst == req->tmpDstBuf) ? 1 : 0;
			end->totalOut += end->reqLen;
		} else {
			swDepMutexUnlock(&eng->mutex);
			break;
		}
	}
}

static void swDepAesAddReqSetSrc(struct SwDepAesAddReq *add, struct SwDepMsg *msg)
{
	if (msg != NULL) {
		add->srcMsg = msg;
		add->src = (unsigned char *)msg->data;
		add->srcLen = msg->len;
		if (msg->cci != 255u)
			add->cci = msg->cci;
	} else {
		add->srcMsg = NULL;
		add->src = NULL;
		add->srcLen = 0;
	}
}

static void swDepAesAddReqClearDst(struct SwDepAesAddReq *add)
{
	if (add->dstMsg != NULL) {
		add->dstMsg = NULL;
		add->dst = NULL;
		add->dstLen = 0;
	}
}

static void swDepAesAddReqConsumeTmpDst(struct SwDepAesAddReq *add)
{
	unsigned int take = (add->tmpDstLen >= add->dstLen) ? add->dstLen : add->tmpDstLen;

	add->dst += take;
	add->dstLen -= take;
	add->tmpDstLen -= take;
}

static int swDepAesAddReqBuildOne(struct SwDepAesAddReq *add)
{
	struct SwDepAesProc *eng = add->owner;
	unsigned int dstRem = add->dstLimit - add->chunkLen;
	unsigned int srcRem = add->srcLimit - add->chunkLen - add->tmpSrcLen;
	unsigned int srcAvail = (srcRem >= add->srcLen) ? add->srcLen : srcRem;
	unsigned int dstAvail = (dstRem >= add->dstLen) ? add->dstLen : dstRem;
	int needPad = 0;

	if ((add->srcLen == 0 && add->isEosMsg != 0) ||
	    (eng->isEncrypt != 0 && eng->isPcp != 0 && add->srcLimit - add->chunkLen == add->tmpSrcLen))
		needPad = 1;

	if (add->tmpSrcLen == 16u) {
		struct SwDepAesReq *req = add->curReq;

		if (dstAvail < 16u) {
			unsigned int valid;

			if (dstAvail == 0)
				return 0;
			if (dstAvail != dstRem && add->nextDstMsg == NULL)
				return 0;
			req->src = req->tmpSrcBuf;
			req->dst = req->tmpDstBuf;
			req->len = 16u;
			if (req->padding != 0) {
				valid = 16u - req->padding;
			} else {
				valid = 16u;
				if (add->chunkLen + 16u > add->maxLen) {
					valid = add->maxLen - add->chunkLen;
					req->padding = (unsigned char)(16u - valid);
				}
			}
			add->chunkLen += valid;
			add->tmpSrcLen = 0;
			add->tmpDstLen = valid;
			swDepAesAddReqConsumeTmpDst(add);
			if (dstAvail != dstRem) {
				swDepAesAddReqClearDst(add);
				swDepAesAddReqEnsureDst(add);
				swDepAesAddReqConsumeTmpDst(add);
			}
		} else {
			unsigned int valid;

			req->src = req->tmpSrcBuf;
			req->dst = add->dst;
			req->len = 16u;
			if (req->padding != 0) {
				valid = 16u - req->padding;
			} else {
				valid = 16u;
				if (add->chunkLen + 16u > add->maxLen)
					valid = add->maxLen - add->chunkLen;
				req->padding = (unsigned char)(16u - valid);
			}
			add->chunkLen += valid;
			add->dst += valid;
			add->dstLen -= valid;
			add->tmpSrcLen = 0;
		}
		return 1;
	}

	if (add->tmpSrcLen != 0) {
		if (srcAvail != 0) {
			unsigned int take = (16u - add->tmpSrcLen >= srcAvail) ?
				srcAvail : (16u - add->tmpSrcLen);

			memcpy(add->tmpSrc, add->src, take);
			add->src += take;
			add->srcLen -= take;
			add->tmpSrc += take;
			add->tmpSrcLen += take;
			return 1;
		}
		if (needPad != 0) {
			unsigned int padLen = 16u - add->tmpSrcLen;

			memset(add->tmpSrc, 0, padLen);
			add->tmpSrc += padLen;
			add->tmpSrcLen += padLen;
			add->curReq->padding = (unsigned char)padLen;
			return 1;
		}
		return 0;
	}

	if (srcAvail < 16u) {
		unsigned int take;

		if (srcAvail == 0)
			return 0;
		take = (16u - add->tmpSrcLen >= srcAvail) ? srcAvail : (16u - add->tmpSrcLen);
		memcpy(add->tmpSrc, add->src, take);
		add->src += take;
		add->srcLen -= take;
		add->tmpSrc += take;
		add->tmpSrcLen += take;
		return 1;
	}

	if (dstAvail < 16u) {
		struct SwDepAesReq *req = add->curReq;
		unsigned int valid = 16u;

		if (dstAvail != dstRem && add->nextDstMsg == NULL)
			return 0;
		req->src = add->src;
		req->dst = req->tmpDstBuf;
		req->len = 16u;
		add->src += 16u;
		add->srcLen -= 16u;
		if (add->chunkLen + 16u > add->maxLen) {
			valid = add->maxLen - add->chunkLen;
			req->padding = (unsigned char)(16u - valid);
		}
		add->chunkLen += valid;
		add->tmpDstLen = valid;
		swDepAesAddReqConsumeTmpDst(add);
		if (dstAvail != dstRem) {
			swDepAesAddReqClearDst(add);
			swDepAesAddReqEnsureDst(add);
			swDepAesAddReqConsumeTmpDst(add);
		}
		return 1;
	}

	{
		unsigned int blocks = ((dstAvail >> 4) <= (srcAvail >> 4)) ?
			((dstAvail >> 4) << 4) : ((srcAvail >> 4) << 4);
		struct SwDepAesReq *req = add->curReq;

		req->src = add->src;
		req->dst = add->dst;
		req->len = blocks;
		add->chunkLen += blocks;
		add->src += blocks;
		add->srcLen -= blocks;
		add->dst += blocks;
		add->dstLen -= blocks;
	}
	return 1;
}

static void swDepAesAddReqPump(struct SwDepAesAddReq *add, int *cont)
{
	struct SwDepAesProc *eng = add->owner;
	struct SwDepAesReqRing *ring = &eng->ring;
	struct SwDepStream *stream = eng->proc->stream;

	if (*cont == 0)
		return;
	for (;;) {
		int gotSrc = 0;
		int gotDst;
		int built;
		int chunkDone;
		int eosDone;

		if (add->chunkLen != 0 && add->chunkLen >= add->maxLen) {
			*cont = 0;
			return;
		}
		if (add->srcMsg == NULL) {
			struct SwDepMsg *msg = swDepMsgQueuePop(&eng->srcQueue);

			if (msg != NULL) {
				if (msg->type == 7u) {
					add->isEosMsg = 1;
					swDepMsgMgrFree(&stream->msgMgr, msg);
					gotSrc = 1;
				} else if (msg->type >= 2u && msg->type <= 4u) {
					swDepAesAddReqSetSrc(add, msg);
					swDepMsgQueuePushTail(&eng->reqSrcQ, msg);
					gotSrc = 1;
				} else {
					swDepMsgMgrFree(&stream->msgMgr, msg);
				}
			}
		}
		gotDst = swDepAesAddReqEnsureDst(add);
		if (add->tmpDstLen == 0 && add->curReq == NULL) {
			struct SwDepAesReq *slot = (ring->count == 16u) ? NULL : ring->tail;

			if (slot == NULL) {
				*cont = 0;
				return;
			}
			add->curReq = slot;
			slot->len = 0;
			slot->padding = 0;
			slot->src = NULL;
			slot->dst = NULL;
			add->tmpSrc = slot->tmpSrcBuf;
			add->tmpDst = slot->tmpDstBuf;
			add->tmpSrcLen = 0;
			add->tmpDstLen = 0;
		}
		built = swDepAesAddReqBuildOne(add);
		if (add->srcMsg != NULL && add->srcLen == 0)
			swDepAesAddReqSetSrc(add, NULL);
		chunkDone = (add->chunkLen >= add->maxLen);
		eosDone = (add->isEosMsg != 0 && add->srcMsg == NULL &&
			   add->tmpSrcLen == 0 && add->tmpDstLen == 0);
		if (add->dstMsg != NULL) {
			if ((eng->isPcp != 0 && eng->isEncrypt != 0 && chunkDone) || eosDone) {
				add->dstMsg->pktInfo = (add->chunkLen < add->maxLen) ?
					add->chunkLen : add->maxLen;
				swDepAesAddReqClearDst(add);
			} else if (add->dstLen == 0) {
				swDepAesAddReqClearDst(add);
			}
		}
		if (add->curReq != NULL && add->curReq->len != 0) {
			struct SwDepAesReq *submitted = add->curReq;

			++ring->count;
			++ring->tail;
			if (ring->tail == ring->end)
				ring->tail = ring->items;
			swDepAesHwSubmit(&eng->hw, submitted);
			++eng->reqCount;
			add->curReq = NULL;
		}
		if (eosDone) {
			add->isEos = 1;
			return;
		}
		if (chunkDone)
			return;
		if ((gotDst | gotSrc | built) == 0)
			break;
		if (*cont == 0)
			return;
	}
	*cont = 0;
}

static void swDepAesEngineStepRun(struct SwDepAesProc *eng, int *cont)
{
	do {
		if (eng->addReq.isHdrActive != 0) {
			swDepAesAddReqCopyHdr(&eng->addReq, cont);
		} else {
			if (eng->notifyCount != 0)
				swDepAesEndReqDrain(&eng->endReq);
			if (eng->addReq.isEos == 0) {
				swDepAesAddReqPump(&eng->addReq, cont);
				if (eng->notifyCount != 0)
					*cont = 1;
			} else {
				*cont = 0;
			}
		}
	} while (*cont != 0);

	if (eng->reqCount == 0) {
		if (eng->addReq.isEos != 0) {
			eng->state = 5;
			*cont = 1;
		} else if (eng->addReq.chunkLen >= eng->addReq.maxLen) {
			eng->state = 4;
			*cont = 1;
		}
	}
}

static void swDepAesEndReqFlushPartial(struct SwDepAesEndReq *end)
{
	if (end->dstMsg != NULL) {
		struct SwDepMsg *msg = end->dstMsg;

		msg->len = msg->bufLen - end->dstLen;
		swDepAesEndReqEmitMsg(end, msg);
		end->dstLen = 0;
		end->dstMsg = NULL;
	}
}

static void swDepAesEngineStepFlush(struct SwDepAesProc *eng, int flushDst, int *cont)
{
	if (flushDst != 0) {
		swDepAesAddReqClearDst(&eng->addReq);
		swDepAesEndReqFlushPartial(&eng->endReq);
	}
	eng->state = (eng->addReq.isEos != 0) ? 5 : 1;
	*cont = 1;
}

static void swDepAesEngineStepEos(struct SwDepAesProc *eng, int *cont)
{
	swDepAesAddReqClearDst(&eng->addReq);
	swDepAesEndReqFlushPartial(&eng->endReq);
	swDepProcPushEos(eng->proc->nextProc);
	eng->proc->isEos = 1;
	eng->state = 6;
	*cont = 1;
}

static void swDepAesEngineDump(struct SwDepAesProc *eng)
{
	swLogPrintf("sw dep:<aesproc>\n");
	swLogPrintf("sw dep:  state          = %s\n", "");
	swLogPrintf("sw dep:  req_count      = %d\n", eng->reqCount);
	swLogPrintf("sw dep:  notify_count   = %d\n", eng->notifyCount);
	swLogPrintf("sw dep:  <mqueue_src>\n");
	swLogPrintf("sw dep:  <mqueue_reqsrc>\n");
	swLogPrintf("sw dep:  <mqueue_reqdst>\n");
	swDepAesAddReqDump(&eng->addReq);
	swDepAesEndReqDump(&eng->endReq);
}
