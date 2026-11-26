/*
 * np_sns_fb_lib.cpp: fblib Facebook REST client, allocator/mutex utilities,
 * MD5 request signer, URL-encoded form builder, HTTP/bXCe SAX transport,
 * auth/FQL/access-token/grant-permission SAX parsers, and stream-publish client
 * (0x813F85B4..0x813FA7F7, 0x81440528..0x814405F7, 0x81455EAC..0x81456740).
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libmd5.h>
#include <string.h>
#include "np_sns_fb_internal.h"

struct NpSnsFbMspaceStats {
	struct MspaceStats stats;
	SceUInt32 extraStats[4];
};

#define NP_SNS_FB_BXCE_ERR_NEED_MORE	0x80852100U
#define NP_SNS_FB_DEFAULT_WORK_SIZE	0x2000U
#define NP_SNS_FB_SAX_READ_CHUNK	0x400U
#define NP_SNS_FB_SAX_DEFAULT_TEXT_CAP	256U
#define NP_SNS_FB_TOKEN_TEXT_CAP	4097U
#define NP_SNS_FB_AUTH_TEXT_BUF_MAX	256

/* Global allocator state (0x815494F8..0x81549504, 0x815D1210..0x815D1218) */
static SceInt32 s_fbUseCallbackAlloc = 0;
static SceInt32 s_fbUseExternalMspace = 0;
static SceInt32 s_fbAllocCount = 0;
static NpSnsFbAllocCallback s_fbAllocCallback = NULL;
static NpSnsFbFreeCallback s_fbFreeCallback = NULL;
static void *s_fbMspace = NULL;
static SceUID s_fbMemBlockId = -1;

/* Stream-publish module global config (0x81549574..0x81549580) */
struct NpSnsFbStreamGlobalState {
	SceUInt8 initialized;
	SceUInt8 pad[3];
	SceInt32 httpTemplateId;
	NpSnsFbAllocCallback allocFunc;
	NpSnsFbFreeCallback freeFunc;
};

static NpSnsFbStreamGlobalState s_streamState;

/* HTTP endpoint descriptor passed to npSnsFbHttpPostAndParse */
struct NpSnsFbHttpEndpoint {
	const char *scheme;					/* +0x00 */
	const char *host;					/* +0x04 */
	const char *user;					/* +0x08 */
	const char *password;					/* +0x0C */
	const char *path;					/* +0x10 */
	SceUInt32 port;						/* +0x14 */
};

/* HTTP response & SAX handler binding (28 bytes = 0x1C) */
struct NpSnsFbHttpResponseContext {
	void *reservedHeader;					/* +0x00 */
	void *reservedCallback;					/* +0x04 */
	BxceSaxHandler *saxHandler;				/* +0x08 */
	void *reservedUser;					/* +0x0C */
	void *saxUserData;					/* +0x10 */
	SceInt32 httpStatusCode;				/* +0x14 */
	SceInt32 reservedStatus;				/* +0x18 */
};

/* Internal SAX text accumulator (12 bytes = 0x0C) */
struct NpSnsFbTextBuffer {
	SceSize capacity;					/* +0x00 */
	SceSize length;						/* +0x04 */
	char *text;						/* +0x08 */

	NpSnsFbTextBuffer()
		: capacity(NP_SNS_FB_SAX_DEFAULT_TEXT_CAP), length(0), text(NULL)
	{
	}

	~NpSnsFbTextBuffer()
	{
		if (text != NULL) {
			npSnsFbMemFree(text);
			text = NULL;
		}
	}

	void reset()
	{
		if (text != NULL)
			sceClibMemset(text, 0, capacity);
		length = 0;
	}

	int append(const char *src, SceSize len)
	{
		if (text == NULL) {
			text = static_cast<char *>(npSnsFbMemAlloc(capacity));
			if (text != NULL)
				sceClibMemset(text, 0, capacity);
			if (text == NULL)
				return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		}
		if (length + len >= capacity)
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		sceClibMemcpy_safe(text + length, src, len);
		length += len;
		text[length] = '\0';
		return 0;
	}

	const char *cStr()
	{
		if (text == NULL) {
			text = static_cast<char *>(npSnsFbMemAlloc(capacity));
			if (text != NULL)
				sceClibMemset(text, 0, capacity);
		}
		return text;
	}
};

/* ============================================================================
 * 1. Allocator, Base Object, LwMutex & ScopedLock (0x813F85B4..0x813F891F)
 * ============================================================================ */

/* 0x813F85B4, 0x813F85CA */
NpSnsFbAllocObject::NpSnsFbAllocObject()
{
}

NpSnsFbAllocObject::~NpSnsFbAllocObject()
{
}

/* 0x813F85E0 */
void *NpSnsFbAllocObject::operator new(SceSize size)
{
	return npSnsFbMemAlloc(size);
}

/* 0x813F85C2 */
void NpSnsFbAllocObject::operator delete(void *ptr)
{
	npSnsFbMemFree(ptr);
}

