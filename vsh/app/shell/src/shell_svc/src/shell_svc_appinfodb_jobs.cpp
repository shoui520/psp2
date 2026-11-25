/*
 * SceShell shell_svc: SceAppInfoDbJob classes
 * 3.65-CEX: 0x811C0998 - 0x811C1300 (sti 0x811C1222)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/list>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <thread/thread.h>

#include "../../livespace_db/lsdb_keys.h"

struct AppInfoDbValue {
	int type;						/* +0: 0=int64, 1=string* */
	int pad;						/* +4 */
	union {
		SceInt64 i64;					/* +8 */
		paf::string *str;				/* +8 */
	};

	AppInfoDbValue() : type(0), pad(0), i64(0) {}
	AppInfoDbValue(const AppInfoDbValue &o) : type(o.type), pad(0), i64(0)
	{
		if (type == 1)
			str = o.str ? new paf::string(*o.str) : new paf::string("");
		else
			i64 = o.i64;
	}
	~AppInfoDbValue()
	{
		if (type == 1 && str != NULL) {
			delete str;
			str = NULL;
		}
	}
};

extern "C" {
extern void SceLsdb_A6D8772C(void *ctx);
extern int SceLsdb_E90B81CD(void *ctx);
extern void SceLsdb_E2101B03(void *ctx);
extern int SceLsdb_C36501F7(const paf::string *titleId, SceUInt32 key, const AppInfoDbValue *val);
extern int SceLsdb_9EC8F00A(const paf::common::SharedPtr<paf::map<SceUInt32, AppInfoDbValue> > *mapPtr);
extern int SceLsdb_65ECEF39(void *ctx, const paf::string *titleId, SceUInt32 key, AppInfoDbValue *outVal);
extern int SceLsdb_70A430B1(int arg1, const char *str);
extern int SceLsdb_BCFEAC86(int arg1, int zero, SceInt64 arg2, SceInt64 arg3);
}

extern int shellSvcAppInfoDbSendGetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const int *type, const SceInt64 *val, const char *str);	/* 0x811C0726 */
extern int shellSvcAppInfoDbSendSetMultiResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C081A */
extern int shellSvcAppInfoDbSendSetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C083C */
extern int shellSvcAppInfoDbSendDeleteResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C085E */

class AppInfoDbJobBase : public paf::job::JobItem {
public:
	AppInfoDbJobBase(IPMI::Session *session, SceUInt32 reqId);
	virtual ~AppInfoDbJobBase();				/* 0x811C0998 / 0x811C09F4 */
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	IPMI::Session *m_session;				/* +28 */
	SceUInt32 m_reqId;					/* +32 */
};

static paf::list<AppInfoDbJobBase *> s_appInfoDbJobs;		/* 0x8156B768 */
static paf::thread::RMutex s_appInfoDbMutex("SceAppInfoDbJob", 0); /* 0x8156B778 */

AppInfoDbJobBase::AppInfoDbJobBase(IPMI::Session *session, SceUInt32 reqId)
	: paf::job::JobItem("SceAppInfoDbJob", NULL), m_session(session), m_reqId(reqId)
{
	s_appInfoDbMutex.Lock();
	s_appInfoDbJobs.push_back(this);
	s_appInfoDbMutex.Unlock();
}

/* 0x811C0998 / 0x811C09F4 */
AppInfoDbJobBase::~AppInfoDbJobBase()
{
	s_appInfoDbMutex.Lock();
	s_appInfoDbJobs.remove(this);
	s_appInfoDbMutex.Unlock();
}

/* 0x811C0A54 */
void shellSvcAppInfoDbCancelSession(IPMI::Session *session)
{
	s_appInfoDbMutex.Lock();
	for (paf::list<AppInfoDbJobBase *>::iterator it = s_appInfoDbJobs.begin();
	     it != s_appInfoDbJobs.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Cancel();
			break;
		}
	}
	s_appInfoDbMutex.Unlock();
}

class AppInfoDbSetKeyJob : public AppInfoDbJobBase {
public:
	AppInfoDbSetKeyJob(IPMI::Session *session, SceUInt32 reqId,
		const char *titleId, SceUInt32 key, const AppInfoDbValue *val, SceInt64 ts); /* 0x811C0A9A */
	virtual ~AppInfoDbSetKeyJob() {}
	virtual int Run();					/* 0x811C0BD6 */

	paf::string m_titleId;					/* +36 */
	SceUInt32 m_key;					/* +48 */
	SceInt64 m_ts;						/* +56 */
	AppInfoDbValue m_val;					/* +64 */
};

/* 0x811C0A9A */
AppInfoDbSetKeyJob::AppInfoDbSetKeyJob(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key, const AppInfoDbValue *val, SceInt64 ts)
	: AppInfoDbJobBase(session, reqId),
	  m_titleId(titleId ? titleId : ""),
	  m_key(key),
	  m_ts(ts),
	  m_val(*val)
{
}

/* 0x811C0BD6 */
int AppInfoDbSetKeyJob::Run()
{
	int r = SceLsdb_C36501F7(&m_titleId, m_key, &m_val);
	s_appInfoDbMutex.Lock();
	if (!IsCanceled())
		r = shellSvcAppInfoDbSendSetKeyResp(m_session, m_reqId, r);
	s_appInfoDbMutex.Unlock();
	return r;
}

class AppInfoDbSetMultiJob : public AppInfoDbJobBase {
public:
	AppInfoDbSetMultiJob(IPMI::Session *session, SceUInt32 reqId,
		SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
		const SceInt64 *i64Vals, const char **strVals);	/* 0x811C0C24 */
	virtual ~AppInfoDbSetMultiJob() {}
	virtual int Run();					/* 0x811C0EF6 */

