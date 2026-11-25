/*
 * SceShell shell_svc: SceAppInfoDb (0x00090000..0x0009FFFF) async job enqueuers
 * 3.65-CEX: 0x811C0164 - 0x811C0726 (sti 0x811C06B2)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

#include "../../livespace_db/lsdb_keys.h"

extern void *g_pafFramework;					/* 0x81488474 */

struct AppInfoDbValue {
	int type;						/* +0: 0=int64, 1=string* */
	int pad;						/* +4 */
	union {
		SceInt64 intVal;				/* +8 */
		paf::string *str;				/* +8 */
	};
};

extern int shellSvcLiveAreaSyncDefault(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32); /* 0x8115B554 */
extern int shellSvcAppInfoDbDispatchAsync(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount);	/* 0x811C0880 */
extern void shellSvcAppInfoDbCancelSession(IPMI::Session *session); /* 0x811C0A54 */

extern int shellSvcAppInfoDbSendGetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const int *type, const SceInt64 *val, const char *str);	/* 0x811C0726 */
extern int shellSvcAppInfoDbSendSetMultiResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C081A */
extern int shellSvcAppInfoDbSendSetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C083C */
extern int shellSvcAppInfoDbSendDeleteResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811C085E */

extern paf::job::JobItem *appInfoDbSetKeyJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key, const AppInfoDbValue *val, SceInt64 ts); /* 0x811C0A9A */
extern paf::job::JobItem *appInfoDbSetMultiJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
	const SceInt64 *intVals, const char **strVals);		/* 0x811C0C24 */
extern paf::job::JobItem *appInfoDbGetKeyJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key);			/* 0x811C0F32 */
extern paf::job::JobItem *appInfoDbDeleteJobCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	int key, int mode, SceInt64 bits, const char *str, SceInt64 mask); /* 0x811C10D0 */

static paf::job::JobQueue *shellSvcAppInfoDbJobQueue()
{
	return paf::job::JobQueue::default_queue;
}

/* 0x811C0164 */
int shellSvcAppInfoDbEnqueueSetKey(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key, int type, SceInt64 intVal,
	const char *strVal, SceInt64 ts)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(80);
	if (mem != NULL) {
		AppInfoDbValue val;
		val.type = 0;
		val.intVal = 0;
		if (type != 0) {
			val.type = 1;
			val.str = new paf::string(strVal ? strVal : "");
		} else {
			val.type = 0;
			val.intVal = intVal;
		}
		job = appInfoDbSetKeyJobCtor(mem, session, reqId, titleId, key, &val, ts);
		if (val.type == 1) {
			delete val.str;
			val.str = NULL;
		}
	}
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (!sp.get()) {
		shellSvcAppInfoDbSendSetKeyResp(session, reqId, (int)0x80101002);
		return 0;
	}
	int r = shellSvcAppInfoDbJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcAppInfoDbSendSetKeyResp(session, reqId, r);
	return 0;
}

/* 0x811C0318 */
int shellSvcAppInfoDbEnqueueSetMulti(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
	const SceInt64 *intVals, const char **strVals)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(44);
	if (mem != NULL)
		job = appInfoDbSetMultiJobCtor(mem, session, reqId, count, keys, types, intVals, strVals);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (!sp.get()) {
		shellSvcAppInfoDbSendSetMultiResp(session, reqId, (int)0x80101002);
		return 0;
	}
	int r = shellSvcAppInfoDbJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcAppInfoDbSendSetMultiResp(session, reqId, r);
	return 0;
}

/* 0x811C0434 */
int shellSvcAppInfoDbEnqueueGetKey(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(52);
	if (mem != NULL)
		job = appInfoDbGetKeyJobCtor(mem, session, reqId, titleId, key);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (!sp.get()) {
		int type = 0;
		SceInt64 val = 0;
		shellSvcAppInfoDbSendGetKeyResp(session, reqId, (int)0x80101002, &type, &val, "");
		return 0;
	}
	int r = shellSvcAppInfoDbJobQueue()->Enqueue(sp);
	if (r != 0) {
		int type = 0;
		SceInt64 val = 0;
		shellSvcAppInfoDbSendGetKeyResp(session, reqId, r, &type, &val, "");
	}
	return 0;
}

/* 0x811C0556 */
int shellSvcAppInfoDbEnqueueDelete(IPMI::Session *session, SceUInt32 reqId,
	int key, int mode, SceInt64 bits, const char *str, SceInt64 mask)
{
	paf::job::JobItem *job = NULL;
	void *mem = operator new(80);
	if (mem != NULL)
		job = appInfoDbDeleteJobCtor(mem, session, reqId, key, mode, bits, str, mask);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (!sp.get()) {
		shellSvcAppInfoDbSendDeleteResp(session, reqId, (int)0x80101002);
		return 0;
	}
	int r = shellSvcAppInfoDbJobQueue()->Enqueue(sp);
	if (r != 0)
		shellSvcAppInfoDbSendDeleteResp(session, reqId, r);
	return 0;
}

/* 0x811C0686 */
int shellSvcDisp_09_Sync(IPMI::Session *s, SceUInt32 m, const IPMI::DataInfo *in,
	SceUInt32 inCount, IPMI::BufferInfo *out, SceUInt32 outCount)
{
	return shellSvcLiveAreaSyncDefault(s, m, in, inCount, out, outCount);
}

/* 0x811C069A */
int shellSvcDisp_09_Async(IPMI::Session *s, SceUInt32 reqId, SceUInt32 m,
	const IPMI::DataInfo *in, SceUInt32 inCount)
{
	return shellSvcAppInfoDbDispatchAsync(s, reqId, m, in, inCount);
}

/* 0x811C06AA */
void shellSvcDisp_09_Disconn(IPMI::Session *s)
{
	shellSvcAppInfoDbCancelSession(s);
}
