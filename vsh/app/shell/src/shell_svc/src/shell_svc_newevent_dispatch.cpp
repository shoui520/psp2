/*
 * SceShell shell_svc: SceNewEventDb sync/async dispatch & response helpers
 * 3.65-CEX: 0x811C3F9C - 0x811C4554
 */

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>

#include "shell_svc_newevent.h"

extern int shellSvcNewEventEnqueueAdd(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, const char *groupId, SceInt64 eventId,
	int type, int f28, SceUInt8 f2c, SceUInt8 f2d,
	const char *f30, int f3c, int f40, const char **args, SceUInt32 argCount,
	const char *text1, const char *text2, int fC8,
	const char *fCC, const char *fD8, const char *fE4,
	int fF0, float fF4, int fFC, SceUInt8 flag);		/* 0x811C31D4 */
extern int shellSvcNewEventBatchPush(IPMI::Session *session,
	const char *titleId, const char *groupId, SceInt64 eventId,
	int type, int f28, SceUInt8 f2c, SceUInt8 f2d,
	const char *f30, int f3c, int f40, const char **args, SceUInt32 argCount,
	const char *text1, const char *text2, int fC8,
	const char *fCC, const char *fD8, const char *fE4,
	int fF0, float fF4, int fFC);				/* 0x811C34A2 */
extern int shellSvcNewEventEnqueueBatchCommit(IPMI::Session *session, SceUInt32 reqId, int flags); /* 0x811C372C */
extern int shellSvcNewEventEnqueueDelete(IPMI::Session *session, SceUInt32 reqId,
	const char **titleIds, const char **groupIds, const SceInt64 *eventIds, int count); /* 0x811C3868 */
extern int shellSvcNewEventEnqueueSendSimple(IPMI::Session *session, SceUInt32 reqId,
	const void *payload, SceUInt8 flag);			/* 0x811C3A78 */
extern int shellSvcNewEventEnqueueClear(IPMI::Session *session, SceUInt32 reqId, SceUInt8 flag); /* 0x811C3BBC */
extern int shellSvcNewEventEnqueueProgStart(IPMI::Session *session, SceUInt32 reqId,
	const void *payload, SceUInt8 flag);			/* 0x811C3CC2 */
extern int shellSvcNewEventSyncUpdateProg(IPMI::Session *session, const void *payload); /* 0x811C3E06 */
extern int shellSvcNewEventEnqueueProgFinish(IPMI::Session *session, SceUInt32 reqId, const void *payload); /* 0x811C3E10 */

struct NewEventAddReqHdr {
	SceInt64 eventId;					/* +0 */
	int type;						/* +8 */
	int f28;						/* +12 */
	SceUInt8 f2c;						/* +16 */
	SceUInt8 f2d;						/* +17 */
	SceUInt16 pad;						/* +18 */
	int f40;						/* +20 */
	SceUInt32 strCount;					/* +24 */
	SceUInt32 argCount;					/* +28 */
	int fC8;						/* +32 */
	int fF0;						/* +36 */
	float fF4;						/* +40 */
	int fFC;						/* +44 */
	SceUInt8 flag;						/* +48 */
};

/* 0x811C3F9C */
int shellSvcNewEventHandleAdd(IPMI::Session *session, SceUInt32 reqId, const IPMI::DataInfo *inBufs)
{
	const NewEventAddReqHdr *h = (const NewEventAddReqHdr *)inBufs[0].pData;
	const char *titleId = (const char *)inBufs[1].pData;
	const char *groupId = (const char *)inBufs[2].pData;
	const char *f30 = (const char *)inBufs[3].pData;
	int f3c = (int)(SceUIntPtr)inBufs[4].pData;
	const char *args[16];
	SceUInt32 idx = 5;
	for (SceUInt32 i = 0; i < h->strCount && i < 16; ++i, ++idx)
		args[i] = (const char *)inBufs[idx].pData;
	const char *text1 = (const char *)inBufs[idx].pData;
	const char *text2 = (const char *)inBufs[idx + 1].pData;
	const char *fCC = (const char *)inBufs[idx + 2].pData;
	const char *fD8 = (const char *)inBufs[idx + 3].pData;
	const char *fE4 = (const char *)inBufs[idx + 4].pData;
	return shellSvcNewEventEnqueueAdd(session, reqId, titleId, groupId, h->eventId,
		h->type, h->f28, h->f2c, h->f2d, f30, f3c, h->f40, args, h->argCount,
		text1, text2, h->fC8, fCC, fD8, fE4, h->fF0, h->fF4, h->fFC, h->flag);
}

/* 0x811C4108 */
int shellSvcNewEventSendAddResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const SceInt64 *eventId, const int *f1, const int *f2)
{
	IPMI::DataInfo out[3];
	out[0].pData = eventId;
	out[0].dataSize = 8;
	out[1].pData = f1;
	out[1].dataSize = 4;
	out[2].pData = f2;
	out[2].dataSize = 4;
	return session->respondToAsyncMethodRequest(0x70001, reqId, err, out, 3);
}

