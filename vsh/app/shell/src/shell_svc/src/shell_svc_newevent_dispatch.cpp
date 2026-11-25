/*
 * SceShell shell_svc: SceNewEventDb sync/async dispatch & response helpers
 * 3.65-CEX: 0x811C3F9C - 0x811C4554
 */

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>

#include "shell_svc_newevent.h"

extern int shellSvcNewEventEnqueueAdd(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, const char *itemId, SceInt64 rowid,
	int msgType, int actType, SceUInt8 newFlag, SceUInt8 popupNo,
	const char *iconPath, const void *iconData, int iconDataSize,
	const char **msgArgs, SceUInt32 argCount,
	const char *title, const char *desc, int execMode,
	const char *execTitleId, const char *execArg, const char *timeUpdated,
	int seId, float seVolume, int notifyFlags, SceUInt8 popupFlag); /* 0x811C31D4 */
extern int shellSvcNewEventBatchPush(IPMI::Session *session,
	const char *titleId, const char *itemId, SceInt64 rowid,
	int msgType, int actType, SceUInt8 newFlag, SceUInt8 popupNo,
	const char *iconPath, const void *iconData, int iconDataSize,
	const char **msgArgs, SceUInt32 argCount,
	const char *title, const char *desc, int execMode,
	const char *execTitleId, const char *execArg, const char *timeUpdated,
	int seId, float seVolume, int notifyFlags);		/* 0x811C34A2 */
extern int shellSvcNewEventEnqueueBatchCommit(IPMI::Session *session, SceUInt32 reqId, int flags); /* 0x811C372C */
extern int shellSvcNewEventEnqueueDelete(IPMI::Session *session, SceUInt32 reqId,
	const char **titleIds, const char **itemIds, const SceInt64 *rowIds, int count); /* 0x811C3868 */
extern int shellSvcNewEventEnqueueSendSimple(IPMI::Session *session, SceUInt32 reqId,
	const NewEventSimplePayload *payload, SceUInt8 resolveFlag); /* 0x811C3A78 */
extern int shellSvcNewEventEnqueueClear(IPMI::Session *session, SceUInt32 reqId, SceUInt8 resolveFlag); /* 0x811C3BBC */
extern int shellSvcNewEventEnqueueProgStart(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressStartPayload *payload, SceUInt8 resolveFlag); /* 0x811C3CC2 */
extern int shellSvcNewEventSyncUpdateProg(IPMI::Session *session,
	const NewEventProgressUpdatePayload *payload);		/* 0x811C3E06 */
extern int shellSvcNewEventEnqueueProgFinish(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressFinishPayload *payload);		/* 0x811C3E10 */

struct NewEventAddReqHdr {
	SceInt64 rowid;						/* +0 */
	int msgType;						/* +8 */
	int actType;						/* +12 */
	SceUInt8 newFlag;					/* +16 */
	SceUInt8 popupNo;					/* +17 */
	SceUInt16 pad;						/* +18 */
	int iconDataSize;					/* +20 */
	SceUInt32 strCount;					/* +24 */
	SceUInt32 argCount;					/* +28 */
	int execMode;						/* +32 */
	int seId;						/* +36 */
	float seVolume;						/* +40 */
	int notifyFlags;					/* +44 */
	SceUInt8 popupFlag;					/* +48 */
};

struct NewEventDeleteReqHdr {
	SceUInt32 titleCount;					/* +0 */
	SceUInt32 itemCount;					/* +4 */
	int keyCount;						/* +8 */
};

/* 0x811C3F9C */
int shellSvcNewEventHandleAdd(IPMI::Session *session, SceUInt32 reqId, const IPMI::DataInfo *inBufs)
{
	const NewEventAddReqHdr *hdr = static_cast<const NewEventAddReqHdr *>(inBufs[0].pData);
	const char *titleId = static_cast<const char *>(inBufs[1].pData);
	const char *itemId = static_cast<const char *>(inBufs[2].pData);
	const char *iconPath = static_cast<const char *>(inBufs[3].pData);
	const void *iconData = inBufs[4].pData;
	const char *msgArgs[16];
	SceUInt32 idx = 5;
	for (SceUInt32 i = 0; i < hdr->strCount && i < 16; ++i, ++idx)
		msgArgs[i] = static_cast<const char *>(inBufs[idx].pData);
	const char *title = static_cast<const char *>(inBufs[idx].pData);
	const char *desc = static_cast<const char *>(inBufs[idx + 1].pData);
	const char *execTitleId = static_cast<const char *>(inBufs[idx + 2].pData);
	const char *execArg = static_cast<const char *>(inBufs[idx + 3].pData);
	const char *timeUpdated = static_cast<const char *>(inBufs[idx + 4].pData);
	return shellSvcNewEventEnqueueAdd(session, reqId, titleId, itemId, hdr->rowid,
		hdr->msgType, hdr->actType, hdr->newFlag, hdr->popupNo, iconPath, iconData,
		hdr->iconDataSize, msgArgs, hdr->argCount, title, desc, hdr->execMode,
		execTitleId, execArg, timeUpdated, hdr->seId, hdr->seVolume,
		hdr->notifyFlags, hdr->popupFlag);
}

