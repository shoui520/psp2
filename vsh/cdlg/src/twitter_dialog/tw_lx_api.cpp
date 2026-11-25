/*
 * SceTwitterDialogPlugin: HTTP binary downloader and Twitter REST API client.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/list>
#include <std/map>
#include "tw_lx_api.h"

#define LX_ERROR_NO_MEM			((SceInt32)0xA0000701)
#define LX_ERROR_SHORT_BODY		((SceInt32)0xA0000702)
#define LX_ERROR_ABORTED		((SceInt32)0xA0000704)
#define LX_ERROR_TOO_LARGE		((SceInt32)0xA000070B)
#define LX_ERROR_HTTP_BAD_STATUS	((SceInt32)0xA000070D)
#define LX_ERROR_TWITTER_AUTH		((SceInt32)0xA0000305)

extern "C" {
void _ZN3paf15HeapAllocBuffer8AllocateEjPNS_6memory13HeapAllocatorE(
	paf::common::SharedPtr<LxPafBuffer> *outBuf, SceSize size,
	paf::memory::HeapAllocator *allocator);
SceInt32 sceHttpGetResponseContentLength(SceInt32 reqId, SceUInt64 *contentLength);
}

static const char *FindAsciiSubstr(const char *haystack, const char *needle)
{
	if (haystack == NULL || needle == NULL)
		return NULL;
	SceSize needleLen = sce_paf_strlen(needle);

	if (needleLen == 0)
		return haystack;
	for (const char *p = haystack; (p = sce_paf_strchr(p, needle[0])) != NULL; ++p) {
		if (sce_paf_strncmp(p, needle, needleLen) == 0)
			return p;
	}
	return NULL;
}

struct DownloadChunk {
	paf::common::SharedPtr<LxPafBuffer> buffer;
	SceSize validBytes;
};

SceInt32 LxHttpDownloadChunked(LxHttpRequest &req, paf::memory::HeapAllocator *allocator,
			       paf::common::SharedPtr<LxPafBuffer> &outBuf,
			       SceUInt64 maxBytes)
{
	static const SceSize CHUNK_SIZE = 0x8000;
	paf::list<DownloadChunk> chunks;
	SceUInt64 totalBytes = 0;

	for (;;) {
		if (totalBytes + CHUNK_SIZE > maxBytes)
			return LX_ERROR_TOO_LARGE;

		paf::common::SharedPtr<LxPafBuffer> chunkBuf;

		_ZN3paf15HeapAllocBuffer8AllocateEjPNS_6memory13HeapAllocatorE(&chunkBuf, CHUNK_SIZE, allocator);
		if (chunkBuf.get() == NULL)
			return LX_ERROR_NO_MEM;

		uint8_t *dst = (uint8_t *)chunkBuf->GetAddr();
		SceSize filled = 0;
		SceSize lastRead = 0;

		do {
			SceInt32 ret = req.Read(dst + filled, CHUNK_SIZE - filled, &lastRead);

			if (ret < 0)
				return ret;
			if (lastRead == 0)
				break;
			filled += lastRead;
		} while (filled < CHUNK_SIZE);

		DownloadChunk entry;

		entry.buffer = chunkBuf;
		entry.validBytes = filled;
		chunks.push_back(entry);
		totalBytes += filled;

		if (lastRead == 0)
			break;
	}

	paf::common::SharedPtr<LxPafBuffer> combined;

	_ZN3paf15HeapAllocBuffer8AllocateEjPNS_6memory13HeapAllocatorE(&combined, (SceSize)totalBytes, allocator);
	if (combined.get() == NULL)
		return LX_ERROR_NO_MEM;

	uint8_t *writePtr = (uint8_t *)combined->GetAddr();

	for (paf::list<DownloadChunk>::iterator it = chunks.begin(); it != chunks.end(); ++it) {
		if (it->validBytes > 0) {
			sce_paf_memcpy(writePtr, it->buffer->GetAddr(), it->validBytes);
			writePtr += it->validBytes;
		}
	}
	outBuf = combined;
	return 0;
}

SceInt32 LxHttpDownloadBody(LxHttpRequest &req, paf::memory::HeapAllocator *allocator,
			    paf::common::SharedPtr<LxPafBuffer> &outBuf,
			    SceUInt64 maxBytes)
{
	SceUInt64 contentLength = 0;
	SceInt32 ret = sceHttpGetResponseContentLength(req.m_reqId, &contentLength);

	if (ret < 0)
		return LxHttpDownloadChunked(req, allocator, outBuf, maxBytes);
	if (contentLength > maxBytes)
		return LX_ERROR_TOO_LARGE;

	paf::common::SharedPtr<LxPafBuffer> bodyBuf;

	_ZN3paf15HeapAllocBuffer8AllocateEjPNS_6memory13HeapAllocatorE(&bodyBuf, (SceSize)contentLength, allocator);
	if (bodyBuf.get() == NULL)
		return LX_ERROR_NO_MEM;

	uint8_t *dst = (uint8_t *)bodyBuf->GetAddr();
	SceUInt64 received = 0;

	while (received < contentLength) {
		SceSize chunk = 0;

		ret = req.Read(dst + received, (SceSize)(contentLength - received), &chunk);
		if (ret < 0)
			return ret;
		if (chunk == 0)
			break;
		received += chunk;
	}
	if (received != contentLength)
		return LX_ERROR_SHORT_BODY;

	outBuf = bodyBuf;
	return 0;
}

SceInt32 LxHttpDownloadUrl(const char *url, paf::memory::HeapAllocator *allocator,
			   paf::common::SharedPtr<LxPafBuffer> &outBuf,
			   LxAbortCondition *abortCond, SceUInt64 maxBytes)
{
	LxRefPtr<LxHttpTemplate> tmpl = LxHttpGetDefaultTemplate();
	LxHttpRequest req(tmpl);
	SceInt32 ret = req.Open(url, "GET", 0, abortCond);

	if (ret < 0)
		return ret;
	ret = req.Send(NULL, 0);
	if (ret < 0)
		return ret;

	SceInt32 statusCode = 0;

	ret = req.GetStatusCode(&statusCode);
	if (ret < 0)
		return ret;
	if (statusCode < 200 || statusCode >= 300)
		return LX_ERROR_HTTP_BAD_STATUS;

	ret = LxHttpDownloadBody(req, allocator, outBuf, maxBytes);
	return (ret < 0) ? ret : 0;
}

/* LxTwitterApiClient */
LxTwitterApiClient::LxTwitterApiClient(const LxOAuthConfig &config)
	: m_signer(), m_request(config.tmpl), m_abortCond(config.abortCond)
{
	LxString token;
	LxString tokenSecret;
	LxRegistry reg;

	reg.Init();
	reg.Term();
	m_signer.Init(config.consumerKey.CStr(), config.consumerSecret.CStr(),
		      token.CStr(), tokenSecret.CStr());
}