/* 0x811C4158 */
int shellSvcNewEventHandleBatchPush(IPMI::Session *session, const IPMI::DataInfo *inBufs,
	SceUInt32 inCount, IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	const NewEventAddReqHdr *h = (const NewEventAddReqHdr *)inBufs[0].pData;
	const char *titleId = (const char *)inBufs[1].pData;
	const char *groupId = (const char *)inBufs[2].pData;
	const char *f30 = (const char *)inBufs[3].pData;
	int f3c = (int)(SceUIntPtr)inBufs[4].pData;
	const char *args[16];
	SceUInt32 idx = 5;
	for (SceUInt32 i = 0; i < h->strCount && i < 16; ++i, ++idx)
		args[i] = (const char *)inBufs[idx].pData;
	const char *text1 = (const char *)inBufs[idx].pData;
	const char *text2 = (const char *)inBufs[idx + 1].pData;
	const char *fCC = (const char *)inBufs[idx + 2].pData;
	const char *fD8 = (const char *)inBufs[idx + 3].pData;
	const char *fE4 = (const char *)inBufs[idx + 4].pData;
	int r = shellSvcNewEventBatchPush(session, titleId, groupId, h->eventId,
		h->type, h->f28, h->f2c, h->f2d, f30, f3c, h->f40, args, h->argCount,
		text1, text2, h->fC8, fCC, fD8, fE4, h->fF0, h->fF4, h->fFC);
	return session->respondToSyncMethodRequest(r, outBufs, outCount);
}

/* 0x811C42DE */
int shellSvcNewEventSendBatchResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x70004, reqId, err, NULL, 0);
}

/* 0x811C42FE */
int shellSvcNewEventHandleDelete(IPMI::Session *session, SceUInt32 reqId, const IPMI::DataInfo *inBufs)
{
	const SceUInt32 *hdr = (const SceUInt32 *)inBufs[0].pData;
	SceUInt32 n1 = hdr[0];
	SceUInt32 n2 = hdr[1];
	int count = (int)hdr[2];
	const char *titleIds[64];
	const char *groupIds[64];
	SceUInt32 idx = 1;
	for (SceUInt32 i = 0; i < n1 && i < 64; ++i, ++idx)
		titleIds[i] = (const char *)inBufs[idx].pData;
	for (SceUInt32 i = 0; i < n2 && i < 64; ++i, ++idx)
		groupIds[i] = (const char *)inBufs[idx].pData;
	const SceInt64 *eventIds = (const SceInt64 *)inBufs[idx].pData;
	return shellSvcNewEventEnqueueDelete(session, reqId, titleIds, groupIds, eventIds, count);
}

/* 0x811C43DC */
int shellSvcNewEventSendDeleteResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x70006, reqId, err, NULL, 0);
}

/* 0x811C43FE */
int shellSvcNewEventSendSimpleResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x70008, reqId, err, NULL, 0);
}

/* 0x811C441E */
int shellSvcNewEventSendClearResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x7000A, reqId, err, NULL, 0);
}

/* 0x811C4440 */
int shellSvcNewEventSendProgStartResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x7000C, reqId, err, NULL, 0);
}

/* 0x811C4460 */
int shellSvcNewEventSendProgFinishResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x7000F, reqId, err, NULL, 0);
}

/* 0x811C4480 */
int shellSvcNewEventDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	if (methodId == 0x7000D) {
		int r = shellSvcNewEventSyncUpdateProg(session, inBufs[0].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	if (methodId == 0x70002)
		return shellSvcNewEventHandleBatchPush(session, inBufs, inCount, outBufs, outCount);
	return (int)0x800205C1;
}

/* 0x811C44E2 */
int shellSvcNewEventDispatchAsync(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	(void)inCount;
	switch (methodId) {
	case 0x70000:
		return shellSvcNewEventHandleAdd(session, reqId, inBufs);
	case 0x70003:
		return shellSvcNewEventEnqueueBatchCommit(session, reqId, *(const int *)inBufs[0].pData);
	case 0x70005:
		return shellSvcNewEventHandleDelete(session, reqId, inBufs);
	case 0x70007:
		return shellSvcNewEventEnqueueSendSimple(session, reqId,
			inBufs[1].pData, *(const SceUInt8 *)inBufs[0].pData);
	case 0x70009:
		return shellSvcNewEventEnqueueClear(session, reqId, *(const SceUInt8 *)inBufs[0].pData);
	case 0x7000B:
		return shellSvcNewEventEnqueueProgStart(session, reqId,
			inBufs[1].pData, *(const SceUInt8 *)inBufs[0].pData);
	case 0x7000E:
		return shellSvcNewEventEnqueueProgFinish(session, reqId, inBufs[0].pData);
	default:
		return (int)0x800205C1;
	}
}
