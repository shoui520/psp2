/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * SceFriendLibHttp and SceFriendLibImage — pooled HTTP/HTTPS GET jobs,
 * local-file fallback, and Surface::Load decoding.
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
#include <file/file.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <url/url.h>
#include "friend_select_internal.h"

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
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::image;
using namespace paf::job;

#define SCE_HTTP_ERROR_NO_CONTENT_LEN	(-2143088527)
#define SCE_HTTP_ERROR_CHUNK_ENC	(-2143088526)

static int32_t s_httpTemplateId = -1;

/* ========================================================================= */
/* Global HTTP template lifecycle                                            */
/* ========================================================================= */

void FriendLibHttpInit(void)
{
	int32_t tmplId = sceHttpCreateTemplate("", 2, 0);

	if (tmplId >= 0) {
		if (sceHttpsEnableOptionPrivate(tmplId, 101) < 0 ||
		    sceHttpsDisableOptionPrivate(tmplId, 24) < 0) {
			if (tmplId >= 0)
				sceHttpDeleteTemplate(tmplId);
		} else {
			s_httpTemplateId = tmplId;
		}
	}
}

void FriendLibHttpTerm(void)
{
	if (s_httpTemplateId >= 0) {
		sceHttpDeleteTemplate(s_httpTemplateId);
		s_httpTemplateId = -1;
	}
}

/* ========================================================================= */
/* FriendLibHttpConnection                                                   */
/* ========================================================================= */

FriendLibHttpConnection::FriendLibHttpConnection(int32_t templateId, const Url &url)
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

FriendLibHttpConnection::~FriendLibHttpConnection()
{
	if (m_connId >= 0)
		sceHttpDeleteConnection(m_connId);
}

/* ========================================================================= */
/* FriendLibHttpJob                                                          */
/* ========================================================================= */

FriendLibHttpJob::FriendLibHttpJob(FriendLibHttpManager *mgr, const char *urlStr,
				   int32_t responseMode, uint32_t optionFlags,
				   Callback2<FriendLibHttpJob *, int32_t> *callback,
				   const char *jobName)
	: JobItem((jobName != NULL) ? jobName : "SceFriendLibHttp", NULL), m_mgr(mgr),
	  m_url(string(urlStr)), m_responseMode(responseMode), m_optionFlags(optionFlags),
	  m_callback(callback), m_requestId(-1), m_responseBody(), m_httpStatus(0)
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

int32_t FriendLibHttpJob::ReadFixedLength(uint32_t contentLength)
{
	uint32_t allocSize = (m_responseMode == 0) ? (contentLength + 1U) : contentLength;

	m_responseBody = FriendLibAllocBuffer(allocSize);
	if (m_responseBody.get() == NULL)
		return FRIEND_SELECT_ERROR_NOMEM;

	int32_t ret = sceHttpReadData(m_requestId, m_responseBody->GetAddr(), contentLength);

	return (ret >= 0) ? 0 : ret;
}

int32_t FriendLibHttpJob::ReadChunked()
{
	vector<SharedPtr<Buffer> > chunks;
	uint32_t totalSize = 0;

	for (;;) {
		SharedPtr<Buffer> chunk = FriendLibAllocBuffer(0x4000U);

		if (chunk.get() == NULL)
			return FRIEND_SELECT_ERROR_NOMEM;

		int32_t readBytes = sceHttpReadData(m_requestId, chunk->GetAddr(), 0x4000U);

		if (readBytes < 0)
			return readBytes;
		if (readBytes == 0)
			break;
		chunks.push_back(chunk);
		totalSize += (uint32_t)readBytes;
	}

	uint32_t allocSize = (m_responseMode == 0) ? (totalSize + 1U) : totalSize;

	m_responseBody = FriendLibAllocBuffer(allocSize);
	if (m_responseBody.get() == NULL)
		return FRIEND_SELECT_ERROR_NOMEM;

	uint8_t *dst = (uint8_t *)m_responseBody->GetAddr();
	uint32_t remaining = totalSize;

	for (vector<SharedPtr<Buffer> >::iterator it = chunks.begin(); it != chunks.end(); ++it) {
		uint32_t copyLen = (remaining > 0x4000U) ? 0x4000U : remaining;

		sce_paf_memcpy(dst, (*it)->GetAddr(), copyLen);
		dst += copyLen;
		remaining -= copyLen;
	}
	return 0;
}

