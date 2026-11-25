/*
 * net_upnp.cpp: SceNetUpnp (background NAT type and STUN/UPnP state machine)
 * inside SceShell.
 * 3.65-CEX: 0x8121A038 - 0x8121ADB0
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include <libnetctl.h>
#include "netctl_services.h"

#define NET_UPNP_MAX_CALLBACKS		3
#define NET_UPNP_STUN_PORT		3478
#define NET_UPNP_LOCAL_BIND_PORT	9308
#define NET_UPNP_LARGE_PACKET_SIZE	2048

#define NET_UPNP_FLAG_ACTIVE		0x0001u
#define NET_UPNP_FLAG_STOP_REQ		0x0002u
#define NET_UPNP_FLAG_MUTEX_CREATED	0x0008u
#define NET_UPNP_FLAG_STUN_DONE		0x0010u
#define NET_UPNP_FLAG_PORT_CHECK	0x0800u
#define NET_UPNP_FLAG_HAS_NAT_RESULT	0x1000u

#define NET_UPNP_EV_EXIT		0x01u
#define NET_UPNP_EV_START		0x02u
#define NET_UPNP_EV_STOP		0x04u
#define NET_UPNP_EV_INET_CONNECTED	0x08u
#define NET_UPNP_EV_INET_DISCONNECTED	0x10u
#define NET_UPNP_EV_STUN_DONE		0x20u
#define NET_UPNP_EV_STUN_ABORTED	0x40u
#define NET_UPNP_EV_RETRY		0x80u

#define NET_UPNP_STATE_IDLE		0
#define NET_UPNP_STATE_WAIT_INET	1
#define NET_UPNP_STATE_STUN_CHECK	5
#define NET_UPNP_STATE_COMPLETE		6
#define NET_UPNP_STATE_ERROR_WAIT	7

typedef void (*NetUpnpStateCallback)(int prevState, int curState, int eventCode,
				     int errorCode, void *userArg);

typedef struct NetUpnpCallbackSlot {
	int id;
	NetUpnpStateCallback callback;
	void *userArg;
} NetUpnpCallbackSlot;

typedef struct NetUpnpErrorHistoryFormat {
	int natType;
	SceUInt32 flags;
} NetUpnpErrorHistoryFormat;

typedef struct NetUpnpContext {
	char stunHost[36];		/* +0x00 */
	SceSize stackSize;		/* +0x24 */
	int threadPriority;		/* +0x28 */
	int netCtlCallbackId;		/* +0x2C */
	int state;			/* +0x30 */
	int eventCode;			/* +0x34 */
	int lastError;			/* +0x38 */
	int reportedError;		/* +0x3C */
	SceUID threadId;		/* +0x40 */
	SceUID eventFlagId;		/* +0x44 */
	SceKernelLwMutexWork mutex;	/* +0x48 */
	SceUInt32 flags;		/* +0x68 */
	ShellNetUpnpParam param;	/* +0x6C */
	NetStunNatResult stunResult;	/* +0x70 */
	int stunStatus;			/* +0x88 */
	int largePacketOk;		/* +0x8C */
	SceUInt32 reserved;		/* +0x90 */
	SceNetInAddr mappedAddr;	/* +0x94 */
	void *ownerSession;		/* +0x98 */
	SceNetInAddr localAddr;		/* +0x9C */
} NetUpnpContext;

typedef int (*NetUpnpStateHandler)(NetUpnpContext *ctx, int prevState,
				   int *outNextState, int *outEvent);

/* PSN STUN HMAC-SHA1 key (0x81513BF4) */
static const SceUInt8 s_stunHmacKey[20] = {
	0xa0, 0x07, 0x0e, 0x61, 0xad, 0x3f, 0xca, 0x50, 0x6d, 0xb5,
	0xad, 0x3d, 0x57, 0x13, 0x32, 0xce, 0xa7, 0x32, 0x0d, 0x9e
};

static int g_upnpInitialized = 0;		/* 0x815450D8 */
static int g_upnpInetConnected = 0;		/* 0x815450DC */
static NetUpnpCallbackSlot g_upnpCallbacks[NET_UPNP_MAX_CALLBACKS]; /* 0x815A13B8 */
static NetUpnpContext g_upnpContext;		/* 0x815A13E0 */
static SceUInt8 g_upnpLargePacketBuf[NET_UPNP_LARGE_PACKET_SIZE]; /* 0x815A1480 */

