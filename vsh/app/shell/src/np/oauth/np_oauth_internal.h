/*
 * np_oauth_internal.h: internal types, constants, and declarations for NpOAuthVsh
 * (SceShell OAuth 2.0 client for PSN authorization codes and access tokens).
 */
#ifndef SHELL_NP_OAUTH_INTERNAL_H
#define SHELL_NP_OAUTH_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <rtc.h>
#include <np/np_common.h>
#include <np/np_auth_oauth.h>
#include "../ipc/np_ipc_internal.h"
#include "../manager/np_manager_core.h"

/* NpOAuthVsh error codes (facility 0x80559Cxx) */
#define NP_OAUTH_ERR_INTERNAL			-2141873151	/* 0x80559C01 */
#define NP_OAUTH_ERR_INVALID_ARGUMENT		-2141873150	/* 0x80559C02 */
#define NP_OAUTH_ERR_OUT_OF_MEMORY		-2141873149	/* 0x80559C03 */
#define NP_OAUTH_ERR_BUFFER_TOO_SMALL		-2141873148	/* 0x80559C04 */
#define NP_OAUTH_ERR_INVALID_RESPONSE		-2141873147	/* 0x80559C05 */
#define NP_OAUTH_ERR_ABORTED			-2141873146	/* 0x80559C06 */
#define NP_OAUTH_ERR_SIGNED_OUT			-2141873145	/* 0x80559C07 */
#define NP_OAUTH_ERR_INVALID_REQUEST_ID		-2141873144	/* 0x80559C08 */
#define NP_OAUTH_ERR_COMMON_INVALID_ARG		-2141913085	/* 0x80550003 */

/* Server / HTTP OAuth mapped error codes */
#define NP_OAUTH_SERVER_ERR_INVALID_NPSSO	-2099244987	/* 0x82E01045 */
#define NP_OAUTH_SERVER_ERR_GENERIC		0x82FFFFFFU
#define NP_OAUTH_SERVER_ERR_BASE		0x82E00000U
#define NP_OAUTH_HTTP_ERR_BASE			0x82E10000U

/* HTTP error codes handled specially by NpOAuthVsh */
#define NP_OAUTH_HTTP_ERR_PARSE_NOT_FOUND	-2143088535	/* 0x80431069 */
#define NP_OAUTH_HTTP_ERR_NO_CONTENT_LENGTH	-2143088528	/* 0x80431070 */
#define NP_OAUTH_HTTP_ERR_CHUNK_ENC		-2143088527	/* 0x80431071 */
#define NP_OAUTH_HTTP_ERR_SSL			-2143088523	/* 0x80431075 */

/* HTTPS private option masks */
#define NP_OAUTH_HTTPS_ENABLE_OPTS		357U
#define NP_OAUTH_HTTPS_DISABLE_OPTS		24U

/* Token cache and buffer limits */
#define NP_OAUTH_ACCESS_TOKEN_MIN_REMAINING_US	0x11E1A300ULL	/* 300 seconds */
#define NP_OAUTH_URL_BUF_SIZE			2048U
#define NP_OAUTH_BODY_BUF_SIZE			2048U
#define NP_OAUTH_CONSOLE_TOKEN_BUF_SIZE		0x1810U
#define NP_OAUTH_NPSSO_TOKEN_SIZE		1040U
#define NP_OAUTH_JWT_BUF_SIZE			4097U
#define NP_OAUTH_MAX_REQUEST_ID			0xEFFFFFFFU
#define NP_OAUTH_REDIRECT_URI_MAX_LEN		0x2000U

/* Parsed JSON presence bitflags for access token responses */
#define NP_OAUTH_TOKEN_HAS_EXPIRES_IN		0x01U
#define NP_OAUTH_TOKEN_HAS_TOKEN_TYPE		0x02U
#define NP_OAUTH_TOKEN_HAS_ACCESS_TOKEN		0x08U
#define NP_OAUTH_TOKEN_HAS_SCOPE		0x10U
#define NP_OAUTH_TOKEN_REQUIRED_MASK		(NP_OAUTH_TOKEN_HAS_EXPIRES_IN | \
						 NP_OAUTH_TOKEN_HAS_TOKEN_TYPE | \
						 NP_OAUTH_TOKEN_HAS_ACCESS_TOKEN | \
						 NP_OAUTH_TOKEN_HAS_SCOPE)

