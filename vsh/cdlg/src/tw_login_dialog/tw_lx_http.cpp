/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: Lx HTTP client wrapper over SceHttp.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libhttp.h>
#include <std/string.h>
#include "tw_lx_http.h"

struct LxHttpMethodEntry {
	const char *name;
	SceInt32 method;
};

static const LxHttpMethodEntry s_httpMethods[8] = {
	{ "GET", SCE_HTTP_METHOD_GET },
	{ "POST", SCE_HTTP_METHOD_POST },
	{ "HEAD", SCE_HTTP_METHOD_HEAD },
	{ "OPTIONS", SCE_HTTP_METHOD_OPTIONS },
	{ "PUT", SCE_HTTP_METHOD_PUT },
	{ "DELETE", SCE_HTTP_METHOD_DELETE },
	{ "TRACE", SCE_HTTP_METHOD_TRACE },
	{ "CONNECT", SCE_HTTP_METHOD_CONNECT },
};

static LxRefPtr<LxHttpTemplate> s_defaultHttpTemplate;

/* LxHttpConnection */
LxHttpConnection::LxHttpConnection()
	: m_connId(-1), m_host(), m_scheme(), m_port(0)
{
}

LxHttpConnection::~LxHttpConnection()
{
	if (m_connId >= 0) {
		sceHttpDeleteConnection(m_connId);
		m_connId = -1;
	}
}

/* LxHttpConnectionPool */
int LxHttpConnectionPool::Acquire(LxRefPtr<LxHttpConnection> &outConn, SceInt32 tmplId,
				  const char *url)
{
	SceSize require = 0;
	int ret = sceHttpUriParse(NULL, url, NULL, &require, 0);

	if (ret < 0)
		return ret;
	LxBuffer poolBuf;

	if (!poolBuf.Resize(require))
		return LX_ERROR_HTTP_NO_MEM_POOL;
	SceHttpUriElement uri;

	ret = sceHttpUriParse(&uri, url, poolBuf.begin, &require, poolBuf.Capacity());
	if (ret < 0)
		return ret;

	LxRefPtr<LxHttpConnection> conn(new LxHttpConnection());
	SceInt32 connId = sceHttpCreateConnection(tmplId, uri.hostname, uri.scheme, uri.port, 1);

	if (connId < 0)
		return connId;

	conn->m_connId = connId;
	conn->m_host = LxString(uri.hostname);
	conn->m_scheme = LxString(uri.scheme);
	conn->m_port = uri.port;
	outConn = conn;
	return 0;
}

/* LxHttpTemplate */
LxHttpTemplate::LxHttpTemplate(const LxHttpTemplateParam &param)
	: m_tmplId(-1), m_pool(param.pool), m_flushCookies(param.flushCookies),
	  m_activeRequests(0)
{
	SceInt32 id = sceHttpCreateTemplate(param.userAgent.CStr(), param.httpVersion,
					    param.autoProxy);

	if (id >= 0)
		m_tmplId = id;
	if (m_pool.IsNull())
		m_pool = LxRefPtr<LxHttpConnectionPool>(new LxHttpConnectionPool());
}

LxHttpTemplate::~LxHttpTemplate()
{
	if (m_tmplId >= 0) {
		sceHttpDeleteTemplate(m_tmplId);
		m_tmplId = -1;
	}
}

/* Global default HTTP template */
void LxHttpInit()
{
	LxHttpTemplateParam param;

	param.httpVersion = SCE_HTTP_VERSION_1_1;
	param.autoProxy = 1;
	param.flushCookies = 0;
	param.pool = LxRefPtr<LxHttpConnectionPool>(new LxHttpConnectionPool());

	s_defaultHttpTemplate = LxRefPtr<LxHttpTemplate>(new LxHttpTemplate(param));
	if (!s_defaultHttpTemplate.IsNull() && s_defaultHttpTemplate->m_tmplId >= 0) {
		SceInt32 tmplId = s_defaultHttpTemplate->m_tmplId;

		sceHttpSetResolveRetry(tmplId, 2);
		sceHttpSetResolveTimeOut(tmplId, 4000000U);
		sceHttpSetConnectTimeOut(tmplId, 30000000U);
		sceHttpSetSendTimeOut(tmplId, 30000000U);
		sceHttpSetRecvTimeOut(tmplId, 30000000U);
	}
}

void LxHttpTerm()
{
	s_defaultHttpTemplate = NULL;
}

LxRefPtr<LxHttpTemplate> LxHttpGetDefaultTemplate()
{
	return s_defaultHttpTemplate;
}

/* LxHttpRequest */
static void AtomicInc(volatile unsigned int *val)
{
	unsigned int prev;

	__builtin_dmb();
	do {
		prev = __builtin_ldrex(val);
	} while (__builtin_strex(prev + 1, val) != 0);
	__builtin_dmb();
}

static unsigned int AtomicDec(volatile unsigned int *val)
{
	unsigned int prev;

	__builtin_dmb();
	do {
		prev = __builtin_ldrex(val);
	} while (__builtin_strex(prev - 1, val) != 0);
	__builtin_dmb();
	return prev;
}

