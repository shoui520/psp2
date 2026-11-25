/*
 * np_commerce2_core.h: shell-internal SceNpCommerce2 client engine (0x812BD5A4 - 0x812C16B3),
 * used by SceNpCommerce2SessionManager and game_plugin's GameLicenseUpdater.
 */
#ifndef SHELL_NP_COMMERCE2_CORE_H
#define SHELL_NP_COMMERCE2_CORE_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <np/np_common.h>
#include <np/np_commerce2.h>

/* Error codes (0x80550Fxx / 0x805510xx) */
#define SHELL_NP_COMM2_ERR_NOT_INITIALIZED		-2141909247	/* 0x80550F01 */
#define SHELL_NP_COMM2_ERR_ALREADY_INITIALIZED		-2141909246	/* 0x80550F02 */
#define SHELL_NP_COMM2_ERR_INVALID_ARG			-2141909245	/* 0x80550F03 */
#define SHELL_NP_COMM2_ERR_UNSUPPORTED_VERSION		-2141909244	/* 0x80550F04 */
#define SHELL_NP_COMM2_ERR_CTX_MAX			-2141909243	/* 0x80550F05 */
#define SHELL_NP_COMM2_ERR_OUT_OF_MEMORY		-2141909237	/* 0x80550F0B */
#define SHELL_NP_COMM2_ERR_CTX_NOT_FOUND		-2141909236	/* 0x80550F0C */
#define SHELL_NP_COMM2_ERR_CTX_ID_FULL			-2141909235	/* 0x80550F0D */
#define SHELL_NP_COMM2_ERR_REQ_NOT_FOUND		-2141909234	/* 0x80550F0E */
#define SHELL_NP_COMM2_ERR_REQ_ID_FULL			-2141909233	/* 0x80550F0F */
#define SHELL_NP_COMM2_ERR_ABORTED			-2141909232	/* 0x80550F10 */
#define SHELL_NP_COMM2_ERR_FORM_OVERFLOW		-2141909231	/* 0x80550F11 */
#define SHELL_NP_COMM2_ERR_INSUFFICIENT_BUF		-2141909230	/* 0x80550F12 */
#define SHELL_NP_COMM2_ERR_INVALID_RESPONSE		-2141909229	/* 0x80550F13 */
#define SHELL_NP_COMM2_ERR_INVALID_RESULT_DATA		-2141909228	/* 0x80550F14 */
#define SHELL_NP_COMM2_ERR_CONSOLE_ID_BUF_SMALL		-2141909227	/* 0x80550F15 */
#define SHELL_NP_COMM2_ERR_SERVER_MAINTENANCE		-2141909226	/* 0x80550F16 */
#define SHELL_NP_COMM2_ERR_SERVER_UNKNOWN		-2141909225	/* 0x80550F17 */
#define SHELL_NP_COMM2_ERR_RESPONSE_BUF_TOO_SMALL	-2141909224	/* 0x80550F18 */
#define SHELL_NP_COMM2_ERR_REQ_MAX			-2141909223	/* 0x80550F19 */
#define SHELL_NP_COMM2_ERR_INVALID_OP			-2141909119	/* 0x80550F81 */
#define SHELL_NP_COMM2_ERR_INVALID_PROTO_VER		-2141909117	/* 0x80550F83 */
#define SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND		-2141909113	/* 0x80550F87 */
#define SHELL_NP_COMM2_ERR_RECORD_CUT_OFF		-2141909112	/* 0x80550F88 */
#define SHELL_NP_COMM2_ERR_RECORD_TOO_SHORT		-2141909111	/* 0x80550F89 */
#define SHELL_NP_COMM2_ERR_RECORD_TRUNCATED		-2141909110	/* 0x80550F8A */
#define SHELL_NP_COMM2_ERR_NO_ENVIRONMENT		-2141909109	/* 0x80550F8B */
#define SHELL_NP_COMM2_ERR_ENV_TOO_LONG			-2141909108	/* 0x80550F8C */
#define SHELL_NP_COMM2_ERR_CONTENT_TOO_LONG		-2141909104	/* 0x80550F90 */
#define SHELL_NP_COMM2_ERR_HAS_EXT_ERROR_RECORD		-2141909101	/* 0x80550F93 */
#define SHELL_NP_COMM2_ERR_INVALID_SERVICE_TYPE		-2141909098	/* 0x80550F96 */
#define SHELL_NP_COMM2_ERR_SERVER_BASE			0x80551000u
#define SHELL_NP_COMM2_ERR_SERVER_GENERIC		-2141908991	/* 0x80551001 */
#define SHELL_NP_COMM2_ERR_SERVER_CC_REJECTED		-2141908949	/* 0x8055102B */
#define SHELL_NP_COMM2_ERR_SERVER_ADDR_REJECTED		-2141908927	/* 0x80551041 */

