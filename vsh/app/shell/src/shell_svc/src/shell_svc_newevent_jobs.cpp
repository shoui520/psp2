/*
 * SceShell shell_svc: SceNewEventDbJob classes & client title resolver
 * 3.65-CEX: 0x811C1300 - 0x811C300C (sti 0x811C2F0C)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/list>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"
#include "shell_svc_newevent.h"

struct LsdbAttrVal {
	int type;
	int pad;
	union {
		SceInt64 intVal;
		paf::string *str;
	};
};

struct NewEventProgressEntry {
	paf::string m_titleId;					/* +0 */
	SceInt64 m_eventId;					/* +16 */
	paf::string m_title;					/* +24 */
	paf::string m_desc;					/* +36 */
	float m_progress;					/* +48 */
	IPMI::Session *m_session;				/* +52 */
	paf::string m_cancelText;				/* +56 */

	NewEventProgressEntry() : m_eventId(0), m_progress(0.0f), m_session(NULL) {}
};

extern "C" {
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *statusBuf);
extern void SceLsdb_A6D8772C(void *ctx);
extern int SceLsdb_E90B81CD(void *ctx);
extern void SceLsdb_E2101B03(void *ctx);
extern int SceLsdb_D239031A(void *ctx, const paf::string *titleId,
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row);
extern int SceLsdb_315B9FD6(NewEventDbEntry *entry, SceUInt8 popupFlag);
extern int SceLsdb_B3252544(const paf::common::SharedPtr<NewEventDbEntry> *entries,
	SceSize count, int flags);
extern int SceLsdb_AE04DDEB(const NewEventDeleteKey *keys, SceSize count);
}

extern int shellSvcNotifyEvent(IPMI::Session *session, int ev);	/* 0x811B7560 */

static paf::list<NewEventDbJobBase *> s_newEventJobs;		/* 0x8156B7B8 */
static paf::thread::RMutex s_newEventMutex("SceNewEventDbJob", 0); /* 0x8156B7C8 */
static paf::map<SceInt64, NewEventProgressEntry> s_progressMap;	/* 0x8156B7E8 */
static NewEventBatchAddJob *s_pendingBatchJob = NULL;		/* 0x81544E04 */

static const char kDefaultUserGroupId[] = "g00";

/* 0x811C1300 */
void shellSvcResolveClientTitleId(paf::string *out, SceUID appId, SceUInt8 flag, SceUInt8 rawMode)
{
	paf::string res;
	char statusBuf[128];
	if (sceAppMgrGetStatusByAppId(appId, statusBuf) != 0) {
		*out = "";
		return;
	}
	const char *titleId = statusBuf + 12;
	SceUInt8 appKind = (SceUInt8)statusBuf[112];
	if (rawMode == 0) {
		if (appKind != 0) {
			if (appKind == 1)
				res = "NPXS10998";
			else if (appKind == 2)
				res = "NPXS10999";
			*out = res;
			return;
		}
		if (flag != 0) {
			paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > row(
				new paf::map<SceUInt32, LsdbAttrVal>());
			char ctx[4];
			SceLsdb_A6D8772C(ctx);
			if (SceLsdb_E90B81CD(ctx) == 0) {
				paf::string key(titleId);
				if (SceLsdb_D239031A(ctx, &key, &row) == 0) {
					paf::map<SceUInt32, LsdbAttrVal>::iterator it = row->find(0x308A7798u);
					if (it != row->end()) {
						const char *valStr = (it->second.type == 1 && it->second.str != NULL)
							? it->second.str->c_str() : "";
						res = valStr ? valStr : "";
					}
				}
			}
			SceLsdb_E2101B03(ctx);
			*out = res;
			return;
		}
	}
	res = titleId;
	*out = res;
}

/* 0x811C16A6 */
NewEventDbJobBase::NewEventDbJobBase(IPMI::Session *session, SceUInt32 reqId)
	: paf::job::JobItem("SceNewEventDbJob", NULL),
	  m_session(session), m_reqId(reqId)
{
	s_newEventMutex.Lock();
	s_newEventJobs.push_back(this);
	s_newEventMutex.Unlock();
}

NewEventDbJobBase::~NewEventDbJobBase()
{
	s_newEventMutex.Lock();
	s_newEventJobs.remove(this);
	s_newEventMutex.Unlock();
}

/* 0x811C1712 */
void shellSvcNewEventCancelSession(IPMI::Session *session)
{
	s_newEventMutex.Lock();
	for (paf::list<NewEventDbJobBase *>::iterator it = s_newEventJobs.begin();
	     it != s_newEventJobs.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Cancel();
			break;
		}
	}
	s_newEventMutex.Unlock();
}

