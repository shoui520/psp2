/*
 * np_oauth_response.cpp: OAuth 2.0 HTTP/JSON/redirect response parsing, JWT id_token
 * Base64URL decoding, and server error mapping for NpOAuthVsh.
 */
#include "np_oauth_internal.h"
#include <libkernel/libkernel.h>

/* JSON lexer root-level schema shared across all four OAuth response parsers */
static const NpOAuthJsonLexerNode kNpOAuthRootLevel[] = {
	{ NP_OAUTH_JSON_NODE_KEY, NULL, 1 },
	{ NP_OAUTH_JSON_NODE_END, "", 0 },
};

/* Access token endpoint JSON keys */
static const NpOAuthJsonLexerNode kNpOAuthAccessTokenKeys[] = {
	{ NP_OAUTH_JSON_NODE_KEY, "access_token", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "token_type", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "expires_in", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "scope", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "id_token", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error_code", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error_description", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "description", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "", 0 },
};

static const NpOAuthJsonLexerNode *const kNpOAuthAccessTokenLevels[] = {
	kNpOAuthRootLevel,
	kNpOAuthAccessTokenKeys,
};

static const NpOAuthJsonLexerConfig kNpOAuthAccessTokenJsonConfig = {
	kNpOAuthAccessTokenLevels,
	2U,
};

/* NPSSO password-grant endpoint JSON keys */
static const NpOAuthJsonLexerNode kNpOAuthNpssoKeys[] = {
	{ NP_OAUTH_JSON_NODE_KEY, "npsso_token", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "id_token", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error_code", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error_description", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "", 0 },
};

static const NpOAuthJsonLexerNode *const kNpOAuthNpssoLevels[] = {
	kNpOAuthRootLevel,
	kNpOAuthNpssoKeys,
};

static const NpOAuthJsonLexerConfig kNpOAuthNpssoJsonConfig = {
	kNpOAuthNpssoLevels,
	2U,
};

/* Error-only JSON response keys (authorize endpoint fallback) */
static const NpOAuthJsonLexerNode kNpOAuthErrorKeys[] = {
	{ NP_OAUTH_JSON_NODE_KEY, "error_code", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "error_description", 0 },
	{ NP_OAUTH_JSON_NODE_END, "", 0 },
};

static const NpOAuthJsonLexerNode *const kNpOAuthErrorLevels[] = {
	kNpOAuthRootLevel,
	kNpOAuthErrorKeys,
};

static const NpOAuthJsonLexerConfig kNpOAuthErrorJsonConfig = {
	kNpOAuthErrorLevels,
	2U,
};

/* JWT id_token payload claims */
static const NpOAuthJsonLexerNode kNpOAuthIdTokenKeys[] = {
	{ NP_OAUTH_JSON_NODE_KEY, "auth_time", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "iat", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "exp", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "iss", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "env_iss_id", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "user_id", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "aud", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "acr", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "acct_id", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "ctry_code", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "lang", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "comm_domain", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "online_id", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "dob", 0 },
	{ NP_OAUTH_JSON_NODE_KEY, "age", 0 },
	{ NP_OAUTH_JSON_NODE_END, "", 0 },
};

static const NpOAuthJsonLexerNode *const kNpOAuthIdTokenLevels[] = {
	kNpOAuthRootLevel,
	kNpOAuthIdTokenKeys,
};

static const NpOAuthJsonLexerConfig kNpOAuthIdTokenJsonConfig = {
	kNpOAuthIdTokenLevels,
	2U,
};

/* Fatal OAuth/CSLA errors that invalidate the active NP Manager session */
static const int kNpOAuthFatalSessionErrors[] = {
	(int)0x8055A814U,
	(int)0x82E00014U,
	(int)0x82E0001CU,
	(int)0x82E0001DU,
	(int)0x82E0001BU,
	(int)0x82E01050U,
	(int)0x82E00064U,
	(int)0x82E00067U,
	(int)0x82E01042U,
	(int)0x82E1019AU,
};

static bool npOAuthIsHttpSuccessStatus(int httpStatusCode)
{
	return (httpStatusCode >= 200 && httpStatusCode <= 207);
}

