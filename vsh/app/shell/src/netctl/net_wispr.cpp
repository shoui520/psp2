/*
 * net_wispr.cpp: SceNetWispr (WISPr 1.0 captive-portal HTTP/XML authenticator,
 * streaming XML parser, and AT&T hotspot credential provider) inside SceShell.
 * 3.65-CEX: 0x8121ADB0 - 0x8121CFE2
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include "netctl_services.h"

#define NET_WISPR_STATE_IDLE		0
#define NET_WISPR_STATE_REDIRECT	1
#define NET_WISPR_STATE_AUTH		2
#define NET_WISPR_STATE_LOGOFF		3

#define NET_WISPR_FLAG_IDLE		0x1u
#define NET_WISPR_FLAG_EXIT		0x2u
#define NET_WISPR_FLAG_MUTEX_CREATED	0x4u

#define NET_WISPR_EV_START		0x01u
#define NET_WISPR_EV_ABORT		0x02u
#define NET_WISPR_EV_EXIT		0x08u
#define NET_WISPR_EV_LOGOFF		0x10u

#define NET_WISPR_ACTION_CONTINUE	0
#define NET_WISPR_ACTION_ABORTED	1
#define NET_WISPR_ACTION_DONE		2
#define NET_WISPR_ACTION_AUTH_FAILED	3

#define NET_WISPR_HTTP_SSL_ERROR	-2143088523	/* 0x80431075 */

#define NET_WISPR_XML_TAG_BUF_SIZE	255
#define NET_WISPR_XML_VAL_BUF_SIZE	511
#define NET_WISPR_XML_MAX_DEPTH		8

#define NET_WISPR_XML_EV_START		1
#define NET_WISPR_XML_EV_END		2
#define NET_WISPR_XML_EV_TEXT		3
#define NET_WISPR_XML_EV_ATTR		4

#define NET_WISPR_PARSE_FLAG_ROOT	0x01u
#define NET_WISPR_PARSE_FLAG_REDIRECT	0x02u
#define NET_WISPR_PARSE_FLAG_AUTH_REPLY	0x04u
#define NET_WISPR_PARSE_FLAG_MSG_TYPE	0x08u
#define NET_WISPR_PARSE_FLAG_RESP_CODE	0x10u
#define NET_WISPR_PARSE_FLAG_LOGIN_URL	0x20u
#define NET_WISPR_PARSE_FLAG_AUTH_SEED	0x40u
#define NET_WISPR_PARSE_FLAG_VALID	0x80u

typedef struct NetWisprContext {
	int state;			/* +0x00 */
	int eventCode;			/* +0x04 */
	SceUID threadId;		/* +0x08 */
	SceUID eventFlagId;		/* +0x0C */
	SceKernelLwMutexWork mutex;	/* +0x10 */
	int ispId;			/* +0x30 */
	ShellNetWisprParam param;	/* +0x34 (44 bytes) */
	int lastError;			/* +0x60 */
	SceUInt32 flags;		/* +0x64 */
	NetWisprResultInfo resultInfo;	/* +0x68 (0x304 bytes) */
	ShellNetWisprCallback callback;	/* +0x36C */
	int callbackArg;		/* +0x370 */
	char postBody[256];		/* +0x374 */
	SceUInt32 tailPad;		/* +0x474 */
} NetWisprContext;

typedef struct NetWisprXmlTagEntry {
	const char *name;
	int hasTextValue;
	int matchSuffix;
} NetWisprXmlTagEntry;

typedef struct NetWisprXmlSchema {
	const NetWisprXmlTagEntry *const *levels;
	SceUInt32 levelCount;
} NetWisprXmlSchema;

struct NetWisprXmlParser;
typedef int (*NetWisprXmlCallback)(struct NetWisprXmlParser *parser, int eventType,
				   int depth, int tagIndex, const char *value,
				   const char *attrValue, void *userArg);

typedef struct NetWisprXmlParser {
	const NetWisprXmlSchema *schema;		/* +0x000 */
	char tagName[NET_WISPR_XML_TAG_BUF_SIZE];	/* +0x004 */
	char textBuf[NET_WISPR_XML_VAL_BUF_SIZE];	/* +0x103 */
	SceUInt8 partialBuf[6];				/* +0x302 */
	SceUInt32 partialLen;				/* +0x308 */
	SceUInt32 partialNeeded;			/* +0x30C */
	SceUInt32 depth;				/* +0x310 */
	int tagStack[NET_WISPR_XML_MAX_DEPTH];		/* +0x314 */
	SceUInt32 state;				/* +0x334 */
	SceUInt32 attrFlags;				/* +0x338 */
	SceUInt32 writePos;				/* +0x33C */
	int isSelfClosing;				/* +0x340 */
	NetWisprXmlCallback callback;			/* +0x344 */
	void *userArg;					/* +0x348 */
} NetWisprXmlParser;

typedef struct NetWisprXmlSession {
	NetWisprXmlParser parser;	/* +0x000 (0x34C bytes) */
	NetWisprContext *wisprCtx;	/* +0x34C */
	int phase;			/* +0x350 (1=redirect, 2=auth, 3=logoff) */
	SceUInt32 pad;			/* +0x354 */
	SceUInt32 seenFlags;		/* +0x358 */
	int messageType;		/* +0x35C */
	int responseCode;		/* +0x360 */
	char loginUrl[256];		/* +0x364 */
	char logoffUrl[256];		/* +0x464 */
	char replyMessage[256];		/* +0x564 */
	char authSeed[256];		/* +0x664 */
} NetWisprXmlSession;

typedef struct NetWisprIspOps {
	int (*configureTemplate)(NetWisprContext *ctx, int tmplId);
	int (*getProbeUrl)(NetWisprContext *ctx, const char **outUrl);
	int (*checkProbeStatus)(NetWisprContext *ctx, int statusCode, int *outAction);
	int (*handleProbeReply)(NetWisprContext *ctx, int reqId, int *outAction,
				const char **outLoginUrl, const char **outRedirectUrl,
				SceSize *outRedirectLen);
	int (*buildAuthPostBody)(NetWisprContext *ctx, char *outBody, SceSize bodySize);
	int (*checkAuthStatus)(NetWisprContext *ctx, int statusCode, int *outAction);
	int (*handleAuthReply)(NetWisprContext *ctx, int reqId, int *outAction,
			       int *outResponseCode, const char **outReplyMessage,
			       const char **outLoginUrl, SceSize *outLoginUrlLen,
			       const char **outLogoffUrl);
} NetWisprIspOps;

typedef int (*NetWisprStateHandler)(NetWisprContext *ctx, int prevState,
				    int *outNextState, int *outEvent);

typedef struct NetWisprEncryptedBlock {
	SceUInt32 length;
	SceUInt8 payload[32];
} NetWisprEncryptedBlock;

typedef struct NetWisprDecryptedBlock {
	SceUInt32 length;
	SceUInt8 magic[8];
	SceUInt8 secret[24];
} NetWisprDecryptedBlock;

static int g_wisprInitialized = 0;		/* 0x815450E0 */
static int g_wisprHttpTemplate = 0;		/* 0x815450E4 */
static int g_wisprHttpConn = 0;			/* 0x815450E8 */
static int g_wisprHttpReq = 0;			/* 0x815450EC */
static NetWisprContext g_wisprContext;		/* 0x815A1C80 */
static char g_wisprLogoffUrls[4][256];		/* 0x815A20F8 */
static SceUInt8 g_wisprReadBuf[256];		/* 0x815A24F8 */
static NetWisprXmlSession g_wisprXmlSession;	/* 0x815A25F8 */
static char g_wisprAttAuthSeed[128];		/* 0x815A2D60 */

/* AT&T WISPr portability-encrypted shared secret material (0x81513F88..0x81545148) */
static const SceUInt8 s_attPortabilityIv[16] = {
	0xd1, 0x51, 0x1b, 0x2b, 0xe3, 0x05, 0x82, 0x87,
	0x92, 0x30, 0x06, 0xe2, 0x1e, 0x73, 0x3b, 0x12
};

static const NetWisprEncryptedBlock s_attKeyHead = {
	0x10u,
	{
		0xcd, 0x8e, 0xd0, 0x9d, 0x49, 0x7d, 0x8e, 0x56,
		0xbc, 0x91, 0xdb, 0xa9, 0xdc, 0x4a, 0xad, 0x0d
	}
};

static const NetWisprEncryptedBlock s_attKeyTail = {
	0x20u,
	{
		0xd9, 0x29, 0x1f, 0x48, 0xc9, 0x96, 0x64, 0x43,
		0x89, 0xa6, 0xa3, 0x24, 0xab, 0xae, 0x14, 0xbb,
		0xc5, 0x09, 0xa5, 0x54, 0x12, 0x1a, 0xd9, 0x7e,
		0x4b, 0xbd, 0x46, 0xcd, 0xc1, 0x21, 0x80, 0x4f
	}
};

