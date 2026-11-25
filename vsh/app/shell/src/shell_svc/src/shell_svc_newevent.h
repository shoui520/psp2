/*
 * SceShell shell_svc: SceNewEventDb (0x00070000..0x0007FFFF) internal types
 */

#ifndef SHELL_SVC_NEWEVENT_H
#define SHELL_SVC_NEWEVENT_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

/* Matches Sony's LsdbNewEvent layout (tbl_newevent) */
struct NewEventDbEntry {
	paf::string m_titleId;					/* +0x00 */
	paf::string m_itemId;					/* +0x0C */
	SceInt64 m_rowid;					/* +0x18 */
	SceInt64 m_msgType;					/* +0x20 (init 256) */
	int m_actType;						/* +0x28 */
	SceUInt8 m_newFlag;					/* +0x2C */
	SceUInt8 m_popupNo;					/* +0x2D (init 1) */
	paf::string m_iconPath;					/* +0x30 */
	const void *m_iconData;					/* +0x3C */
	int m_iconDataSize;					/* +0x40 */
	paf::string m_msgArgs[9];				/* +0x44 */
	paf::string m_title;					/* +0xB0 */
	paf::string m_desc;					/* +0xBC */
	int m_execMode;						/* +0xC8 */
	paf::string m_execTitleId;				/* +0xCC */
	paf::string m_execArg;					/* +0xD8 */
	paf::string m_timeUpdated;				/* +0xE4 */
	int m_seId;						/* +0xF0 */
	float m_seVolume;					/* +0xF4 */
	int m_hash;						/* +0xF8 */
	int m_notifyFlags;					/* +0xFC */

	NewEventDbEntry()
		: m_rowid(0), m_msgType(256), m_actType(0),
		  m_newFlag(0), m_popupNo(1), m_iconData(NULL), m_iconDataSize(0),
		  m_execMode(0), m_seId(0), m_seVolume(0.0f), m_hash(0), m_notifyFlags(0) {}
};

struct NewEventDeleteKey {
	paf::string m_titleId;					/* +0x00 */
	paf::string m_itemId;					/* +0x0C */
	SceInt64 m_rowid;					/* +0x18 */

	NewEventDeleteKey() : m_rowid(0) {}
};

struct NewEventSimplePayload {
	wchar_t descText[64];					/* +0 */
	char userArg[1008];					/* +128 */
};

struct NewEventProgressStartPayload {
	wchar_t titleText[64];					/* +0 */
	wchar_t descText[64];					/* +128 */
	char userArg[1008];					/* +256 */
	wchar_t cancelText[64];					/* +1264 */
};

struct NewEventProgressUpdatePayload {
	wchar_t titleText[64];					/* +0 */
	wchar_t descText[64];					/* +128 */
	float progress;						/* +256 */
};

struct NewEventProgressFinishPayload {
	wchar_t titleText[64];					/* +0 */
	wchar_t descText[64];					/* +128 */
	char userArg[1000];					/* +256 */
};

class NewEventDbJobBase : public paf::job::JobItem {
public:
	NewEventDbJobBase(IPMI::Session *session, SceUInt32 reqId); /* 0x811C16A6 */
	virtual ~NewEventDbJobBase();
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	IPMI::Session *m_session;				/* +28 */
	SceUInt32 m_reqId;					/* +32 */
};

class NewEventAddJob : public NewEventDbJobBase {
public:
	NewEventAddJob(IPMI::Session *session, SceUInt32 reqId, SceUInt8 popupFlag)
		: NewEventDbJobBase(session, reqId), m_popupFlag(popupFlag) {}
	virtual ~NewEventAddJob() {}
	virtual int Run();					/* 0x811C1758 */

	NewEventDbEntry m_entry;				/* +40 */
	SceUInt8 m_popupFlag;					/* +296 */
};

class NewEventBatchAddJob : public NewEventDbJobBase {
public:
	NewEventBatchAddJob(IPMI::Session *session)
		: NewEventDbJobBase(session, 0), m_flags(0) {}
	virtual ~NewEventBatchAddJob();				/* 0x811C17AE / 0x811C181E */
	virtual int Run();					/* 0x811C19A0 */

	paf::vector<paf::common::SharedPtr<NewEventDbEntry> > m_entries; /* +36 */
	int m_flags;						/* +52 */
};

