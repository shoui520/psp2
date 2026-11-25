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

struct NewEventDbEntry {
	paf::string m_titleId;					/* +0x00 */
	paf::string m_groupId;					/* +0x0C */
	SceInt64 m_eventId;					/* +0x18 */
	int m_type;						/* +0x20 (init 256) */
	int m_f24;						/* +0x24 */
	int m_f28;						/* +0x28 */
	SceUInt8 m_f2c;						/* +0x2C */
	SceUInt8 m_f2d;						/* +0x2D (init 1) */
	paf::string m_f30;					/* +0x30 */
	int m_f3c;						/* +0x3C */
	int m_f40;						/* +0x40 */
	paf::string m_args[9];					/* +0x44 */
	paf::string m_text1;					/* +0xB0 */
	paf::string m_text2;					/* +0xBC */
	int m_fC8;						/* +0xC8 */
	paf::string m_fCC;					/* +0xCC */
	paf::string m_fD8;					/* +0xD8 */
	paf::string m_fE4;					/* +0xE4 */
	int m_fF0;						/* +0xF0 */
	float m_fF4;						/* +0xF4 */
	int m_fF8;						/* +0xF8 */
	int m_fFC;						/* +0xFC */

	NewEventDbEntry()
		: m_eventId(0), m_type(256), m_f24(0), m_f28(0),
		  m_f2c(0), m_f2d(1), m_f3c(0), m_f40(0),
		  m_fC8(0), m_fF0(0), m_fF4(0.0f), m_fF8(0), m_fFC(0) {}
};

struct NewEventDeleteKey {
	paf::string m_titleId;					/* +0x00 */
	paf::string m_groupId;					/* +0x0C */
	SceInt64 m_eventId;					/* +0x18 */

	NewEventDeleteKey() : m_eventId(0) {}
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
	NewEventAddJob(IPMI::Session *session, SceUInt32 reqId, SceUInt8 flag)
		: NewEventDbJobBase(session, reqId), m_flag(flag) {}
	virtual ~NewEventAddJob() {}
	virtual int Run();					/* 0x811C1758 */

	NewEventDbEntry m_entry;				/* +40 */
	SceUInt8 m_flag;					/* +296 */
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
	NewEventSendSimpleJob(IPMI::Session *session, SceUInt32 reqId, const void *payload, SceUInt8 flag);
	virtual ~NewEventSendSimpleJob() {}
	virtual int Run();					/* 0x811C1A1C */

	SceUInt8 m_flag;					/* +36 */
	SceUInt32 m_payload[284];				/* +40 (1136 bytes -> total 1176) */
};

class NewEventClearJob : public NewEventDbJobBase {
public:
	NewEventClearJob(IPMI::Session *session, SceUInt32 reqId, SceUInt8 flag)
		: NewEventDbJobBase(session, reqId), m_flag(flag) {}
	virtual ~NewEventClearJob() {}
	virtual int Run();					/* 0x811C1DDA */

	SceUInt8 m_flag;					/* +36 */
};

class NewEventProgressStartJob : public NewEventDbJobBase {
public:
	NewEventProgressStartJob(IPMI::Session *session, SceUInt32 reqId, const void *payload, SceUInt8 flag);
	virtual ~NewEventProgressStartJob() {}
	virtual int Run();					/* 0x811C1ED0 */

	SceUInt8 m_flag;					/* +36 */
	SceUInt32 m_payload[348];				/* +40 (1392 bytes -> total 1432) */
};

class NewEventProgressFinishJob : public NewEventDbJobBase {
public:
	NewEventProgressFinishJob(IPMI::Session *session, SceUInt32 reqId, const void *payload);
	virtual ~NewEventProgressFinishJob() {}
	virtual int Run();					/* 0x811C257A */

	SceUInt16 m_payload[628];				/* +36 (1256 bytes -> total 1292) */
};

/* Cross-TU functions */
extern void shellSvcResolveClientTitleId(paf::string *out, SceUID appId, SceUInt8 flag, SceUInt8 rawMode); /* 0x811C1300 */
extern void shellSvcNewEventCancelSession(IPMI::Session *session); /* 0x811C1712 */
extern int shellSvcNewEventGetOrCreateBatch(IPMI::Session *session, NewEventBatchAddJob **outJob, SceUInt8 detach); /* 0x811C1894 */
extern void shellSvcNewEventAbortBatch(IPMI::Session *session);	/* 0x811C1976 */
extern int shellSvcNewEventUpdateProgress(IPMI::Session *session, const void *payload); /* 0x811C2A32 */
extern int shellSvcNewEventGetProgress(SceInt64 eventId, float *outProg, paf::string *outT1, paf::string *outT2); /* 0x811C2BF2 */
extern int shellSvcNewEventCancelProgress(SceInt64 eventId);	/* 0x811C2C80 */
extern paf::string shellSvcNewEventGetCancelText(SceInt64 eventId); /* 0x811C2D58 */
extern int shellSvcNewEventCleanupProgressSession(IPMI::Session *session); /* 0x811C2DF6 */

extern int shellSvcNewEventSendAddResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const SceInt64 *eventId, const int *f1, const int *f2);	/* 0x811C4108 */
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
