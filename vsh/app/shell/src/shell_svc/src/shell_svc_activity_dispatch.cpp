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

struct ActivityPostTrophyReq {
	int grade;
};

struct ActivityPostStorePurchaseReq {
	int itemType;
	int price;
	int quantity;
	int flags;
};

struct ActivityPostStoreRatingReq {
	int itemType;
	int rating;
};

struct ActivityPostGameStatusReq {
	int statusType;
	int statusFlags;
};

struct ActivityPostGameStatusUriReq {
	int statusType;
	SceUInt32 reqVersion;
};

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
		const ActivityPostTrophyReq *req =
			static_cast<const ActivityPostTrophyReq *>(inBufs[0].pData);
		r = shellSvcActivityPostTrophy(session,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			req->grade,
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
		const ActivityPostStorePurchaseReq *req =
			static_cast<const ActivityPostStorePurchaseReq *>(inBufs[0].pData);
		r = shellSvcActivityPostStorePurchase(session,
			(const char *)inBufs[1].pData, req->itemType,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData,
			(const char *)inBufs[5].pData,
			req->price, req->quantity, req->flags);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80005: {
		const ActivityPostStoreRatingReq *req =
			static_cast<const ActivityPostStoreRatingReq *>(inBufs[0].pData);
		r = shellSvcActivityPostStoreRating(session,
			(const char *)inBufs[1].pData, req->itemType,
			(const char *)inBufs[2].pData, req->rating,
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80006: {
		const ActivityPostGameStatusReq *req =
			static_cast<const ActivityPostGameStatusReq *>(inBufs[0].pData);
		r = shellSvcActivityPostGameStatus(session, req->statusType, req->statusFlags,
			(const char *)inBufs[1].pData,
			(const char *)inBufs[2].pData,
			(const char *)inBufs[3].pData,
			(const char *)inBufs[4].pData,
			(const char *)inBufs[5].pData);
		return session->respondToSyncMethodRequest(r, outBufs, outCount);
	}
	case 0x80007: {
		const ActivityPostGameStatusUriReq *req =
			static_cast<const ActivityPostGameStatusUriReq *>(inBufs[0].pData);
		r = shellSvcActivityPostGameStatusWithUri(session, req->statusType,
			(const char *)inBufs[1].pData,
			req->reqVersion,
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
	paf::job::JobQueue::default_queue->Enqueue(job);
	return r;
}
