/*
 * SceNpCommonPs4: HttpTrans (npext/http/http_trans.cpp), one HTTP connection with one request on it.
 * The connection comes from a connection pool or is created for the transaction alone.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

/* a Scope that aborts the request when it is cancelled (m_reqId follows the Scope) */
struct NpCommonPs4HttpScope {
	struct NpCommonPs4Scope scope;
	SceInt32 reqId;
};

extern const void *g_httpScopeVtable[];		/* 0x8102a740 */
extern const void *g_httpTransVtable[];		/* 0x8102a714: the vtables of HttpTrans and of its second base */
struct NpCommonPs4HttpScope *npCommonPs4HttpScopeDestruct(struct NpCommonPs4HttpScope *scope);
void npCommonPs4HttpScopeDelete(struct NpCommonPs4HttpScope *scope);
int npCommonPs4HttpScopeCancel(struct NpCommonPs4HttpScope *scope);

static inline void httpScopeInit(struct NpCommonPs4HttpScope *scope, SceInt32 reqId)
{
	SceNpCommonPs4_5BBB1ABA(&scope->scope);
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_httpScopeVtable);
	scope->reqId = reqId;
}

static inline void httpScopeTerm(struct NpCommonPs4HttpScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_httpScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
}

struct NpCommonPs4HttpTrans *SceNpCommonPs4_6FCD2FD2(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Allocator *pool)
{
	trans->pool = pool;
	trans->vtable = NP_COMMON_PS4_VTABLE(g_httpTransVtable);
	trans->vtableSecondBase = &g_httpTransVtable[8];
	trans->connPool = NULL;
	trans->reqId = 0;
	trans->connId = 0;
	return trans;
}

struct NpCommonPs4HttpTrans *SceNpCommonPs4_9256CF02(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Allocator *pool)
{
	trans->pool = pool;
	trans->vtable = NP_COMMON_PS4_VTABLE(g_httpTransVtable);
	trans->vtableSecondBase = &g_httpTransVtable[8];
	trans->connPool = NULL;
	trans->reqId = 0;
	trans->connId = 0;
	return trans;
}

__attribute__((noinline)) struct NpCommonPs4HttpTrans *SceNpCommonPs4_4A41173D(struct NpCommonPs4HttpTrans *trans)
{
	SceInt32 reqId;

	reqId = trans->reqId;
	trans->vtable = NP_COMMON_PS4_VTABLE(g_httpTransVtable);
	trans->vtableSecondBase = &g_httpTransVtable[8];
	if (reqId != 0)
		sceHttpDeleteRequest(reqId);
	if (trans->connId != 0) {
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolRelease(trans->connPool, trans->connId);
		else
			sceHttpDeleteConnection(trans->connId);
	}
	trans->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	trans->vtableSecondBase = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return trans;
}

__attribute__((noinline)) int SceNpCommonPs4_C06E108A(struct NpCommonPs4HttpTrans *trans)
{
	SceNpCommonPs4_4A41173D(trans);
	return SceNpCommonPs4_BB5282C8(trans);
}

struct NpCommonPs4HttpTrans *SceNpCommonPs4_47C5936B(struct NpCommonPs4HttpTrans *trans)
{
	return SceNpCommonPs4_4A41173D(trans);
}

int SceNpCommonPs4_53E8E212(struct NpCommonPs4HttpTrans *trans)
{
	return trans->reqId != 0;
}