/* 0x813F85E8 */
int npSnsFbMemInitCallbacks(NpSnsFbAllocCallback allocFunc, NpSnsFbFreeCallback freeFunc)
{
	s_fbUseCallbackAlloc = 1;
	s_fbAllocCallback = allocFunc;
	s_fbFreeCallback = freeFunc;
	s_fbUseExternalMspace = 0;
	s_fbMspace = NULL;
	s_fbAllocCount = 0;
	s_fbMemBlockId = -1;
	if (allocFunc == NULL || freeFunc == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return 0;
}

/* 0x813F864C */
void npSnsFbMemTerm(void)
{
	if (s_fbUseCallbackAlloc != 0) {
		s_fbAllocCallback = NULL;
		s_fbFreeCallback = NULL;
		s_fbAllocCount = 0;
	} else if (s_fbUseExternalMspace != 0) {
		if (s_fbMspace != NULL) {
			NpSnsFbMspaceStats mspaceStats;
			sceClibMspaceMallocStats(s_fbMspace, &mspaceStats.stats);
			sceClibMspaceDestroy(s_fbMspace);
			s_fbAllocCount = 0;
		}
		s_fbMspace = NULL;
	} else {
		if (s_fbMspace != NULL) {
			NpSnsFbMspaceStats mspaceStats;
			sceClibMspaceMallocStats(s_fbMspace, &mspaceStats.stats);
			sceClibMspaceDestroy(s_fbMspace);
			s_fbAllocCount = 0;
			if (s_fbMemBlockId != 0) {
				s_fbMspace = NULL;
				sceKernelFreeMemBlock(s_fbMemBlockId);
				s_fbMemBlockId = 0;
			}
		}
		s_fbMspace = NULL;
	}
}

/* 0x813F873A */
void *npSnsFbMemAlloc(SceSize size)
{
	void *mem = NULL;
	if (s_fbUseCallbackAlloc != 0) {
		if (s_fbAllocCallback != NULL)
			mem = s_fbAllocCallback(size);
		if (mem == NULL)
			return NULL;
	} else {
		if (s_fbMspace == NULL)
			return NULL;
		mem = sceClibMspaceMalloc(s_fbMspace, size);
		if (mem == NULL) {
			NpSnsFbMspaceStats mspaceStats;
			sceClibMspaceMallocStats(s_fbMspace, &mspaceStats.stats);
			return NULL;
		}
	}
	++s_fbAllocCount;
	return mem;
}

/* 0x813F87BC */
void npSnsFbMemFree(void *ptr)
{
	--s_fbAllocCount;
	if (s_fbUseCallbackAlloc != 0) {
		if (s_fbFreeCallback != NULL)
			s_fbFreeCallback(ptr);
	} else if (s_fbMspace != NULL) {
		sceClibMspaceFree(s_fbMspace, ptr);
	}
}

/* 0x813F882C, 0x813F885C, 0x813F8880 */
NpSnsFbLwMutex::NpSnsFbLwMutex(const char *name)
	: alignPad(0)
{
	sceKernelCreateLwMutex(&m_work, name, 2U, 0, NULL);
}

NpSnsFbLwMutex::~NpSnsFbLwMutex()
{
	sceKernelDeleteLwMutex(&m_work);
}

/* 0x8118A770 */
int NpSnsFbLwMutex::lock()
{
	return sceKernelLockLwMutex(&m_work, 1, NULL);
}

/* 0x8118A764 */
int NpSnsFbLwMutex::unlock()
{
	return sceKernelUnlockLwMutex(&m_work, 1);
}

/* 0x813F88AC, 0x813F88D4, 0x813F88F6 */
NpSnsFbScopedLock::NpSnsFbScopedLock(NpSnsFbLwMutex *mutex)
	: m_mutex(mutex)
{
	if (m_mutex != NULL)
		m_mutex->lock();
}

NpSnsFbScopedLock::~NpSnsFbScopedLock()
{
	if (m_mutex != NULL)
		m_mutex->unlock();
}

/* ============================================================================
 * 2. Permission Linked-List Map (0x81440528..0x814405F7)
 * ============================================================================ */

NpSnsFbPermissionNode::NpSnsFbPermissionNode()
	: prev(NULL), next(NULL)
{
	sceClibMemset(value.header, 0, sizeof(value.header));
	sceClibMemset(value.summary, 0, sizeof(value.summary));
	value.granted = 0;
	sceClibMemset(permissionName, 0, sizeof(permissionName));
}

/* 0x81440528, 0x8144058C */
NpSnsFbPermissionNode::~NpSnsFbPermissionNode()
{
}

NpSnsFbPermissionMap::NpSnsFbPermissionMap()
{
}

/* 0x81440544, 0x814405AC */
NpSnsFbPermissionMap::~NpSnsFbPermissionMap()
{
	NpSnsFbPermissionNode *cur = m_head.next;
	m_head.next = NULL;
	while (cur != NULL) {
		NpSnsFbPermissionNode *nextNode = cur->next;
		delete cur;
		cur = nextNode;
	}
}

NpSnsFbPermissionValue *NpSnsFbPermissionMap::findOrInsert(const char *permName)
{
	for (NpSnsFbPermissionNode *cur = m_head.next; cur != NULL; cur = cur->next) {
		if (sceClibStrncmp(cur->permissionName, permName, 64U) == 0)
			return &cur->value;
	}

	NpSnsFbPermissionNode *tail = &m_head;
	while (tail->next != NULL)
		tail = tail->next;

	NpSnsFbPermissionNode *fresh = new NpSnsFbPermissionNode();
	if (fresh == NULL)
		return &m_head.value;

	sceClibStrncpy(fresh->permissionName, permName, 64U);
	tail->next = fresh;
	fresh->prev = tail;
	return &fresh->value;
}

/* ============================================================================
 * 3. MD5 Request Signer, Form Encoder & HTTP/SAX Transport (0x813F8920..0x813F932F)
 * ============================================================================ */

/* 0x813F8920 */
static int npSnsFbSignParamsMd5(const NpSnsFbParamList *params, const char *secret,
				char *outHexSig)
{
	SceMd5Context md5Ctx;
	int ret = sceMd5BlockInit(&md5Ctx);
	if (ret != 0)
		return ret;

	for (SceUInt32 idx = 0; idx < params->count; ++idx) {
		const char *key = params->pairs[idx].key;
		const char *value = params->pairs[idx].value;
		if (key != NULL && value != NULL) {
			sceMd5BlockUpdate(&md5Ctx, key, sceClibStrnlen(key, 0x7FFFFFFFU));
			sceMd5BlockUpdate(&md5Ctx, "=", 1U);
			sceMd5BlockUpdate(&md5Ctx, value, sceClibStrnlen(value, 0x7FFFFFFFU));
		}
	}

	sceMd5BlockUpdate(&md5Ctx, secret, sceClibStrnlen(secret, 0x7FFFFFFFU));

	SceUInt8 digest[16];
	sceMd5BlockResult(&md5Ctx, digest);

	char *cursor = outHexSig;
	for (int byteIdx = 0; byteIdx < 16; ++byteIdx) {
		sceClibSnprintf(cursor, 3U, "%02x", static_cast<unsigned int>(digest[byteIdx]));
		cursor += 2;
	}
	outHexSig[32] = '\0';
	return 0;
}

/* 0x813F8A20 */
static int npSnsFbBuildFormBody(const NpSnsFbParamList *params, const char *sigHex,
				char *dstBuf, SceSize dstBufSize)
{
	if (params == NULL || sigHex == NULL || dstBuf == NULL || dstBufSize == 0)
		return NP_SNS_FB_POSIX_EINVAL;

	sceClibMemset(dstBuf, 0, dstBufSize);

	SceSize written = 0;
	SceSize required = 0;
	int ret = 0;

	for (SceUInt32 idx = 0; idx < params->count; ++idx) {
		const char *key = params->pairs[idx].key;
		const char *value = params->pairs[idx].value;
		if (key == NULL || value == NULL)
			continue;

		ret = sceHttpUriEscape(NULL, &required, 0, key);
		if (ret != 0)
			break;
		ret = sceHttpUriEscape(dstBuf + written, &required, dstBufSize - written, key);
		if (ret != 0)
			break;
		written += required - 1U;

		if (dstBufSize - written < 2U) {
			ret = NP_SNS_FB_HTTP_ERR_BUFFER_FULL;
			break;
		}
		sceClibStrncat(dstBuf, "=", dstBufSize - written);
		++written;

		if (sceClibStrnlen(value, 0x7FFFFFFFU) != 0) {
			ret = sceHttpUriEscape(NULL, &required, 0, value);
			if (ret != 0)
				break;
			ret = sceHttpUriEscape(dstBuf + written, &required,
					       dstBufSize - written, value);
			if (ret != 0)
				break;
			written += required - 1U;
		}

		if (dstBufSize - written < 2U) {
			ret = NP_SNS_FB_HTTP_ERR_BUFFER_FULL;
			break;
		}
		sceClibStrncat(dstBuf, "&", dstBufSize - written);
		++written;
	}

	SceSize sigLen = sceClibStrnlen(sigHex, 0x7FFFFFFFU);
	if (written + sigLen + 4U >= dstBufSize)
		return NP_SNS_FB_HTTP_ERR_BUFFER_FULL;

	sceClibStrncat(dstBuf, "sig=", dstBufSize - written);
	sceClibStrncat(dstBuf, sigHex, dstBufSize - (written + 4U));
	return ret;
}

/* 0x813F8BDA */
static int npSnsFbHttpSendBody(SceInt32 httpConnId, SceInt32 httpReqId, const char *body,
			       SceUInt64 bodyLen, SceUInt64 *outSent)
{
	(void)httpConnId;
	SceSize lastSent = 0;
	SceSize totalSent = 0;
	int ret = 0;

	if (bodyLen != 0) {
		while (totalSent < bodyLen) {
			SceSize chunkLen = static_cast<SceSize>(bodyLen) - totalSent;
			int sendRet = sceHttpSendRequest(httpReqId, body + totalSent, chunkLen);
			lastSent = chunkLen;
			ret = (sendRet >= 0) ? 0 : sendRet;
			if (ret != 0)
				break;
			totalSent += lastSent;
		}
	} else {
		int sendRet = sceHttpSendRequest(httpReqId, NULL, 0);
		ret = (sendRet >= 0) ? 0 : sendRet;
	}

	*outSent = static_cast<SceUInt64>(lastSent);
	return ret;
}

static void npSnsFbCloseRequestHttpLocked(NpSnsFbLibRequest *req)
{
	if (sceKernelLockLwMutex(&req->mutexWork, 1, NULL) == 0) {
		if (req->httpReqId > 0)
			sceHttpDeleteRequest(req->httpReqId);
		if (req->httpConnId > 0)
			sceHttpDeleteConnection(req->httpConnId);
		req->httpConnId = 0;
		req->httpReqId = 0;
		req->requestActive = 0;
		sceKernelUnlockLwMutex(&req->mutexWork, 1);
	}
}

static int npSnsFbParseHttpStreamSax(SceInt32 httpReqId, char *readBuf, SceSize readBufSize,
				     BxceSaxHandler *handler, void *userData)
{
	BxceSaxParser *parser = static_cast<BxceSaxParser *>(ScebXCe_B8DDE741(sizeof(BxceSaxParser)));
	if (parser == NULL)
		return NP_SNS_FB_POSIX_ENOMEM;

	int ret = ScebXCe_C85586BD(parser);
	if (ret == 0) {
		ScebXCe_FF7CB737(parser, 1);
		parser->userData = userData;
		parser->handler = handler;
		parser->abortFlag = 0;

		for (;;) {
			int readRet = sceHttpReadData(httpReqId, readBuf, readBufSize);
			SceSize bytesRead = (readRet >= 0) ? static_cast<SceSize>(readRet) : 0;
			ret = (readRet >= 0) ? 0 : readRet;
			if (ret != 0)
				break;

			SceUInt32 parseRet = ScebXCe_4E9A7061(parser, readBuf, bytesRead,
							      (bytesRead == 0) ? 1 : 0);
			if (parseRet == NP_SNS_FB_BXCE_ERR_NEED_MORE)
				continue;
			if (parseRet != 0) {
				ret = static_cast<int>(parseRet);
				break;
			}
			if (bytesRead == 0)
				break;
		}
	}

	ScebXCe_928189AB(parser);
	ScebXCe_EC560647(parser);
	return ret;
}

/* 0x813F8CB8 */
static int npSnsFbHttpExecuteAndParse(SceInt32 httpTemplateId, NpSnsFbLibRequest *req,
				      const NpSnsFbHttpEndpoint *endpoint,
				      const char *const *headers, const char *method,
				      char *workBuf, SceSize workBufSize, SceSize bodyLen,
				      NpSnsFbHttpResponseContext *respCtx)
{
	int ret = sceKernelLockLwMutex(&req->mutexWork, 1, NULL);
	if (ret != 0)
		return ret;

	if (req->aborted != 0) {
		sceKernelUnlockLwMutex(&req->mutexWork, 1);
		return NP_SNS_FB_POSIX_ECANCELED;
	}

	req->httpConnId = 0;
	req->httpReqId = 0;
	SceInt32 httpMethod = (sceClibStrncmp(method, "POST", 4U) == 0) ? 1 : 0;

	SceInt32 connId = sceHttpCreateConnection(httpTemplateId, endpoint->host,
						  endpoint->scheme, endpoint->port, 1);
	if (connId < 0) {
		ret = connId;
		sceKernelUnlockLwMutex(&req->mutexWork, 1);
		npSnsFbCloseRequestHttpLocked(req);
		return ret;
	}

	req->httpConnId = connId;
	SceInt32 reqId = sceHttpCreateRequest(connId, httpMethod, endpoint->path, 0);
	if (reqId < 0) {
		ret = reqId;
		sceHttpDeleteConnection(connId);
		req->httpConnId = 0;
		sceKernelUnlockLwMutex(&req->mutexWork, 1);
		npSnsFbCloseRequestHttpLocked(req);
		return ret;
	}

	req->httpReqId = reqId;
	req->requestActive = 1;
	ret = sceKernelUnlockLwMutex(&req->mutexWork, 1);

	if (ret == 0) {
		for (const char *const *hdr = headers; hdr != NULL && hdr[0] != NULL; hdr += 2) {
			ret = sceHttpAddRequestHeader(req->httpReqId, hdr[0], hdr[1], 1U);
			if (ret != 0)
				break;
		}
	}

	if (ret == 0 && sceClibStrncmp(method, "POST", 0x400U) == 0)
		ret = sceHttpSetRequestContentLength(req->httpReqId,
						     static_cast<SceUInt64>(bodyLen));

	if (ret != 0) {
		npSnsFbCloseRequestHttpLocked(req);
		return ret;
	}

	SceUInt64 sentBytes = 0;
	if (bodyLen != 0) {
		ret = npSnsFbHttpSendBody(req->httpConnId, req->httpReqId, workBuf,
					  static_cast<SceUInt64>(bodyLen), &sentBytes);
	} else {
		ret = npSnsFbHttpSendBody(req->httpConnId, req->httpReqId, NULL, 0, &sentBytes);
	}

	if (ret == 0)
		ret = sceHttpGetStatusCode(req->httpReqId, &respCtx->httpStatusCode);

	if (ret == 0) {
		if (respCtx->httpStatusCode != 200) {
			ret = NP_SNS_FB_HTTP_ERR_BAD_STATUS;
		} else {
			ret = npSnsFbParseHttpStreamSax(req->httpReqId, workBuf, workBufSize,
							respCtx->saxHandler,
							respCtx->saxUserData);
		}
	}

	npSnsFbCloseRequestHttpLocked(req);
	return ret;
}

/* 0x813F8FE6 */
static int npSnsFbInvokeRestMethod(const NpSnsFbLibConfig *config, const NpSnsFbSession *session,
				   NpSnsFbLibRequest *req, NpSnsFbParamList *baseParams,
				   const NpSnsFbParamList *overrideParams,
				   NpSnsFbHttpResponseContext *respCtx, SceUInt8 useHttps)
{
	if (respCtx != NULL) {
		respCtx->httpStatusCode = 200;
		respCtx->reservedStatus = 0;
	}

	if (config->allocFunc == NULL || config->freeFunc == NULL || config->apiKey == NULL ||
	    config->appSecret == NULL || req == NULL || respCtx == NULL) {
		return NP_SNS_FB_POSIX_EINVAL;
	}

	SceSize workSize = (config->workBufSize != 0) ? config->workBufSize
						      : NP_SNS_FB_DEFAULT_WORK_SIZE;
	char *workBuf = static_cast<char *>(config->allocFunc(workSize));
	if (workBuf == NULL) {
		config->freeFunc(NULL);
		return NP_SNS_FB_POSIX_ENOMEM;
	}

	NpSnsFbHttpEndpoint endpoint;
	endpoint.scheme = (useHttps != 0) ? "https" : "http";
	endpoint.host = "api.facebook.com";
	endpoint.user = NULL;
	endpoint.password = NULL;
	endpoint.path = "/restserver.php";
	endpoint.port = (useHttps != 0) ? 443U : 80U;

	const char *headers[4] = {
		"Content-Type",
		"application/x-www-form-urlencoded",
		NULL,
		NULL
	};

	SceUInt64 nowMicros = 0;
	SceNpCommon_55F7FCF7(&nowMicros);
	char callIdStr[32];
	sceClibSnprintf(callIdStr, 31U, "%llu", nowMicros / 1000ULL);

	int ret = 0;
	if (baseParams == NULL) {
		ret = NP_SNS_FB_POSIX_EINVAL;
	} else {
		for (SceUInt32 idx = 0; idx < baseParams->count; ++idx) {
			if (sceClibStrncmp(baseParams->pairs[idx].key, "call_id", 0x400U) == 0) {
				baseParams->pairs[idx].value = callIdStr;
				break;
			}
		}

		if (overrideParams != NULL) {
			for (SceUInt32 ovIdx = 0; ovIdx < overrideParams->count; ++ovIdx) {
				for (SceUInt32 baseIdx = 0; baseIdx < baseParams->count; ++baseIdx) {
					if (sceClibStrncmp(baseParams->pairs[baseIdx].key,
							   overrideParams->pairs[ovIdx].key,
							   0x400U) == 0) {
						baseParams->pairs[baseIdx].value =
							overrideParams->pairs[ovIdx].value;
						break;
					}
				}
			}
		}

		const char *signingSecret = (session->sessionSecret[0] != '\0')
						    ? session->sessionSecret
						    : config->appSecret;
		char sigHex[36];
		ret = npSnsFbSignParamsMd5(baseParams, signingSecret, sigHex);
		if (ret == 0)
			ret = npSnsFbBuildFormBody(baseParams, sigHex, workBuf, workSize);
		if (ret == 0) {
			SceSize bodyLen = sceClibStrnlen(workBuf, 0x7FFFFFFFU);
			ret = npSnsFbHttpExecuteAndParse(config->httpTemplateId, req, &endpoint,
							 headers, "POST", workBuf, workSize,
							 bodyLen, respCtx);
		}
	}

	config->freeFunc(workBuf);

	sceKernelLockLwMutex(&req->mutexWork, 1, NULL);
	if (req->aborted != 0)
		ret = NP_SNS_FB_POSIX_ECANCELED;
	sceKernelUnlockLwMutex(&req->mutexWork, 1);

	return ret;
}

/* 0x813F9266 */
int npSnsFbLibCreateRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest **outReq)
{
	if (config == NULL || config->allocFunc == NULL || config->freeFunc == NULL ||
	    outReq == NULL) {
		return NP_SNS_FB_POSIX_EINVAL;
	}

	NpSnsFbLibRequest *req =
		static_cast<NpSnsFbLibRequest *>(config->allocFunc(sizeof(NpSnsFbLibRequest)));
	if (req == NULL) {
		*outReq = NULL;
		return NP_SNS_FB_POSIX_ENOMEM;
	}

	int ret = sceKernelCreateLwMutex(&req->mutexWork, "fblib", 2U, 0, NULL);
	if (ret != 0) {
		config->freeFunc(req);
		*outReq = NULL;
		return ret;
	}

	req->aborted = 0;
	req->requestActive = 0;
	req->httpConnId = 0;
	req->httpReqId = 0;
	*outReq = req;
	return 0;
}

