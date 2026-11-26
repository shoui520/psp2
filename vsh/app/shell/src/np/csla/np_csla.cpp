/*
 * np_csla.cpp: SceNpCsla console lightweight attestation token client (CSLA).
 * Encrypts OpenPsId with AES-256-CBC, signs with HMAC-SHA256, posts to
 * https://csla.<env>.community.playstation.net/csla/ (version 1) /console/lwConsoleToken,
 * parses the JWT exp claim, and caches the console token.
 * 3.65-CEX: 0x813BA81C..0x813BC123
 */
#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/openpsid.h>
#include <libaes.h>
#include <libmt19937.h>
#include <libhttp.h>
#include "../oauth/np_oauth_internal.h"

#define NP_CSLA_ERR_ALREADY_INITIALIZED		(-2141870783)	/* 0x8055A541 */
#define NP_CSLA_ERR_NOT_INITIALIZED		(-2141870782)	/* 0x8055A542 */
#define NP_CSLA_ERR_INVALID_ARGUMENT		(-2141870781)	/* 0x8055A543 */
#define NP_CSLA_ERR_OUT_OF_MEMORY		(-2141870780)	/* 0x8055A544 */
#define NP_CSLA_ERR_INVALID_REQUEST_ID		(-2141870779)	/* 0x8055A545 */
#define NP_CSLA_ERR_CONTEXT_ALREADY_EXISTS	(-2141870778)	/* 0x8055A546 */
#define NP_CSLA_ERR_REQUEST_IN_USE		(-2141870777)	/* 0x8055A547 */
#define NP_CSLA_ERR_ABORTED			(-2141870776)	/* 0x8055A548 */

#define NP_CSLA_HTTP_ERR_ALREADY_INITIALIZED	(-2141870846)	/* 0x8055A502 */
#define NP_CSLA_HTTP_ERR_NOT_INITIALIZED	(-2141870845)	/* 0x8055A503 */
#define NP_CSLA_HTTP_ERR_INVALID_ARGUMENT	(-2141870844)	/* 0x8055A504 */
#define NP_CSLA_HTTP_ERR_OUT_OF_MEMORY		(-2141870843)	/* 0x8055A505 */
#define NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL	(-2141870842)	/* 0x8055A506 */
#define NP_CSLA_HTTP_ERR_INVALID_HANDLE		(-2141870841)	/* 0x8055A507 */
#define NP_CSLA_HTTP_ERR_ABORTED		(-2141870840)	/* 0x8055A508 */

#define NP_CSLA_TOKEN_BUF_SIZE			6145U
#define NP_CSLA_TOKEN_CACHE_COPY_SIZE		6160U
#define NP_CSLA_UNIX_EPOCH_RTC_TICKS		62135596800000000ULL

#define NP_CSLA_HTTP_ERR_NO_CONTENT_LENGTH	(-2143088526)
#define NP_CSLA_HTTP_ERR_CHUNKED_ENCODING	(-2143088527)

/* 0x81549254: CSLA AES-256 / HMAC-SHA256 key for np / mgmt environments */
static const SceUInt8 kCslaProdKey[32] = {
	0xea, 0x11, 0x3e, 0x3c, 0x77, 0x58, 0x8c, 0x21,
	0x0a, 0xc4, 0x0c, 0x71, 0x2b, 0xed, 0x0e, 0xa6,
	0x26, 0x93, 0xe8, 0xa5, 0xee, 0x18, 0x08, 0xba,
	0x0e, 0xfa, 0x9e, 0x2c, 0x4f, 0xf4, 0xe1, 0xaa
};

/* 0x81549274: CSLA AES-256 / HMAC-SHA256 key for non-production environments */
static const SceUInt8 kCslaDevKey[32] = {
	0x04, 0x5d, 0x3c, 0x86, 0x9c, 0xfc, 0x4d, 0x53,
	0x87, 0x64, 0xd7, 0xed, 0xfb, 0x1a, 0x90, 0x13,
	0xbb, 0xe6, 0x04, 0x0f, 0x0b, 0x73, 0xa7, 0x14,
	0xe6, 0x87, 0x25, 0xe7, 0xc3, 0x43, 0x33, 0x2d
};

struct NpCslaChunkLexerToken {
	int type;
	char *text;
	int complete;
};

struct NpCslaChunkLexer;
typedef int (*NpCslaChunkLexerCallback)(NpCslaChunkLexer *lexer, int event, int depth,
					int nodeIndex, const char *name,
					const NpCslaChunkLexerToken *token, int arrayIndex,
					SceSize offset, void *context);

struct NpCslaChunkLexer {
	const NpOAuthJsonLexerConfig *config;
	char *nameBuf;
	SceSize nameBufSize;
	int tokenType;
	char *tokenBuf;
	SceSize tokenBufSize;
	SceSize depth;
	int nodeIndex[16];
	int arrayIndex[16];
	int state;
	SceSize position;
	SceSize tokenStart;
	SceSize length;
	const char *literal;
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];
	SceSize partialLength;
	SceSize remaining;
	NpCslaChunkLexerCallback callback;
	void *context;
};

extern "C" {
extern void *SceNpCommonPs4_0C922423(SceSize size, NpOAuthAllocator *allocator);
extern void *SceNpCommonPs4_AA980113(NpOAuthAllocator *allocator, SceSize size);
extern int SceNpCommonPs4_6B428829(NpOAuthAllocator *allocator, void *pointer);
extern int SceNpCommonPs4_4858EFB6(SceKernelLwMutexWork *work, const char *name, char recursive);
extern int SceNpCommonPs4_29895EBB(SceKernelLwMutexWork *work);
extern int SceNpCommonPs4_F5840D6E(SceKernelLwMutexWork *work);
extern int SceNpCommonPs4_4A1C1C3C(SceKernelLwMutexWork *work);
extern int SceNpCommonPs4_A25DA29A(SceUInt32 *outTime);
extern int SceNpCommonPs4_EC456A83(const unsigned char *src, char *dst, SceSize length);
extern SceUInt64 SceNpCommonPs4_80F6D82E(const char *str, const char **endPtr, int base);
extern int SceNpCommonPs4_8BF34333(NpCslaChunkLexer *lexer, const NpOAuthJsonLexerConfig *config,
				   char *nameBuf, SceSize nameBufSize, char *tokenBuf,
				   SceSize tokenBufSize, NpCslaChunkLexerCallback callback,
				   void *context);
extern int SceNpCommonPs4_40E4EF01(NpCslaChunkLexer *lexer, const char *src, int size);
extern int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *outTick);
extern int sceRtcTickAddTicks(SceRtcTick *outTick, const SceRtcTick *baseTick, SceInt64 delta);
extern int sceRtcFormatRFC3339LocalTime(char *outBuf, const SceRtcTick *tick);
}