static int npOAuthOnErrorJsonToken(NpOAuthJsonStreamLexer *lexer, int event, int depth, int node,
				   const char *keyName, const NpOAuthJsonStreamToken *token,
				   int arrayIndex, void *context)
{
	(void)lexer;
	(void)node;
	(void)arrayIndex;

	NpOAuthResponseHeader *header = (NpOAuthResponseHeader *)context;
	if (header == NULL)
		return NP_OAUTH_ERR_COMMON_INVALID_ARG;

	if (depth == 1 && event == NP_OAUTH_JSON_EVENT_VALUE && keyName != NULL && token != NULL) {
		if (sceClibStrncmp(keyName, "error", 32U) == 0) {
			header->isError = 1;
			header->hasServerError = 1;
		} else if (sceClibStrncmp(keyName, "error_description", 32U) == 0) {
			header->hasServerError = 1;
			sceClibMemset(header->errorDescription, 0, sizeof(header->errorDescription));
			sceClibStrncpy(header->errorDescription, token->text,
				       sizeof(header->errorDescription) - 1U);
		} else if (sceClibStrncmp(keyName, "error_code", 32U) == 0) {
			header->serverErrorCode = (int)sce_paf_strtol(token->text, NULL, 10);
			header->hasServerError = 1;
			header->isError = 1;
		}
	}
	return 0;
}