/* 0x813F92C8 */
int npSnsFbLibDestroyRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest *req)
{
	if (config == NULL || config->freeFunc == NULL || req == NULL)
		return NP_SNS_FB_POSIX_EINVAL;

	int ret = sceKernelDeleteLwMutex(&req->mutexWork);
	if (ret == 0)
		config->freeFunc(req);
	return ret;
}

/* 0x813F92F2 */
int npSnsFbLibAbortRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest *req)
{
	if (config == NULL || req == NULL)
		return NP_SNS_FB_POSIX_EINVAL;

	int ret = sceKernelLockLwMutex(&req->mutexWork, 1, NULL);
	if (ret != 0)
		return ret;

	SceUInt8 wasActive = req->requestActive;
	req->aborted = 1;
	if (wasActive != 0)
		ret = sceHttpAbortRequest(req->httpReqId);
	sceKernelUnlockLwMutex(&req->mutexWork, 1);
	return ret;
}

/* ============================================================================
 * 4. bXCe SAX Handler Base & Auth Response Parser (0x813F9330..0x813F9827)
 * ============================================================================ */

/* 0x814330CC, 0x814330DC, 0x81409E2C, 0x814098DC */
BxceSaxHandler::~BxceSaxHandler()
{
}

int BxceSaxHandler::onStartDocument(void *userData)
{
	(void)userData;
	return 0;
}

int BxceSaxHandler::onStartElement(void *userData, const char *name, SceSize nameLen,
				   const void *attrs)
{
	(void)userData;
	(void)name;
	(void)nameLen;
	(void)attrs;
	return 0;
}

int BxceSaxHandler::onCharacters(void *userData, const char *text, SceSize textLen)
{
	(void)userData;
	(void)text;
	(void)textLen;
	return 0;
}