extern int npAccountCopyCachedTicket(void *outBuf, SceSize *ioSize);
extern int liveareaHmacSha256(const void *key, SceSize keyLen, const void *msg,
			      SceSize msgLen, void *outDigest);

/* ============================================================================
 * CSLA JSON Schemas (0x8152871C, 0x81528770, 0x81528898)
 * ============================================================================ */

static const NpOAuthJsonLexerNode kCslaRootNodes[] = {
	{ 1, NULL, 1 },
	{ 0, NULL, 0 }
};

static const NpOAuthJsonLexerNode kCslaTokenLevelNodes[] = {
	{ 1, "consoleToken", 0 },
	{ 0, NULL, 0 }
};

static const NpOAuthJsonLexerNode *const kCslaTokenLevels[] = {
	kCslaRootNodes,
	kCslaTokenLevelNodes
};

static const NpOAuthJsonLexerConfig kCslaTokenJsonConfig = {
	kCslaTokenLevels,
	2
};

static const NpOAuthJsonLexerNode kCslaErrorTopNodes[] = {
	{ 1, "error", 1 },
	{ 0, NULL, 0 }
};

static const NpOAuthJsonLexerNode kCslaErrorCodeNodes[] = {
	{ 1, "code", 0 },
	{ 0, NULL, 0 }
};

static const NpOAuthJsonLexerNode *const kCslaErrorLevels[] = {
	kCslaRootNodes,
	kCslaErrorTopNodes,
	kCslaErrorCodeNodes
};

static const NpOAuthJsonLexerConfig kCslaErrorJsonConfig = {
	kCslaErrorLevels,
	3
};

static const NpOAuthJsonLexerNode kCslaJwtExpNodes[] = {
	{ 1, "exp", 0 },
	{ 0, NULL, 0 }
};

static const NpOAuthJsonLexerNode *const kCslaJwtExpLevels[] = {
	kCslaRootNodes,
	kCslaJwtExpNodes
};

static const NpOAuthJsonLexerConfig kCslaJwtExpJsonConfig = {
	kCslaJwtExpLevels,
	2
};

/* ============================================================================
 * CSLA HTTP Layer & Global State
 * ============================================================================ */

struct CslaHttpHandle {
	CslaHttpHandle *selfMagic;
	NpOAuthAllocator *allocator;
	int templateId;
	int connectionId;
	int requestId;
	SceUInt8 inFlight;
	SceUInt8 aborted;
};

struct CslaRequestNode {
	int requestId;
	CslaHttpHandle *httpHandle;
	SceUInt8 inUse;
	SceUInt8 aborted;
	CslaRequestNode *next;
	NpOAuthAllocator *allocator;
};

struct CslaTokenCache {
	char cachedToken[NP_CSLA_TOKEN_CACHE_COPY_SIZE];
	SceRtcTick expireTick;
	SceRtcTick issuedTick;
};

struct CslaContext {
	CslaTokenCache *cache;
	SceKernelLwMutexWork lock;
	CslaRequestNode *requestListHead;
};

static SceUInt8 s_cslaInitialized = 0;			/* 0x81549214 */
static SceUInt8 s_cslaHttpApiInit = 0;			/* 0x81549218 */
static SceUInt8 s_cslaHttpLayerInit = 0;		/* 0x81549220 */
static SceKernelLwMutexWork s_cslaHttpLock;		/* 0x81549228 */
static int s_cslaHttpTemplateId = 0;			/* 0x81549248 */
static CslaContext *s_cslaContext = NULL;		/* 0x81549250 */
static NpOAuthAllocator s_cslaAllocator;		/* 0x815CBD0C */
static SceUInt32 s_cslaNextReqId = 0;			/* 0x815CB340 */
static SceMt19937Context s_cslaRng;			/* 0x815CB348 */

static int SelectCslaEnvironmentKey(SceUInt8 outKey[32])
{
	char envName[20];
	sceClibMemset(envName, 0, sizeof(envName));
	int ret = sceNpCommonGetNpEnviroment(envName);
	if (ret < 0)
		return ret;

	if (sceClibStrncmp("np", envName, 16) != 0 && sceClibStrncmp("mgmt", envName, 16) != 0)
		sceClibMemcpy_safe(outKey, kCslaDevKey, 32);
	else
		sceClibMemcpy_safe(outKey, kCslaProdKey, 32);
	return 0;
}

/* 0x813BB5B4 */
static int CslaHttpLayerInit(void)
{
	if (s_cslaHttpLayerInit != 0)
		return NP_CSLA_HTTP_ERR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_4858EFB6(&s_cslaHttpLock, "SceNpCslaLock", 1);
	if (ret < 0)
		return ret;

	char npVer[8];
	char userAgent[32];
	sceClibMemset(npVer, 0, sizeof(npVer));
	SceNpCommon_0624AE44(npVer);
	sceClibMemset(userAgent, 0, sizeof(userAgent));
	sceClibSnprintf(userAgent, sizeof(userAgent), "NpCsla/%s", npVer);

	int tmplId = sceHttpCreateTemplate(userAgent, 2, 1);
	if (tmplId < 0) {
		SceNpCommonPs4_29895EBB(&s_cslaHttpLock);
		return tmplId;
	}

	s_cslaHttpTemplateId = tmplId;
	s_cslaHttpLayerInit = 1;
	return tmplId;
}

/* 0x813BB68A */
static int CslaHttpLayerTerm(void)
{
	if (s_cslaHttpLayerInit == 0)
		return NP_CSLA_HTTP_ERR_NOT_INITIALIZED;

	sceHttpDeleteTemplate(s_cslaHttpTemplateId);
	s_cslaHttpTemplateId = 0;
	SceNpCommonPs4_29895EBB(&s_cslaHttpLock);
	s_cslaHttpLayerInit = 0;
	return 0;
}

