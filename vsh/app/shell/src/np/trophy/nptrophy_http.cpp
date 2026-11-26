/*
 * nptrophy_http.cpp: HTTP template, connection pool, cancellable request/stream
 * wrappers, PSN trophy CDN URL signing, and AES-128-CBC decrypt stream
 * (0x812E2360..0x812E328E, 0x812E6BC4..0x812E7867).
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libaes.h>
#include <libhttp.h>
#include "nptrophy_net.h"

extern "C" {
extern int SceNetCtl_E8E8DF18(int *outState);
}

namespace {

static const char *const kDefaultKeyVersion = "01";
static const char *const kEncryptedExtPrefix = "E";
static const char *const kPlainExtPrefix = "";

struct TrophyAesKeyEntry {
	const char *version;
	uint8_t key[16];
};

struct TrophyUrlHmacKeyEntry {
	const char *version;
	uint8_t key[64];
};

/* AES-128 CDN asset decryption key table (0x81546024) */
static const TrophyAesKeyEntry kTrophyAssetAesKeys[1] = {
	{
		"01",
		{
			0x59, 0xED, 0x05, 0xE1, 0x2B, 0x97, 0x3E, 0xE0,
			0x62, 0xB8, 0x07, 0xA4, 0xEB, 0x48, 0x10, 0x10
		}
	}
};

/* HMAC-SHA1 CDN URL path signing key table (0x81546038) */
static const TrophyUrlHmacKeyEntry kTrophyUrlHmacKeys[1] = {
	{
		"01",
		{
			0x65, 0x9A, 0x82, 0x19, 0x27, 0xCF, 0xD6, 0x2F,
			0x0C, 0x1C, 0xC2, 0x5F, 0xAF, 0x67, 0x96, 0x5B,
			0x93, 0xD0, 0x63, 0xF8, 0x6B, 0x94, 0x86, 0xAF,
			0xEF, 0x90, 0x35, 0xDF, 0xC7, 0x1E, 0xD6, 0x9D,
			0x1E, 0xA8, 0x35, 0xB1, 0x4A, 0x2A, 0x90, 0x02,
			0x95, 0x06, 0x46, 0x39, 0xCC, 0x2D, 0x9C, 0xFC,
			0x08, 0x4C, 0x31, 0x62, 0x85, 0x1B, 0xD1, 0xD1,
			0xA5, 0xF2, 0x68, 0xD4, 0x8D, 0x00, 0x79, 0xA8
		}
	}
};

class TrophyHttpCancelToken : public TrophyScopedCancelToken {
public:
	explicit TrophyHttpCancelToken(int reqId) : m_reqId(reqId) {}
	virtual ~TrophyHttpCancelToken() {}

protected:
	virtual void OnCancel(int errorCode)
	{
		(void)errorCode;
		if (m_reqId != 0)
			sceHttpAbortRequest(m_reqId);
	}

private:
	int m_reqId;
};

static const uint8_t *findAesKey(const char *keyVersion)
{
	const uint8_t *found = NULL;

	for (SceSize i = 0; i < sizeof(kTrophyAssetAesKeys) / sizeof(kTrophyAssetAesKeys[0]); ++i) {
		const char *entryVer = kTrophyAssetAesKeys[i].version;
		if (keyVersion == NULL && entryVer == NULL) {
			found = kTrophyAssetAesKeys[i].key;
		} else if (entryVer != NULL && keyVersion != NULL) {
			SceSize len = sceClibStrnlen(entryVer, 0x7FFFFFFFu);
			if (sceClibStrncmp(entryVer, keyVersion, len + 1) == 0)
				found = kTrophyAssetAesKeys[i].key;
		}
	}
	return found;
}

static const uint8_t *findUrlHmacKey(const char *keyVersion)
{
	const uint8_t *found = NULL;

	for (SceSize i = 0; i < sizeof(kTrophyUrlHmacKeys) / sizeof(kTrophyUrlHmacKeys[0]); ++i) {
		const char *entryVer = kTrophyUrlHmacKeys[i].version;
		if (keyVersion == NULL && entryVer == NULL) {
			found = kTrophyUrlHmacKeys[i].key;
		} else if (entryVer != NULL && keyVersion != NULL) {
			SceSize len = sceClibStrnlen(entryVer, 0x7FFFFFFFu);
			if (sceClibStrncmp(entryVer, keyVersion, len + 1) == 0)
				found = kTrophyUrlHmacKeys[i].key;
		}
	}
	return found;
}

} /* namespace */

/* =========================================================================
 * TrophyHttpTemplate (0x812E6BC4..0x812E6C5A)
 * ========================================================================= */

/* 0x812E6BC4 */
TrophyHttpTemplate::TrophyHttpTemplate() : m_tmplId(0) {}

/* 0x812E6BD8 */
TrophyHttpTemplate::~TrophyHttpTemplate()
{
	if (m_tmplId != 0)
		sceHttpDeleteTemplate(m_tmplId);
}

/* 0x812E6C2C */
int TrophyHttpTemplate::Init(const char *userAgent, int httpVer, int autoProxyConf)
{
	int tmplId = sceHttpCreateTemplate(userAgent, httpVer, autoProxyConf);
	if (tmplId < 0)
		return tmplId;
	m_tmplId = tmplId;
	return 0;
}

