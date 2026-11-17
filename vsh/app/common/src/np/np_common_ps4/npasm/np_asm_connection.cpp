/*
 * SceNpCommonPs4, NpAsm: the HTTP connection to the ASM server. Every call of a request makes a template, a connection
 * and a request of its own, sends it with the title's access token, and reads the answer's JSON through the lexer.
 */

#include "np_asm.h"

extern "C" {

SceInt32 g_npAsmConnectionState;		/* 0x8102b088 */
SceKernelLwMutexWork g_npAsmConnectionMutex;	/* 0x8102b9e0 */

/* the lexer's paths (0x8102907c is the empty string every end node carries): a string under any key of the first
   level is the title token ("npTitleToken", "exp") */
const struct NpCommonPs4LexerNode g_npAsmTokenNodes0[] = {	/* 0x81029090 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmTokenNodes1[] = {	/* 0x810290ac */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "npTitleToken", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "exp", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode *const g_npAsmTokenLevels[] = {	/* 0x810290e4 */
	g_npAsmTokenNodes0,
	g_npAsmTokenNodes1
};

const struct NpCommonPs4LexerConfig g_npAsmTokenConfig = { g_npAsmTokenLevels, 2 };	/* 0x810290ec */

const struct NpCommonPs4LexerNode g_npAsmBaseUrlNodes0[] = {	/* 0x810290f4 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const char g_npAsmBaseUrlKey[] = "url";		/* 0x8102910c */

const struct NpCommonPs4LexerNode g_npAsmBaseUrlNodes1[] = {	/* 0x81029110 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, g_npAsmBaseUrlKey, 0 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode *const g_npAsmBaseUrlLevels[] = {	/* 0x81029128 */
	g_npAsmBaseUrlNodes0,
	g_npAsmBaseUrlNodes1
};

const struct NpCommonPs4LexerConfig g_npAsmBaseUrlConfig = { g_npAsmBaseUrlLevels, 2 };	/* 0x81029130 */

const struct NpCommonPs4LexerNode g_npAsmNpComInfoNodes0[] = {	/* 0x81029138 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmNpComInfoNodes1[] = {	/* 0x8102915c */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "npComInfo", 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmNpComInfoNodes2[] = {	/* 0x81029174 */
	{ NP_COMMON_PS4_LEXER_NODE_ELEMENT, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmNpComInfoNodes3[] = {	/* 0x810291d0 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "npComId", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "encryptedNpPassPhrase", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "npPassPhraseIV", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "npServiceLabel", 0 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode *const g_npAsmNpComInfoLevels[] = {	/* 0x8102920c */
	g_npAsmNpComInfoNodes0,
	g_npAsmNpComInfoNodes1,
	g_npAsmNpComInfoNodes2,
	g_npAsmNpComInfoNodes3
};

const struct NpCommonPs4LexerConfig g_npAsmNpComInfoConfig = { g_npAsmNpComInfoLevels, 4 };	/* 0x8102921c */

const struct NpCommonPs4LexerNode g_npAsmRelatedNodes0[] = {	/* 0x81029224 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmRelatedNodes1[] = {	/* 0x81029258 */
	{ NP_COMMON_PS4_LEXER_NODE_KEY, "relatedGameNpTitleIds", 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode g_npAsmRelatedNodes2[] = {	/* 0x81029270 */
	{ NP_COMMON_PS4_LEXER_NODE_ELEMENT, NULL, 1 },
	{ NP_COMMON_PS4_LEXER_NODE_END, "", 0 }
};

const struct NpCommonPs4LexerNode *const g_npAsmRelatedLevels[] = {	/* 0x81029288 */
	g_npAsmRelatedNodes0,
	g_npAsmRelatedNodes1,
	g_npAsmRelatedNodes2
};

const struct NpCommonPs4LexerConfig g_npAsmRelatedConfig = { g_npAsmRelatedLevels, 3 };	/* 0x81029294 */

/* what the lexer's callbacks of the npComInfo and relatedGameNpTitleIds answers keep: the list being filled, how many
   entries the answer has, whether the key and its array were reached, the entry being read and an error of a field */
struct NpAsmListContext {
	void *list;
	SceUInt32 total;
	SceUInt8 inKey;
	SceUInt8 inArray;
	SceUInt32 index;
	int error;
};

/* where the answer of baseUrls puts the url */
struct NpAsmBaseUrlContext {
	char *buffer;
	SceSize size;
};

int npAsmConnectionLockCreate(void);
int npAsmConnectionLockDestroy(void);
int npAsmConnectionNew(int httpContextId, struct NpCommonPs4Allocator *allocator, struct NpAsmConnection **pConnection);
int npAsmConnectionDelete(struct NpAsmConnection *connection);
int npAsmConnectionOpen(struct NpAsmConnection *connection, const char *url, int method, SceSize contentLength,
	const char *accessToken, const char *titleToken);
unsigned int npAsmConnectionGetSslError(int requestId, unsigned int defaultResult);
int npAsmConnectionSend(struct NpAsmConnection *connection, const void *body, SceSize bodySize, char *buffer,
	SceSize bufferSize, SceSize *pLength, int *pStatus);
int npAsmConnectionRequestTitleToken(struct NpAsmConnection *connection, const char *titleId, const SceUInt8 *titleSecret,
	const char *accessToken, struct NpAsmToken *tokenOut);
int npAsmConnectionRequestBaseUrl(struct NpAsmConnection *connection, const char *titleId, const char *serviceName,
	const char *accessToken, const char *titleToken, char *buffer, int size);
int npAsmConnectionRequestNpComInfo(struct NpAsmConnection *connection, const char *titleId, const char *npComId,
	const char *accessToken, const char *titleToken, struct NpAsmNpComInfo **pList, unsigned int *pCount);
int npAsmConnectionRequestRelatedTitleIds(struct NpAsmConnection *connection, const char *id, const char *serviceName,
	const char *accessToken, const char *titleToken, void **pList, unsigned int *pCount);
int npAsmConnectionAbortRequest(struct NpAsmConnection *connection);
int npAsmConnectionGetResponseHeader(struct NpAsmConnection *connection, const char *name, void *buffer, SceSize bufferSize,
	SceSize *pSize);
int npAsmConnectionParseTitleToken(const char *json, int size, struct NpAsmToken *tokenOut);
int npAsmConnectionParseBaseUrl(const char *json, int size, char *buffer, int bufferSize);
int npAsmConnectionParseNpComInfo(const char *json, int size, struct NpCommonPs4Allocator *allocator,
	struct NpAsmNpComInfo **pList, unsigned int *pCount);
int npAsmConnectionParseRelatedTitleIds(const char *json, int size, struct NpCommonPs4Allocator *allocator, void **pList,
	unsigned int *pCount);
int npAsmConnectionOnTitleToken(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionOnBaseUrl(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionOnNpComInfoCount(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionOnNpComInfo(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionOnRelatedCount(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionOnRelated(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npAsmConnectionCheckEscape(const char *name, SceSize length, char *pNeedsEscape, SceSize *pLength);
int npAsmConnectionEscape(char *dst, SceSize dstSize, const char *src, SceSize srcLength);

/*SORT-START*/
int npAsmConnectionInit(void)
{
	int result;

	if (g_npAsmConnectionState != 0)
		return NP_ASM_CONNECTION_ERROR_ALREADY_INITIALIZED;
	result = npAsmConnectionLockCreate();
	if (result >= 0)
		g_npAsmConnectionState = 1;
	return result;
}

int npAsmConnectionTerm(void)
{
	int result;

	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	result = npAsmConnectionLockDestroy();
	g_npAsmConnectionState = 0;
	return result;
}

int npAsmConnectionCreate(int httpContextId, struct NpCommonPs4Allocator *allocator, void **pConnection)
{
	int result;

	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	result = NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
	if (allocator != NULL)
		return npAsmConnectionNew(httpContextId, allocator, (struct NpAsmConnection **)pConnection);
	return result;
}

int npAsmConnectionDestroy(void *connection)
{
	if (g_npAsmConnectionState != 0)
		return npAsmConnectionDelete((struct NpAsmConnection *)connection);
	return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
}

int npAsmConnectionGetTitleToken(void *connection, const void *titleId, const void *titleSecret, const void *accessToken,
	void *tokenOut)
{
	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	if (titleId != NULL && titleSecret != NULL && accessToken != NULL)
		return npAsmConnectionRequestTitleToken((struct NpAsmConnection *)connection, (const char *)titleId,
			(const SceUInt8 *)titleSecret, (const char *)accessToken, (struct NpAsmToken *)tokenOut);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

int npAsmConnectionGetBaseUrl(void *connection, const void *titleId, const void *serviceName, const void *accessToken,
	const void *tokenEntry, void *buffer, int size)
{
	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	if (titleId != NULL && serviceName != NULL && accessToken != NULL && tokenEntry != NULL)
		return npAsmConnectionRequestBaseUrl((struct NpAsmConnection *)connection, (const char *)titleId,
			(const char *)serviceName, (const char *)accessToken, (const char *)tokenEntry, (char *)buffer, size);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

int npAsmConnectionGetNpComInfo(void *connection, const char *titleId, const char *npComId, const void *accessToken,
	const void *tokenEntry, struct NpAsmNpComInfo **pList, unsigned int *pCount)
{
	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	if (titleId != NULL && npComId != NULL && accessToken != NULL && tokenEntry != NULL)
		return npAsmConnectionRequestNpComInfo((struct NpAsmConnection *)connection, titleId, npComId,
			(const char *)accessToken, (const char *)tokenEntry, pList, pCount);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

int npAsmConnectionGetRelatedTitleIds(void *connection, const char *id, const char *serviceName, const void *accessToken,
	const void *tokenEntry, void **pList, unsigned int *pCount)
{
	if (g_npAsmConnectionState == 0)
		return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
	if (id != NULL && serviceName != NULL && accessToken != NULL && tokenEntry != NULL)
		return npAsmConnectionRequestRelatedTitleIds((struct NpAsmConnection *)connection, id, serviceName,
			(const char *)accessToken, (const char *)tokenEntry, pList, pCount);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

int npAsmConnectionAbort(void *connection)
{
	if (g_npAsmConnectionState != 0)
		return npAsmConnectionAbortRequest((struct NpAsmConnection *)connection);
	return NP_ASM_CONNECTION_ERROR_NOT_INITIALIZED;
}

int npAsmConnectionGetHeaderSize(void *connection, const char *name, unsigned int *pSize)
{
	if (name != NULL && pSize != NULL)
		return npAsmConnectionGetResponseHeader((struct NpAsmConnection *)connection, name, NULL, 0, pSize);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

int npAsmConnectionGetHeader(void *connection, const char *name, char *buffer, unsigned int size)
{
	if (name != NULL && buffer != NULL && size != 0)
		return npAsmConnectionGetResponseHeader((struct NpAsmConnection *)connection, name, buffer, size, NULL);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}

__attribute__((noinline)) int npAsmConnectionLockCreate(void)
{
	return SceNpCommonPs4_4858EFB6(&g_npAsmConnectionMutex, "SceNpAsmLock", 1);
}

__attribute__((noinline)) int npAsmConnectionLockDestroy(void)
{
	SceNpCommonPs4_29895EBB(&g_npAsmConnectionMutex);
	return 0;
}

__attribute__((noinline)) int npAsmConnectionNew(int httpContextId, struct NpCommonPs4Allocator *allocator,
	struct NpAsmConnection **pConnection)
{
	struct NpAsmConnection *connection;
	int result;

	result = 0;
	connection = (struct NpAsmConnection *)SceNpCommonPs4_AA980113(allocator, sizeof(struct NpAsmConnection));
	if (connection == NULL)
		return NP_ASM_CONNECTION_ERROR_OUT_OF_MEMORY;
	sceClibMemset(connection, 0, sizeof(struct NpAsmConnection));
	connection->self = connection;
	connection->httpContextId = httpContextId;
	connection->allocator = allocator;
	connection->requestId = 0;
	connection->aborted = 0;
	if (pConnection != NULL)
		*pConnection = connection;
	return result;
}

__attribute__((noinline)) int npAsmConnectionDelete(struct NpAsmConnection *connection)
{
	struct NpCommonPs4Allocator *allocator;
	int result;

	if (connection == NULL || connection->self != connection)
		return NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	if (connection->requestId != 0) {
		sceHttpDeleteRequest(connection->requestId);
		connection->requestId = 0;
	}
	if (connection->connectionId != 0) {
		sceHttpDeleteConnection(connection->connectionId);
		connection->connectionId = 0;
	}
	if (connection->templateId != 0) {
		sceHttpDeleteTemplate(connection->templateId);
		connection->templateId = 0;
	}
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	allocator = connection->allocator;
	result = 0;
	connection->self = NULL;
	SceNpCommonPs4_6B428829(allocator, connection);
	return result;
}

/* the template, connection and request of one call, with its headers */
__attribute__((noinline)) int npAsmConnectionOpen(struct NpAsmConnection *connection, const char *url, int method,
	SceSize contentLength, const char *accessToken, const char *titleToken)
{
	int result;
	const char *token;
	char version[8];
	char userAgent[32];
	char authorization[264];

	sceClibMemset(version, 0, 8);
	SceNpCommon_0624AE44(version);
	sceClibMemset(userAgent, 0, 32);
	sceClibSnprintf(userAgent, 32, "AsmClient/%s", version);
	result = sceHttpCreateTemplate(userAgent, 2, 0);
	if (result >= 0) {
		SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
		connection->templateId = result;
		SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
		result = sceHttpCreateConnectionWithURL(connection->templateId, url, 1);
		if (result >= 0) {
			token = titleToken;
			SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
			connection->connectionId = result;
			SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
			result = sceHttpCreateRequestWithURL(connection->connectionId, method, url, contentLength);
			if (result >= 0) {
				SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
				connection->requestId = result;
				SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
				result = 0;
				if (contentLength == 0
					|| ((result = sceHttpSetRequestContentLength(connection->requestId, contentLength)) >= 0
						&& (result = sceHttpAddRequestHeader(connection->requestId, "Content-Type", "application/json", 0)) >= 0)) {
					if (accessToken == NULL
						|| (sceClibMemset(authorization, 0, 0x107),
							sceClibSnprintf(authorization, 0x107, "%s %s", "Bearer", accessToken),
							(result = sceHttpAddRequestHeader(connection->requestId, "Authorization", authorization, 0)) >= 0)) {
						if (token == NULL
							|| (result = sceHttpAddRequestHeader(connection->requestId, "X-NP-TITLE-TOKEN", token, 0)) >= 0) {
							if (SceNpCommon_BBC13621() == 1)
								result = SceNpCommonPs4_69090D80(connection->requestId);
						}
					}
				}
			}
		}
	}
	return result;
}

/* the error the TLS layer reports for a failed request, as an NP error (defaultResult when it reports none) */
__attribute__((noinline)) unsigned int npAsmConnectionGetSslError(int requestId, unsigned int defaultResult)
{
	int errNum;
	unsigned int detail;
	int r;
	unsigned int result;
	int code;

	errNum = 0;
	detail = 0;
	r = sceHttpsGetSslError(requestId, &errNum, &detail);
	result = defaultResult;
	if (r >= 0) {
		if (errNum == -2143072160) {
			code = -2141870335;
			if ((detail & 4) == 0) {
				code = -2141870334;
				if ((detail & 0x20) == 0) {
					code = -2141870333;
					if ((detail & 8) == 0) {
						code = -2141870332;
						if ((detail & 0x10) == 0) {
							code = -2141870330;
							if ((detail & 2) != 0)
								code = -2141870331;
						}
					}
				}
			}
			result = code;
		} else if (errNum == -2143072158) {
			result = detail | 0x80410100;
		} else if (errNum < 0) {
			result = errNum;
		}
	}
	return result;
}

/* send the request (with the body, if any), take the status and read the answer into the buffer */
__attribute__((noinline)) int npAsmConnectionSend(struct NpAsmConnection *connection, const void *body, SceSize bodySize,
	char *buffer, SceSize bufferSize, SceSize *pLength, int *pStatus)
{
	int total;
	int result;

	total = 0;
	result = sceHttpSendRequest(connection->requestId, body, bodySize);
	if (result < 0) {
		if (result != -2143088523)
			return result;
		return npAsmConnectionGetSslError(connection->requestId, result);
	}
	result = sceHttpGetStatusCode(connection->requestId, pStatus);
	if (result >= 0) {
		sceClibMemset(buffer, 0, bufferSize);
		do {
			result = sceHttpReadData(connection->requestId, &buffer[total], bufferSize - total);
			if (result < 0) {
				if (result != -2143088523)
					return result;
				return npAsmConnectionGetSslError(connection->requestId, result);
			}
			if (result > 0)
				total += result;
		} while (result > 0);
		if (pLength != NULL)
			*pLength = total;
		return result;
	}
	return result;
}

__attribute__((noinline)) int npAsmConnectionRequestTitleToken(struct NpAsmConnection *connection, const char *titleId,
	const SceUInt8 *titleSecret, const char *accessToken, struct NpAsmToken *tokenOut)
{
	char signing[36];
	SceUInt8 digest[32];
	char environment[20];
	int status;
	int readLength;
	SceRtcTick tick;
	char signature[48];
	char host[48];
	char url[256];
	char body[260];
	char response[2048];
	int result;
	int r;
	SceSize length;
	SceUInt64 seconds;
	SceUInt64 elapsed;

	status = 0;
	readLength = 0;
	if (connection != NULL && connection->self == connection) {
		sceClibMemset(host, 0, 0x30);
		sceClibMemset(environment, 0, 0x11);
		result = sceNpCommonGetNpEnviroment(environment);
		if (result >= 0)
			sceClibSnprintf(host, 0x30, "asm.%s.community.playstation.net", environment);
		if (result >= 0) {
			sceClibMemset(url, 0, 0x100);
			sceClibSnprintf(url, 0x100, "%s://%s/asm/%s/apps/%s/npTitleToken", "https", host, "v1", titleId);
			sceClibMemset(body, 0, 0x100);
			result = sceRtcGetCurrentRetainedNetworkTick(&tick);
			if (result >= 0) {
				tick.tick -= 62135596800000000LL;
				elapsed = tick.tick / 1000;
				seconds = elapsed / 1000;
				sceClibMemset(signing, 0, 0x24);
				sceClibSnprintf(signing, 0x24, "%s.%lld", titleId, seconds);
				length = sceClibStrnlen(signing, 0xFFFFFFFF);
				r = npAsmHmacSha256(titleSecret, 128, signing, length, digest);
				if (r >= 0 && signature != NULL) {
					sceClibMemset(signature, 0, 0x2D);
					r = SceNpCommonPs4_EC456A83(digest, (unsigned char *)signature, 32);
				}
				result = r;
				if (r >= 0) {
					sceClibSnprintf(body, 0x100, "\t\t{\"%s\":%lld,\"%s\":\"%s\"} \t\t", "requestTimestamp", seconds,
						"signature", signature);
					length = sceClibStrnlen(body, 0xFFFFFFFF);
					result = npAsmConnectionOpen(connection, url, 1, length, accessToken, NULL);
					if (result >= 0) {
						result = npAsmConnectionSend(connection, body, length, response, 0x800, (SceSize *)&readLength, &status);
						if (result >= 0) {
							if (status < 200 || status >= 300)
								result = SceNpCommonPs4_FB63A152(status, response, readLength);
							else
								result = npAsmConnectionParseTitleToken(response, readLength, tokenOut);
						}
					}
				}
			}
		}
	} else {
		result = NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	}
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	if (connection->aborted != 0)
		result = NP_ASM_CONNECTION_ERROR_ABORTED;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	return result;
}

__attribute__((noinline)) int npAsmConnectionRequestBaseUrl(struct NpAsmConnection *connection, const char *titleId,
	const char *serviceName, const char *accessToken, const char *titleToken, char *buffer, int size)
{
	int result;
	int status;
	int readLength;
	SceSize length;
	SceSize nameLength;
	char *escaped;
	SceUInt8 needsEscape;
	char environment[17];
	char host[48];
	char url[260];
	char response[2048];

	needsEscape = 0;
	status = 0;
	readLength = 0;
	length = 0;
	if (connection == NULL || connection->self != connection) {
		result = NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
		escaped = NULL;
		goto finish;
	}
	nameLength = sceClibStrnlen(serviceName, 0x80);
	result = npAsmConnectionCheckEscape(serviceName, nameLength, (char *)&needsEscape, &length);
	escaped = NULL;
	if (result >= 0) {
		escaped = NULL;
		if (needsEscape == 0)
			goto build;
		escaped = (char *)SceNpCommonPs4_AA980113(connection->allocator, length);
		if (escaped == NULL) {
			result = NP_ASM_CONNECTION_ERROR_OUT_OF_MEMORY;
			goto finish;
		}
		sceClibMemset(escaped, 0, length);
		result = npAsmConnectionEscape(escaped, length, serviceName, nameLength);
		if (result >= 0) {
build:
			result = SceNpCommon_14BC2B99(NULL, 0, serviceName, nameLength, &length);
			if (result >= 0) {
				if (nameLength == length)
					sceClibMemset(host, 0, 0x30);
				sceClibMemset(environment, 0, 0x11);
				result = sceNpCommonGetNpEnviroment(environment);
				if (result >= 0)
					sceClibSnprintf(host, 0x30, "asm.%s.community.playstation.net", environment);
				if (result >= 0) {
					sceClibMemset(url, 0, 0x100);
					if (escaped != NULL)
						serviceName = escaped;
					sceClibSnprintf(url, 0x100, "%s://%s/asm/%s/apps/%s/baseUrls/%s", "https", host, "v1", titleId,
						serviceName);
					result = npAsmConnectionOpen(connection, url, 0, 0, accessToken, titleToken);
					if (result >= 0) {
						result = npAsmConnectionSend(connection, NULL, 0, response, 0x800, (SceSize *)&readLength, &status);
						if (result >= 0) {
							if (status < 200 || status >= 300)
								result = SceNpCommonPs4_FB63A152(status, response, readLength);
							else
								result = npAsmConnectionParseBaseUrl(response, readLength, buffer, size);
						}
					}
				}
			}
		}
	}
finish:
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	if (connection->aborted != 0)
		result = NP_ASM_CONNECTION_ERROR_ABORTED;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	if (escaped != NULL && connection != NULL)
		SceNpCommonPs4_6B428829(connection->allocator, escaped);
	return result;
}

__attribute__((noinline)) int npAsmConnectionRequestNpComInfo(struct NpAsmConnection *connection, const char *titleId,
	const char *npComId, const char *accessToken, const char *titleToken, struct NpAsmNpComInfo **pList,
	unsigned int *pCount)
{
	int result;
	int status;
	int readLength;
	char environment[20];
	char host[48];
	char url[260];
	char response[2048];

	status = 0;
	readLength = 0;
	if (connection != NULL && connection->self == connection) {
		sceClibMemset(host, 0, 0x30);
		sceClibMemset(environment, 0, 0x11);
		result = sceNpCommonGetNpEnviroment(environment);
		if (result >= 0)
			sceClibSnprintf(host, 0x30, "asm.%s.community.playstation.net", environment);
		if (result >= 0) {
			sceClibMemset(url, 0, 0x100);
			sceClibSnprintf(url, 0x100, "%s://%s/asm/%s/apps/%s/npComId/%s", "https", host, "v1", titleId, npComId);
			result = npAsmConnectionOpen(connection, url, 0, 0, accessToken, titleToken);
			if (result >= 0) {
				result = npAsmConnectionSend(connection, NULL, 0, response, 0x800, (SceSize *)&readLength, &status);
				if (result >= 0) {
					if (status < 200 || status >= 300)
						result = SceNpCommonPs4_FB63A152(status, response, readLength);
					else
						result = npAsmConnectionParseNpComInfo(response, readLength, connection->allocator, pList, pCount);
				}
			}
		}
	} else {
		result = NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	}
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	if (connection->aborted != 0)
		result = NP_ASM_CONNECTION_ERROR_ABORTED;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	return result;
}

__attribute__((noinline)) int npAsmConnectionRequestRelatedTitleIds(struct NpAsmConnection *connection, const char *id,
	const char *serviceName, const char *accessToken, const char *titleToken, void **pList, unsigned int *pCount)
{
	int result;
	int status;
	int readLength;
	char environment[20];
	char host[48];
	char url[260];
	char response[2048];

	status = 0;
	readLength = 0;
	if (connection != NULL && connection->self == connection) {
		sceClibMemset(host, 0, 0x30);
		sceClibMemset(environment, 0, 0x11);
		result = sceNpCommonGetNpEnviroment(environment);
		if (result >= 0)
			sceClibSnprintf(host, 0x30, "asm.%s.community.playstation.net", environment);
		if (result >= 0) {
			sceClibMemset(url, 0, 0x100);
			sceClibSnprintf(url, 0x100, "%s://%s/asm/%s/services/%s/relatedGameNpTitleIds?id=%s", "https", host, "v1",
				serviceName, id);
			result = npAsmConnectionOpen(connection, url, 0, 0, accessToken, titleToken);
			if (result >= 0) {
				result = npAsmConnectionSend(connection, NULL, 0, response, 0x800, (SceSize *)&readLength, &status);
				if (result >= 0) {
					if (status < 200 || status >= 300)
						result = SceNpCommonPs4_FB63A152(status, response, readLength);
					else
						result = npAsmConnectionParseRelatedTitleIds(response, readLength, connection->allocator, pList,
							pCount);
				}
			}
		}
	} else {
		result = NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	}
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	if (connection->aborted != 0)
		result = NP_ASM_CONNECTION_ERROR_ABORTED;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	return result;
}

__attribute__((noinline)) int npAsmConnectionAbortRequest(struct NpAsmConnection *connection)
{
	int result;
	int requestId;

	result = 0;
	if (connection == NULL || connection->self != connection)
		return NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	SceNpCommonPs4_F5840D6E(&g_npAsmConnectionMutex);
	requestId = connection->requestId;
	connection->aborted = 1;
	if (requestId != 0)
		result = sceHttpAbortRequest(requestId);
	SceNpCommonPs4_4A1C1C3C(&g_npAsmConnectionMutex);
	return result;
}

/* the value of a header of the answer: its length, and its text as far as the buffer holds it */
__attribute__((noinline)) int npAsmConnectionGetResponseHeader(struct NpAsmConnection *connection, const char *name,
	void *buffer, SceSize bufferSize, SceSize *pSize)
{
	int result;
	SceSize count;
	char *header;
	SceSize headerSize;
	const char *fieldValue;
	SceSize valueLength;

	if (connection != NULL && connection->self == connection) {
		fieldValue = NULL;
		valueLength = 0;
		header = NULL;
		headerSize = 0;
		result = sceHttpGetAllResponseHeaders(connection->requestId, &header, &headerSize);
		if (result >= 0) {
			result = sceHttpParseResponseHeader(header, headerSize, name, &fieldValue, &valueLength);
			if (result >= 0) {
				if (buffer != NULL) {
					count = bufferSize;
					if (valueLength <= bufferSize)
						count = valueLength;
					sceClibMemcpy_safe(buffer, fieldValue, count);
				}
				if (pSize != NULL)
					*pSize = valueLength;
			}
		}
	} else {
		result = NP_ASM_CONNECTION_ERROR_INVALID_CONNECTION;
	}
	return result;
}

__attribute__((noinline)) int npAsmConnectionParseTitleToken(const char *json, int size, struct NpAsmToken *tokenOut)
{
	struct NpCommonPs4Lexer lexer;
	int result;

	result = SceNpCommonPs4_738FF77D(&lexer, &g_npAsmTokenConfig, npAsmConnectionOnTitleToken, tokenOut);
	if (result >= 0)
		result = SceNpCommonPs4_6C941C30(&lexer, json, size);
	return result;
}

__attribute__((noinline)) int npAsmConnectionParseBaseUrl(const char *json, int size, char *buffer, int bufferSize)
{
	struct NpAsmBaseUrlContext context;
	struct NpCommonPs4Lexer lexer;
	int result;

	context.buffer = buffer;
	context.size = bufferSize;
	result = SceNpCommonPs4_738FF77D(&lexer, &g_npAsmBaseUrlConfig, npAsmConnectionOnBaseUrl, &context);
	if (result >= 0)
		result = SceNpCommonPs4_6C941C30(&lexer, json, size);
	return result;
}

__attribute__((noinline)) int npAsmConnectionParseNpComInfo(const char *json, int size,
	struct NpCommonPs4Allocator *allocator, struct NpAsmNpComInfo **pList, unsigned int *pCount)
{
	struct NpAsmListContext context;
	struct NpCommonPs4Lexer lexer;
	void *list;
	int result;
	int count;

	sceClibMemset(&context, 0, 0x14);
	result = SceNpCommonPs4_738FF77D(&lexer, &g_npAsmNpComInfoConfig, npAsmConnectionOnNpComInfoCount, &context);
	if (result < 0)
		goto finish;
	SceNpCommonPs4_6C941C30(&lexer, json, size);
	count = context.total;
	if (count == 0)
		goto assign;
	list = SceNpCommonPs4_AA980113(allocator, 312 * context.total);
	if (list == NULL) {
		result = NP_ASM_CONNECTION_ERROR_OUT_OF_MEMORY;
		goto finish;
	}
	context.list = list;
	sceClibMemset(list, 0, 312 * context.total);
	result = SceNpCommonPs4_738FF77D(&lexer, &g_npAsmNpComInfoConfig, npAsmConnectionOnNpComInfo, &context);
	if (result < 0)
		goto finish;
	result = SceNpCommonPs4_6C941C30(&lexer, json, size);
	if (result < 0)
		goto finish;
	count = context.total;
	if (context.error < 0) {
		result = context.error;
	} else {
assign:
		if (pList != NULL) {
			*pList = (struct NpAsmNpComInfo *)context.list;
			*pCount = count;
		}
	}
finish:
	return result;
}

__attribute__((noinline)) int npAsmConnectionParseRelatedTitleIds(const char *json, int size,
	struct NpCommonPs4Allocator *allocator, void **pList, unsigned int *pCount)
{
	struct NpAsmListContext context;
	struct NpCommonPs4Lexer lexer;
	void *list;
	int result;
	int count;

	result = 0;
	sceClibMemset(&context, 0, 0x14);
	SceNpCommonPs4_738FF77D(&lexer, &g_npAsmRelatedConfig, npAsmConnectionOnRelatedCount, &context);
	SceNpCommonPs4_6C941C30(&lexer, json, size);
	count = context.total;
	if (count == 0) {
assign:
		if (pList != NULL) {
			*pList = context.list;
			*pCount = count;
		}
		goto finish;
	}
	list = SceNpCommonPs4_AA980113(allocator, 16 * context.total);
	if (list == NULL) {
		result = NP_ASM_CONNECTION_ERROR_OUT_OF_MEMORY;
		goto finish;
	}
	context.list = list;
	sceClibMemset(list, 0, 16 * context.total);
	result = SceNpCommonPs4_738FF77D(&lexer, &g_npAsmRelatedConfig, npAsmConnectionOnRelated, &context);
	if (result >= 0) {
		SceNpCommonPs4_6C941C30(&lexer, json, size);
		count = context.total;
		if (context.error < 0) {
			result = context.error;
			goto finish;
		}
		goto assign;
	}
finish:
	return result;
}

/* the answer of npTitleToken: the token ("npTitleToken") and the time it expires ("exp", seconds since 1970) */
__attribute__((noinline)) int npAsmConnectionOnTitleToken(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmToken *tokenOut;
	long long seconds;

	tokenOut = (struct NpAsmToken *)context;
	if (token != NULL) {
		if (token->type == NP_COMMON_PS4_LEXER_TOKEN_STRING) {
			if (sceClibStrncmp(name, "npTitleToken", sceClibStrnlen("npTitleToken", 0xFFFFFFFF)) == 0 && token->text != NULL
				&& tokenOut != NULL) {
				sceClibMemset(tokenOut, 0, 0x410);
				sceClibMemcpy_safe(tokenOut, token->text, 0x400);
			}
		} else if (token->type == NP_COMMON_PS4_LEXER_TOKEN_NUMBER) {
			if (sceClibStrncmp(name, "exp", sceClibStrnlen("exp", 0xFFFFFFFF)) == 0 && token->text != NULL) {
				seconds = sceClibStrtoll(token->text, NULL, 10);
				tokenOut->expires = seconds * 1000000 + 62135596800000000LL;
			}
		}
	}
	return 0;
}

__attribute__((noinline)) int npAsmConnectionOnBaseUrl(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmBaseUrlContext *out;
	SceSize size;

	out = (struct NpAsmBaseUrlContext *)context;
	if (token != NULL && token->type == NP_COMMON_PS4_LEXER_TOKEN_STRING) {
		if (sceClibStrncmp(name, g_npAsmBaseUrlKey, sceClibStrnlen(g_npAsmBaseUrlKey, 0xFFFFFFFF)) == 0
			&& token->text != NULL && out->buffer != NULL) {
			size = out->size;
			if (size != 0)
				sceClibStrncpy(out->buffer, token->text, size);
		}
	}
	return 0;
}

__attribute__((noinline)) int npAsmConnectionOnNpComInfoCount(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmListContext *list;

	list = (struct NpAsmListContext *)context;
	if (event == NP_COMMON_PS4_LEXER_EVENT_KEY && depth == 1) {
		if (sceClibStrncmp(name, "npComInfo", sceClibStrnlen("npComInfo", 0xFFFFFFFF)) == 0)
			list->inKey = 1;
	}
	if (event == NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN && depth == 1)
		list->inArray = 1;
	if (list->inKey != 0 && list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN && depth == 2)
		++list->index;
	if (event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_END && depth == 0) {
		list->inArray = 0;
		list->inKey = 0;
		if (list != NULL)
			list->total = list->index;
		list->index = 0;
	}
	return 0;
}

__attribute__((noinline)) int npAsmConnectionOnNpComInfo(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmListContext *list;
	struct NpAsmNpComInfo *entry;
	int inKey;
	int r;

	list = (struct NpAsmListContext *)context;
	if (list->index <= list->total) {
		if (event == NP_COMMON_PS4_LEXER_EVENT_KEY && depth == 1) {
			if (sceClibStrncmp(name, "npComInfo", sceClibStrnlen("npComInfo", 0xFFFFFFFF)) == 0)
				list->inKey = 1;
		}
		inKey = list->inKey;
		if (event == NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN && depth == 1)
			list->inArray = 1;
		if (inKey != 0) {
			if (list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN && depth == 2)
				++list->index;
			if (list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_VALUE && depth == 3 && token != NULL
				&& token->type == NP_COMMON_PS4_LEXER_TOKEN_STRING) {
				entry = (struct NpAsmNpComInfo *)list->list + (list->index - 1);
				if (sceClibStrncmp(name, "npComId", sceClibStrnlen("npComId", 0xFFFFFFFF)) != 0 || token->text == NULL) {
					if (sceClibStrncmp(name, "encryptedNpPassPhrase", sceClibStrnlen("encryptedNpPassPhrase", 0xFFFFFFFF)) != 0
						|| token->text == NULL) {
						if (sceClibStrncmp(name, "npPassPhraseIV", sceClibStrnlen("npPassPhraseIV", 0xFFFFFFFF)) != 0
							|| token->text == NULL) {
							if (sceClibStrncmp(name, "npServiceLabel", sceClibStrnlen("npServiceLabel", 0xFFFFFFFF)) == 0
								&& token->text != NULL)
								entry->serviceLabel = SceNpCommonPs4_32155BF8(token->text, NULL, 10);
						} else {
							r = SceNpCommonPs4_D082556B(token->text, sceClibStrnlen(token->text, 0xFFFFFFFF), entry->iv, 32);
							if (r >= 0)
								entry->ivSize = r;
							else
								list->error = 0;
						}
					} else {
						r = SceNpCommonPs4_D082556B(token->text, sceClibStrnlen(token->text, 0xFFFFFFFF), entry->passPhrase,
							256);
						if (r >= 0)
							entry->passPhraseSize = r;
						else
							list->error = 0;
					}
				} else {
					SceNpCommon_5BC54899(entry, token->text);
				}
			}
		}
		if (event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_END && depth == 0) {
			list->inKey = 0;
			list->inArray = 0;
			list->index = 0;
		}
	}
	return 0;
}

__attribute__((noinline)) int npAsmConnectionOnRelatedCount(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmListContext *list;

	list = (struct NpAsmListContext *)context;
	if (event == NP_COMMON_PS4_LEXER_EVENT_KEY && depth == 1) {
		if (sceClibStrncmp(name, "relatedGameNpTitleIds", sceClibStrnlen("relatedGameNpTitleIds", 0xFFFFFFFF)) == 0)
			list->inKey = 1;
	}
	if (event == NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN && depth == 1)
		list->inArray = 1;
	if (list->inKey != 0 && list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_VALUE && depth == 2)
		++list->index;
	if (event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_END && depth == 0) {
		list->inArray = 0;
		list->inKey = 0;
		if (list != NULL)
			list->total = list->index;
		list->index = 0;
	}
	return 0;
}

__attribute__((noinline)) int npAsmConnectionOnRelated(struct NpCommonPs4Lexer *lexer, int event, int depth, int node,
	const char *name, const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context)
{
	struct NpAsmListContext *list;
	char *entry;
	int inKey;

	list = (struct NpAsmListContext *)context;
	if (list->index <= list->total) {
		if (event == NP_COMMON_PS4_LEXER_EVENT_KEY && depth == 1) {
			if (sceClibStrncmp(name, "relatedGameNpTitleIds", sceClibStrnlen("relatedGameNpTitleIds", 0xFFFFFFFF)) == 0)
				list->inKey = 1;
		}
		inKey = list->inKey;
		if (event == NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN && depth == 1)
			list->inArray = 1;
		if (inKey != 0) {
			if (list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_VALUE && depth == 2)
				++list->index;
			if (list->inArray != 0 && event == NP_COMMON_PS4_LEXER_EVENT_VALUE && depth == 2 && token != NULL
				&& token->type == NP_COMMON_PS4_LEXER_TOKEN_STRING) {
				entry = (char *)list->list + 16 * (list->index - 1);
				sceClibStrncpy(entry, token->text, sceClibStrnlen(token->text, 0xFFFFFFFF));
			}
		}
		if (event == NP_COMMON_PS4_LEXER_EVENT_OBJECT_END && depth == 0) {
			list->inKey = 0;
			list->inArray = 0;
			list->index = 0;
		}
	}
	return 0;
}

/* whether a service name has to be escaped before it goes into a URL (a space, or a length the escaping changes),
   and the length of its escaped form */
__attribute__((noinline)) int npAsmConnectionCheckEscape(const char *name, SceSize length, char *pNeedsEscape,
	SceSize *pLength)
{
	const SceUInt8 *scan;
	SceSize escaped;
	SceSize i;
	char needs;
	int result;

	needs = 0;
	scan = (const SceUInt8 *)name;
	if (name != NULL && length != 0) {
		escaped = 0;
		result = SceNpCommon_14BC2B99(NULL, 0, name, length, &escaped);
		if (result >= 0) {
			if (escaped - 1 <= length) {
				if (escaped == length) {
					i = 0;
					if (length != 0) {
						do {
							if (*scan == 32) {
								needs = 1;
								break;
							}
							++i;
							++scan;
						} while (i < length);
					}
				}
			} else {
				needs = 1;
			}
			if (pNeedsEscape != NULL)
				*pNeedsEscape = needs;
			if (pLength != NULL)
				*pLength = escaped;
		}
	} else {
		result = NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

__attribute__((noinline)) int npAsmConnectionEscape(char *dst, SceSize dstSize, const char *src, SceSize srcLength)
{
	if (dst != NULL && dstSize != 0 && src != NULL && srcLength != 0)
		return SceNpCommon_14BC2B99(dst, dstSize, src, srcLength, NULL);
	return NP_ASM_CONNECTION_ERROR_INVALID_ARGUMENT;
}
/*SORT-END*/

}
