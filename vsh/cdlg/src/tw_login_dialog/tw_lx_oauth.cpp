/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: Twitter OAuth 1.0a xAuth implementation.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libhmac.h>
#include <std/string.h>
#include <std/vector>
#include <std/algorithm>
#include <vsh/paf/include/rtc/rtc.h>
#include <job/job.h>
#include "tw_lx_oauth.h"

using namespace paf;

static const char s_defaultConsumerKey[] = "Pv57kPITxrF6VLXpvI5AGQ";
static const char s_defaultConsumerSecret[] = "eQdVH5lBRoLGk8GUlZQljPFyGANgGAqdawEmNYqxc";
static const char s_xauthConsumerKey[] = "4ly7suWYkOW0GOOwljTQw";
static const char s_xauthConsumerSecret[] = "M0oav9gxv6XgC0S7friqQemSJHRYMR1HurHT0T1Ckpk";
static const char s_xauthAccessTokenUrl[] = "https://api.twitter.com/oauth/access_token";

/* Abort condition adapter over paf::job::JobItem */
class LxJobAbortCondition : public LxAbortCondition {
public:
	explicit LxJobAbortCondition(job::JobItem *job) : m_job(job) {}
	virtual ~LxJobAbortCondition() {}
	virtual int IsAborted() { return (m_job != NULL) ? (m_job->IsCanceled() ? 1 : 0) : 0; }

	job::JobItem *m_job;
};

/* LxOAuthConfig */
LxOAuthConfig::LxOAuthConfig(LxAbortCondition *cond, int cfgFlags)
	: tmpl(LxHttpGetDefaultTemplate()), abortCond(cond), consumerKey(), consumerSecret(),
	  flags(cfgFlags)
{
	if (consumerKey.Empty()) {
		consumerKey = LxString(s_defaultConsumerKey);
		consumerSecret = LxString(s_defaultConsumerSecret);
	}
}

LxOAuthConfig::~LxOAuthConfig()
{
}

/* LxOAuthSigner */
LxOAuthSigner::LxOAuthSigner()
	: m_consumerKey(), m_consumerSecret(), m_token(), m_tokenSecret(), m_oauthParams()
{
}

LxOAuthSigner::~LxOAuthSigner()
{
}

void LxOAuthSigner::Init(const char *consumerKey, const char *consumerSecret, const char *token,
			 const char *tokenSecret)
{
	(void)consumerKey;
	(void)consumerSecret;
	m_token = LxString((token != NULL) ? token : "");
	m_tokenSecret = LxString((tokenSecret != NULL) ? tokenSecret : "");
	m_consumerKey = LxString(s_xauthConsumerKey);
	m_consumerSecret = LxString(s_xauthConsumerSecret);
	RefreshOAuthParams();
}

void LxOAuthSigner::RefreshOAuthParams()
{
	if (!m_consumerKey.Empty())
		m_oauthParams[LxString("oauth_consumer_key")] = m_consumerKey;
	else
		m_oauthParams.erase(LxString("oauth_consumer_key"));

	if (!m_token.Empty())
		m_oauthParams[LxString("oauth_token")] = m_token;
	else
		m_oauthParams.erase(LxString("oauth_token"));

	m_oauthParams[LxString("oauth_signature_method")] = LxString("HMAC-SHA1");

	/* Unix timestamp from RTC tick relative to 1970-01-01 */
	rtc::DateTime epoch;
	SceUInt64 epochTick = 0;
	SceUInt64 oneSecTick = 0;
	SceUInt64 nowTick = 0;

	epoch.year = 1970;
	epoch.month = 1;
	epoch.day = 1;
	epoch.hour = 0;
	epoch.minute = 0;
	epoch.second = 0;
	epoch.microsecond = 0;
	LxConvertDateTimeToTick(&epoch, &epochTick);
	LxTickAddSeconds(&oneSecTick, &oneSecTick, 1);
	LxGetCurrentTick(&nowTick);
	SceUInt64 unixTime = (oneSecTick != 0) ? ((nowTick - epochTick) / oneSecTick) : 0;

	m_oauthParams[LxString("oauth_timestamp")] = LxUInt64ToString(unixTime);

	/* Nonce: two process timestamps concatenated */
	LxString nonce = LxUInt64ToString(LxGetProcessTime());

	nonce.Append(LxUInt64ToString(LxGetProcessTime()));
	m_oauthParams[LxString("oauth_nonce")] = nonce;
	m_oauthParams[LxString("oauth_version")] = LxString("1.0");
}

struct OAuthParamRef {
	const char *key;
	map<LxString, LxString> *source;
};

static bool CompareOAuthParamRef(const OAuthParamRef &lhs, const OAuthParamRef &rhs)
{
	return sce_paf_strcmp(lhs.key, rhs.key) < 0;
}