/* 0x811C4108 */
int shellSvcNewEventSendAddResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const SceInt64 *rowid, const int *hash, const int *notifyFlags)
{
	IPMI::DataInfo out[3];
	out[0].pData = rowid;
	out[0].dataSize = 8;
	out[1].pData = hash;
	out[1].dataSize = 4;
	out[2].pData = notifyFlags;
	out[2].dataSize = 4;
	return session->respondToAsyncMethodRequest(0x70001, reqId, err, out, 3);
}

/* 0x811C4158 */
int shellSvcNewEventHandleBatchPush(IPMI::Session *session, const IPMI::DataInfo *inBufs,
	SceUInt32 inCount, IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	const NewEventAddReqHdr *hdr = static_cast<const NewEventAddReqHdr *>(inBufs[0].pData);
	const char *titleId = static_cast<const char *>(inBufs[1].pData);
	const char *itemId = static_cast<const char *>(inBufs[2].pData);
	const char *iconPath = static_cast<const char *>(inBufs[3].pData);
	const void *iconData = inBufs[4].pData;
	const char *msgArgs[16];
	SceUInt32 idx = 5;
	for (SceUInt32 i = 0; i < hdr->strCount && i < 16; ++i, ++idx)
		msgArgs[i] = static_cast<const char *>(inBufs[idx].pData);
	const char *title = static_cast<const char *>(inBufs[idx].pData);
	const char *desc = static_cast<const char *>(inBufs[idx + 1].pData);
	const char *execTitleId = static_cast<const char *>(inBufs[idx + 2].pData);
	const char *execArg = static_cast<const char *>(inBufs[idx + 3].pData);
	const char *timeUpdated = static_cast<const char *>(inBufs[idx + 4].pData);
	int r = shellSvcNewEventBatchPush(session, titleId, itemId, hdr->rowid,
		hdr->msgType, hdr->actType, hdr->newFlag, hdr->popupNo, iconPath, iconData,
		hdr->iconDataSize, msgArgs, hdr->argCount, title, desc, hdr->execMode,
		execTitleId, execArg, timeUpdated, hdr->seId, hdr->seVolume, hdr->notifyFlags);
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
	const NewEventDeleteReqHdr *hdr = static_cast<const NewEventDeleteReqHdr *>(inBufs[0].pData);
	const char *titleIds[64];
	const char *itemIds[64];
	SceUInt32 idx = 1;
	for (SceUInt32 i = 0; i < hdr->titleCount && i < 64; ++i, ++idx)
		titleIds[i] = static_cast<const char *>(inBufs[idx].pData);
	for (SceUInt32 i = 0; i < hdr->itemCount && i < 64; ++i, ++idx)
		itemIds[i] = static_cast<const char *>(inBufs[idx].pData);
	const SceInt64 *rowIds = static_cast<const SceInt64 *>(inBufs[idx].pData);
	return shellSvcNewEventEnqueueDelete(session, reqId, titleIds, itemIds, rowIds, hdr->keyCount);
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
		int r = shellSvcNewEventSyncUpdateProg(session,
			static_cast<const NewEventProgressUpdatePayload *>(inBufs[0].pData));
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
			static_cast<const NewEventSimplePayload *>(inBufs[1].pData),
			*(const SceUInt8 *)inBufs[0].pData);
	case 0x70009:
		return shellSvcNewEventEnqueueClear(session, reqId, *(const SceUInt8 *)inBufs[0].pData);
	case 0x7000B:
		return shellSvcNewEventEnqueueProgStart(session, reqId,
			static_cast<const NewEventProgressStartPayload *>(inBufs[1].pData),
			*(const SceUInt8 *)inBufs[0].pData);
	case 0x7000E:
		return shellSvcNewEventEnqueueProgFinish(session, reqId,
			static_cast<const NewEventProgressFinishPayload *>(inBufs[0].pData));
	default:
		return (int)0x800205C1;
	}
}
