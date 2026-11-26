/*
 * net_pppoe.cpp: SceNetPppoe client lifecycle and monitoring thread
 * statically linked into SceShell (0x8137B954..0x8137BFFF).
 */
#include "net_config_internal.h"

#define PPPOE_FLAG_STATIC_DNS			0x00000001u
#define PPPOE_CTX_IN_CALLBACK			0x00000002u
#define PPPOE_CTX_STOP_REQUESTED		0x00000004u

enum NetPppoeCallbackEvent {
	PPPOE_EV_CONNECTED = 1,
	PPPOE_EV_DISCONNECTED = 2,
	PPPOE_EV_ERROR = 3
};

typedef void (*NetPppoeEventCallback)(void *userArg, int event, int err);

typedef struct NetPppoeStartParam {
	SceNetInAddr staticDnsMain;		/* +0x000 */
	SceNetInAddr staticDnsAlt;		/* +0x004 */
	char authName[128];			/* +0x008 */
	char authKey[128];			/* +0x088 */
	SceUInt32 mtu;				/* +0x108 */
	SceUInt32 flags;			/* +0x10C */
} NetPppoeStartParam;

typedef struct NetPppoeContext {
	int lowerIfId;				/* +0x000 */
	int pppoeIfId;				/* +0x004 */
	NetPppoeEventCallback callback;		/* +0x008 */
	void *callbackArg;			/* +0x00C */
	NetPppoeStartParam param;		/* +0x010..+0x11F (272B) */
	SceUID threadId;			/* +0x120 */
	int ctlSock;				/* +0x124 */
	SceUInt32 stateFlags;			/* +0x128 */
} NetPppoeContext;

typedef struct PppoeIfNameReq {
	char ifName[16];
	union {
		int mtu;
		SceUInt8 pad[16];
	} value;
} PppoeIfNameReq;

typedef struct PppoeIntReq {
	char ifName[16];
	int value;
} PppoeIntReq;

typedef struct PppoeAuthStatusReq {
	char ifName[16];
	int status;
	SceUInt8 detail[256];
} PppoeAuthStatusReq;

typedef struct PppoeDnsReq {
	char ifName[16];
	SceNetInAddr dnsMain;
	SceNetInAddr dnsAlt;
} PppoeDnsReq;

typedef struct PppoeBindParamsReq {
	char ifName[16];
	char ethIfName[16];
	SceUInt8 trailing[16];
} PppoeBindParamsReq;

/* NetBSD struct spppauthcfg (64 bytes) */
typedef struct SpppAuthCfg {
	char ifName[16];
	SceUInt32 hisAuth;
	SceUInt32 myAuth;
	SceSize myNameLen;
	SceSize mySecretLen;
	SceSize hisNameLen;
	SceSize hisSecretLen;
	SceUInt32 myAuthFlags;
	SceUInt32 hisAuthFlags;
	const char *myName;
	const char *mySecret;
	const char *hisName;
	const char *hisSecret;
} SpppAuthCfg;

static NetPppoeContext *g_pppoeActive = NULL;
static NetPppoeContext g_pppoeStorage;
static NetThreadParam g_pppoeThreadParam;