/* 0x813BB6C2 */
static int CslaHttpCreateHandle(NpOAuthAllocator *allocator, CslaHttpHandle **outHandle)
{
	if (outHandle == NULL)
		return NP_CSLA_HTTP_ERR_INVALID_ARGUMENT;

	CslaHttpHandle *handle = static_cast<CslaHttpHandle *>(
		SceNpCommonPs4_AA980113(allocator, sizeof(CslaHttpHandle)));
	if (handle == NULL)
		return NP_CSLA_HTTP_ERR_OUT_OF_MEMORY;

	sceClibMemset(handle, 0, sizeof(*handle));
	handle->selfMagic = handle;
	handle->allocator = allocator;
	*outHandle = handle;
	return 0;
}

/* 0x813BB708 */
static int CslaHttpDestroyHandle(CslaHttpHandle *handle)
{
	if (handle == NULL || handle->selfMagic != handle)
		return NP_CSLA_HTTP_ERR_INVALID_HANDLE;

	SceNpCommonPs4_6B428829(handle->allocator, handle);
	return 0;
}

/* 0x813BB72C */
static int CslaHttpAbortHandle(CslaHttpHandle *handle)
{
	if (handle == NULL || handle->selfMagic != handle)
		return NP_CSLA_HTTP_ERR_INVALID_HANDLE;

	int ret = 0;
	SceNpCommonPs4_F5840D6E(&s_cslaHttpLock);
	if (handle->aborted == 0) {
		handle->aborted = 1;
		if (handle->requestId != 0)
			ret = sceHttpAbortRequest(handle->requestId);
	}
	SceNpCommonPs4_4A1C1C3C(&s_cslaHttpLock);
	return ret;
}

/* 0x813BB774 */
static int CslaHttpOpenRequest(CslaHttpHandle *handle, const char *url, int method,
			       SceUInt64 contentLength)
{
	int connId = sceHttpCreateConnectionWithURL(s_cslaHttpTemplateId, url, 1);
	if (connId < 0)
		return connId;

	int reqId = sceHttpCreateRequestWithURL(connId, method, url, contentLength);
	if (reqId < 0) {
		sceHttpDeleteConnection(connId);
		return reqId;
	}

	int ret = 0;
	if (contentLength != 0) {
		ret = sceHttpSetRequestContentLength(reqId, contentLength);
		if (ret >= 0) {
			ret = sceHttpAddRequestHeader(reqId, "Content-Type",
						      "application/json; charset=utf-8", 0);
		}
	}

	if (ret >= 0) {
		SceNpCommonPs4_F5840D6E(&s_cslaHttpLock);
		if (handle->aborted == 0) {
			handle->connectionId = connId;
			handle->requestId = reqId;
			SceNpCommonPs4_4A1C1C3C(&s_cslaHttpLock);
			return 0;
		}
		SceNpCommonPs4_4A1C1C3C(&s_cslaHttpLock);
		ret = NP_CSLA_HTTP_ERR_ABORTED;
	}

	sceHttpDeleteRequest(reqId);
	sceHttpDeleteConnection(connId);
	return ret;
}

/* 0x813BBCE8: Chunk lexer callback extracting "consoleToken" */
struct CslaTokenParseContext {
	char *outBuf;
	SceSize capacity;
	SceSize written;
};

static int CslaOnConsoleTokenValue(NpCslaChunkLexer *lexer, int event, int depth,
				   int nodeIndex, const char *name,
				   const NpCslaChunkLexerToken *token, int arrayIndex,
				   SceSize offset, void *context)
{
	(void)lexer;
	(void)event;
	(void)depth;
	(void)nodeIndex;
	(void)arrayIndex;
	(void)offset;

	CslaTokenParseContext *parseCtx = static_cast<CslaTokenParseContext *>(context);
	SceSize remaining = parseCtx->capacity - parseCtx->written;
	if (token != NULL && token->type == 4 &&
	    sceClibStrncmp(name, "consoleToken", 12) == 0 && token->text != NULL) {
		SceSize chunkLen = sceClibStrnlen(token->text, (SceSize)-1);
		if (chunkLen > remaining)
			return NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL;
		sceClibMemcpy_safe(parseCtx->outBuf + parseCtx->written, token->text, chunkLen);
		parseCtx->written += chunkLen;
	}
	return 0;
}

/* 0x813BBB6C */
static int CslaParseConsoleTokenJson(const char *jsonBody, SceSize bodyLen,
				     char *outTokenBuf, SceSize outTokenBufSize)
{
	CslaTokenParseContext parseCtx;
	sceClibMemset(&parseCtx, 0, sizeof(parseCtx));
	parseCtx.outBuf = outTokenBuf;
	parseCtx.capacity = outTokenBufSize;

	NpCslaChunkLexer lexer;
	char keyBuf[132];
	char valBuf[NP_CSLA_TOKEN_BUF_SIZE];
	sceClibMemset(&lexer, 0, sizeof(lexer));
	sceClibMemset(keyBuf, 0, sizeof(keyBuf));
	sceClibMemset(valBuf, 0, sizeof(valBuf));

	int ret = SceNpCommonPs4_8BF34333(&lexer, &kCslaTokenJsonConfig, keyBuf, 128,
					  valBuf, NP_CSLA_TOKEN_BUF_SIZE,
					  CslaOnConsoleTokenValue, &parseCtx);
	if (ret >= 0)
		ret = SceNpCommonPs4_40E4EF01(&lexer, jsonBody, static_cast<int>(bodyLen));
	return ret;
}

/* 0x813BBD66: Lexer callback extracting error.code */
static int CslaOnErrorCodeValue(NpOAuthJsonLexer *lexer, int event, int depth, int nodeIndex,
				const char *name, const NpOAuthJsonLexerToken *token,
				int arrayIndex, void *context)
{
	(void)lexer;
	(void)event;
	(void)depth;
	(void)nodeIndex;
	(void)arrayIndex;

	int *outCode = static_cast<int *>(context);
	if (token != NULL && token->type == 3) {
		SceSize keyLen = sceClibStrnlen("code", (SceSize)-1);
		if (sceClibStrncmp(name, "code", keyLen) == 0)
			*outCode = SceNpCommonPs4_32155BF8(token->text, NULL, 10);
	}
	return 0;
}