/* SceShell internal NetCtl exports called by SceNetUpnp */
extern "C" {
int SceShell_2E0B6FA6(void (*callback)(int prevState, int curState, int event,
				       int error, int code, void *arg),
		      void *arg, int *outId);
int SceShell_B4D888D5(int callbackId);
int SceShell_77642C92(int *outState);
int SceShell_7AD86CE1(int infoCode, void *outInfo);
}

static int netUpnpWorkerThread(SceSize args, void *argp);
static int netUpnpStateIdle(NetUpnpContext *ctx, int prevState,
			    int *outNextState, int *outEvent);
static int netUpnpStateWaitInet(NetUpnpContext *ctx, int prevState,
				int *outNextState, int *outEvent);
static int netUpnpStateNoop(NetUpnpContext *ctx, int prevState,
			    int *outNextState, int *outEvent);
static int netUpnpStateStunCheck(NetUpnpContext *ctx, int prevState,
				 int *outNextState, int *outEvent);
static int netUpnpStateComplete(NetUpnpContext *ctx, int prevState,
				int *outNextState, int *outEvent);
static int netUpnpStateErrorWait(NetUpnpContext *ctx, int prevState,
				 int *outNextState, int *outEvent);

/* 0x81513C08 */
static const NetUpnpStateHandler s_upnpStateHandlers[8] = {
	netUpnpStateIdle,
	netUpnpStateWaitInet,
	netUpnpStateNoop,
	netUpnpStateNoop,
	netUpnpStateNoop,
	netUpnpStateStunCheck,
	netUpnpStateComplete,
	netUpnpStateErrorWait
};

static int netUpnpStateNoop(NetUpnpContext *ctx, int prevState,
			    int *outNextState, int *outEvent)
{
	(void)ctx;
	(void)prevState;
	(void)outNextState;
	(void)outEvent;
	return 0;
}

/* 0x8121A148 */
static int netUpnpNotifyCallbacks(NetUpnpContext *ctx, int prevState)
{
	if (sceKernelLockLwMutex(&ctx->mutex, 1, NULL) < 0)
		return 0;

	if (ctx->eventCode == 4 || ctx->eventCode == 8 || ctx->eventCode == 9)
		ctx->reportedError = ctx->lastError;

	for (int i = 0; i < NET_UPNP_MAX_CALLBACKS; i++) {
		NetUpnpStateCallback cb = g_upnpCallbacks[i].callback;
		if (cb == NULL)
			continue;
		int err = 0;
		if (ctx->eventCode == 4 || ctx->eventCode == 8 || ctx->eventCode == 9)
			err = ctx->lastError;
		cb(prevState, ctx->state, ctx->eventCode, err, g_upnpCallbacks[i].userArg);
	}

	sceKernelUnlockLwMutex(&ctx->mutex, 1);
	return 0;
}

