/*
 * SceShell shell_svc: SceNewEventDb (0x00070000..0x0007FFFF) job enqueuers
 * 3.65-CEX: 0x811C300C - 0x811C3F9C
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

#include "shell_svc_newevent.h"

static paf::job::JobQueue *shellSvcNewEventJobQueue()
{
	return paf::job::JobQueue::default_queue;
}

/* 0x811C300C */
void shellSvcNewEventFillEntry(NewEventDbEntry *entry, const char *titleId, const char *itemId,
	SceInt64 rowid, int msgType, int actType, SceUInt8 newFlag, SceUInt8 popupNo,
	const char *iconPath, const void *iconData, int iconDataSize,
	const char **msgArgs, SceUInt32 argCount,
	const char *title, const char *desc, int execMode,
	const char *execTitleId, const char *execArg, const char *timeUpdated,
	int seId, float seVolume, int notifyFlags)
{
	entry->m_titleId = titleId ? titleId : "";
	entry->m_itemId = itemId ? itemId : "";
	entry->m_rowid = rowid;
	entry->m_msgType = msgType;
	entry->m_actType = actType;
	entry->m_newFlag = newFlag;
	entry->m_popupNo = popupNo;
	entry->m_iconPath = iconPath ? iconPath : "";
	entry->m_iconData = iconData;
	entry->m_iconDataSize = iconDataSize;
	SceUInt32 count = (argCount < 9u) ? argCount : 9u;
	for (SceUInt32 idx = 0; idx < count; ++idx)
		entry->m_msgArgs[idx] = msgArgs[idx] ? msgArgs[idx] : "";
	entry->m_title = title ? title : "";
	entry->m_desc = desc ? desc : "";
	entry->m_execMode = execMode;
	entry->m_execTitleId = execTitleId ? execTitleId : "";
	entry->m_execArg = execArg ? execArg : "";
	entry->m_timeUpdated = timeUpdated ? timeUpdated : "";
	entry->m_seId = seId;
	entry->m_seVolume = seVolume;
	entry->m_notifyFlags = notifyFlags;
}

/* 0x811C31D4 */
int shellSvcNewEventEnqueueAdd(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, const char *itemId, SceInt64 rowid,
	int msgType, int actType, SceUInt8 newFlag, SceUInt8 popupNo,
	const char *iconPath, const void *iconData, int iconDataSize,
	const char **msgArgs, SceUInt32 argCount,
	const char *title, const char *desc, int execMode,
	const char *execTitleId, const char *execArg, const char *timeUpdated,
	int seId, float seVolume, int notifyFlags, SceUInt8 popupFlag)
{
	NewEventAddJob *job = new NewEventAddJob(session, reqId, popupFlag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		SceInt64 zeroRowId = 0;
		int zeroHash = 0;
		int zeroNotify = 0;
		shellSvcNewEventSendAddResp(session, reqId, (int)0x80101002,
			&zeroRowId, &zeroHash, &zeroNotify);
		return 0;
	}
	shellSvcNewEventFillEntry(&job->m_entry, titleId, itemId, rowid,
		msgType, actType, newFlag, popupNo, iconPath, iconData, iconDataSize,
		msgArgs, argCount, title, desc, execMode, execTitleId, execArg, timeUpdated,
		seId, seVolume, notifyFlags);
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0) {
		SceInt64 zeroRowId = 0;
		int zeroHash = 0;
		int zeroNotify = 0;
		shellSvcNewEventSendAddResp(session, reqId, r,
			&zeroRowId, &zeroHash, &zeroNotify);
	}
	return 0;
}

/* 0x811C34A2 */
int shellSvcNewEventBatchPush(IPMI::Session *session,
	const char *titleId, const char *itemId, SceInt64 rowid,
	int msgType, int actType, SceUInt8 newFlag, SceUInt8 popupNo,
	const char *iconPath, const void *iconData, int iconDataSize,
	const char **msgArgs, SceUInt32 argCount,
	const char *title, const char *desc, int execMode,
	const char *execTitleId, const char *execArg, const char *timeUpdated,
	int seId, float seVolume, int notifyFlags)
{
	NewEventBatchAddJob *job = NULL;
	int r = shellSvcNewEventGetOrCreateBatch(session, &job, 0);
	if (r != 0)
		return r;
	paf::common::SharedPtr<NewEventDbEntry> entry(new NewEventDbEntry());
	job->m_entries.push_back(entry);
	shellSvcNewEventFillEntry(job->m_entries[job->m_entries.size() - 1].get(),
		titleId, itemId, rowid, msgType, actType, newFlag, popupNo,
		iconPath, iconData, iconDataSize, msgArgs, argCount, title, desc,
		execMode, execTitleId, execArg, timeUpdated, seId, seVolume, notifyFlags);
	return 0;
}