/* 0x811C1758 */
int NewEventAddJob::Run()
{
	int r = SceLsdb_315B9FD6(&m_entry, m_popupFlag);
	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendAddResp(m_session, m_reqId, r,
			&m_entry.m_rowid, &m_entry.m_hash, &m_entry.m_notifyFlags);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C17AE / 0x811C181E */
NewEventBatchAddJob::~NewEventBatchAddJob()
{
}

/* 0x811C1894 */
int shellSvcNewEventGetOrCreateBatch(IPMI::Session *session, NewEventBatchAddJob **outJob, SceUInt8 detach)
{
	NewEventBatchAddJob *job = s_pendingBatchJob;
	if (job != NULL) {
		if (job->m_session != session)
			return (int)0x80101012;
	} else {
		job = new NewEventBatchAddJob(session);
		s_pendingBatchJob = job;
	}
	*outJob = job;
	if (detach != 0)
		s_pendingBatchJob = NULL;
	return 0;
}

/* 0x811C1976 */
void shellSvcNewEventAbortBatch(IPMI::Session *session)
{
	if (s_pendingBatchJob != NULL && s_pendingBatchJob->m_session == session) {
		delete s_pendingBatchJob;
		s_pendingBatchJob = NULL;
	}
}

/* 0x811C19A0 */
int NewEventBatchAddJob::Run()
{
	int r = SceLsdb_B3252544(m_entries.size() ? &m_entries[0] : NULL, m_entries.size(), m_flags);
	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendBatchResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C19E0 */
int NewEventDeleteJob::Run()
{
	int r = SceLsdb_AE04DDEB(m_keys.size() ? &m_keys[0] : NULL, m_keys.size());
	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendDeleteResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C1A1C */
int NewEventSendSimpleJob::Run()
{
	NewEventDbEntry entry;
	shellSvcResolveClientTitleId(&entry.m_titleId, m_session->getAppId(), m_resolveFlag, 0);
	int r;
	if (entry.m_titleId.size() != 0) {
		entry.m_itemId = kDefaultUserGroupId;
		paf::wstring wtext(m_payload.descText);
		paf::common::Utf16ToUtf8(wtext, &entry.m_desc);
		entry.m_msgType = 258;
		entry.m_actType = 2;
		entry.m_newFlag = 1;
		entry.m_popupNo = 1;
		entry.m_execMode = 0x20000;
		entry.m_execTitleId = entry.m_titleId;
		if (m_payload.userArg[0] != '\0')
			entry.m_execArg = paf::string("type=USER_DEFINED&arg=") + m_payload.userArg;
		r = SceLsdb_315B9FD6(&entry, 1);
		if (r != 0)
			r = (int)0x801063FF;
	} else {
		r = (int)0x801063FF;
	}

	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendSimpleResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C1DDA */
int NewEventClearJob::Run()
{
	NewEventDeleteKey key;
	shellSvcResolveClientTitleId(&key.m_titleId, m_session->getAppId(), m_resolveFlag, 0);
	key.m_itemId = kDefaultUserGroupId;
	int r = SceLsdb_AE04DDEB(&key, 1);
	if (r != 0)
		r = (int)0x801063FF;
	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendClearResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C1ED0 */
int NewEventProgressStartJob::Run()
{
	NewEventDbEntry entry;
	shellSvcResolveClientTitleId(&entry.m_titleId, m_session->getAppId(), m_resolveFlag, 0);
	int r;
	if (entry.m_titleId.size() != 0) {
		entry.m_itemId = kDefaultUserGroupId;
		paf::wstring titleW(m_payload.titleText);
		paf::common::Utf16ToUtf8(titleW, &entry.m_title);
		paf::wstring descW(m_payload.descText);
		paf::common::Utf16ToUtf8(descW, &entry.m_desc);
		entry.m_msgType = 64;
		entry.m_actType = 2;
		entry.m_newFlag = 0;
		entry.m_popupNo = 1;
		entry.m_execMode = 0x20000;
		entry.m_execTitleId = entry.m_titleId;
		if (m_payload.userArg[0] != '\0')
			entry.m_execArg = paf::string("type=USER_DEFINED&arg=") + m_payload.userArg;
		r = SceLsdb_315B9FD6(&entry, 1);
		if (r != 0)
			r = (int)0x801063FF;

		NewEventProgressEntry progressEntry;
		progressEntry.m_titleId = entry.m_titleId;
		progressEntry.m_eventId = entry.m_rowid;
		progressEntry.m_title = "";
		progressEntry.m_desc = "";
		progressEntry.m_progress = 0.0f;
		progressEntry.m_session = m_session;
		paf::wstring cancelW(m_payload.cancelText);
		paf::common::Utf16ToUtf8(cancelW, &progressEntry.m_cancelText);

		paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.find(entry.m_rowid);
		if (it == s_progressMap.end())
			s_progressMap.insert(paf::pair<SceInt64, NewEventProgressEntry>(entry.m_rowid, progressEntry));
		else
			it->second = progressEntry;
	} else {
		r = (int)0x801063FF;
	}

	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendProgStartResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C257A */
int NewEventProgressFinishJob::Run()
{
	NewEventDbEntry entry;
	paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.begin();
	for (; it != s_progressMap.end(); ++it) {
		if (it->second.m_session->getAppId() == m_session->getAppId()) {
			entry.m_titleId = it->second.m_titleId;
			break;
		}
	}

	int r;
	if (it == s_progressMap.end()) {
		r = (int)0x801063FF;
	} else {
		entry.m_itemId = kDefaultUserGroupId;
		paf::wstring titleW(m_payload.titleText);
		paf::common::Utf16ToUtf8(titleW, &entry.m_title);
		paf::wstring descW(m_payload.descText);
		paf::common::Utf16ToUtf8(descW, &entry.m_desc);
		entry.m_msgType = 128;
		entry.m_actType = 2;
		entry.m_newFlag = 1;
		entry.m_popupNo = 1;
		entry.m_execMode = 0x20000;
		entry.m_execTitleId = entry.m_titleId;
		if (m_payload.userArg[0] != '\0')
			entry.m_execArg = paf::string("type=USER_DEFINED&arg=") + m_payload.userArg;
		r = SceLsdb_315B9FD6(&entry, 1);
		if (r != 0)
			r = (int)0x801063FF;
		s_progressMap.erase(it);
	}

	s_newEventMutex.Lock();
	if (!IsCanceled())
		r = shellSvcNewEventSendProgFinishResp(m_session, m_reqId, r);
	s_newEventMutex.Unlock();
	return r;
}

/* 0x811C2A32 */
int shellSvcNewEventUpdateProgress(IPMI::Session *session,
	const NewEventProgressUpdatePayload *payload)
{
	for (paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.begin();
	     it != s_progressMap.end(); ++it) {
		if (it->second.m_session->getAppId() == session->getAppId()) {
			paf::wstring titleW(payload->titleText);
			paf::common::Utf16ToUtf8(titleW, &it->second.m_title);
			paf::wstring descW(payload->descText);
			paf::common::Utf16ToUtf8(descW, &it->second.m_desc);
			it->second.m_progress = payload->progress;
		}
	}
	return 0;
}

/* 0x811C2BF2 */
int shellSvcNewEventGetProgress(SceInt64 eventId, float *outProg,
	paf::string *outTitle, paf::string *outDesc)
{
	paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.find(eventId);
	if (it == s_progressMap.end())
		return (int)0x801063FF;
	*outProg = it->second.m_progress;
	*outTitle = it->second.m_title;
	*outDesc = it->second.m_desc;
	return 0;
}

/* 0x811C2C80 */
int shellSvcNewEventCancelProgress(SceInt64 eventId)
{
	paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.find(eventId);
	if (it != s_progressMap.end()) {
		shellSvcNotifyEvent(it->second.m_session, 52);
		s_progressMap.erase(it);
	}
	NewEventDeleteKey key;
	key.m_rowid = eventId;
	return SceLsdb_AE04DDEB(&key, 1);
}

/* 0x811C2D58 */
paf::string shellSvcNewEventGetCancelText(SceInt64 eventId)
{
	paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.find(eventId);
	if (it == s_progressMap.end())
		return paf::string("");
	return it->second.m_cancelText;
}

/* 0x811C2DF6 */
int shellSvcNewEventCleanupProgressSession(IPMI::Session *session)
{
	for (paf::map<SceInt64, NewEventProgressEntry>::iterator it = s_progressMap.begin();
	     it != s_progressMap.end(); ++it) {
		if (it->second.m_session->getAppId() == session->getAppId()) {
			NewEventDeleteKey key;
			key.m_rowid = it->second.m_eventId;
			int r = SceLsdb_AE04DDEB(&key, 1);
			s_progressMap.erase(it);
			return r;
		}
	}
	return 0;
}

NewEventSendSimpleJob::NewEventSendSimpleJob(IPMI::Session *session, SceUInt32 reqId,
	const NewEventSimplePayload *payload, SceUInt8 resolveFlag)
	: NewEventDbJobBase(session, reqId), m_resolveFlag(resolveFlag)
{
	sce_paf_memcpy(&m_payload, payload, sizeof(m_payload));
}

NewEventProgressStartJob::NewEventProgressStartJob(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressStartPayload *payload, SceUInt8 resolveFlag)
	: NewEventDbJobBase(session, reqId), m_resolveFlag(resolveFlag)
{
	sce_paf_memcpy(&m_payload, payload, sizeof(m_payload));
}

NewEventProgressFinishJob::NewEventProgressFinishJob(IPMI::Session *session, SceUInt32 reqId,
	const NewEventProgressFinishPayload *payload)
	: NewEventDbJobBase(session, reqId)
{
	sce_paf_memcpy(&m_payload, payload, sizeof(m_payload));
}