int32_t FriendLibHttpJob::ExecuteHttpRequest()
{
	SharedPtr<FriendLibHttpConnection> conn = m_mgr->AcquireConnection(m_url);
	int32_t ret = conn->m_connId;

	if (ret >= 0) {
		string fullUrl = m_url.GetUrlString();

		ret = sceHttpCreateRequestWithURL(ret, 0, fullUrl.c_str(), 0);
		if (ret >= 0) {
			m_requestId = ret;
			if (IsCanceled()) {
				ret = -1;
			} else {
				uint32_t flags = (m_optionFlags & 1U) ? 1U : 0U;

				if ((m_optionFlags & 2U) != 0)
					flags |= 2U;
				if ((m_optionFlags & 4U) != 0)
					flags |= 4U;

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
										    m_responseMode == 0) {
											uint8_t *buf =
												(uint8_t *)
													m_responseBody
														->GetAddr();
											buf[m_responseBody->GetSize() -
											    1] = '\0';
										}
									} else {
										ret = FRIEND_SELECT_ERROR_NOMEM;
									}
								}
							} else {
								ret = FRIEND_SELECT_ERROR_HTTP_STATUS;
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
	if (conn.get() != NULL) {
		list<SharedPtr<FriendLibHttpConnection> > &bucket =
			conn->m_isHttps ? m_mgr->m_httpsConns : m_mgr->m_httpConns;
		int32_t limit = conn->m_isHttps ? m_mgr->m_maxHttpsConns : m_mgr->m_maxHttpConns;

		if (limit > 0) {
			bucket.push_back(conn);
			while ((int32_t)bucket.size() > limit)
				bucket.pop_front();
		}
		conn.reset();
	}
	if (ret < 0)
		m_responseBody.reset();
	return ret;
}

int32_t FriendLibHttpJob::Run()
{
	if (IsCanceled())
		return -1;
	return ExecuteHttpRequest();
}

void FriendLibHttpJob::Finish(int32_t result)
{
	if (IsCanceled())
		result = -1;
	m_mgr->RemoveJob(this);
	NotifyCallback(result);
}

void FriendLibHttpJob::NotifyCallback(int32_t result)
{
	Callback2<FriendLibHttpJob *, int32_t> *cb = m_callback;

	if (cb != NULL) {
		m_callback = NULL;
		cb->Invoke(this, result);
		delete cb;
	}
}

/* ========================================================================= */
/* FriendLibHttpManager                                                      */
/* ========================================================================= */

FriendLibHttpManager::FriendLibHttpManager(JobQueue *jobQueue)
	: m_jobQueue(jobQueue), m_activeJobs(), m_maxHttpConns(0), m_maxHttpsConns(0),
	  m_httpConns(), m_httpsConns(), m_defaultOptionFlags(0)
{
	if (m_jobQueue == NULL)
		m_jobQueue = FriendLibGetJobQueue();
}

FriendLibHttpManager::~FriendLibHttpManager()
{
	m_httpConns.clear();
	m_httpsConns.clear();
}

SharedPtr<FriendLibHttpConnection> FriendLibHttpManager::AcquireConnection(const Url &url)
{
	string scheme = url.GetSchemeString();
	bool isHttps = (scheme == string("https"));
	list<SharedPtr<FriendLibHttpConnection> > &bucket =
		isHttps ? m_httpsConns : m_httpConns;

	for (list<SharedPtr<FriendLibHttpConnection> >::iterator it = bucket.begin();
	     it != bucket.end(); ++it) {
		FriendLibHttpConnection *conn = it->get();
		bool matches = false;

		if (conn->m_connId >= 0) {
			string baseUrl = url.GetBaseUrlString();

			matches = (sce_paf_strcmp(conn->m_baseUrl.c_str(), baseUrl.c_str()) == 0);
		}
		if (matches) {
			SharedPtr<FriendLibHttpConnection> found = *it;

			bucket.erase(it);
			return found;
		}
	}

	return SharedPtr<FriendLibHttpConnection>(
		new FriendLibHttpConnection(s_httpTemplateId, url));
}

void FriendLibHttpManager::CancelAllJobs()
{
	for (list<SharedPtr<JobItem> >::iterator it = m_activeJobs.begin();
	     it != m_activeJobs.end(); ++it)
		(*it)->Cancel();
}

list<SharedPtr<JobItem> >::iterator FriendLibHttpManager::FindJob(const char *url)
{
	for (list<SharedPtr<JobItem> >::iterator it = m_activeJobs.begin();
	     it != m_activeJobs.end(); ++it) {
		FriendLibHttpJob *job = (FriendLibHttpJob *)it->get();
		string jobUrl = job->m_url.GetUrlString();

		if (jobUrl == string(url))
			return it;
	}
	return m_activeJobs.end();
}

void FriendLibHttpManager::CancelJobByUrl(const char *url)
{
	list<SharedPtr<JobItem> >::iterator it = FindJob(url);

	if (it != m_activeJobs.end())
		(*it)->Cancel();
}

void FriendLibHttpManager::RemoveJob(FriendLibHttpJob *job)
{
	for (list<SharedPtr<JobItem> >::iterator it = m_activeJobs.begin();
	     it != m_activeJobs.end(); ++it) {
		if (it->get() == job) {
			m_activeJobs.erase(it);
			break;
		}
	}
}

/* ========================================================================= */
/* FriendLibImageJob & FriendLibImageLoader                                  */
/* ========================================================================= */

static bool ImageDecodeIsCanceled(void *userData)
{
	return ((JobItem *)userData)->IsCanceled();
}