int BxceSaxHandler::onEndElement(void *userData, const char *name, SceSize nameLen)
{
	(void)userData;
	(void)name;
	(void)nameLen;
	return 0;
}

int BxceSaxHandler::onComment(void *userData, const char *text, SceSize textLen)
{
	(void)userData;
	(void)text;
	(void)textLen;
	return 0;
}

int BxceSaxHandler::onProcessingInstruction(void *userData, const char *target, SceSize targetLen,
					    const char *content, SceSize contentLen)
{
	(void)userData;
	(void)target;
	(void)targetLen;
	(void)content;
	(void)contentLen;
	return 0;
}

int BxceSaxHandler::onEntityReference(void *userData, int entityKind, const char *entityName,
				      SceSize entityLen)
{
	(void)userData;
	(void)entityKind;
	(void)entityName;
	(void)entityLen;
	return 0;
}

int BxceSaxHandler::onCdataSection(void *userData, const char *text, SceSize textLen)
{
	(void)userData;
	(void)text;
	(void)textLen;
	return 0;
}

void BxceSaxHandler::onEndDocument(void *userData)
{
	(void)userData;
}

int BxceSaxHandler::onXmlDeclaration(void *userData, const char *version, SceSize versionLen)
{
	(void)userData;
	(void)version;
	(void)versionLen;
	return 0;
}

/* Auth / getLoggedInUser XML tag table (0x8154950C) */
enum NpSnsFbAuthXmlTag {
	FB_AUTH_TAG_NONE			= 0,
	FB_AUTH_TAG_AUTH_LOGIN_RESPONSE		= 1,
	FB_AUTH_TAG_AUTH_EXPIRE_RESPONSE	= 2,
	FB_AUTH_TAG_GET_LOGGED_IN_USER_RESPONSE	= 3,
	FB_AUTH_TAG_SECRET			= 4,
	FB_AUTH_TAG_SESSION_KEY			= 5,
	FB_AUTH_TAG_UID				= 6,
	FB_AUTH_TAG_ERROR_RESPONSE		= 7,
	FB_AUTH_TAG_ERROR_CODE			= 8,
	FB_AUTH_TAG_ERROR_MSG			= 9,
	FB_AUTH_TAG_REQUEST_ARGS		= 10,
	FB_AUTH_TAG_ARG				= 11,
	FB_AUTH_TAG_KEY				= 12,
	FB_AUTH_TAG_VALUE			= 13,
	FB_AUTH_TAG_ACCESS_TOKEN		= 14
};

static const char *const s_authXmlTags[14] = {
	"auth_login_response",
	"auth_expireSession_response",
	"users_getLoggedInUser_response",
	"secret",
	"session_key",
	"uid",
	"error_response",
	"error_code",
	"error_msg",
	"request_args",
	"arg",
	"key",
	"value",
	"access_token"
};

/* User data for AuthResponseSaxHandler (32 bytes = 0x20) */
struct NpSnsFbAuthParseResult {
	NpSnsFbSession *session;				/* +0x00 */
	char loggedInUid[24];					/* +0x04 */
	SceInt32 fbErrorCode;					/* +0x1C */
};

/* Auth & getLoggedInUser SAX handler (vtable 0x815417D4, 268 bytes = 0x10C) */
class NpSnsFbAuthSaxHandler : public BxceSaxHandler {
public:
	/* 0x813F94A4 */
	NpSnsFbAuthSaxHandler()
		: m_currentTag(FB_AUTH_TAG_NONE), m_textLen(0)
	{
	}

	/* 0x813F94B8 */
	virtual int onStartDocument(void *userData)
	{
		return (userData != NULL) ? 0 : -1;
	}

	/* 0x813F94C4 */
	virtual int onStartElement(void *userData, const char *name, SceSize nameLen,
				   const void *attrs)
	{
		(void)userData;
		(void)attrs;
		m_textLen = 0;
		int matched = FB_AUTH_TAG_NONE;
		for (int idx = 0; idx < 14; ++idx) {
			if (sceClibStrncmp(name, s_authXmlTags[idx], nameLen) == 0) {
				matched = idx + 1;
				break;
			}
		}
		m_currentTag = matched;
		return 0;
	}

	/* 0x813F9500 */
	virtual int onCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		if (m_currentTag >= FB_AUTH_TAG_AUTH_EXPIRE_RESPONSE &&
		    m_currentTag <= FB_AUTH_TAG_ERROR_CODE) {
			if (m_textLen + static_cast<SceInt32>(textLen) >= NP_SNS_FB_AUTH_TEXT_BUF_MAX)
				return -1;
			sceClibMemcpy_safe(m_textBuf + m_textLen, text, textLen);
			m_textLen += static_cast<SceInt32>(textLen);
			m_textBuf[m_textLen] = '\0';
		}
		return 0;
	}

	/* 0x813F954A */
	virtual int onEndElement(void *userData, const char *name, SceSize nameLen)
	{
		if (userData == NULL)
			return -1;
		NpSnsFbAuthParseResult *result = static_cast<NpSnsFbAuthParseResult *>(userData);
		if (result->session == NULL)
			return -1;

		int matched = FB_AUTH_TAG_NONE;
		for (int idx = 0; idx < 14; ++idx) {
			if (sceClibStrncmp(name, s_authXmlTags[idx], nameLen) == 0) {
				matched = idx + 1;
				break;
			}
		}
		m_currentTag = matched;

		switch (matched) {
		case FB_AUTH_TAG_AUTH_EXPIRE_RESPONSE:
			if (SceNpCommon_CC8E6AC6(m_textBuf) == 0)
				result->fbErrorCode = 1;
			break;
		case FB_AUTH_TAG_GET_LOGGED_IN_USER_RESPONSE:
			if (m_textLen <= 20) {
				sceClibMemcpy_safe(result->loggedInUid, m_textBuf, m_textLen);
				result->loggedInUid[m_textLen] = '\0';
			}
			break;
		case FB_AUTH_TAG_SECRET:
			if (m_textLen <= 32) {
				sceClibMemcpy_safe(result->session->sessionSecret, m_textBuf,
						   m_textLen);
				result->session->sessionSecret[m_textLen] = '\0';
			}
			break;
		case FB_AUTH_TAG_SESSION_KEY:
			if (m_textLen <= 150) {
				sceClibMemcpy_safe(result->session->sessionKey, m_textBuf,
						   m_textLen);
				result->session->sessionKey[m_textLen] = '\0';
			}
			break;
		case FB_AUTH_TAG_UID:
			if (m_textLen <= 20) {
				sceClibMemcpy_safe(result->session->uid, m_textBuf, m_textLen);
				result->session->uid[m_textLen] = '\0';
			}
			break;
		case FB_AUTH_TAG_ERROR_CODE:
			result->fbErrorCode = SceNpCommon_CC8E6AC6(m_textBuf);
			break;
		case FB_AUTH_TAG_ACCESS_TOKEN:
			result->session->accessToken[0] = '\0';
			break;
		default:
			break;
		}

		m_textLen = 0;
		return 0;
	}

private:
	SceInt32 m_currentTag;					/* +0x004 */
	SceInt32 m_textLen;					/* +0x008 */
	char m_textBuf[NP_SNS_FB_AUTH_TEXT_BUF_MAX];		/* +0x00C */
};

/* 0x813F9658 */
static int npSnsFbInvokeAuthLogin(const NpSnsFbLibConfig *config, NpSnsFbSession *outSession,
				  NpSnsFbLibRequest *req, const NpSnsFbParamList *credentials,
				  NpSnsFbHttpResponseContext *respCtx)
{
	NpSnsFbParamPair pairs[5] = {
		{ "api_key",	(config->apiKey != NULL) ? config->apiKey : NULL },
		{ "email",	NULL },
		{ "method",	"facebook.auth.login" },
		{ "password",	NULL },
		{ "v",		"1.0" }
	};
	NpSnsFbParamList baseParams = { 5U, pairs };
	sceClibMemset(outSession, 0, sizeof(NpSnsFbSession));
	return npSnsFbInvokeRestMethod(config, outSession, req, &baseParams, credentials,
				       respCtx, 1);
}

/* 0x813F9716 */
static int npSnsFbInvokeGrantExtendedPermission(const NpSnsFbLibConfig *config,
						const NpSnsFbSession *session,
						NpSnsFbLibRequest *req,
						const NpSnsFbParamList *permParams,
						NpSnsFbHttpResponseContext *respCtx)
{
	NpSnsFbParamPair pairs[5] = {
		{ "api_key",		(config->apiKey != NULL) ? config->apiKey : NULL },
		{ "method",		"facebook.auth.grantExtendedPermission" },
		{ "perm",		NULL },
		{ "session_key",	(session != NULL) ? session->sessionKey : NULL },
		{ "v",			"1.0" }
	};
	NpSnsFbParamList baseParams = { 5U, pairs };
	return npSnsFbInvokeRestMethod(config, session, req, &baseParams, permParams, respCtx, 1);
}