/* 0x813BBC20 */
static int CslaParseServerErrorJson(int httpStatusCode, const char *jsonBody, SceSize bodyLen,
				    int *outMappedError)
{
	int serverCode = -1;
	NpOAuthJsonLexer lexer;
	sceClibMemset(&lexer, 0, sizeof(lexer));

	int ret = SceNpCommonPs4_738FF77D(&lexer, &kCslaErrorJsonConfig, CslaOnErrorCodeValue,
					  &serverCode);
	if (ret >= 0) {
		ret = SceNpCommonPs4_6C941C30(&lexer, jsonBody, static_cast<int>(bodyLen));
		if (ret >= 0) {
			SceUInt32 mapped = 0;
			if (serverCode > 0 && serverCode <= 255) {
				mapped = static_cast<SceUInt32>(serverCode) | 0x8055A800U;
			} else if (serverCode == -1) {
				mapped = (httpStatusCode >= 100 && httpStatusCode <= 599)
						 ? (static_cast<SceUInt32>(httpStatusCode) | 0x82F00000U)
						 : 0x82FFFFFFU;
			} else {
				mapped = (static_cast<SceUInt32>(serverCode) < 0xC00000U)
						 ? (static_cast<SceUInt32>(serverCode) | 0x82000000U)
						 : 0x82FFFFFFU;
			}
			*outMappedError = static_cast<int>(mapped);
		}
	}
	return ret;
}

/* 0x813BC072: Lexer callback extracting JWT "exp" timestamp */
static int CslaOnJwtExpValue(NpOAuthJsonLexer *lexer, int event, int depth, int nodeIndex,
			     const char *name, const NpOAuthJsonLexerToken *token,
			     int arrayIndex, void *context)
{
	(void)lexer;
	(void)event;
	(void)depth;
	(void)nodeIndex;
	(void)arrayIndex;

	SceRtcTick *outTick = static_cast<SceRtcTick *>(context);
	if (token != NULL && token->type == 3) {
		SceSize keyLen = sceClibStrnlen("exp", (SceSize)-1);
		if (sceClibStrncmp(name, "exp", keyLen) == 0) {
			SceUInt64 epochSec = SceNpCommonPs4_80F6D82E(token->text, NULL, 10);
			outTick->tick = epochSec * 1000000ULL + NP_CSLA_UNIX_EPOCH_RTC_TICKS;
			char formatted[64];
			sceClibMemset(formatted, 0, sizeof(formatted));
			sceRtcFormatRFC3339LocalTime(formatted, outTick);
		}
	}
	return 0;
}

/* 0x813BBFF0 */
static int CslaParseJwtExpJson(SceRtcTick *outTick, const char *payloadJson, SceSize payloadLen)
{
	NpOAuthJsonLexer lexer;
	SceRtcTick parsedTick;
	sceClibMemset(&lexer, 0, sizeof(lexer));
	sceClibMemset(&parsedTick, 0, sizeof(parsedTick));

	int ret = SceNpCommonPs4_738FF77D(&lexer, &kCslaJwtExpJsonConfig, CslaOnJwtExpValue,
					  &parsedTick);
	if (ret >= 0) {
		ret = SceNpCommonPs4_6C941C30(&lexer, payloadJson, static_cast<int>(payloadLen));
		*outTick = parsedTick;
	}
	return ret;
}

/* 0x813BB4AC: Decode middle segment of JWT and parse its "exp" claim */
static int CslaExtractJwtExpiration(NpOAuthAllocator *allocator, SceRtcTick *outExpireTick,
				    const char *jwtToken)
{
	const char *firstDot = sceClibStrchr(jwtToken, '.');
	if (firstDot == NULL)
		return NP_CSLA_ERR_INVALID_ARGUMENT;
	const char *payloadStart = firstDot + 1;
	const char *secondDot = sceClibStrchr(payloadStart, '.');
	if (secondDot == NULL)
		return NP_CSLA_ERR_INVALID_ARGUMENT;

	SceSize encodedLen = static_cast<SceSize>(secondDot - payloadStart);
	int decodedCap = SceNpCommonPs4_2E65CBA6(payloadStart, encodedLen);
	if (decodedCap < 0)
		return decodedCap;

	SceSize allocSize = static_cast<SceSize>(decodedCap) + 1U;
	char *decodedBuf = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, allocSize));
	if (decodedBuf == NULL)
		return NP_CSLA_ERR_OUT_OF_MEMORY;

	sceClibMemset(decodedBuf, 0, allocSize);
	int ret = SceNpCommonPs4_4A7DB2EF(payloadStart, encodedLen,
					  reinterpret_cast<unsigned char *>(decodedBuf), allocSize);
	if (ret >= 0) {
		SceSize approxLen = (encodedLen & ~3U) - (encodedLen >> 2) + 1U;
		ret = CslaParseJwtExpJson(outExpireTick, decodedBuf, approxLen);
	}
	SceNpCommonPs4_6B428829(allocator, decodedBuf);
	return ret;
}

/* 0x813BAF0E: Fetch NP ticket and Base64-encode it */
static int CslaBuildBase64Ticket(NpOAuthAllocator *allocator, char **outTicketB64)
{
	SceSize ticketSize = 1536;
	unsigned char *ticketRaw = static_cast<unsigned char *>(
		SceNpCommonPs4_AA980113(allocator, ticketSize));
	if (ticketRaw == NULL)
		return NP_CSLA_ERR_OUT_OF_MEMORY;

	sceClibMemset(ticketRaw, 0, ticketSize);
	int ret = npAccountCopyCachedTicket(ticketRaw, &ticketSize);
	if (ret >= 0) {
		SceSize b64Size = 4U * ((ticketSize + 2U) / 3U) + 1U;
		char *encoded = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, b64Size));
		*outTicketB64 = encoded;
		if (encoded == NULL) {
			ret = NP_CSLA_ERR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(encoded, 0, b64Size);
			ret = SceNpCommonPs4_EC456A83(ticketRaw, encoded, ticketSize);
			if (ret >= 0) {
				encoded[ret] = '\0';
			} else {
				SceNpCommonPs4_6B428829(allocator, encoded);
				*outTicketB64 = NULL;
			}
		}
	}

	SceNpCommonPs4_6B428829(allocator, ticketRaw);
	return ret;
}