/* 0x811C372C */
int shellSvcNewEventEnqueueBatchCommit(IPMI::Session *session, SceUInt32 reqId, int flags)
{
	NewEventBatchAddJob *job = NULL;
	int r = shellSvcNewEventGetOrCreateBatch(session, &job, 1);
	if (r != 0) {
		shellSvcNewEventSendBatchResp(session, reqId, r);
		return 0;
	}
	job->m_flags = flags;
	job->m_reqId = reqId;
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendBatchResp(session, reqId, r);
	return 0;
}

/* 0x811C3868 */
int shellSvcNewEventEnqueueDelete(IPMI::Session *session, SceUInt32 reqId,
	const char **titleIds, const char **itemIds, const SceInt64 *rowIds, int count)
{
	NewEventDeleteJob *job = new NewEventDeleteJob(session, reqId);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendDeleteResp(session, reqId, (int)0x80101002);
		return 0;
	}
	for (int idx = 0; idx < count; ++idx) {
		NewEventDeleteKey key;
		job->m_keys.push_back(key);
		NewEventDeleteKey &dst = job->m_keys[job->m_keys.size() - 1];
		dst.m_titleId = titleIds[idx] ? titleIds[idx] : "";
		dst.m_itemId = itemIds[idx] ? itemIds[idx] : "";
		dst.m_rowid = rowIds[idx];
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendDeleteResp(session, reqId, r);
	return 0;
}

/* 0x811C3A78 */
int shellSvcNewEventEnqueueSendSimple(IPMI::Session *session, SceUInt32 reqId,
	const NewEventSimplePayload *payload, SceUInt8 resolveFlag)
{
	NewEventSendSimpleJob *job = new NewEventSendSimpleJob(session, reqId, payload, resolveFlag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendSimpleResp(session, reqId, (int)0x801063FF);
		return 0;
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendSimpleResp(session, reqId, r);
	return 0;
}

/* 0x811C3BBC */
int shellSvcNewEventEnqueueClear(IPMI::Session *session, SceUInt32 reqId, SceUInt8 resolveFlag)
{
	NewEventClearJob *job = new NewEventClearJob(session, reqId, resolveFlag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendClearResp(session, reqId, (int)0x801063FF);
		return 0;
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendClearResp(session, reqId, r);
	return 0;
}

/* 0x811C3CC2 */
int shellSvcNewEventEnqueueProgStart(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressStartPayload *payload, SceUInt8 resolveFlag)
{
	NewEventProgressStartJob *job = new NewEventProgressStartJob(session, reqId, payload, resolveFlag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendProgStartResp(session, reqId, (int)0x801063FF);
		return 0;
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendProgStartResp(session, reqId, r);
	return 0;
}

/* 0x811C3E06 */
int shellSvcNewEventSyncUpdateProg(IPMI::Session *session,
	const NewEventProgressUpdatePayload *payload)
{
	shellSvcNewEventUpdateProgress(session, payload);
	return 0;
}

/* 0x811C3E10 */
int shellSvcNewEventEnqueueProgFinish(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressFinishPayload *payload)
{
	NewEventProgressFinishJob *job = new NewEventProgressFinishJob(session, reqId, payload);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendProgFinishResp(session, reqId, (int)0x801063FF);
		return 0;
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendProgFinishResp(session, reqId, r);
	return 0;
}

/* 0x811C3F42 */
int shellSvcDisp_07_Sync(IPMI::Session *session, SceUInt32 methodId, const IPMI::DataInfo *inBufs,
	SceUInt32 inCount, IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return shellSvcNewEventDispatchSync(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* 0x811C3F56 */
int shellSvcDisp_07_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	return shellSvcNewEventDispatchAsync(session, reqId, methodId, inBufs, inCount);
}

/* 0x811C3F66 */
void shellSvcDisp_07_Disconn(IPMI::Session *session)
{
	shellSvcNewEventAbortBatch(session);
	shellSvcNewEventCancelSession(session);
	shellSvcNewEventCleanupProgressSession(session);
}

/* 0x811C3F7C */
int shellSvcNewEventGetProgressWrap(SceInt64 eventId, float *outProg,
	paf::string *outTitle, paf::string *outDesc)
{
	return shellSvcNewEventGetProgress(eventId, outProg, outTitle, outDesc);
}

/* 0x811C3F8C */
int shellSvcNewEventCancelProgressWrap(SceInt64 eventId)
{
	return shellSvcNewEventCancelProgress(eventId);
}
