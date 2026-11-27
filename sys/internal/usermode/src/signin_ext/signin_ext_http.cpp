/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * HttpConnection, HttpRequestJob, and HttpClient — pooled HTTP/HTTPS GET
 * client with keep-alive connection reuse and auth callback.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <url/url.h>
#include <libhttp.h>
#include "signin_ext_internal.h"

extern "C" {
int sceHttpsEnableOptionPrivate(int tmplId, int option);
int sceHttpsDisableOptionPrivate(int tmplId, int option);
}

using namespace paf;
using namespace paf::common;
using namespace paf::job;

#define SCE_HTTP_ERROR_NO_CONTENT_LEN	(-2143088527)
#define SCE_HTTP_ERROR_CHUNK_ENC	(-2143088526)

static int32_t s_httpTemplateId = -1;

static SceInt32 HttpAuthInfoCallback(SceInt32 request, SceHttpAuthType authType, const char *realm,
				     char *username, char *password, SceBool needEntity,
				     SceUChar8 **entityBody, SceSize *entitySize,
				     SceBool *save, void *userArg)
{
	(void)request;
	(void)authType;
	(void)realm;
	(void)needEntity;
	(void)entityBody;
	(void)entitySize;
	(void)save;
	(void)userArg;
	sce_paf_strncpy(username, "r246", 0x100);
	sce_paf_strncpy(password, "GPoEfbda", 0x100);
	return 0;
}

void SigninExtHttpInit(void)
{
	int32_t tmplId = sceHttpCreateTemplate("PSVHTTP/1.0 REG/1.0", 2, 0);
	if (tmplId < 0)
		return;

	if (sceHttpsEnableOptionPrivate(tmplId, 101) < 0 ||
	    sceHttpsDisableOptionPrivate(tmplId, 24) < 0 ||
	    sceHttpSetAuthInfoCallback(tmplId, HttpAuthInfoCallback, NULL) < 0) {
		if (tmplId >= 0)
			sceHttpDeleteTemplate(tmplId);
		return;
	}

	s_httpTemplateId = tmplId;
}

void SigninExtHttpTerm(void)
{
	if (s_httpTemplateId >= 0) {
		sceHttpDeleteTemplate(s_httpTemplateId);
		s_httpTemplateId = -1;
	}
}

/* ---- HttpConnection ---- */

HttpConnection::HttpConnection(int32_t templateId, const Url &url)
	: m_baseUrl(url.GetBaseUrlString()), m_connId(-1), m_isHttps(false)
{
	(void)templateId;
	string scheme = url.GetSchemeString();
	if (scheme == string("https"))
		m_isHttps = true;

	string host = url.GetHostString();
	string schemeStr = url.GetSchemeString();
	uint16_t port = url.GetPort();
	m_connId = sceHttpCreateConnection(s_httpTemplateId, host.c_str(), schemeStr.c_str(),
					   port, 1);
}

HttpConnection::~HttpConnection()
{
	if (m_connId >= 0)
		sceHttpDeleteConnection(m_connId);
}

/* ---- HttpRequestJob ---- */

HttpRequestJob::HttpRequestJob(HttpClient *client, const char *url, int32_t responseMode,
			       Callback2<HttpRequestJob *, int32_t> *callback)
	: JobItem("SceSigninExtHttp", NULL),
	  m_client(client),
	  m_url(string(url)),
	  m_responseMode(responseMode),
	  m_callback(callback),
	  m_reqId(-1),
	  m_buffer(),
	  m_statusCode(0)
{
}

HttpRequestJob::~HttpRequestJob()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
}

void HttpRequestJob::Cancel(void)
{
	if (m_reqId >= 0)
		sceHttpAbortRequest(m_reqId);
	JobItem::Cancel();
}

int32_t HttpRequestJob::ReadFixedLength(uint32_t contentLength)
{
	uint32_t allocSize = (m_responseMode == 0) ? (contentLength + 1) : contentLength;
	m_buffer = SigninExtAllocBuffer(allocSize);
	if (m_buffer.get() == NULL)
		return SIGNIN_EXT_ERROR_NOMEM;

	int32_t ret = sceHttpReadData(m_reqId, m_buffer->GetAddr(), contentLength);
	if (ret >= 0)
		return 0;
	return ret;
}

int32_t HttpRequestJob::ReadChunked(void)
{
	uint32_t totalLen = 0;
	vector<SharedPtr<Buffer> > chunks;

	for (;;) {
		SharedPtr<Buffer> chunk = SigninExtAllocBuffer(0x4000);
		if (chunk.get() == NULL)
			return SIGNIN_EXT_ERROR_NOMEM;

		int32_t n = sceHttpReadData(m_reqId, chunk->GetAddr(), 0x4000);
		if (n < 0)
			return n;
		if (n == 0)
			break;

		chunks.push_back(chunk);
		totalLen += (uint32_t)n;
	}

	uint32_t allocSize = (m_responseMode == 0) ? (totalLen + 1) : totalLen;
	m_buffer = SigninExtAllocBuffer(allocSize);
	if (m_buffer.get() == NULL)
		return SIGNIN_EXT_ERROR_NOMEM;

	char *dst = (char *)m_buffer->GetAddr();
	for (vector<SharedPtr<Buffer> >::iterator it = chunks.begin(); it != chunks.end(); ++it) {
		uint32_t copyLen = (totalLen > 0x4000) ? 0x4000 : totalLen;
		sce_paf_memcpy(dst, (*it)->GetAddr(), copyLen);
		dst += copyLen;
		totalLen -= copyLen;
	}
	return 0;
}