/* Context / Request flags */
#define SHELL_NP_COMM2_CTX_FLAG_REMOVED			0x1u
#define SHELL_NP_COMM2_REQ_FLAG_REMOVED			0x1u
#define SHELL_NP_COMM2_REQ_FLAG_ABORTED			0x2u

/* Store / Account operations used by SceShell */
enum ShellNpCommerce2Op {
	SHELL_NP_COMM2_OP_CR_SESS		= 1,
	SHELL_NP_COMM2_OP_DEL_SESS		= 2,
	SHELL_NP_COMM2_OP_GET_CAT		= 3,
	SHELL_NP_COMM2_OP_GET_PROD		= 4,
	SHELL_NP_COMM2_OP_SRCH_PROD		= 5,
	SHELL_NP_COMM2_OP_GET_DRM		= 7,
	SHELL_NP_COMM2_OP_REG_CC		= 23,
	SHELL_NP_COMM2_OP_BIND_ACCOUNT		= 28,
	SHELL_NP_COMM2_OP_START_REG		= 29,
	SHELL_NP_COMM2_OP_SET_CRED		= 31,
	SHELL_NP_COMM2_OP_SET_RES_ADDR		= 38,
	SHELL_NP_COMM2_OP_SUBMIT_REG		= 48,
	SHELL_NP_COMM2_OP_AUTHENTICATE		= 49,
	SHELL_NP_COMM2_OP_GET_SRVS		= 53,
	SHELL_NP_COMM2_OP_GET_SRV_DETAIL	= 54,
	SHELL_NP_COMM2_OP_GET_SUBSCR		= 55,
	SHELL_NP_COMM2_OP_VERIFY_DOB		= 61,
	SHELL_NP_COMM2_OP_VERIFY_MASTER_BILL	= 64,
	SHELL_NP_COMM2_OP_GET_EPW		= 65,
	SHELL_NP_COMM2_OP_CR_GUEST_SESS		= 66,
	SHELL_NP_COMM2_OP_SET_SEC_Q		= 74,
	SHELL_NP_COMM2_OP_SUBMIT_UPGRADE	= 75,
	SHELL_NP_COMM2_OP_RECOM_PRODS		= 76,
	SHELL_NP_COMM2_OP_SEND_EMAIL_VER	= 100,
	SHELL_NP_COMM2_OP_MAX			= 114
};

/* Response header prepended to the server's TLV payload (0x64 bytes) */
struct ShellNpCommerce2ResponseHeader {
	SceUInt32 version;		/* +0x00 */
	SceUInt32 emptyString;		/* +0x04 */
	SceUInt32 length;		/* +0x08 */
	SceUInt32 status;		/* +0x0C */
	SceUInt32 nativeBuildVersion;	/* +0x10 */
	char currencyCode[4];		/* +0x14 */
	char currencySymbol[4];		/* +0x18 */
	char decimalPoint[4];		/* +0x1C */
	char thousandSeparator[4];	/* +0x20 */
	char reserved[64];		/* +0x24 */
};

/* Common view header for parsed response structs (0x5C bytes) */
struct ShellNpCommerce2CommonData {
	SceUInt32 version;		/* +0x00 */
	const char *bufHead;		/* +0x04 */
	SceSize bufSize;		/* +0x08 */
	const char *data;		/* +0x0C */
	SceSize dataSize;		/* +0x10 */
	const char *subData;		/* +0x14 */
	SceUInt32 reserved[17];		/* +0x18..+0x5B */
};