/* 0x813BAFEC: Encrypt OpenPsId (PKCS#7 padded to 32B) with AES-256-CBC and Base64-encode */
static int CslaBuildEncryptedOpenPsId(NpOAuthAllocator *allocator, char **outEncIdB64,
				      char **outIvB64)
{
	SceKernelOpenPsId openPsId;
	int ret = sceKernelGetOpenPsId(&openPsId);
	if (ret < 0)
		return ret;

	SceUInt8 plainBlocks[32];
	sceClibMemcpy_safe(plainBlocks, openPsId.id, 16);
	sceClibMemset(plainBlocks + 16, 16, 16);

	SceUInt8 iv[16];
	for (int wordIdx = 0; wordIdx < 4; ++wordIdx) {
		SceUInt32 randWord = sceMt19937UInt(&s_cslaRng);
		sceClibMemcpy_safe(iv + wordIdx * 4, &randWord, sizeof(randWord));
	}

	SceUInt8 aesKey[32];
	ret = SelectCslaEnvironmentKey(aesKey);
	if (ret < 0)
		return ret;

	SceAesContext aesCtx;
	ret = sceAesScheduleKey(&aesCtx, 128, 256, aesKey);
	if (ret < 0)
		return ret;

	SceUInt8 xorBlock[16];
	SceUInt8 cipherBlocks[32];
	for (int byteIdx = 0; byteIdx < 16; ++byteIdx)
		xorBlock[byteIdx] = plainBlocks[byteIdx] ^ iv[byteIdx];
	sceAesEncrypt(&aesCtx, xorBlock, cipherBlocks);

	for (int byteIdx = 0; byteIdx < 16; ++byteIdx)
		xorBlock[byteIdx] = plainBlocks[16 + byteIdx] ^ cipherBlocks[byteIdx];
	sceAesEncrypt(&aesCtx, xorBlock, cipherBlocks + 16);

	char *encB64 = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, 45));
	if (encB64 == NULL)
		return NP_CSLA_ERR_OUT_OF_MEMORY;
	sceClibMemset(encB64, 0, 45);
	ret = SceNpCommonPs4_EC456A83(cipherBlocks, encB64, 32);
	if (ret < 0) {
		SceNpCommonPs4_6B428829(allocator, encB64);
		return ret;
	}

	char *ivB64 = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, 25));
	if (ivB64 == NULL) {
		SceNpCommonPs4_6B428829(allocator, encB64);
		return NP_CSLA_ERR_OUT_OF_MEMORY;
	}
	sceClibMemset(ivB64, 0, 25);
	ret = SceNpCommonPs4_EC456A83(iv, ivB64, 16);
	if (ret < 0) {
		SceNpCommonPs4_6B428829(allocator, ivB64);
		SceNpCommonPs4_6B428829(allocator, encB64);
		return ret;
	}

	*outEncIdB64 = encB64;
	*outIvB64 = ivB64;
	return ret;
}

/* 0x813BB258: Format system firmware version as "%x.%03x.%03x" */
static int CslaFormatSystemSwVersion(char outVer[12])
{
	SceKernelSystemSwVersion swVer;
	sceClibMemset(&swVer, 0, sizeof(swVer));
	swVer.size = sizeof(swVer);
	int ret = sceKernelGetSystemSwVersion(&swVer);
	if (ret >= 0) {
		SceUInt32 majorVer = (swVer.version >> 24) & 0xFFU;
		SceUInt32 minorVer = (swVer.version >> 12) & 0xFFFU;
		SceUInt32 buildVer = swVer.version & 0xFFFU;
		sceClibMemset(outVer, 0, 11);
		sceClibSnprintf(outVer, 11, "%x.%03x.%03x", majorVer, minorVer, buildVer);
	}
	return ret;
}

/* 0x813BB2D0: Compute Base64(HMAC-SHA256(key, "<ticket>.<encId>.<at>.<timestampMs>")) */
static int CslaComputeSignature(NpOAuthAllocator *allocator, char **outSignatureB64,
				const char *ticketB64, const char *encIdB64,
				SceUInt32 attestationType, SceUInt64 timestampMs)
{
	char atBuf[8];
	char tsBuf[32];
	sceClibMemset(atBuf, 0, sizeof(atBuf));
	sceClibMemset(tsBuf, 0, sizeof(tsBuf));
	sceClibSnprintf(atBuf, sizeof(atBuf), "%u", attestationType);
	sceClibSnprintf(tsBuf, sizeof(tsBuf), "%lld", static_cast<long long>(timestampMs));

	SceSize msgCap = sceClibStrnlen(ticketB64, (SceSize)-1) +
			 sceClibStrnlen(encIdB64, (SceSize)-1) +
			 sceClibStrnlen(atBuf, (SceSize)-1) +
			 sceClibStrnlen(tsBuf, (SceSize)-1) + 4U;
	char *message = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, msgCap));
	if (message == NULL)
		return NP_CSLA_ERR_OUT_OF_MEMORY;

	sceClibMemset(message, 0, msgCap);
	sceClibSnprintf(message, msgCap, "%s.%s.%s.%s", ticketB64, encIdB64, atBuf, tsBuf);

	SceUInt8 hmacKey[32];
	int ret = SelectCslaEnvironmentKey(hmacKey);
	if (ret >= 0) {
		SceUInt8 digest[32];
		sceClibMemset(digest, 0, sizeof(digest));
		ret = liveareaHmacSha256(hmacKey, sizeof(hmacKey), message,
					 sceClibStrnlen(message, (SceSize)-1), digest);
		if (ret >= 0 && outSignatureB64 != NULL) {
			char *sigB64 = static_cast<char *>(SceNpCommonPs4_AA980113(allocator, 45));
			if (sigB64 == NULL) {
				ret = NP_CSLA_ERR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(sigB64, 0, 45);
				ret = SceNpCommonPs4_EC456A83(digest, sigB64, sizeof(digest));
				if (ret >= 0) {
					*outSignatureB64 = sigB64;
				} else {
					SceNpCommonPs4_6B428829(allocator, sigB64);
					*outSignatureB64 = NULL;
				}
			}
		}
	}

	SceNpCommonPs4_6B428829(allocator, message);
	return ret;
}