/* 0x8121A1F2 */
static int netUpnpInitInternal(SceSize stackSize, int priority, const char *env)
{
	NetUpnpContext *ctx = &g_upnpContext;
	int ret;

	sceClibMemset(g_upnpCallbacks, 0, sizeof(g_upnpCallbacks));
	sceClibMemset(ctx, 0, sizeof(*ctx));
	sceClibSnprintf(ctx->stunHost, sizeof(ctx->stunHost), "%s.%s.%s",
			"us", env, "stun.playstation.net");
	ctx->stunHost[sizeof(ctx->stunHost) - 1] = '\0';
	ctx->stackSize = stackSize;
	ctx->threadPriority = priority;
	ctx->netCtlCallbackId = -1;
	ctx->threadId = -1;
	ctx->eventFlagId = -1;
	ctx->state = NET_UPNP_STATE_IDLE;

	ret = sceKernelCreateLwMutex(&ctx->mutex, "SceNetUpnp", 2, 0, NULL);
	if (ret >= 0) {
		ctx->flags |= NET_UPNP_FLAG_MUTEX_CREATED;
		ret = sceKernelCreateEventFlag("SceNetUpnp", 0, 0, NULL);
		if (ret >= 0) {
			ctx->eventFlagId = ret;
			ret = sceKernelCreateThread("SceNetUpnp", netUpnpWorkerThread,
						    priority, stackSize, 0, 0, NULL);
			if (ret >= 0) {
				ctx->threadId = ret;
				NetUpnpContext *threadArg = ctx;
				ret = sceKernelStartThread(ctx->threadId, sizeof(threadArg),
							   &threadArg);
				if (ret >= 0)
					return 0;
			}
		}
	}

	sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_EXIT);
	if (ctx->threadId >= 0) {
		sceKernelWaitThreadEnd(ctx->threadId, NULL, NULL);
		sceKernelDeleteThread(ctx->threadId);
		ctx->threadId = -1;
	}
	if (ctx->eventFlagId >= 0) {
		sceKernelDeleteEventFlag(ctx->eventFlagId);
		ctx->eventFlagId = -1;
	}
	if ((ctx->flags & NET_UPNP_FLAG_MUTEX_CREATED) != 0) {
		sceKernelDeleteLwMutex(&ctx->mutex);
		ctx->flags &= ~NET_UPNP_FLAG_MUTEX_CREATED;
	}
	return ret;
}

/* 0x8121A038 */
int shellNetUpnpInit(SceSize stackSize, int priority, const char *env)
{
	if (g_upnpInitialized == 1)
		return NET_UPNP_ERROR_ALREADY_INITIALIZED;

	int ret = netUpnpInitInternal(stackSize, priority, env);
	if (ret >= 0) {
		g_upnpInitialized = 1;
		return 0;
	}
	return ret;
}

/* 0x8121A392 */
static int netUpnpStartInternal(const ShellNetUpnpParam *param)
{
	if ((g_upnpContext.flags & NET_UPNP_FLAG_ACTIVE) != 0)
		return NET_UPNP_ERROR_ALREADY_STARTED;

	g_upnpContext.flags |= NET_UPNP_FLAG_ACTIVE;
	sceClibMemcpy_safe(&g_upnpContext.param, param, sizeof(g_upnpContext.param));
	sceKernelSetEventFlag(g_upnpContext.eventFlagId, NET_UPNP_EV_START);
	return 0;
}

/* 0x8121A06C */
int shellNetUpnpSetParam(const ShellNetUpnpParam *param)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	if (param == NULL)
		return NET_UPNP_ERROR_INVALID_PARAM;
	return netUpnpStartInternal(param);
}

/* 0x8121A3CE */
static int netUpnpSignalDisconnected(void)
{
	sceKernelSetEventFlag(g_upnpContext.eventFlagId, NET_UPNP_EV_INET_DISCONNECTED);
	return 0;
}