/* 0x813F97B8 */
static int npSnsFbInvokeGetLoggedInUser(const NpSnsFbLibConfig *config,
					const NpSnsFbSession *session, NpSnsFbLibRequest *req,
					const NpSnsFbParamList *extraParams,
					NpSnsFbHttpResponseContext *respCtx)
{
	if (config == NULL)
		return NP_SNS_FB_POSIX_EINVAL;

	NpSnsFbParamPair pairs[6] = {
		{ "api_key",		config->apiKey },
		{ "call_id",		NULL },
		{ "fields",		NULL },
		{ "method",		"facebook.users.getLoggedInUser" },
		{ "session_key",	session->sessionKey },
		{ "v",			"1.0" }
	};
	NpSnsFbParamList baseParams = { 6U, pairs };
	return npSnsFbInvokeRestMethod(config, session, req, &baseParams, extraParams,
				       respCtx, 1);
}

/* 0x813F9330 */
int npSnsFbLibAuthLogin(const NpSnsFbLibConfig *config, NpSnsFbSession *outSession,
			NpSnsFbLibRequest *req, const char *email, const char *password,
			SceInt32 *outFbError)
{
	NpSnsFbParamPair credPairs[2] = {
		{ "email",	email },
		{ "password",	password }
	};
	NpSnsFbParamList credParams = { 2U, credPairs };

	NpSnsFbAuthSaxHandler saxHandler;
	NpSnsFbAuthParseResult parseResult;
	parseResult.session = outSession;
	parseResult.fbErrorCode = 0;

	NpSnsFbHttpResponseContext respCtx;
	sceClibMemset(&respCtx, 0, sizeof(respCtx));
	respCtx.saxHandler = &saxHandler;
	respCtx.saxUserData = &parseResult;

	if (outFbError != NULL)
		*outFbError = 0;

	if (email == NULL || password == NULL)
		return NP_SNS_FB_POSIX_EINVAL;

	int ret = npSnsFbInvokeAuthLogin(config, outSession, req, &credParams, &respCtx);
	if (ret != 0)
		sceClibMemset(outSession, 0, sizeof(NpSnsFbSession));

	if (parseResult.fbErrorCode != 0) {
		ret = -1;
		if (outFbError != NULL)
			*outFbError = (parseResult.fbErrorCode == 1) ? 401
								     : parseResult.fbErrorCode;
	}
	return ret;
}

/* 0x813F9404 */
int npSnsFbLibGetLoggedInUser(const NpSnsFbLibConfig *config, const NpSnsFbSession *session,
			      NpSnsFbLibRequest *req, char *outUid, SceInt32 *outFbError)
{
	NpSnsFbAuthSaxHandler saxHandler;
	NpSnsFbAuthParseResult parseResult;
	parseResult.session = const_cast<NpSnsFbSession *>(session);
	sceClibMemset(parseResult.loggedInUid, 0, sizeof(parseResult.loggedInUid));
	parseResult.fbErrorCode = 0;

	NpSnsFbHttpResponseContext respCtx;
	sceClibMemset(&respCtx, 0, sizeof(respCtx));
	respCtx.saxHandler = &saxHandler;
	respCtx.saxUserData = &parseResult;

	if (outFbError != NULL)
		*outFbError = 0;
	sceClibMemset(outUid, 0, NP_SNS_FB_UID_MAX);

	int ret = npSnsFbInvokeGetLoggedInUser(config, session, req, NULL, &respCtx);
	if (ret == 0) {
		if (parseResult.fbErrorCode != 0)
			ret = -1;
		if (outFbError != NULL)
			*outFbError = parseResult.fbErrorCode;
		sceClibMemcpy_safe(outUid, parseResult.loggedInUid, NP_SNS_FB_UID_MAX);
	}
	return ret;
}

/* ============================================================================
 * 5. Error-Aware SAX Base & Derived Parsers (0x813F9828..0x813FA10B, 0x81455EAC..0x81456740)
 * ============================================================================ */

enum NpSnsFbErrorTagState {
	FB_ERR_STATE_NONE	= 0,
	FB_ERR_STATE_ROOT	= 1,
	FB_ERR_STATE_CODE	= 2,
	FB_ERR_STATE_MSG	= 3
};

/* Base SAX handler that intercepts <error_response> (vtable 0x81541960, 36 bytes = 0x24) */
class NpSnsFbSaxHandlerBase : public BxceSaxHandler {
public:
	/* 0x813F9D30 */
	explicit NpSnsFbSaxHandlerBase(NpSnsFbErrorInfo *errDetail)
		: m_lastError(0), m_inErrorResponse(0), m_errState(FB_ERR_STATE_NONE),
		  m_errDetail(errDetail)
	{
		if (m_errDetail != NULL) {
			m_errDetail->errorCode = 0;
			if (m_errDetail->errorMsgBuf != NULL)
				sceClibMemset(m_errDetail->errorMsgBuf, 0,
					      m_errDetail->errorMsgBufSize);
			m_errDetail->truncated = 0;
		}
	}

	/* 0x813F9D88, 0x813F9DB4 */
	virtual ~NpSnsFbSaxHandlerBase()
	{
	}

	/* 0x813F9DFA */
	virtual int onStartElement(void *userData, const char *name, SceSize nameLen,
				   const void *attrs)
	{
		if (m_inErrorResponse != 0) {
			if (nameLen == 10U && sceClibStrncmp(name, "error_code", 10U) == 0) {
				m_textBuf.reset();
				m_errState = FB_ERR_STATE_CODE;
			} else if (nameLen == 9U && sceClibStrncmp(name, "error_msg", 9U) == 0) {
				m_textBuf.reset();
				m_errState = FB_ERR_STATE_MSG;
			}
			return 0;
		}

		if (nameLen == 14U && sceClibStrncmp(name, "error_response", 14U) == 0) {
			m_inErrorResponse = 1;
			m_errState = FB_ERR_STATE_ROOT;
			return 0;
		}

		int ret = handleStartElement(userData, name, nameLen, attrs);
		if (ret < 0)
			m_lastError = ret;
		return ret;
	}

	/* 0x813F9F0C */
	virtual int onCharacters(void *userData, const char *text, SceSize textLen)
	{
		int ret = 0;
		if (m_inErrorResponse != 0) {
			if (m_errState == FB_ERR_STATE_CODE || m_errState == FB_ERR_STATE_MSG) {
				ret = m_textBuf.append(text, textLen);
				if (ret >= 0)
					return 0;
			} else {
				return 0;
			}
		} else {
			ret = handleCharacters(userData, text, textLen);
			if (ret >= 0)
				return ret;
		}
		m_lastError = ret;
		return ret;
	}

	/* 0x813FA02A */
	virtual int onEndElement(void *userData, const char *name, SceSize nameLen)
	{
		if (m_inErrorResponse != 0) {
			if (m_errState == FB_ERR_STATE_CODE) {
				m_errState = FB_ERR_STATE_ROOT;
				if (m_errDetail != NULL)
					m_errDetail->errorCode =
						SceNpCommon_CC8E6AC6(m_textBuf.cStr());
				m_textBuf.reset();
			} else if (m_errState == FB_ERR_STATE_MSG) {
				m_errState = FB_ERR_STATE_ROOT;
				if (m_errDetail != NULL && m_errDetail->errorMsgBuf != NULL) {
					const char *msg = m_textBuf.cStr();
					sceClibStrncpy(m_errDetail->errorMsgBuf, msg,
						       m_errDetail->errorMsgBufSize - 1U);
					m_errDetail->errorMsgBuf[m_errDetail->errorMsgBufSize - 1U] =
						'\0';
					m_errDetail->truncated =
						(m_textBuf.length >= m_errDetail->errorMsgBufSize)
							? 1
							: 0;
				}
				m_textBuf.reset();
			}
			return 0;
		}

		int ret = handleEndElement(userData, name, nameLen);
		if (ret < 0)
			m_lastError = ret;
		return ret;
	}

	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs) = 0;
	virtual int handleCharacters(void *userData, const char *text, SceSize textLen) = 0;
	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen) = 0;

	SceInt32 lastError() const { return m_lastError; }
	SceUInt8 hasErrorResponse() const { return m_inErrorResponse; }

protected:
	SceInt32 m_lastError;					/* +0x04 */
	SceUInt8 m_inErrorResponse;				/* +0x08 */
	SceUInt8 m_errFlagPad[3];				/* +0x09 */
	SceInt32 m_errState;					/* +0x0C */
	SceInt32 m_subStateUnused;				/* +0x10 */
	NpSnsFbErrorInfo *m_errDetail;				/* +0x14 */
	NpSnsFbTextBuffer m_textBuf;				/* +0x18 */
};

/* FQL permissions_info SAX parser (vtable 0x8154180C, 0x81455EAC..0x81456347) */
class NpSnsFbPermissionsInfoSaxHandler : public NpSnsFbSaxHandlerBase {
public:
	explicit NpSnsFbPermissionsInfoSaxHandler(NpSnsFbErrorInfo *errDetail)
		: NpSnsFbSaxHandlerBase(errDetail), m_parseState(0)
	{
	}

	/* 0x81455EAC, 0x81455ED8 */
	virtual ~NpSnsFbPermissionsInfoSaxHandler()
	{
	}

