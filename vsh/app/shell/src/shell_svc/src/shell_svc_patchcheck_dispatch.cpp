/*
 * SceShell shell_svc: ScePatchCheckCache sync/async dispatch & response
 * 3.65-CEX: 0x811C47FE - 0x811C4898
 */

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>

struct PatchCheckCacheData {
	SceInt64 f0;
	int f8;
	int fC;
};

extern int shellSvcPatchCheckEnqueueGet(IPMI::Session *session, SceUInt32 reqId, const char *titleId); /* 0x811C4554 */
extern int shellSvcPatchCheckEnqueueSet(IPMI::Session *session, const char *titleId,
	const PatchCheckCacheData *data, int flags);		/* 0x811C468C */

/* 0x811C47FE */
int shellSvcPatchCheckSendGetResp(IPMI::Session *session, SceUInt32 reqId,
	int err, const PatchCheckCacheData *data)
{
	IPMI::DataInfo out[1];
	out[0].pData = data;
	out[0].dataSize = 16;
	return session->respondToAsyncMethodRequest(0x190001, reqId, err, out, 1);
}

/* 0x811C4840 */
int shellSvcPatchCheckDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	if (methodId != 0x190002)
		return (int)0x800205C1;
	int r = shellSvcPatchCheckEnqueueSet(session,
		(const char *)inBufs[1].pData,
		(const PatchCheckCacheData *)inBufs[2].pData,
		*(const int *)inBufs[0].pData);
	return session->respondToSyncMethodRequest(r, outBufs, outCount);
}

/* 0x811C487E */
int shellSvcPatchCheckDispatchAsync(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	(void)inCount;
	if (methodId == 0x190000)
		return shellSvcPatchCheckEnqueueGet(session, reqId, (const char *)inBufs[0].pData);
	return (int)0x800205C1;
}