/* 0x812E6C48 */
int TrophyHttpTemplate::Term()
{
	if (m_tmplId == 0)
		return 0;
	sceHttpDeleteTemplate(m_tmplId);
	m_tmplId = 0;
	return 0;
}

/* 0x812E6C5A */
int TrophyHttpTemplate::SetAuthCallback(SceHttpAuthInfoCallback callback, void *userArg)
{
	int ret = sceHttpSetAuthInfoCallback(m_tmplId, callback, userArg);
	return (ret >= 0) ? 0 : ret;
}

/* =========================================================================
 * TrophyHttpConnPoolEntry & TrophyHttpConnPool (0x812E74AA..0x812E7867)
 * ========================================================================= */

TrophyHttpConnPoolEntry::TrophyHttpConnPoolEntry()
	: m_connId(0), m_tmplId(0), m_scheme(NULL), m_serverName(NULL),
	  m_port(0), m_inUse(0), m_evictOnRelease(0), m_lastError(0),
	  m_next(NULL), m_prev(NULL)
{
}

/* 0x812E77F2 */
TrophyHttpConnPoolEntry::~TrophyHttpConnPoolEntry()
{
	if (m_scheme != NULL)
		TrophyHeapObject::operator delete(m_scheme);
	if (m_serverName != NULL)
		TrophyHeapObject::operator delete(m_serverName);
	if (m_connId != 0)
		sceHttpDeleteConnection(m_connId);
}

/* 0x812E74AA */
int TrophyHttpConnPoolEntry::Init(int tmplId, const char *scheme, const char *serverName,
				  uint16_t port)
{
	int connId = sceHttpCreateConnection(tmplId, serverName, scheme, port, 1);
	if (connId < 0)
		return connId;

	char *schemeCopy = NULL;
	char *hostCopy = NULL;
	SceSize schemeLen = sceClibStrnlen(scheme, 0x7FFFFFFFu);
	int ret = npTrophyStrndup(&schemeCopy, scheme, schemeLen);
	if (ret >= 0) {
		SceSize hostLen = sceClibStrnlen(serverName, 0x7FFFFFFFu);
		ret = npTrophyStrndup(&hostCopy, serverName, hostLen);
	}

	if (ret < 0) {
		if (schemeCopy != NULL)
			TrophyHeapObject::operator delete(schemeCopy);
		if (hostCopy != NULL)
			TrophyHeapObject::operator delete(hostCopy);
		sceHttpDeleteConnection(connId);
		return ret;
	}

	m_connId = connId;
	m_tmplId = tmplId;
	m_scheme = schemeCopy;
	m_serverName = hostCopy;
	m_port = port;
	m_inUse = 0;
	m_evictOnRelease = 0;
	m_lastError = 0;
	m_next = NULL;
	m_prev = NULL;
	return 0;
}

TrophyHttpConnPool::TrophyHttpConnPool()
	: m_maxConnections(0), m_activeCount(0), m_head(NULL), m_tail(NULL)
{
}

TrophyHttpConnPool::~TrophyHttpConnPool()
{
	Term();
}

int TrophyHttpConnPool::Init(uint32_t maxConnections)
{
	int ret = m_mutex.Init("SceNpTrophyHttpConnPool", false);
	if (ret < 0)
		return ret;
	m_maxConnections = maxConnections;
	return 0;
}

void TrophyHttpConnPool::Term()
{
	m_mutex.Lock();
	while (m_head != NULL) {
		TrophyHttpConnPoolEntry *entry = m_head;
		m_head = entry->m_next;
		delete entry;
	}
	m_tail = NULL;
	m_activeCount = 0;
	m_mutex.Unlock();
	m_mutex.Term();
}