/* SceNpCommonPs4 allocator & streaming JSON lexer types (matching np_common_ps4_internal.h) */
struct NpOAuthAllocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
};

struct NpOAuthJsonLexerNode {
	int kind;
	const char *name;
	int matchFlag;
};

struct NpOAuthJsonLexerConfig {
	const NpOAuthJsonLexerNode *const *levels;
	SceSize numLevels;
};

#define NP_OAUTH_JSON_NODE_END			0
#define NP_OAUTH_JSON_NODE_KEY			1
#define NP_OAUTH_JSON_EVENT_VALUE		6

struct NpOAuthJsonLexerToken {
	int type;
	char text[1536];
};

struct NpOAuthJsonLexer;
typedef int (*NpOAuthJsonLexerCallback)(NpOAuthJsonLexer *lexer, int event, int depth, int node,
					const char *keyName, const NpOAuthJsonLexerToken *token,
					int arrayIndex, void *context);

struct NpOAuthJsonLexer {
	const NpOAuthJsonLexerConfig *config;
	char name[128];
	NpOAuthJsonLexerToken token;
	SceSize depth;
	int nodeIndex[16];
	int arrayIndex[16];
	int state;
	SceSize length;
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];
	SceSize partialLength;
	SceSize remaining;
	NpOAuthJsonLexerCallback callback;
	void *context;
};

struct NpOAuthJsonStreamToken {
	int type;
	char *text;
};

struct NpOAuthJsonStreamLexer;
typedef int (*NpOAuthJsonStreamCallback)(NpOAuthJsonStreamLexer *lexer, int event, int depth,
					 int node, const char *keyName,
					 const NpOAuthJsonStreamToken *token, int arrayIndex,
					 void *context);

struct NpOAuthJsonStreamLexer {
	const NpOAuthJsonLexerConfig *config;
	char *name;
	SceSize nameSize;
	int tokenType;
	char *tokenText;
	SceSize tokenSize;
	SceSize depth;
	int nodeIndex[16];
	int arrayIndex[16];
	int state;
	SceSize length;
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];
	SceSize partialLength;
	SceSize remaining;
	NpOAuthJsonStreamCallback callback;
	void *context;
};

/* Cached access token (264 bytes = 0x108) */
struct NpOAuthAccessToken {
	char accessToken[256];			/* +0x000 */
	SceRtcTick expireTick;			/* +0x100 */
};

/* Active OAuth request handle (44 bytes = 0x2C) */
struct NpOAuthRequest {
	SceUInt32 requestId;			/* +0x00 */
	int inUse;				/* +0x04 */
	int httpTemplateId;			/* +0x08 */
	SceKernelLwMutexWork *mutex;		/* +0x0C */
	int httpReqId;				/* +0x10 */
	int ticketReqId;			/* +0x14 */
	int cslaReqId;				/* +0x18 */
	SceUInt32 refCount;			/* +0x1C */
	int destroyPending;			/* +0x20 */
	int aborted;				/* +0x24 */
	NpOAuthRequest *next;			/* +0x28 */
};

/* Global NpOAuthVsh singleton state */
struct NpOAuthContext {
	NpOAuthAllocator *allocator;		/* +0x000 */
	int netCallbackId;			/* +0x004 */
	int accountCallbackId;			/* +0x008 */
	int httpTemplateId;			/* +0x00C */
	const char *clientId;			/* +0x010 */
	const char *clientSecret;		/* +0x014 */
	SceKernelLwMutexWork *mutex;		/* +0x018 */
	SceUInt8 hasNpssoToken;			/* +0x01C */
	char npssoToken[NP_OAUTH_NPSSO_TOKEN_SIZE]; /* +0x01D */
	int issuerId;				/* +0x430 */
	NpOAuthAccessToken cachedAccessToken;	/* +0x438 */
	NpOAuthRequest *requestListHead;	/* +0x540 */
	SceUInt32 nextRequestId;		/* +0x544 */
};