	/* 0x81456024 */
	virtual int onEntityReference(void *userData, int entityKind, const char *entityName,
				      SceSize entityLen)
	{
		switch (entityKind) {
		case 0:
			handleCharacters(userData, "&", 1U);
			break;
		case 1:
			handleCharacters(userData, "\"", 1U);
			break;
		case 2:
			handleCharacters(userData, "<", 1U);
			break;
		case 3:
			handleCharacters(userData, ">", 1U);
			break;
		case 4:
			handleCharacters(userData, "'", 1U);
			break;
		case 6:
			handleCharacters(userData, entityName, entityLen);
			break;
		default:
			handleCharacters(userData, "&", 1U);
			handleCharacters(userData, entityName, entityLen);
			handleCharacters(userData, ";", 1U);
			break;
		}
		return 0;
	}

	/* 0x81455F08 */
	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs)
	{
		(void)userData;
		(void)attrs;
		if (nameLen == 18U && sceClibStrncmp(name, "fql_query_response", 18U) == 0) {
			if (m_parseState == 0)
				m_parseState = 1;
		} else if (nameLen == 16U && sceClibStrncmp(name, "permissions_info", 16U) == 0) {
			if (m_parseState == 1)
				m_parseState = 2;
		} else if (nameLen == 15U && sceClibStrncmp(name, "permission_name", 15U) == 0) {
			if (m_parseState == 2) {
				m_permNameBuf.reset();
				m_parseState = 3;
			}
		} else if (nameLen == 6U && sceClibStrncmp(name, "header", 6U) == 0) {
			if (m_parseState == 2) {
				m_headerBuf.reset();
				m_parseState = 4;
			}
		} else if (nameLen == 7U && sceClibStrncmp(name, "summary", 7U) == 0) {
			if (m_parseState == 2) {
				m_textBuf.reset();
				m_parseState = 5;
			}
		}
		return 0;
	}

	/* 0x81455FE0 */
	virtual int handleCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		int ret = 0;
		if (m_parseState == 3)
			ret = m_permNameBuf.append(text, textLen);
		else if (m_parseState == 4)
			ret = m_headerBuf.append(text, textLen);
		else if (m_parseState == 5)
			ret = m_textBuf.append(text, textLen);
		return (ret >= 0) ? 0 : ret;
	}

	/* 0x814560E8 */
	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen)
	{
		NpSnsFbPermissionMap *permMap = *static_cast<NpSnsFbPermissionMap **>(userData);
		switch (m_parseState) {
		case 1:
			if (nameLen == 18U && sceClibStrncmp(name, "fql_query_response", 18U) == 0)
				m_parseState = 0;
			break;
		case 2:
			if (nameLen == 16U && sceClibStrncmp(name, "permissions_info", 16U) == 0) {
				NpSnsFbPermissionValue *entry =
					permMap->findOrInsert(m_permNameBuf.cStr());
				sceClibStrncpy(entry->header, m_headerBuf.cStr(), 0xFFU);
				sceClibStrncpy(entry->summary, m_textBuf.cStr(), 0xFFU);
				m_parseState = 1;
			}
			break;
		case 3:
			if (nameLen == 15U && sceClibStrncmp(name, "permission_name", 15U) == 0)
				m_parseState = 2;
			break;
		case 4:
			if (nameLen == 6U && sceClibStrncmp(name, "header", 6U) == 0)
				m_parseState = 2;
			break;
		case 5:
			if (nameLen == 7U && sceClibStrncmp(name, "summary", 7U) == 0)
				m_parseState = 2;
			break;
		default:
			break;
		}
		return 0;
	}

private:
	SceInt32 m_parseState;					/* +0x24 */
	NpSnsFbTextBuffer m_permNameBuf;			/* +0x28 */
	NpSnsFbTextBuffer m_headerBuf;				/* +0x34 */
};

/* FQL permissions check SAX parser (vtable 0x815418BC, 0x81456348..0x8145652B) */
class NpSnsFbCheckPermissionsSaxHandler : public NpSnsFbSaxHandlerBase {
public:
	explicit NpSnsFbCheckPermissionsSaxHandler(NpSnsFbErrorInfo *errDetail)
		: NpSnsFbSaxHandlerBase(errDetail), m_parseState(0)
	{
	}

	/* 0x81456348, 0x8145636C */
	virtual ~NpSnsFbCheckPermissionsSaxHandler()
	{
	}

	/* 0x81456394 */
	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs)
	{
		(void)userData;
		(void)attrs;
		if (nameLen == 18U && sceClibStrncmp(name, "fql_query_response", 18U) == 0) {
			if (m_parseState == 0)
				m_parseState = 1;
		} else if (nameLen == 11U && sceClibStrncmp(name, "permissions", 11U) == 0) {
			if (m_parseState == 1)
				m_parseState = 2;
		} else if (m_parseState == 2) {
			m_permNameBuf.reset();
			m_permNameBuf.append(name, nameLen);
			m_textBuf.reset();
			m_parseState = 3;
		}
		return 0;
	}

	/* 0x81456414 */
	virtual int handleCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		if (m_parseState != 3)
			return 0;
		int ret = m_textBuf.append(text, textLen);
		return (ret >= 0) ? 0 : ret;
	}

	/* 0x81456430 */
	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen)
	{
		(void)name;
		(void)nameLen;
		if (m_parseState == 1) {
			m_parseState = 0;
		} else if (m_parseState == 2) {
			m_parseState = 1;
		} else if (m_parseState == 3) {
			m_parseState = 2;
			int grantedInt = SceNpCommon_CC8E6AC6(m_textBuf.cStr());
			NpSnsFbPermissionMap *permMap =
				*static_cast<NpSnsFbPermissionMap **>(userData);
			NpSnsFbPermissionValue *entry =
				permMap->findOrInsert(m_permNameBuf.cStr());
			entry->granted = (grantedInt != 0) ? 1 : 0;
		}
		return 0;
	}

private:
	SceInt32 m_parseState;					/* +0x24 */
	NpSnsFbTextBuffer m_permNameBuf;			/* +0x28 */
};

/* Access-token proxy.authorize XML buffer (4112 bytes = 0x1010) */
struct NpSnsFbAccessTokenParseBuf {
	char accessToken[SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX + 1]; /* +0x0000 */
	SceUInt8 pad[7];					/* +0x1001 */
	SceUInt64 expires;					/* +0x1008 */
};

/* proxy.authorize access_token/expires SAX parser (vtable 0x81541910, 0x8145652C..0x8145666B) */
class NpSnsFbAccessTokenSaxHandler : public NpSnsFbSaxHandlerBase {
public:
	explicit NpSnsFbAccessTokenSaxHandler(NpSnsFbErrorInfo *errDetail)
		: NpSnsFbSaxHandlerBase(errDetail), m_parseState(0)
	{
		m_textBuf.capacity = NP_SNS_FB_TOKEN_TEXT_CAP;
	}

	/* 0x8145652C, 0x81456548 */
	virtual ~NpSnsFbAccessTokenSaxHandler()
	{
	}

	/* 0x81456568 */
	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs)
	{
		(void)userData;
		(void)attrs;
		if (nameLen == 12U && sceClibStrncmp(name, "access_token", 12U) == 0) {
			m_textBuf.reset();
			m_parseState = 3;
		} else if (nameLen == 7U && sceClibStrncmp(name, "expires", 7U) == 0) {
			m_textBuf.reset();
			m_parseState = 4;
		}
		return 0;
	}

	/* 0x814565C4 */
	virtual int handleCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		if (m_parseState != 3 && m_parseState != 4)
			return 0;
		int ret = m_textBuf.append(text, textLen);
		return (ret >= 0) ? 0 : ret;
	}

	/* 0x814565E4 */
	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen)
	{
		(void)name;
		(void)nameLen;
		NpSnsFbAccessTokenParseBuf *out =
			static_cast<NpSnsFbAccessTokenParseBuf *>(userData);
		if (m_parseState == 3) {
			m_parseState = 0;
			sceClibStrncpy(out->accessToken, m_textBuf.cStr(),
				       NP_SNS_FB_TOKEN_TEXT_CAP);
		} else if (m_parseState == 4) {
			m_parseState = 0;
			const char *digits = m_textBuf.cStr();
			SceUInt64 parsedVal = 0;
			while ((sceClibLookCtypeTable(*digits) & 4) != 0) {
				parsedVal = parsedVal * 10ULL +
					    static_cast<SceUInt64>(*digits - '0');
				++digits;
			}
			out->expires = parsedVal;
		}
		return 0;
	}

private:
	SceInt32 m_parseState;					/* +0x24 */
};

/* auth.grantExtendedPermission SAX parser (vtable 0x815419A4, 0x8145666C..0x8145670F) */
class NpSnsFbGrantPermissionSaxHandler : public NpSnsFbSaxHandlerBase {
public:
	explicit NpSnsFbGrantPermissionSaxHandler(NpSnsFbErrorInfo *errDetail)
		: NpSnsFbSaxHandlerBase(errDetail), m_parseState(0)
	{
	}