static const NetWisprEncryptedBlock s_attAltKeyHead = {
	0x10u,
	{
		0x06, 0xb9, 0x25, 0x97, 0xa3, 0x80, 0xbf, 0x5d,
		0x22, 0x11, 0x89, 0xf4, 0x6c, 0x19, 0x6d, 0x29
	}
};

static const NetWisprEncryptedBlock s_attAltKeyTail = {
	0x20u,
	{
		0x06, 0x1c, 0x1c, 0x85, 0x7d, 0x91, 0xd5, 0xce,
		0x37, 0xd7, 0xf1, 0x2b, 0x7d, 0xf9, 0xfa, 0xc2,
		0xf3, 0x0c, 0xa1, 0xd4, 0x88, 0x36, 0xe4, 0x59,
		0xcb, 0x28, 0x6d, 0x34, 0x6d, 0x14, 0x11, 0xfc
	}
};

static const SceUInt8 s_attDecryptedMagic[8] = {
	0x65, 0x84, 0x89, 0x98, 0x42, 0x6d, 0xd3, 0xe9
};

static const NetWisprXmlTagEntry s_wisprXmlLevelRoot[] = {
	{ "XXX", 1, 0 },
	{ "WISPAccessGatewayParam", 0, 0 },
	{ "", 1, 0 }
};

static const NetWisprXmlTagEntry s_wisprXmlLevelSection[] = {
	{ "XXX", 1, 0 },
	{ "Redirect", 0, 0 },
	{ "Proxy", 0, 0 },
	{ "AuthenticationReply", 0, 0 },
	{ "AuthenticationPollReply", 0, 0 },
	{ "LogoffReply", 0, 0 },
	{ "AbortLoginReply", 0, 0 },
	{ "", 1, 0 }
};

static const NetWisprXmlTagEntry s_wisprXmlLevelLeaf[] = {
	{ "XXX", 1, 0 },
	{ "AccessProcedure", 1, 0 },
	{ "AccessLocation", 1, 0 },
	{ "LocationName", 1, 0 },
	{ "LoginURL", 1, 0 },
	{ "AbortLoginURL", 1, 0 },
	{ "MessageType", 1, 0 },
	{ "ResponseCode", 1, 0 },
	{ "NextURL", 1, 0 },
	{ "Delay", 1, 0 },
	{ "ReplyMessage", 1, 0 },
	{ "LoginResultsURL", 1, 0 },
	{ "LogoffURL", 1, 0 },
	{ "AuthenticationSeed", 1, 0 },
	{ "", 1, 0 }
};

static const NetWisprXmlTagEntry *const s_wisprXmlLevels[3] = {
	s_wisprXmlLevelRoot,
	s_wisprXmlLevelSection,
	s_wisprXmlLevelLeaf
};

static const NetWisprXmlSchema s_wisprXmlSchema = {
	s_wisprXmlLevels,
	3
};

static int netWisprWorkerThread(SceSize args, void *argp);
static int netWisprStateIdle(NetWisprContext *ctx, int prevState,
			     int *outNextState, int *outEvent);
static int netWisprStateRedirect(NetWisprContext *ctx, int prevState,
				 int *outNextState, int *outEvent);
static int netWisprStateAuth(NetWisprContext *ctx, int prevState,
			     int *outNextState, int *outEvent);
static int netWisprStateLogoff(NetWisprContext *ctx, int prevState,
			       int *outNextState, int *outEvent);

static int netWisprHttpInit(void);
static void netWisprHttpTerm(void);
static int netWisprRunRedirect(NetWisprContext *ctx, int *outAction);
static int netWisprRunAuth(NetWisprContext *ctx, int *outAction);
static int netWisprRunLogoff(NetWisprContext *ctx, int *outAction);
static int netWisprAbortHttpRequest(void);
static int netWisprXmlSessionBegin(int phase, NetWisprContext *ctx);
static int netWisprXmlSessionFeed(const void *buf, int len);
static int netWisprXmlHandler(NetWisprXmlParser *parser, int eventType,
			      int depth, int tagIndex, const char *value,
			      const char *attrValue, void *userArg);
static int netWisprXmlParserInit(NetWisprXmlParser *parser,
				 const NetWisprXmlSchema *schema,
				 NetWisprXmlCallback callback, void *userArg);
static int netWisprXmlParserFeed(NetWisprXmlParser *parser, const SceUInt8 *input, int len);
static int netWisprDecodeChar(SceUInt8 *outBuf, SceSize outCap,
			      const SceUInt8 *inBuf, SceSize inLen,
			      SceSize *outDecodedLen, SceSize *outConsumedLen);

static int netWisprAttConfigure(NetWisprContext *ctx, int tmplId);
static int netWisprAttGetProbeUrl(NetWisprContext *ctx, const char **outUrl);
static int netWisprAttCheckProbeStatus(NetWisprContext *ctx, int statusCode, int *outAction);
static int netWisprAttHandleProbeReply(NetWisprContext *ctx, int reqId, int *outAction,
				       const char **outLoginUrl, const char **outRedirectUrl,
				       SceSize *outRedirectLen);
static int netWisprAttBuildAuthBody(NetWisprContext *ctx, char *outBody, SceSize bodySize);
static int netWisprAttCheckAuthStatus(NetWisprContext *ctx, int statusCode, int *outAction);
static int netWisprAttHandleAuthReply(NetWisprContext *ctx, int reqId, int *outAction,
				      int *outResponseCode, const char **outReplyMessage,
				      const char **outLoginUrl, SceSize *outLoginUrlLen,
				      const char **outLogoffUrl);

/* 0x81513C40 */
static const NetWisprStateHandler s_wisprStateHandlers[4] = {
	netWisprStateIdle,
	netWisprStateRedirect,
	netWisprStateAuth,
	netWisprStateLogoff
};

/* 0x81513F00 */
static const NetWisprIspOps s_wisprIspTable[1] = {
	{
		netWisprAttConfigure,
		netWisprAttGetProbeUrl,
		netWisprAttCheckProbeStatus,
		netWisprAttHandleProbeReply,
		netWisprAttBuildAuthBody,
		netWisprAttCheckAuthStatus,
		netWisprAttHandleAuthReply
	}
};

/* 0x8121AE40 */
static int netWisprInitInternal(ShellNetWisprCallback callback, SceSize stackSize,
				int priority, int callbackArg)
{
	NetWisprContext *ctx = &g_wisprContext;
	int ret = netWisprHttpInit();

	if (ret >= 0) {
		sceClibMemset(ctx, 0, sizeof(*ctx));
		ctx->flags |= NET_WISPR_FLAG_IDLE;
		ctx->threadId = -1;
		ctx->eventFlagId = -1;
		ctx->callback = callback;
		ctx->callbackArg = callbackArg;

		ret = sceKernelCreateLwMutex(&ctx->mutex, "SceNetWispr", 2, 0, NULL);
		if (ret >= 0) {
			ctx->flags |= NET_WISPR_FLAG_MUTEX_CREATED;
			ret = sceKernelCreateEventFlag("SceNetWispr", 0, 0, NULL);
			if (ret >= 0) {
				ctx->eventFlagId = ret;
				ret = sceKernelCreateThread("SceNetWispr", netWisprWorkerThread,
							    priority, stackSize, 0, 0, NULL);
				if (ret >= 0) {
					ctx->threadId = ret;
					NetWisprContext *threadArg = ctx;
					ret = sceKernelStartThread(ctx->threadId,
								   sizeof(threadArg), &threadArg);
					if (ret >= 0)
						return 0;
				}
			}
		}
	}

	sceKernelSetEventFlag(ctx->eventFlagId, NET_WISPR_EV_EXIT);
	if (ctx->threadId >= 0) {
		sceKernelWaitThreadEnd(ctx->threadId, NULL, NULL);
		sceKernelDeleteThread(ctx->threadId);
		ctx->threadId = -1;
	}
	if (ctx->eventFlagId >= 0) {
		sceKernelDeleteEventFlag(ctx->eventFlagId);
		ctx->eventFlagId = -1;
	}
	if ((ctx->flags & NET_WISPR_FLAG_MUTEX_CREATED) != 0) {
		sceKernelDeleteLwMutex(&ctx->mutex);
		ctx->flags &= ~NET_WISPR_FLAG_MUTEX_CREATED;
	}
	netWisprHttpTerm();
	return ret;
}

/* 0x8121ADB0 */
int shellNetWisprInit(ShellNetWisprCallback callback, SceSize stackSize,
		      int priority, int callbackArg)
{
	if (g_wisprInitialized == 1)
		return NET_WISPR_ERROR_ALREADY_INITIALIZED;

	int ret = netWisprInitInternal(callback, stackSize, priority, callbackArg);
	if (ret >= 0) {
		g_wisprInitialized = 1;
		return 0;
	}
	return ret;
}