/* Common OAuth HTTP/JSON response header (344 bytes = 0x158) */
struct NpOAuthResponseHeader {
	SceUInt32 presentMask;			/* +0x000 */
	int isError;				/* +0x004 */
	int httpStatusCode;			/* +0x008 */
	int hasServerError;			/* +0x00C */
	int serverErrorCode;			/* +0x010 */
	char errorName[65];			/* +0x014 */
	char errorDescription[257];		/* +0x055 */
};

/* Access token endpoint JSON response (608 bytes = 0x260) */
struct NpOAuthAccessTokenResponse {
	NpOAuthResponseHeader header;		/* +0x000 */
	NpOAuthAccessToken token;		/* +0x158 */
};

/* NPSSO password-grant endpoint JSON response (1388 bytes = 0x56C) */
struct NpOAuthNpssoResponse {
	NpOAuthResponseHeader header;		/* +0x000 */
	char npssoToken[NP_OAUTH_NPSSO_TOKEN_SIZE]; /* +0x158 */
	int issuerId;				/* +0x568 */
};

/* Authorization code redirect/error response (480 bytes = 0x1E0) */
struct NpOAuthAuthCodeResponse {
	NpOAuthResponseHeader header;		/* +0x000 */
	SceNpAuthorizationCode authCode;	/* +0x158 */
};

/* JWT id_token payload parse state (348 bytes = 0x15C) */
struct NpOAuthIdTokenParseResult {
	NpOAuthResponseHeader header;		/* +0x000 */
	int *outIssuerId;			/* +0x158 */
};

/* CSLA console-token request option block passed to npCslaCreateRequest */
struct NpCslaCreateRequestOption {
	NpOAuthAllocator *allocator;
};

/* RAII LW mutex lock guard matching Sony's vtable-backed scoped lock */
class NpOAuthScopedLwMutexLockBase {
public:
	explicit NpOAuthScopedLwMutexLockBase(SceKernelLwMutexWork *mutex);
	virtual ~NpOAuthScopedLwMutexLockBase();

protected:
	SceKernelLwMutexWork *m_mutex;
};

class NpOAuthScopedLwMutexLock : public NpOAuthScopedLwMutexLockBase {
public:
	explicit NpOAuthScopedLwMutexLock(NpOAuthRequest *request);
	virtual ~NpOAuthScopedLwMutexLock();
};

/* External C imports from SceNpCommon, SceNpCommonPs4, SceNpManagerTicket, SceHttp, ScePafStdc */
extern "C" {
extern int sceNpCommonGetNpEnviroment(char *outEnv);
extern int SceNpManagerTicket_A30BF29A(int ticketReqId);
extern NpOAuthAllocator *SceNpCommonPs4_53484B79(void);
extern int SceNpCommonPs4_32155BF8(const char *str, const char **endPtr, int base);
extern int SceNpCommonPs4_2E65CBA6(const char *src, SceSize srcLength);
extern int SceNpCommonPs4_4A7DB2EF(const char *src, SceSize srcLength, unsigned char *dst,
				   SceSize dstSize);
extern int SceNpCommonPs4_738FF77D(NpOAuthJsonLexer *lexer, const NpOAuthJsonLexerConfig *config,
				   NpOAuthJsonLexerCallback callback, void *context);
extern int SceNpCommonPs4_6C941C30(NpOAuthJsonLexer *lexer, const char *jsonData, int size);
extern int SceNpCommonPs4_CAFD09C2(NpOAuthJsonStreamLexer *lexer,
				   const NpOAuthJsonLexerConfig *config, char *nameBuf,
				   SceSize nameBufSize, char *valueBuf, SceSize valueBufSize,
				   NpOAuthJsonStreamCallback callback, void *context);
extern int SceNpCommonPs4_D7AB56B8(NpOAuthJsonStreamLexer *lexer, const char *jsonData, int size);
extern int sceHttpsEnableOptionPrivate(int templateId, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int templateId, SceUInt32 sslFlags);
extern int sceHttpsGetSslError(int reqId, int *errNum, unsigned int *detail);
extern int sceHttpSetAutoRedirect(int reqId, SceBool enable);
extern int sceHttpGetAllResponseHeaders(int reqId, char **header, SceSize *headerSize);
extern int sceHttpParseResponseHeader(const char *header, SceSize headerSize, const char *fieldStr,
				      const char **fieldValue, SceSize *valueLen);
extern int sceHttpUriEscape(char *out, SceSize *require, SceSize prepare, const char *in);
extern int sceHttpUriParse(SceHttpUriElement *out, const char *srcUrl, void *pool,
			   SceSize *require, SceSize prepare);
extern SceInt64 sce_paf_atoll(const char *str);
extern long sce_paf_strtol(const char *str, char **endPtr, int base);
}