/* 0x8121A3E4 */
static int netUpnpGetNatInfoInternal(NetUpnpNatInfo *outInfo)
{
	NetUpnpContext *ctx = &g_upnpContext;
	int inetState = 0;

	sceClibMemset(outInfo, 0, sizeof(*outInfo));
	outInfo->mappedAddr = ctx->mappedAddr;

	if (ctx->state < NET_UPNP_STATE_STUN_CHECK || ctx->state > NET_UPNP_STATE_ERROR_WAIT) {
		outInfo->stunStatus = SCE_NET_CTL_NATINFO_STUN_UNCHECKED;
		outInfo->detailedStatus = 0;
		outInfo->upnpStatus = 0;
		return 0;
	}

	if ((ctx->flags & NET_UPNP_FLAG_HAS_NAT_RESULT) != 0) {
		if ((ctx->stunResult.upnpResult & 4) != 0) {
			outInfo->upnpStatus =
				(ctx->mappedAddr.s_addr == ctx->stunResult.upnpExternalAddr.s_addr)
					? 2 : 1;
		} else {
			outInfo->upnpStatus =
				(ctx->mappedAddr.s_addr == ctx->localAddr.s_addr) ? 2 : 1;
		}
	} else {
		outInfo->upnpStatus = 0;
	}

	outInfo->historyNatType = 0;
	if (SceShell_77642C92(&inetState) >= 0) {
		if (inetState == 0) {
			outInfo->historyNatType = 0;
		} else if (ctx->state == NET_UPNP_STATE_STUN_CHECK) {
			outInfo->historyNatType = 1;
		} else if (ctx->state == NET_UPNP_STATE_COMPLETE ||
			   ctx->state == NET_UPNP_STATE_ERROR_WAIT) {
			if ((ctx->flags & NET_UPNP_FLAG_STUN_DONE) != 0) {
				outInfo->stunResult = ctx->stunResult;
				if (outInfo->stunResult.mappedAddr.s_addr == ctx->localAddr.s_addr)
					outInfo->historyNatType = 3;
				else if (outInfo->upnpStatus != 2 &&
					 outInfo->stunResult.natMappingType != 0)
					outInfo->historyNatType = 5;
				else
					outInfo->historyNatType = 4;
			} else {
				outInfo->historyNatType = 2;
			}
		}
	}

	if (ctx->state == NET_UPNP_STATE_STUN_CHECK) {
		if ((ctx->flags & NET_UPNP_FLAG_HAS_NAT_RESULT) == 0) {
			outInfo->stunStatus = SCE_NET_CTL_NATINFO_STUN_UNCHECKED;
			outInfo->detailedStatus = 0;
			return 0;
		}
		outInfo->detailedStatus = 2;
	} else {
		if ((ctx->flags & NET_UPNP_FLAG_STUN_DONE) == 0) {
			outInfo->stunStatus = SCE_NET_CTL_NATINFO_STUN_FAILED;
			outInfo->detailedStatus = 1;
			return 0;
		}
		outInfo->detailedStatus = 3;
		outInfo->largePacketOk = (ctx->largePacketOk == 1) ? 1 : 0;
	}

	outInfo->stunStatus = SCE_NET_CTL_NATINFO_STUN_OK;
	outInfo->stunResult = ctx->stunResult;
	if (outInfo->stunResult.mappedAddr.s_addr == ctx->localAddr.s_addr)
		outInfo->natType = SCE_NET_CTL_NATINFO_NAT_TYPE_1;
	else if (outInfo->upnpStatus != 2 && outInfo->stunResult.natMappingType != 0)
		outInfo->natType = SCE_NET_CTL_NATINFO_NAT_TYPE_3;
	else
		outInfo->natType = SCE_NET_CTL_NATINFO_NAT_TYPE_2;

	return 0;
}

/* 0x8121A096 */
int netUpnpGetNatInfo(NetUpnpNatInfo *outInfo)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	return netUpnpGetNatInfoInternal(outInfo);
}

/* 0x8121A5A2 */
static int netUpnpGetLastErrorInternal(int *outError)
{
	*outError = g_upnpContext.reportedError;
	return 0;
}

/* 0x8121A0B6 */
int netUpnpGetLastError(int *outError)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	if (outError == NULL)
		return NET_UPNP_ERROR_INVALID_PARAM;
	return netUpnpGetLastErrorInternal(outError);
}

/* 0x8121A5B2 */
static int netUpnpEnablePortCheckInternal(void *ownerSession)
{
	g_upnpContext.ownerSession = ownerSession;
	g_upnpContext.flags |= NET_UPNP_FLAG_PORT_CHECK;
	return 0;
}

/* 0x8121A0E0 */
int netUpnpEnablePortCheck(void *ownerSession)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	if (ownerSession == NULL)
		return NET_UPNP_ERROR_INVALID_PARAM;
	return netUpnpEnablePortCheckInternal(ownerSession);
}

/* 0x8121A5CA */
static int netUpnpDisablePortCheckInternal(void)
{
	g_upnpContext.ownerSession = NULL;
	g_upnpContext.flags &= ~NET_UPNP_FLAG_PORT_CHECK;
	return 0;
}

/* 0x8121A10A */
int netUpnpDisablePortCheck(void)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	return netUpnpDisablePortCheckInternal();
}

/* 0x8121A5E2 */
static int netUpnpReleaseOwnerSessionInternal(void *session)
{
	if (session != NULL && session == g_upnpContext.ownerSession) {
		g_upnpContext.ownerSession = NULL;
		g_upnpContext.flags &= ~NET_UPNP_FLAG_PORT_CHECK;
	}
	return 0;
}