/* 0x8121AFBA */
static void netWisprTermInternal(void)
{
	NetWisprContext *ctx = &g_wisprContext;

	if ((ctx->flags & NET_WISPR_FLAG_IDLE) == 0)
		shellNetWisprStop();

	sceKernelSetEventFlag(ctx->eventFlagId, NET_WISPR_EV_EXIT);
	if (ctx->threadId >= 0) {
		sceKernelWaitThreadEnd(ctx->threadId, NULL, NULL);
		sceKernelDeleteThread(ctx->threadId);
		ctx->threadId = -1;
	}
	if (ctx->eventFlagId >= 0) {
		sceKernelDeleteEventFlag(ctx->eventFlagId);
		ctx->eventFlagId = -1;
	}
	if ((ctx->flags & NET_WISPR_FLAG_MUTEX_CREATED) != 0) {
		sceKernelDeleteLwMutex(&ctx->mutex);
		ctx->flags &= ~NET_WISPR_FLAG_MUTEX_CREATED;
	}
	netWisprHttpTerm();
}

/* 0x8121ADE2 */
void shellNetWisprTerm(void)
{
	if (g_wisprInitialized != 0) {
		netWisprTermInternal();
		g_wisprInitialized = 0;
	}
}

/* 0x8121B058 */
static int netWisprStartInternal(int ispId, const ShellNetWisprParam *param)
{
	NetWisprContext *ctx = &g_wisprContext;

	if ((ctx->flags & NET_WISPR_FLAG_IDLE) == 0)
		return NET_WISPR_ERROR_BUSY;

	ctx->ispId = ispId;
	ctx->flags &= ~NET_WISPR_FLAG_IDLE;
	sceClibMemcpy_safe(&ctx->param, param, sizeof(ctx->param));
	sceKernelSetEventFlag(ctx->eventFlagId, NET_WISPR_EV_START);
	return 0;
}

/* 0x8121AE02 */
int shellNetWisprStart(int ispId, const ShellNetWisprParam *param)
{
	if (g_wisprInitialized == 0)
		return NET_WISPR_ERROR_NOT_INITIALIZED;
	return netWisprStartInternal(ispId, param);
}

/* 0x8121B094 */
static int netWisprStopInternal(void)
{
	netWisprAbortHttpRequest();
	sceKernelSetEventFlag(g_wisprContext.eventFlagId, NET_WISPR_EV_ABORT);
	return 0;
}

/* 0x8121AE20 */
int shellNetWisprStop(void)
{
	if (g_wisprInitialized == 0)
		return NET_WISPR_ERROR_NOT_INITIALIZED;
	return netWisprStopInternal();
}

/* 0x8121B0B0 */
static int netWisprNotifyCallback(int prevState)
{
	NetWisprContext *ctx = &g_wisprContext;

	if (sceKernelLockLwMutex(&ctx->mutex, 1, NULL) >= 0) {
		if (ctx->callback != NULL) {
			int err = (ctx->eventCode == 4) ? ctx->lastError : 0;
			ctx->callback(prevState, ctx->state, ctx->eventCode, err,
				      (int)&ctx->resultInfo, ctx->callbackArg);
		}
		sceKernelUnlockLwMutex(&ctx->mutex, 1);
	}
	return 0;
}

/* 0x8121B104 */
static int netWisprWorkerThread(SceSize args, void *argp)
{
	NetWisprContext *ctx = *(NetWisprContext **)argp;
	int nextState = 0;
	int nextEvent = 0;

	(void)args;
	s_wisprStateHandlers[ctx->state](ctx, 0, &nextState, &nextEvent);
	while (1) {
		int prevState = ctx->state;
		if (prevState == NET_WISPR_STATE_IDLE &&
		    (ctx->flags & NET_WISPR_FLAG_EXIT) != 0) {
			break;
		}
		ctx->state = nextState;
		ctx->eventCode = nextEvent;
		netWisprNotifyCallback(prevState);
		s_wisprStateHandlers[ctx->state](ctx, prevState, &nextState, &nextEvent);
	}
	sceKernelExitThread(0);
	return 0;
}

/* 0x8121B17A */
static int netWisprStateIdle(NetWisprContext *ctx, int prevState,
			     int *outNextState, int *outEvent)
{
	SceUInt32 outBits = 0;

	(void)prevState;
	ctx->lastError = 0;
	sceClibMemset(&ctx->resultInfo, 0, sizeof(ctx->resultInfo));
	sceKernelWaitEventFlag(ctx->eventFlagId, 0xFFFFFFFFu,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       &outBits, NULL);
	if ((outBits & NET_WISPR_EV_EXIT) != 0) {
		ctx->flags |= NET_WISPR_FLAG_EXIT;
	} else if ((outBits & NET_WISPR_EV_ABORT) != 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
		ctx->flags |= NET_WISPR_FLAG_IDLE;
	} else if ((outBits & NET_WISPR_EV_START) != 0) {
		*outNextState = NET_WISPR_STATE_REDIRECT;
		*outEvent = 0;
	} else if ((outBits & NET_WISPR_EV_LOGOFF) != 0) {
		*outNextState = NET_WISPR_STATE_LOGOFF;
		*outEvent = 5;
	}
	return 0;
}

/* 0x8121B216 */
static int netWisprStateRedirect(NetWisprContext *ctx, int prevState,
				 int *outNextState, int *outEvent)
{
	int action = NET_WISPR_ACTION_CONTINUE;
	int ret = netWisprRunRedirect(ctx, &action);

	(void)prevState;
	if (sceKernelPollEventFlag(ctx->eventFlagId, NET_WISPR_EV_ABORT,
				   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				   NULL) == 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else if (ret < 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 4;
		ctx->lastError = ret;
	} else if (action == NET_WISPR_ACTION_DONE) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 2;
	} else if (action == NET_WISPR_ACTION_ABORTED) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else {
		*outNextState = NET_WISPR_STATE_AUTH;
		*outEvent = 1;
	}

	if (*outNextState == NET_WISPR_STATE_IDLE)
		ctx->flags |= NET_WISPR_FLAG_IDLE;
	return 0;
}

/* 0x8121B2B0 */
static int netWisprStateAuth(NetWisprContext *ctx, int prevState,
			     int *outNextState, int *outEvent)
{
	int action = NET_WISPR_ACTION_CONTINUE;
	int ret = netWisprRunAuth(ctx, &action);

	(void)prevState;
	if (sceKernelPollEventFlag(ctx->eventFlagId, NET_WISPR_EV_ABORT,
				   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				   NULL) == 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else if (ret < 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 4;
		ctx->lastError = ret;
	} else if (action == NET_WISPR_ACTION_AUTH_FAILED) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 4;
		ctx->lastError = NET_WISPR_ERROR_AUTH_FAILED;
	} else if (action == NET_WISPR_ACTION_ABORTED) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 2;
	}

	if (*outNextState == NET_WISPR_STATE_IDLE)
		ctx->flags |= NET_WISPR_FLAG_IDLE;
	return 0;
}

/* 0x8121B34A */
static int netWisprStateLogoff(NetWisprContext *ctx, int prevState,
			       int *outNextState, int *outEvent)
{
	int action = NET_WISPR_ACTION_CONTINUE;
	int ret = netWisprRunLogoff(ctx, &action);

	(void)prevState;
	if (sceKernelPollEventFlag(ctx->eventFlagId, NET_WISPR_EV_ABORT,
				   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				   NULL) == 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else if (ret < 0) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 4;
		ctx->lastError = ret;
	} else if (action == NET_WISPR_ACTION_ABORTED) {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 3;
	} else {
		*outNextState = NET_WISPR_STATE_IDLE;
		*outEvent = 6;
	}

	if (*outNextState == NET_WISPR_STATE_IDLE)
		ctx->flags |= NET_WISPR_FLAG_IDLE;
	return 0;
}

/* 0x8121B3D0 */
static int netWisprHttpInit(void)
{
	sceClibMemset(g_wisprLogoffUrls, 0, sizeof(g_wisprLogoffUrls));
	int tmpl = sceHttpCreateTemplate("WISPr/1.0", SCE_HTTP_VERSION_1_1, SCE_FALSE);
	if (tmpl >= 0) {
		g_wisprHttpTemplate = tmpl;
		return sceHttpsDisableOptionPrivate(tmpl, 26);
	}
	return tmpl;
}

/* 0x8121B40A */
static void netWisprHttpTerm(void)
{
	if (g_wisprHttpTemplate > 0) {
		sceHttpDeleteTemplate(g_wisprHttpTemplate);
		g_wisprHttpTemplate = 0;
	}
}