class NewEventDeleteJob : public NewEventDbJobBase {
public:
	NewEventDeleteJob(IPMI::Session *session, SceUInt32 reqId)
		: NewEventDbJobBase(session, reqId) {}
	virtual ~NewEventDeleteJob() {}
	virtual int Run();					/* 0x811C19E0 */

	paf::vector<NewEventDeleteKey> m_keys;			/* +36 */
};

class NewEventSendSimpleJob : public NewEventDbJobBase {
public:
	NewEventSendSimpleJob(IPMI::Session *session, SceUInt32 reqId,
		const NewEventSimplePayload *payload, SceUInt8 resolveFlag);
	virtual ~NewEventSendSimpleJob() {}
	virtual int Run();					/* 0x811C1A1C */

	SceUInt8 m_resolveFlag;					/* +36 */
	NewEventSimplePayload m_payload;			/* +40 (1136 bytes -> total 1176) */
};

class NewEventClearJob : public NewEventDbJobBase {
public:
	NewEventClearJob(IPMI::Session *session, SceUInt32 reqId, SceUInt8 resolveFlag)
		: NewEventDbJobBase(session, reqId), m_resolveFlag(resolveFlag) {}
	virtual ~NewEventClearJob() {}
	virtual int Run();					/* 0x811C1DDA */

	SceUInt8 m_resolveFlag;					/* +36 */
};

class NewEventProgressStartJob : public NewEventDbJobBase {
public:
	NewEventProgressStartJob(IPMI::Session *session, SceUInt32 reqId,
		const NewEventProgressStartPayload *payload, SceUInt8 resolveFlag);
	virtual ~NewEventProgressStartJob() {}
	virtual int Run();					/* 0x811C1ED0 */

	SceUInt8 m_resolveFlag;					/* +36 */
	NewEventProgressStartPayload m_payload;			/* +40 (1392 bytes -> total 1432) */
};

class NewEventProgressFinishJob : public NewEventDbJobBase {
public:
	NewEventProgressFinishJob(IPMI::Session *session, SceUInt32 reqId,
		const NewEventProgressFinishPayload *payload);
	virtual ~NewEventProgressFinishJob() {}
	virtual int Run();					/* 0x811C257A */

	NewEventProgressFinishPayload m_payload;		/* +36 (1256 bytes -> total 1292) */
};

/* Cross-TU functions */
extern void shellSvcResolveClientTitleId(paf::string *out, SceUID appId, SceUInt8 flag, SceUInt8 rawMode); /* 0x811C1300 */
extern void shellSvcNewEventCancelSession(IPMI::Session *session); /* 0x811C1712 */
extern int shellSvcNewEventGetOrCreateBatch(IPMI::Session *session, NewEventBatchAddJob **outJob, SceUInt8 detach); /* 0x811C1894 */
extern void shellSvcNewEventAbortBatch(IPMI::Session *session);	/* 0x811C1976 */
extern int shellSvcNewEventUpdateProgress(IPMI::Session *session,
	const NewEventProgressUpdatePayload *payload);		/* 0x811C2A32 */
extern int shellSvcNewEventGetProgress(SceInt64 eventId, float *outProg,
	paf::string *outTitle, paf::string *outDesc);		/* 0x811C2BF2 */
extern int shellSvcNewEventCancelProgress(SceInt64 eventId);	/* 0x811C2C80 */
extern paf::string shellSvcNewEventGetCancelText(SceInt64 eventId); /* 0x811C2D58 */
extern int shellSvcNewEventCleanupProgressSession(IPMI::Session *session); /* 0x811C2DF6 */

extern int shellSvcNewEventSendAddResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const SceInt64 *rowid, const int *hash, const int *notifyFlags); /* 0x811C4108 */
extern int shellSvcNewEventSendBatchResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C42DE */
extern int shellSvcNewEventSendDeleteResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C43DC */
extern int shellSvcNewEventSendSimpleResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C43FE */
extern int shellSvcNewEventSendClearResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C441E */
extern int shellSvcNewEventSendProgStartResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C4440 */
extern int shellSvcNewEventSendProgFinishResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C4460 */

extern int shellSvcNewEventDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount);		/* 0x811C4480 */
extern int shellSvcNewEventDispatchAsync(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount); /* 0x811C44E2 */

#endif /* SHELL_SVC_NEWEVENT_H */