/* Session result wrapper (0x60 bytes) */
struct ShellNpCommerce2SessionResult {
	ShellNpCommerce2CommonData common;
	SceUInt32 tagCount;		/* +0x5C */
};

/* Authenticate result wrapper (0x70 bytes) */
struct ShellNpCommerce2AuthResult {
	ShellNpCommerce2CommonData common;
	SceUInt32 sessionCount;		/* +0x5C */
	SceUInt32 accountCount;		/* +0x60 */
	SceUInt32 entitlementCount;	/* +0x64 */
	SceUInt32 unused;		/* +0x68 */
	const char *redirectUrl;	/* +0x6C */
};

/* EPW result wrapper (0x60 bytes, matches GameStoreEpw) */
struct ShellNpCommerce2EpwResult {
	ShellNpCommerce2CommonData common;
	const char *epw;		/* +0x5C */
};

/* Parsed session info (0x98 bytes) */
struct ShellNpCommerce2SessionInfo {
	ShellNpCommerce2CommonData common;	/* +0x00..+0x5B */
	SceUInt32 padding;			/* +0x5C */
	SceUInt64 accountId;			/* +0x60 */
	const char *onlineId;			/* +0x68 */
	const char *currencyCode;		/* +0x6C */
	const char *currencySymbol;		/* +0x70 */
	SceUInt32 decimals;			/* +0x74 */
	const char *localeString;		/* +0x78 */
	SceUInt32 symbolPosition;		/* +0x7C */
	SceUInt32 symbolWithSpace;		/* +0x80 */
	SceUInt32 alignPad;			/* +0x84 */
	SceUInt64 serverTimestamp;		/* +0x88 */
	SceUInt16 storeVersion;			/* +0x90 */
	SceUInt16 tailPad;			/* +0x92 */
	SceUInt32 tailReserved;			/* +0x94 */
};

/* Active request belonging to a context (0x18 bytes) */
struct ShellNpCommerce2Request {
	ShellNpCommerce2Request *next;		/* +0x00 */
	int refCount;				/* +0x04 */
	SceUInt32 id;				/* +0x08 (ctxId << 16 | reqSeq) */
	SceUInt32 flags;			/* +0x0C */
	int httpRequest;			/* +0x10 */
	SceUInt16 op;				/* +0x14 */
	SceUInt16 pad;				/* +0x16 */
};

/* Commerce2 context (0x558 = 1368 bytes) */
struct ShellNpCommerce2Context {
	ShellNpCommerce2Context *next;		/* +0x000 */
	int refCount;				/* +0x004 */
	SceUInt32 version;			/* +0x008 */
	SceUInt32 id;				/* +0x00C */
	SceUInt32 flags;			/* +0x010 */
	SceUInt32 reserved;			/* +0x014 */
	int httpTemplate;			/* +0x018 */
	int httpConnection;			/* +0x01C */
	ShellNpCommerce2Request *requests;	/* +0x020 */
	char cookieHeader[256];			/* +0x024 */
	char cachedResponse[1024];		/* +0x124 */
	SceSize cachedResponseSize;		/* +0x524 */
	SceUInt32 serviceType;			/* +0x528 */
	SceUInt32 timePad;			/* +0x52C */
	SceUInt64 lastRequestTime;		/* +0x530 */
	int ctxPoolKind;			/* +0x538 */
	int nativeBuildVersion;			/* +0x53C */
	char currencyCode[4];			/* +0x540 */
	char currencySymbol[4];			/* +0x544 */
	char tailPad[16];			/* +0x548 */
};

/* Shared allocator in NP core (0x8132DA00, 0x8132DA08) */
extern void *npCommerce2Alloc(SceSize size);
extern void npCommerce2Free(void *ptr);

/* Shared table mutex and internal helpers across core/http TUs */
extern SceKernelLwMutexWork g_npComm2TableMutex;

