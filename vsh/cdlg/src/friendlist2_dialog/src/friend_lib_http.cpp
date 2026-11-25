/*
 * SceFriendList2DialogPlugin: SceFriendLibHttp and SceFriendLibImage.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/vector>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <buffer/buffer.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <url/url.h>
#include "friend_lib_http.h"
#include "friend_lib.h"

extern "C" {
int sceHttpCreateTemplate(const char *userAgent, int httpVer, int autoProxyConf);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpsEnableOptionPrivate(int tmplId, int option);
int sceHttpsDisableOptionPrivate(int tmplId, int option);
int sceHttpCreateConnection(int tmplId, const char *serverName, const char *scheme,
			    uint16_t port, int enableKeepalive);
int sceHttpDeleteConnection(int connId);
int sceHttpCreateRequestWithURL(int connId, int method, const char *url, uint64_t contentLength);
int sceHttpDeleteRequest(int reqId);
int sceHttpAbortRequest(int reqId);
int sceHttpSendRequest(int reqId, const void *postData, SceSize size);
int sceHttpGetStatusCode(int reqId, int *statusCode);
int sceHttpGetResponseContentLength(int reqId, uint64_t *contentLength);
int sceHttpReadData(int reqId, void *data, SceSize size);
int SceHttp_F30073EC(int reqId, uint32_t flags);
int sceHttpUriEscape(char *out, SceSize *require, SceSize prepare, const char *in);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::image;
using namespace paf::job;
using namespace paf::memory;

#define FRIEND_HTTP_ERROR_NOMEM		-2136014814	/* 0x80AE0022 */
#define FRIEND_HTTP_ERROR_STATUS	-2135994284	/* 0x80AE5054 */
#define SCE_HTTP_ERROR_NO_CONTENT_LEN	-2143088527	/* 0x80431071 */
#define SCE_HTTP_ERROR_CHUNK_ENC	-2143088526	/* 0x80431072 */

static int s_httpTemplateId = -1;

int FriendLibHttpInit(void)
{
	if (s_httpTemplateId < 0) {
		s_httpTemplateId = sceHttpCreateTemplate("SceFriendLibHttp", 2, 1);
		if (s_httpTemplateId >= 0) {
			sceHttpsEnableOptionPrivate(s_httpTemplateId, 1);
			sceHttpsDisableOptionPrivate(s_httpTemplateId, 2);
		}
	}
	return s_httpTemplateId;
}

void FriendLibHttpTerm(void)
{
	if (s_httpTemplateId >= 0) {
		sceHttpDeleteTemplate(s_httpTemplateId);
		s_httpTemplateId = -1;
	}
}

/* Pooled HTTP/HTTPS connection handle */
class FriendLibHttpConnection {
public:
	FriendLibHttpConnection(int templateId, const Url &url)
		: m_baseUrl(url.GetBaseUrlString()), m_connId(-1), m_isHttps(false)
	{
		string scheme = url.GetSchemeString();

		if (sce_paf_strcmp(scheme.c_str(), "https") == 0)
			m_isHttps = true;
		string host = url.GetHostString();
		string schemeStr = url.GetSchemeString();
		uint16_t port = url.GetPort();

		m_connId = sceHttpCreateConnection(templateId, host.c_str(), schemeStr.c_str(),
						   port, 1);
	}

	virtual ~FriendLibHttpConnection()
	{
		if (m_connId >= 0) {
			sceHttpDeleteConnection(m_connId);
			m_connId = -1;
		}
	}

	string m_baseUrl;
	int m_connId;
	bool m_isHttps;
};

class FriendLibHttpConnectionPool {
public:
	FriendLibHttpConnectionPool()
		: m_maxHttp(1), m_maxHttps(1)
	{
	}

	virtual ~FriendLibHttpConnectionPool()
	{
		m_httpConns.clear();
		m_httpsConns.clear();
	}