/* 0x8121A128 */
int netUpnpReleaseOwnerSession(void *session)
{
	if (g_upnpInitialized == 0)
		return NET_UPNP_ERROR_NOT_INITIALIZED;
	netUpnpReleaseOwnerSessionInternal(session);
	return 0;
}

static void netUpnpUpdateErrorHistory(void)
{
	NetUpnpNatInfo natInfo;
	NetUpnpErrorHistoryFormat fmt;

	sceClibMemset(&natInfo, 0, sizeof(natInfo));
	if (netUpnpGetNatInfoInternal(&natInfo) >= 0) {
		sceClibMemset(&fmt, 0, sizeof(fmt));
		fmt.natType = natInfo.historyNatType;
		fmt.flags |= 1u;
		sceErrorHistorySetDefaultFormat(&fmt);
	}
}

/* 0x8121A608 */
static int netUpnpWorkerThread(SceSize args, void *argp)
{
	NetUpnpContext *ctx = *(NetUpnpContext **)argp;
	int nextState = 0;
	int nextEvent = 0;

	(void)args;
	s_upnpStateHandlers[ctx->state](ctx, ctx->state, &nextState, &nextEvent);
	while (1) {
		int prevState = ctx->state;
		if (prevState == NET_UPNP_STATE_IDLE &&
		    (ctx->flags & NET_UPNP_FLAG_STOP_REQ) != 0) {
			break;
		}
		ctx->state = nextState;
		ctx->eventCode = nextEvent;
		netUpnpNotifyCallbacks(ctx, prevState);
		s_upnpStateHandlers[ctx->state](ctx, prevState, &nextState, &nextEvent);
	}

	if (ctx->netCtlCallbackId >= 0) {
		SceShell_B4D888D5(ctx->netCtlCallbackId);
		ctx->netCtlCallbackId = -1;
	}
	sceKernelExitThread(0);
	return 0;
}

/* 0x8121A68C */
static int netUpnpStateIdle(NetUpnpContext *ctx, int prevState,
			    int *outNextState, int *outEvent)
{
	SceUInt32 outBits = 0;

	(void)prevState;
	netUpnpUpdateErrorHistory();
	ctx->flags &= ~(NET_UPNP_FLAG_STUN_DONE | NET_UPNP_FLAG_HAS_NAT_RESULT);
	ctx->largePacketOk = 0;

	sceKernelWaitEventFlag(ctx->eventFlagId, 0xFFFFFFFFu,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       &outBits, NULL);
	if ((outBits & NET_UPNP_EV_EXIT) != 0) {
		ctx->flags |= NET_UPNP_FLAG_STOP_REQ;
	} else if ((outBits & NET_UPNP_EV_STOP) != 0) {
		ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
		*outNextState = NET_UPNP_STATE_IDLE;
		*outEvent = 6;
	} else if ((outBits & NET_UPNP_EV_START) != 0) {
		*outNextState = NET_UPNP_STATE_WAIT_INET;
		*outEvent = 0;
	}
	return 0;
}

/* 0x8121A74E */
static void netUpnpOnNetCtlEvent(int prevState, int curState, int eventCode,
				 int errorCode, int code, void *arg)
{
	NetUpnpContext *ctx = (NetUpnpContext *)arg;

	(void)errorCode;
	(void)code;
	if (ctx == NULL || (ctx->flags & NET_UPNP_FLAG_ACTIVE) == 0)
		return;

	if (curState == 4 && (eventCode == 6 || prevState == 5)) {
		g_upnpInetConnected = 1;
		sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_INET_CONNECTED);
	} else if (g_upnpInetConnected == 1 && curState == 0) {
		netUpnpSignalDisconnected();
		g_upnpInetConnected = 0;
	}
}