LxTwitterApiClient::~LxTwitterApiClient()
{
}

SceInt32 LxTwitterApiClient::CallEndpoint(LxVariant &outJson, const char *endpoint,
					  paf::map<LxString, LxString> &params,
					  const char *method,
					  LxJsonArrayCallback *arrayCb,
					  LxMultipartForm &multipart)
{
	(void)arrayCb;
	LxString url("https://api.twitter.com/1.1/");

	url.Append(endpoint, sce_paf_strlen(endpoint));
	url.Append(".json", 5);

	SceInt32 ret = 0;

	if (multipart.m_parts.size() != 0) {
		SceUInt64 contentLen = 0;

		ret = multipart.ComputeContentLength(&contentLen);
		if (ret < 0)
			return ret;

		paf::map<LxString, LxString> oauthOnly;
		LxString authHeader = m_signer.BuildAuthHeader(method, url.CStr(), oauthOnly);

		ret = m_request.Open(url.CStr(), method, contentLen, m_abortCond);
		if (ret < 0)
			return ret;
		m_request.AddHeader("Authorization", authHeader.CStr());
		ret = multipart.SetContentTypeHeader(m_request);
		if (ret < 0)
			return ret;
		ret = multipart.SendAll(m_request);
		if (ret < 0)
			return ret;
	} else {
		ret = m_signer.SendSignedRequest(m_request, method, url.CStr(), params,
						 m_abortCond);
		if (ret < 0)
			return ret;
	}

	SceInt32 statusCode = 0;

	ret = m_request.GetStatusCode(&statusCode);
	if (ret < 0)
		return ret;

	LxString body;

	ret = m_signer.ReadResponseBody(m_request, body);
	if (ret < 0)
		return ret;

	if (!body.Empty())
		LxJsonParseString(body.CStr(), body.Length(), outJson);

	LxVariant &statusMember = LxVariantEnsureMember(outJson, LxString("__http_status_code"));

	statusMember.SetType(LxVariant::TYPE_INT64);
	statusMember.m_value.intVal = statusCode;

	if (statusCode == 401)
		return LX_ERROR_TWITTER_AUTH;
	if (statusCode < 200 || statusCode >= 300)
		return LX_ERROR_HTTP_STATUS;
	return 0;
}