	paf::common::SharedPtr<paf::map<SceUInt32, AppInfoDbValue> > m_map; /* +36 */
};

/* 0x811C0C24 */
AppInfoDbSetMultiJob::AppInfoDbSetMultiJob(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
	const SceInt64 *i64Vals, const char **strVals)
	: AppInfoDbJobBase(session, reqId),
	  m_map(new paf::map<SceUInt32, AppInfoDbValue>())
{
	AppInfoDbValue tmp;
	for (SceUInt32 i = 0; i < count; ++i) {
		if (types[i] != 0) {
			if (tmp.type == 1 && tmp.str != NULL) {
				*tmp.str = strVals[i] ? strVals[i] : "";
			} else {
				tmp.str = new paf::string(strVals[i] ? strVals[i] : "");
			}
			tmp.type = 1;
		} else {
			if (tmp.type == 1) {
				delete tmp.str;
				tmp.str = NULL;
			}
			tmp.i64 = i64Vals[i];
			tmp.type = 0;
		}
		m_map->insert(paf::pair<SceUInt32, AppInfoDbValue>(keys[i], tmp));
	}
}

/* 0x811C0EF6 */
int AppInfoDbSetMultiJob::Run()
{
	int r = SceLsdb_9EC8F00A(&m_map);
	s_appInfoDbMutex.Lock();
	if (!IsCanceled())
		r = shellSvcAppInfoDbSendSetMultiResp(m_session, m_reqId, r);
	s_appInfoDbMutex.Unlock();
	return r;
}

class AppInfoDbGetKeyJob : public AppInfoDbJobBase {
public:
	AppInfoDbGetKeyJob(IPMI::Session *session, SceUInt32 reqId,
		const char *titleId, SceUInt32 key);		/* 0x811C0F32 */
	virtual ~AppInfoDbGetKeyJob() {}
	virtual int Run();					/* 0x811C0FE0 */

	paf::string m_titleId;					/* +36 */
	SceUInt32 m_key;					/* +48 */
};

/* 0x811C0F32 */
AppInfoDbGetKeyJob::AppInfoDbGetKeyJob(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key)
	: AppInfoDbJobBase(session, reqId),
	  m_titleId(titleId ? titleId : ""),
	  m_key(key)
{
}

/* 0x811C0FE0 */
int AppInfoDbGetKeyJob::Run()
{
	AppInfoDbValue val;
	char ctx[4];
	SceLsdb_A6D8772C(ctx);
	int r = SceLsdb_E90B81CD(ctx);
	if (r == 0)
		r = SceLsdb_65ECEF39(ctx, &m_titleId, m_key, &val);
	SceLsdb_E2101B03(ctx);

	s_appInfoDbMutex.Lock();
	if (!IsCanceled()) {
		int type = val.type;
		SceInt64 i64 = (val.type == 0) ? val.i64 : 0;
		const char *str = (val.type == 1 && val.str != NULL) ? val.str->c_str() : "";
		r = shellSvcAppInfoDbSendGetKeyResp(m_session, m_reqId, r, &type, &i64, str);
	}
	s_appInfoDbMutex.Unlock();
	return r;
}

class AppInfoDbDeleteJob : public AppInfoDbJobBase {
public:
	AppInfoDbDeleteJob(IPMI::Session *session, SceUInt32 reqId,
		int arg1, int mode, SceInt64 arg2, const char *str, SceInt64 arg3); /* 0x811C10D0 */
	virtual ~AppInfoDbDeleteJob() {}
	virtual int Run();					/* 0x811C11C2 */

	int m_arg1;						/* +36 */
	int m_mode;						/* +40 */
	SceInt64 m_arg2;					/* +48 */
	paf::string m_str;					/* +56 */
	SceInt64 m_arg3;					/* +72 */
};

/* 0x811C10D0 */
AppInfoDbDeleteJob::AppInfoDbDeleteJob(IPMI::Session *session, SceUInt32 reqId,
	int arg1, int mode, SceInt64 arg2, const char *str, SceInt64 arg3)
	: AppInfoDbJobBase(session, reqId),
	  m_arg1(arg1), m_mode(mode), m_arg2(arg2),
	  m_str(str ? str : ""), m_arg3(arg3)
{
}

/* 0x811C11C2 */
int AppInfoDbDeleteJob::Run()
{
	int r = 0;
	if (m_mode == 1)
		r = SceLsdb_70A430B1(m_arg1, m_str.c_str());
	else if (m_mode == 0)
		r = SceLsdb_BCFEAC86(m_arg1, 0, m_arg2, m_arg3);

	s_appInfoDbMutex.Lock();
	if (!IsCanceled())
		r = shellSvcAppInfoDbSendDeleteResp(m_session, m_reqId, r);
	s_appInfoDbMutex.Unlock();
	return r;
}

paf::job::JobItem *appInfoDbSetKeyJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key, const AppInfoDbValue *val, SceInt64 ts)
{
	return new (mem) AppInfoDbSetKeyJob(session, reqId, titleId, key, val, ts);
}

paf::job::JobItem *appInfoDbSetMultiJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
	const SceInt64 *i64Vals, const char **strVals)
{
	return new (mem) AppInfoDbSetMultiJob(session, reqId, count, keys, types, i64Vals, strVals);
}

paf::job::JobItem *appInfoDbGetKeyJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key)
{
	return new (mem) AppInfoDbGetKeyJob(session, reqId, titleId, key);
}

paf::job::JobItem *appInfoDbDeleteJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	int arg1, int mode, SceInt64 arg2, const char *str, SceInt64 arg3)
{
	return new (mem) AppInfoDbDeleteJob(session, reqId, arg1, mode, arg2, str, arg3);
}