	SharedPtr<FriendLibHttpConnection> Acquire(const Url &url)
	{
		string scheme = url.GetSchemeString();
		bool isHttps = (sce_paf_strcmp(scheme.c_str(), "https") == 0);
		list<SharedPtr<FriendLibHttpConnection> > &bucket =
			isHttps ? m_httpsConns : m_httpConns;
		string baseUrl = url.GetBaseUrlString();

		for (list<SharedPtr<FriendLibHttpConnection> >::iterator it = bucket.begin();
		     it != bucket.end(); ++it) {
			SharedPtr<FriendLibHttpConnection> conn = *it;

			if (conn.get() != NULL && conn->m_connId >= 0 &&
			    sce_paf_strcmp(conn->m_baseUrl.c_str(), baseUrl.c_str()) == 0) {
				bucket.erase(it);
				return conn;
			}
		}

		FriendLibHttpConnection *fresh =
			new FriendLibHttpConnection(s_httpTemplateId, url);
		return SharedPtr<FriendLibHttpConnection>(fresh);
	}

	void Release(SharedPtr<FriendLibHttpConnection> conn)
	{
		if (conn.get() == NULL || conn->m_connId < 0)
			return;
		list<SharedPtr<FriendLibHttpConnection> > &bucket =
			conn->m_isHttps ? m_httpsConns : m_httpConns;
		uint32_t limit = conn->m_isHttps ? m_maxHttps : m_maxHttp;

		if (bucket.size() < limit)
			bucket.push_back(conn);
	}

	uint32_t m_maxHttp;
	uint32_t m_maxHttps;
	list<SharedPtr<FriendLibHttpConnection> > m_httpConns;
	list<SharedPtr<FriendLibHttpConnection> > m_httpsConns;
};

/* FriendLibHttpJob */
FriendLibHttpJob::FriendLibHttpJob(FriendLibHttpConnectionPool *pool, const string &urlStr,
				   int responseMode, uint32_t optionFlags,
				   FriendLibHttpCallback *callback)
	: JobItem("SceFriendLibHttp", NULL), m_pool(pool), m_url(urlStr),
	  m_responseMode(responseMode), m_optionFlags(optionFlags), m_callback(callback),
	  m_requestId(-1), m_responseBody(), m_httpStatus(0)
{
}

FriendLibHttpJob::~FriendLibHttpJob()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
}

void FriendLibHttpJob::Cancel()
{
	if (m_requestId >= 0)
		sceHttpAbortRequest(m_requestId);
	JobItem::Cancel();
}

int FriendLibHttpJob::ReadFixedLength(uint32_t contentLength)
{
	size_t allocSize = (m_responseMode == 0) ? (contentLength + 1U) : contentLength;

	m_responseBody = FriendLibAllocBuffer(allocSize);
	if (m_responseBody.get() == NULL)
		return FRIEND_HTTP_ERROR_NOMEM;

	int ret = sceHttpReadData(m_requestId, m_responseBody->GetAddr(), contentLength);

	return (ret >= 0) ? 0 : ret;
}

int FriendLibHttpJob::ReadChunked()
{
	vector<SharedPtr<Buffer> > chunks;
	uint32_t totalSize = 0;

	for (;;) {
		SharedPtr<Buffer> chunk = FriendLibAllocBuffer(0x4000U);

		if (chunk.get() == NULL)
			return FRIEND_HTTP_ERROR_NOMEM;
		int readBytes = sceHttpReadData(m_requestId, chunk->GetAddr(), 0x4000U);

		if (readBytes < 0)
			return readBytes;
		if (readBytes == 0)
			break;
		chunks.push_back(chunk);
		totalSize += (uint32_t)readBytes;
	}

	size_t allocSize = (m_responseMode == 0) ? (totalSize + 1U) : totalSize;

	m_responseBody = FriendLibAllocBuffer(allocSize);
	if (m_responseBody.get() == NULL)
		return FRIEND_HTTP_ERROR_NOMEM;

	uint8_t *dst = (uint8_t *)m_responseBody->GetAddr();
	uint32_t remaining = totalSize;

	for (size_t i = 0; i < chunks.size(); ++i) {
		uint32_t copyLen = (remaining > 0x4000U) ? 0x4000U : remaining;

		sce_paf_memcpy(dst, chunks[i]->GetAddr(), copyLen);
		dst += copyLen;
		remaining -= copyLen;
	}
	return 0;
}

