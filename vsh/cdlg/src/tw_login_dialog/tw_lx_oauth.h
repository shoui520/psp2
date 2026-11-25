/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: Twitter OAuth 1.0a xAuth client and session.
 */
#ifndef TW_LX_OAUTH_H
#define TW_LX_OAUTH_H

#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/map>
#include "tw_lx.h"
#include "tw_lx_http.h"

/* OAuth session configuration */
struct LxOAuthConfig {
	LxOAuthConfig(LxAbortCondition *abortCond, int flags);
	~LxOAuthConfig();

	LxRefPtr<LxHttpTemplate> tmpl;
	LxAbortCondition *abortCond;
	LxString consumerKey;
	LxString consumerSecret;
	int flags;
};

/* OAuth 1.0a signer and HTTP request helper */
class LxOAuthSigner {
public:
	LxOAuthSigner();
	~LxOAuthSigner();

	void Init(const char *consumerKey, const char *consumerSecret, const char *token,
		  const char *tokenSecret);
	void RefreshOAuthParams();
	LxString ComputeSignature(const char *method, const char *url,
				  paf::map<LxString, LxString> &params);
	LxString BuildAuthHeader(const char *method, const char *url,
				 paf::map<LxString, LxString> &params);
	int SendSignedRequest(LxHttpRequest &request, const char *method, const char *url,
			      paf::map<LxString, LxString> &params,
			      LxAbortCondition *abortCond);
	int ReadResponseBody(LxHttpRequest &request, LxString &bodyOut);
	void ParseQueryString(const LxString &query, paf::map<LxString, LxString> &outMap);
	int RequestXAuthToken(LxHttpRequest &request, const char *url, const char *username,
			      const char *password, LxAbortCondition *abortCond);

	LxString m_consumerKey;
	LxString m_consumerSecret;
	LxString m_token;
	LxString m_tokenSecret;
	paf::map<LxString, LxString> m_oauthParams;
};

/* Combined Twitter OAuth client */
class LxTwitterOAuthClient {
public:
	LxTwitterOAuthClient(const LxOAuthConfig &config, const char *token,
			     const char *tokenSecret);
	~LxTwitterOAuthClient();

	int Login(LxString &tokenOut, LxString &secretOut, const char *username,
		  const char *password);

	int m_reserved;
	LxOAuthSigner m_signer;
	LxHttpRequest m_request;
	LxAbortCondition *m_abortCond;
	int m_flags;
};

/* High-level login session used by TwLoginDialogApp */
class LxLoginSession {
public:
	static void InitHeap();
	static void TermHeap();
	static void Init(paf::Plugin *plugin);
	static void Term();
	static int Login(void *jobItem, const char *username, const char *password,
			 LxString &errorDetailOut);
};

#endif /* TW_LX_OAUTH_H */
