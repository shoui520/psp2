/*
 * SceShell shell_svc: ScePatchCheckCacheJob classes
 * 3.65-CEX: 0x811C4898 - 0x811C4BF8 (sti 0x811C4B1A)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/list>
#include <std/string>
#include <ipmi/session.h>
#include <job/job.h>
#include <plugin.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

struct PatchCheckCacheData {
	SceInt64 f0;
	int f8;
	int fC;
};

extern "C" {
extern int SceLsdb_4B64A790(void *ctx, const paf::string *titleId, PatchCheckCacheData *outData);
extern int SceLsdb_79A6D877(void *ctx, const paf::string *titleId, bool flag,
	const PatchCheckCacheData *data, int flags);
}

extern int shellSvcPatchCheckSendGetResp(IPMI::Session *session, SceUInt32 reqId,
	int err, const PatchCheckCacheData *data);		/* 0x811C47FE */

class PatchCheckCacheJobBase : public paf::job::JobItem {
public:
	PatchCheckCacheJobBase(IPMI::Session *session, SceUInt32 reqId); /* 0x811C4898 */
	virtual ~PatchCheckCacheJobBase();			/* 0x811C4904 / 0x811C4960 */
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	IPMI::Session *m_session;				/* +28 */
	SceUInt32 m_reqId;					/* +32 */
};

static paf::list<PatchCheckCacheJobBase *> s_patchCheckJobs;	/* 0x8156B810 */
static paf::thread::RMutex s_patchCheckMutex("ScePatchCheckCacheJob", 0); /* 0x8156B820 */

/* 0x811C4898 */
PatchCheckCacheJobBase::PatchCheckCacheJobBase(IPMI::Session *session, SceUInt32 reqId)
	: paf::job::JobItem("ScePatchCheckCacheJob", NULL),
	  m_session(session), m_reqId(reqId)
{
	s_patchCheckMutex.Lock();
	s_patchCheckJobs.push_back(this);
	s_patchCheckMutex.Unlock();
}

/* 0x811C4904 / 0x811C4960 */
PatchCheckCacheJobBase::~PatchCheckCacheJobBase()
{
	s_patchCheckMutex.Lock();
	s_patchCheckJobs.remove(this);
	s_patchCheckMutex.Unlock();
}

/* 0x811C49C0 */
void shellSvcPatchCheckCancelSession(IPMI::Session *session)
{
	s_patchCheckMutex.Lock();
	for (paf::list<PatchCheckCacheJobBase *>::iterator it = s_patchCheckJobs.begin();
	     it != s_patchCheckJobs.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Cancel();
			break;
		}
	}
	s_patchCheckMutex.Unlock();
}

class PatchCheckGetJob : public PatchCheckCacheJobBase {
public:
	PatchCheckGetJob(IPMI::Session *session, SceUInt32 reqId, const char *titleId)
		: PatchCheckCacheJobBase(session, reqId),
		  m_titleId(titleId ? titleId : "") {}
	virtual ~PatchCheckGetJob() {}
	virtual int Run();					/* 0x811C4A06 */

	paf::string m_titleId;					/* +36 */
};

/* 0x811C4A06 */
int PatchCheckGetJob::Run()
{
	char ctx[8];
	PatchCheckCacheData data;
	sce_paf_memset(&data, 0, sizeof(data));
	int r = SceLsdb_4B64A790(ctx, &m_titleId, &data);
	s_patchCheckMutex.Lock();
	if (!IsCanceled())
		r = shellSvcPatchCheckSendGetResp(m_session, m_reqId, r, &data);
	s_patchCheckMutex.Unlock();
	return r;
}

class PatchCheckSetJob : public PatchCheckCacheJobBase {
public:
	PatchCheckSetJob(IPMI::Session *session, const char *titleId,
		const PatchCheckCacheData *data, int flags)
		: PatchCheckCacheJobBase(session, 0),
		  m_titleId(titleId ? titleId : ""),
		  m_data(*data), m_flags(flags) {}
	virtual ~PatchCheckSetJob() {}
	virtual int Run();					/* 0x811C4A74 */
	virtual void Finish(int result);			/* 0x811C4AC0 */

	paf::string m_titleId;					/* +36 */
	PatchCheckCacheData m_data;				/* +48 */
	int m_flags;						/* +64 */
};

/* 0x811C4A74 */
int PatchCheckSetJob::Run()
{
	char ctx[4];
	bool flag = (m_data.f8 != 1 && m_data.f8 != 3);
	return SceLsdb_79A6D877(ctx, &m_titleId, flag, &m_data, m_flags);
}

/* 0x811C4AC0 */
void PatchCheckSetJob::Finish(int result)
{
	if (result == 0) {
		paf::Plugin *plugin = paf::Plugin::Find("topmenu_plugin");
		if (plugin != NULL) {
			void *iface = plugin->GetInterface(1);
			if (iface != NULL) {
				struct {
					const char *titleId;
					int f4;
					int f8;
				} arg;
				arg.titleId = m_titleId.c_str();
				arg.f4 = 0;
				arg.f8 = 0x20000;
				void (*fn)(void *) = *(void (**)(void *))((char *)iface + 180);
				fn(&arg);
			}
		}
	}
}

paf::job::JobItem *patchCheckGetJobCtor(void *mem, IPMI::Session *session,
	SceUInt32 reqId, const char *titleId)
{
	return new (mem) PatchCheckGetJob(session, reqId, titleId);
}

paf::job::JobItem *patchCheckSetJobCtor(void *mem, IPMI::Session *session,
	const char *titleId, const PatchCheckCacheData *data, int flags)
{
	return new (mem) PatchCheckSetJob(session, titleId, data, flags);
}