	/* 0x8145666C, 0x81456688 */
	virtual ~NpSnsFbGrantPermissionSaxHandler()
	{
	}

	/* 0x814566A8 */
	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs)
	{
		(void)userData;
		(void)attrs;
		if (nameLen == 37U &&
		    sceClibStrncmp(name, "auth_grantExtendedPermission_response", 37U) == 0) {
			m_textBuf.reset();
			m_parseState = 1;
		}
		return 0;
	}

	/* 0x814566D4 */
	virtual int handleCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		if (m_parseState != 1)
			return 0;
		int ret = m_textBuf.append(text, textLen);
		return (ret >= 0) ? 0 : ret;
	}

	/* 0x814566F0 */
	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen)
	{
		(void)name;
		(void)nameLen;
		if (m_parseState == 1) {
			m_parseState = 0;
			*static_cast<SceInt32 *>(userData) = SceNpCommon_CC8E6AC6(m_textBuf.cStr());
		}
		return 0;
	}

private:
	SceInt32 m_parseState;					/* +0x24 */
};

/* stream.publish SAX parser (vtable 0x81541A10, 0x81456710..0x81456740) */
class NpSnsFbStreamPublishSaxHandler : public NpSnsFbSaxHandlerBase {
public:
	explicit NpSnsFbStreamPublishSaxHandler(NpSnsFbErrorInfo *errDetail)
		: NpSnsFbSaxHandlerBase(errDetail), m_parseState(0), m_postIdLen(-1)
	{
	}

	/* 0x81456710, 0x8145672C */
	virtual ~NpSnsFbStreamPublishSaxHandler()
	{
	}

	virtual int handleStartElement(void *userData, const char *name, SceSize nameLen,
				       const void *attrs)
	{
		(void)userData;
		(void)name;
		(void)nameLen;
		(void)attrs;
		return 0;
	}

	virtual int handleCharacters(void *userData, const char *text, SceSize textLen)
	{
		(void)userData;
		(void)text;
		(void)textLen;
		return 0;
	}

	virtual int handleEndElement(void *userData, const char *name, SceSize nameLen)
	{
		(void)userData;
		(void)name;
		(void)nameLen;
		return 0;
	}

private:
	SceInt32 m_parseState;					/* +0x24 */
	SceInt32 m_postIdLen;					/* +0x28 */
};