int npComm2SetEnvironment(const char *env);				/* 0x812BFC40 */
int npComm2GetEnvironment(char *outEnv, SceSize maxLen);		/* 0x812BFC82 */
int npComm2SetCommonValue(int value);					/* 0x812BFD58 */
int npComm2SetEnvironmentType(int envType);				/* 0x812BFD66 */
int npComm2GetSystemTime(SceUInt64 *outTime);				/* 0x812BFD74 */

int npComm2ResolveEndpoint(SceUInt32 serviceType, SceUInt16 op,
			   const char **outPrefix, const char **outAction);	/* 0x812BFDF4 */
int npComm2SendEmptyAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			   SceUInt16 op, int *outHttpReq);			/* 0x812BFFDC */
int npComm2SendCredsAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			   SceUInt16 op, int *outHttpReq, int protoVer,
			   const char *username, const char *password,
			   const char *consoleId);				/* 0x812C0128 */
int npComm2SendStartRegAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			      SceUInt16 op, int *outHttpReq, int protoVer,
			      const char *country, const char *language,
			      int birthYear, int birthMonth, int birthDay,
			      const char *consoleId, int usePsnEntity);		/* 0x812C0558 */
int npComm2SendAuthAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			  SceUInt16 op, int *outHttpReq, int protoVer,
			  const char *username, const char *password,
			  const char *consoleId, int usePsnEntity);		/* 0x812C065E */
int npComm2SendVerifyDobAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			       SceUInt16 op, int *outHttpReq, int protoVer,
			       const char *loginName, int birthYear, int birthMonth,
			       int birthDay, const char *consoleId);		/* 0x812C082E */
int npComm2SendGuestSessAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			       SceUInt16 op, int *outHttpReq, int protoVer,
			       const char *country, const char *language,
			       int birthYear, int birthMonth, int birthDay,
			       const char *consoleId);				/* 0x812C0D0A */
int npComm2SendEmailVerAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			      SceUInt16 op, int *outHttpReq, int protoVer,
			      const char *username, const char *consoleId);	/* 0x812C0DF8 */

int npComm2AddContext(SceKernelLwMutexWork *lock, ShellNpCommerce2Context *ctx,
		      SceUInt32 *outCtxId, int poolKind);			/* 0x812C0F3C */
int npComm2AcquireContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId,
			  ShellNpCommerce2Context **outCtx, int rejectRemoved);	/* 0x812C1032 */
int npComm2ReleaseContext(SceKernelLwMutexWork *lock,
			  ShellNpCommerce2Context *ctx);			/* 0x812C10B6 */
int npComm2RemoveContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId);		/* 0x812C10FA */

int npComm2HttpCreateTemplate(int *outTmpl, int *outConn);			/* 0x812C1194 */
int npComm2HttpDeleteTemplate(int tmplId, int connId);				/* 0x812C11EC */
int npComm2HttpCreateRequest(int tmplId, SceUInt32 serviceType, SceUInt16 op,
			     const char *prefix, const char *action, int unusedFlag,
			     int *httpConn, int *outHttpReq,
			     SceUInt64 contentLength);				/* 0x812C1204 */
int npComm2HttpDeleteRequest(int httpReq);					/* 0x812C1366 */
int npComm2HttpSendRequest(int httpReq, const void *body, SceSize len);		/* 0x812C136E */
int npComm2HttpCheckStatus(int httpReq, int *nativeBuildVersion);		/* 0x812C1376 */
int npComm2HttpReadExact(int httpReq, char *buf, SceSize size);			/* 0x812C1452 */
int npComm2HttpReadBody(int httpReq, const char *base, char *dst, SceSize totalSize,
			int *outTruncated, int skipBytes);			/* 0x812C1480 */
int npComm2HttpAbortRequest(int httpReq);					/* 0x812C1598 */
int npComm2HttpResetConnection(int tmplId, int *httpConn);			/* 0x812C15A0 */
int npComm2HttpUriEscape(char *outBuf, SceSize outSize, const char *src,
			 SceSize srcLen, SceSize *required);			/* 0x812C15B2 */
int npComm2HttpUpdateCookie(int tmplId, int connId, int httpReq, char *cookieBuf,
			    SceSize cookieBufSize);				/* 0x812C15C2 */