/* 0x813BB830: Execute HTTP POST /csla/ (version 1) /console/lwConsoleToken */
static int CslaPostConsoleTokenRequest(CslaHttpHandle *handle, const char *ticketB64,
				       const char *encIdB64, const char *ivB64,
				       const char *swVer, SceUInt32 attestationType,
				       SceUInt32 serviceIndex, SceUInt64 timestampMs,
				       const char *signatureB64, char *outTokenBuf,
				       SceSize outTokenBufSize)
{
	if (handle == NULL || handle->selfMagic != handle)
		return NP_CSLA_HTTP_ERR_INVALID_HANDLE;

	SceNpCommonPs4_F5840D6E(&s_cslaHttpLock);
	if (handle->inFlight != 0) {
		SceNpCommonPs4_4A1C1C3C(&s_cslaHttpLock);
		return NP_CSLA_HTTP_ERR_INVALID_ARGUMENT;
	}
	handle->inFlight = 1;
	SceNpCommonPs4_4A1C1C3C(&s_cslaHttpLock);

	char *reqBody = NULL;
	char *respBody = NULL;
	int ret = 0;

	char envName[20];
	sceClibMemset(envName, 0, sizeof(envName));
	ret = sceNpCommonGetNpEnviroment(envName);

	char hostBuf[48];
	char urlBuf[256];
	sceClibMemset(hostBuf, 0, sizeof(hostBuf));
	sceClibMemset(urlBuf, 0, sizeof(urlBuf));

	if (ret >= 0) {
		sceClibSnprintf(hostBuf, sizeof(hostBuf),
				"csla.%s.community.playstation.net", envName);
		int urlLen = sceClibSnprintf(urlBuf, sizeof(urlBuf),
					     "%s://%s/csla/%s/console/lwConsoleToken",
					     "https", hostBuf, "v" "1");
		if (urlLen >= static_cast<int>(sizeof(urlBuf)))
			ret = NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL;
	}

	SceSize reqBodyLen = 0;
	if (ret >= 0) {
		reqBody = static_cast<char *>(SceNpCommonPs4_AA980113(handle->allocator, 2304));
		if (reqBody == NULL) {
			ret = NP_CSLA_HTTP_ERR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(reqBody, 0, 2304);
			int bodyLen = sceClibSnprintf(
				reqBody, 2304,
				"{\"ticket\":\"%s\",\"id\":\"%s\",\"iv\":\"%s\",\"sv\":\"%s\","
				"\"at\":%u,\"si\":\"%02u\",\"requestTimestamp\":%llu,"
				"\"signature\":\"%s\"}",
				ticketB64, encIdB64, ivB64, swVer, attestationType, serviceIndex,
				static_cast<unsigned long long>(timestampMs), signatureB64);
			if (bodyLen >= 2304)
				ret = NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL;
			else
				reqBodyLen = sceClibStrnlen(reqBody, (SceSize)-1);
		}
	}

	if (ret >= 0)
		ret = CslaHttpOpenRequest(handle, urlBuf, 1, reqBodyLen);

	if (ret >= 0) {
		respBody = static_cast<char *>(SceNpCommonPs4_AA980113(handle->allocator, 6176));
		if (respBody == NULL)
			ret = NP_CSLA_HTTP_ERR_OUT_OF_MEMORY;
	}

	int httpStatus = 0;
	SceSize respLen = 0;
	if (ret >= 0) {
		ret = sceHttpSendRequest(handle->requestId, reqBody, reqBodyLen);
		if (ret >= 0)
			ret = sceHttpGetStatusCode(handle->requestId, &httpStatus);
		bool hasContentLength = false;
		if (ret >= 0) {
			SceUInt64 contentLength = 0;
			int lenRet = sceHttpGetResponseContentLength(handle->requestId,
								     &contentLength);
			if (lenRet == 0) {
				hasContentLength = true;
				if (contentLength > 6176U)
					ret = NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL;
			} else if (lenRet != NP_CSLA_HTTP_ERR_NO_CONTENT_LENGTH &&
				   lenRet != NP_CSLA_HTTP_ERR_CHUNKED_ENCODING) {
				ret = lenRet;
			}
		}

		if (ret >= 0) {
			sceClibMemset(respBody, 0, 6176);
			for (;;) {
				int chunk = sceHttpReadData(handle->requestId,
							    respBody + respLen, 6176 - respLen);
				if (chunk < 0) {
					ret = chunk;
					break;
				}
				if (chunk == 0) {
					if (!hasContentLength && respLen == 6176)
						ret = NP_CSLA_HTTP_ERR_BUFFER_TOO_SMALL;
					break;
				}
				respLen += static_cast<SceSize>(chunk);
			}
		}

		if (ret >= 0) {
			if (httpStatus >= 200 && httpStatus < 300) {
				ret = CslaParseConsoleTokenJson(respBody, respLen,
								outTokenBuf, outTokenBufSize);
			} else {
				int mappedErr = 0;
				CslaParseServerErrorJson(httpStatus, respBody, respLen, &mappedErr);
				ret = mappedErr;
			}
		}
	}

	if (handle->aborted != 0)
		ret = NP_CSLA_HTTP_ERR_ABORTED;
	if (handle->requestId != 0) {
		sceHttpDeleteRequest(handle->requestId);
		handle->requestId = 0;
	}
	if (handle->connectionId != 0) {
		sceHttpDeleteConnection(handle->connectionId);
		handle->connectionId = 0;
	}
	if (handle->templateId != 0) {
		sceHttpDeleteTemplate(handle->templateId);
		handle->templateId = 0;
	}
	if (respBody != NULL)
		SceNpCommonPs4_6B428829(handle->allocator, respBody);
	if (reqBody != NULL)
		SceNpCommonPs4_6B428829(handle->allocator, reqBody);
	return ret;
}