int32_t HttpRequestJob::ExecuteHttp(void)
{
	SharedPtr<HttpConnection> conn = m_client->AcquireConnection(m_url);
	int32_t ret = conn->m_connId;

	if (ret >= 0) {
		string urlStr = m_url.GetUrlString();
		ret = sceHttpCreateRequestWithURL(ret, 0, urlStr.c_str(), 0);
		if (ret >= 0) {
			m_reqId = ret;
			if (IsCanceled()) {
				ret = -1;
			} else {
				ret = sceHttpSendRequest(m_reqId, NULL, 0);
				if (ret >= 0) {
					ret = sceHttpGetStatusCode(m_reqId, &m_statusCode);
					if (ret >= 0) {
						if (m_statusCode == 200) {
							uint64_t contentLen = 0;
							int32_t lenRet = sceHttpGetResponseContentLength(
								m_reqId, &contentLen);
							ret = lenRet;
							if (lenRet >= 0 ||
							    lenRet == SCE_HTTP_ERROR_NO_CONTENT_LEN ||
							    lenRet == SCE_HTTP_ERROR_CHUNK_ENC) {
								if (contentLen < 0xFFFFFFFFULL) {
									if (lenRet == SCE_HTTP_ERROR_NO_CONTENT_LEN ||
									    lenRet == SCE_HTTP_ERROR_CHUNK_ENC)
										ret = ReadChunked();
									else
										ret = ReadFixedLength((uint32_t)contentLen);

									if (ret >= 0 && m_responseMode == 0) {
										char *addr = (char *)m_buffer->GetAddr();
										addr[m_buffer->GetSize() - 1] = '\0';
									}
								} else {
									ret = SIGNIN_EXT_ERROR_NOMEM;
								}
							}
						} else {
							ret = SIGNIN_EXT_ERROR_HTTP_STATUS;
						}
					}
				}
			}
		}
	}

	if (m_reqId >= 0) {
		sceHttpDeleteRequest(m_reqId);
		m_reqId = -1;
	}

	if (conn.get() != NULL) {
		m_client->ReleaseConnection(conn);
		conn.reset();
	}

	if (ret < 0)
		m_buffer.reset();

	return ret;
}

int32_t HttpRequestJob::Run(void)
{
	if (IsCanceled())
		return -1;
	return ExecuteHttp();
}

void HttpRequestJob::Finish(int32_t result)
{
	if (IsCanceled())
		result = -1;

	list<SharedPtr<JobItem> > &jobs = m_client->m_activeJobs;
	for (list<SharedPtr<JobItem> >::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if (it->get() == this) {
			jobs.erase(it);
			break;
		}
	}

	OnFinished(result);
}

void HttpRequestJob::OnFinished(int32_t result)
{
	if (m_callback != NULL) {
		Callback2<HttpRequestJob *, int32_t> *cb = m_callback;
		m_callback = NULL;
		cb->Invoke(this, result);
		delete cb;
	}
}

/* ---- HttpClient ---- */

HttpClient::HttpClient(JobQueue *queue)
	: m_jobQueue(queue),
	  m_activeJobs(),
	  m_maxHttpConns(0),
	  m_maxHttpsConns(0),
	  m_httpPool(),
	  m_httpsPool()
{
	if (m_jobQueue == NULL)
		m_jobQueue = SigninExtGetJobQueue();
}

HttpClient::~HttpClient()
{
	m_httpPool.clear();
	m_httpsPool.clear();
}

SharedPtr<HttpConnection> HttpClient::AcquireConnection(const Url &url)
{
	bool isHttps = (url.GetSchemeString() == string("https"));
	list<SharedPtr<HttpConnection> > &pool = isHttps ? m_httpsPool : m_httpPool;

	for (list<SharedPtr<HttpConnection> >::iterator it = pool.begin(); it != pool.end(); ++it) {
		HttpConnection *conn = it->get();
		if (conn->m_connId >= 0) {
			string baseUrl = url.GetBaseUrlString();
			if (sce_paf_strcmp(conn->m_baseUrl.c_str(), baseUrl.c_str()) == 0) {
				SharedPtr<HttpConnection> found = *it;
				pool.erase(it);
				return found;
			}
		}
	}

	return SharedPtr<HttpConnection>(new HttpConnection(s_httpTemplateId, url));
}

void HttpClient::ReleaseConnection(SharedPtr<HttpConnection> &conn)
{
	list<SharedPtr<HttpConnection> > &pool = conn->m_isHttps ? m_httpsPool : m_httpPool;
	int32_t limit = conn->m_isHttps ? m_maxHttpsConns : m_maxHttpConns;

	if (limit > 0) {
		pool.push_front(conn);
		while (pool.size() > (uint32_t)limit)
			pool.pop_back();
	}
}

void HttpClient::Enqueue(const char *url, int32_t responseMode,
			 Callback2<HttpRequestJob *, int32_t> *callback)
{
	SharedPtr<JobItem> item(new HttpRequestJob(this, url, responseMode, callback));
	m_activeJobs.push_back(item);
	m_jobQueue->Enqueue(item);
}

void HttpClient::CancelAll(void)
{
	for (list<SharedPtr<JobItem> >::iterator it = m_activeJobs.begin();
	     it != m_activeJobs.end(); ++it) {
		(*it)->Cancel();
	}
}

bool HttpClient::HasActiveJobs(void) const
{
	return m_activeJobs.size() != 0;
}