int npComm2HttpSetThreadCookie(SceUID threadId, const char *cookie);		/* 0x812C1678 */
int npComm2HttpClearThreadCookie(void);						/* 0x812C1698 */

/* Public shell-internal C entrypoints (shared with game_plugin/license_updater.cpp) */
extern "C" {
int npCommerce2CoreInitialize(const char *env, int npCommonValue, int envType);	/* 0x812BD5A4 */
int SceNpCommerce2_AA090F5E(SceUInt32 version, SceUInt32 *outCtxId);		/* 0x812BD5D4 */
int sceNpCommerce2DestroyCtx(SceUInt32 ctxId);					/* 0x812BD5F8 */
int sceNpCommerce2DestroyReq(SceUInt32 reqId);					/* 0x812BD61A */
int sceNpCommerce2AbortReq(SceUInt32 reqId);					/* 0x812BD63C */
int sceNpCommerce2CreateSessionCreateReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD65E */
int sceNpCommerce2CreateSessionStart(SceUInt32 reqId);				/* 0x812BD68A */
int SceNpCommerce2_B7387C96(SceUInt32 reqId, void *buf, SceSize bufLen,
			    SceSize *fillSize);					/* 0x812BD6AC */
int SceNpCommerce2_C737A929(SceUInt32 ctxId, SceUInt32 *outReqId);		/* 0x812BD6CC */
int SceNpCommerce2_2DC2A0F9(SceUInt32 reqId);					/* 0x812BD6F8 */
int npCommerce2CreateBindAccountReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD71A */
int npCommerce2StartBindAccountReq(SceUInt32 reqId, const char *username,
				   const char *password);			/* 0x812BD746 */
int npCommerce2CreateStartRegReq(SceUInt32 ctxId, SceUInt32 *outReqId);		/* 0x812BD772 */
int npCommerce2StartStartRegReq(SceUInt32 reqId, const char *country,
				const char *language, int birthYear, int birthMonth,
				int birthDay, int usePsnEntity);		/* 0x812BD79E */
int npCommerce2CreateAuthenticateReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD7E4 */
int npCommerce2StartAuthenticateReq(SceUInt32 reqId, int useExplicitCreds,
				    const char *username, const char *password,
				    int usePsnEntity);				/* 0x812BD810 */
int npCommerce2CreateVerifyDobReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD84C */
int npCommerce2StartVerifyDobReq(SceUInt32 reqId, const char *loginName,
				 int birthYear, int birthMonth, int birthDay);	/* 0x812BD878 */
int SceNpCommerce2_07C16199(SceUInt32 ctxId, SceUInt32 *outReqId);		/* 0x812BD8B8 */
int SceNpCommerce2_D35F548B(SceUInt32 reqId);					/* 0x812BD8E4 */
int SceNpCommerce2_5953F48D(ShellNpCommerce2EpwResult *result, const void *data,
			    SceSize size);					/* 0x812BD906 */
int npCommerce2CreateSendEmailVerReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD934 */
int npCommerce2StartSendEmailVerReq(SceUInt32 reqId, const char *username);	/* 0x812BD960 */
int npCommerce2CreateGuestSessionReq(SceUInt32 ctxId, SceUInt32 *outReqId);	/* 0x812BD98C */
int npCommerce2StartGuestSessionReq(SceUInt32 reqId, const char *country,
				    const char *language, int birthYear,
				    int birthMonth, int birthDay);		/* 0x812BD9B8 */
int npCommerce2BindThreadCookie(SceUID threadId, const char *cookie);		/* 0x812BDA02 */
int npCommerce2UnbindThreadCookie(void);					/* 0x812BDA2E */
int npCommerce2GetContextCookie(SceUInt32 ctxId, const char **outCookie);	/* 0x812BDA4A */
int npCommerce2SetContextServiceType(SceUInt32 ctxId, SceUInt32 serviceType);	/* 0x812BDA76 */
int npCommerce2GetContextBuildVersion(SceUInt32 ctxId, int *outBuildVersion);	/* 0x812BDAA4 */
int npCommerce2CoreInit(void);							/* 0x812BFD84 */
}

#endif /* SHELL_NP_COMMERCE2_CORE_H */
