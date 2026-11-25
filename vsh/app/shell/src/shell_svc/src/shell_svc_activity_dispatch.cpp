/*
 * SceShell shell_svc: Activity sync dispatcher & PostSystemSoftwareStatusJob enqueuer
 * 3.65-CEX: 0x811BCFBA - 0x811BD998 (sti 0x811BD928)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

#include "../../livespace_db/lsdb_keys.h"
#include "shell_svc_activity.h"

extern void *g_pafFramework;					/* 0x81488474 */

static paf::job::JobQueue *shellSvcActivityDispatchJobQueue()
{
	return (paf::job::JobQueue *)((char *)&g_pafFramework + 146 * 4);
}

/* 0x811BCFBA */
int shellSvcActivityDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int r;
	switch (methodId) {
	case 0x80000:
		r = shellSvcActivityPostSystemSoftware(session,
			(const char *)inBufs[0].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	case 0x80001: {
		const int *p0 = (const int *)inBufs[0].pData;
		r = shellSvcActivityPostTrophy(session,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			p0[0],
			(const char *)inBufs[4].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80002:
		r = shellSvcActivityPostHiddenTrophy(session,
			(const char *)inBufs[0].pData,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	case 0x80003:
		r = shellSvcActivityPostPlatinumTrophy(session,
			(const char *)inBufs[0].pData,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	case 0x80004: {
		const int *p0 = (const int *)inBufs[0].pData;
		r = shellSvcActivityPostStorePurchase(session,
			(const char *)inBufs[1].pData, p0[0],
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData,
			(const char *)inBufs[5].pData,
			p0[1], p0[2], p0[3]);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80005: {
		const int *p0 = (const int *)inBufs[0].pData;
		r = shellSvcActivityPostStoreRating(session,
			(const char *)inBufs[1].pData, p0[0],
			(const char *)inBufs[2].pData, p0[1],
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80006: {
		const int *p0 = (const int *)inBufs[0].pData;
		r = shellSvcActivityPostGameStatus(session, p0[0], p0[1],
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData,
			(const char *)inBufs[5].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80007: {
		const int *p0 = (const int *)inBufs[0].pData;
		r = shellSvcActivityPostGameStatusWithUri(session, p0[0],
			(const char *)inBufs[1].pData,
			(SceUInt32)p0[1],
			(const char *)inBufs[2].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	default:
		return (int)0x800205C1;
	}
}

/* 0x811BD168 */
int shellSvcActivityPostSystemSoftwareImpl(const char *titleId)
{
	ShellSvcActivityRecord rec;
	rec.m_type = 4;
	rec.m_titleId = titleId ? titleId : "";

	int r = shellSvcActivityCheckEnabled();
	if (r < 0)
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job(new PostSystemSoftwareStatusJob(rec));
	shellSvcActivityDispatchJobQueue()->Enqueue(job);
	return r;
}
