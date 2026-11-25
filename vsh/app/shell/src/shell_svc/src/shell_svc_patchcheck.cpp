/*
 * SceShell shell_svc: ScePatchCheckCache (0x00190000..0x0019FFFF) enqueuers
 * 3.65-CEX: 0x811C4554 - 0x811C47FE
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

extern void *g_pafFramework;					/* 0x81488474 */

struct PatchCheckCacheData {
	SceInt64 f0;
	int f8;
	int fC;
};

extern int shellSvcPatchCheckDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount);		/* 0x811C4840 */
extern int shellSvcPatchCheckDispatchAsync(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount); /* 0x811C487E */
extern void shellSvcPatchCheckCancelSession(IPMI::Session *session); /* 0x811C49C0 */
extern int shellSvcPatchCheckSendGetResp(IPMI::Session *session, SceUInt32 reqId,
	int err, const PatchCheckCacheData *data);		/* 0x811C47FE */

extern paf::job::JobItem *patchCheckGetJobCtor(void *mem, IPMI::Session *session,
	SceUInt32 reqId, const char *titleId);
extern paf::job::JobItem *patchCheckSetJobCtor(void *mem, IPMI::Session *session,
	const char *titleId, const PatchCheckCacheData *data, int flags);

static paf::job::JobQueue *shellSvcPatchCheckJobQueue()
{
	return (paf::job::JobQueue *)((char *)&g_pafFramework + 146 * 4);
}

/* 0x811C4554 */
int shellSvcPatchCheckEnqueueGet(IPMI::Session *session, SceUInt32 reqId, const char *titleId)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(48);
	if (mem != NULL)
		job = patchCheckGetJobCtor(mem, session, reqId, titleId);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (sp.get() != NULL) {
		shellSvcPatchCheckJobQueue()->Enqueue(sp);
	} else {
		PatchCheckCacheData zero;
		sce_paf_memset(&zero, 0, sizeof(zero));
		shellSvcPatchCheckSendGetResp(session, reqId, (int)0x80101002, &zero);
	}
	return 0;
}

/* 0x811C468C */
int shellSvcPatchCheckEnqueueSet(IPMI::Session *session, const char *titleId,
	const PatchCheckCacheData *data, int flags)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(72);
	if (mem != NULL)
		job = patchCheckSetJobCtor(mem, session, titleId, data, flags);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (sp.get() == NULL)
		return (int)0x80101002;
	shellSvcPatchCheckJobQueue()->Enqueue(sp);
	return 0;
}

/* 0x811C47CE */
int shellSvcDisp_19_Sync(IPMI::Session *s, SceUInt32 m, const IPMI::DataInfo *in,
	SceUInt32 inCount, IPMI::BufferInfo *out, SceUInt32 outCount)
{
	return shellSvcPatchCheckDispatchSync(s, m, in, inCount, out, outCount);
}

/* 0x811C47E2 */
int shellSvcDisp_19_Async(IPMI::Session *s, SceUInt32 reqId, SceUInt32 m,
	const IPMI::DataInfo *in, SceUInt32 inCount)
{
	return shellSvcPatchCheckDispatchAsync(s, reqId, m, in, inCount);
}

/* 0x811C47F2 */
void shellSvcDisp_19_Disconn(IPMI::Session *s)
{
	shellSvcPatchCheckCancelSession(s);
}