static int netWisprResolveHttpError(int reqId, int httpErr)
{
	if (httpErr == NET_WISPR_HTTP_SSL_ERROR) {
		int sslErr = 0;
		SceUInt32 sslDetail = 0;
		int ret = sceHttpsGetSslError(reqId, &sslErr, &sslDetail);
		if (ret >= 0)
			return sslErr;
		return ret;
	}
	return httpErr;
}

/* 0x8121B42C */
static int netWisprReadAndParseXml(NetWisprContext *ctx, int phase, int reqId,
				   SceUInt8 *readBuf, SceSize readBufSize)
{
	int ret = netWisprXmlSessionBegin(phase, ctx);
	if (ret < 0)
		return ret;

	bool reachedEof = false;
	while (1) {
		int readLen = sceHttpReadData(reqId, readBuf, readBufSize);
		if (readLen < 0)
			return netWisprResolveHttpError(reqId, readLen);
		if (readLen == 0)
			reachedEof = true;

		ret = netWisprXmlSessionFeed(readBuf, readLen);
		if (ret < 0)
			return ret;
		if (reachedEof)
			return 0;
	}
}

static void netWisprCloseRequestAndConn(void)
{
	if (g_wisprHttpReq > 0) {
		sceHttpDeleteRequest(g_wisprHttpReq);
		g_wisprHttpReq = 0;
	}
	if (g_wisprHttpConn > 0) {
		sceHttpDeleteConnection(g_wisprHttpConn);
		g_wisprHttpConn = 0;
	}
}

/* 0x8121B4B0 */
static int netWisprRunRedirect(NetWisprContext *ctx, int *outAction)
{
	const NetWisprIspOps *ops = &s_wisprIspTable[ctx->ispId];
	const char *probeUrl = NULL;
	const char *loginUrl = NULL;
	const char *redirectUrl = NULL;
	SceSize redirectLen = 0;
	int statusCode = 0;

	int ret = ops->configureTemplate(ctx, g_wisprHttpTemplate);
	if (ret >= 0)
		ret = ops->getProbeUrl(ctx, &probeUrl);
	if (ret >= 0) {
		int connId = sceHttpCreateConnectionWithURL(g_wisprHttpTemplate, probeUrl,
							    SCE_TRUE);
		ret = connId;
		if (connId >= 0) {
			g_wisprHttpConn = connId;
			int reqId = sceHttpCreateRequestWithURL(connId, SCE_HTTP_METHOD_GET,
								probeUrl, 0);
			ret = reqId;
			if (reqId >= 0) {
				g_wisprHttpReq = reqId;
				ret = sceHttpSetIcmOption(reqId, 1);
				if (ret >= 0) {
					ret = sceHttpSendRequest(reqId, NULL, 0);
					if (ret < 0)
						ret = netWisprResolveHttpError(reqId, ret);
				}
				if (ret >= 0)
					ret = sceHttpGetStatusCode(reqId, &statusCode);
				if (ret >= 0)
					ret = ops->checkProbeStatus(ctx, statusCode, outAction);
				if (ret >= 0 && *outAction != NET_WISPR_ACTION_DONE) {
					ret = netWisprReadAndParseXml(ctx, 1, reqId,
								      g_wisprReadBuf,
								      sizeof(g_wisprReadBuf));
					if (ret >= 0) {
						ret = ops->handleProbeReply(ctx, reqId, outAction,
									    &loginUrl,
									    &redirectUrl,
									    &redirectLen);
					}
					if (ret >= 0) {
						if (*outAction == NET_WISPR_ACTION_DONE) {
							if (redirectUrl != NULL) {
								if (redirectLen >= sizeof(ctx->resultInfo.loginUrl))
									redirectLen = sizeof(ctx->resultInfo.loginUrl) - 1;
								sceClibStrncpy(ctx->resultInfo.loginUrl,
									       redirectUrl,
									       redirectLen);
								ctx->resultInfo.loginUrl[redirectLen] = '\0';
							} else {
								sceClibMemset(ctx->resultInfo.loginUrl, 0,
									      sizeof(ctx->resultInfo.loginUrl));
							}
						} else {
							sceClibStrncpy(ctx->resultInfo.loginUrl,
								       loginUrl,
								       sizeof(ctx->resultInfo.loginUrl));
							ctx->resultInfo.loginUrl[sizeof(ctx->resultInfo.loginUrl) - 1] = '\0';
						}
					}
				}
			}
		}
	}

	netWisprCloseRequestAndConn();
	return ret;
}

/* 0x8121B6E6 */
static int netWisprRunAuth(NetWisprContext *ctx, int *outAction)
{
	const NetWisprIspOps *ops = &s_wisprIspTable[ctx->ispId];
	const char *replyMessage = NULL;
	const char *nextLoginUrl = NULL;
	const char *logoffUrl = NULL;
	SceSize nextLoginUrlLen = 0;
	int statusCode = 0;

	int bodyLen = ops->buildAuthPostBody(ctx, ctx->postBody, sizeof(ctx->postBody));
	int ret = bodyLen;
	if (bodyLen >= 0) {
		int connId = sceHttpCreateConnectionWithURL(g_wisprHttpTemplate,
							    ctx->resultInfo.loginUrl, SCE_TRUE);
		ret = connId;
		if (connId >= 0) {
			g_wisprHttpConn = connId;
			int reqId = sceHttpCreateRequestWithURL(connId, SCE_HTTP_METHOD_POST,
								ctx->resultInfo.loginUrl,
								(SceULong64)bodyLen);
			ret = reqId;
			if (reqId >= 0) {
				g_wisprHttpReq = reqId;
				ret = sceHttpSetIcmOption(reqId, 1);
				if (ret >= 0) {
					ret = sceHttpAddRequestHeader(
						reqId, "Content-Type",
						"application/x-www-form-urlencoded", 0);
				}
				if (ret >= 0) {
					ret = sceHttpSendRequest(reqId, ctx->postBody,
								 (SceSize)bodyLen);
					if (ret < 0)
						ret = netWisprResolveHttpError(reqId, ret);
				}
				if (ret >= 0)
					ret = sceHttpGetStatusCode(reqId, &statusCode);
				if (ret >= 0)
					ret = ops->checkAuthStatus(ctx, statusCode, outAction);
				if (ret >= 0 && *outAction != NET_WISPR_ACTION_DONE) {
					ret = netWisprReadAndParseXml(ctx, 2, reqId,
								      g_wisprReadBuf,
								      sizeof(g_wisprReadBuf));
					if (ret >= 0) {
						ret = ops->handleAuthReply(
							ctx, reqId, outAction,
							&ctx->resultInfo.responseCode,
							&replyMessage, &nextLoginUrl,
							&nextLoginUrlLen, &logoffUrl);
					}
					if (ret >= 0) {
						if (replyMessage != NULL) {
							sceClibStrncpy(ctx->resultInfo.replyMessage,
								       replyMessage,
								       sizeof(ctx->resultInfo.replyMessage));
							ctx->resultInfo.replyMessage[sizeof(ctx->resultInfo.replyMessage) - 1] = '\0';
						} else if (*outAction == NET_WISPR_ACTION_AUTH_FAILED) {
							sceClibStrncpy(ctx->resultInfo.replyMessage,
								       "Authentication Failure.",
								       sizeof(ctx->resultInfo.replyMessage));
							ctx->resultInfo.replyMessage[sizeof(ctx->resultInfo.replyMessage) - 1] = '\0';
						} else {
							sceClibMemset(ctx->resultInfo.replyMessage, 0,
								      sizeof(ctx->resultInfo.replyMessage));
						}

						if (nextLoginUrl != NULL) {
							if (nextLoginUrlLen >= sizeof(ctx->resultInfo.loginUrl))
								nextLoginUrlLen = sizeof(ctx->resultInfo.loginUrl) - 1;
							sceClibStrncpy(ctx->resultInfo.loginUrl,
								       nextLoginUrl,
								       nextLoginUrlLen);
							ctx->resultInfo.loginUrl[nextLoginUrlLen] = '\0';
						} else {
							sceClibMemset(ctx->resultInfo.loginUrl, 0,
								      sizeof(ctx->resultInfo.loginUrl));
						}

						if (logoffUrl != NULL) {
							sceClibStrncpy(ctx->resultInfo.logoffUrl,
								       logoffUrl,
								       sizeof(ctx->resultInfo.logoffUrl));
							ctx->resultInfo.logoffUrl[sizeof(ctx->resultInfo.logoffUrl) - 1] = '\0';
						} else {
							sceClibMemset(ctx->resultInfo.logoffUrl, 0,
								      sizeof(ctx->resultInfo.logoffUrl));
						}
					}
				}
			}
		}
	}

	netWisprCloseRequestAndConn();
	return ret;
}