int SceNpCommonPs4_AFA1D770(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4HttpTemplate *tmpl,
	struct NpCommonPs4HttpConnPool *connPool, SceInt32 method, const char *scheme, const char *serverName, SceUShort16 port,
	const char *path, SceULong64 contentLength)
{
	int result;
	SceInt32 connId;
	SceInt32 reqId;

	if (trans->reqId != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	NP_COMMON_PS4_ASSERT(tmpl->templateId != 0, "Init", "npext\\http\\http_trans.cpp", 78, "tmpl.GetTemplateId() != 0");
	NP_COMMON_PS4_ASSERT(trans->connId == 0, "Init", "npext\\http\\http_trans.cpp", 79, "m_connId == 0");
	NP_COMMON_PS4_ASSERT(trans->reqId == 0, "Init", "npext\\http\\http_trans.cpp", 80, "m_reqId == 0");
	if (connPool != NULL) {
		result = npCommonPs4HttpConnPoolGetConnection(connPool, tmpl, scheme, serverName, port, &trans->connId);
		connId = trans->connId;
		if (result < 0)
			goto fail;
	} else {
		connId = sceHttpCreateConnection(tmpl->templateId, serverName, scheme, port, 1);
		result = connId;
		if (connId < 0)
			goto fail;
		trans->connId = result;
	}
	trans->connPool = connPool;
	reqId = sceHttpCreateRequest(connId, method, path, contentLength);
	result = reqId;
	if (reqId >= 0) {
		trans->reqId = reqId;
		trans->receivedSize = 0;
		return 0;
	}
fail:
	if (trans->reqId != 0) {
		sceHttpDeleteRequest(trans->reqId);
		trans->reqId = 0;
	}
	if (trans->connId != 0) {
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolRelease(trans->connPool, trans->connId);
		else
			sceHttpDeleteConnection(trans->connId);
		trans->connPool = NULL;
		trans->connId = 0;
	}
	return result;
}

int SceNpCommonPs4_D30A8E84(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4HttpTemplate *tmpl,
	struct NpCommonPs4HttpConnPool *connPool, SceInt32 method, const char *url, SceULong64 contentLength)
{
	int result;
	SceInt32 connId;
	SceInt32 reqId;

	if (trans->reqId != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	NP_COMMON_PS4_ASSERT(tmpl->templateId != 0, "Init", "npext\\http\\http_trans.cpp", 159, "tmpl.GetTemplateId() != 0");
	NP_COMMON_PS4_ASSERT(trans->connId == 0, "Init", "npext\\http\\http_trans.cpp", 160, "m_connId == 0");
	NP_COMMON_PS4_ASSERT(trans->reqId == 0, "Init", "npext\\http\\http_trans.cpp", 161, "m_reqId == 0");
	if (connPool != NULL) {
		result = npCommonPs4HttpConnPoolGetConnectionWithUrl(connPool, tmpl, url, &trans->connId);
		connId = trans->connId;
		if (result < 0)
			goto fail;
	} else {
		connId = sceHttpCreateConnectionWithURL(tmpl->templateId, url, 1);
		result = connId;
		if (connId < 0)
			goto fail;
		trans->connId = connId;
	}
	trans->connPool = connPool;
	reqId = sceHttpCreateRequestWithURL(connId, method, url, contentLength);
	result = reqId;
	if (reqId >= 0) {
		trans->reqId = reqId;
		trans->receivedSize = 0;
		return 0;
	}
fail:
	if (trans->reqId != 0) {
		sceHttpDeleteRequest(trans->reqId);
		trans->reqId = 0;
	}
	if (trans->connId != 0) {
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolRelease(trans->connPool, trans->connId);
		else
			sceHttpDeleteConnection(trans->connId);
		trans->connPool = NULL;
		trans->connId = 0;
	}
	return result;
}

int SceNpCommonPs4_BB371047(struct NpCommonPs4HttpTrans *trans)
{
	int result;

	if (trans->reqId != 0)
		sceHttpDeleteRequest(trans->reqId);
	if (trans->connId != 0) {
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolRelease(trans->connPool, trans->connId);
		else
			sceHttpDeleteConnection(trans->connId);
		trans->connPool = NULL;
	}
	if (trans->reqId != 0 || (result = trans->connId) != 0) {
		trans->connId = 0;
		trans->reqId = 0;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_3E6B0342(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4HttpTimeouts *timeouts)
{
	int result;

	if (timeouts->connectTimeout == 0 ||
	    (result = sceHttpSetConnectTimeOut(trans->reqId, timeouts->connectTimeout)) >= 0) {
		if (timeouts->sendTimeout == 0 ||
		    (result = sceHttpSetSendTimeOut(trans->reqId, timeouts->sendTimeout)) >= 0) {
			if (timeouts->recvTimeout == 0)
				return 0;
			result = sceHttpSetRecvTimeOut(trans->reqId, timeouts->recvTimeout);
			if (result >= 0)
				return 0;
		}
	}
	return result;
}

int SceNpCommonPs4_44FF13E8(struct NpCommonPs4HttpTrans *trans, const char *name, const char *value)
{
	int result;

	result = sceHttpAddRequestHeader(trans->reqId, name, value, 0);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_9D6CADE6(struct NpCommonPs4HttpTrans *trans, const char *name, const char *value)
{
	int result;

	result = sceHttpAddRequestHeader(trans->reqId, name, value, 1);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_2708F8BF(struct NpCommonPs4HttpTrans *trans, const char *value)
{
	int reqId;
	int result;

	reqId = trans->reqId;
	if (reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "SetRequestContentType", "npext\\http\\http_trans.cpp", 329, "IsInit()");
		reqId = trans->reqId;
	}
	result = sceHttpAddRequestHeader(reqId, "Content-Type", value, 0);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_30A21F21(struct NpCommonPs4HttpTrans *trans, SceULong64 contentLength)
{
	int reqId;
	int result;

	reqId = trans->reqId;
	if (reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "SetRequestContentLength", "npext\\http\\http_trans.cpp", 345, "IsInit()");
		reqId = trans->reqId;
	}
	result = sceHttpSetRequestContentLength(reqId, contentLength);
	if (result >= 0)
		return 0;
	if (trans->connPool != NULL)
		npCommonPs4HttpConnPoolReportError(trans->connPool, trans->connId, result);
	return result;
}

int SceNpCommonPs4_3FC1A88F(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle,
	const void *data, SceSize size)
{
	int reqId;
	int result;
	int sent;
	struct NpCommonPs4HttpScope scope;

	reqId = trans->reqId;
	if (reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "SendRequest", "npext\\http\\http_trans.cpp", 369, "IsInit()");
		reqId = trans->reqId;
	}
	httpScopeInit(&scope, reqId);
	result = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
	if (result < 0 ||
	    (sent = sceHttpSendRequest(trans->reqId, data, size),
	     (result = SceNpCommonPs4_3D0FA7C9(&scope.scope, sent, 0, 0, 0)) < 0)) {
		httpScopeTerm(&scope);
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolReportError(trans->connPool, trans->connId, result);
		return result;
	}
	httpScopeTerm(&scope);
	return 0;
}

int SceNpCommonPs4_6B6E813E(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, SceInt32 *pStatusCode)
{
	int reqId;
	int result;
	int status;
	struct NpCommonPs4HttpScope scope;

	reqId = trans->reqId;
	if (reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "GetResponseStatusCode", "npext\\http\\http_trans.cpp", 407, "IsInit()");
		reqId = trans->reqId;
	}
	httpScopeInit(&scope, reqId);
	result = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
	if (result < 0 ||
	    (status = sceHttpGetStatusCode(trans->reqId, pStatusCode),
	     (result = SceNpCommonPs4_3D0FA7C9(&scope.scope, status, 0, 0, 0)) < 0)) {
		httpScopeTerm(&scope);
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolReportError(trans->connPool, trans->connId, result);
		return result;
	}
	httpScopeTerm(&scope);
	return 0;
}

int SceNpCommonPs4_81B8FF72(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, bool *pIsEmpty,
	SceULong64 *pContentLength)
{
	int reqId;
	int result;
	int status;
	struct NpCommonPs4HttpScope scope;

	reqId = trans->reqId;
	if (reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "GetResponseContentLength", "npext\\http\\http_trans.cpp", 441, "IsInit()");
		reqId = trans->reqId;
	}
	httpScopeInit(&scope, reqId);
	result = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
	if (result < 0 ||
	    (status = sceHttpGetResponseContentLength(trans->reqId, pContentLength),
	     (result = SceNpCommonPs4_3D0FA7C9(&scope.scope, status, 0, 0, 0)) < 0)) {
		httpScopeTerm(&scope);
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolReportError(trans->connPool, trans->connId, result);
		return result;
	}
	httpScopeTerm(&scope);
	*pIsEmpty = status == 0;
	return 0;
}

int SceNpCommonPs4_C80B0275(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, void *buffer,
	SceSize size, SceSize *pReceived)
{
	int result;
	int received;
	int length;
	void *chunk;
	struct NpCommonPs4HttpScope scope;

	if (trans->reqId == 0) {
		NP_COMMON_PS4_ASSERT(0, "RecvResponse", "npext\\http\\http_trans.cpp", 492, "IsInit()");
	}
	chunk = NULL;
	if (pReceived != NULL)
		*pReceived = 0;
	if ((buffer != NULL && size != 0) || (chunk = SceNpCommonPs4_299ADFC3(trans->pool, 0x8000)) != NULL) {
		do {
			httpScopeInit(&scope, trans->reqId);
			result = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
			if (result < 0) {
				httpScopeTerm(&scope);
				goto fail;
			}
			if (chunk != NULL) {
				received = sceHttpReadData(trans->reqId, chunk, 0x8000);
				result = SceNpCommonPs4_3D0FA7C9(&scope.scope, received, 0, 0, 0);
				if (result < 0)
					goto scopeFail;
				length = received;
			} else {
				received = sceHttpReadData(trans->reqId, buffer, size);
				result = SceNpCommonPs4_3D0FA7C9(&scope.scope, received, 0, 0, 0);
				if (result < 0) {
scopeFail:
					httpScopeTerm(&scope);
					goto fail;
				}
				length = received;
			}
			httpScopeTerm(&scope);
			if (pReceived != NULL)
				*pReceived += length;
			trans->receivedSize += (SceUInt32)length;
		} while (chunk != NULL && length != 0);
		result = 0;
	} else {
		result = ((int *)trans->pool)[4];
fail:
		if (trans->connPool != NULL)
			npCommonPs4HttpConnPoolReportError(trans->connPool, trans->connId, result);
	}
	if (chunk != NULL)
		SceNpCommonPs4_38CC9AF5(trans->pool, chunk);
	return result;
}

int SceNpCommonPs4_2E564583(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle)
{
	return SceNpCommonPs4_C80B0275(trans, handle, NULL, 0, NULL);
}

int SceNpCommonPs4_47443DCE(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, void *buffer,
	SceSize size, SceSize *pReceived)
{
	int result;

	result = SceNpCommonPs4_C80B0275(trans, handle, buffer, size, pReceived);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_F81085EE(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, const void *data,
	SceSize size, SceSize *pSent)
{
	int result;

	result = SceNpCommonPs4_3FC1A88F(trans, handle, data, size);
	if (result >= 0) {
		if (pSent != NULL)
			*pSent = size;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_9D70AA39(char *secondBase)
{
	return SceNpCommonPs4_C06E108A((struct NpCommonPs4HttpTrans *)(secondBase - 4));
}

struct NpCommonPs4HttpTrans *SceNpCommonPs4_E563B7DB(char *secondBase)
{
	return SceNpCommonPs4_4A41173D((struct NpCommonPs4HttpTrans *)(secondBase - 4));
}

int SceNpCommonPs4_8EBA83E1(char *secondBase, struct NpCommonPs4Handle *handle, const void *data, SceSize size,
	SceSize *pSent)
{
	return SceNpCommonPs4_F81085EE((struct NpCommonPs4HttpTrans *)(secondBase - 4), handle, data, size, pSent);
}

struct NpCommonPs4HttpScope *npCommonPs4HttpScopeDestruct(struct NpCommonPs4HttpScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_httpScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	return scope;
}

int npCommonPs4HttpScopeCancel(struct NpCommonPs4HttpScope *scope)
{
	return sceHttpAbortRequest(scope->reqId);
}

void npCommonPs4HttpScopeDelete(struct NpCommonPs4HttpScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_httpScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	operator delete(scope);
}

const void *g_httpTransVtable[] = {		/* 0x8102a714 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_4A41173D,
	(const void *)SceNpCommonPs4_C06E108A,
	(const void *)SceNpCommonPs4_47443DCE,
	(const void *)SceNpCommonPs4_F81085EE,
	(const void *)-4,
	NULL,
	(const void *)SceNpCommonPs4_E563B7DB,
	(const void *)SceNpCommonPs4_9D70AA39,
	(const void *)SceNpCommonPs4_8EBA83E1,
};

const void *g_httpScopeVtable[] = {		/* 0x8102a740 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)npCommonPs4HttpScopeDestruct,
	(const void *)npCommonPs4HttpScopeDelete,
	(const void *)npCommonPs4HttpScopeCancel,
};

}
