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

extern void *g_pafFramework;					/* 0x81488474 */

static paf::job::JobQueue *shellSvcNewEventJobQueue()
{
	return (paf::job::JobQueue *)((char *)&g_pafFramework + 146 * 4);
}

/* 0x811C300C */
void shellSvcNewEventFillEntry(NewEventDbEntry *e, const char *titleId, const char *groupId,
	SceInt64 eventId, int type, int f28, SceUInt8 f2c, SceUInt8 f2d,
	const char *f30, int f3c, int f40, const char **args, SceUInt32 argCount,
	const char *text1, const char *text2, int fC8,
	const char *fCC, const char *fD8, const char *fE4,
	int fF0, float fF4, int fFC)
{
	e->m_titleId = titleId ? titleId : "";
	e->m_groupId = groupId ? groupId : "";
	e->m_eventId = eventId;
	e->m_type = type;
	e->m_f24 = 0;
	e->m_f28 = f28;
	e->m_f2c = f2c;
	e->m_f2d = f2d;
	e->m_f30 = f30 ? f30 : "";
	e->m_f3c = f3c;
	e->m_f40 = f40;
	SceUInt32 n = (argCount < 9u) ? argCount : 9u;
	for (SceUInt32 i = 0; i < n; ++i)
		e->m_args[i] = args[i] ? args[i] : "";
	e->m_text1 = text1 ? text1 : "";
	e->m_text2 = text2 ? text2 : "";
	e->m_fC8 = fC8;
	e->m_fCC = fCC ? fCC : "";
	e->m_fD8 = fD8 ? fD8 : "";
	e->m_fE4 = fE4 ? fE4 : "";
	e->m_fF0 = fF0;
	e->m_fF4 = fF4;
	e->m_fFC = fFC;
}

/* 0x811C31D4 */
int shellSvcNewEventEnqueueAdd(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, const char *groupId, SceInt64 eventId,
	int type, int f28, SceUInt8 f2c, SceUInt8 f2d,
	const char *f30, int f3c, int f40, const char **args, SceUInt32 argCount,
	const char *text1, const char *text2, int fC8,
	const char *fCC, const char *fD8, const char *fE4,
	int fF0, float fF4, int fFC, SceUInt8 flag)
{
	NewEventAddJob *job = new NewEventAddJob(session, reqId, flag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		SceInt64 zeroId = 0;
		int z1 = 0, z2 = 0;
		shellSvcNewEventSendAddResp(session, reqId, (int)0x80101002, &zeroId, &z1, &z2);
		return 0;
	}
	shellSvcNewEventFillEntry(&job->m_entry, titleId, groupId, eventId,
		type, f28, f2c, f2d, f30, f3c, f40, args, argCount,
		text1, text2, fC8, fCC, fD8, fE4, fF0, fF4, fFC);
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0) {
		SceInt64 zeroId = 0;
		int z1 = 0, z2 = 0;
		shellSvcNewEventSendAddResp(session, reqId, r, &zeroId, &z1, &z2);
	}
	return 0;
}

/* 0x811C34A2 */
int shellSvcNewEventBatchPush(IPMI::Session *session,
	const char *titleId, const char *groupId, SceInt64 eventId,
	int type, int f28, SceUInt8 f2c, SceUInt8 f2d,
	const char *f30, int f3c, int f40, const char **args, SceUInt32 argCount,
	const char *text1, const char *text2, int fC8,
	const char *fCC, const char *fD8, const char *fE4,
	int fF0, float fF4, int fFC)
{
	NewEventBatchAddJob *job = NULL;
	int r = shellSvcNewEventGetOrCreateBatch(session, &job, 0);
	if (r != 0)
		return r;
	paf::common::SharedPtr<NewEventDbEntry> entry(new NewEventDbEntry());
	job->m_entries.push_back(entry);
	shellSvcNewEventFillEntry(job->m_entries[job->m_entries.size() - 1].get(),
		titleId, groupId, eventId, type, f28, f2c, f2d,
		f30, f3c, f40, args, argCount, text1, text2,
		fC8, fCC, fD8, fE4, fF0, fF4, fFC);
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
	const char **titleIds, const char **groupIds, const SceInt64 *eventIds, int count)
{
	NewEventDeleteJob *job = new NewEventDeleteJob(session, reqId);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (job == NULL) {
		shellSvcNewEventSendDeleteResp(session, reqId, (int)0x80101002);
		return 0;
	}
	for (int i = 0; i < count; ++i) {
		NewEventDeleteKey k;
		job->m_keys.push_back(k);
		NewEventDeleteKey &dst = job->m_keys[job->m_keys.size() - 1];
		dst.m_titleId = titleIds[i] ? titleIds[i] : "";
		dst.m_groupId = groupIds[i] ? groupIds[i] : "";
		dst.m_eventId = eventIds[i];
	}
	int r = shellSvcNewEventJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcNewEventSendDeleteResp(session, reqId, r);
	return 0;
}

/* 0x811C3A78 */
int shellSvcNewEventEnqueueSendSimple(IPMI::Session *session, SceUInt32 reqId,
	const void *payload, SceUInt8 flag)
{
	NewEventSendSimpleJob *job = new NewEventSendSimpleJob(session, reqId, payload, flag);
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
int shellSvcNewEventEnqueueClear(IPMI::Session *session, SceUInt32 reqId, SceUInt8 flag)
{
	NewEventClearJob *job = new NewEventClearJob(session, reqId, flag);
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
	const void *payload, SceUInt8 flag)
{
	NewEventProgressStartJob *job = new NewEventProgressStartJob(session, reqId, payload, flag);
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
int shellSvcNewEventSyncUpdateProg(IPMI::Session *session, const void *payload)
{
	shellSvcNewEventUpdateProgress(session, payload);
	return 0;
}

/* 0x811C3E10 */
int shellSvcNewEventEnqueueProgFinish(IPMI::Session *session, SceUInt32 reqId, const void *payload)
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
int shellSvcDisp_07_Sync(IPMI::Session *s, SceUInt32 m, const IPMI::DataInfo *in,
	SceUInt32 inCount, IPMI::BufferInfo *out, SceUInt32 outCount)
{
	return shellSvcNewEventDispatchSync(s, m, in, inCount, out, outCount);
}

/* 0x811C3F56 */
int shellSvcDisp_07_Async(IPMI::Session *s, SceUInt32 reqId, SceUInt32 m,
	const IPMI::DataInfo *in, SceUInt32 inCount)
{
	return shellSvcNewEventDispatchAsync(s, reqId, m, in, inCount);
}

/* 0x811C3F66 */
void shellSvcDisp_07_Disconn(IPMI::Session *s)
{
	shellSvcNewEventAbortBatch(s);
	shellSvcNewEventCancelSession(s);
	shellSvcNewEventCleanupProgressSession(s);
}

/* 0x811C3F7C */
int shellSvcNewEventGetProgressWrap(SceInt64 eventId, float *outProg, paf::string *outT1, paf::string *outT2)
{
	return shellSvcNewEventGetProgress(eventId, outProg, outT1, outT2);
}

/* 0x811C3F8C */
int shellSvcNewEventCancelProgressWrap(SceInt64 eventId)
{
	return shellSvcNewEventCancelProgress(eventId);
}

/* 0x811C3F94 */
paf::string shellSvcNewEventGetCancelTextWrap(SceInt64 eventId)
{
	return shellSvcNewEventGetCancelText(eventId);
}