/* 0x813F9828 */
int npSnsFbLibParsePermissionsInfoResponse(SceInt32 httpConnId, SceInt32 httpReqId,
					   NpSnsFbPermissionMap *permMap,
					   NpSnsFbErrorInfo *errDetail)
{
	(void)httpConnId;
	NpSnsFbPermissionsInfoSaxHandler handler(errDetail);
	NpSnsFbPermissionMap *mapHolder = permMap;

	char *readBuf = static_cast<char *>(npSnsFbMemAlloc(NP_SNS_FB_SAX_READ_CHUNK));
	if (readBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	int ret = npSnsFbParseHttpStreamSax(httpReqId, readBuf, NP_SNS_FB_SAX_READ_CHUNK,
					    &handler, &mapHolder);
	if (ret >= 0 && handler.hasErrorResponse() != 0)
		ret = NP_SNS_FB_ERR_API_ERROR;

	npSnsFbMemFree(readBuf);
	return ret;
}

/* 0x813F99C4 */
int npSnsFbLibParseCheckPermissionsResponse(SceInt32 httpConnId, SceInt32 httpReqId,
					    NpSnsFbPermissionMap *permMap,
					    NpSnsFbErrorInfo *errDetail)
{
	(void)httpConnId;
	NpSnsFbCheckPermissionsSaxHandler handler(errDetail);
	NpSnsFbPermissionMap *mapHolder = permMap;

	char *readBuf = static_cast<char *>(npSnsFbMemAlloc(NP_SNS_FB_SAX_READ_CHUNK));
	if (readBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	int ret = npSnsFbParseHttpStreamSax(httpReqId, readBuf, NP_SNS_FB_SAX_READ_CHUNK,
					    &handler, &mapHolder);
	if (ret >= 0 && handler.hasErrorResponse() != 0)
		ret = NP_SNS_FB_ERR_API_ERROR;

	npSnsFbMemFree(readBuf);
	return ret;
}

/* 0x813F9B54 */
int npSnsFbLibParseAccessTokenResponse(SceInt32 httpConnId, SceInt32 httpReqId,
				       char *outAccessToken, SceUInt64 *outExpires,
				       NpSnsFbErrorInfo *errDetail)
{
	(void)httpConnId;
	NpSnsFbAccessTokenSaxHandler handler(errDetail);

	NpSnsFbAccessTokenParseBuf *parseBuf =
		static_cast<NpSnsFbAccessTokenParseBuf *>(
			npSnsFbMemAlloc(sizeof(NpSnsFbAccessTokenParseBuf)));
	if (parseBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	char *readBuf = static_cast<char *>(npSnsFbMemAlloc(NP_SNS_FB_SAX_READ_CHUNK));
	if (readBuf == NULL) {
		npSnsFbMemFree(parseBuf);
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;
	}

	int ret = npSnsFbParseHttpStreamSax(httpReqId, readBuf, NP_SNS_FB_SAX_READ_CHUNK,
					    &handler, parseBuf);
	if (ret >= 0) {
		if (handler.hasErrorResponse() != 0) {
			ret = NP_SNS_FB_ERR_API_ERROR;
		} else {
			sceClibStrncpy(outAccessToken, parseBuf->accessToken,
				       SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX);
			*outExpires = parseBuf->expires;
		}
	} else if (handler.lastError() < 0) {
		ret = handler.lastError();
	}

	npSnsFbMemFree(readBuf);
	npSnsFbMemFree(parseBuf);
	return ret;
}

/* ============================================================================
 * 6. Stream Publish Client (0x813FA10C..0x813FA7F7)
 * ============================================================================ */

/* 0x813FA10C */
int npSnsFbStreamInit(const NpSnsFbStreamInitParam *param)
{
	if (s_streamState.initialized != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	s_streamState.httpTemplateId = param->httpTemplateId;
	s_streamState.allocFunc = param->allocFunc;
	s_streamState.freeFunc = param->freeFunc;
	s_streamState.initialized = 1;
	return 0;
}

/* 0x813FA13C */
int npSnsFbStreamTerm(void)
{
	if (s_streamState.initialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	sceClibMemset(&s_streamState, 0, sizeof(s_streamState));
	return 0;
}

/* 0x813FA16E */
static int npSnsFbStreamCheckInitialized(void)
{
	return (s_streamState.initialized != 0) ? 0 : NP_SNS_FB_ERR_NOT_INITIALIZED;
}

/* 0x813FA188 */
static void npSnsFbStreamBuildLibConfig(NpSnsFbLibConfig *outCfg,
					const NpSnsFbStreamAppConfig *appCfg)
{
	sceClibMemset(outCfg, 0, sizeof(NpSnsFbLibConfig));
	outCfg->httpTemplateId = s_streamState.httpTemplateId;
	outCfg->allocFunc = s_streamState.allocFunc;
	outCfg->freeFunc = s_streamState.freeFunc;

	if (appCfg != NULL) {
		outCfg->apiKey = appCfg->apiKey;
		outCfg->appSecret = appCfg->appSecret;
		outCfg->workBufSize = appCfg->workBufSize;
		if (appCfg->httpTemplateId != 0)
			outCfg->httpTemplateId = appCfg->httpTemplateId;
	}
}

/* 0x813FA1C2 */
static void npSnsFbStreamCopySession(NpSnsFbSession *dst, const NpSnsFbSession *src)
{
	sceClibMemset(dst, 0, sizeof(NpSnsFbSession));
	sceClibStrncpy(dst->uid, src->uid, 20U);
	sceClibStrncpy(dst->sessionKey, src->sessionKey, 150U);
	sceClibStrncpy(dst->sessionSecret, src->sessionSecret, 32U);
}

/* 0x813FA1FC */
int npSnsFbStreamCreateRequest(NpSnsFbStreamRequest **outReq)
{
	int ret = npSnsFbStreamCheckInitialized();
	if (ret < 0)
		return ret;

	NpSnsFbStreamRequest *req =
		static_cast<NpSnsFbStreamRequest *>(npSnsFbMemAlloc(sizeof(NpSnsFbStreamRequest)));
	if (req == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	sceClibMemset(req, 0, sizeof(NpSnsFbStreamRequest));
	ret = sceKernelCreateLwMutex(&req->mutexWork, "fblib", 2U, 0, NULL);
	if (ret < 0) {
		npSnsFbMemFree(req);
		return ret;
	}

	req->aborted = 0;
	*outReq = req;
	return 0;
}

/* 0x813FA260 */
int npSnsFbStreamDestroyRequest(NpSnsFbStreamRequest *req)
{
	int ret = npSnsFbStreamCheckInitialized();
	if (ret < 0)
		return ret;

	if (req->activeLibReq != NULL) {
		NpSnsFbLibConfig libCfg;
		npSnsFbStreamBuildLibConfig(&libCfg, NULL);
		npSnsFbLibDestroyRequest(&libCfg, req->activeLibReq);
	}
	sceKernelDeleteLwMutex(&req->mutexWork);
	npSnsFbMemFree(req);
	return 0;
}

/* 0x813FA2B4 */
int npSnsFbStreamAbortRequest(NpSnsFbStreamRequest *req)
{
	int ret = npSnsFbStreamCheckInitialized();
	if (ret < 0)
		return ret;

	ret = sceKernelLockLwMutex(&req->mutexWork, 1, NULL);
	if (ret < 0)
		return ret;

	if (req->aborted == 0) {
		NpSnsFbLibConfig libCfg;
		npSnsFbStreamBuildLibConfig(&libCfg, NULL);
		if (req->activeLibReq != NULL)
			npSnsFbLibAbortRequest(&libCfg, req->activeLibReq);
		req->aborted = 1;
	}

	sceKernelUnlockLwMutex(&req->mutexWork, 1);
	return 0;
}

/* 0x813FA320 */
static int npSnsFbStreamResetLibRequest(NpSnsFbStreamRequest *req, NpSnsFbLibRequest **outLibReq)
{
	int ret = npSnsFbStreamCheckInitialized();
	if (ret < 0)
		return ret;

	ret = sceKernelLockLwMutex(&req->mutexWork, 1, NULL);
	if (ret < 0)
		return ret;

	if (req->aborted != 0) {
		ret = NP_SNS_FB_ERR_ABORTED;
	} else {
		NpSnsFbLibConfig libCfg;
		npSnsFbStreamBuildLibConfig(&libCfg, NULL);
		if (req->activeLibReq != NULL) {
			npSnsFbLibDestroyRequest(&libCfg, req->activeLibReq);
			req->activeLibReq = NULL;
		}
		ret = npSnsFbLibCreateRequest(&libCfg, &req->activeLibReq);
		if (ret >= 0) {
			*outLibReq = req->activeLibReq;
			ret = 0;
		} else {
			req->activeLibReq = NULL;
		}
	}

	sceKernelUnlockLwMutex(&req->mutexWork, 1);
	return ret;
}

/* 0x813FA640 */
static int npSnsFbStreamGrantExtendedPermission(const NpSnsFbLibConfig *libCfg,
						const NpSnsFbSession *session,
						NpSnsFbLibRequest *libReq,
						const NpSnsFbParamList *permParams,
						NpSnsFbErrorInfo *errDetail)
{
	SceInt32 grantResult = -1;
	NpSnsFbGrantPermissionSaxHandler handler(errDetail);

	NpSnsFbHttpResponseContext respCtx;
	sceClibMemset(&respCtx, 0, sizeof(respCtx));
	respCtx.saxHandler = &handler;
	respCtx.saxUserData = &grantResult;

	int ret = npSnsFbInvokeGrantExtendedPermission(libCfg, session, libReq, permParams,
						       &respCtx);
	if (ret >= 0)
		return (handler.hasErrorResponse() != 0) ? NP_SNS_FB_ERR_API_ERROR : 0;

	if (handler.lastError() < 0)
		ret = handler.lastError();
	return ret;
}

/* 0x813FA6FC */
static int npSnsFbStreamExecutePublish(const NpSnsFbLibConfig *libCfg,
				       const NpSnsFbSession *session, NpSnsFbLibRequest *libReq,
				       const NpSnsFbParamList *publishParams, char *outPostIdBuf,
				       SceSize outPostIdSize, NpSnsFbErrorInfo *errDetail)
{
	struct PublishResultHolder {
		char *postIdBuf;
		SceSize postIdSize;
	};
	PublishResultHolder resultHolder = { outPostIdBuf, outPostIdSize };

	NpSnsFbStreamPublishSaxHandler handler(errDetail);
	NpSnsFbHttpResponseContext respCtx;
	sceClibMemset(&respCtx, 0, sizeof(respCtx));
	respCtx.saxHandler = &handler;
	respCtx.saxUserData = &resultHolder;

	NpSnsFbParamPair pairs[9] = {
		{ "action_links",	NULL },
		{ "api_key",		(libCfg->apiKey != NULL) ? libCfg->apiKey : NULL },
		{ "attachment",		NULL },
		{ "message",		NULL },
		{ "method",		"facebook.stream.publish" },
		{ "session_key",	(session != NULL) ? session->sessionKey : NULL },
		{ "target_id",		NULL },
		{ "uid",		NULL },
		{ "v",			"1.0" }
	};
	NpSnsFbParamList baseParams = { 9U, pairs };

	int ret = npSnsFbInvokeRestMethod(libCfg, session, libReq, &baseParams, publishParams,
					  &respCtx, 1);
	if (ret >= 0)
		return (handler.hasErrorResponse() != 0) ? NP_SNS_FB_ERR_API_ERROR : 0;

	if (handler.lastError() < 0)
		ret = handler.lastError();
	return ret;
}

/* 0x813FA3B8 */
static int npSnsFbStreamPublishEx(NpSnsFbStreamRequest *req, const NpSnsFbStreamAppConfig *appCfg,
				  const NpSnsFbSession *session, const char *uid,
				  const char *targetId, const char *privacy, const char *message,
				  const char *attachment, const char *actionLinks,
				  char *outPostIdBuf, SceSize outPostIdSize,
				  NpSnsFbErrorInfo *outErr)
{
	(void)uid;
	(void)targetId;
	(void)privacy;

	int ret = npSnsFbStreamCheckInitialized();
	if (ret < 0)
		return ret;

	NpSnsFbErrorInfo localErr;
	sceClibMemset(&localErr, 0, sizeof(localErr));
	if (outErr != NULL) {
		localErr.errorMsgBuf = outErr->errorMsgBuf;
		localErr.errorMsgBufSize = outErr->errorMsgBufSize;
	}

	NpSnsFbParamPair publishPairs[3] = {
		{ "message",		message },
		{ "attachment",		attachment },
		{ "action_links",	actionLinks }
	};
	NpSnsFbParamList publishParams = { 3U, publishPairs };

	NpSnsFbLibConfig libCfg;
	NpSnsFbSession sessionCopy;
	NpSnsFbLibRequest *libReq = NULL;
	npSnsFbStreamBuildLibConfig(&libCfg, appCfg);
	npSnsFbStreamCopySession(&sessionCopy, session);

	ret = npSnsFbStreamResetLibRequest(req, &libReq);
	if (ret >= 0) {
		int pubRet = npSnsFbStreamExecutePublish(&libCfg, &sessionCopy, libReq,
							 &publishParams, outPostIdBuf,
							 outPostIdSize, &localErr);
		ret = (pubRet < 0) ? pubRet : 0;
	}

	/* If Facebook returns error 200 (permissions error), request publish_stream and retry */
	if (ret == NP_SNS_FB_ERR_API_ERROR && localErr.errorCode == 200) {
		NpSnsFbParamPair grantPair = { "perm", "publish_stream" };
		NpSnsFbParamList grantParams = { 1U, &grantPair };

		npSnsFbStreamBuildLibConfig(&libCfg, appCfg);
		npSnsFbStreamCopySession(&sessionCopy, session);
		ret = npSnsFbStreamResetLibRequest(req, &libReq);
		if (ret >= 0) {
			int grantRet = npSnsFbStreamGrantExtendedPermission(&libCfg, &sessionCopy,
									    libReq, &grantParams,
									    &localErr);
			ret = (grantRet < 0) ? grantRet : 0;
		}

		if (ret >= 0) {
			npSnsFbStreamBuildLibConfig(&libCfg, appCfg);
			npSnsFbStreamCopySession(&sessionCopy, session);
			ret = npSnsFbStreamResetLibRequest(req, &libReq);
			if (ret >= 0) {
				int retryRet = npSnsFbStreamExecutePublish(&libCfg, &sessionCopy,
									   libReq, &publishParams,
									   outPostIdBuf,
									   outPostIdSize,
									   &localErr);
				ret = (retryRet < 0) ? retryRet : 0;
			}
		}
	}

	if (ret == NP_SNS_FB_ERR_API_ERROR && outErr != NULL) {
		outErr->errorCode = localErr.errorCode;
		outErr->truncated = localErr.truncated;
	}
	return ret;
}

/* 0x813FA5B6 */
int npSnsFbStreamPublishSimple(NpSnsFbStreamRequest *req, const NpSnsFbStreamAppConfig *appCfg,
			       const NpSnsFbSession *session, const char *message,
			       const char *attachment, const char *actionLinks,
			       const char *errMsgBuf, SceSize errMsgBufSize,
			       SceInt32 *outFbError)
{
	(void)errMsgBuf;
	(void)errMsgBufSize;

	NpSnsFbErrorInfo localErr;
	sceClibMemset(&localErr, 0, sizeof(localErr));
	if (outFbError != NULL)
		*outFbError = 0;

	int ret = npSnsFbStreamPublishEx(req, appCfg, session, NULL, NULL, NULL, message,
					 attachment, actionLinks, NULL, 0, &localErr);
	if (ret == NP_SNS_FB_ERR_API_ERROR && outFbError != NULL)
		*outFbError = localErr.errorCode;
	return ret;
}
