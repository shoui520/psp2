/*
 * np_push_get_server_addr_trans.cpp: HTTP GET /np/serveraddr transaction and streaming
 * JSON response parser for obtaining the push WebSocket server FQDN and retry interval.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_get_server_addr_trans.cpp
 * 3.65-CEX: 0x813B6204..0x813B6927
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* JSON schema for GET /np/serveraddr:
 *   { "fqdn": "...", "error": { "code": N, "message": "...", "retryInterval": N } }
 */
static const NpPushJsonLexerNode kServerAddrRootNodes[] = {
	{ NP_PUSH_LEXER_NODE_KEY, NULL, 1 },
	{ NP_PUSH_LEXER_NODE_END, "",   0 }
};

static const NpPushJsonLexerNode kServerAddrTopNodes[] = {
	{ NP_PUSH_LEXER_NODE_KEY, "fqdn",  0 },
	{ NP_PUSH_LEXER_NODE_KEY, "error", 1 },
	{ NP_PUSH_LEXER_NODE_END, "",      0 }
};

static const NpPushJsonLexerNode kServerAddrErrorNodes[] = {
	{ NP_PUSH_LEXER_NODE_KEY, "code",          0 },
	{ NP_PUSH_LEXER_NODE_KEY, "message",       0 },
	{ NP_PUSH_LEXER_NODE_KEY, "retryInterval", 0 },
	{ NP_PUSH_LEXER_NODE_END, "",              0 }
};

static const NpPushJsonLexerNode *const kServerAddrParseLevels[] = {
	kServerAddrRootNodes,
	kServerAddrTopNodes,
	kServerAddrErrorNodes
};

static const NpPushJsonLexerConfig kServerAddrParseConfig = {
	kServerAddrParseLevels,
	3
};

/* ============================================================================
 * ServerAddr (0x813B6204..0x813B62FF)
 * ============================================================================ */

ServerAddr::ServerAddr() : m_fqdn(NULL) {}

ServerAddr::~ServerAddr()
{
	Clear();
}