LxHttpRequest::LxHttpRequest(const LxRefPtr<LxHttpTemplate> &tmpl)
	: m_tmpl(tmpl), m_conn(), m_abortWatcher(NULL), m_method(0), m_reqId(-1), m_url(),
	  m_redirectUrl(), m_sent(0)
{
	if (!m_tmpl.IsNull())
		AtomicInc(&m_tmpl->m_activeRequests);
}

LxHttpRequest::~LxHttpRequest()
{
	Close();
	if (!m_tmpl.IsNull()) {
		if (AtomicDec(&m_tmpl->m_activeRequests) == 1 && m_tmpl->m_flushCookies != 0)
			sceHttpCookieFlush();
	}
}

bool LxHttpRequest::CanReuseConnection()
{
	if (m_conn.IsNull())
		return false;
	if (m_reqId < 0 || m_sent == 0)
		return true;
	if (m_method != SCE_HTTP_METHOD_GET && m_method != SCE_HTTP_METHOD_HEAD)
		return false;
	if (m_abortWatcher != NULL && m_abortWatcher->m_condition->IsAborted() != 0)
		return false;
	SceInt32 err = 0;

	if ((sceHttpGetLastErrno(m_reqId, &err) | err) < 0)
		return false;
	uint8_t probeByte = 0;

	return sceHttpReadData(m_reqId, &probeByte, 1) == 0;
}

int LxHttpRequest::Close()
{
	bool reusable = CanReuseConnection();

	if (m_abortWatcher != NULL) {
		delete m_abortWatcher;
		m_abortWatcher = NULL;
	}
	if (m_reqId >= 0) {
		SceInt32 err = 0;

		sceHttpGetLastErrno(m_reqId, &err);
		sceHttpDeleteRequest(m_reqId);
		m_reqId = -1;
	}
	if (!m_conn.IsNull()) {
		if (reusable && !m_tmpl.IsNull() && !m_tmpl->m_pool.IsNull())
			m_tmpl->m_pool->ReleaseConnection(m_conn);
		m_conn = NULL;
	}
	m_url.Assign(NULL, 0);
	return 0;
}

static void OnRequestAbort(void *userData)
{
	LxHttpRequest *req = (LxHttpRequest *)userData;

	req->Abort();
}

int LxHttpRequest::Open(const char *url, const char *method, uint64_t contentLength,
			LxAbortCondition *abortCond)
{
	Close();
	if (m_tmpl.IsNull())
		return LX_ERROR_HTTP_NO_TEMPLATE;
	if (m_tmpl->m_pool.IsNull())
		return LX_ERROR_HTTP_NO_POOL;

	LxRefPtr<LxHttpConnection> conn;
	int ret = m_tmpl->m_pool->Acquire(conn, m_tmpl->m_tmplId, url);

	if (ret < 0)
		return ret;

	SceInt32 methodId = SCE_HTTP_METHOD_GET;

	for (unsigned int i = 0; i < 8; ++i) {
		if (sce_paf_strcmp(method, s_httpMethods[i].name) == 0) {
			methodId = s_httpMethods[i].method;
			break;
		}
	}
	m_method = methodId;

	SceInt32 reqId = sceHttpCreateRequestWithURL(conn->m_connId, methodId, url, contentLength);

	if (reqId < 0)
		return reqId;

	m_conn = conn;
	m_reqId = reqId;
	if (abortCond != NULL)
		m_abortWatcher = new LxAbortWatcher(abortCond, OnRequestAbort, this, true);
	sceHttpSetRedirectCallback(m_reqId, OnRedirect, this);
	m_url = LxString(url);
	return 0;
}

int LxHttpRequest::AddHeader(const char *name, const char *value)
{
	return sceHttpAddRequestHeader(m_reqId, name, value, SCE_HTTP_HEADER_OVERWRITE);
}

int LxHttpRequest::GetStatusCode(int *statusCode)
{
	return sceHttpGetStatusCode(m_reqId, statusCode);
}

int LxHttpRequest::Send(const void *postData, SceSize size)
{
	if (m_abortWatcher != NULL && m_abortWatcher->m_condition->IsAborted() != 0)
		return LX_ERROR_HTTP_ABORTED;
	m_sent = 1;
	return sceHttpSendRequest(m_reqId, postData, size);
}

int LxHttpRequest::Read(void *buf, SceSize size, SceSize *bytesRead)
{
	if (m_abortWatcher != NULL && m_abortWatcher->m_condition->IsAborted() != 0)
		return LX_ERROR_HTTP_ABORTED;
	int ret = sceHttpReadData(m_reqId, buf, size);

	if (ret >= 0) {
		if (bytesRead != NULL)
			*bytesRead = (SceSize)ret;
		return 0;
	}
	return ret;
}

int LxHttpRequest::Abort()
{
	return sceHttpAbortRequest(m_reqId);
}

int LxHttpRequest::OnRedirect(SceInt32 reqId, SceInt32 statusCode, SceInt32 *method,
			      const char *location, void *userArg)
{
	(void)reqId;
	(void)statusCode;
	(void)method;
	LxHttpRequest *self = (LxHttpRequest *)userArg;

	self->m_redirectUrl = LxString(location);
	return 0;
}