SceInt32 LxTwitterApiClient::GetConfiguration(LxVariant &outJson)
{
	paf::map<LxString, LxString> params;
	LxMultipartForm emptyForm;

	return CallEndpoint(outJson, "help/configuration", params, "GET", NULL, emptyForm);
}

SceInt32 LxTwitterApiClient::VerifyCredentials(LxVariant &outJson)
{
	paf::map<LxString, LxString> params;
	LxMultipartForm emptyForm;

	return CallEndpoint(outJson, "account/verify_credentials", params, "GET", NULL, emptyForm);
}

SceInt32 LxTwitterApiClient::PostTweet(LxVariant &outJson, const char *statusText,
				       const LxTweetOptions &options)
{
	paf::map<LxString, LxString> params;
	LxMultipartForm multipart;

	if (!options.mediaPath.Empty()) {
		multipart.AddTextField(LxString("status"), LxString(statusText));
		if (options.inReplyToStatusId != 0) {
			multipart.AddTextField(LxString("in_reply_to_status_id"),
					       LxUInt64ToString(options.inReplyToStatusId));
		}
		if (options.hasLocation) {
			multipart.AddTextField(LxString("lat"),
					       LxFormatString("%f", options.latitude));
			multipart.AddTextField(LxString("long"),
					       LxFormatString("%f", options.longitude));
		}
		const char *dot = sce_paf_strrchr(options.mediaPath.CStr(), '.');
		const char *mime = (dot != NULL) ? LxGetMimeTypeForExtension(dot + 1) : NULL;

		multipart.AddFileField(LxString("media[]"), options.mediaPath,
				       LxString(mime != NULL ? mime : ""));
		return CallEndpoint(outJson, "statuses/update_with_media", params, "POST", NULL,
				    multipart);
	}

	params[LxString("status")] = LxString(statusText);
	if (options.inReplyToStatusId != 0)
		params[LxString("in_reply_to_status_id")] =
			LxUInt64ToString(options.inReplyToStatusId);
	if (options.hasLocation) {
		params[LxString("lat")] = LxFormatString("%f", options.latitude);
		params[LxString("long")] = LxFormatString("%f", options.longitude);
	}
	return CallEndpoint(outJson, "statuses/update", params, "POST", NULL, multipart);
}

bool LxTwitterApiClient::ExtractErrorDetail(const LxVariant &json, SceInt32 *outCode,
					    LxString &outMessage)
{
	const LxVariant &errors = LxVariantGetMember(json, LxString("errors"));

	if (errors.m_type == LxVariant::TYPE_LIST && errors.m_value.listVal != NULL &&
	    errors.m_value.listVal->size() != 0) {
		const LxVariant &first = errors.m_value.listVal->front();
		const LxVariant &msg = LxVariantGetMember(first, LxString("message"));

		if (msg.m_type == LxVariant::TYPE_STRING && msg.m_value.strVal != NULL) {
			outMessage = *msg.m_value.strVal;
			if (outCode != NULL)
				*outCode = 0;
			return true;
		}
	}
	const LxVariant &err = LxVariantGetMember(json, LxString("error"));

	if (err.m_type == LxVariant::TYPE_STRING && err.m_value.strVal != NULL) {
		outMessage = *err.m_value.strVal;
		if (outCode != NULL)
			*outCode = 0;
		return true;
	}
	return false;
}

void LxTwitterApiClient::ExtractHttpError(const LxVariant &json, SceInt32 *outHttpStatus,
					  LxString &outMessage)
{
	const LxVariant &status = LxVariantGetMember(json, LxString("__http_status_code"));

	if (outHttpStatus != NULL)
		*outHttpStatus = (status.m_type == LxVariant::TYPE_INT64)
					 ? (SceInt32)status.m_value.intVal
					 : 0;

	SceInt32 errCode = 0;
	LxString detail;

	if (ExtractErrorDetail(json, &errCode, detail))
		outMessage = detail;

	const char *str = outMessage.CStr();

	if (str != NULL &&
	    (FindAsciiSubstr(str, "<html>") != NULL || FindAsciiSubstr(str, "<HTML>") != NULL)) {
		outMessage.Assign(NULL, 0);
	}
}