LxString LxOAuthSigner::ComputeSignature(const char *method, const char *url,
					 map<LxString, LxString> &params)
{
	vector<OAuthParamRef> sorted;

	for (map<LxString, LxString>::iterator it = params.begin(); it != params.end(); ++it) {
		OAuthParamRef ref;

		ref.key = it->first.CStr();
		ref.source = &params;
		sorted.push_back(ref);
	}
	for (map<LxString, LxString>::iterator it = m_oauthParams.begin();
	     it != m_oauthParams.end(); ++it) {
		OAuthParamRef ref;

		ref.key = it->first.CStr();
		ref.source = &m_oauthParams;
		sorted.push_back(ref);
	}
	sort(sorted.begin(), sorted.end(), CompareOAuthParamRef);

	LxString paramStr;

	for (size_t i = 0; i < sorted.size(); ++i) {
		if (i > 0)
			paramStr.AppendChar('&');
		paramStr.Append(LxUrlEncode(sorted[i].key));
		paramStr.Append("=", 1);
		map<LxString, LxString>::iterator found =
			sorted[i].source->find(LxString(sorted[i].key));
		if (found != sorted[i].source->end())
			paramStr.Append(LxUrlEncode(found->second.CStr()));
	}

	LxString baseStr(method);

	baseStr.Append("&", 1);
	baseStr.Append(LxUrlEncode(url));
	baseStr.Append("&", 1);
	baseStr.Append(LxUrlEncode(paramStr.CStr()));

	LxString signingKey(m_consumerSecret);

	signingKey.Append("&", 1);
	signingKey.Append(m_tokenSecret);

	uint8_t digest[20];
	char b64[32];

	sceHmacSha1Digest((const unsigned char *)signingKey.CStr(), signingKey.Length(),
			  (const unsigned char *)baseStr.CStr(), baseStr.Length(), digest);
	size_t b64Len = LxBase64Encode(b64, digest, sizeof(digest));

	return LxString(b64, b64Len);
}

LxString LxOAuthSigner::BuildAuthHeader(const char *method, const char *url,
					map<LxString, LxString> &params)
{
	LxString header("OAuth ");

	for (map<LxString, LxString>::iterator it = m_oauthParams.begin();
	     it != m_oauthParams.end(); ++it) {
		header.Append(LxUrlEncode(it->first.CStr()));
		header.Append("=\"", 2);
		header.Append(LxUrlEncode(it->second.CStr()));
		header.Append("\", ", 3);
	}
	LxString sig = ComputeSignature(method, url, params);

	header.Append("oauth_signature=\"", 17);
	header.Append(LxUrlEncode(sig.CStr()));
	header.Append("\"", 1);
	return header;
}

int LxOAuthSigner::SendSignedRequest(LxHttpRequest &request, const char *method, const char *url,
				     map<LxString, LxString> &params,
				     LxAbortCondition *abortCond)
{
	if (sce_paf_strcmp(method, "POST") == 0) {
		LxString body;

		for (map<LxString, LxString>::iterator it = params.begin();
		     it != params.end(); ++it) {
			if (it != params.begin())
				body.AppendChar('&');
			body.Append(LxFormEncode(it->first.CStr()));
			body.Append("=", 1);
			body.Append(LxFormEncode(it->second.CStr()));
		}
		int ret = request.Open(url, method, body.Length(), abortCond);

		if (ret < 0)
			return ret;
		LxString authHeader = BuildAuthHeader(method, url, params);

		ret = request.AddHeader("Authorization", authHeader.CStr());
		if (ret < 0)
			return ret;
		ret = request.AddHeader("Content-type", "application/x-www-form-urlencoded");
		if (ret < 0)
			return ret;
		ret = request.Send(body.CStr(), body.Length());
		if (ret < 0)
			return ret;
		return 0;
	}

	LxString query;

	for (map<LxString, LxString>::iterator it = params.begin(); it != params.end(); ++it) {
		if (it != params.begin())
			query.AppendChar('&');
		query.Append(LxUrlEncode(it->first.CStr()));
		query.Append("=", 1);
		query.Append(LxUrlEncode(it->second.CStr()));
	}
	LxString fullUrl(url);

	if (!query.Empty()) {
		fullUrl.Append("?", 1);
		fullUrl.Append(query);
	}
	int ret = request.Open(fullUrl.CStr(), method, 0, abortCond);

	if (ret < 0)
		return ret;
	LxString authHeader = BuildAuthHeader(method, url, params);

	ret = request.AddHeader("Authorization", authHeader.CStr());
	if (ret < 0)
		return ret;
	ret = request.Send(NULL, 0);
	if (ret < 0)
		return ret;
	return 0;
}