/* 0x8121A7A4 */
static int netUpnpStateWaitInet(NetUpnpContext *ctx, int prevState,
				int *outNextState, int *outEvent)
{
	SceUInt32 outBits = 0;
	int inetState = 0;
	SceNetCtlInfo ipInfo;
	int ret = 0;

	(void)prevState;
	netUpnpUpdateErrorHistory();
	ctx->flags &= ~(NET_UPNP_FLAG_STUN_DONE | NET_UPNP_FLAG_HAS_NAT_RESULT);
	ctx->largePacketOk = 0;
	ctx->lastError = 0;
	ctx->mappedAddr.s_addr = 0;
	sceKernelClearEventFlag(ctx->eventFlagId,
				~(NET_UPNP_EV_INET_CONNECTED | NET_UPNP_EV_INET_DISCONNECTED));

	if (ctx->netCtlCallbackId < 0)
		ret = SceShell_2E0B6FA6(netUpnpOnNetCtlEvent, ctx, &ctx->netCtlCallbackId);

	if (ret >= 0) {
		ret = SceShell_77642C92(&inetState);
		if (ret >= 0) {
			if (inetState == 4) {
				g_upnpInetConnected = 1;
			} else {
				sceKernelWaitEventFlag(ctx->eventFlagId,
						       NET_UPNP_EV_STOP | NET_UPNP_EV_INET_CONNECTED,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						       &outBits, NULL);
				if ((outBits & NET_UPNP_EV_STOP) != 0) {
					*outEvent = 6;
					*outNextState = NET_UPNP_STATE_IDLE;
					ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
					return 0;
				}
			}

			ret = SceShell_7AD86CE1(SCE_NET_CTL_INFO_IP_ADDRESS, &ipInfo);
			if (ret >= 0) {
				sceNetInetPton(SCE_NET_AF_INET, ipInfo.ip_address,
					       &ctx->localAddr);
				*outEvent = 10;
				*outNextState = NET_UPNP_STATE_STUN_CHECK;
				return 0;
			}
		}
	}

	if (sceKernelPollEventFlag(ctx->eventFlagId,
				   NET_UPNP_EV_STOP | NET_UPNP_EV_INET_DISCONNECTED,
				   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				   &outBits) >= 0) {
		if ((outBits & NET_UPNP_EV_STOP) != 0) {
			*outEvent = 6;
			*outNextState = NET_UPNP_STATE_IDLE;
			ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
			return 0;
		}
		if ((outBits & NET_UPNP_EV_INET_DISCONNECTED) != 0) {
			*outEvent = 5;
			*outNextState = NET_UPNP_STATE_WAIT_INET;
			return 0;
		}
	}

	*outEvent = 9;
	ctx->lastError = ret;
	*outNextState = NET_UPNP_STATE_STUN_CHECK;
	return 0;
}

/* 0x8121A8FE */
static void netUpnpOnStunEvent(int stunId, void *userArg, int eventOrError, const void *result)
{
	NetUpnpContext *ctx = (NetUpnpContext *)userArg;

	(void)stunId;
	if (eventOrError < 0) {
		ctx->stunStatus = eventOrError;
		sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_DONE);
		return;
	}

	switch (eventOrError) {
	case NET_STUN_EVENT_NAT_TYPE_DONE:
		ctx->stunStatus = 0;
		sceClibMemcpy_safe(&ctx->stunResult, result, sizeof(ctx->stunResult));
		sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_DONE);
		break;
	case NET_STUN_EVENT_MAPPED_ADDR_DONE:
		ctx->stunStatus = 0;
		sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_DONE);
		break;
	case NET_STUN_EVENT_ABORTED:
		ctx->stunStatus = 0;
		sceKernelSetEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_ABORTED);
		break;
	case NET_STUN_EVENT_MAPPED_ADDR_NOTIFY:
		sceClibMemcpy_safe(&ctx->mappedAddr, result, sizeof(ctx->mappedAddr));
		break;
	default:
		break;
	}
}