/* 0x8121B98A */
static int netWisprRunLogoff(NetWisprContext *ctx, int *outAction)
{
	const char *logoffUrl = g_wisprLogoffUrls[ctx->ispId];
	const NetWisprIspOps *ops = &s_wisprIspTable[ctx->ispId];
	int statusCode = 0;

	(void)outAction;
	if (sceClibStrnlen(logoffUrl, sizeof(g_wisprLogoffUrls)) == 0)
		return NET_WISPR_ERROR_NO_LOGOFF_URL;

	int ret = ops->configureTemplate(ctx, g_wisprHttpTemplate);
	if (ret >= 0) {
		int connId = sceHttpCreateConnectionWithURL(g_wisprHttpTemplate, logoffUrl,
							    SCE_TRUE);
		ret = connId;
		if (connId >= 0) {
			g_wisprHttpConn = connId;
			int reqId = sceHttpCreateRequestWithURL(connId, SCE_HTTP_METHOD_GET,
								logoffUrl, 0);
			ret = reqId;
			if (reqId >= 0) {
				g_wisprHttpReq = reqId;
				ret = sceHttpSetIcmOption(reqId, 1);
				if (ret >= 0) {
					ret = sceHttpSendRequest(reqId, NULL, 0);
					if (ret < 0)
						ret = netWisprResolveHttpError(reqId, ret);
				}
				if (ret >= 0)
					ret = sceHttpGetStatusCode(reqId, &statusCode);
				if (ret >= 0) {
					ret = netWisprReadAndParseXml(ctx, 3, reqId,
								      g_wisprReadBuf,
								      sizeof(g_wisprReadBuf));
				}
			}
		}
	}

	netWisprCloseRequestAndConn();
	return ret;
}

/* 0x8121BB18 */
static int netWisprAbortHttpRequest(void)
{
	if (g_wisprHttpReq > 0)
		sceHttpAbortRequest(g_wisprHttpReq);
	return 0;
}

/* 0x8121BB30 */
static int netWisprXmlSessionBegin(int phase, NetWisprContext *ctx)
{
	sceClibMemset(&g_wisprXmlSession, 0, sizeof(g_wisprXmlSession));
	int ret = netWisprXmlParserInit(&g_wisprXmlSession.parser, &s_wisprXmlSchema,
					netWisprXmlHandler, NULL);
	if (ret >= 0) {
		g_wisprXmlSession.wisprCtx = ctx;
		g_wisprXmlSession.phase = phase;
	}
	return ret;
}

/* 0x8121BB78 */
static int netWisprXmlSessionFeed(const void *buf, int len)
{
	return netWisprXmlParserFeed(&g_wisprXmlSession.parser, (const SceUInt8 *)buf, len);
}

/* 0x8121BB8E */
static int netWisprXmlGetRedirectResult(const char **outLoginUrl, const char **outAuthSeed)
{
	if ((g_wisprXmlSession.seenFlags & NET_WISPR_PARSE_FLAG_VALID) == 0)
		return NET_WISPR_ERROR_NO_XML_ROOT;
	*outLoginUrl = g_wisprXmlSession.loginUrl;
	*outAuthSeed = g_wisprXmlSession.authSeed;
	return 0;
}

/* 0x8121BBC0 */
static int netWisprXmlGetAuthResult(int *outAction, int *outResponseCode,
				    const char **outReplyMessage, const char **outLogoffUrl)
{
	if ((g_wisprXmlSession.seenFlags & NET_WISPR_PARSE_FLAG_VALID) == 0)
		return NET_WISPR_ERROR_NO_XML_ROOT;

	int code = g_wisprXmlSession.responseCode;
	*outResponseCode = code;
	*outReplyMessage = g_wisprXmlSession.replyMessage;
	if (code == 50)
		*outLogoffUrl = g_wisprXmlSession.logoffUrl;
	else
		*outAction = NET_WISPR_ACTION_AUTH_FAILED;
	return 0;
}

/* 0x8121BC02 */
static int netWisprMapMessageTypeError(int messageType)
{
	switch (messageType) {
	case 100:
		return NET_WISPR_ERROR_MSG_TYPE_REDIRECT;
	case 110:
		return NET_WISPR_ERROR_MSG_TYPE_PROXY;
	case 120:
		return NET_WISPR_ERROR_MSG_TYPE_AUTH_REPLY;
	case 130:
		return NET_WISPR_ERROR_MSG_TYPE_LOGOFF_REPLY;
	case 140:
		return NET_WISPR_ERROR_MSG_TYPE_AUTH_POLL;
	case 150:
		return NET_WISPR_ERROR_MSG_TYPE_ABORT_REPLY;
	default:
		return NET_WISPR_ERROR_MSG_TYPE_UNKNOWN;
	}
}

/* 0x8121BC60 */
static int netWisprXmlHandler(NetWisprXmlParser *parser, int eventType,
			      int depth, int tagIndex, const char *value,
			      const char *attrValue, void *userArg)
{
	NetWisprXmlSession *session = &g_wisprXmlSession;

	(void)parser;
	(void)attrValue;
	(void)userArg;
	if (eventType == NET_WISPR_XML_EV_START && depth == 0 && tagIndex == 1)
		session->seenFlags |= NET_WISPR_PARSE_FLAG_ROOT;
	if (eventType == NET_WISPR_XML_EV_START && depth == 1 && tagIndex == 1)
		session->seenFlags |= NET_WISPR_PARSE_FLAG_REDIRECT;
	if (eventType == NET_WISPR_XML_EV_START && depth == 1 && tagIndex == 3)
		session->seenFlags |= NET_WISPR_PARSE_FLAG_AUTH_REPLY;

	if (eventType == NET_WISPR_XML_EV_TEXT && depth == 2) {
		switch (tagIndex) {
		case 6: /* MessageType */
			session->messageType = (int)sceClibStrtoll(value, NULL, 10);
			session->seenFlags |= NET_WISPR_PARSE_FLAG_MSG_TYPE;
			break;
		case 7: /* ResponseCode */
			session->responseCode = (int)sceClibStrtoll(value, NULL, 10);
			session->seenFlags |= NET_WISPR_PARSE_FLAG_RESP_CODE;
			break;
		case 4: /* LoginURL */
			sceClibStrncpy(session->loginUrl, value, sizeof(session->loginUrl));
			session->loginUrl[sizeof(session->loginUrl) - 1] = '\0';
			session->seenFlags |= NET_WISPR_PARSE_FLAG_LOGIN_URL;
			break;
		case 10: /* ReplyMessage */
			sceClibStrncpy(session->replyMessage, value, sizeof(session->replyMessage));
			session->replyMessage[sizeof(session->replyMessage) - 1] = '\0';
			break;
		case 12: /* LogoffURL */
			sceClibStrncpy(session->logoffUrl, value, sizeof(session->logoffUrl));
			session->logoffUrl[sizeof(session->logoffUrl) - 1] = '\0';
			break;
		case 13: /* AuthenticationSeed */
			sceClibStrncpy(session->authSeed, value, sizeof(session->authSeed));
			session->authSeed[sizeof(session->authSeed) - 1] = '\0';
			session->seenFlags |= NET_WISPR_PARSE_FLAG_AUTH_SEED;
			break;
		default:
			break;
		}
	}

	if (eventType != NET_WISPR_XML_EV_END || depth != 0 || tagIndex != 1)
		return 0;

	if ((session->seenFlags & NET_WISPR_PARSE_FLAG_ROOT) == 0)
		return NET_WISPR_ERROR_NO_XML_ROOT;
	if ((session->seenFlags & NET_WISPR_PARSE_FLAG_MSG_TYPE) == 0)
		return NET_WISPR_ERROR_MISSING_MSG_TYPE;
	if ((session->seenFlags & NET_WISPR_PARSE_FLAG_RESP_CODE) == 0)
		return NET_WISPR_ERROR_MISSING_RESP_CODE;

	if (session->phase == 2) {
		if ((session->seenFlags & NET_WISPR_PARSE_FLAG_AUTH_REPLY) == 0)
			return NET_WISPR_ERROR_MISSING_AUTH_REPLY;
		if (session->messageType != 120)
			return netWisprMapMessageTypeError(session->messageType);
		session->seenFlags |= NET_WISPR_PARSE_FLAG_VALID;
		return 0;
	}

	if (session->phase != 1) {
		session->seenFlags |= NET_WISPR_PARSE_FLAG_VALID;
		return 0;
	}

	if ((session->seenFlags & NET_WISPR_PARSE_FLAG_REDIRECT) == 0)
		return NET_WISPR_ERROR_MISSING_REDIRECT;
	if (session->messageType != 100)
		return netWisprMapMessageTypeError(session->messageType);

	if (session->responseCode == 0) {
		if ((session->seenFlags & NET_WISPR_PARSE_FLAG_LOGIN_URL) == 0)
			return NET_WISPR_ERROR_MISSING_LOGIN_URL;
		if ((session->seenFlags & NET_WISPR_PARSE_FLAG_AUTH_SEED) == 0)
			return NET_WISPR_ERROR_MISSING_AUTH_SEED;
		session->seenFlags |= NET_WISPR_PARSE_FLAG_VALID;
		return 0;
	}

	switch (session->responseCode) {
	case 50:
		return NET_WISPR_ERROR_RESP_LOGIN_SUCCEEDED;
	case 100:
		return NET_WISPR_ERROR_RESP_LOGIN_FAILED;
	case 102:
		return NET_WISPR_ERROR_RESP_RADIUS_ERROR;
	case 105:
		return NET_WISPR_ERROR_RESP_NETWORK_ADMIN;
	case 150:
		return NET_WISPR_ERROR_RESP_LOGOFF_SUCCEEDED;
	case 151:
		return NET_WISPR_ERROR_RESP_LOGIN_ABORTED;
	case 200:
		return NET_WISPR_ERROR_RESP_PROXY_RETRY;
	case 201:
		return NET_WISPR_ERROR_RESP_AUTH_PENDING;
	case 255:
		return NET_WISPR_ERROR_RESP_GW_INTERNAL;
	default:
		return NET_WISPR_ERROR_RESP_OTHER;
	}
}