int FriendLibHttpJob::Run()
{
	SharedPtr<FriendLibHttpConnection> conn = m_pool->Acquire(m_url);
	int ret = (conn.get() != NULL) ? conn->m_connId : -1;

	if (ret >= 0) {
		string fullUrl = m_url.GetUrlString();

		ret = sceHttpCreateRequestWithURL(conn->m_connId, 0, fullUrl.c_str(), 0);
		if (ret >= 0) {
			m_requestId = ret;
			if (IsCanceled()) {
				ret = -1;
			} else {
				uint32_t flags = m_optionFlags & 7U;

				ret = SceHttp_F30073EC(m_requestId, flags);
				if (ret >= 0) {
					ret = sceHttpSendRequest(m_requestId, NULL, 0);
					if (ret >= 0) {
						ret = sceHttpGetStatusCode(m_requestId,
									   &m_httpStatus);
						if (ret >= 0) {
							if (m_httpStatus == 200) {
								uint64_t contentLen = 0;

								ret = sceHttpGetResponseContentLength(
									m_requestId, &contentLen);
								if (ret >= 0 ||
								    ret == SCE_HTTP_ERROR_NO_CONTENT_LEN ||
								    ret == SCE_HTTP_ERROR_CHUNK_ENC) {
									if (contentLen < 0xFFFFFFFFULL) {
										if (ret == SCE_HTTP_ERROR_NO_CONTENT_LEN ||
										    ret == SCE_HTTP_ERROR_CHUNK_ENC)
											ret = ReadChunked();
										else
											ret = ReadFixedLength(
												(uint32_t)contentLen);
										if (ret >= 0 &&
										    m_responseMode == 0 &&
										    m_responseBody.get() !=
											    NULL) {
											uint8_t *buf =
												(uint8_t *)
													m_responseBody
														->GetAddr();
											size_t sz =
												m_responseBody
													->GetSize();
											if (sz > 0)
												buf[sz - 1] =
													'\0';
										}
									} else {
										ret = FRIEND_HTTP_ERROR_NOMEM;
									}
								}
							} else {
								ret = FRIEND_HTTP_ERROR_STATUS;
							}
						}
					}
				}
			}
		}
	}

	if (m_requestId >= 0) {
		sceHttpDeleteRequest(m_requestId);
		m_requestId = -1;
	}
	if (conn.get() != NULL)
		m_pool->Release(conn);
	return ret;
}

void FriendLibHttpJob::Finish(int result)
{
	if (IsCanceled())
		result = -1;
	if (m_callback != NULL) {
		FriendLibHttpCallback *cb = m_callback;

		m_callback = NULL;
		cb->OnComplete(result, m_httpStatus, m_responseBody);
		delete cb;
	}
}

/* Image decode job ("SceFriendLibImage") */
class FriendLibImageDecodeJob : public JobItem {
public:
	FriendLibImageDecodeJob(const char *pathOrUrl, FriendLibImageCallback *callback,
				const FriendLibImageOption &option)
		: JobItem("SceFriendLibImage", NULL), m_url(string(pathOrUrl ? pathOrUrl : "")),
		  m_callback(callback), m_option(option), m_surface()
	{
	}

	virtual ~FriendLibImageDecodeJob()
	{
		if (m_callback != NULL) {
			delete m_callback;
			m_callback = NULL;
		}
	}

	virtual int Run()
	{
		if (IsCanceled())
			return -1;

		Surface::LoadOption loadOpt;

		if (m_option.sourceBuffer.get() != NULL) {
			m_surface = Surface::Load(m_option.surfacePool,
						  m_option.sourceBuffer->GetAddr(),
						  (int64_t)m_option.sourceBuffer->GetSize(),
						  &loadOpt);
			return (m_surface.get() != NULL) ? 0 : -1;
		}

		string urlStr = m_url.GetUrlString();
		int openErr = 0;
		SharedPtr<File> file =
			LocalFile::Open(urlStr.c_str(), 1, 0, &openErr);

		if (file.get() == NULL || openErr < 0)
			return (openErr < 0) ? openErr : -1;

		int64_t fileSize = file->GetFileSize();

		if (fileSize <= 0)
			return -1;

		SharedPtr<Buffer> buf = FriendLibAllocBuffer((size_t)fileSize);

		if (buf.get() == NULL)
			return FRIEND_HTTP_ERROR_NOMEM;
		if (file->Read(buf->GetAddr(), (size_t)fileSize) < 0)
			return -1;
		m_surface = Surface::Load(m_option.surfacePool, buf->GetAddr(), fileSize, &loadOpt);
		return (m_surface.get() != NULL) ? 0 : -1;
	}