/* 0x8121A96C */
static int netUpnpStateStunCheck(NetUpnpContext *ctx, int prevState,
				 int *outNextState, int *outEvent)
{
	int natStunId = 0;
	int largePacketStunId = 0;
	int sockId = -1;
	SceUInt32 outBits = 0;
	SceUInt32 abortBits = 0;
	NetStunOption natOpt;
	NetStunOption largeOpt;
	SceNetSockaddrIn bindAddr;
	int ret;

	(void)prevState;
	sceClibMemset(&natOpt, 0, sizeof(natOpt));
	natOpt.requestXorMapped = 1;
	natOpt.username = sceNetHtonl(1);
	sceClibMemcpy_safe(natOpt.hmacKey, s_stunHmacKey, sizeof(natOpt.hmacKey));
	natOpt.upnpFlags = NET_STUN_UPNP_ENABLE;

	ret = netStunCreate(ctx->threadPriority, 0, ctx->stunHost, NET_UPNP_STUN_PORT,
			    &natOpt, netUpnpOnStunEvent, &natStunId);
	if (ret >= 0) {
		sceClibMemset(&ctx->stunResult, 0, sizeof(ctx->stunResult));
		sceKernelClearEventFlag(ctx->eventFlagId,
					~(NET_UPNP_EV_STUN_DONE | NET_UPNP_EV_STUN_ABORTED));
		ret = sceNetSocket("SceNatType", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
		if (ret >= 0) {
			sockId = ret;
			sceClibMemset(&bindAddr, 0, sizeof(bindAddr));
			bindAddr.sin_family = SCE_NET_AF_INET;
			bindAddr.sin_port = sceNetHtons(NET_UPNP_LOCAL_BIND_PORT);
			bindAddr.sin_addr.s_addr = 0;
			ret = sceNetBind(sockId, (const SceNetSockaddr *)&bindAddr,
					 sizeof(bindAddr));
			if (ret >= 0)
				ret = netStunStartNatTypeCheckOnSocket(natStunId, sockId, ctx);
		}
	}

	if (ret < 0) {
		*outEvent = 9;
		*outNextState = NET_UPNP_STATE_ERROR_WAIT;
		ctx->lastError = ret;
	} else {
		sceKernelWaitEventFlag(ctx->eventFlagId,
				       NET_UPNP_EV_STOP | NET_UPNP_EV_INET_DISCONNECTED |
				       NET_UPNP_EV_STUN_DONE,
				       SCE_KERNEL_EVF_WAITMODE_OR |
				       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       &outBits, NULL);
		if ((outBits & NET_UPNP_EV_STOP) != 0) {
			if (netStunAbort(natStunId, ctx) >= 0) {
				sceKernelWaitEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_ABORTED,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						       &abortBits, NULL);
			}
			ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
			*outNextState = NET_UPNP_STATE_IDLE;
			*outEvent = 6;
		} else if ((outBits & NET_UPNP_EV_INET_DISCONNECTED) != 0) {
			if (netStunAbort(natStunId, ctx) >= 0) {
				sceKernelWaitEventFlag(ctx->eventFlagId, NET_UPNP_EV_STUN_ABORTED,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						       &abortBits, NULL);
			}
			*outEvent = 5;
			*outNextState = NET_UPNP_STATE_WAIT_INET;
		} else if (ctx->stunStatus < 0) {
			ctx->lastError = ctx->stunStatus;
			*outEvent = 8;
			*outNextState = NET_UPNP_STATE_COMPLETE;
		} else {
			ctx->flags |= NET_UPNP_FLAG_HAS_NAT_RESULT;
			bool finishComplete = true;

			if ((ctx->flags & NET_UPNP_FLAG_PORT_CHECK) != 0) {
				ctx->largePacketOk = 1;
				sceClibMemset(&largeOpt, 0, sizeof(largeOpt));
				largeOpt.requestXorMapped = 1;
				largeOpt.username = sceNetHtonl(1);
				sceClibMemcpy_safe(largeOpt.hmacKey, s_stunHmacKey,
						   sizeof(largeOpt.hmacKey));
				sceClibMemset(g_upnpLargePacketBuf, 0,
					      sizeof(g_upnpLargePacketBuf));
				largeOpt.hasCustomAttr = 1;
				largeOpt.customAttrData = g_upnpLargePacketBuf;
				largeOpt.customAttrSize = sizeof(g_upnpLargePacketBuf);

				ret = netStunCreate(ctx->threadPriority, 0, ctx->stunHost,
						    NET_UPNP_STUN_PORT, &largeOpt,
						    netUpnpOnStunEvent, &largePacketStunId);
				if (ret >= 0) {
					ret = netStunStartMappedAddrCheck(largePacketStunId,
									  0, ctx);
				}
				if (ret >= 0) {
					sceKernelWaitEventFlag(ctx->eventFlagId,
							       NET_UPNP_EV_STOP |
							       NET_UPNP_EV_INET_DISCONNECTED |
							       NET_UPNP_EV_STUN_DONE,
							       SCE_KERNEL_EVF_WAITMODE_OR |
							       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							       &outBits, NULL);
					if ((outBits & NET_UPNP_EV_STOP) != 0) {
						if (netStunAbort(largePacketStunId, ctx) >= 0) {
							sceKernelWaitEventFlag(
								ctx->eventFlagId,
								NET_UPNP_EV_STUN_ABORTED,
								SCE_KERNEL_EVF_WAITMODE_OR |
								SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
								&abortBits, NULL);
						}
						ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
						*outNextState = NET_UPNP_STATE_IDLE;
						*outEvent = 6;
						finishComplete = false;
					} else if ((outBits & NET_UPNP_EV_INET_DISCONNECTED) != 0) {
						if (netStunAbort(largePacketStunId, ctx) >= 0) {
							sceKernelWaitEventFlag(
								ctx->eventFlagId,
								NET_UPNP_EV_STUN_ABORTED,
								SCE_KERNEL_EVF_WAITMODE_OR |
								SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
								&abortBits, NULL);
						}
						*outEvent = 5;
						*outNextState = NET_UPNP_STATE_WAIT_INET;
						finishComplete = false;
					} else if (ctx->stunStatus == NET_STUN_ERROR_TIMEOUT ||
						   ctx->stunStatus == NET_STUN_ERROR_EXT_ATTR_LEN_MISMATCH ||
						   ctx->stunStatus == NET_STUN_ERROR_EXT_ATTR_MISMATCH) {
						ctx->largePacketOk = 0;
					}
				}
			}

			if (finishComplete) {
				ctx->flags |= NET_UPNP_FLAG_STUN_DONE;
				*outEvent = 7;
				*outNextState = NET_UPNP_STATE_COMPLETE;
			}
		}
	}

	if (natStunId != 0)
		netStunDestroy(natStunId);
	if (largePacketStunId != 0)
		netStunDestroy(largePacketStunId);
	if (sockId >= 0)
		sceNetSocketClose(sockId);
	return ret;
}