static CslaRequestNode *FindCslaRequestNode(CslaContext *ctx, int reqId)
{
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	CslaRequestNode *cur = ctx->requestListHead;
	while (cur != NULL) {
		if (cur->requestId == reqId)
			break;
		cur = cur->next;
	}
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return cur;
}

/* 0x813BAAD0: Core token fetch with midpoint-expiry caching */
static int CslaFetchOrReuseToken(CslaContext *ctx, CslaRequestNode *reqNode,
				 char *outTokenBuf, SceUInt8 forceRefresh)
{
	SceRtcTick nowTick;
	sceClibMemset(&nowTick, 0, sizeof(nowTick));
	int ret = sceRtcGetCurrentRetainedNetworkTick(&nowTick);
	if (ret < 0)
		return ret;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	SceRtcTick midpointTick;
	SceInt64 halfLife = static_cast<SceInt64>(
		(ctx->cache->expireTick.tick - ctx->cache->issuedTick.tick) >> 1);
	ret = sceRtcTickAddTicks(&midpointTick, &ctx->cache->issuedTick, halfLife);
	if (ret < 0) {
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		return ret;
	}
	if (forceRefresh == 0 && nowTick.tick < midpointTick.tick) {
		sceClibMemcpy_safe(outTokenBuf, ctx->cache->cachedToken,
				   NP_CSLA_TOKEN_CACHE_COPY_SIZE);
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		return 0;
	}
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);

	char *tokenBuf = static_cast<char *>(
		SceNpCommonPs4_AA980113(&s_cslaAllocator, NP_CSLA_TOKEN_BUF_SIZE));
	if (tokenBuf == NULL)
		return NP_CSLA_ERR_OUT_OF_MEMORY;
	sceClibMemset(tokenBuf, 0, NP_CSLA_TOKEN_BUF_SIZE);

	char *ticketB64 = NULL;
	char *encIdB64 = NULL;
	char *ivB64 = NULL;
	char *signatureB64 = NULL;
	CslaHttpHandle *httpHandle = NULL;
	char swVer[12];
	sceClibMemset(swVer, 0, sizeof(swVer));

	ret = CslaBuildBase64Ticket(&s_cslaAllocator, &ticketB64);
	if (ret >= 0)
		ret = CslaBuildEncryptedOpenPsId(&s_cslaAllocator, &encIdB64, &ivB64);
	if (ret >= 0)
		ret = CslaFormatSystemSwVersion(swVer);
	if (ret >= 0)
		ret = CslaHttpCreateHandle(&s_cslaAllocator, &httpHandle);

	if (ret >= 0) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		if (reqNode->aborted == 0) {
			reqNode->httpHandle = httpHandle;
			SceNpCommonPs4_4A1C1C3C(&ctx->lock);

			SceUInt64 nowMs = (nowTick.tick - NP_CSLA_UNIX_EPOCH_RTC_TICKS) / 1000ULL;
			ret = CslaComputeSignature(&s_cslaAllocator, &signatureB64,
						   ticketB64, encIdB64, 2, nowMs);
			if (ret >= 0) {
				ret = CslaPostConsoleTokenRequest(
					httpHandle, ticketB64, encIdB64, ivB64, swVer,
					2, 0, nowMs, signatureB64, tokenBuf,
					NP_CSLA_TOKEN_BUF_SIZE);
			}
			if (ret >= 0) {
				SceRtcTick expireTick;
				sceClibMemset(&expireTick, 0, sizeof(expireTick));
				ret = CslaExtractJwtExpiration(&s_cslaAllocator, &expireTick,
							       tokenBuf);
				if (ret >= 0) {
					SceNpCommonPs4_F5840D6E(&ctx->lock);
					sceClibMemset(ctx->cache->cachedToken, 0,
						      NP_CSLA_TOKEN_CACHE_COPY_SIZE);
					sceClibStrncpy(ctx->cache->cachedToken, tokenBuf,
						       NP_CSLA_TOKEN_BUF_SIZE - 1);
					ctx->cache->expireTick = expireTick;
					ctx->cache->issuedTick = nowTick;
					sceClibMemcpy_safe(outTokenBuf, ctx->cache->cachedToken,
							   NP_CSLA_TOKEN_CACHE_COPY_SIZE);
					SceNpCommonPs4_4A1C1C3C(&ctx->lock);
				}
			}
		} else {
			SceNpCommonPs4_4A1C1C3C(&ctx->lock);
			ret = NP_CSLA_ERR_ABORTED;
		}
	}

	if (signatureB64 != NULL)
		SceNpCommonPs4_6B428829(&s_cslaAllocator, signatureB64);
	if (encIdB64 != NULL)
		SceNpCommonPs4_6B428829(&s_cslaAllocator, encIdB64);
	if (ivB64 != NULL)
		SceNpCommonPs4_6B428829(&s_cslaAllocator, ivB64);
	if (ticketB64 != NULL)
		SceNpCommonPs4_6B428829(&s_cslaAllocator, ticketB64);
	if (httpHandle != NULL) {
		CslaHttpDestroyHandle(httpHandle);
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		reqNode->httpHandle = NULL;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	SceNpCommonPs4_6B428829(&s_cslaAllocator, tokenBuf);
	return ret;
}

/* ============================================================================
 * Public Entry Points (0x813BA81C..0x813BA8FF)
 * ============================================================================ */