static int npOAuthOnAccessTokenJsonToken(NpOAuthJsonStreamLexer *lexer, int event, int depth,
					 int node, const char *keyName,
					 const NpOAuthJsonStreamToken *token, int arrayIndex,
					 void *context)
{
	NpOAuthAccessTokenResponse *resp = (NpOAuthAccessTokenResponse *)context;
	if (resp == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	int ret = 0;
	if (depth == 1 && event == NP_OAUTH_JSON_EVENT_VALUE && keyName != NULL && token != NULL) {
		if (sceClibStrncmp(keyName, "expires_in", 32U) == 0) {
			SceInt64 expiresInSec = sce_paf_atoll(token->text);
			SceRtcTick nowTick;
			ret = sceRtcGetCurrentNetworkTick(&nowTick);
			if (ret >= 0) {
				ret = sceRtcTickAddSeconds(&resp->token.expireTick, &nowTick,
							   expiresInSec);
			}
			resp->header.presentMask |= NP_OAUTH_TOKEN_HAS_EXPIRES_IN;
		} else if (sceClibStrncmp(keyName, "token_type", 32U) == 0) {
			resp->header.presentMask |= NP_OAUTH_TOKEN_HAS_TOKEN_TYPE;
		} else if (sceClibStrncmp(keyName, "access_token", 32U) == 0) {
			sceClibMemset(resp->token.accessToken, 0, sizeof(resp->token.accessToken));
			sceClibStrncpy(resp->token.accessToken, token->text,
				       sizeof(resp->token.accessToken) - 1U);
			resp->header.presentMask |= NP_OAUTH_TOKEN_HAS_ACCESS_TOKEN;
		} else if (sceClibStrncmp(keyName, "scope", 32U) == 0) {
			resp->header.presentMask |= NP_OAUTH_TOKEN_HAS_SCOPE;
		} else if (sceClibStrncmp(keyName, "id_token", 32U) != 0) {
			return npOAuthOnErrorJsonToken(lexer, event, depth, node, keyName, token,
						       arrayIndex, &resp->header);
		}
	}
	return ret;
}

static int npOAuthOnNpssoJsonToken(NpOAuthJsonStreamLexer *lexer, int event, int depth, int node,
				   const char *keyName, const NpOAuthJsonStreamToken *token,
				   int arrayIndex, void *context)
{
	NpOAuthNpssoResponse *resp = (NpOAuthNpssoResponse *)context;
	if (resp == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	if (depth == 1 && event == NP_OAUTH_JSON_EVENT_VALUE && keyName != NULL && token != NULL) {
		if (sceClibStrncmp(keyName, "npsso_token", 32U) == 0) {
			sceClibMemset(resp->npssoToken, 0, 0x401U);
			sceClibStrncpy(resp->npssoToken, token->text, 0x400U);
			resp->header.presentMask |= 1U;
		} else if (sceClibStrncmp(keyName, "id_token", 32U) == 0) {
			return npOAuthDecodeIdToken(token->text, &resp->issuerId);
		} else {
			return npOAuthOnErrorJsonToken(lexer, event, depth, node, keyName, token,
						       arrayIndex, &resp->header);
		}
	}
	return 0;
}

static int npOAuthOnIdTokenJsonToken(NpOAuthJsonLexer *lexer, int event, int depth, int node,
				     const char *keyName, const NpOAuthJsonLexerToken *token,
				     int arrayIndex, void *context)
{
	(void)lexer;
	(void)node;
	(void)arrayIndex;

	NpOAuthIdTokenParseResult *result = (NpOAuthIdTokenParseResult *)context;
	if (result == NULL)
		return NP_OAUTH_ERR_COMMON_INVALID_ARG;

	if (depth == 1 && event == NP_OAUTH_JSON_EVENT_VALUE && keyName != NULL && token != NULL) {
		if (sceClibStrncasecmp(keyName, "env_iss_id", 32U) == 0) {
			int issuerId = (int)sceClibStrtoll(token->text, NULL, 10);
			if (result->outIssuerId != NULL)
				*result->outIssuerId = issuerId;
			result->header.presentMask |= 1U;
		}
	}
	return 0;
}

void npOAuthHandleFatalError(int errorCode)
{
	for (SceSize i = 0; i < sizeof(kNpOAuthFatalSessionErrors) / sizeof(kNpOAuthFatalSessionErrors[0]); ++i) {
		if (errorCode == kNpOAuthFatalSessionErrors[i])
			npAccountClearSessionWithResult(errorCode);
	}
}

int npOAuthMapServerError(int httpStatusCode, int hasServerError, unsigned int serverErrorCode)
{
	unsigned int mappedError;

	if (httpStatusCode == 410 || httpStatusCode == 503) {
		mappedError = (unsigned int)httpStatusCode | NP_OAUTH_HTTP_ERR_BASE;
	} else if (hasServerError != 0) {
		if (serverErrorCode == 0xFFFFFFFFU) {
			mappedError = NP_OAUTH_SERVER_ERR_GENERIC;
			if (httpStatusCode >= 100 && httpStatusCode <= 599)
				mappedError = (unsigned int)httpStatusCode | NP_OAUTH_HTTP_ERR_BASE;
		} else {
			mappedError = NP_OAUTH_SERVER_ERR_GENERIC;
			if (serverErrorCode < 0x10000U)
				mappedError = serverErrorCode | NP_OAUTH_SERVER_ERR_BASE;
		}
	} else {
		mappedError = NP_OAUTH_SERVER_ERR_GENERIC;
		if (httpStatusCode >= 100 && httpStatusCode <= 599)
			mappedError = (unsigned int)httpStatusCode | NP_OAUTH_HTTP_ERR_BASE;
	}

	npOAuthHandleFatalError((int)mappedError);
	return (int)mappedError;
}

int npOAuthReadAccessTokenResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				   SceUInt64 contentLength,
				   NpOAuthAccessTokenResponse *outResponse)
{
	NpOAuthJsonStreamLexer lexer;
	char keyBuf[128];
	char chunkBuf[512];
	char valueBuf[1536];

	sceClibMemset(keyBuf, 0, sizeof(keyBuf));
	sceClibMemset(valueBuf, 0, sizeof(valueBuf));

	SceSize remaining = (isChunked != 0) ? sizeof(chunkBuf) : (SceSize)contentLength;
	outResponse->header.httpStatusCode = httpStatusCode;
	if (!npOAuthIsHttpSuccessStatus(httpStatusCode))
		outResponse->header.isError = 1;

	int ret = SceNpCommonPs4_CAFD09C2(&lexer, &kNpOAuthAccessTokenJsonConfig, keyBuf,
					  sizeof(keyBuf), valueBuf, sizeof(valueBuf),
					  npOAuthOnAccessTokenJsonToken, outResponse);
	if (ret < 0)
		return ret;

	do {
		if (request->aborted != 0)
			return NP_OAUTH_ERR_ABORTED;

		SceSize readSize = (remaining > sizeof(chunkBuf)) ? sizeof(chunkBuf) : remaining;
		int readBytes = sceHttpReadData(request->httpReqId, chunkBuf, readSize);
		ret = readBytes;
		if (readBytes < 0) {
			if (readBytes == NP_OAUTH_HTTP_ERR_SSL) {
				int sslErrNum = 0;
				unsigned int sslDetail = 0;
				sceHttpsGetSslError(request->httpReqId, &sslErrNum, &sslDetail);
			}
			return ret;
		}
		if (readBytes == 0)
			break;
		if (isChunked == 0)
			remaining -= (SceSize)readBytes;

		int lexRet = SceNpCommonPs4_D7AB56B8(&lexer, chunkBuf, readBytes);
		if (lexRet < 0)
			return lexRet;
	} while ((isChunked != 0 || remaining != 0) && ret >= 0);

	if (isChunked == 0 && remaining != 0)
		return NP_OAUTH_ERR_INVALID_RESPONSE;

	if (outResponse->header.isError != 0) {
		return npOAuthMapServerError(httpStatusCode, outResponse->header.hasServerError,
					     (unsigned int)outResponse->header.serverErrorCode);
	}

	if ((outResponse->header.presentMask & NP_OAUTH_TOKEN_REQUIRED_MASK) !=
	    NP_OAUTH_TOKEN_REQUIRED_MASK) {
		return NP_OAUTH_ERR_INVALID_RESPONSE;
	}

	return (ret >= 0) ? 0 : ret;
}

int npOAuthReadNpssoResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
			     SceUInt64 contentLength, NpOAuthNpssoResponse *outResponse)
{
	NpOAuthAllocator *allocator = npOAuthGetContext()->allocator;
	NpOAuthJsonStreamLexer lexer;
	char keyBuf[128];
	char chunkBuf[512];
	sceClibMemset(keyBuf, 0, sizeof(keyBuf));

	char *valueBuf = (char *)allocator->allocate(NP_OAUTH_JWT_BUF_SIZE, allocator->context);
	if (valueBuf == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(valueBuf, 0, NP_OAUTH_JWT_BUF_SIZE);
	SceSize remaining = (isChunked != 0) ? sizeof(chunkBuf) : (SceSize)contentLength;
	outResponse->header.httpStatusCode = httpStatusCode;
	if (!npOAuthIsHttpSuccessStatus(httpStatusCode))
		outResponse->header.isError = 1;

	int ret = SceNpCommonPs4_CAFD09C2(&lexer, &kNpOAuthNpssoJsonConfig, keyBuf, sizeof(keyBuf),
					  valueBuf, NP_OAUTH_JWT_BUF_SIZE, npOAuthOnNpssoJsonToken,
					  outResponse);
	if (ret >= 0) {
		bool streamAbortedOrFailed = false;
		do {
			if (request->aborted != 0) {
				ret = NP_OAUTH_ERR_ABORTED;
				streamAbortedOrFailed = true;
				break;
			}

			SceSize readSize = (remaining > sizeof(chunkBuf)) ? sizeof(chunkBuf)
									  : remaining;
			int readBytes = sceHttpReadData(request->httpReqId, chunkBuf, readSize);
			ret = readBytes;
			if (readBytes < 0) {
				if (readBytes == NP_OAUTH_HTTP_ERR_SSL) {
					int sslErrNum = 0;
					unsigned int sslDetail = 0;
					sceHttpsGetSslError(request->httpReqId, &sslErrNum,
							    &sslDetail);
				}
				streamAbortedOrFailed = true;
				break;
			}
			if (readBytes == 0)
				break;
			if (isChunked == 0)
				remaining -= (SceSize)readBytes;

			int lexRet = SceNpCommonPs4_D7AB56B8(&lexer, chunkBuf, readBytes);
			if (lexRet < 0) {
				ret = lexRet;
				streamAbortedOrFailed = true;
				break;
			}
		} while ((isChunked != 0 || remaining != 0) && ret >= 0);

		if (!streamAbortedOrFailed) {
			if (isChunked == 0 && remaining != 0) {
				ret = NP_OAUTH_ERR_INVALID_RESPONSE;
			} else if (outResponse->header.isError != 0) {
				ret = npOAuthMapServerError(
					httpStatusCode, outResponse->header.hasServerError,
					(unsigned int)outResponse->header.serverErrorCode);
			} else if ((outResponse->header.presentMask & 1U) != 0) {
				if (ret >= 0)
					ret = 0;
			} else {
				ret = NP_OAUTH_ERR_INVALID_RESPONSE;
			}
		}
	}

	allocator->deallocate(valueBuf, allocator->context);
	return ret;
}

static int npOAuthExtractQueryValue(const char *query, SceSize *ioIndex, char *outValue,
				    SceSize maxLen)
{
	SceSize count = 0;
	while (query[*ioIndex] != '&' && query[*ioIndex] != '\0') {
		if (count >= maxLen)
			return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
		outValue[count++] = query[*ioIndex];
		++(*ioIndex);
	}
	outValue[count] = '\0';
	return 0;
}

int npOAuthParseRedirectLocation(NpOAuthRequest *request, const void *locationHeader,
				 SceSize locationLength, int *outHasErrorCode, int *outErrorCode,
				 int *outHasErrorName, char *outErrorName, SceSize errorNameSize,
				 char *outErrorDesc, SceSize errorDescSize, int *outHasState,
				 char *outState, SceSize stateSize)
{
	(void)request;

	NpOAuthAllocator *allocator = npOAuthGetContext()->allocator;
	if (outHasErrorCode == NULL || outErrorCode == NULL || outHasErrorName == NULL ||
	    outErrorName == NULL || outHasState == NULL || outState == NULL) {
		return NP_OAUTH_ERR_COMMON_INVALID_ARG;
	}
	if (locationLength == 0 || locationLength > NP_OAUTH_REDIRECT_URI_MAX_LEN)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	*outHasErrorCode = 0;
	*outHasErrorName = 0;
	*outHasState = 0;
	*outErrorCode = 0;

	char *urlCopy = (char *)allocator->allocate(locationLength + 1U, allocator->context);
	if (urlCopy == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(urlCopy, 0, locationLength + 1U);
	sceClibMemcpy_safe(urlCopy, locationHeader, locationLength);

	SceSize poolRequired = 0;
	int ret = sceHttpUriParse(NULL, urlCopy, NULL, &poolRequired, 0);
	if (ret >= 0) {
		void *uriPool = allocator->allocate(poolRequired, allocator->context);
		if (uriPool != NULL) {
			SceHttpUriElement uriElem;
			SceSize poolUsed = 0;
			ret = sceHttpUriParse(&uriElem, urlCopy, uriPool, &poolUsed, poolRequired);
			if (ret >= 0 && uriElem.query != NULL) {
				SceSize queryLen = sceClibStrnlen(uriElem.query, locationLength);
				char valueBuf[260];
				for (SceSize i = 0; i < queryLen && uriElem.query[i] != '\0'; ++i) {
					char ch = uriElem.query[i];
					if (ch == '?' || ch == '&')
						continue;

					if (sceClibStrncmp(&uriElem.query[i], "code=", 5U) == 0) {
						i += 5U;
						ret = npOAuthExtractQueryValue(uriElem.query, &i,
									       valueBuf, 256U);
						if (ret < 0)
							break;
					}
					if (sceClibStrncmp(&uriElem.query[i], "error=", 6U) == 0) {
						i += 6U;
						ret = npOAuthExtractQueryValue(uriElem.query, &i,
									       valueBuf, 256U);
						if (ret < 0)
							break;
						*outHasErrorName = 1;
						sceClibStrncpy(outErrorName, valueBuf,
							       errorNameSize);
					}
					if (sceClibStrncmp(&uriElem.query[i], "error_code=",
							   11U) == 0) {
						i += 11U;
						ret = npOAuthExtractQueryValue(uriElem.query, &i,
									       valueBuf, 256U);
						if (ret < 0)
							break;
						*outHasErrorCode = 1;
						*outErrorCode = SceNpCommonPs4_32155BF8(valueBuf,
											NULL, 10);
					}
					if (sceClibStrncmp(&uriElem.query[i], "error_description=",
							   18U) == 0) {
						i += 18U;
						ret = npOAuthExtractQueryValue(uriElem.query, &i,
									       valueBuf, 256U);
						if (ret < 0)
							break;
						sceClibStrncpy(outErrorDesc, valueBuf,
							       errorDescSize);
					}
					if (sceClibStrncmp(&uriElem.query[i], "state=", 6U) == 0) {
						i += 6U;
						ret = npOAuthExtractQueryValue(uriElem.query, &i,
									       valueBuf, 256U);
						if (ret < 0)
							break;
						*outHasState = 1;
						sceClibStrncpy(outState, valueBuf, stateSize - 1U);
					}
				}
			}
			allocator->deallocate(uriPool, allocator->context);
		} else {
			ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
		}
	}

	allocator->deallocate(urlCopy, allocator->context);
	return ret;
}

int npOAuthReadAuthCodeResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				SceUInt64 contentLength, NpOAuthAuthCodeResponse *outResponse)
{
	char *allHeaders = NULL;
	SceSize allHeadersSize = 0;

	outResponse->header.httpStatusCode = httpStatusCode;
	if (httpStatusCode != 302)
		outResponse->header.isError = 1;

	int ret = sceHttpGetAllResponseHeaders(request->httpReqId, &allHeaders, &allHeadersSize);
	if (ret < 0)
		return ret;

	const char *locationVal = NULL;
	SceSize locationLen = 0;
	ret = sceHttpParseResponseHeader(allHeaders, allHeadersSize, "Location", &locationVal,
					 &locationLen);
	if (ret >= 0) {
		int hasErrorCode = 0;
		int hasErrorName = 0;
		int hasState = 0;
		int parsedErrorCode = 0;
		char errorNameBuf[64];
		char stateBuf[32];

		int parseRet = npOAuthParseRedirectLocation(
			request, locationVal, locationLen, &hasErrorCode, &parsedErrorCode,
			&hasErrorName, errorNameBuf, sizeof(errorNameBuf),
			outResponse->header.errorDescription, 256U, &hasState, stateBuf,
			sizeof(stateBuf));
		if (parseRet >= 0) {
			if (hasErrorCode != 0) {
				outResponse->header.isError = 1;
				outResponse->header.hasServerError = 1;
				outResponse->header.serverErrorCode = parsedErrorCode;
				return npOAuthMapServerError(
					httpStatusCode, outResponse->header.hasServerError,
					(unsigned int)outResponse->header.serverErrorCode);
			}
			if (hasErrorName == 0) {
				const char *grantCodeVal = NULL;
				SceSize grantCodeLen = 0;
				ret = sceHttpParseResponseHeader(allHeaders, allHeadersSize,
								 "X-NP-GRANT-CODE", &grantCodeVal,
								 &grantCodeLen);
				if (ret >= 0) {
					if (grantCodeLen > 128U)
						return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
					sceClibStrncpy(outResponse->authCode.code, grantCodeVal,
						       grantCodeLen);
					return 0;
				}
				return ret;
			}
			if (outResponse->header.isError == 0)
				return NP_OAUTH_ERR_INVALID_RESPONSE;
			return npOAuthMapServerError(
				httpStatusCode, outResponse->header.hasServerError,
				(unsigned int)outResponse->header.serverErrorCode);
		}
		if (outResponse->header.isError == 0)
			return parseRet;
		return npOAuthMapServerError(httpStatusCode, outResponse->header.hasServerError,
					     (unsigned int)outResponse->header.serverErrorCode);
	}

	if (outResponse->header.isError != 0) {
		NpOAuthJsonStreamLexer lexer;
		char keyBuf[132];
		char chunkBuf[512];
		char valueBuf[1536];
		SceSize remaining = (isChunked != 0) ? sizeof(chunkBuf) : (SceSize)contentLength;

		sceClibMemset(keyBuf, 0, 128U);
		sceClibMemset(valueBuf, 0, sizeof(valueBuf));
		ret = SceNpCommonPs4_CAFD09C2(&lexer, &kNpOAuthErrorJsonConfig, keyBuf, 128U,
					      valueBuf, sizeof(valueBuf), npOAuthOnErrorJsonToken,
					      &outResponse->header);
		if (ret < 0)
			return ret;

		int lastRead = 0;
		for (;;) {
			if (request->aborted != 0)
				return NP_OAUTH_ERR_ABORTED;

			SceSize readSize = (remaining > sizeof(chunkBuf)) ? sizeof(chunkBuf)
									  : remaining;
			lastRead = sceHttpReadData(request->httpReqId, chunkBuf, readSize);
			if (lastRead < 0) {
				if (lastRead == NP_OAUTH_HTTP_ERR_SSL) {
					int sslErrNum = 0;
					unsigned int sslDetail = 0;
					sceHttpsGetSslError(request->httpReqId, &sslErrNum,
							    &sslDetail);
				}
				return lastRead;
			}
			if (lastRead == 0)
				break;
			if (isChunked == 0 && lastRead > 0)
				remaining -= (SceSize)lastRead;
			if (lastRead > 0) {
				int lexRet = SceNpCommonPs4_D7AB56B8(&lexer, chunkBuf, lastRead);
				if (lexRet < 0)
					return lexRet;
			}
			if (isChunked == 0 && remaining == 0)
				break;
		}

		if (outResponse->header.isError != 0) {
			return npOAuthMapServerError(
				httpStatusCode, outResponse->header.hasServerError,
				(unsigned int)outResponse->header.serverErrorCode);
		}
		return lastRead;
	}

	return ret;
}

int npOAuthParseIdTokenPayload(const char *payloadJson, int payloadLength,
			       NpOAuthIdTokenParseResult *outResult)
{
	NpOAuthJsonLexer lexer;
	int ret = SceNpCommonPs4_738FF77D(&lexer, &kNpOAuthIdTokenJsonConfig,
					  npOAuthOnIdTokenJsonToken, outResult);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_6C941C30(&lexer, payloadJson, payloadLength);
	if (ret < 0)
		return ret;

	return ((outResult->header.presentMask & 1U) != 0) ? 0 : NP_OAUTH_ERR_INVALID_RESPONSE;
}

int npOAuthParseAccessTokenResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				    SceUInt64 contentLength,
				    NpOAuthAccessTokenResponse *outResponse)
{
	NpOAuthAccessTokenResponse parsed;
	sceClibMemset(&parsed, 0, sizeof(parsed));

	int ret = npOAuthReadAccessTokenResponse(request, httpStatusCode, isChunked, contentLength,
						 &parsed);
	if (ret >= 0) {
		outResponse->token = parsed.token;
		outResponse->header = parsed.header;
	}
	return ret;
}