int ServerAddr::SetFqdn(const char *fqdn)
{
	Clear();
	if (fqdn == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	m_fqdn = SceNpCommonPs4_1CAB4A2E(GetAllocator(), fqdn);
	if (m_fqdn == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

void ServerAddr::Clear(void)
{
	if (m_fqdn != NULL) {
		SceNpCommonPs4_6B428829(GetAllocator(), m_fqdn);
		m_fqdn = NULL;
	}
}

/* ============================================================================
 * Streaming JSON Observer for ServerAddr Response
 * ============================================================================ */

struct ServerAddrResponseParser {
	NpPushJsonParser parser;
	const void *const *observerVtable;
	ServerAddr *outServerAddr;
	bool hasFqdn;
	bool hasErrorCode;
	SceInt32 serverErrorCode;
	bool hasRetryInterval;
	SceUInt32 retryIntervalSec;
};

static int ServerAddrObserverNoOp(void)
{
	return 0;
}

static int ServerAddrObserverOnObjectValue(const void *const *const *observerSelf,
					   int depth, int nodeIndex, const char *name,
					   int valueType, const char *value)
{
	const SceSize observerOffset = offsetof(ServerAddrResponseParser, observerVtable);
	const unsigned char *rawObserver = reinterpret_cast<const unsigned char *>(observerSelf);
	ServerAddrResponseParser *self = reinterpret_cast<ServerAddrResponseParser *>(
		const_cast<unsigned char *>(rawObserver - observerOffset));

	if (depth == 1 && nodeIndex == 0) {
		NP_PUSH_ASSERT(sceClibStrncmp(name, "fqdn", sizeof("fqdn")) == 0,
			       "onObjectValue", "core\\np_push_get_server_addr_trans.cpp", 118,
			       "sceClibStrncmp((name), (\"fqdn\"), (sizeof(\"fqdn\")))==0");
		if (value == NULL || valueType != NP_PUSH_LEXER_TOKEN_STRING)
			return SCE_NP_PUSH_ERROR_JSON_PARSE;
		int ret = self->outServerAddr->SetFqdn(value);
		if (ret < 0)
			return ret;
		self->hasFqdn = true;
		return 0;
	}

	if (depth == 2 && nodeIndex == 0) {
		NP_PUSH_ASSERT(sceClibStrncmp(name, "code", sizeof("code")) == 0,
			       "onObjectValue", "core\\np_push_get_server_addr_trans.cpp", 132,
			       "sceClibStrncmp((name), (\"code\"), (sizeof(\"code\")))==0");
		if (value == NULL || valueType != NP_PUSH_LEXER_TOKEN_NUMBER)
			return SCE_NP_PUSH_ERROR_JSON_PARSE;
		SceInt32 parsedCode = 0;
		if (SceNpCommonPs4_3AE5ABF3(value, &parsedCode, 10, NULL) < 0)
			return SCE_NP_PUSH_ERROR_JSON_PARSE;
		self->serverErrorCode = parsedCode;
		self->hasErrorCode = true;
		return 0;
	}

	if (depth == 2 && nodeIndex == 2) {
		NP_PUSH_ASSERT(sceClibStrncmp(name, "retryInterval", sizeof("retryInterval")) == 0,
			       "onObjectValue", "core\\np_push_get_server_addr_trans.cpp", 146,
			       "sceClibStrncmp((name), (\"retryInterval\"), (sizeof(\"retryInterval\")))==0");
		if (value == NULL || valueType != NP_PUSH_LEXER_TOKEN_NUMBER)
			return SCE_NP_PUSH_ERROR_JSON_PARSE;
		SceUInt32 parsedInterval = 0;
		if (SceNpCommonPs4_D5B30475(value, &parsedInterval, 10, NULL) < 0)
			return SCE_NP_PUSH_ERROR_JSON_PARSE;
		self->retryIntervalSec = parsedInterval;
		self->hasRetryInterval = true;
		return 0;
	}

	return 0;
}

static const void *const kServerAddrObserverVtable[] = {
	reinterpret_cast<const void *>(ServerAddrObserverNoOp),
	reinterpret_cast<const void *>(ServerAddrObserverNoOp),
	reinterpret_cast<const void *>(ServerAddrObserverNoOp),
	reinterpret_cast<const void *>(ServerAddrObserverOnObjectValue),
	reinterpret_cast<const void *>(ServerAddrObserverNoOp),
	reinterpret_cast<const void *>(ServerAddrObserverNoOp),
	reinterpret_cast<const void *>(ServerAddrObserverNoOp)
};

/* ============================================================================
 * GetServerAddrTrans (0x813B6310..0x813B6927)
 * ============================================================================ */

GetServerAddrTrans::GetServerAddrTrans(NpPushAllocator *allocator,
				       const NpPushOnlineId *onlineId)
	: m_allocator(allocator)
{
	SceNpCommonPs4_3FC6688C(&m_npTrans, allocator, onlineId);
}

GetServerAddrTrans::~GetServerAddrTrans()
{
	SceNpCommonPs4_47D94EF0(&m_npTrans);
}

int GetServerAddrTrans::Execute(NpPushHandle *cancelHandle, NpPushHttpContext *httpContext,
				const char *baseUrl, ServerAddr *outServerAddr,
				SceUInt32 *outRetryIntervalSec)
{
	if (httpContext == NULL || baseUrl == NULL || outServerAddr == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	if (outRetryIntervalSec != NULL)
		*outRetryIntervalSec = 0;

	ProtocolVersion protoVer;
	int ret = GetServerAddrProtocolVersion(&protoVer);
	if (ret < 0)
		return ret;

	char pathBuf[64];
	if (protoVer.majorVersion == 1 && protoVer.minorVersion == 0) {
		sceClibSnprintf(pathBuf, sizeof(pathBuf), "/np/serveraddr");
	} else {
		sceClibSnprintf(pathBuf, sizeof(pathBuf), "/np/serveraddr?version=%u.%u",
				protoVer.majorVersion, protoVer.minorVersion);
	}

	SceSize urlBufSize = 256;
	char *fullUrl = static_cast<char *>(SceNpCommonPs4_AA980113(m_allocator, urlBufSize));
	if (fullUrl == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	SceSize required = 0;
	ret = SceNpCommonPs4_D9298357(baseUrl, pathBuf, fullUrl, urlBufSize, &required);
	if (ret < 0 && required > urlBufSize) {
		SceNpCommonPs4_6B428829(m_allocator, fullUrl);
		urlBufSize = required + 1;
		fullUrl = static_cast<char *>(SceNpCommonPs4_AA980113(m_allocator, urlBufSize));
		if (fullUrl == NULL)
			return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
		ret = SceNpCommonPs4_D9298357(baseUrl, pathBuf, fullUrl, urlBufSize, &required);
	}

	if (ret >= 0) {
		ret = SceNpCommonPs4_D30A8E84(&m_npTrans.httpTrans, httpContext->httpTemplate,
					      httpContext->connPool, 0, fullUrl, 0);
	}
	SceNpCommonPs4_6B428829(m_allocator, fullUrl);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_57B14D22(&m_npTrans, cancelHandle);
	if (ret < 0)
		return ret;

	if (protoVer.useTitleToken != 0) {
		ret = SceNpCommonPs4_060EB06B(&m_npTrans, cancelHandle);
		if (ret < 0)
			return ret;
	}

	ret = SceNpCommonPs4_3FC1A88F(&m_npTrans.httpTrans, cancelHandle, NULL, 0);
	if (ret < 0)
		return ret;

	SceInt32 statusCode = 0;
	ret = SceNpCommonPs4_6B6E813E(&m_npTrans.httpTrans, cancelHandle, &statusCode);
	if (ret < 0)
		return ret;

	ServerAddrResponseParser *respParser = static_cast<ServerAddrResponseParser *>(
		SceNpCommonPs4_AA980113(m_allocator, sizeof(ServerAddrResponseParser)));
	if (respParser == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	sceClibMemset(respParser, 0, sizeof(*respParser));
	SceNpCommonPs4_4F720AFD(&respParser->parser, m_allocator);
	respParser->observerVtable = kServerAddrObserverVtable;
	respParser->outServerAddr = outServerAddr;

	ret = SceNpCommonPs4_89322005(&respParser->parser,
				      const_cast<NpPushJsonLexerConfig *>(&kServerAddrParseConfig),
				      &respParser->observerVtable);

	char chunkBuf[1024];
	while (ret >= 0) {
		SceSize received = 0;
		ret = SceNpCommonPs4_C80B0275(&m_npTrans.httpTrans, cancelHandle,
					      chunkBuf, sizeof(chunkBuf), &received);
		if (ret < 0)
			break;
		if (received == 0)
			break;
		ret = SceNpCommonPs4_FE98E2A7(&respParser->parser, chunkBuf,
					      static_cast<int>(received));
	}

	if (ret >= 0) {
		if (statusCode == 200) {
			if (!respParser->hasFqdn)
				ret = SCE_NP_COMMON_PS4_ERROR_JSON_MISSING_KEY;
		} else {
			if (respParser->hasRetryInterval && outRetryIntervalSec != NULL)
				*outRetryIntervalSec = respParser->retryIntervalSec;
			if (respParser->hasErrorCode) {
				SceNpCommonPs4_6DA866D5(0, respParser->serverErrorCode);
				ret = SCE_NP_PUSH_ERROR_SERVER_RESPONSE;
			} else if (statusCode == 503) {
				ret = SCE_NP_PUSH_ERROR_SERVICE_UNAVAILABLE;
			} else {
				ret = SCE_NP_PUSH_ERROR_SERVER_RESPONSE;
			}
		}
	}

	SceNpCommonPs4_0A3AA2EF(&respParser->parser);
	SceNpCommonPs4_6B428829(m_allocator, respParser);
	return ret;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