static int pppoeGetError(int pppoeIfId, const int *ctlSock)
{
	const char *ifName = SceNetInternal_F3917021(pppoeIfId);
	PppoeIntReq sessionReq;

	sceClibMemset(&sessionReq, 0, sizeof(sessionReq));
	sceClibStrncpy(sessionReq.ifName, ifName, sizeof(sessionReq.ifName) - 1);
	if (SceNetInternal_333F9CB3(*ctlSock, NET_PPPOEGETSESSION, &sessionReq) < 0)
		return NETPPPOE_ERR_SESSION_IOCTL;

	int sessionErr = 0;
	switch (sessionReq.value) {
	case 0:
		sessionErr = NETPPPOE_ERR_STATE_INITIAL;
		break;
	case 1:
		sessionErr = NETPPPOE_ERR_STATE_PADI;
		break;
	case 2:
	case 4: {
		PppoeAuthStatusReq authReq;
		sceClibMemset(&authReq, 0, sizeof(authReq));
		sceClibStrncpy(authReq.ifName, ifName, sizeof(authReq.ifName) - 1);
		if (SceNetInternal_333F9CB3(*ctlSock, NET_PPPOEGETAUTHSTATUS, &authReq) < 0)
			return NETPPPOE_ERR_AUTH_STATUS_IOCTL;

		switch (authReq.status) {
		case 513:
			sessionErr = NETPPPOE_ERR_AUTH_PAP;
			break;
		case 514:
			sessionErr = NETPPPOE_ERR_AUTH_CHAP;
			break;
		case 515:
			sessionErr = NETPPPOE_ERR_AUTH_UNKNOWN;
			break;
		default:
			sessionErr = 0;
			break;
		}
		break;
	}
	case 3:
		sessionErr = NETPPPOE_ERR_STATE_PADR;
		break;
	case 6:
		sessionErr = NETPPPOE_ERR_STATE_CLOSING;
		break;
	default:
		sessionErr = 0;
		break;
	}

	PppoeIntReq statusReq;
	sceClibMemset(&statusReq, 0, sizeof(statusReq));
	sceClibStrncpy(statusReq.ifName, ifName, sizeof(statusReq.ifName) - 1);
	int ioctlRet = SceNetInternal_333F9CB3(*ctlSock, NET_SPPPGETSTATUS, &statusReq);
	if (ioctlRet < 0)
		return NETPPPOE_ERR_STATUS_IOCTL;

	switch (statusReq.value) {
	case 0:
	case 1:
		return sessionErr;
	case 2:
		return NETPPPOE_ERR_NO_CONCENTRATOR;
	case 3:
		return NETPPPOE_ERR_LCP_TIMEOUT;
	case 4:
		return NETPPPOE_ERR_AUTH_FAILED;
	default:
		return ioctlRet;
	}
}

static int pppoeMonitorThread(void)
{
	NetPppoeContext *ctx = g_pppoeActive;
	const char *ifName = SceNetInternal_F3917021(ctx->pppoeIfId);

	SceNetInAddr localIp = { 0 };
	SceNetInAddr peerIp = { 0 };
	SceNetInAddr initPeerIp;
	initPeerIp.s_addr = sceNetHtonl(1u);

	int err = sceNetConfigSetIfaddr(ctx->pppoeIfId, &localIp, &initPeerIp, NULL);
	bool notifyError = true;

	if (err >= 0) {
		PppoeIntReq linkReq;
		sceClibMemset(&linkReq, 0, sizeof(linkReq));
		sceClibStrncpy(linkReq.ifName, ifName, sizeof(linkReq.ifName) - 1);

		int negotiatingTicks = 0;
		bool ipAssigned = false;
		while (!ipAssigned) {
			if (SceNetInternal_333F9CB3(ctx->ctlSock, NET_PPPOEGETLINK, &linkReq) < 0) {
				err = NETPPPOE_ERR_STATUS_IOCTL;
				break;
			}
			if (linkReq.value == 0 ||
			    (linkReq.value == 3 && ++negotiatingTicks > 10)) {
				err = pppoeGetError(ctx->pppoeIfId, &ctx->ctlSock);
				break;
			}

			NetIfAddrRecord ifAddr;
			err = sceNetConfigGetIfaddrs(ctx->pppoeIfId, &ifAddr, 1);
			if (err < 0)
				break;

			localIp.s_addr = ifAddr.address;
			peerIp.s_addr = ifAddr.broadcastAddress;
			if (ifAddr.address != 0) {
				ipAssigned = true;
				break;
			}
			if ((ctx->stateFlags & PPPOE_CTX_STOP_REQUESTED) != 0) {
				notifyError = false;
				break;
			}
			SceNetInternal_104E4820(1000000);
		}

		if (ipAssigned) {
			err = sceNetConfigSetDefaultRoute(&peerIp);
			if (err >= 0) {
				SceNetInAddr dnsPair[2];
				if ((ctx->param.flags & PPPOE_FLAG_STATIC_DNS) != 0) {
					dnsPair[0] = ctx->param.staticDnsMain;
					dnsPair[1] = ctx->param.staticDnsAlt;
				} else {
					PppoeDnsReq dnsReq;
					sceClibMemset(&dnsReq, 0, sizeof(dnsReq));
					sceClibStrncpy(dnsReq.ifName, ifName,
						       sizeof(dnsReq.ifName) - 1);
					if (SceNetInternal_333F9CB3(ctx->ctlSock, NET_SPPPGETDNS,
								    &dnsReq) < 0) {
						err = NETPPPOE_ERR_GET_DNS;
					} else {
						dnsPair[0] = dnsReq.dnsMain;
						dnsPair[1] = dnsReq.dnsAlt;
					}
				}

				if (err >= 0)
					err = SceNetInternal_16222ED6(dnsPair, 0);

				if (err >= 0) {
					sceNetConfigAddMRoute(&localIp);
					if (ctx->callback != NULL) {
						ctx->stateFlags |= PPPOE_CTX_IN_CALLBACK;
						ctx->callback(ctx->callbackArg,
							      PPPOE_EV_CONNECTED, 0);
						ctx->stateFlags &= ~PPPOE_CTX_IN_CALLBACK;
					}

					for (;;) {
						if (SceNetInternal_333F9CB3(ctx->ctlSock,
									    NET_PPPOEGETLINK,
									    &linkReq) < 0) {
							err = NETPPPOE_ERR_STATUS_IOCTL;
							break;
						}
						if (linkReq.value == 0) {
							err = pppoeGetError(ctx->pppoeIfId,
									    &ctx->ctlSock);
							break;
						}
						if ((ctx->stateFlags & PPPOE_CTX_STOP_REQUESTED) != 0) {
							notifyError = false;
							break;
						}
						SceNetInternal_104E4820(1000000);
					}
				}
			}
		}
	}

	if (notifyError && ctx->callback != NULL) {
		ctx->stateFlags |= PPPOE_CTX_IN_CALLBACK;
		ctx->callback(ctx->callbackArg, PPPOE_EV_ERROR, err);
		ctx->stateFlags &= ~PPPOE_CTX_IN_CALLBACK;
	}

	sceNetConfigDownInterface(ctx->pppoeIfId);
	SceNetInternal_104E4820(1000000);
	sceNetConfigDelIfaddr(ctx->pppoeIfId);
	sceNetConfigDelMRoute(&localIp);
	sceNetConfigDelDefaultRoute();

	SceNetInAddr zeroDns[2];
	sceClibMemset(zeroDns, 0, sizeof(zeroDns));
	SceNetInternal_16222ED6(zeroDns, 0);

	if (ctx->callback != NULL) {
		ctx->stateFlags |= PPPOE_CTX_IN_CALLBACK;
		ctx->callback(ctx->callbackArg, PPPOE_EV_DISCONNECTED, err);
		ctx->stateFlags &= ~PPPOE_CTX_IN_CALLBACK;
	}

	return SceNetInternal_4EF96CD8();
}