/* 0x812E7586 */
int TrophyHttpConnPool::Acquire(TrophyHttpTemplate *httpTemplate, const char *scheme,
				const char *serverName, uint16_t port, int *outConnId)
{
	m_mutex.Lock();

	int tmplId = httpTemplate->GetTemplateId();
	SceSize schemeLen = sceClibStrnlen(scheme, 0x7FFFFFFFu);
	SceSize hostLen = sceClibStrnlen(serverName, 0x7FFFFFFFu);

	for (TrophyHttpConnPoolEntry *cur = m_head; cur != NULL; cur = cur->m_next) {
		if (cur->m_inUse == 0 && cur->m_tmplId == tmplId &&
		    sceClibStrncmp(cur->m_scheme, scheme, schemeLen + 1) == 0 &&
		    sceClibStrncmp(cur->m_serverName, serverName, hostLen + 1) == 0 &&
		    cur->m_port == port) {
			cur->m_inUse = 1;
			*outConnId = cur->m_connId;
			m_mutex.Unlock();
			return 0;
		}
	}

	if (m_activeCount >= m_maxConnections) {
		TrophyHttpConnPoolEntry *evict = m_head;
		while (evict != NULL && evict->m_inUse != 0)
			evict = evict->m_next;

		if (evict == NULL) {
			m_mutex.Unlock();
			return NP_TROPHY_ERR_HTTP_POOL_FULL;
		}

		if (evict->m_next != NULL)
			evict->m_next->m_prev = evict->m_prev;
		else
			m_tail = evict->m_prev;
		if (evict->m_prev != NULL)
			evict->m_prev->m_next = evict->m_next;
		else
			m_head = evict->m_next;
		evict->m_next = NULL;
		evict->m_prev = NULL;
		--m_activeCount;
		delete evict;
	}

	TrophyHttpConnPoolEntry *entry = new TrophyHttpConnPoolEntry();
	if (entry == NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	int ret = entry->Init(tmplId, scheme, serverName, port);
	if (ret < 0) {
		delete entry;
		m_mutex.Unlock();
		return ret;
	}

	entry->m_inUse = 1;
	entry->m_next = NULL;
	entry->m_prev = m_tail;
	if (m_tail != NULL)
		m_tail->m_next = entry;
	if (m_head == NULL)
		m_head = entry;
	m_tail = entry;
	++m_activeCount;

	*outConnId = entry->m_connId;
	m_mutex.Unlock();
	return 0;
}

/* 0x812E773E */
int TrophyHttpConnPool::Release(int connId)
{
	m_mutex.Lock();

	TrophyHttpConnPoolEntry *cur = m_head;
	while (cur != NULL && cur->m_connId != connId)
		cur = cur->m_next;

	if (cur == NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_HTTP_CONN_NOT_FOUND;
	}

	if (cur->m_evictOnRelease != 0) {
		if (cur->m_next != NULL)
			cur->m_next->m_prev = cur->m_prev;
		else
			m_tail = cur->m_prev;
		if (cur->m_prev != NULL)
			cur->m_prev->m_next = cur->m_next;
		else
			m_head = cur->m_next;
		cur->m_next = NULL;
		cur->m_prev = NULL;
		--m_activeCount;
		delete cur;
	} else {
		cur->m_inUse = 0;
	}

	m_mutex.Unlock();
	return 0;
}

/* 0x812E77B0 */
int TrophyHttpConnPool::ReportError(int connId, int errorCode)
{
	m_mutex.Lock();

	TrophyHttpConnPoolEntry *cur = m_head;
	while (cur != NULL && cur->m_connId != connId)
		cur = cur->m_next;

	if (cur == NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_HTTP_CONN_NOT_FOUND;
	}

	cur->m_lastError = errorCode;
	cur->m_evictOnRelease = 1;
	m_mutex.Unlock();
	return 0;
}

/* =========================================================================
 * TrophyHttpRequest & TrophyHttpStream (0x812E6C6C..0x812E736E)
 * ========================================================================= */

/* 0x812E6E18 */
TrophyHttpRequest::TrophyHttpRequest()
	: m_pool(NULL), m_connId(0), m_reqId(0), m_bytesRead(0)
{
}

/* 0x812E6E30 */
TrophyHttpRequest::~TrophyHttpRequest()
{
	if (m_reqId != 0)
		sceHttpDeleteRequest(m_reqId);
	if (m_connId != 0) {
		if (m_pool != NULL)
			m_pool->Release(m_connId);
		else
			sceHttpDeleteConnection(m_connId);
	}
}

/* 0x812E6EB6 */
int TrophyHttpRequest::OpenPooled(TrophyHttpTemplate *httpTemplate, TrophyHttpConnPool *pool,
				  int method, const char *scheme, const char *serverName,
				  uint16_t port, const char *path, uint64_t contentLength)
{
	if (m_reqId != 0)
		return NP_TROPHY_ERR_HTTP_ALREADY_OPEN;

	int ret = 0;
	if (pool != NULL) {
		ret = pool->Acquire(httpTemplate, scheme, serverName, port, &m_connId);
	} else {
		int connId = sceHttpCreateConnection(httpTemplate->GetTemplateId(), serverName,
						     scheme, port, 1);
		if (connId < 0)
			ret = connId;
		else
			m_connId = connId;
	}

	if (ret >= 0) {
		m_pool = pool;
		int reqId = sceHttpCreateRequest(m_connId, method, path, contentLength);
		if (reqId >= 0) {
			m_reqId = reqId;
			m_bytesRead = 0;
			return 0;
		}
		ret = reqId;
	}

	if (m_reqId != 0) {
		sceHttpDeleteRequest(m_reqId);
		m_reqId = 0;
	}
	if (m_connId != 0) {
		if (m_pool != NULL)
			m_pool->Release(m_connId);
		else
			sceHttpDeleteConnection(m_connId);
		m_pool = NULL;
		m_connId = 0;
	}
	return ret;
}

/* 0x812E6F84 */
int TrophyHttpRequest::Open(TrophyHttpTemplate *httpTemplate, int method, const char *scheme,
			    const char *serverName, uint16_t port, const char *path,
			    uint64_t contentLength)
{
	return OpenPooled(httpTemplate, NULL, method, scheme, serverName, port, path,
			  contentLength);
}

/* 0x812E6FB4 */
int TrophyHttpRequest::SetTimeouts(const TrophyHttpTimeouts *timeouts)
{
	int ret = 0;

	if (timeouts->connectTimeoutUsec != 0) {
		ret = sceHttpSetConnectTimeOut(m_reqId, timeouts->connectTimeoutUsec);
		if (ret < 0)
			return ret;
	}
	if (timeouts->sendTimeoutUsec != 0) {
		ret = sceHttpSetSendTimeOut(m_reqId, timeouts->sendTimeoutUsec);
		if (ret < 0)
			return ret;
	}
	if (timeouts->recvTimeoutUsec != 0) {
		ret = sceHttpSetRecvTimeOut(m_reqId, timeouts->recvTimeoutUsec);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812E6FE8 */
int TrophyHttpRequest::Close()
{
	if (m_reqId != 0) {
		sceHttpDeleteRequest(m_reqId);
		m_reqId = 0;
	}
	if (m_connId != 0) {
		if (m_pool != NULL)
			m_pool->Release(m_connId);
		else
			sceHttpDeleteConnection(m_connId);
		m_pool = NULL;
		m_connId = 0;
	}
	return 0;
}

/* 0x812E7014 */
int TrophyHttpRequest::SetContentLength(uint64_t contentLength)
{
	if (m_reqId == 0)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	int ret = sceHttpSetRequestContentLength(m_reqId, contentLength);
	if (ret < 0) {
		if (m_pool != NULL)
			m_pool->ReportError(m_connId, ret);
		return ret;
	}
	return 0;
}

/* 0x812E7044 */
int TrophyHttpRequest::Send(TrophyHandle *handle, const void *postBody, SceSize bodySize)
{
	if (m_reqId == 0)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	TrophyHttpCancelToken token(m_reqId);
	int ret = token.Attach(handle);
	if (ret >= 0) {
		int sendRes = sceHttpSendRequest(m_reqId, postBody, bodySize);
		ret = token.Detach(sendRes);
	}
	if (ret < 0) {
		if (m_pool != NULL)
			m_pool->ReportError(m_connId, ret);
		return ret;
	}
	return 0;
}

/* 0x812E710C */
int TrophyHttpRequest::GetStatusCode(TrophyHandle *handle, int *outStatusCode)
{
	if (m_reqId == 0)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	TrophyHttpCancelToken token(m_reqId);
	int ret = token.Attach(handle);
	if (ret >= 0) {
		int statusRes = sceHttpGetStatusCode(m_reqId, outStatusCode);
		ret = token.Detach(statusRes);
	}
	if (ret < 0) {
		if (m_pool != NULL)
			m_pool->ReportError(m_connId, ret);
		return ret;
	}
	return 0;
}

/* 0x812E71C8 */
int TrophyHttpRequest::GetResponseContentLength(TrophyHandle *handle, uint64_t *outContentLength)
{
	if (m_reqId == 0)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	TrophyHttpCancelToken token(m_reqId);
	int ret = token.Attach(handle);
	if (ret >= 0) {
		SceULong64 lengthVal = 0;
		int lenRes = sceHttpGetResponseContentLength(m_reqId, &lengthVal);
		ret = token.Detach(lenRes);
		if (ret >= 0)
			*outContentLength = lengthVal;
	}
	if (ret < 0) {
		if (m_pool != NULL)
			m_pool->ReportError(m_connId, ret);
		return ret;
	}
	return 0;
}

/* 0x812E7284 */
int TrophyHttpRequest::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (m_reqId == 0)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;
	if (outBuf == NULL || size == 0)
		return NP_TROPHY_ERR_HTTP_BAD_ARGUMENT;

	TrophyHttpCancelToken token(m_reqId);
	int ret = token.Attach(handle);
	if (ret >= 0) {
		int readRes = sceHttpReadData(m_reqId, outBuf, size);
		ret = token.Detach(readRes);
	}
	if (ret < 0) {
		if (m_pool != NULL)
			m_pool->ReportError(m_connId, ret);
		return ret;
	}

	*outRead = static_cast<SceSize>(ret);
	m_bytesRead += *outRead;
	return 0;
}

TrophyHttpStream::TrophyHttpStream() : m_peekByte(0), m_hasPeekByte(0) {}

/* 0x812E6C6C */
int TrophyHttpStream::Open(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			   TrophyHttpConnPool *pool, const char *scheme, const char *serverName,
			   uint16_t port, const char *path, const TrophyHttpTimeouts *timeouts)
{
	m_hasPeekByte = 0;
	int ret = m_request.OpenPooled(httpTemplate, pool, 0, scheme, serverName, port, path, 0);
	if (ret < 0)
		return ret;

	if (timeouts != NULL) {
		ret = m_request.SetTimeouts(timeouts);
		if (ret < 0) {
			m_request.Close();
			return ret;
		}
	}

	ret = m_request.Send(handle, NULL, 0);
	if (ret >= 0) {
		int statusCode = 0;
		ret = m_request.GetStatusCode(handle, &statusCode);
		if (ret >= 0) {
			if (statusCode != 200) {
				ret = NP_TROPHY_ERR_HTTP_STATUS_ERROR;
			} else {
				uint64_t contentLength = 0;
				ret = m_request.GetResponseContentLength(handle, &contentLength);
				if (ret == TROPHY_HTTP_ERR_NO_CONTENT_LENGTH) {
					SceSize probeRead = 0;
					ret = m_request.Read(handle, &m_peekByte, 1, &probeRead);
					if (ret >= 0) {
						m_hasPeekByte = probeRead;
						if (probeRead != 0)
							return 0;
						ret = NP_TROPHY_ERR_HTTP_STATUS_ERROR;
					}
				} else if (ret >= 0) {
					if (contentLength != 0)
						return 0;
					ret = NP_TROPHY_ERR_HTTP_STATUS_ERROR;
				}
			}
		}
	}

	if (m_request.IsOpen())
		m_request.Close();
	return ret;
}

/* 0x812E6DAE */
int TrophyHttpStream::Close()
{
	if (!m_request.IsOpen())
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;
	m_request.Close();
	return 0;
}

/* 0x812E6DD2 */
int TrophyHttpStream::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (outBuf == NULL || size == 0)
		return NP_TROPHY_ERR_HTTP_BAD_ARGUMENT;

	if (m_hasPeekByte != 0) {
		sceClibMemcpy(outBuf, &m_peekByte, 1);
		*outRead = 1;
		--m_hasPeekByte;
		return 0;
	}
	return m_request.Read(handle, outBuf, size, outRead);
}

/* =========================================================================
 * Network online & CDN URL / path formatting (0x812E26B2..0x812E307F, 0x812E738A..0x812E74A9)
 * ========================================================================= */

/* 0x812E738A */
bool npTrophyNetIsOnline()
{
	int netState = 0;
	return (SceNetCtl_E8E8DF18(&netState) >= 0 && netState == 4);
}

/* 0x812E73C8 */
int npTrophyFormatUrl(char *outUrl, const char *scheme, const char *host, int partCount, ...)
{
	va_list ap;
	int ret = 0;

	sceClibMemset(outUrl, 0, 512);
	if (npTrophyStrlcat(outUrl, scheme, 512) >= 512 ||
	    npTrophyStrlcat(outUrl, "://", 512) >= 512 ||
	    npTrophyStrlcat(outUrl, host, 512) >= 512 ||
	    npTrophyStrlcat(outUrl, "/", 512) >= 512) {
		sceClibMemset(outUrl, 0, 512);
		return NP_TROPHY_ERR_URL_TOO_LONG;
	}

	SceSize prefixLen = sceClibStrnlen(outUrl, 0x7FFFFFFFu);
	va_start(ap, partCount);
	ret = npTrophyBuildPathV(outUrl + prefixLen, 512 - prefixLen, '/', partCount, ap);
	va_end(ap);

	if (ret < 0) {
		sceClibMemset(outUrl, 0, 512);
		return ret;
	}
	return 0;
}

/* 0x812E26B2 */
int npTrophyFormatTrophyHost(char *outHost, const char *npEnv)
{
	sceClibMemset(outHost, 0, 64);
	if (npTrophyStrlcat(outHost, "trophy01.", 64) >= 64 ||
	    npTrophyStrlcat(outHost, npEnv, 64) >= 64 ||
	    npTrophyStrlcat(outHost, ".community.playstation.net", 64) >= 64) {
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	}
	return 0;
}

/* 0x812E2E08 */
int npTrophyAppendSignedAssetSuffix(char *pathBuf, SceSize pathSize, const char *keyVersion,
				    const TrophyCommId *commId, const char *assetName,
				    const char *encPrefix)
{
	const uint8_t *hmacKey = findUrlHmacKey(keyVersion);
	if (hmacKey == NULL)
		return NP_TROPHY_ERR_GENERIC;

	SceSize assetLen = sceClibStrnlen(assetName, 0x7FFFFFFFu);
	SceSize combinedCap = assetLen + 15;
	char *combinedBuf = static_cast<char *>(TrophyHeapObject::operator new(combinedCap));
	if (combinedBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	char commStr[16];
	sceClibMemset(commStr, 0, sizeof(commStr));
	int ret = commId->FormatToString(commStr, sizeof(commStr));
	if (ret >= 0)
		ret = npTrophyAppendPathSep(pathBuf, pathSize, '/');

	if (ret >= 0) {
		TrophySha1Digest dirDigest;
		SceSize commLen = sceClibStrnlen(commStr, 0x7FFFFFFFu);
		ret = TrophyHmacSha1Hasher::ComputeDigest(hmacKey, 64, commStr, commLen,
							  &dirDigest);
		if (ret >= 0) {
			if (npTrophyStrlcat(pathBuf, commStr, pathSize) >= pathSize ||
			    npTrophyStrlcat(pathBuf, "_", pathSize) >= pathSize) {
				ret = NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
			} else if (keyVersion != NULL &&
				   npTrophyStrlcat(pathBuf, keyVersion, pathSize) >= pathSize) {
				ret = NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
			} else {
				SceSize curLen = sceClibStrnlen(pathBuf, 0x7FFFFFFFu);
				ret = dirDigest.FormatHexUpper(pathBuf + curLen,
							       pathSize - curLen);
				if (ret >= 0) {
					if (npTrophyStrlcat(pathBuf, "/", pathSize) >= pathSize) {
						ret = NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
					} else if (npTrophyStrlcpy(combinedBuf, commStr,
								   combinedCap) >= combinedCap ||
						   npTrophyStrlcat(combinedBuf, "/",
								   combinedCap) >= combinedCap ||
						   npTrophyStrlcat(combinedBuf, assetName,
								   combinedCap) >= combinedCap) {
						ret = NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
					} else {
						TrophySha1Digest fileDigest;
						SceSize combinedLen = sceClibStrnlen(combinedBuf,
										     0x7FFFFFFFu);
						ret = TrophyHmacSha1Hasher::ComputeDigest(
							hmacKey, 64, combinedBuf, combinedLen,
							&fileDigest);
						if (ret >= 0) {
							curLen = sceClibStrnlen(pathBuf,
										0x7FFFFFFFu);
							ret = fileDigest.FormatHexUpper(
								pathBuf + curLen,
								pathSize - curLen);
						}
						if (ret >= 0) {
							const char *extDot =
								sceClibStrrchr(assetName, '.');
							if (extDot == NULL) {
								ret = NP_TROPHY_ERR_GENERIC;
							} else if (npTrophyStrlcat(pathBuf, ".",
										   pathSize) >=
									   pathSize ||
								   npTrophyStrlcat(pathBuf,
										   encPrefix,
										   pathSize) >=
									   pathSize ||
								   npTrophyStrlcat(pathBuf,
										   extDot + 1,
										   pathSize) >=
									   pathSize) {
								ret = NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
							} else {
								ret = 0;
							}
						}
					}
				}
			}
		}
	}

	TrophyHeapObject::operator delete(combinedBuf);
	return ret;
}

/* 0x812E2704 */
int npTrophyFormatSignedAssetPath(char *outPath, const char *keyVersion, const char *npEnv,
				  const TrophyCommId *commId, const char *assetName,
				  const char *encPrefix)
{
	sceClibMemset(outPath, 0, 256);
	int ret = npTrophyAppendPathComponents(outPath, 256, '/', 3, "/", "trophy", npEnv);
	if (ret < 0)
		return ret;
	return npTrophyAppendSignedAssetSuffix(outPath, 256, keyVersion, commId, assetName,
					       encPrefix);
}

/* =========================================================================
 * TrophyAesCbcDecryptStream (0x812E2774..0x812E2C60)
 * ========================================================================= */

/* 0x812E2B1C */
TrophyAesCbcDecryptStream::TrophyAesCbcDecryptStream()
	: m_ivLoaded(0), m_derivedKey(NULL), m_totalDecrypted(0), m_source(NULL)
{
	sceClibMemset(&m_aesCtx, 0, sizeof(m_aesCtx));
	sceClibMemset(m_cbcChain, 0, sizeof(m_cbcChain));
}

TrophyAesCbcDecryptStream::~TrophyAesCbcDecryptStream()
{
	if (m_source != NULL)
		Close();
	if (m_derivedKey != NULL)
		TrophyHeapObject::operator delete(m_derivedKey);
}

/* 0x812E2774 */
int TrophyAesCbcDecryptStream::InitCipher(const char *keyVersion, const TrophyCommId *commId)
{
	const uint8_t *masterKey = findAesKey(keyVersion);
	if (masterKey == NULL)
		return NP_TROPHY_ERR_GENERIC;

	int ret = m_readBuf.Init(0x10000);
	if (ret < 0)
		return ret;

	m_derivedKey = static_cast<uint8_t *>(TrophyHeapObject::operator new(16));
	if (m_derivedKey == NULL) {
		m_readBuf.Term();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	uint8_t commBlock[16];
	sceClibMemset(commBlock, 0, sizeof(commBlock));
	ret = commId->FormatToString(reinterpret_cast<char *>(commBlock), sizeof(commBlock));
	if (ret >= 0) {
		SceAesContext keyGenCtx;
		sceClibMemset(&keyGenCtx, 0, sizeof(keyGenCtx));
		ret = sceAesScheduleKey(&keyGenCtx, 128, 128, masterKey);
		if (ret >= 0) {
			/* Derive per-title AES key by encrypting zero-padded commId with zero IV */
			sceAesEncrypt(&keyGenCtx, commBlock, m_derivedKey);
			m_ivLoaded = 0;
			m_totalDecrypted = 0;
			return 0;
		}
	}

	TrophyHeapObject::operator delete(m_derivedKey);
	m_derivedKey = NULL;
	m_readBuf.Term();
	return ret;
}

/* 0x812E2B4E */
int TrophyAesCbcDecryptStream::Open(const char *keyVersion, const TrophyCommId *commId,
				    TrophyInputStream *source)
{
	if (m_source != NULL)
		return NP_TROPHY_ERR_AES_ALREADY_OPEN;

	int ret = InitCipher(keyVersion, commId);
	if (ret < 0)
		return ret;

	m_source = source;
	return 0;
}

/* 0x812E2B78 */
int TrophyAesCbcDecryptStream::Close()
{
	if (m_source == NULL)
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	if (m_derivedKey != NULL) {
		TrophyHeapObject::operator delete(m_derivedKey);
		m_derivedKey = NULL;
	}
	m_readBuf.Term();
	m_ivLoaded = 0;
	m_source = NULL;
	return 0;
}

/* 0x812E2906 */
int TrophyAesCbcDecryptStream::DecryptBlocks(uint8_t *outBuf, SceSize maxBytes,
					     SceSize *outDecrypted)
{
	if (maxBytes < 16 || (maxBytes & 0xFu) != 0)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	SceSize avail = m_readBuf.GetAvailable();
	bool atEof = m_readBuf.IsEof();
	if (atEof) {
		if (avail == 0) {
			*outDecrypted = 0;
			return 0;
		}
	} else if (avail <= 16) {
		*outDecrypted = 0;
		return 0;
	}

	if (avail < 16 || (avail & 0xFu) != 0)
		return NP_TROPHY_ERR_AES_BAD_PADDING;

	if (m_ivLoaded == 0) {
		sceClibMemset(&m_aesCtx, 0, sizeof(m_aesCtx));
		int ret = sceAesScheduleKey(&m_aesCtx, 128, 128, m_derivedKey);
		if (ret < 0)
			return ret;

		const uint8_t *ivBytes = m_readBuf.GetCursor();
		sceClibMemcpy(m_cbcChain, ivBytes, 16);
		m_readBuf.Consume(16);
		avail = m_readBuf.GetAvailable();
		m_ivLoaded = 1;
	}

	SceSize toProcess = maxBytes;
	if (toProcess >= avail)
		toProcess = atEof ? avail : (avail - 16);

	SceSize produced = 0;
	uint8_t plainBlock[16];
	while (produced < toProcess) {
		const uint8_t *cipherBlock = m_readBuf.GetCursor();
		sceAesDecrypt(&m_aesCtx, cipherBlock, plainBlock);

		for (SceSize i = 0; i < 16; ++i) {
			uint8_t decryptedByte = plainBlock[i] ^ m_cbcChain[i];
			m_cbcChain[i] = cipherBlock[i];
			outBuf[produced + i] = decryptedByte;
		}

		m_readBuf.Consume(16);
		produced += 16;
	}

	if (!atEof || m_readBuf.GetAvailable() != 0) {
		*outDecrypted = produced;
		m_totalDecrypted += produced;
		return 0;
	}

	/* Verify and strip PKCS#7 padding on the final block */
	uint8_t padLen = outBuf[produced - 1];
	if (padLen > produced)
		return NP_TROPHY_ERR_AES_BAD_PADDING;
	for (SceSize i = 0; i < padLen; ++i) {
		if (outBuf[produced - 1 - i] != padLen)
			return NP_TROPHY_ERR_AES_BAD_PADDING;
	}

	SceSize unpadded = produced - padLen;
	*outDecrypted = unpadded;
	m_totalDecrypted += unpadded;
	return 0;
}

/* 0x812E2BB0 */
int TrophyAesCbcDecryptStream::Read(TrophyHandle *handle, void *outBuf, SceSize size,
				    SceSize *outRead)
{
	if (m_source == NULL)
		return NP_TROPHY_ERR_STREAM_NOT_INIT;

	if (size == 0) {
		*outRead = 0;
		return 0;
	}

	uint8_t *dstBytes = static_cast<uint8_t *>(outBuf);
	SceSize decrypted = 0;
	int ret = DecryptBlocks(dstBytes, size, &decrypted);
	if (ret < 0)
		return ret;

	if (decrypted == 0) {
		ret = m_readBuf.Refill(handle, m_source);
		if (ret < 0)
			return ret;
		ret = DecryptBlocks(dstBytes, size, &decrypted);
		if (ret < 0)
			return ret;
	}

	if (decrypted == 0 && !m_readBuf.IsEof())
		return NP_TROPHY_ERR_AES_BAD_PADDING;

	*outRead = decrypted;
	return 0;
}

/* =========================================================================
 * TrophyNetAssetStream, TrophyNetIconStream, TrophyNetConfStream
 * (0x812E2360..0x812E328E)
 * ========================================================================= */

/* 0x812E2360 */
TrophyNetAssetStream::TrophyNetAssetStream()
	: m_boundHandle(NULL), m_decryptStream(NULL), m_activeStream(NULL)
{
}

/* 0x812E23A2 */
TrophyNetAssetStream::~TrophyNetAssetStream()
{
	if (m_decryptStream != NULL)
		delete m_decryptStream;
	if (m_boundHandle != NULL)
		m_boundHandle->ReleaseRef();
}

/* 0x812E2410 */
int TrophyNetAssetStream::OpenAsset(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
				    TrophyHttpConnPool *pool, const TrophyCommId *commId,
				    const char *assetName, const TrophyHttpTimeouts *timeouts,
				    char *outUrlBuf, const char *keyVersion)
{
	char npEnv[20];
	char hostName[64];
	char assetPath[260];

	int ret = npTrophyGetNpEnvironment(npEnv);
	if (ret < 0)
		return ret;
	ret = npTrophyFormatTrophyHost(hostName, npEnv);
	if (ret < 0)
		return ret;

	/* Check whether assetName is an SFM file (encrypted on CDN) or plain icon */
	TrophyAssetDesc assetDesc;
	bool isEncrypted = (npTrophyParseAssetFilename(assetName, &assetDesc) == 0 &&
			    assetDesc.kind == TROPHY_ASSET_SFM);

	if (isEncrypted) {
		m_decryptStream = new TrophyAesCbcDecryptStream();
		if (m_decryptStream == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;

		ret = npTrophyFormatSignedAssetPath(assetPath, keyVersion, npEnv, commId,
						    assetName, kEncryptedExtPrefix);
		if (ret >= 0) {
			ret = m_httpStream.Open(handle, httpTemplate, pool, "http", hostName, 80,
						assetPath, timeouts);
		}
		if (ret >= 0)
			ret = m_decryptStream->Open(keyVersion, commId, &m_httpStream);

		if (ret < 0) {
			delete m_decryptStream;
			m_decryptStream = NULL;
			if (m_httpStream.IsOpen())
				m_httpStream.Close();
			return ret;
		}
		m_activeStream = m_decryptStream;
	} else {
		ret = npTrophyFormatSignedAssetPath(assetPath, keyVersion, npEnv, commId,
						    assetName, kPlainExtPrefix);
		if (ret >= 0) {
			ret = m_httpStream.Open(handle, httpTemplate, pool, "http", hostName, 80,
						assetPath, timeouts);
		}
		if (ret < 0) {
			if (m_httpStream.IsOpen())
				m_httpStream.Close();
			return ret;
		}
		m_activeStream = &m_httpStream;
	}

	if (outUrlBuf != NULL)
		npTrophyFormatUrl(outUrlBuf, "http", hostName, 1, assetPath);

	handle->AddRef();
	m_boundHandle = handle;
	return 0;
}

/* 0x812E25E4 */
int TrophyNetAssetStream::OpenAssetDirect(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
					  const TrophyCommId *commId, const char *assetName,
					  const TrophyHttpTimeouts *timeouts, char *outUrlBuf,
					  const char *keyVersion)
{
	return OpenAsset(handle, httpTemplate, NULL, commId, assetName, timeouts, outUrlBuf,
			 keyVersion);
}

/* 0x812E260E */
int TrophyNetAssetStream::Close()
{
	if (!m_httpStream.IsOpen())
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;

	int ret = m_httpStream.Close();
	if (ret < 0)
		return ret;

	if (m_decryptStream != NULL) {
		m_decryptStream->Close();
		delete m_decryptStream;
		m_decryptStream = NULL;
	}
	if (m_boundHandle != NULL) {
		m_boundHandle->ReleaseRef();
		m_boundHandle = NULL;
	}
	return 0;
}

/* 0x812E265C */
int TrophyNetAssetStream::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (handle != m_boundHandle)
		return NP_TROPHY_ERR_BUSY;
	return m_activeStream->Read(handle, outBuf, size, outRead);
}

/* 0x812E2CD6 */
int TrophyNetIconStream::OpenTitleIcon(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
				       TrophyHttpConnPool *pool, const TrophyCommId *commId,
				       int lang, int smallFlag, bool allowDefaultFallback,
				       char *outUrlBuf)
{
	char iconName[32];
	int ret = npTrophyFormatTitleIconFilename(iconName, lang, smallFlag);
	if (ret < 0)
		return ret;

	ret = OpenAsset(handle, httpTemplate, pool, commId, iconName, NULL, outUrlBuf,
			kDefaultKeyVersion);
	if (ret == NP_TROPHY_ERR_HTTP_STATUS_ERROR)
		ret = NP_TROPHY_ERR_HTTP_ICON_NOT_FOUND;

	if (ret == NP_TROPHY_ERR_HTTP_ICON_NOT_FOUND && lang >= 0 && allowDefaultFallback) {
		ret = npTrophyFormatTitleIconFilename(iconName, -1, smallFlag);
		if (ret < 0)
			return ret;
		ret = OpenAsset(handle, httpTemplate, pool, commId, iconName, NULL, outUrlBuf,
				kDefaultKeyVersion);
		if (ret == NP_TROPHY_ERR_HTTP_STATUS_ERROR)
			ret = NP_TROPHY_ERR_HTTP_ICON_NOT_FOUND;
	}
	return (ret >= 0) ? 0 : ret;
}

/* 0x812E3100 */
int TrophyNetConfStream::OpenConf(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
				  TrophyHttpConnPool *pool, const TrophyCommId *commId, int lang,
				  bool allowDefaultFallback, const TrophyHttpTimeouts *timeouts)
{
	char sfmName[32];
	int ret = npTrophyFormatSfmFilename(sfmName, lang);
	if (ret >= 0) {
		ret = OpenAsset(handle, httpTemplate, pool, commId, sfmName, timeouts, NULL,
				kDefaultKeyVersion);
		if (ret == NP_TROPHY_ERR_HTTP_STATUS_ERROR && lang >= 0 && allowDefaultFallback) {
			ret = npTrophyFormatSfmFilename(sfmName, -1);
			if (ret >= 0) {
				ret = OpenAsset(handle, httpTemplate, pool, commId, sfmName,
						timeouts, NULL, kDefaultKeyVersion);
			}
		}
		if (ret >= 0) {
			ret = m_stripper.Init(this);
			if (ret >= 0)
				return 0;
		}
	}

	m_stripper.Term();
	if (IsOpen())
		Close();
	return (ret == NP_TROPHY_ERR_HTTP_STATUS_ERROR) ? NP_TROPHY_ERR_HTTP_NOT_FOUND : ret;
}

/* 0x812E322C */
int TrophyNetConfStream::OpenConfDirect(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
					const TrophyCommId *commId, int lang,
					bool allowDefaultFallback,
					const TrophyHttpTimeouts *timeouts)
{
	return OpenConf(handle, httpTemplate, NULL, commId, lang, allowDefaultFallback, timeouts);
}

/* 0x812E324A */
int TrophyNetConfStream::CloseConf()
{
	if (!IsOpen())
		return NP_TROPHY_ERR_HTTP_NOT_OPEN;
	int ret = m_stripper.Term();
	if (ret < 0)
		return ret;
	return Close();
}