static intrusive_ptr<Surface> ImageDecodeAllocSurface(SurfacePool *pool,
						      const SharedPtr<Image> &image,
						      int32_t numPage, int32_t numLevel,
						      void *userData)
{
	(void)numPage;
	FriendLibImageJob *job = (FriendLibImageJob *)userData;

	if (job->m_allocCb != NULL)
		return job->m_allocCb->Invoke(pool, image.get(), numLevel);
	return intrusive_ptr<Surface>();
}

FriendLibImageJob::FriendLibImageJob(FriendLibHttpManager *mgr, const char *urlStr,
				     uint32_t optionFlags,
				     Callback2<FriendLibImageJob *, int32_t> *callback,
				     const FriendLibImageLoadOption &option)
	: FriendLibHttpJob(mgr, urlStr, 1, optionFlags, NULL, "SceFriendLibImage"),
	  m_imageCallback(callback), m_maxWidth(option.maxWidth), m_maxHeight(option.maxHeight),
	  m_surfacePool(option.surfacePool), m_allocCb(option.allocCb), m_surface(), m_localFile()
{
	m_responseBody = option.sourceBuffer;
	if (m_surfacePool == NULL)
		m_surfacePool = FriendLibGetSurfacePool();
}

FriendLibImageJob::~FriendLibImageJob()
{
	if (m_imageCallback != NULL) {
		delete m_imageCallback;
		m_imageCallback = NULL;
	}
	if (m_allocCb != NULL) {
		delete m_allocCb;
		m_allocCb = NULL;
	}
}

void FriendLibImageJob::Cancel()
{
	if (m_localFile.get() != NULL)
		m_localFile->Abort();
	FriendLibHttpJob::Cancel();
}

int32_t FriendLibImageJob::DecodeSurface()
{
	Surface::LoadOption loadOpt;

	loadOpt.scale.w = m_maxWidth;
	loadOpt.scale.h = m_maxHeight;
	loadOpt.cancel = ImageDecodeIsCanceled;
	loadOpt.cancel_userdata = this;
	if (m_allocCb != NULL) {
		loadOpt.alloc_surface = ImageDecodeAllocSurface;
		loadOpt.alloc_surface_userdata = this;
	}

	m_surface = Surface::Load(m_surfacePool, m_responseBody->GetAddr(),
				  m_responseBody->GetSize(), &loadOpt);
	if (m_surface.get() != NULL)
		return 0;
	return loadOpt.error;
}

int32_t FriendLibImageJob::ReadLocalFile()
{
	int32_t err = 0;
	string path = m_url.GetUrlString();

	m_localFile = LocalFile::Open(path.c_str(), 1, 0, &err);
	if (m_localFile.get() != NULL) {
		int64_t fileSize = m_localFile->GetFileSize();

		if ((uint64_t)fileSize < 0xFFFFFFFFULL) {
			m_responseBody = FriendLibAllocBuffer((size_t)fileSize);
			if (m_responseBody.get() != NULL) {
				err = m_localFile->Read(m_responseBody->GetAddr(),
							(size_t)fileSize);
			} else {
				err = FRIEND_SELECT_ERROR_NOMEM;
			}
		} else {
			err = FRIEND_SELECT_ERROR_NOMEM;
		}
	} else {
		err = FRIEND_SELECT_ERROR_LOCAL_FILE;
	}

	if (m_localFile.get() != NULL) {
		m_localFile->Close();
		m_localFile.reset();
	}
	if (err < 0)
		m_responseBody.reset();
	return err;
}

int32_t FriendLibImageJob::Run()
{
	if (IsCanceled())
		return -1;
	if (m_responseBody.get() == NULL) {
		string urlStr = m_url.GetUrlString();
		int32_t ret;

		if (sce_paf_strncmp(urlStr.c_str(), "http", 4) == 0)
			ret = ExecuteHttpRequest();
		else
			ret = ReadLocalFile();
		if (ret < 0)
			return ret;
	}
	return DecodeSurface();
}

void FriendLibImageJob::NotifyCallback(int32_t result)
{
	Callback2<FriendLibImageJob *, int32_t> *cb = m_imageCallback;

	if (cb != NULL) {
		m_imageCallback = NULL;
		cb->Invoke(this, result);
		delete cb;
	}
}

FriendLibImageLoader::FriendLibImageLoader(JobQueue *jobQueue)
	: FriendLibHttpManager(jobQueue)
{
}

FriendLibImageLoader::~FriendLibImageLoader()
{
}

void FriendLibImageLoader::EnqueueImage(const char *url,
					Callback2<FriendLibImageJob *, int32_t> *callback,
					const FriendLibImageLoadOption *option)
{
	FriendLibImageLoadOption defaultOpt;
	const FriendLibImageLoadOption *opt = (option != NULL) ? option : &defaultOpt;
	FriendLibImageJob *job =
		new FriendLibImageJob(this, url, m_defaultOptionFlags, callback, *opt);
	SharedPtr<JobItem> item(job);

	m_activeJobs.push_back(item);
	m_jobQueue->Enqueue(item);
}