/* 0x8121BF8C */
static int netWisprXmlParserInit(NetWisprXmlParser *parser,
				 const NetWisprXmlSchema *schema,
				 NetWisprXmlCallback callback, void *userArg)
{
	sceClibMemset(parser, 0, sizeof(*parser));
	parser->schema = schema;
	parser->callback = callback;
	parser->userArg = userArg;
	return 0;
}

static bool netWisprTagMatches(const NetWisprXmlTagEntry *entry, const char *name)
{
	if (entry->matchSuffix == 0)
		return sceClibStrncasecmp(entry->name, name, NET_WISPR_XML_TAG_BUF_SIZE) == 0;
	if (entry->matchSuffix == 1) {
		SceSize suffixLen = sceClibStrnlen(entry->name, NET_WISPR_XML_TAG_BUF_SIZE);
		SceSize nameLen = sceClibStrnlen(name, NET_WISPR_XML_TAG_BUF_SIZE);
		if (nameLen >= suffixLen &&
		    sceClibStrncasecmp(entry->name, name + (nameLen - suffixLen),
				       NET_WISPR_XML_TAG_BUF_SIZE) == 0) {
			return true;
		}
	}
	return false;
}

static bool netWisprIsXmlSpace(SceUInt8 ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

/* 0x8121BFB2 */
static int netWisprXmlParserFeed(NetWisprXmlParser *parser, const SceUInt8 *input, int len)
{
	SceUInt32 state = parser->state;
	const SceUInt8 *cursor = input;
	const SceUInt8 *end = input + len;

	while (cursor < end) {
		if (state == 0) {
			/* Scanning for '<' */
			if (*cursor == '<') {
				parser->writePos = 0;
				parser->isSelfClosing = 0;
				parser->attrFlags = 0;
				sceClibMemset(parser->tagName, 0, sizeof(parser->tagName));
				sceClibMemset(parser->textBuf, 0, sizeof(parser->textBuf));
				state = 1;
			}
			cursor++;
		} else if (state == 1 || state == 4) {
			/* Reading opening tag name (state 1) or closing tag name (state 4) */
			if (state == 1 && *cursor == '/' && parser->writePos == 0) {
				cursor++;
				state = 4;
				continue;
			}

			SceUInt8 ch = *cursor;
			if (parser->writePos < NET_WISPR_XML_TAG_BUF_SIZE - 1 &&
			    !netWisprIsXmlSpace(ch) && ch != '/' && ch != '<' && ch != '>') {
				parser->tagName[parser->writePos] = (char)ch;
				parser->writePos++;
				cursor++;
				continue;
			}

			if (ch == '/')
				parser->isSelfClosing = 1;
			parser->tagName[parser->writePos] = '\0';

			if (state == 4) {
				bool matchedClose = false;
				int cbRet = 0;
				if (parser->depth > 0 && parser->depth <= parser->schema->levelCount) {
					SceUInt32 levelIdx = parser->depth - 1;
					int tagIdx = parser->tagStack[levelIdx];
					const NetWisprXmlTagEntry *entry =
						&parser->schema->levels[levelIdx][tagIdx];
					if (parser->isSelfClosing == 1 ||
					    netWisprTagMatches(entry, parser->tagName)) {
						matchedClose = true;
						if (entry->hasTextValue == 1) {
							cbRet = parser->callback(
								parser, NET_WISPR_XML_EV_TEXT,
								(int)levelIdx, tagIdx,
								parser->textBuf, NULL,
								parser->userArg);
						} else {
							cbRet = parser->callback(
								parser, NET_WISPR_XML_EV_END,
								(int)levelIdx, tagIdx,
								NULL, NULL, parser->userArg);
						}
					}
				}
				if (cbRet < 0)
					return cbRet;
				if (matchedClose)
					parser->depth--;
				state = 0;
				cursor++;
			} else {
				/* state == 1: look up opening tag in current schema level */
				bool matchedOpen = false;
				int cbRet = 0;
				if (parser->depth < parser->schema->levelCount) {
					const NetWisprXmlTagEntry *level =
						parser->schema->levels[parser->depth];
					int idx = 0;
					while (sceClibStrnlen(level[idx].name,
							      NET_WISPR_XML_TAG_BUF_SIZE) != 0) {
						if (netWisprTagMatches(&level[idx],
								       parser->tagName)) {
							break;
						}
						idx++;
					}
					if (sceClibStrnlen(level[idx].name,
							   NET_WISPR_XML_TAG_BUF_SIZE) != 0) {
						parser->tagStack[parser->depth] = idx;
						cbRet = parser->callback(
							parser, NET_WISPR_XML_EV_START,
							(int)parser->depth, idx,
							parser->tagName, NULL,
							parser->userArg);
						matchedOpen = true;
					}
				}
				if (cbRet < 0)
					return cbRet;
				state = matchedOpen ? 2u : 0u;
			}
		} else if (state == 2) {
			/* Inside opening tag, scanning attributes or '>' */
			SceUInt8 ch = *cursor;
			if (ch == '>') {
				SceUInt32 curDepth = parser->depth;
				parser->depth = curDepth + 1;
				if (cursor > input && cursor[-1] == '/')
					parser->isSelfClosing = 1;
				if (parser->isSelfClosing == 1) {
					parser->attrFlags = 0;
					state = 4;
				} else {
					int tagIdx = parser->tagStack[curDepth];
					const NetWisprXmlTagEntry *entry =
						&parser->schema->levels[curDepth][tagIdx];
					if (entry->hasTextValue == 1) {
						parser->writePos = 0;
						sceClibMemset(parser->textBuf, 0,
							      sizeof(parser->textBuf));
						parser->partialLen = 0;
						parser->partialNeeded = 0;
						sceClibMemset(parser->partialBuf, 0,
							      sizeof(parser->partialBuf));
						state = 3;
					} else {
						state = 0;
					}
					cursor++;
				}
			} else if (ch == '/' || netWisprIsXmlSpace(ch)) {
				cursor++;
			} else {
				parser->attrFlags = 0;
				parser->writePos = 0;
				sceClibMemset(parser->tagName, 0, sizeof(parser->tagName));
				sceClibMemset(parser->textBuf, 0, sizeof(parser->textBuf));
				state = 5;
			}
		} else if (state == 3) {
			/* Reading element text content with UTF-8/entity decoding */
			SceSize avail = (SceSize)(end - cursor);
			SceUInt8 decoded[8];
			SceSize decodedLen = 0;
			SceSize consumedLen = 0;
			SceSize stepBytes = 0;

			if (parser->partialNeeded != 0) {
				stepBytes = (parser->partialNeeded <= avail)
						    ? parser->partialNeeded
						    : avail;
				sceClibMemcpy_safe(parser->partialBuf + parser->partialLen,
						   cursor, stepBytes);
				parser->partialLen += stepBytes;
				parser->partialNeeded -= stepBytes;
				if (parser->partialNeeded != 0) {
					cursor += stepBytes;
					continue;
				}
				int decRet = netWisprDecodeChar(decoded, 6, parser->partialBuf,
								parser->partialLen,
								&decodedLen, &consumedLen);
				if (decRet == NET_WISPR_ERROR_XML_NEED_MORE) {
					parser->partialNeeded = consumedLen - parser->partialLen;
					cursor += stepBytes;
					continue;
				}
				if (decRet >= 0) {
					parser->partialLen = 0;
					parser->partialNeeded = 0;
				} else {
					decodedLen = 1;
					consumedLen = 1;
					decoded[0] = parser->partialBuf[0];
				}
			} else {
				SceSize chunk = (avail < 6) ? avail : 6;
				int decRet = netWisprDecodeChar(decoded, 6, cursor, chunk,
								&decodedLen, &consumedLen);
				if (decRet == NET_WISPR_ERROR_XML_NEED_MORE) {
					sceClibMemcpy_safe(parser->partialBuf, cursor, chunk);
					parser->partialLen = chunk;
					parser->partialNeeded = consumedLen - chunk;
					cursor += chunk;
					continue;
				}
				if (decRet >= 0) {
					stepBytes = consumedLen;
				} else {
					stepBytes = 1;
					decodedLen = 1;
					consumedLen = 1;
					decoded[0] = *cursor;
				}
			}

			if (parser->writePos + decodedLen < NET_WISPR_XML_VAL_BUF_SIZE &&
			    !(decodedLen == 1 && consumedLen == 1 && decoded[0] == '<')) {
				sceClibMemcpy_safe(parser->textBuf + parser->writePos,
						   decoded, decodedLen);
				parser->writePos += decodedLen;
				cursor += stepBytes;
			} else {
				parser->textBuf[parser->writePos] = '\0';
				parser->writePos = 0;
				sceClibMemset(parser->tagName, 0, sizeof(parser->tagName));
				parser->isSelfClosing = 0;
				parser->attrFlags = 0;
				cursor++;
				state = 1;
			}
		} else if (state == 5) {
			/* Reading attribute name */
			if (parser->writePos >= NET_WISPR_XML_TAG_BUF_SIZE - 1) {
				int cbRet = parser->callback(
					parser, NET_WISPR_XML_EV_ATTR, (int)parser->depth,
					parser->tagStack[parser->depth], parser->tagName,
					parser->textBuf, parser->userArg);
				if (cbRet < 0)
					return cbRet;
				state = 2;
				continue;
			}

			SceUInt8 ch = *cursor;
			if (parser->attrFlags != 1) {
				if (!netWisprIsXmlSpace(ch) && ch != '/' && ch != '=' && ch != '>') {
					parser->tagName[parser->writePos] = (char)ch;
					parser->writePos++;
					cursor++;
					continue;
				}
				parser->tagName[parser->writePos] = '\0';
				parser->attrFlags = 1;
			}

			if (netWisprIsXmlSpace(ch)) {
				cursor++;
			} else if (ch == '/') {
				parser->isSelfClosing = 1;
				cursor++;
			} else if (ch == '=') {
				parser->attrFlags = 0;
				parser->writePos = 0;
				cursor++;
				state = 6;
			} else {
				int cbRet = parser->callback(
					parser, NET_WISPR_XML_EV_ATTR, (int)parser->depth,
					parser->tagStack[parser->depth], parser->tagName,
					parser->textBuf, parser->userArg);
				if (cbRet < 0)
					return cbRet;
				state = 2;
			}
		} else if (state == 6) {
			/* Reading attribute value */
			bool finishAttr = false;
			if (parser->writePos >= NET_WISPR_XML_VAL_BUF_SIZE - 1) {
				finishAttr = true;
			} else {
				SceUInt8 ch = *cursor;
				if ((parser->attrFlags & 2u) != 0 && ch != '\'' && ch != '"') {
					parser->textBuf[parser->writePos] = (char)ch;
					parser->writePos++;
					cursor++;
					continue;
				}
				if (ch == '\'' || ch == '"') {
					if ((parser->attrFlags & 2u) != 0)
						parser->attrFlags &= ~2u;
					else
						parser->attrFlags |= 3u;
					cursor++;
					continue;
				}
				if (netWisprIsXmlSpace(ch)) {
					if ((parser->attrFlags & 1u) == 0) {
						cursor++;
						continue;
					}
					finishAttr = true;
				} else if (ch == '/' || ch == '>') {
					if (ch == '/')
						parser->isSelfClosing = 1;
					finishAttr = true;
				} else {
					parser->attrFlags |= 1u;
					parser->textBuf[parser->writePos] = (char)ch;
					parser->writePos++;
					cursor++;
					continue;
				}
			}

			if (finishAttr) {
				parser->textBuf[parser->writePos] = '\0';
				int cbRet = parser->callback(
					parser, NET_WISPR_XML_EV_ATTR, (int)parser->depth,
					parser->tagStack[parser->depth], parser->tagName,
					parser->textBuf, parser->userArg);
				if (cbRet < 0)
					return cbRet;
				state = 2;
			}
		} else {
			break;
		}
	}

	parser->state = state;
	return 0;
}

/* 0x8121C868 */
static int netWisprUnescapeXml(SceUInt8 *outBuf, SceSize outCap,
			       const SceUInt8 *inBuf, SceSize inLen,
			       SceSize *outLen)
{
	if (inBuf == NULL || inLen == 0)
		return NET_WISPR_ERROR_XML_INVALID_ARG;

	bool writeOut = true;
	if (outBuf == NULL) {
		if (outLen == NULL)
			return NET_WISPR_ERROR_XML_INVALID_ARG;
		writeOut = false;
	}

	const SceUInt8 *cursor = inBuf;
	const SceUInt8 *end = inBuf + inLen;
	SceSize count = 0;

	while (cursor < end) {
		count++;
		if (writeOut && count > outCap)
			return NET_WISPR_ERROR_XML_NEED_MORE;

		SceUInt8 ch = *cursor;
		if (ch == '&') {
			if (cursor + 3 < end && cursor[1] == 'l' && cursor[2] == 't' &&
			    cursor[3] == ';') {
				if (writeOut)
					*outBuf++ = '<';
				cursor += 4;
			} else if (cursor + 3 < end && cursor[1] == 'g' && cursor[2] == 't' &&
				   cursor[3] == ';') {
				if (writeOut)
					*outBuf++ = '>';
				cursor += 4;
			} else if (cursor + 4 < end && cursor[1] == 'a' && cursor[2] == 'm' &&
				   cursor[3] == 'p' && cursor[4] == ';') {
				if (writeOut)
					*outBuf++ = '&';
				cursor += 5;
			} else if (cursor + 5 < end && cursor[1] == 'a' && cursor[2] == 'p' &&
				   cursor[3] == 'o' && cursor[4] == 's' && cursor[5] == ';') {
				if (writeOut)
					*outBuf++ = '\'';
				cursor += 6;
			} else if (cursor + 5 < end && cursor[1] == 'q' && cursor[2] == 'u' &&
				   cursor[3] == 'o' && cursor[4] == 't' && cursor[5] == ';') {
				if (writeOut)
					*outBuf++ = '"';
				cursor += 6;
			} else {
				if (writeOut)
					*outBuf++ = '&';
				cursor++;
			}
		} else {
			if (writeOut)
				*outBuf++ = ch;
			cursor++;
		}
	}

	SceSize totalLen = count + 1;
	if (totalLen <= outCap) {
		if (writeOut)
			*outBuf = '\0';
	} else if (writeOut) {
		return NET_WISPR_ERROR_XML_NEED_MORE;
	}

	if (outLen != NULL)
		*outLen = totalLen;
	return 0;
}

/* 0x8121C9E8 */
static int netWisprDecodeChar(SceUInt8 *outBuf, SceSize outCap,
			      const SceUInt8 *inBuf, SceSize inLen,
			      SceSize *outDecodedLen, SceSize *outConsumedLen)
{
	if (inBuf == NULL || inLen == 0)
		return NET_WISPR_ERROR_XML_INVALID_ARG;

	bool writeOut = true;
	if (outBuf == NULL) {
		if (outDecodedLen == NULL)
			return NET_WISPR_ERROR_XML_INVALID_ARG;
		writeOut = false;
	}

	SceSize decodedLen = 0;
	SceSize consumedLen = 0;
	int status = 0;
	SceUInt8 lead = inBuf[0];

	if ((lead & 0x80u) != 0) {
		if ((lead & 0xE0u) == 0xC0u && lead >= 0xC2u) {
			if (inLen < 2) {
				consumedLen = 2;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if ((inBuf[1] & 0xC0u) == 0x80u) {
				decodedLen = 2;
				consumedLen = 2;
			}
		} else if ((lead & 0xF0u) == 0xE0u) {
			if (inLen < 3) {
				consumedLen = 3;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if ((inBuf[1] & 0xC0u) == 0x80u && (inBuf[2] & 0xC0u) == 0x80u) {
				decodedLen = 3;
				consumedLen = 3;
			}
		} else if ((lead & 0xF8u) == 0xF0u) {
			if (inLen < 4) {
				consumedLen = 4;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if ((inBuf[1] & 0xC0u) == 0x80u &&
				   (inBuf[2] & 0xC0u) == 0x80u &&
				   (inBuf[3] & 0xC0u) == 0x80u) {
				decodedLen = 4;
				consumedLen = 4;
			}
		} else {
			decodedLen = 1;
			consumedLen = 1;
		}
	} else {
		decodedLen = 1;
		if (lead == '&') {
			if (inLen < 4) {
				consumedLen = 4;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if ((inBuf[1] == 'l' || inBuf[1] == 'g') &&
				   inBuf[2] == 't' && inBuf[3] == ';') {
				consumedLen = 4;
			} else if (inLen < 5) {
				consumedLen = 5;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if (inBuf[1] == 'a' && inBuf[2] == 'm' &&
				   inBuf[3] == 'p' && inBuf[4] == ';') {
				consumedLen = 5;
			} else if (inLen < 6) {
				consumedLen = 6;
				status = NET_WISPR_ERROR_XML_NEED_MORE;
			} else if ((inBuf[1] == 'a' && inBuf[2] == 'p' && inBuf[3] == 'o' &&
				    inBuf[4] == 's' && inBuf[5] == ';') ||
				   (inBuf[1] == 'q' && inBuf[2] == 'u' && inBuf[3] == 'o' &&
				    inBuf[4] == 't' && inBuf[5] == ';')) {
				consumedLen = 6;
			} else {
				consumedLen = 1;
			}
		} else {
			consumedLen = 1;
		}
	}

	if (status >= 0) {
		if (writeOut && decodedLen > outCap) {
			status = NET_WISPR_ERROR_XML_NEED_MORE;
		} else {
			SceUInt8 unescaped[8];
			status = netWisprUnescapeXml(unescaped, 7, inBuf, inLen, NULL);
			if (status < 0)
				return status;
			if (writeOut)
				sceClibMemcpy_safe(outBuf, unescaped, decodedLen);
		}
	}

	if (outDecodedLen != NULL)
		*outDecodedLen = decodedLen;
	if (outConsumedLen != NULL)
		*outConsumedLen = consumedLen;
	return status;
}

/* 0x8121CC24 */
static int netWisprAttConfigure(NetWisprContext *ctx, int tmplId)
{
	(void)ctx;
	return sceHttpSetAutoRedirect(tmplId, SCE_FALSE);
}

/* 0x8121CC30 */
static int netWisprAttGetProbeUrl(NetWisprContext *ctx, const char **outUrl)
{
	(void)ctx;
	*outUrl = "http://www.scei.co.jp/";
	return 0;
}

/* 0x8121CC3E */
static int netWisprAttCheckProbeStatus(NetWisprContext *ctx, int statusCode, int *outAction)
{
	(void)ctx;
	if (statusCode != 302)
		*outAction = NET_WISPR_ACTION_DONE;
	return 0;
}

/* 0x8121CC4C */
static int netWisprAttHandleProbeReply(NetWisprContext *ctx, int reqId, int *outAction,
				       const char **outLoginUrl, const char **outRedirectUrl,
				       SceSize *outRedirectLen)
{
	const char *authSeed = NULL;

	(void)ctx;
	(void)reqId;
	(void)outRedirectLen;
	*outLoginUrl = NULL;
	*outRedirectUrl = NULL;

	int ret = netWisprXmlGetRedirectResult(outLoginUrl, &authSeed);
	if (ret == NET_WISPR_ERROR_NO_XML_ROOT) {
		*outAction = NET_WISPR_ACTION_DONE;
		return 0;
	}
	if (ret < 0)
		return ret;
	if (sceClibStrncasecmp("https://", *outLoginUrl, 8) != 0)
		return NET_WISPR_ERROR_NON_HTTPS_LOGIN_URL;

	sceClibStrlcpy(g_wisprAttAuthSeed, authSeed, sizeof(g_wisprAttAuthSeed));
	return 0;
}

/* 0x8121CCC8 */
static void netWisprDecryptAttSecret(const NetWisprEncryptedBlock *headBlock,
				     const NetWisprEncryptedBlock *tailBlock,
				     char *outSecret, int *outHeadValid, int *outTailValid)
{
	NetWisprDecryptedBlock decrypted;

	sceClibMemset(&decrypted, 0, sizeof(decrypted));
	vshSblSsDecryptWithPortability(5, s_attPortabilityIv, headBlock, &decrypted);
	*outHeadValid = (sceClibMemcmp(decrypted.magic, s_attDecryptedMagic,
				       sizeof(s_attDecryptedMagic)) == 0) ? 1 : 0;
	sceClibMemcpy_safe(outSecret, decrypted.secret, 8);

	sceClibMemset(&decrypted, 0, sizeof(decrypted));
	vshSblSsDecryptWithPortability(5, s_attPortabilityIv, tailBlock, &decrypted);
	*outTailValid = (sceClibMemcmp(decrypted.magic, s_attDecryptedMagic,
				       sizeof(s_attDecryptedMagic)) == 0) ? 1 : 0;
	sceClibMemcpy_safe(outSecret + 8, decrypted.secret, 24);
}

/* 0x8121CD8C */
static int netWisprAttBuildAuthBody(NetWisprContext *ctx, char *outBody, SceSize bodySize)
{
	char defaultSecret[36];
	char alternateSecret[36];
	int defaultHeadOk = 0;
	int defaultTailOk = 0;
	int altHeadOk = 0;
	int altTailOk = 0;

	sceClibMemset(defaultSecret, 0, 33);
	sceClibMemset(alternateSecret, 0, 33);
	netWisprDecryptAttSecret(&s_attKeyHead, &s_attKeyTail,
				 defaultSecret, &defaultHeadOk, &defaultTailOk);
	netWisprDecryptAttSecret(&s_attAltKeyHead, &s_attAltKeyTail,
				 alternateSecret, &altHeadOk, &altTailOk);

	const char *selectedSecret = NULL;
	if (defaultHeadOk == 1 && defaultTailOk == 1)
		selectedSecret = defaultSecret;
	else if (altHeadOk == 1 && altTailOk == 1)
		selectedSecret = alternateSecret;
	else
		return NET_WISPR_ERROR_CREDENTIAL_INVALID;

	char *querySep = sceClibStrchr(ctx->resultInfo.loginUrl, '?');
	if (querySep != NULL)
		*querySep = '\0';

	int written = sceClibSnprintf(outBody, bodySize, "%s&", querySep + 1);
	if (written < 0 || (SceSize)written >= bodySize)
		return NET_WISPR_ERROR_BUFFER_TOO_SMALL;

	char *cursor = outBody + written;
	SceSize remaining = bodySize - (SceSize)written;

	int userLen = sceClibSnprintf(cursor, remaining, "username=%s%%40%s&",
				      ctx->param.ssid, "attedoclient2.com");
	if (userLen < 0 || (SceSize)userLen >= remaining)
		return NET_WISPR_ERROR_BUFFER_TOO_SMALL;
	cursor += userLen;
	remaining -= (SceSize)userLen;

	char bssidText[20];
	sceClibSnprintf(bssidText, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
			ctx->param.bssid[0], ctx->param.bssid[1], ctx->param.bssid[2],
			ctx->param.bssid[3], ctx->param.bssid[4], ctx->param.bssid[5]);

	char digestInput[256];
	int inputLen = sceClibSnprintf(digestInput, sizeof(digestInput), "%s%s%s",
				       bssidText, g_wisprAttAuthSeed, selectedSecret);
	if (inputLen < 0 || (SceSize)inputLen >= sizeof(digestInput))
		return NET_WISPR_ERROR_BUFFER_TOO_SMALL;

	SceUInt8 digest[16];
	shellMd5Digest(digestInput, sceClibStrnlen(digestInput, sizeof(digestInput)), digest);

	int passLen = sceClibSnprintf(
		cursor, remaining,
		"password=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		digest[0], digest[1], digest[2], digest[3],
		digest[4], digest[5], digest[6], digest[7],
		digest[8], digest[9], digest[10], digest[11],
		digest[12], digest[13], digest[14], digest[15]);
	if (passLen < 0 || (SceSize)passLen >= remaining)
		return NET_WISPR_ERROR_BUFFER_TOO_SMALL;

	return (int)((cursor + passLen) - outBody);
}

/* 0x8121CFB4 */
static int netWisprAttCheckAuthStatus(NetWisprContext *ctx, int statusCode, int *outAction)
{
	(void)ctx;
	(void)outAction;
	if (statusCode == 200)
		return 0;
	return NET_WISPR_ERROR_HTTP_STATUS;
}

/* 0x8121CFC4 */
static int netWisprAttHandleAuthReply(NetWisprContext *ctx, int reqId, int *outAction,
				      int *outResponseCode, const char **outReplyMessage,
				      const char **outLoginUrl, SceSize *outLoginUrlLen,
				      const char **outLogoffUrl)
{
	(void)ctx;
	(void)reqId;
	(void)outLoginUrlLen;
	*outReplyMessage = NULL;
	*outLoginUrl = NULL;
	*outLogoffUrl = NULL;
	return netWisprXmlGetAuthResult(outAction, outResponseCode,
					outReplyMessage, outLogoffUrl);
}