int npOAuthDecodeIdToken(const char *jwtToken, int *outIssuerId)
{
	NpOAuthAllocator *allocator = npOAuthGetContext()->allocator;
	char *encodedPayload = (char *)allocator->allocate(NP_OAUTH_JWT_BUF_SIZE,
							   allocator->context);
	if (encodedPayload == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(encodedPayload, 0, NP_OAUTH_JWT_BUF_SIZE);
	char *decodedPayload = (char *)allocator->allocate(NP_OAUTH_JWT_BUF_SIZE,
							   allocator->context);
	int ret;
	if (decodedPayload == NULL) {
		allocator->deallocate(encodedPayload, allocator->context);
		return NP_OAUTH_ERR_OUT_OF_MEMORY;
	}

	sceClibMemset(decodedPayload, 0, NP_OAUTH_JWT_BUF_SIZE);
	NpOAuthIdTokenParseResult parseResult;
	sceClibMemset(&parseResult, 0, sizeof(parseResult));
	parseResult.outIssuerId = outIssuerId;

	if (jwtToken == NULL) {
		ret = NP_OAUTH_ERR_COMMON_INVALID_ARG;
	} else {
		ret = 0;
		int dotCount = 0;
		SceSize outPos = 0;
		for (SceSize i = 0; i < NP_OAUTH_JWT_BUF_SIZE && jwtToken[i] != '\0'; ++i) {
			char ch = jwtToken[i];
			if (ch == '.') {
				++dotCount;
			} else if (dotCount == 1) {
				if (outPos >= NP_OAUTH_JWT_BUF_SIZE - 1U) {
					ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
					break;
				}
				encodedPayload[outPos++] = ch;
			} else if (dotCount == 2) {
				break;
			} else if (dotCount != 0) {
				ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
			}
		}
	}

	if (ret >= 0) {
		SceSize encodedLen = sceClibStrnlen(encodedPayload, NP_OAUTH_JWT_BUF_SIZE);
		int decodedCap = SceNpCommonPs4_2E65CBA6(encodedPayload, encodedLen);
		int decodedLen = 0;
		if ((SceSize)decodedCap <= NP_OAUTH_JWT_BUF_SIZE) {
			sceClibMemset(decodedPayload, 0, (SceSize)decodedCap);
			int decRet = SceNpCommonPs4_4A7DB2EF(encodedPayload, encodedLen,
							     (unsigned char *)decodedPayload,
							     NP_OAUTH_JWT_BUF_SIZE);
			if (decRet >= 0) {
				decodedLen = decRet;
				ret = 0;
			} else {
				ret = decRet;
			}
		} else {
			ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
		}

		if (ret >= 0) {
			allocator->deallocate(encodedPayload, allocator->context);
			encodedPayload = NULL;
			ret = npOAuthParseIdTokenPayload(decodedPayload, decodedLen, &parseResult);
		}
	}

	allocator->deallocate(decodedPayload, allocator->context);
	if (encodedPayload != NULL)
		allocator->deallocate(encodedPayload, allocator->context);
	return ret;
}