/* SceNpCsla internal service functions (implemented in np_csla.cpp) */
extern int npCslaInit(NpOAuthAllocator *allocator);
extern int npCslaCreateRequest(const NpCslaCreateRequestOption *option);
extern int npCslaDestroyRequest(int cslaReqId);
extern int npCslaAbortRequest(int cslaReqId);
extern int npCslaGetConsoleToken(int cslaReqId, char *outTokenBuf, SceUInt8 forceRefresh);
extern int npCslaClearCache(void);

/* NpOAuthVsh singleton and lifecycle (np_oauth_vsh.cpp) */
NpOAuthContext *npOAuthGetContext(void);
int npOAuthInit(void);
void npOAuthOnSignOutCallback(int eventType, void *userArg);
int npOAuthOnStateChangeCallback(int eventType, IPMI::Session *session);

/* NpOAuthRequest management (np_oauth_request.cpp) */
int npOAuthRequestCreate(NpOAuthRequest **outRequest);
void npOAuthRequestRelease(NpOAuthRequest *request);
int npOAuthRequestAcquire(NpOAuthRequest *request, NpOAuthRequest **outRequest);
int npOAuthRequestAbort(NpOAuthRequest *request);
int npOAuthRequestDestroy(NpOAuthRequest *request);
bool npOAuthRequestIsInUse(const NpOAuthRequest *request);
void npOAuthRequestMarkInUse(NpOAuthRequest *request);
int npOAuthRequestSetHttpReqId(NpOAuthRequest *request, int httpReqId);
int npOAuthRequestClearHttpReqId(NpOAuthRequest *request);
int npOAuthRequestSetTicketReqId(NpOAuthRequest *request, int ticketReqId);
int npOAuthRequestClearTicketReqId(NpOAuthRequest *request);
int npOAuthRequestSetCslaReqId(NpOAuthRequest *request, int cslaReqId);
int npOAuthRequestClearCslaReqId(NpOAuthRequest *request);

/* NpOAuth response parsing and error mapping (np_oauth_response.cpp) */
int npOAuthMapServerError(int httpStatusCode, int hasServerError, unsigned int serverErrorCode);
int npOAuthReadAccessTokenResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				   SceUInt64 contentLength,
				   NpOAuthAccessTokenResponse *outResponse);
int npOAuthReadNpssoResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
			     SceUInt64 contentLength, NpOAuthNpssoResponse *outResponse);
int npOAuthParseRedirectLocation(NpOAuthRequest *request, const void *locationHeader,
				 SceSize locationLength, int *outHasErrorCode, int *outErrorCode,
				 int *outHasErrorName, char *outErrorName, SceSize errorNameSize,
				 char *outErrorDesc, SceSize errorDescSize, int *outHasState,
				 char *outState, SceSize stateSize);
int npOAuthReadAuthCodeResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				SceUInt64 contentLength, NpOAuthAuthCodeResponse *outResponse);
int npOAuthParseIdTokenPayload(const char *payloadJson, int payloadLength,
			       NpOAuthIdTokenParseResult *outResult);
int npOAuthParseAccessTokenResponse(NpOAuthRequest *request, int httpStatusCode, int isChunked,
				    SceUInt64 contentLength,
				    NpOAuthAccessTokenResponse *outResponse);
void npOAuthHandleFatalError(int errorCode);
int npOAuthDecodeIdToken(const char *jwtToken, int *outIssuerId);

#endif /* SHELL_NP_OAUTH_INTERNAL_H */