int LxOAuthSigner::ReadResponseBody(LxHttpRequest &request, LxString &bodyOut)
{
	int statusCode = 0;
	int ret = request.GetStatusCode(&statusCode);

	if (ret < 0)
		return ret;
	if (statusCode < 200 || statusCode >= 300)
		return LX_ERROR_HTTP_STATUS;

	LxBuffer chunk;

	if (!chunk.Resize(1024))
		return LX_ERROR_HTTP_NO_MEM;
	for (;;) {
		SceSize bytesRead = 0;

		ret = request.Read(chunk.begin, chunk.Capacity(), &bytesRead);
		if (ret < 0)
			return ret;
		if (bytesRead == 0)
			break;
		bodyOut.Append((const char *)chunk.begin, bytesRead);
	}
	return 0;
}

void LxOAuthSigner::ParseQueryString(const LxString &query, map<LxString, LxString> &outMap)
{
	map<LxString, LxString> rawMap;

	LxParseKeyValue(rawMap, query, 0, '&', '=');
	for (map<LxString, LxString>::iterator it = rawMap.begin(); it != rawMap.end(); ++it)
		outMap[LxUrlDecode(it->first.CStr())] = LxUrlDecode(it->second.CStr());
}

int LxOAuthSigner::RequestXAuthToken(LxHttpRequest &request, const char *url, const char *username,
				     const char *password, LxAbortCondition *abortCond)
{
	map<LxString, LxString> params;

	params[LxString("x_auth_username")] = LxString(username);
	params[LxString("x_auth_password")] = LxString(password);
	params[LxString("x_auth_mode")] = LxString("client_auth");

	int sendRet = SendSignedRequest(request, "POST", url, params, abortCond);
	int statusCode = 0;

	if (request.GetStatusCode(&statusCode) == 0 && statusCode == 401)
		return LX_ERROR_OAUTH_UNAUTHORIZED;
	if (sendRet < 0)
		return sendRet;

	LxString responseBody;
	int readRet = ReadResponseBody(request, responseBody);

	if (readRet < 0)
		return readRet;

	map<LxString, LxString> responseMap;

	ParseQueryString(responseBody, responseMap);
	m_token = responseMap[LxString("oauth_token")];
	m_tokenSecret = responseMap[LxString("oauth_token_secret")];
	RefreshOAuthParams();
	if (m_token.Empty() || m_tokenSecret.Empty())
		return LX_ERROR_OAUTH_NO_TOKEN;
	return readRet;
}

/* LxTwitterOAuthClient */
LxTwitterOAuthClient::LxTwitterOAuthClient(const LxOAuthConfig &config, const char *token,
					   const char *tokenSecret)
	: m_reserved(0), m_signer(), m_request(config.tmpl), m_abortCond(config.abortCond),
	  m_flags(config.flags)
{
	m_signer.Init(config.consumerKey.CStr(), config.consumerSecret.CStr(), token, tokenSecret);
}

LxTwitterOAuthClient::~LxTwitterOAuthClient()
{
}

int LxTwitterOAuthClient::Login(LxString &tokenOut, LxString &secretOut, const char *username,
				const char *password)
{
	int ret = m_signer.RequestXAuthToken(m_request, s_xauthAccessTokenUrl, username, password,
					     m_abortCond);

	if (ret >= 0) {
		tokenOut = m_signer.m_token;
		secretOut = m_signer.m_tokenSecret;
	}
	return ret;
}

/* LxLoginSession */
void LxLoginSession::InitHeap()
{
	LxInitHeapAllocator();
}

void LxLoginSession::TermHeap()
{
	LxTermHeapAllocator();
}

void LxLoginSession::Init(Plugin *plugin)
{
	LxInitSystemPlugin(plugin);
	LxInitTaskRoot();
	LxRegistry::Init();
	LxHttpInit();
	LxRefPtr<LxHttpTemplate> tmpl = LxHttpGetDefaultTemplate();

	if (!tmpl.IsNull())
		tmpl->m_flushCookies = 1;
}

void LxLoginSession::Term()
{
	LxHttpTerm();
	LxRegistry::Term();
	LxTermTaskRoot();
	LxTermSystemPlugin();
}

int LxLoginSession::Login(void *jobItem, const char *username, const char *password,
			  LxString &errorDetailOut)
{
	(void)errorDetailOut;
	LxJobAbortCondition abortCond((job::JobItem *)jobItem);
	LxOAuthConfig config(&abortCond, 0);
	LxTwitterOAuthClient client(config, "", "");
	LxString token;
	LxString secret;

	int ret = client.Login(token, secret, username, password);

	if (ret == 0) {
		LxRegistry::SetParamString(1, token.CStr());
		LxRegistry::SetParamString(2, secret.CStr());
	}
	return ret;
}