	virtual void Finish(int result)
	{
		if (IsCanceled())
			result = -1;
		if (m_callback != NULL) {
			FriendLibImageCallback *cb = m_callback;

			m_callback = NULL;
			cb->OnComplete(result, m_surface);
			delete cb;
		}
	}

	Url m_url;
	FriendLibImageCallback *m_callback;
	FriendLibImageOption m_option;
	intrusive_ptr<Surface> m_surface;
};

/* HTTP->Image bridge callback */
class FriendLibImageHttpBridge : public FriendLibHttpCallback {
public:
	FriendLibImageHttpBridge(FriendLibImageLoader *loader, const string &urlStr,
				 FriendLibImageCallback *callback,
				 const FriendLibImageOption &option)
		: m_loader(loader), m_urlStr(urlStr), m_callback(callback), m_option(option)
	{
	}

	virtual ~FriendLibImageHttpBridge()
	{
		if (m_callback != NULL) {
			delete m_callback;
			m_callback = NULL;
		}
	}

	virtual void OnComplete(int result, int httpStatus, SharedPtr<Buffer> responseBody)
	{
		(void)httpStatus;
		if (result >= 0 && responseBody.get() != NULL) {
			m_option.sourceBuffer = responseBody;
			FriendLibImageCallback *cb = m_callback;

			m_callback = NULL;
			SharedPtr<JobItem> decodeJob(
				new FriendLibImageDecodeJob(m_urlStr.c_str(), cb, m_option));
			m_loader->m_jobQueue->Enqueue(decodeJob);
		} else if (m_callback != NULL) {
			FriendLibImageCallback *cb = m_callback;

			m_callback = NULL;
			cb->OnComplete(result, intrusive_ptr<Surface>());
			delete cb;
		}
	}

	FriendLibImageLoader *m_loader;
	string m_urlStr;
	FriendLibImageCallback *m_callback;
	FriendLibImageOption m_option;
};

/* FriendLibImageLoader */
FriendLibImageLoader::FriendLibImageLoader(JobQueue *jobQueue)
	: m_jobQueue(jobQueue), m_httpPool(new FriendLibHttpConnectionPool()),
	  m_maxHttpConnections(1), m_maxHttpsConnections(1)
{
}

FriendLibImageLoader::~FriendLibImageLoader()
{
	delete m_httpPool;
	m_httpPool = NULL;
}

int FriendLibImageLoader::LoadAsync(const char *urlOrPath, FriendLibImageCallback *callback,
				    const FriendLibImageOption &option)
{
	if (urlOrPath == NULL || m_jobQueue == NULL)
		return -1;
	m_httpPool->m_maxHttp = m_maxHttpConnections;
	m_httpPool->m_maxHttps = m_maxHttpsConnections;

	string urlStr(urlOrPath);

	if (sce_paf_strncmp(urlOrPath, "http", 4) == 0) {
		FriendLibImageHttpBridge *bridge =
			new FriendLibImageHttpBridge(this, urlStr, callback, option);
		SharedPtr<JobItem> httpJob(
			new FriendLibHttpJob(m_httpPool, urlStr, 1, 7U, bridge));
		return m_jobQueue->Enqueue(httpJob);
	}
	SharedPtr<JobItem> decodeJob(new FriendLibImageDecodeJob(urlOrPath, callback, option));
	return m_jobQueue->Enqueue(decodeJob);
}

void FriendLibImageLoader::CancelAll()
{
	if (m_jobQueue != NULL) {
		m_jobQueue->CancelAllItems();
		m_jobQueue->WaitEmpty();
	}
}