/* 0x8121AC6E */
static int netUpnpStateComplete(NetUpnpContext *ctx, int prevState,
				int *outNextState, int *outEvent)
{
	SceUInt32 outBits = 0;

	(void)prevState;
	netUpnpUpdateErrorHistory();
	sceKernelWaitEventFlag(ctx->eventFlagId,
			       NET_UPNP_EV_STOP | NET_UPNP_EV_INET_DISCONNECTED,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       &outBits, NULL);
	if ((outBits & NET_UPNP_EV_INET_DISCONNECTED) != 0) {
		*outEvent = 5;
		*outNextState = NET_UPNP_STATE_WAIT_INET;
	} else {
		*outEvent = 6;
		*outNextState = NET_UPNP_STATE_IDLE;
		ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
	}
	return 0;
}

/* 0x8121AD06 */
static int netUpnpStateErrorWait(NetUpnpContext *ctx, int prevState,
				 int *outNextState, int *outEvent)
{
	SceUInt32 outBits = 0;

	(void)prevState;
	netUpnpUpdateErrorHistory();
	sceKernelWaitEventFlag(ctx->eventFlagId,
			       NET_UPNP_EV_STOP | NET_UPNP_EV_INET_DISCONNECTED | NET_UPNP_EV_RETRY,
			       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       &outBits, NULL);
	if ((outBits & NET_UPNP_EV_INET_DISCONNECTED) != 0) {
		*outEvent = 5;
		*outNextState = NET_UPNP_STATE_WAIT_INET;
	} else if ((outBits & NET_UPNP_EV_RETRY) != 0) {
		*outEvent = 12;
		*outNextState = NET_UPNP_STATE_WAIT_INET;
	} else {
		*outEvent = 6;
		*outNextState = NET_UPNP_STATE_IDLE;
		ctx->flags &= ~NET_UPNP_FLAG_ACTIVE;
	}
	return 0;
}
