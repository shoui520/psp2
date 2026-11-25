/*
 * SceTwitterDialogPlugin: HTTP binary downloader and Twitter REST API v1.1 client.
 */
#ifndef TWITTER_DIALOG_TW_LX_API_H
#define TWITTER_DIALOG_TW_LX_API_H

#include <stdint.h>
#include <scetypes.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include "../tw_login_dialog/tw_lx.h"
#include "../tw_login_dialog/tw_lx_http.h"
#include "../tw_login_dialog/tw_lx_oauth.h"
#include "tw_lx_json.h"
#include "tw_lx_multipart.h"

/* PAF reference-counted memory buffer returned by ScePafMisc_E9C6A564 */
class LxPafBuffer {
public:
	virtual void *GetAddr() = 0;
	virtual SceSize GetSize() = 0;
	virtual ~LxPafBuffer() {}
};

/* Optional tweet parameters for PostTweet */
struct LxTweetOptions {
	LxTweetOptions() : hasLocation(false), latitude(0.0), longitude(0.0),
			   inReplyToStatusId(0), mediaPath() {}
	bool hasLocation;
	double latitude;
	double longitude;
	SceUInt64 inReplyToStatusId;
	LxString mediaPath;
};

/* HTTP binary download helpers (used for Twitter profile icons) */
SceInt32 LxHttpDownloadChunked(LxHttpRequest &req, paf::memory::HeapAllocator *allocator,
			       paf::common::SharedPtr<LxPafBuffer> &outBuf,
			       SceUInt64 maxBytes);
SceInt32 LxHttpDownloadBody(LxHttpRequest &req, paf::memory::HeapAllocator *allocator,
			    paf::common::SharedPtr<LxPafBuffer> &outBuf,
			    SceUInt64 maxBytes);
SceInt32 LxHttpDownloadUrl(const char *url, paf::memory::HeapAllocator *allocator,
			   paf::common::SharedPtr<LxPafBuffer> &outBuf,
			   LxAbortCondition *abortCond, SceUInt64 maxBytes);

/* Twitter REST API client over LxOAuthSigner + LxHttpRequest + LxJsonParser */
class LxTwitterApiClient {
public:
	explicit LxTwitterApiClient(const LxOAuthConfig &config);
	~LxTwitterApiClient();

	SceInt32 CallEndpoint(LxVariant &outJson, const char *endpoint,
			      paf::map<LxString, LxString> &params,
			      const char *method, LxJsonArrayCallback *arrayCb,
			      LxMultipartForm &multipart);
	SceInt32 GetConfiguration(LxVariant &outJson);
	SceInt32 VerifyCredentials(LxVariant &outJson);
	SceInt32 PostTweet(LxVariant &outJson, const char *statusText,
			   const LxTweetOptions &options);

	static bool ExtractErrorDetail(const LxVariant &json, SceInt32 *outCode,
				       LxString &outMessage);
	static void ExtractHttpError(const LxVariant &json, SceInt32 *outHttpStatus,
				     LxString &outMessage);

	LxOAuthSigner m_signer;
	LxHttpRequest m_request;
	LxAbortCondition *m_abortCond;
};

#endif /* TWITTER_DIALOG_TW_LX_API_H */