/* 0x813BA81C */
int npCslaInit(NpOAuthAllocator *allocator)
{
	if (s_cslaInitialized != 0)
		return NP_CSLA_ERR_ALREADY_INITIALIZED;
	if (allocator == NULL)
		return NP_CSLA_ERR_INVALID_ARGUMENT;

	if (s_cslaHttpApiInit != 0)
		return NP_CSLA_HTTP_ERR_ALREADY_INITIALIZED;

	int ret = CslaHttpLayerInit();
	if (ret < 0)
		return ret;
	s_cslaHttpApiInit = 1;

	SceUInt32 seed = 0;
	ret = SceNpCommonPs4_A25DA29A(&seed);
	if (ret >= 0) {
		sceMt19937Init(&s_cslaRng, seed);
		if (s_cslaContext != NULL) {
			ret = NP_CSLA_ERR_CONTEXT_ALREADY_EXISTS;
		} else {
			s_cslaAllocator = *allocator;
			CslaContext *ctx = static_cast<CslaContext *>(
				SceNpCommonPs4_0C922423(sizeof(CslaContext), &s_cslaAllocator));
			if (ctx == NULL) {
				ret = NP_CSLA_ERR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(ctx, 0, sizeof(*ctx));
				ctx->cache = static_cast<CslaTokenCache *>(
					SceNpCommonPs4_0C922423(sizeof(CslaTokenCache),
								&s_cslaAllocator));
				if (ctx->cache == NULL) {
					SceNpCommonPs4_6B428829(&s_cslaAllocator, ctx);
					ret = NP_CSLA_ERR_OUT_OF_MEMORY;
				} else {
					sceClibMemset(ctx->cache, 0, sizeof(*ctx->cache));
					ret = SceNpCommonPs4_4858EFB6(&ctx->lock,
								      "SceNpCslaClientLock", 1);
					if (ret >= 0) {
						s_cslaContext = ctx;
						s_cslaInitialized = 1;
						return 0;
					}
					SceNpCommonPs4_6B428829(&s_cslaAllocator, ctx->cache);
					SceNpCommonPs4_6B428829(&s_cslaAllocator, ctx);
				}
			}
		}
	}

	CslaHttpLayerTerm();
	s_cslaHttpApiInit = 0;
	return ret;
}

/* 0x813BA858 */
int npCslaCreateRequest(const NpCslaCreateRequestOption *option)
{
	if (s_cslaInitialized == 0 || s_cslaContext == NULL)
		return NP_CSLA_ERR_NOT_INITIALIZED;

	NpOAuthAllocator *alloc = (option != NULL && option->allocator != NULL)
					  ? option->allocator
					  : &s_cslaAllocator;

	SceNpCommonPs4_F5840D6E(&s_cslaContext->lock);
	CslaRequestNode *node = static_cast<CslaRequestNode *>(
		SceNpCommonPs4_0C922423(sizeof(CslaRequestNode), alloc));
	if (node == NULL) {
		SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
		return NP_CSLA_ERR_OUT_OF_MEMORY;
	}

	sceClibMemset(node, 0, sizeof(*node));
	for (;;) {
		s_cslaNextReqId++;
		if (s_cslaNextReqId > 0xEFFFFFFFU)
			s_cslaNextReqId = 1;
		bool duplicate = false;
		for (CslaRequestNode *cur = s_cslaContext->requestListHead; cur != NULL;
		     cur = cur->next) {
			if (cur->requestId == static_cast<int>(s_cslaNextReqId)) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate)
			break;
	}

	node->requestId = static_cast<int>(s_cslaNextReqId);
	node->allocator = alloc;
	node->next = s_cslaContext->requestListHead;
	s_cslaContext->requestListHead = node;
	int assignedId = node->requestId;
	SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
	return assignedId;
}

/* 0x813BA876 */
int npCslaDestroyRequest(int cslaReqId)
{
	if (s_cslaInitialized == 0 || s_cslaContext == NULL)
		return NP_CSLA_ERR_NOT_INITIALIZED;

	SceNpCommonPs4_F5840D6E(&s_cslaContext->lock);
	CslaRequestNode *prev = NULL;
	CslaRequestNode *cur = s_cslaContext->requestListHead;
	while (cur != NULL) {
		if (cur->requestId == cslaReqId)
			break;
		prev = cur;
		cur = cur->next;
	}

	if (cur == NULL) {
		SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
		return NP_CSLA_ERR_INVALID_REQUEST_ID;
	}

	if (prev != NULL)
		prev->next = cur->next;
	else
		s_cslaContext->requestListHead = cur->next;

	if (cur->aborted == 0) {
		cur->aborted = 1;
		if (cur->httpHandle != NULL)
			CslaHttpAbortHandle(cur->httpHandle);
	}
	SceNpCommonPs4_6B428829(cur->allocator, cur);
	SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
	return 0;
}

/* 0x813BA894 */
int npCslaAbortRequest(int cslaReqId)
{
	if (s_cslaInitialized == 0 || s_cslaContext == NULL)
		return NP_CSLA_ERR_NOT_INITIALIZED;

	CslaRequestNode *node = FindCslaRequestNode(s_cslaContext, cslaReqId);
	if (node == NULL)
		return NP_CSLA_ERR_INVALID_REQUEST_ID;

	int ret = 0;
	SceNpCommonPs4_F5840D6E(&s_cslaContext->lock);
	if (node->aborted == 0) {
		if (node->httpHandle != NULL)
			ret = CslaHttpAbortHandle(node->httpHandle);
		if (ret >= 0)
			node->aborted = 1;
	}
	SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
	return ret;
}

/* 0x813BA8B2 */
int npCslaGetConsoleToken(int cslaReqId, char *outTokenBuf, SceUInt8 forceRefresh)
{
	if (s_cslaInitialized == 0 || s_cslaContext == NULL)
		return NP_CSLA_ERR_NOT_INITIALIZED;
	if (outTokenBuf == NULL)
		return NP_CSLA_ERR_INVALID_ARGUMENT;

	CslaRequestNode *node = FindCslaRequestNode(s_cslaContext, cslaReqId);
	if (node == NULL)
		return NP_CSLA_ERR_INVALID_REQUEST_ID;

	SceNpCommonPs4_F5840D6E(&s_cslaContext->lock);
	bool alreadyInUse = (node->inUse != 0);
	if (!alreadyInUse)
		node->inUse = 1;
	SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);

	if (alreadyInUse)
		return NP_CSLA_ERR_REQUEST_IN_USE;

	return CslaFetchOrReuseToken(s_cslaContext, node, outTokenBuf, forceRefresh);
}

/* 0x813BA8DE */
int npCslaClearCache(void)
{
	if (s_cslaInitialized == 0 || s_cslaContext == NULL)
		return NP_CSLA_ERR_NOT_INITIALIZED;

	SceNpCommonPs4_F5840D6E(&s_cslaContext->lock);
	sceClibMemset(s_cslaContext->cache, 0, sizeof(*s_cslaContext->cache));
	SceNpCommonPs4_4A1C1C3C(&s_cslaContext->lock);
	return 0;
}