int sceNetPppoeStart(int ifId, int pppoeIfId, void (*callback)(void *userArg, int event, int err),
		     void *userArg, SceSize stackSize, int priority, const void *pppoeParam)
{
	const NetPppoeStartParam *param = (const NetPppoeStartParam *)pppoeParam;
	int err;

	if (g_pppoeActive != NULL)
		return NETPPPOE_ERR_ALREADY_STARTED;

	g_pppoeActive = &g_pppoeStorage;
	sceClibMemset(&g_pppoeStorage, 0, sizeof(g_pppoeStorage));
	g_pppoeStorage.lowerIfId = ifId;
	g_pppoeStorage.pppoeIfId = pppoeIfId;
	g_pppoeStorage.callback = callback;
	g_pppoeStorage.callbackArg = userArg;
	sceClibMemcpy_safe(&g_pppoeStorage.param, param, sizeof(g_pppoeStorage.param));

	const char *lowerName = SceNetInternal_F3917021(g_pppoeStorage.lowerIfId);
	const char *pppoeName = SceNetInternal_F3917021(g_pppoeStorage.pppoeIfId);

	int sock = sceNetSocket("SceNetPppoe", NET_AF_INET, NET_SOCK_DGRAM, 0);
	if (sock < 0)
		return NETPPPOE_ERR_SOCKET;
	g_pppoeStorage.ctlSock = sock;

	PppoeIfNameReq ifReq;
	sceClibMemset(&ifReq, 0, sizeof(ifReq));
	sceClibStrncpy(ifReq.ifName, pppoeName, sizeof(ifReq.ifName) - 1);
	if (SceNetInternal_333F9CB3(sock, NET_SIOCIFCREATE, &ifReq) < 0) {
		sceNetSocketClose(sock);
		return NETPPPOE_ERR_IF_CREATE;
	}

	PppoeBindParamsReq bindReq;
	sceClibMemset(&bindReq, 0, sizeof(bindReq));
	sceClibStrncpy(bindReq.ethIfName, lowerName, sizeof(bindReq.ethIfName) - 1);
	sceClibStrncpy(bindReq.ifName, pppoeName, sizeof(bindReq.ifName) - 1);

	bool linkConfigured = (SceNetInternal_333F9CB3(sock, NET_PPPOESETPARMS, &bindReq) >= 0);
	if (linkConfigured && param->mtu >= 1u && param->mtu <= 1492u) {
		sceClibMemset(&ifReq, 0, sizeof(ifReq));
		sceClibStrncpy(ifReq.ifName, pppoeName, sizeof(ifReq.ifName) - 1);
		ifReq.value.mtu = (int)param->mtu;
		linkConfigured = (SceNetInternal_333F9CB3(sock, NET_SIOCSIFMTU, &ifReq) >= 0);
	}

	if (!linkConfigured) {
		err = NETPPPOE_ERR_LINK_PARAMS;
	} else {
		SpppAuthCfg authCfg;
		sceClibMemset(&authCfg, 0, sizeof(authCfg));
		sceClibStrncpy(authCfg.ifName, pppoeName, sizeof(authCfg.ifName) - 1);
		authCfg.myAuth = 0;
		authCfg.hisAuth = 0;

		if (sceClibStrnlen(param->authName, sizeof(param->authName)) != 0) {
			authCfg.myName = param->authName;
			authCfg.myNameLen = sceClibStrnlen(param->authName,
							   sizeof(param->authName)) + 1u;
		}
		if (sceClibStrnlen(param->authKey, sizeof(param->authKey)) != 0) {
			authCfg.mySecret = param->authKey;
			authCfg.mySecretLen = sceClibStrnlen(param->authKey,
							     sizeof(param->authKey)) + 1u;
		}

		PppoeIntReq lcpReq;
		sceClibMemset(&lcpReq, 0, sizeof(lcpReq));
		sceClibStrncpy(lcpReq.ifName, pppoeName, sizeof(lcpReq.ifName) - 1);
		lcpReq.value = 1;

		if (SceNetInternal_333F9CB3(sock, NET_SPPPSETAUTHCFG, &authCfg) < 0 ||
		    SceNetInternal_333F9CB3(sock, NET_SPPPSETLCP, &lcpReq) < 0) {
			err = NETPPPOE_ERR_AUTH_CONFIG;
		} else {
			bool dnsConfigured = true;
			if ((param->flags & PPPOE_FLAG_STATIC_DNS) == 0) {
				PppoeIntReq dnsReq;
				sceClibMemset(&dnsReq, 0, sizeof(dnsReq));
				sceClibStrncpy(dnsReq.ifName, pppoeName,
					       sizeof(dnsReq.ifName) - 1);
				dnsReq.value = 3;
				dnsConfigured = (SceNetInternal_333F9CB3(sock, NET_SPPPSETDNS,
									 &dnsReq) >= 0);
			}

			if (!dnsConfigured) {
				err = NETPPPOE_ERR_DNS_CONFIG;
			} else {
				g_pppoeThreadParam.entry = pppoeMonitorThread;
				g_pppoeThreadParam.argument = NULL;
				g_pppoeThreadParam.priority = priority;
				g_pppoeThreadParam.stackSize = (int)stackSize;
				g_pppoeThreadParam.flags = 0;
				err = SceNetInternal_225C4801(&g_pppoeStorage.threadId,
							      &g_pppoeThreadParam, "SceNetPppoe");
				if (err >= 0)
					return 0;
			}
		}
	}

	sceClibMemset(&ifReq, 0, sizeof(ifReq));
	sceClibStrncpy(ifReq.ifName, pppoeName, sizeof(ifReq.ifName) - 1);
	SceNetInternal_333F9CB3(sock, NET_SIOCIFDESTROY, &ifReq);
	sceNetSocketClose(sock);
	return err;
}

int sceNetPppoeStop(int pppoeIfId)
{
	(void)pppoeIfId;
	NetPppoeContext *ctx = g_pppoeActive;

	if (ctx == NULL)
		return NETPPPOE_ERR_NOT_STARTED;

	const char *pppoeName = SceNetInternal_F3917021(ctx->pppoeIfId);
	ctx->stateFlags |= PPPOE_CTX_STOP_REQUESTED;
	SceNetInternal_3B5F94B7(ctx->threadId);

	PppoeIfNameReq ifReq;
	sceClibMemset(&ifReq, 0, sizeof(ifReq));
	sceClibStrncpy(ifReq.ifName, pppoeName, sizeof(ifReq.ifName) - 1);
	SceNetInternal_333F9CB3(ctx->ctlSock, NET_SIOCIFDESTROY, &ifReq);
	sceNetSocketClose(ctx->ctlSock);

	g_pppoeActive = NULL;
	return 0;
}
