/*
 * SceShell shell_svc: SceAppInfoDb async dispatch & response helpers
 * 3.65-CEX: 0x811C0726 - 0x811C0998 (sti 0x811C0926)
 */

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/session.h>

#include "../../livespace_db/lsdb_keys.h"

extern int shellSvcAppInfoDbEnqueueSetKey(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key, int type, SceInt64 intVal,
	const char *strVal, SceInt64 ts);			/* 0x811C0164 */
extern int shellSvcAppInfoDbEnqueueSetMulti(IPMI::Session *session, SceUInt32 reqId,
	SceUInt32 count, const SceUInt32 *keys, const SceUInt32 *types,
	const SceInt64 *intVals, const char **strVals);		/* 0x811C0318 */
extern int shellSvcAppInfoDbEnqueueGetKey(IPMI::Session *session, SceUInt32 reqId,
	const char *titleId, SceUInt32 key);			/* 0x811C0434 */
extern int shellSvcAppInfoDbEnqueueDelete(IPMI::Session *session, SceUInt32 reqId,
	int key, int mode, SceInt64 bits, const char *str, SceInt64 mask); /* 0x811C0556 */

/* 0x811C0726 */
int shellSvcAppInfoDbSendGetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err,
	const int *type, const SceInt64 *val, const char *str)
{
	IPMI::DataInfo out[3];
	out[0].pData = type;
	out[0].dataSize = 4;
	out[1].pData = val;
	out[1].dataSize = 8;
	out[2].pData = str;
	out[2].dataSize = 2048;
	return session->respondToAsyncMethodRequest(0x90001, reqId, err, out, 3);
}

/* 0x811C077A */
int shellSvcAppInfoDbHandleSetMulti(IPMI::Session *session, SceUInt32 reqId,
	const IPMI::DataInfo *inBufs)
{
	const SceUInt32 *hdr = (const SceUInt32 *)inBufs[0].pData;
	SceUInt32 count = hdr[0];
	SceUInt32 strCount = hdr[1];
	const SceUInt32 *keys = (const SceUInt32 *)inBufs[1].pData;
	const SceUInt32 *types = (const SceUInt32 *)inBufs[2].pData;
	const SceInt64 *i64Vals = (const SceInt64 *)inBufs[3].pData;
	const char *strVals[64];
	for (SceUInt32 i = 0; i < strCount && i < 64; ++i)
		strVals[i] = (const char *)inBufs[4 + i].pData;
	return shellSvcAppInfoDbEnqueueSetMulti(session, reqId, count, keys, types, i64Vals, strVals);
}

/* 0x811C081A */
int shellSvcAppInfoDbSendSetMultiResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x90003, reqId, err, NULL, 0);
}

/* 0x811C083C */
int shellSvcAppInfoDbSendSetKeyResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x90005, reqId, err, NULL, 0);
}

/* 0x811C085E */
int shellSvcAppInfoDbSendDeleteResp(IPMI::Session *session, SceUInt32 reqId, int err)
{
	return session->respondToAsyncMethodRequest(0x90007, reqId, err, NULL, 0);
}

/* 0x811C087E */
void shellSvcAppInfoDbNoop(void)
{
}

struct AppInfoDbSetKeyReq {
	SceUInt32 key;
	int type;
	SceInt64 intVal;
	SceInt64 ts;
};

struct AppInfoDbDeleteReq {
	int key;
	int mode;
	SceInt64 bits;
	SceInt64 mask;
};

/* 0x811C0880 */
int shellSvcAppInfoDbDispatchAsync(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount)
{
	(void)inCount;
	switch (methodId) {
	case 0x90000:
		return shellSvcAppInfoDbEnqueueGetKey(session, reqId,
			(const char *)inBufs[1].pData, *(const SceUInt32 *)inBufs[0].pData);
	case 0x90002:
		return shellSvcAppInfoDbHandleSetMulti(session, reqId, inBufs);
	case 0x90004: {
		const AppInfoDbSetKeyReq *p = (const AppInfoDbSetKeyReq *)inBufs[0].pData;
		return shellSvcAppInfoDbEnqueueSetKey(session, reqId,
			(const char *)inBufs[1].pData, p->key, p->type,
			p->intVal, (const char *)inBufs[2].pData, p->ts);
	}
	case 0x90006: {
		const AppInfoDbDeleteReq *p = (const AppInfoDbDeleteReq *)inBufs[0].pData;
		return shellSvcAppInfoDbEnqueueDelete(session, reqId,
			p->key, p->mode, p->bits,
			(const char *)inBufs[1].pData, p->mask);
	}
	default:
		return (int)0x800205C1;
	}
}
