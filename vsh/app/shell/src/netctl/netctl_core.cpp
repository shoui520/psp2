/*
 * netctl_core.cpp: SceNetCtlInt core state, locks, callbacks, key parsers,
 * connection lifecycle, process tracking, network event filter, and IP/DHCP/PPPoE/WISPr setup.
 * 3.65-CEX: 0x8120F1B4 - 0x81213E0C
 */
#include <stddef.h>
#include <stdlib.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <net.h>
#include "netctl_internal.h"

#define NETCTL_IP_WAIT_TIMEOUT_USEC		60000000u
#define NETCTL_SLEEP_TIMEOUT_SHORT		120
#define NETCTL_SLEEP_TIMEOUT_INFINITE		0x7FFFFFFF

struct NetCtlIfInfo {
	SceUInt8 header[80];
	SceNetEtherAddr etherAddr;
	SceUInt8 padEther[2];
	SceUInt32 mtu;
	SceUInt8 trailing[228];
};

struct NetCtlIfAddrEntry {
	SceNetInAddr addr;
	SceNetInAddr peer;
	SceNetInAddr netmask;
	SceNetInAddr broadcast;
};

struct NetCtlIfStatDelta {
	SceUInt32 txBytes;
	SceUInt32 reservedHead[7];
	SceUInt32 rxBytes;
	SceUInt32 reservedMid[7];
	SceUInt32 activeSec;
	SceUInt32 reservedTail;
};

struct NetCtlSleepParam {
	int handle;
	int reservedHead;
	int timeoutSec;
	int enable;
	int reservedTail;
};

struct NetCtlEventFilterCtx {
	int mode;					/* 0=dispatch, 1=flushAll, 2=flushUp, 3=flushDown */
	int isScan;
	int targetIfId;
	NetCtlChannel *channel;
	SceUInt32 waitMask;
	SceUInt32 *outBits;
};

struct NetCtlDhcpParam {
	SceNetInAddr dnsServers[2];			/* +0x000 */
	char hostname[256];				/* +0x008 */
	char vendorClass[16];				/* +0x108 */
	SceUInt32 reserved;				/* +0x118 */
	SceUInt32 flags;				/* +0x11C */
};

struct NetCtlPppoeParam {
	SceNetInAddr dnsServers[2];			/* +0x000 */
	char authName[128];				/* +0x008 */
	char authKey[128];				/* +0x088 */
	SceUInt32 mtu;					/* +0x108 */
	SceUInt32 flags;				/* +0x10C */
};

struct NetCtlAutoConnectIfInfo {
	SceUInt32 reserved;
	SceUInt32 isSpecial;
	SceUInt32 tail[2];
};

NetCtlChannel g_netCtlInet;				/* 0x8156F5E8 */
NetCtlChannel g_netCtlAdhoc;				/* 0x81583890 */
SceKernelLwMutexWork g_netCtlMutex;			/* 0x81597CB8 */
int g_netCtlMutexReady = 0;				/* 0x8154507C */
SceUInt64 g_netCtlAdhocLeaveTime = 0;			/* 0x815A1140 */

static int s_callbackMutexReady = 0;			/* 0x81545080 */
static int s_fileIoMutexReady = 0;			/* 0x81545084 */
static SceUID s_modeSema = -1;				/* 0x81545088 */
static SceUID s_frozenPid = -1;				/* 0x8154508C */
static int s_wirelessMode = 0;				/* 0x81545090 */
static int s_activeOwner = 0;				/* 0x81545094 */
static int s_ethIfRefCount = 0;				/* 0x81545098 */
static int s_wlanIfRefCount = 0;			/* 0x8154509C */
static int s_phoneIfRefCount = 0;			/* 0x815450A0 */
static int s_ethIfOwner = -1;				/* 0x815450A4 */
static int s_wlanIfOwner = -1;				/* 0x815450A8 */
static int s_phoneIfOwner = -1;				/* 0x815450AC */
static int s_macAddrStatus = 0;				/* 0x815450B0 */
static int s_errorPrinted = 0;				/* 0x815450B4 */
static int s_staticIpActive = 0;			/* 0x815450B8 */
static int s_dhcpActive = 0;				/* 0x815450BC */
static int s_pppoeActive = 0;				/* 0x815450C0 */

static NetCtlCallbackSlot s_inetCallbacks[NETCTL_MAX_CALLBACKS];  /* 0x81597B38 */
static NetCtlCallbackSlot s_adhocCallbacks[NETCTL_MAX_CALLBACKS]; /* 0x81597BF8 */
static SceKernelLwMutexWork s_callbackMutex;		/* 0x81597CD8 */
static SceKernelLwMutexWork s_fileIoMutex;		/* 0x81597CF8 */
static NetCtlWisprStatus s_wisprStatus;			/* 0x81597D18 */
static NetCtlProcessLink s_processLinks[NETCTL_MAX_PROCESS_LINKS]; /* 0x81598024 */
static SceUInt16 s_nextProcessToken = 0;		/* 0x81598144 */
static NetCtlProcessSlot s_processSlots[NETCTL_MAX_PROCESS_SLOTS]; /* 0x81598148 */
static SceNetEtherAddr s_deviceMacAddr;			/* 0x81598228 */

/* 0x8120F1B4 */
int netCtlCoreIsAdhocCooldownExpired(void)
{
	SceUInt64 now = sceKernelGetSystemTimeWideForUser();

	if (g_netCtlAdhocLeaveTime == 0 || (now - g_netCtlAdhocLeaveTime) > NETCTL_ADHOC_COOLDOWN_USEC)
		return 1;
	return 0;
}

/* 0x8120F204 */
int netCtlCoreGetMacAddress(SceNetEtherAddr *outAddr)
{
	if (s_macAddrStatus < 0)
		return s_macAddrStatus;
	sceClibMemcpy_safe(outAddr, &s_deviceMacAddr, sizeof(SceNetEtherAddr));
	return 0;
}

/* 0x8120F22A */
int netCtlCoreHasEthernetCap(void)
{
	if (vshKernelCheckModelCapability(5) != 0 || vshKernelCheckModelCapability(6) != 0)
		return 1;
	return 0;
}

/* 0x8120F248 */
int netCtlCoreIsDevkit(void)
{
	return (vshKernelCheckModelCapability(4) != 0) ? 1 : 0;
}

/* 0x8120F25C */
int netCtlCoreUnlock(void)
{
	if (g_netCtlMutexReady != 0)
		return sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	return NETCTL_ERR_MUTEX_NOT_READY;
}

/* 0x8120F284 */
int netCtlCoreLock(void)
{
	if (g_netCtlMutexReady != 0)
		return sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL);
	return NETCTL_ERR_MUTEX_NOT_READY;
}

/* 0x8120F2AE */
static int netCtlIsActiveOwner(const NetCtlChannel *ch)
{
	int owned = 0;

	if (g_netCtlMutexReady != 0 && sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL) >= 0) {
		if (ch->isInet == 0)
			owned = (s_activeOwner == 2);
		else if (ch->isInet == 1)
			owned = (s_activeOwner == 1);
		if (g_netCtlMutexReady != 0)
			sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	}
	return owned;
}

/* 0x8120F322 */
int netCtlCoreResetSleepTimers(NetCtlChannel *ch)
{
	int ifIds[10];

	if (ch != NULL && !netCtlIsActiveOwner(ch))
		return 0;

	int count = SceNetInternal_8157DE3E(ifIds, 10, 0);

	if (count >= 0) {
		for (int i = 0; i < count; ++i) {
			NetCtlSleepParam param;

			sceClibMemset(&param, 0, sizeof(param));
			param.handle = -1;
			if ((ch != NULL && ch->fakeMobileIf != 0) ||
			    vshKernelCheckModelCapability(4) == 0)
				param.timeoutSec = NETCTL_SLEEP_TIMEOUT_SHORT;
			else
				param.timeoutSec = NETCTL_SLEEP_TIMEOUT_INFINITE;
			param.enable = 1;
			SceNetInternal_E03F6A77(ifIds[i], &param, 0);
		}
		s_activeOwner = 0;
	}
	return count;
}

/* 0x8120F3D6 */
static int netCtlStartChannelThread(int isInet, NetCtlChannel *ch, const char *name,
				    SceSize stackSize, int priority,
				    const ShellNetCtlHostFuncs *host)
{
	NetCtlChannel *threadArg = ch;

	sceClibMemset(ch, 0, sizeof(NetCtlChannel));
	ch->isInet = isInet;
	ch->flags |= NETCTL_FLAG_IDLE;
	ch->exclusiveRequestPid = -1;
	ch->adhocOwnerPid = -1;
	ch->threadId = -1;
	ch->eventFlagId = -1;
	ch->threadPriority = priority;
	ch->ifId = -1;
	if (host != NULL)
		ch->hostFuncs = host;
	if (ch->isInet == 1) {
		netCtlConfigLoadIfStat(ch);
		netCtlConfigGetMobileDns(ch->mobileDns);
	}

	SceUID evf = sceKernelCreateEventFlag(name, 0, 0, NULL);
	int ret = evf;

	if (evf >= 0) {
		ch->eventFlagId = evf;
		SceUID thid = sceKernelCreateThread(name, netCtlIcmThreadEntry,
						    priority, stackSize, 0, 0, NULL);
		ret = thid;
		if (thid >= 0) {
			ch->threadId = thid;
			ret = sceKernelStartThread(thid, sizeof(threadArg), &threadArg);
			if (ret >= 0)
				return ret;
		}
	}

	if (ch->threadId >= 0) {
		sceKernelDeleteThread(ch->threadId);
		ch->threadId = -1;
	}
	if (ch->eventFlagId >= 0) {
		sceKernelDeleteEventFlag(ch->eventFlagId);
		ch->eventFlagId = -1;
	}
	return ret;
}

/* 0x812132F4 */
static int netCtlResetIpState(void)
{
	s_pppoeActive = 0;
	s_dhcpActive = 0;
	s_staticIpActive = 0;
	return 0;
}

static void netCtlCleanupInit(void)
{
	sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_EXIT);
	if (g_netCtlInet.threadId >= 0) {
		sceKernelWaitThreadEnd(g_netCtlInet.threadId, NULL, NULL);
		sceKernelDeleteThread(g_netCtlInet.threadId);
		g_netCtlInet.threadId = -1;
	}
	if (g_netCtlInet.eventFlagId >= 0) {
		sceKernelDeleteEventFlag(g_netCtlInet.eventFlagId);
		g_netCtlInet.eventFlagId = -1;
	}

	sceKernelSetEventFlag(g_netCtlAdhoc.eventFlagId, NETCTL_EV_EXIT);
	if (g_netCtlAdhoc.threadId >= 0) {
		sceKernelWaitThreadEnd(g_netCtlAdhoc.threadId, NULL, NULL);
		sceKernelDeleteThread(g_netCtlAdhoc.threadId);
		g_netCtlAdhoc.threadId = -1;
	}
	if (g_netCtlAdhoc.eventFlagId >= 0) {
		sceKernelDeleteEventFlag(g_netCtlAdhoc.eventFlagId);
		g_netCtlAdhoc.eventFlagId = -1;
	}

	if (s_modeSema >= 0) {
		sceKernelDeleteSema(s_modeSema);
		s_modeSema = -1;
	}
	if (g_netCtlMutexReady == 1) {
		sceKernelDeleteLwMutex(&g_netCtlMutex);
		g_netCtlMutexReady = 0;
	}
	if (s_callbackMutexReady == 1) {
		sceKernelDeleteLwMutex(&s_callbackMutex);
		s_callbackMutexReady = 0;
	}
	if (s_fileIoMutexReady == 1) {
		sceKernelDeleteLwMutex(&s_fileIoMutex);
		s_fileIoMutexReady = 0;
	}
	netCtlResetIpState();
	netCtlTeardownLoopback();
}

/* 0x8120F4F8 */
int netCtlCoreInit(SceSize inetStackSize, int inetPriority, SceSize adhocStackSize,
		   int adhocPriority, const ShellNetCtlHostFuncs *host)
{
	NetCtlIfInfo ifInfo;
	int ret = netCtlResetIpState();

	if (ret < 0) {
		netCtlCleanupInit();
		return ret;
	}

	sceClibMemset(s_inetCallbacks, 0, sizeof(s_inetCallbacks));
	sceClibMemset(s_adhocCallbacks, 0, sizeof(s_adhocCallbacks));
	s_fileIoMutexReady = 0;
	s_callbackMutexReady = 0;
	g_netCtlMutexReady = 0;
	s_wirelessMode = 0;
	s_phoneIfRefCount = 0;
	s_wlanIfRefCount = 0;
	s_ethIfRefCount = 0;
	s_modeSema = -1;
	s_phoneIfOwner = -1;
	s_wlanIfOwner = -1;
	s_ethIfOwner = -1;

	sceClibMemset(s_processLinks, 0, sizeof(s_processLinks));
	for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
		s_processLinks[i].targetProcessId = -1;
		for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j)
			s_processLinks[i].linkedProcesses[j] = -1;
	}

	s_nextProcessToken = 0;
	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		sceClibMemset(&s_processSlots[i], 0, sizeof(NetCtlProcessSlot));
		s_processSlots[i].processId = -1;
	}

	netCtlSetupLoopback();
	netCtlCoreResetSleepTimers(NULL);

	ret = sceKernelCreateLwMutex(&g_netCtlMutex, "SceNetCtlInt",
				     SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret >= 0) {
		g_netCtlMutexReady = 1;
		ret = sceKernelCreateLwMutex(&s_callbackMutex, "SceNetCtlCallback",
					     SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
		if (ret >= 0) {
			s_callbackMutexReady = 1;
			ret = sceKernelCreateLwMutex(&s_fileIoMutex, "SceNetCtlFileIo",
						     SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
			if (ret >= 0) {
				s_fileIoMutexReady = 1;
				ret = sceKernelCreateSema("SceNetCtl", 0, 1, 1, NULL);
				if (ret >= 0) {
					s_modeSema = ret;
					ret = netCtlStartChannelThread(1, &g_netCtlInet,
								       "SceNetCtlInet",
								       inetStackSize,
								       inetPriority, host);
					if (ret >= 0) {
						ret = netCtlStartChannelThread(0, &g_netCtlAdhoc,
									       "SceNetCtlAdhoc",
									       adhocStackSize,
									       adhocPriority, host);
						if (ret >= 0) {
							if (netCtlCoreHasEthernetCap()) {
								int n = SceNetInternal_235DE96C(NETCTL_IF_WLAN, &ifInfo, 1);

								if (n < 0) {
									s_macAddrStatus = n;
									return 0;
								}
								if (n == 0) {
									s_macAddrStatus = NETCTL_ERR_NO_INTERFACE;
									return 0;
								}
							} else {
								int n = SceNetInternal_235DE96C(NETCTL_IF_ETHERNET, &ifInfo, 1);

								if (n != 1) {
									if (n != 0) {
										s_macAddrStatus = n;
										return 0;
									}
									int w = SceNetInternal_235DE96C(NETCTL_IF_WLAN, &ifInfo, 1);

									if (w < 0) {
										s_macAddrStatus = w;
										return 0;
									}
									if (w == 0) {
										s_macAddrStatus = NETCTL_ERR_NO_INTERFACE;
										return 0;
									}
								}
							}
							sceClibMemcpy_safe(&s_deviceMacAddr,
									   &ifInfo.etherAddr,
									   sizeof(SceNetEtherAddr));
							s_macAddrStatus = 1;
							return 0;
						}
					}
				}
			}
		}
	}

	netCtlCleanupInit();
	return ret;
}

/* 0x8120F97A */
int netCtlCoreTerm(void)
{
	SceShell_3F41E440();
	SceShell_C1886828();
	netCtlCleanupInit();
	return 0;
}

/* 0x8120FAC6 */
int netCtlCoreRegisterInetCallback(NetCtlEventCallback cb, void *arg, int *outId)
{
	int ret = sceKernelLockLwMutex(&s_callbackMutex, 1, NULL);

	if (ret < 0)
		return ret;

	int i;

	for (i = 0; i < NETCTL_MAX_CALLBACKS; ++i) {
		if (s_inetCallbacks[i].callback == NULL)
			break;
	}
	if (i == NETCTL_MAX_CALLBACKS) {
		ret = NETCTL_ERR_CALLBACK_MAX;
	} else {
		s_inetCallbacks[i].id = i;
		s_inetCallbacks[i].callback = cb;
		s_inetCallbacks[i].userArg = arg;
		*outId = i;
	}
	sceKernelUnlockLwMutex(&s_callbackMutex, 1);
	return ret;
}

/* 0x8120FB42 */
int netCtlCoreRegisterAdhocCallback(NetCtlEventCallback cb, void *arg, int *outId)
{
	int ret = sceKernelLockLwMutex(&s_callbackMutex, 1, NULL);

	if (ret < 0)
		return ret;

	int i;

	for (i = 0; i < NETCTL_MAX_CALLBACKS; ++i) {
		if (s_adhocCallbacks[i].callback == NULL)
			break;
	}
	if (i == NETCTL_MAX_CALLBACKS) {
		ret = NETCTL_ERR_CALLBACK_MAX;
	} else {
		s_adhocCallbacks[i].id = i;
		s_adhocCallbacks[i].callback = cb;
		s_adhocCallbacks[i].userArg = arg;
		*outId = i;
	}
	sceKernelUnlockLwMutex(&s_callbackMutex, 1);
	return ret;
}

/* 0x8120FBBE */
int netCtlCoreUnregisterInetCallback(int cid)
{
	int ret = sceKernelLockLwMutex(&s_callbackMutex, 1, NULL);

	if (ret < 0)
		return ret;

	if ((unsigned int)cid < NETCTL_MAX_CALLBACKS)
		s_inetCallbacks[cid].callback = NULL;
	else
		ret = NETCTL_ERR_INVALID_ID;
	sceKernelUnlockLwMutex(&s_callbackMutex, 1);
	return ret;
}

/* 0x8120FC10 */
int netCtlCoreUnregisterAdhocCallback(int cid)
{
	int ret = sceKernelLockLwMutex(&s_callbackMutex, 1, NULL);

	if (ret < 0)
		return ret;

	if ((unsigned int)cid < NETCTL_MAX_CALLBACKS)
		s_adhocCallbacks[cid].callback = NULL;
	else
		ret = NETCTL_ERR_INVALID_ID;
	sceKernelUnlockLwMutex(&s_callbackMutex, 1);
	return ret;
}

/* 0x8120FC62 */
int netCtlCoreNotifyCallbacks(NetCtlChannel *ch, int prevState, int newState, int eventCode)
{
	NetCtlCallbackSlot *table = (ch->isInet != 0) ? s_inetCallbacks : s_adhocCallbacks;
	int errCode = (eventCode != 0) ? ch->lastError : ch->scanError;

	if (sceKernelLockLwMutex(&s_callbackMutex, 1, NULL) >= 0) {
		if (eventCode == 9 && s_errorPrinted == 0) {
			sceClibPrintf("\n*** network error happened: 0x%x ***\n", errCode);
			s_errorPrinted = 1;
		} else if (newState == NETCTL_RAW_STATE_CONNECTED &&
			   (eventCode == 6 || prevState == NETCTL_RAW_STATE_WISPR_AUTH) &&
			   ch->ipAddrStr[0] != '\0') {
			sceClibPrintf("\n*** network ready. myaddr: %s ***\n", ch->ipAddrStr);
		}

		for (int i = 0; i < NETCTL_MAX_CALLBACKS; ++i) {
			if (table[i].callback != NULL) {
				int reportedErr = (eventCode == 9 || eventCode == 0) ? errCode : 0;

				table[i].callback(prevState, newState, eventCode,
						  reportedErr, 0, table[i].userArg);
			}
		}
		sceKernelUnlockLwMutex(&s_callbackMutex, 1);
	}
	return 0;
}

/* 0x8120FD60 */
int netCtlCoreNotifyStateChange(NetCtlChannel *ch, int prevState)
{
	return netCtlCoreNotifyCallbacks(ch, prevState, ch->state, ch->eventCode);
}

/* 0x8120FD6C */
static int netCtlParseWepKey(NetCtlChannel *ch, const char *keyStr)
{
	SceUInt8 parsed[64];
	SceSize len;
	SceSize outLen = 0;
	int ret;

	sceClibMemset(parsed, 0, sizeof(parsed));
	len = sceClibStrnlen(keyStr, 65);
	switch (len) {
	case 5:
	case 13:
		ret = netCtlParseAsciiKey(parsed, keyStr, len);
		if (ret < 0)
			return ret;
		outLen = len;
		break;
	case 10:
	case 26:
		ret = netCtlParseHexKey(parsed, keyStr, len);
		if (ret < 0)
			return ret;
		outLen = len / 2;
		break;
	default:
		return NETCTL_ERR_INVALID_KEY_LEN;
	}

	if (ch != NULL) {
		sceClibMemcpy_safe(ch->parsedKey, parsed, sizeof(ch->parsedKey));
		ch->parsedKeyLen = outLen;
	}
	return 0;
}

/* 0x8120FE1A */
static int netCtlParseWpaKey(NetCtlChannel *ch, const char *keyStr)
{
	SceUInt8 parsed[64];
	SceSize len;
	SceSize outLen;
	int isHex;
	int ret;

	sceClibMemset(parsed, 0, sizeof(parsed));
	len = sceClibStrnlen(keyStr, 65);
	if (len >= 8 && len <= 63) {
		ret = netCtlParseAsciiKey(parsed, keyStr, len);
		if (ret < 0)
			return ret;
		outLen = len;
		isHex = 0;
	} else if (len == 64) {
		ret = netCtlParseHexKey(parsed, keyStr, 64);
		if (ret < 0)
			return ret;
		outLen = 32;
		isHex = 1;
	} else {
		return NETCTL_ERR_INVALID_KEY_LEN;
	}

	if (ch != NULL) {
		sceClibMemcpy_safe(ch->parsedKey, parsed, sizeof(ch->parsedKey));
		ch->parsedKeyLen = outLen;
		ch->parsedKeyIsHex = isHex;
	}
	return ret;
}

/* 0x8120FEC0 */
int netCtlCoreParseActiveKey(NetCtlChannel *ch)
{
	if (ch->activeConfig.device != SCE_NET_CTL_DEVICE_WIRELESS) {
		if (ch->activeConfig.device > SCE_NET_CTL_DEVICE_PHONE)
			return NETCTL_ERR_INVALID_DEVICE;
		return 0;
	}

	switch (ch->activeConfig.wifiSecurity) {
	case SCE_NET_CTL_WIFI_SECURITY_NOAUTH:
		return 0;
	case SCE_NET_CTL_WIFI_SECURITY_WEP:
		return netCtlParseWepKey(ch, ch->activeConfig.securityKey);
	case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK:
	case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP:
	case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES:
	case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP:
	case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES:
		return netCtlParseWpaKey(ch, ch->activeConfig.securityKey);
	default:
		return NETCTL_ERR_INVALID_SECURITY;
	}
}

/* 0x81213318 */
static int netCtlParseStaticIpConfig(const NetCtlNetConfig *cfg, SceNetInAddr *outIp,
				     SceNetInAddr *outMask, SceNetInAddr *outRoute,
				     SceNetInAddr *outDnsMain, SceNetInAddr *outDnsAlt)
{
	if (outIp != NULL && sceNetInetPton(SCE_NET_AF_INET, cfg->ipAddress, outIp) <= 0)
		return NETCTL_ERR_INVALID_IP_ADDR;
	if (outMask != NULL && sceNetInetPton(SCE_NET_AF_INET, cfg->netmask, outMask) <= 0)
		return NETCTL_ERR_INVALID_IP_ADDR;
	if (outRoute != NULL) {
		sceClibMemset(outRoute, 0, sizeof(SceNetInAddr));
		sceNetInetPton(SCE_NET_AF_INET, cfg->defaultRoute, outRoute);
	}
	if (outDnsMain != NULL && outDnsAlt != NULL) {
		sceClibMemset(outDnsMain, 0, sizeof(SceNetInAddr));
		sceClibMemset(outDnsAlt, 0, sizeof(SceNetInAddr));
		sceNetInetPton(SCE_NET_AF_INET, cfg->primary_dns, outDnsMain);
		if (outDnsMain->s_addr != 0)
			sceNetInetPton(SCE_NET_AF_INET, cfg->secondary_dns, outDnsAlt);
	}
	return 0;
}

/* 0x8120FF26 */
int netCtlCoreValidateConfig(const NetCtlNetConfig *cfg)
{
	SceNetInAddr ip;
	SceNetInAddr mask;
	int ret = 0;

	if (cfg->device > SCE_NET_CTL_DEVICE_PHONE)
		return NETCTL_ERR_INVALID_DEVICE;

	if (cfg->device == SCE_NET_CTL_DEVICE_WIRELESS) {
		if (cfg->ssid[0] == '\0')
			return NETCTL_ERR_INVALID_SSID;

		switch (cfg->wifiSecurity) {
		case SCE_NET_CTL_WIFI_SECURITY_NOAUTH:
			break;
		case SCE_NET_CTL_WIFI_SECURITY_WEP:
			ret = netCtlParseWepKey(NULL, cfg->securityKey);
			break;
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK:
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP:
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES:
		case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP:
		case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES:
			ret = netCtlParseWpaKey(NULL, cfg->securityKey);
			break;
		default:
			ret = NETCTL_ERR_INVALID_SECURITY;
			break;
		}
		if (ret < 0)
			return ret;
	}

	if (cfg->mtu > 1500)
		return NETCTL_ERR_INVALID_MTU;

	if (cfg->ipConfig > SCE_NET_CTL_IP_PPPOE)
		return NETCTL_ERR_INVALID_IP_CONFIG;
	if (cfg->ipConfig == SCE_NET_CTL_IP_STATIC) {
		ret = netCtlParseStaticIpConfig(cfg, &ip, &mask, NULL, NULL, NULL);
		if (ret < 0)
			return ret;
	}

	if (cfg->dnsFlag >= 2u)
		return NETCTL_ERR_INVALID_DNS_FLAG;
	if (cfg->httpProxyFlag >= 2u)
		return NETCTL_ERR_INVALID_PROXY_FLAG;
	return ret;
}

/* 0x81210022 */
int netCtlCoreReleaseMode(NetCtlChannel *ch, int token)
{
	if (token == 0 || g_netCtlMutexReady == 0 ||
	    sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL) < 0)
		return 0;

	if (ch->isInet != 0) {
		if (ch->isInet == 1 && (s_wirelessMode == 1 || s_wirelessMode == 2)) {
			int nextMode = (token == 3 && s_wirelessMode == 2) ? 3 : 0;

			s_wirelessMode = nextMode;
			sceKernelSignalSema(s_modeSema, 1);
		}
	} else {
		if (token == 1) {
			s_wirelessMode = 1;
		} else if (s_wirelessMode == 4) {
			s_wirelessMode = 0;
			sceKernelSignalSema(s_modeSema, 1);
		}
	}

	if (g_netCtlMutexReady != 0)
		sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	return 0;
}

/* 0x812100D2 */
int netCtlCoreAcquireInetMode(NetCtlChannel *ch, int *outToken)
{
	int ret;

	(void)ch;
	*outToken = 0;
	ret = netCtlCoreLock();
	if (ret < 0)
		return ret;

	switch (s_wirelessMode) {
	case 0:
		ret = sceKernelPollSema(s_modeSema, 1);
		if (ret >= 0) {
			s_wirelessMode = 1;
			*outToken = 2;
		} else {
			ret = NETCTL_ERR_MODE_CONFLICT;
		}
		break;
	case 2:
	case 3:
	case 4:
		ret = NETCTL_ERR_MODE_CONFLICT;
		break;
	default:
		break;
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x812103A2 */
static int netCtlGetOrAllocProcessSlot(SceUID pid, NetCtlProcessSlot **outSlot)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId == pid) {
			*outSlot = &s_processSlots[i];
			netCtlCoreUnlock();
			return 0;
		}
	}

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId == -1) {
			s_processSlots[i].processId = pid;
			*outSlot = &s_processSlots[i];
			netCtlCoreUnlock();
			return 0;
		}
	}

	netCtlCoreUnlock();
	return NETCTL_ERR_TABLE_FULL;
}

/* 0x81211278 */
static int netCtlFindProcessSlot(SceUID pid, NetCtlProcessSlot **outSlot)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId == pid) {
			*outSlot = &s_processSlots[i];
			netCtlCoreUnlock();
			return 0;
		}
	}

	netCtlCoreUnlock();
	return NETCTL_ERR_INVALID_ID;
}

/* 0x8121017A */
int netCtlCoreInetConnect(unsigned int mode, int configId, const NetCtlNetConfig *cfg)
{
	NetCtlNetConfig loadedCfg;
	int exists = 0;
	int modeToken = 0;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = netCtlCoreAcquireInetMode(&g_netCtlInet, &modeToken);
	if (ret < 0) {
		netCtlCoreUnlock();
		return ret;
	}

	if ((g_netCtlInet.flags & NETCTL_FLAG_IDLE) == 0) {
		netCtlCoreReleaseMode(&g_netCtlInet, modeToken);
		netCtlCoreUnlock();
		return NETCTL_ERR_BUSY;
	}

	g_netCtlInet.flags &= ~NETCTL_FLAG_IDLE;
	netCtlCoreUnlock();

	const NetCtlNetConfig *useCfg = cfg;

	if (mode == 1) {
		ret = netCtlConfigLoadToNetConfig(0, (SceUInt32)configId, &loadedCfg, &exists);
		if (ret >= 0 && exists == 0)
			ret = NETCTL_ERR_CONFIG_NOT_SET;
		useCfg = &loadedCfg;
	}

	if (ret >= 0) {
		sceClibMemset(&g_netCtlInet.activeConfig, 0, sizeof(g_netCtlInet.activeConfig));
		if (mode == 1 || mode == 2) {
			ret = netCtlCoreValidateConfig(useCfg);
			if (ret >= 0) {
				sceClibMemcpy_safe(&g_netCtlInet.activeConfig, useCfg,
						   sizeof(g_netCtlInet.activeConfig));
				if (mode == 2) {
					g_netCtlInet.activeConfig.enableAutoConnect = 1;
					g_netCtlInet.activeConfig.configId = 0;
					g_netCtlInet.activeConfig.confSerialNo = 0;
					sceClibMemset(g_netCtlInet.activeConfig.reservedMeta, 0,
						      sizeof(g_netCtlInet.activeConfig.reservedMeta));
					if (g_netCtlInet.activeConfig.customIeProbeSize != 0 &&
					    g_netCtlInet.activeConfig.customIeProbe != NULL &&
					    g_netCtlInet.activeConfig.customIeProbeSize <= NETCTL_CUSTOM_IE_MAX) {
						sceClibMemcpy_safe(g_netCtlInet.connectCustomIeProbe,
								   g_netCtlInet.activeConfig.customIeProbe,
								   g_netCtlInet.activeConfig.customIeProbeSize);
						g_netCtlInet.activeConfig.customIeProbe =
							g_netCtlInet.connectCustomIeProbe;
					}
					if (g_netCtlInet.activeConfig.customIeAssocSize != 0 &&
					    g_netCtlInet.activeConfig.customIeAssoc != NULL &&
					    g_netCtlInet.activeConfig.customIeAssocSize <= NETCTL_CUSTOM_IE_MAX) {
						sceClibMemcpy_safe(g_netCtlInet.connectCustomIeAssoc,
								   g_netCtlInet.activeConfig.customIeAssoc,
								   g_netCtlInet.activeConfig.customIeAssocSize);
						g_netCtlInet.activeConfig.customIeAssoc =
							g_netCtlInet.connectCustomIeAssoc;
					}
				}
			}
		}
	}

	if (ret >= 0) {
		g_netCtlInet.connectMode = (int)mode;
		sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_CONNECT);
		return 0;
	}

	int lockRet = netCtlCoreLock();

	g_netCtlInet.flags |= NETCTL_FLAG_IDLE;
	if (lockRet >= 0)
		netCtlCoreUnlock();
	netCtlCoreReleaseMode(&g_netCtlInet, modeToken);
	return ret;
}

/* 0x8121045A */
int netCtlCoreInetConnectWithPid(unsigned int mode, int configId, const NetCtlNetConfig *cfg,
				 SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = netCtlCoreInetConnect(mode, configId, cfg);
	if (ret >= 0 && netCtlGetOrAllocProcessSlot(pid, &slot) >= 0 && slot != NULL)
		slot->disconnectOnExit = 1;
	netCtlCoreUnlock();
	return ret;
}

/* 0x812104FA */
int netCtlCoreInetAutoConnect(SceUID requestPid)
{
	int modeToken = 0;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = netCtlCoreAcquireInetMode(&g_netCtlInet, &modeToken);
	if (ret < 0) {
		netCtlCoreUnlock();
		return ret;
	}

	if ((g_netCtlInet.flags & NETCTL_FLAG_IDLE) != 0) {
		g_netCtlInet.flags &= ~NETCTL_FLAG_IDLE;
		netCtlCoreUnlock();
		sceClibMemset(&g_netCtlInet.activeConfig, 0, sizeof(g_netCtlInet.activeConfig));
		g_netCtlInet.exclusiveRequestPid = requestPid;
		sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_AUTO_CONNECT);
		return 0;
	}

	SceNetInternal_4F5E0E8A(-1, 0, 0);
	netCtlCoreReleaseMode(&g_netCtlInet, modeToken);
	netCtlCoreUnlock();
	return NETCTL_ERR_BUSY;
}

/* 0x81210604 */
static int netCtlAcquireAdhocMode(NetCtlChannel *ch, int *outToken)
{
	int ret;

	(void)ch;
	*outToken = 0;
	ret = netCtlCoreLock();
	if (ret < 0)
		return ret;

	switch (s_wirelessMode) {
	case 0:
		ret = sceKernelPollSema(s_modeSema, 1);
		if (ret >= 0) {
			*outToken = 2;
			s_wirelessMode = 4;
		}
		break;
	case 1:
		s_wirelessMode = 2;
		sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_ADHOC_PREEMPT);
		*outToken = 1;
		break;
	default:
		break;
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x812106B8 */
int netCtlCoreAdhocConnect(unsigned int mode, SceUID ownerPid, const char *ssid, int timeout)
{
	int modeToken = 0;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = netCtlAcquireAdhocMode(&g_netCtlAdhoc, &modeToken);
	if (ret < 0) {
		netCtlCoreUnlock();
		return ret;
	}

	if (mode >= 4 && mode <= 6) {
		sceClibMemset(g_netCtlAdhoc.adhocSsid, 0, sizeof(g_netCtlAdhoc.adhocSsid));
		sceClibStrlcpy(g_netCtlAdhoc.adhocSsid, ssid, sizeof(g_netCtlAdhoc.adhocSsid));
		g_netCtlAdhoc.adhocJoinTimeout = timeout;
	} else if (mode == 3) {
		sceClibMemset(g_netCtlAdhoc.adhocSsid, 0, sizeof(g_netCtlAdhoc.adhocSsid));
		sceClibStrlcpy(g_netCtlAdhoc.adhocSsid, ssid, sizeof(g_netCtlAdhoc.adhocSsid));
	}

	if (netCtlConfigGetAdhocChannel(&g_netCtlAdhoc.adhocChannel) < 0)
		g_netCtlAdhoc.adhocChannel = 11;

	if ((g_netCtlAdhoc.flags & NETCTL_FLAG_IDLE) != 0) {
		g_netCtlAdhoc.adhocOwnerPid = ownerPid;
		g_netCtlAdhoc.flags &= ~NETCTL_FLAG_IDLE;
		netCtlCoreUnlock();
		g_netCtlAdhoc.connectMode = (int)mode;
		sceKernelSetEventFlag(g_netCtlAdhoc.eventFlagId, NETCTL_EV_CONNECT);
		return 0;
	}

	netCtlCoreReleaseMode(&g_netCtlAdhoc, modeToken);
	netCtlCoreUnlock();
	return NETCTL_ERR_BUSY;
}

/* 0x8121081E */
int netCtlCoreInetDisconnect(void)
{
	sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_DISCONNECT);
	return 0;
}

/* 0x81210834 */
int netCtlCoreAdhocDisconnect(void)
{
	sceKernelSetEventFlag(g_netCtlAdhoc.eventFlagId, NETCTL_EV_DISCONNECT);
	return 0;
}

/* 0x8121084C */
int netCtlCoreStartScan(const char **ssids, SceUInt32 count, const NetCtlScanParam *param)
{
	if (count > NETCTL_MAX_SCAN_SSIDS)
		return NETCTL_ERR_INVALID_SSID_COUNT;

	for (SceUInt32 i = 0; i < count; ++i) {
		SceSize len = sceClibStrnlen(ssids[i], 32);

		if (len == 0 || len >= 33)
			return NETCTL_ERR_INVALID_SSID;
	}

	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	if (g_netCtlInet.scanInProgress == 1)
		ret = NETCTL_ERR_BUSY;
	else
		g_netCtlInet.scanInProgress = 1;
	netCtlCoreUnlock();

	if (ret >= 0) {
		sceClibMemset(g_netCtlInet.scanSsids, 0, sizeof(g_netCtlInet.scanSsids));
		for (SceUInt32 i = 0; i < count; ++i)
			sceClibMemcpy_safe(g_netCtlInet.scanSsids[i], ssids[i], 33);
		g_netCtlInet.scanSsidCount = count;

		if (param != NULL) {
			sceClibMemcpy_safe(&g_netCtlInet.scanParam, param, sizeof(NetCtlScanParam));
			if ((param->optionFlags & 2u) != 0 &&
			    param->customIeSize != 0 &&
			    param->customIeData != NULL &&
			    param->customIeSize <= NETCTL_CUSTOM_IE_MAX) {
				sceClibMemcpy_safe(g_netCtlInet.scanCustomIe, param->customIeData,
						   param->customIeSize);
				g_netCtlInet.scanParam.customIeData = g_netCtlInet.scanCustomIe;
			}
		}
		sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_SCAN);
	}
	return ret;
}

/* 0x8121097E */
int netCtlCoreGetScanResultCount(void)
{
	return (int)g_netCtlInet.bcastScanCount;
}

/* 0x8121098E */
int netCtlCoreGetScanResultByIndex(SceUInt32 index, NetCtlScanInfo *outInfo)
{
	if (index >= g_netCtlInet.bcastScanCount)
		return NETCTL_ERR_SCAN_INDEX_OOB;
	return netCtlGetScanInfoByIndex(&g_netCtlInet, index, outInfo, NULL, NULL,
					g_netCtlInet.bcastScanList, g_netCtlInet.bcastScanBusy,
					0, g_netCtlInet.bcastScanSysTime,
					g_netCtlInet.bcastScanRtcTime);
}

/* 0x812109F4 */
int netCtlCoreGetBcastScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount)
{
	return netCtlGetScanBssidList(&g_netCtlInet, outList, maxCount,
				      g_netCtlInet.bcastScanList, g_netCtlInet.bcastScanCount,
				      g_netCtlInet.bcastScanBusy);
}

/* 0x81210A20 */
int netCtlCoreGetBcastScanInfoByBssid(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
				      void *outIe, SceSize *ioIeSize)
{
	return netCtlGetScanInfoByBssid(&g_netCtlInet, bssid, outInfo, outIe, ioIeSize,
					g_netCtlInet.bcastScanList, g_netCtlInet.bcastScanCount,
					g_netCtlInet.bcastScanBusy, g_netCtlInet.bcastScanSysTime,
					g_netCtlInet.bcastScanRtcTime);
}

/* 0x81210A7A */
int netCtlCoreGetSingleScanInfo(NetCtlScanInfo *outInfo, void *outIe, SceSize *ioIeSize)
{
	return netCtlGetSingleSsidScanInfo(&g_netCtlInet, outInfo, outIe, ioIeSize);
}

/* 0x81210A90 */
int netCtlCoreGetMultiScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount)
{
	return netCtlGetScanBssidList(&g_netCtlInet, outList, maxCount,
				      g_netCtlInet.multiScanList, g_netCtlInet.multiScanCount,
				      g_netCtlInet.multiScanBusy);
}

/* 0x81210AB6 */
int netCtlCoreGetMultiScanInfoByBssid(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
				      void *outIe, SceSize *ioIeSize)
{
	return netCtlGetScanInfoByBssid(&g_netCtlInet, bssid, outInfo, outIe, ioIeSize,
					g_netCtlInet.multiScanList, g_netCtlInet.multiScanCount,
					g_netCtlInet.multiScanBusy, g_netCtlInet.multiScanSysTime,
					g_netCtlInet.multiScanRtcTime);
}

/* 0x81210B14 */
int netCtlCoreGetWirelessMode(int *outMode)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	switch (s_wirelessMode) {
	case 0:
		*outMode = 1;
		break;
	case 1:
		*outMode = 2;
		break;
	case 2:
	case 3:
	case 4:
		*outMode = 3;
		break;
	default:
		break;
	}
	netCtlCoreUnlock();
	return ret;
}

/* 0x81210B8A */
int netCtlCoreInetGetRawState(int *outState)
{
	*outState = g_netCtlInet.state;
	return 0;
}

/* 0x81210B9A */
int netCtlCoreInetGetInfo(int code, SceNetCtlInfo *outInfo)
{
	return netCtlInetGetInfoImpl(&g_netCtlInet, g_netCtlInet.state, code, outInfo);
}

/* 0x81210BB0 */
int netCtlCoreAdhocGetRawState(int *outState)
{
	*outState = g_netCtlAdhoc.state;
	return 0;
}

/* 0x81210BC0 */
int netCtlCoreAdhocGetInfo(int code, void *outInfo)
{
	return netCtlAdhocGetInfoImpl(&g_netCtlAdhoc, g_netCtlAdhoc.state, code, outInfo);
}

static int netCtlRawStateToSdkState(int rawState)
{
	switch (rawState) {
	case NETCTL_RAW_STATE_IDLE:
		return SCE_NET_CTL_STATE_DISCONNECTED;
	case NETCTL_RAW_STATE_AUTO_SELECT:
	case NETCTL_RAW_STATE_LINK_UP:
		return SCE_NET_CTL_STATE_CONNECTING;
	case NETCTL_RAW_STATE_IP_CONFIG:
	case NETCTL_RAW_STATE_WISPR_AUTH:
		return SCE_NET_CTL_STATE_IPOBTAINING;
	case NETCTL_RAW_STATE_CONNECTED:
		return SCE_NET_CTL_STATE_IPOBTAINED;
	default:
		return SCE_NET_CTL_STATE_DISCONNECTED;
	}
}

/* 0x81210BD6 */
int netCtlCoreInetGetState(int *outState)
{
	*outState = netCtlRawStateToSdkState(g_netCtlInet.state);
	return 0;
}

/* 0x81210C0C */
int netCtlCoreAdhocGetState(int *outState)
{
	*outState = netCtlRawStateToSdkState(g_netCtlAdhoc.state);
	return 0;
}

/* 0x81210C42 */
int netCtlCoreGetMobileStatus(int *outStatus)
{
	if (g_netCtlInet.fakeMobileIf > 0) {
		outStatus[0] = 20 * 1024 * 1024;
		outStatus[1] = 0;
		return 0;
	}
	if (g_netCtlInet.hostFuncs != NULL &&
	    g_netCtlInet.hostFuncs->getMobileNetworkStatus != NULL)
		return g_netCtlInet.hostFuncs->getMobileNetworkStatus(outStatus);
	return 0;
}

/* 0x81210C7A */
unsigned int netCtlCoreAcquireProcessRef(SceUID pid)
{
	int allowed = 1;
	int isFront = 1;
	NetCtlProcessSlot *slot = NULL;
	unsigned int bits = 0;

	if (g_netCtlInet.hostFuncs == NULL || g_netCtlInet.hostFuncs->getProcessStatus == NULL)
		return 0;

	g_netCtlInet.hostFuncs->getProcessStatus(pid, &allowed, &isFront);

	int ret = netCtlCoreLock();

	if (ret < 0)
		return (unsigned int)ret;

	unsigned int token = 0;

	if (netCtlGetOrAllocProcessSlot(pid, &slot) >= 0 && slot != NULL) {
		SceUInt16 cur = s_nextProcessToken;
		SceUInt16 next = (SceUInt16)(cur + 1);

		if (cur == 0) {
			cur = 1;
			next = 2;
		}
		s_nextProcessToken = next;
		token = cur;
		slot->refToken = cur;
	}

	if (allowed == 1) {
		bits |= 1u;
		if (slot != NULL)
			++slot->allowedRefCount;
	}
	if (isFront == 1) {
		bits |= 2u;
		if (slot != NULL)
			++slot->frontRefCount;
	}

	unsigned int result = (token & 0xFFFCFFFFu) | ((bits & 3u) << 16);

	netCtlCoreUnlock();
	return result;
}

/* 0x81210D66 */
int netCtlCoreReleaseProcessRef(unsigned int token)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	SceUInt16 id = (SceUInt16)token;

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId != -1 && s_processSlots[i].refToken == id) {
			if ((token & 0x10000u) != 0)
				--s_processSlots[i].allowedRefCount;
			if ((token & 0x20000u) != 0)
				--s_processSlots[i].frontRefCount;
			break;
		}
	}
	netCtlCoreUnlock();
	return ret;
}

/* 0x81210E1A */
int netCtlCoreGetAllowedRefCount(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	int total = 0;

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId != -1)
			total += s_processSlots[i].allowedRefCount;
	}
	netCtlCoreUnlock();
	return total;
}

/* 0x81210E8E */
int netCtlCoreGetFrontRefCount(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	int total = 0;

	for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
		if (s_processSlots[i].processId != -1)
			total += s_processSlots[i].frontRefCount;
	}
	netCtlCoreUnlock();
	return total;
}

/* 0x81210F02 */
int netCtlCoreIncActiveRef(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	++g_netCtlInet.activeProcessRefCount;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81210F68 */
int netCtlCoreDecActiveRef(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	--g_netCtlInet.activeProcessRefCount;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81210FCE */
int netCtlCoreGetActiveRef(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	int count = g_netCtlInet.activeProcessRefCount;

	netCtlCoreUnlock();
	return count;
}

/* 0x8121102A */
int netCtlCoreApplyPowerSave(NetCtlChannel *ch, int ifId)
{
	int powerSave = 1;
	NetCtlSleepParam activeParam;
	NetCtlSleepParam otherParam;
	int ifIds[10];

	netCtlConfigGetWifiPowerSave(&powerSave);

	sceClibMemset(&activeParam, 0, sizeof(activeParam));
	activeParam.handle = -1;
	if ((ch == NULL || ch->fakeMobileIf == 0) &&
	    (vshKernelCheckModelCapability(4) != 0 ||
	     (powerSave == 1 && (ifId == NETCTL_IF_WLAN || ifId == NETCTL_IF_ETHERNET))))
		activeParam.timeoutSec = NETCTL_SLEEP_TIMEOUT_INFINITE;
	else
		activeParam.timeoutSec = NETCTL_SLEEP_TIMEOUT_SHORT;
	activeParam.enable = 1;
	SceNetInternal_E03F6A77(ifId, &activeParam, 0);

	sceClibMemset(&otherParam, 0, sizeof(otherParam));
	otherParam.enable = 0;

	int count = SceNetInternal_8157DE3E(ifIds, 10, 0);

	if (count >= 0) {
		for (int i = 0; i < count; ++i) {
			if (ifIds[i] != ifId)
				SceNetInternal_E03F6A77(ifIds[i], &otherParam, 0);
		}
		if (powerSave == 1)
			ch->flags |= (NETCTL_FLAG_POWERSAVE_ACTIVE | NETCTL_FLAG_POWERSAVE_ON);
		else
			ch->flags = (ch->flags & ~NETCTL_FLAG_POWERSAVE_ON) | NETCTL_FLAG_POWERSAVE_ACTIVE;
	}
	return count;
}

/* 0x81211110 */
int netCtlCoreFreeze(SceUID pid)
{
	SceShell_3F41E440();
	SceShell_C1886828();

	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	s_frozenPid = pid;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81211170 */
int netCtlCoreUnfreeze(void)
{
	int cleanRet = sceNetConfigCleanUpAllInterfaces();

	if (cleanRet < 0)
		sceClibPrintf("%s: sceNetConfigCleanUpAllInterfaces() failed. ret = 0x%x\n",
			      "_sce_net_ctl_unfreeze_libnetctl", cleanRet);

	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	s_frozenPid = -1;
	netCtlCoreUnlock();
	return ret;
}

/* 0x812111E8 */
SceUID netCtlCoreGetFrozenPid(void)
{
	return s_frozenPid;
}

/* 0x812111F4 */
int netCtlCoreSuppressAutoConnect(SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (netCtlGetOrAllocProcessSlot(pid, &slot) >= 0 && slot != NULL)
		slot->autoConnectSuppressed = 1;
	netCtlCoreUnlock();
	return ret;
}

/* 0x812112FE */
int netCtlCoreResumeAutoConnect(SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (netCtlFindProcessSlot(pid, &slot) >= 0 && slot != NULL)
		slot->autoConnectSuppressed = 0;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81211382 */
int netCtlCoreForceImposePrompt(SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (netCtlGetOrAllocProcessSlot(pid, &slot) >= 0 && slot != NULL)
		slot->imposePromptForced = 1;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81211406 */
int netCtlCoreClearImposePrompt(SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (netCtlFindProcessSlot(pid, &slot) >= 0 && slot != NULL)
		slot->imposePromptForced = 0;
	netCtlCoreUnlock();
	return ret;
}

/* 0x8121148A */
int netCtlCoreIsAutoConnectSuppressed(SceUID pid)
{
	NetCtlProcessSlot *slot = NULL;
	int suppressed = 0;

	if (netCtlCoreLock() >= 0) {
		if (netCtlFindProcessSlot(pid, &slot) >= 0 && slot != NULL)
			suppressed = slot->autoConnectSuppressed;
		netCtlCoreUnlock();
	}
	return suppressed;
}

/* 0x8121150E */
int netCtlCoreHasForcedImposePrompt(void)
{
	int forced = 0;

	if (netCtlCoreLock() >= 0) {
		for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
			if (s_processSlots[i].processId != -1 &&
			    s_processSlots[i].imposePromptForced == 1) {
				forced = 1;
				break;
			}
		}
		netCtlCoreUnlock();
	}
	return forced;
}

/* 0x81211D46 */
int netCtlCoreAnyAutoConnectSuppressed(void)
{
	int suppressed = 0;

	if (netCtlCoreLock() >= 0) {
		for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
			if (s_processSlots[i].processId != -1 &&
			    s_processSlots[i].autoConnectSuppressed == 1) {
				suppressed = 1;
				break;
			}
		}
		netCtlCoreUnlock();
	}
	return suppressed;
}

/* 0x81211580 */
int netCtlCoreResetExclusivePrompt(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (g_netCtlInet.state == NETCTL_RAW_STATE_IDLE)
		g_netCtlInet.exclusiveResetFlag = 1;
	netCtlCoreUnlock();
	return ret;
}

/* 0x812115E4 */
int netCtlCoreGetWisprStatus(NetCtlWisprStatus *outStatus)
{
	sceClibMemcpy_safe(outStatus, &s_wisprStatus, sizeof(NetCtlWisprStatus));
	return 0;
}

/* 0x812115FA */
int netCtlCoreSetWisprStatus(const NetCtlWisprStatus *status)
{
	sceClibMemcpy_safe(&s_wisprStatus, status, sizeof(NetCtlWisprStatus));
	return 0;
}

/* 0x81211612 */
int netCtlCoreAbortExclusive(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if ((g_netCtlInet.flags & NETCTL_FLAG_EXCLUSIVE_ACTIVE) != 0) {
		sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_EXCLUSIVE_ABORT);
		g_netCtlInet.exclusiveResult.clientPid = -1;
	}
	netCtlCoreUnlock();
	return ret;
}

static SceNetCtlIfStat *netCtlSelectIfStat(int device)
{
	switch (device) {
	case SCE_NET_CTL_DEVICE_WIRELESS:
		return &g_netCtlInet.ifStatWifi;
	case SCE_NET_CTL_DEVICE_WIRED:
		return &g_netCtlInet.ifStatEth;
	case SCE_NET_CTL_DEVICE_PHONE:
		if (g_netCtlInet.fakeMobileIf <= 0)
			return &g_netCtlInet.ifStatPhone;
		if ((g_netCtlInet.flags & NETCTL_FLAG_ETH_IFSTAT_DIRTY) != 0)
			return &g_netCtlInet.ifStatEth;
		return &g_netCtlInet.ifStatWifi;
	default:
		return NULL;
	}
}

/* 0x8121168E */
int netCtlCoreGetIfStat(int device, SceNetCtlIfStat *outStat)
{
	SceNetCtlIfStat *src = netCtlSelectIfStat(device);

	if (src == NULL)
		return NETCTL_ERR_INVALID_DEVICE;
	sceClibMemcpy_safe(outStat, src, sizeof(SceNetCtlIfStat));
	return 0;
}

/* 0x8121172E */
int netCtlCoreClearIfStat(int device)
{
	SceNetCtlIfStat *dst = netCtlSelectIfStat(device);

	if (dst == NULL)
		return NETCTL_ERR_INVALID_DEVICE;

	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	sceClibMemset(dst, 0, sizeof(SceNetCtlIfStat));
	sceRtcGetCurrentTick(&dst->resetTick);
	g_netCtlInet.flags = (g_netCtlInet.flags & ~(NETCTL_FLAG_IFSTAT_SAVED |
						     NETCTL_FLAG_IFSTAT_DIRTY |
						     NETCTL_FLAG_FLUSH_IFSTAT_NOW)) |
			     NETCTL_FLAG_IFSTAT_DIRTY | NETCTL_FLAG_FLUSH_IFSTAT_NOW;
	netCtlCoreUnlock();
	return ret;
}

/* 0x81211850 */
int netCtlCoreLinkProcess(SceUID childPid, SceUID parentPid)
{
	SceKernelSysClock clock;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	if (sceKernelGetRemoteProcessTimeForUser(parentPid, &clock) < 0 ||
	    sceKernelGetRemoteProcessTimeForUser(childPid, &clock) < 0) {
		netCtlCoreUnlock();
		return NETCTL_ERR_INVALID_ID;
	}

	int slotIdx = -1;
	int newlyAlloc = 0;

	for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
		if (s_processLinks[i].targetProcessId == parentPid) {
			slotIdx = i;
			break;
		}
	}

	if (slotIdx < 0) {
		for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
			if (s_processLinks[i].targetProcessId < 0) {
				slotIdx = i;
				break;
			}
		}
		if (slotIdx < 0) {
			netCtlCoreUnlock();
			return NETCTL_ERR_TABLE_FULL;
		}
		s_processLinks[slotIdx].targetProcessId = parentPid;
		for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j)
			s_processLinks[slotIdx].linkedProcesses[j] = -1;
		newlyAlloc = 1;
	}

	for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
		if (s_processLinks[slotIdx].linkedProcesses[j] == childPid) {
			if (newlyAlloc)
				s_processLinks[slotIdx].targetProcessId = -1;
			netCtlCoreUnlock();
			return NETCTL_ERR_ALREADY_LINKED;
		}
	}

	for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
		if (s_processLinks[slotIdx].linkedProcesses[j] < 0) {
			s_processLinks[slotIdx].linkedProcesses[j] = childPid;
			netCtlCoreUnlock();
			return 0;
		}
	}

	if (newlyAlloc)
		s_processLinks[slotIdx].targetProcessId = -1;
	netCtlCoreUnlock();
	return NETCTL_ERR_TABLE_FULL;
}

/* 0x812119D4 */
int netCtlCoreUnlinkProcess(SceUID childPid, SceUID parentPid)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	int slotIdx = -1;

	for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
		if (s_processLinks[i].targetProcessId == parentPid) {
			slotIdx = i;
			break;
		}
	}

	if (slotIdx < 0) {
		netCtlCoreUnlock();
		return NETCTL_ERR_INVALID_ID;
	}

	int found = 0;

	for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
		if (s_processLinks[slotIdx].linkedProcesses[j] == childPid) {
			s_processLinks[slotIdx].linkedProcesses[j] = -1;
			found = 1;
			break;
		}
	}

	if (!found) {
		netCtlCoreUnlock();
		return NETCTL_ERR_INVALID_ID;
	}

	int anyLeft = 0;

	for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
		if (s_processLinks[slotIdx].linkedProcesses[j] >= 0) {
			anyLeft = 1;
			break;
		}
	}
	if (!anyLeft)
		s_processLinks[slotIdx].targetProcessId = -1;

	netCtlCoreUnlock();
	return 0;
}

/* 0x81211AAA */
int netCtlCoreIsProcessLinked(SceUID childPid, SceUID parentPid)
{
	int linked = 0;

	if (netCtlCoreLock() >= 0) {
		for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
			if (s_processLinks[i].targetProcessId == childPid) {
				for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
					if (s_processLinks[i].linkedProcesses[j] == parentPid) {
						linked = 1;
						break;
					}
				}
				break;
			}
		}
		netCtlCoreUnlock();
	}
	return linked;
}

/* 0x81211B36 */
int netCtlCoreIsAnyProcessAllowed(const SceUID *pids, SceUInt32 count)
{
	if (g_netCtlInet.hostFuncs != NULL &&
	    g_netCtlInet.hostFuncs->isAnyProcessAllowed != NULL)
		return g_netCtlInet.hostFuncs->isAnyProcessAllowed(pids, count);
	return 0;
}

/* 0x81211B58 */
int netCtlCoreOnProcessEnded(SceUID pid)
{
	int mode = 0;

	if (pid == -1)
		return 0;

	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = SceShell_36788D70(&mode);
	if (ret >= 0) {
		if (mode == 3 && pid == g_netCtlAdhoc.adhocOwnerPid)
			sceKernelSetEventFlag(g_netCtlAdhoc.eventFlagId, NETCTL_EV_DISCONNECT);

		if ((g_netCtlInet.flags & NETCTL_FLAG_EXCLUSIVE_ACTIVE) != 0 &&
		    g_netCtlInet.exclusiveResult.clientPid == pid) {
			sceKernelSetEventFlag(g_netCtlInet.eventFlagId, NETCTL_EV_EXCLUSIVE_ABORT);
			g_netCtlInet.exclusiveResult.clientPid = -1;
		}

		if (netCtlCoreLock() >= 0) {
			for (int i = 0; i < NETCTL_MAX_PROCESS_LINKS; ++i) {
				for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j) {
					if (s_processLinks[i].linkedProcesses[j] == pid)
						s_processLinks[i].linkedProcesses[j] = -1;
				}
				if (s_processLinks[i].targetProcessId == pid) {
					s_processLinks[i].targetProcessId = -1;
					for (int j = 0; j < NETCTL_MAX_PROCESS_LINKS; ++j)
						s_processLinks[i].linkedProcesses[j] = -1;
				}
			}
			netCtlCoreUnlock();
		}

		for (int i = 0; i < NETCTL_MAX_PROCESS_SLOTS; ++i) {
			if (s_processSlots[i].processId == pid) {
				if (s_processSlots[i].disconnectOnExit == 1)
					SceShell_3F41E440();
				sceClibMemset(&s_processSlots[i], 0, sizeof(NetCtlProcessSlot));
				s_processSlots[i].processId = -1;
			}
		}

		if (pid == s_frozenPid)
			netCtlCoreUnfreeze();
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x81211DB8 */
int netCtlCoreStartExclusiveDialog(NetCtlChannel *ch, unsigned int reason, SceUID pid,
				   int forceShow, int *shownFlag)
{
	int dialogId = (reason == 1 || reason == 2) ? 1 : 0;

	if (forceShow == 0 && ch->hostFuncs != NULL &&
	    ch->hostFuncs->hasNetworkThread != NULL &&
	    ch->hostFuncs->hasNetworkThread(pid) == 0)
		return 0;

	if (reason == 3) {
		int imposeScan = 0;

		if (netCtlConfigGetImposeScanListEnabled(&imposeScan) >= 0 && imposeScan == 0)
			return 0;
	}

	sceKernelCheckCallback();

	if (netCtlCoreLock() >= 0) {
		if (ch->exclusiveResetFlag == 1) {
			ch->exclusiveResetFlag = 0;
			ch->exclusiveImposeShown = 0;
		}
		netCtlCoreUnlock();
	}

	if (forceShow == 0 && netCtlCoreHasForcedImposePrompt() == 0 && *shownFlag == 1)
		return 0;

	if (ch->hostFuncs != NULL && ch->hostFuncs->exclusiveBegin != NULL) {
		sceClibMemset(&ch->exclusiveResult, 0, sizeof(ch->exclusiveResult));
		sceKernelClearEventFlag(ch->eventFlagId,
					~(NETCTL_EV_EXCLUSIVE_DONE | NETCTL_EV_EXCLUSIVE_ABORT));
		ch->exclusiveRunning = 1;
		if (ch->hostFuncs->exclusiveBegin(dialogId, (void *)netCtlExclusiveCallback, ch) >= 0) {
			*shownFlag = 1;
			return 1;
		}
		ch->exclusiveRunning = 0;
	}
	return 0;
}

/* 0x81211EDE */
int netCtlCoreNotifyAutoConnectResult(NetCtlChannel *ch, int errCode)
{
	if (ch->exclusiveRunning == 0) {
		if (errCode == SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED)
			errCode = ch->lastError;
		SceNetInternal_26EFF46D(-1, errCode,
					(ch->flags & NETCTL_FLAG_SUSPEND_DISCONN) != 0);
	}
	return 0;
}

/* 0x81211F12 */
int netCtlCoreWaitForAdhocMode(void)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	switch (s_wirelessMode) {
	case 0:
	case 2:
	case 3:
		netCtlCoreUnlock();
		ret = sceKernelWaitSema(s_modeSema, 1, NULL);
		if (ret >= 0) {
			ret = netCtlCoreLock();
			if (ret >= 0) {
				s_wirelessMode = 4;
				netCtlCoreUnlock();
			} else {
				s_wirelessMode = 0;
				sceKernelSignalSema(s_modeSema, 1);
			}
		}
		return ret;
	case 1:
		netCtlCoreUnlock();
		return NETCTL_ERR_INET_IN_USE;
	default:
		netCtlCoreUnlock();
		return ret;
	}
}

/* 0x81212004 */
int netCtlCoreClaimActiveOwner(NetCtlChannel *ch)
{
	int claimed = 0;

	if (netCtlCoreLock() >= 0) {
		if (s_activeOwner == 0 || (unsigned int)s_activeOwner > 2) {
			if (ch->isInet == 1 && s_wirelessMode == 1) {
				s_activeOwner = 1;
				claimed = 1;
			} else if (ch->isInet == 0 && s_wirelessMode == 4) {
				s_activeOwner = 2;
				claimed = 1;
			}
		}
		netCtlCoreUnlock();
	}
	return claimed;
}

/* 0x8121209A */
static void netCtlHandleAutoConnectEvent(int ifId, SceUInt32 evLow, int *outKeepEvent,
					 NetCtlEventFilterCtx *ctx)
{
	NetCtlAutoConnectIfInfo ifInfo;
	NetCtlChannel *ch = ctx->channel;

	*outKeepEvent = 0;
	if (ch->isInet == 0 || SceNetInternal_01B090F1(ifId, &ifInfo, 0) < 0)
		return;

	if ((evLow & 0x08000000u) == 0 && ifInfo.isSpecial == 0) {
		int icmType = 1;

		netCtlConfigGetIcmType(&icmType);
		if (icmType != 0 && (ctx->waitMask & NETCTL_EV_AUTO_DISCONNECT) != 0)
			*ctx->outBits |= NETCTL_EV_AUTO_DISCONNECT;
		return;
	}

	if ((ctx->waitMask & NETCTL_EV_AUTO_CONNECT) == 0) {
		*outKeepEvent = 1;
		return;
	}

	if (netCtlCoreLock() < 0) {
		if (ch->exclusiveRunning == 0)
			SceNetInternal_26EFF46D(-1, SCE_NET_CTL_ERROR_AUTO_CONNECT_DISABLED,
						(ch->flags & NETCTL_FLAG_SUSPEND_DISCONN) != 0);
		return;
	}

	if ((ch->flags & NETCTL_FLAG_IDLE) != 0) {
		ch->flags &= ~NETCTL_FLAG_IDLE;
		netCtlCoreUnlock();

		if (ch->autoConnectDisabled != 1) {
			int token = 0;

			netCtlCoreAcquireInetMode(ch, &token);
			sceClibMemset(&ch->activeConfig, 0, sizeof(ch->activeConfig));
			*ctx->outBits |= NETCTL_EV_AUTO_CONNECT;
			return;
		}

		if (ch->exclusiveRunning == 0)
			SceNetInternal_26EFF46D(-1, SCE_NET_CTL_ERROR_AUTO_CONNECT_DISABLED,
						(ch->flags & NETCTL_FLAG_SUSPEND_DISCONN) != 0);
		int lockRet = netCtlCoreLock();

		ch->flags |= NETCTL_FLAG_IDLE;
		if (lockRet >= 0)
			netCtlCoreUnlock();
		return;
	}

	int mode = 0;

	if (netCtlCoreGetWirelessMode(&mode) >= 0 && mode == 2 &&
	    ch->state == NETCTL_RAW_STATE_CONNECTED &&
	    (ch->flags & NETCTL_FLAG_SUSPEND_DISCONN) == 0) {
		netCtlCoreUnlock();
		*ctx->outBits |= NETCTL_EV_AUTO_CONNECT;
		return;
	}

	netCtlCoreUnlock();
	*outKeepEvent = 1;
}

/* 0x812122E2 */
unsigned int netCtlCoreNetEventFilter(int ifId, int flags, SceUInt32 evLow, SceUInt32 evHigh,
				      int unused, void *filterCtx)
{
	NetCtlEventFilterCtx *ctx = (NetCtlEventFilterCtx *)filterCtx;
	NetCtlChannel *ch = ctx->channel;

	(void)flags;
	(void)evHigh;
	(void)unused;

	if (ctx->mode == 1) {
		SceUInt32 masked = evLow & 0x19FFFFFFu;

		if (ifId == NETCTL_IF_ETHERNET)
			return masked & 0x1FFFFFC0u;
		if (ifId == NETCTL_IF_WLAN)
			return evLow & 0x19CFFFF0u;
		if (ifId == NETCTL_IF_PHONE)
			return evLow & 0x19EFFFF8u;
		if (ifId == -1)
			abort();
		return masked;
	}
	if (ctx->mode == 2)
		return (ctx->targetIfId == ifId) ? (evLow & 0xDBFFFFFFu) : evLow;
	if (ctx->mode == 3)
		return (ctx->targetIfId == ifId) ? (evLow & 0xBBFFFFFFu) : evLow;

	SceUInt32 keepMask = 0;
	SceUInt32 clearMask = 0;

	if (ctx->isScan == 0) {
		if (ifId == -1) {
			if ((evLow & 0x20u) != 0 || (evLow & 0x40u) != 0) {
				if (ch->hostFuncs != NULL &&
				    ch->hostFuncs->notifyComPrevented != NULL)
					ch->hostFuncs->notifyComPrevented((evLow & 0x20u) != 0,
									  (evLow & 0x40u) != 0);
			}
			if ((ctx->waitMask & NETCTL_EV_LINK_ERROR) != 0 && (evLow & 0x08u) != 0) {
				ch->flags |= NETCTL_FLAG_SUSPEND_DISCONN;
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_SUSPEND;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			}
		}

		if ((evLow & 0x0B000000u) != 0) {
			int keepAuto = 0;

			netCtlHandleAutoConnectEvent(ifId, evLow, &keepAuto, ctx);
			if (keepAuto == 1)
				keepMask = evLow & 0x09000000u;
		}

		if ((evLow & 0x10000000u) != 0 && ch->isInet != 0) {
			NetCtlIfStatDelta delta;

			if (SceNetInternal_EE94BA15(NETCTL_IF_WLAN, &delta, 1) >= 0) {
				if ((delta.activeSec | delta.txBytes | delta.rxBytes) != 0) {
					ch->flags |= NETCTL_FLAG_IFSTAT_DIRTY;
					ch->ifStatWifi.totalSec += delta.activeSec;
					ch->ifStatWifi.txBytes += delta.txBytes;
					ch->ifStatWifi.rxBytes += delta.rxBytes;
				}
				if (SceNetInternal_EE94BA15(NETCTL_IF_ETHERNET, &delta, 1) >= 0) {
					if ((delta.activeSec | delta.txBytes | delta.rxBytes) != 0) {
						ch->flags |= (NETCTL_FLAG_IFSTAT_DIRTY |
							      NETCTL_FLAG_ETH_IFSTAT_DIRTY);
						ch->ifStatEth.totalSec += delta.activeSec;
						ch->ifStatEth.txBytes += delta.txBytes;
						ch->ifStatEth.rxBytes += delta.rxBytes;
					}
					if (SceNetInternal_EE94BA15(NETCTL_IF_PHONE, &delta, 1) >= 0) {
						if ((delta.activeSec | delta.txBytes | delta.rxBytes) != 0) {
							ch->flags |= NETCTL_FLAG_IFSTAT_DIRTY;
							ch->ifStatPhone.totalSec += delta.activeSec;
							ch->ifStatPhone.txBytes += delta.txBytes;
							ch->ifStatPhone.rxBytes += delta.rxBytes;
						}
					}
				}
			}
		}
	}

	if ((evLow & 0x64000000u) != 0) {
		SceUInt32 upBit = (ifId == NETCTL_IF_WLAN) ? NETCTL_EV_WLAN_IF_UP : NETCTL_EV_OTHER_IF_UP;
		SceUInt32 downBit = (ifId == NETCTL_IF_WLAN) ? NETCTL_EV_WLAN_IF_DOWN : NETCTL_EV_OTHER_IF_DOWN;
		SceUInt32 errBit = (ifId == NETCTL_IF_WLAN) ? NETCTL_EV_WLAN_IF_ERR : NETCTL_EV_OTHER_IF_ERR;
		int *errDest = (ctx->isScan == 1) ? &ch->scanError : &ch->lastError;
		int ownedByOther = 0;

		if ((ctx->waitMask & (upBit | downBit)) != 0) {
			if ((evLow & 0x04000000u) != 0) {
				*ctx->outBits |= errBit;
				if ((ctx->waitMask & upBit) != 0) {
					int errRet = SceNetInternal_B01ABD0A(ifId, errDest, 0);

					if (errRet < 0)
						*errDest = errRet;
					if (*errDest == NETCTL_NET_ERR_FLIGHT_MODE) {
						*errDest = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
					} else if (*errDest == NETCTL_NET_ERR_WIFI_DISABLED) {
						if (ch->isInet == 1 && netCtlCoreHasEthernetCap())
							*errDest = NETCTL_ERR_NET_FLAG_DISABLED;
						else
							*errDest = SCE_NET_CTL_ERROR_WIFI_DISABLED;
					}
				}
			} else if ((evLow & 0x20000000u) != 0) {
				*ctx->outBits |= upBit;
			} else if ((evLow & 0x40000000u) != 0) {
				*ctx->outBits |= downBit;
			}
		} else {
			if (ifId == NETCTL_IF_PHONE && s_phoneIfOwner == ch->isInet)
				ownedByOther = 1;
			else if (ifId == NETCTL_IF_WLAN && s_wlanIfOwner == ch->isInet)
				ownedByOther = 1;
			else if (ifId == NETCTL_IF_ETHERNET && s_ethIfOwner == ch->isInet)
				ownedByOther = 1;
		}

		if (ch->isInet == 1 && ownedByOther == 1)
			keepMask |= 0x64000000u;
		else
			clearMask = 0x64000000u;
	}

	if (ctx->isScan == 0 && (ctx->waitMask & NETCTL_EV_LINK_ERROR) != 0) {
		if (ifId == NETCTL_IF_ETHERNET && ch->ifId == NETCTL_IF_ETHERNET) {
			if ((evLow & 0x10u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			} else if ((evLow & 0x20u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				if (ch->isInet == 1 && netCtlCoreHasEthernetCap())
					ch->lastError = NETCTL_ERR_NET_FLAG_DISABLED;
				else
					ch->lastError = SCE_NET_CTL_ERROR_WIFI_DISABLED;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			}
			if ((evLow & 0x0Fu) != 0) {
				int linkStatus = 0;

				if (sceNetEthGetLinkStatus(NETCTL_IF_ETHERNET, &linkStatus) >= 0) {
					*ctx->outBits |= NETCTL_EV_LINK_ERROR;
					if ((linkStatus & 1) != 0) {
						ch->lastError = NETCTL_ERR_LINK_CHANGED;
						ch->flags |= NETCTL_FLAG_LINK_UP;
					} else {
						ch->lastError = SCE_NET_CTL_ERROR_ETHERNET_PLUGOUT;
						ch->flags &= ~NETCTL_FLAG_LINK_UP;
					}
				}
			}
		} else if (ifId == NETCTL_IF_WLAN && ch->ifId == NETCTL_IF_WLAN) {
			if ((evLow & 0x0Au) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = SCE_NET_CTL_ERROR_WIFI_DEAUTHED;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			} else if ((evLow & 0x01u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = SCE_NET_CTL_ERROR_WIFI_BEACON_LOST;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			} else if ((evLow & 0x04u) == 0) {
				if ((evLow & 0x00100000u) != 0) {
					*ctx->outBits |= NETCTL_EV_LINK_ERROR;
					ch->lastError = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
					ch->flags &= ~NETCTL_FLAG_LINK_UP;
				} else if ((evLow & 0x00200000u) != 0) {
					*ctx->outBits |= NETCTL_EV_LINK_ERROR;
					if (ch->isInet == 1 && netCtlCoreHasEthernetCap())
						ch->lastError = NETCTL_ERR_NET_FLAG_DISABLED;
					else
						ch->lastError = SCE_NET_CTL_ERROR_WIFI_DISABLED;
					ch->flags &= ~NETCTL_FLAG_LINK_UP;
				}
			}
		} else if (ifId == NETCTL_IF_PHONE && ch->ifId == NETCTL_IF_PHONE) {
			if ((evLow & 0x80000000u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				int errRet = SceNetInternal_B01ABD0A(NETCTL_IF_PHONE, &ch->lastError, 0);

				if (errRet < 0)
					ch->lastError = errRet;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			} else if ((evLow & 0x02u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = NETCTL_ERR_MOBILE_LINK_DOWN;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			} else if ((evLow & 0x04u) != 0) {
				*ctx->outBits |= NETCTL_EV_LINK_ERROR;
				ch->lastError = NETCTL_ERR_MOBILE_DISCONNECTED;
				ch->flags &= ~NETCTL_FLAG_LINK_UP;
			}
		}
	}

	if (ctx->isScan == 1)
		return evLow & ~clearMask;
	return evLow & keepMask;
}

/* 0x81212CC6 */
static int netCtlUpdatePowerSavePolicy(NetCtlChannel *ch, int enable)
{
	int ifId;
	NetCtlSleepParam param;

	if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRED)
		ifId = NETCTL_IF_ETHERNET;
	else if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_PHONE)
		ifId = NETCTL_IF_PHONE;
	else
		ifId = NETCTL_IF_WLAN;

	sceClibMemset(&param, 0, sizeof(param));
	param.handle = -1;
	if (ch->fakeMobileIf == 0 &&
	    (vshKernelCheckModelCapability(4) != 0 ||
	     (enable == 1 && (ifId == NETCTL_IF_WLAN || ifId == NETCTL_IF_ETHERNET))))
		param.timeoutSec = NETCTL_SLEEP_TIMEOUT_INFINITE;
	else
		param.timeoutSec = NETCTL_SLEEP_TIMEOUT_SHORT;
	param.enable = 1;
	SceNetInternal_E03F6A77(ifId, &param, 0);

	if (enable == 1)
		ch->flags |= NETCTL_FLAG_POWERSAVE_ON;
	else
		ch->flags &= ~NETCTL_FLAG_POWERSAVE_ON;
	return 0;
}

/* 0x81212D72 */
int netCtlCoreWaitEvent(NetCtlChannel *ch, int isScan, SceUInt32 mask, SceUInt32 *outBits,
			SceUInt32 timeoutUsec, int *outTimedOut)
{
	if (outTimedOut != NULL)
		*outTimedOut = 0;
	*outBits = 0;

	if ((mask & NETCTL_EV_EXIT) != 0 && (ch->flags & NETCTL_FLAG_EXIT) != 0) {
		*outBits = NETCTL_EV_EXIT;
		ch->flags |= NETCTL_FLAG_EXIT;
		return 0;
	}

	SceUInt64 startTime = sceKernelGetSystemTimeWideForUser();

	for (;;) {
		SceUInt64 now = sceKernelGetSystemTimeWideForUser();

		if (timeoutUsec != 0 && (now - startTime) >= (SceUInt64)timeoutUsec) {
			if (ch->isInet == 1 && netCtlCoreHasEthernetCap()) {
				int netFlag = 0;

				if (netCtlConfigGetNetFlag(&netFlag) >= 0 && netFlag == 1) {
					if (outTimedOut != NULL)
						*outTimedOut = 1;
					break;
				}
			} else {
				if (outTimedOut != NULL)
					*outTimedOut = 1;
				break;
			}
		}

		if ((ch->flags & NETCTL_FLAG_FLUSH_IFSTAT_NOW) != 0)
			netCtlConfigSaveIfStat(ch);

		SceUInt32 prevBits = *outBits;
		SceUInt waitSlice = 100000;

		if (sceKernelWaitEventFlag(ch->eventFlagId, mask,
					   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					   outBits, &waitSlice) < 0)
			*outBits = prevBits;

		NetCtlEventFilterCtx filterCtx;

		sceClibMemset(&filterCtx, 0, sizeof(filterCtx));
		filterCtx.mode = 0;
		filterCtx.isScan = isScan;
		filterCtx.channel = ch;
		filterCtx.waitMask = mask;
		filterCtx.outBits = outBits;
		SceNetInternal_04E6136D(0, &filterCtx, 0);

		if (*outBits != 0)
			break;

		sceKernelCheckCallback();

		if ((ch->flags & NETCTL_FLAG_POWERSAVE_ACTIVE) != 0) {
			int powerSave = 1;

			netCtlConfigGetWifiPowerSave(&powerSave);
			if ((ch->flags & NETCTL_FLAG_POWERSAVE_ON) != 0 && powerSave == 0)
				netCtlUpdatePowerSavePolicy(ch, 0);
			else if ((ch->flags & NETCTL_FLAG_POWERSAVE_ON) == 0 && powerSave == 1)
				netCtlUpdatePowerSavePolicy(ch, 1);
		}

		if (ch->isInet == 1 && netCtlCoreHasEthernetCap()) {
			int netFlag = 0;

			if (netCtlConfigGetNetFlag(&netFlag) >= 0 && ch->netFlag != netFlag) {
				ch->netFlag = netFlag;
				if (netFlag == 0) {
					*outBits |= NETCTL_EV_LINK_ERROR;
					ch->lastError = NETCTL_ERR_NET_FLAG_DISABLED;
					ch->flags &= ~NETCTL_FLAG_LINK_UP;
				}
			}
		}
	}

	if ((*outBits & NETCTL_EV_EXIT) != 0)
		ch->flags |= NETCTL_FLAG_EXIT;
	return 0;
}

/* 0x81212F7C */
int netCtlCorePollEvent(NetCtlChannel *ch, SceUInt32 mask, SceUInt32 *outBits)
{
	if ((mask & NETCTL_EV_EXIT) != 0 && (ch->flags & NETCTL_FLAG_EXIT) != 0) {
		*outBits |= NETCTL_EV_EXIT;
	} else {
		if (sceKernelPollEventFlag(ch->eventFlagId, mask,
					   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					   outBits) < 0)
			*outBits = 0;
	}
	if ((*outBits & NETCTL_EV_EXIT) != 0)
		ch->flags |= NETCTL_FLAG_EXIT;
	return 0;
}

/* 0x81212FC0 */
int netCtlCoreFlushEvents(NetCtlChannel *ch)
{
	NetCtlEventFilterCtx ctx;

	sceClibMemset(&ctx, 0, sizeof(ctx));
	ctx.mode = 1;
	ctx.channel = ch;
	SceNetInternal_04E6136D(0, &ctx, 0);
	return 0;
}

/* 0x81213002 */
int netCtlCoreFlushIfUpEvents(NetCtlChannel *ch, int ifId)
{
	NetCtlEventFilterCtx ctx;

	sceClibMemset(&ctx, 0, sizeof(ctx));
	ctx.mode = 2;
	ctx.targetIfId = ifId;
	ctx.channel = ch;
	SceNetInternal_04E6136D(0, &ctx, 0);
	return 0;
}

/* 0x81213048 */
int netCtlCoreFlushIfDownEvents(NetCtlChannel *ch, int ifId)
{
	NetCtlEventFilterCtx ctx;

	sceClibMemset(&ctx, 0, sizeof(ctx));
	ctx.mode = 3;
	ctx.targetIfId = ifId;
	ctx.channel = ch;
	SceNetInternal_04E6136D(0, &ctx, 0);
	return 0;
}

/* 0x8121308E */
int netCtlCoreAdhocGetInAddr(SceNetInAddr *outAddr)
{
	if (outAddr == NULL)
		return 0;
	if (g_netCtlAdhoc.state != NETCTL_RAW_STATE_CONNECTED)
		return SCE_NET_CTL_ERROR_NOT_CONNECTED;
	*outAddr = g_netCtlAdhoc.ipAddr;
	return 0;
}

/* 0x812130BE */
int netCtlCoreLockFileIo(void)
{
	if (s_fileIoMutexReady != 0)
		return sceKernelLockLwMutex(&s_fileIoMutex, 1, NULL);
	return NETCTL_ERR_MUTEX_NOT_READY;
}

/* 0x812130E8 */
int netCtlCoreUnlockFileIo(void)
{
	if (s_fileIoMutexReady != 0)
		return sceKernelUnlockLwMutex(&s_fileIoMutex, 1);
	return NETCTL_ERR_MUTEX_NOT_READY;
}

/* 0x81213110 */
int netCtlCoreAcquireInterface(int isInet, int ifId, int *outNeedUp)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	int needUp = 0;

	switch (ifId) {
	case NETCTL_IF_PHONE:
		if (s_phoneIfRefCount == 0)
			needUp = 1;
		*outNeedUp = needUp;
		++s_phoneIfRefCount;
		if (needUp)
			s_phoneIfOwner = isInet;
		break;
	case NETCTL_IF_WLAN:
		if (s_wlanIfRefCount == 0)
			needUp = 1;
		*outNeedUp = needUp;
		++s_wlanIfRefCount;
		if (needUp)
			s_wlanIfOwner = isInet;
		break;
	case NETCTL_IF_ETHERNET:
		if (s_ethIfRefCount == 0)
			needUp = 1;
		*outNeedUp = needUp;
		++s_ethIfRefCount;
		if (needUp)
			s_ethIfOwner = isInet;
		break;
	default:
		ret = NETCTL_ERR_INVALID_DEVICE;
		break;
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x8121320E */
int netCtlCoreReleaseInterface(int isInet, int ifId, int *outNeedDown)
{
	int ret = netCtlCoreLock();

	(void)isInet;
	if (ret < 0)
		return ret;

	int needDown = 0;

	switch (ifId) {
	case NETCTL_IF_PHONE:
		if (--s_phoneIfRefCount == 0)
			needDown = 1;
		*outNeedDown = needDown;
		if (needDown)
			s_phoneIfOwner = -1;
		break;
	case NETCTL_IF_WLAN:
		if (--s_wlanIfRefCount == 0)
			needDown = 1;
		*outNeedDown = needDown;
		if (needDown)
			s_wlanIfOwner = -1;
		break;
	case NETCTL_IF_ETHERNET:
		if (--s_ethIfRefCount == 0)
			needDown = 1;
		*outNeedDown = needDown;
		if (needDown)
			s_ethIfOwner = -1;
		break;
	default:
		ret = NETCTL_ERR_INVALID_DEVICE;
		break;
	}

	netCtlCoreUnlock();
	return ret;
}

static void netCtlClearDnsServers(void)
{
	SceNetInAddr zeroDns[2];

	sceClibMemset(zeroDns, 0, sizeof(zeroDns));
	SceNetInternal_16222ED6(zeroDns, 0);
}

/* 0x812133BA */
int netCtlCoreConfigureStaticIp(NetCtlChannel *ch)
{
	SceNetInAddr ipAddr;
	SceNetInAddr maskAddr;
	SceNetInAddr routeAddr;
	SceNetInAddr dnsMain;
	SceNetInAddr dnsAlt;
	SceNetInAddr *useDnsMain = &dnsMain;
	SceNetInAddr *useDnsAlt = &dnsAlt;

	netCtlClearDnsServers();

	int ret = netCtlParseStaticIpConfig(&ch->activeConfig, &ipAddr, &maskAddr,
					    &routeAddr, &dnsMain, &dnsAlt);
	if (ret >= 0) {
		if (dnsMain.s_addr == 0) {
			useDnsMain = NULL;
			useDnsAlt = NULL;
		} else if (dnsAlt.s_addr == 0) {
			useDnsAlt = NULL;
		}

		ret = sceNetConfigSetIfaddr(ch->ifId, &ipAddr, NULL, &maskAddr);
		if (ret >= 0) {
			sceNetConfigSetDefaultRoute(&routeAddr);
			if (useDnsMain != NULL) {
				SceNetInAddr dnsPair[2];

				sceClibMemset(dnsPair, 0, sizeof(dnsPair));
				dnsPair[0] = *useDnsMain;
				if (useDnsAlt != NULL)
					dnsPair[1] = *useDnsAlt;
				ret = SceNetInternal_16222ED6(dnsPair, 0);
			}
			if (ret >= 0)
				sceNetConfigAddMRoute(&ipAddr);
		}
	}

	if (ret < 0) {
		if (sceNetConfigDelIfaddr(ch->ifId) >= 0) {
			SceNetInAddr cleanupIp;

			s_staticIpActive = 0;
			if (netCtlParseStaticIpConfig(&ch->activeConfig, &cleanupIp,
						      NULL, NULL, NULL, NULL) >= 0) {
				sceNetConfigDelMRoute(&cleanupIp);
				netCtlClearDnsServers();
			}
		}
		return ret;
	}

	s_staticIpActive = 1;
	return 0;
}

/* 0x81213CD4 */
static void netCtlOnDhcpEvent(void *arg, int event, int err)
{
	NetCtlChannel *ch = (NetCtlChannel *)arg;

	if (event == 0) {
		if (ch->state == NETCTL_RAW_STATE_IP_CONFIG)
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_IP_READY);
	} else if (event == 5) {
		ch->lastError = err;
		sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_LINK_ERROR);
	}
}

/* 0x812134F4 */
int netCtlCoreConfigureDhcp(NetCtlChannel *ch)
{
	NetCtlDhcpParam dhcpParam;
	SceNetInAddr zeroDns[2];
	int timedOut = 0;

	sceClibMemset(zeroDns, 0, sizeof(zeroDns));
	int ret = SceNetInternal_16222ED6(zeroDns, 0);

	if (ret < 0)
		return ret;

	sceClibMemset(&dhcpParam, 0, sizeof(dhcpParam));
	if (ch->isInet != 0) {
		dhcpParam.flags |= 4u;
		sceClibStrncpy(dhcpParam.hostname, ch->activeConfig.dhcpHostname,
			       sizeof(dhcpParam.hostname));
		dhcpParam.hostname[sizeof(dhcpParam.hostname) - 1] = '\0';
		if (ch->activeConfig.dnsFlag == 1) {
			dhcpParam.flags |= 1u;
			sceNetInetPton(SCE_NET_AF_INET, ch->activeConfig.primary_dns,
				       &dhcpParam.dnsServers[0]);
			sceNetInetPton(SCE_NET_AF_INET, ch->activeConfig.secondary_dns,
				       &dhcpParam.dnsServers[1]);
		}
		if (ch->ifId == NETCTL_IF_PHONE && ch->mobileDns[0].s_addr != 0) {
			dhcpParam.flags |= 1u;
			sceClibMemcpy_safe(dhcpParam.dnsServers, ch->mobileDns,
					   sizeof(dhcpParam.dnsServers));
		}
	} else {
		dhcpParam.flags |= 8u;
	}

	if (vshSblAimgrIsDolce() != 0)
		sceClibStrlcpy(dhcpParam.vendorClass, "PS Vita TV", sizeof(dhcpParam.vendorClass));

	ret = sceNetDhcpStart(ch->ifId, netCtlOnDhcpEvent, ch, 4096,
			      ch->threadPriority, &dhcpParam);
	if (ret < 0)
		return ret;

	SceUInt32 bits = 0;
	SceUInt32 waitMask = (ch->isInet == 1)
				     ? (NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
					NETCTL_EV_LINK_ERROR | NETCTL_EV_IP_READY |
					NETCTL_EV_AUTO_DISCONNECT | NETCTL_EV_ADHOC_PREEMPT)
				     : (NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
					NETCTL_EV_LINK_ERROR | NETCTL_EV_IP_READY);

	do {
		bits = 0;
		timedOut = 0;
		netCtlCoreWaitEvent(ch, 0, waitMask, &bits, NETCTL_IP_WAIT_TIMEOUT_USEC, &timedOut);
		if ((bits & NETCTL_EV_SCAN) == 0)
			break;
		ch->scanError = netCtlRunPendingScan(ch);
		netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
		bits &= ~NETCTL_EV_SCAN;
	} while (bits == 0);

	if ((bits & NETCTL_EV_DISCONNECT) != 0) {
		ret = NETCTL_ERR_DISCONNECT_REQUESTED;
	} else if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
		ret = NETCTL_ERR_AUTO_DISCONNECT_REQ;
	} else if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0) {
		ret = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
		ch->lastError = ret;
	} else if ((bits & NETCTL_EV_LINK_ERROR) != 0) {
		ret = ch->lastError;
	} else if (timedOut == 1) {
		ret = SCE_NET_CTL_ERROR_DHCP_TIMEOUT;
	} else {
		s_dhcpActive = 1;
		return 0;
	}

	sceNetDhcpStop(ch->ifId);
	netCtlClearDnsServers();
	return ret;
}

/* 0x81213D04 */
static void netCtlOnPppoeEvent(void *arg, int event, int err)
{
	NetCtlChannel *ch = (NetCtlChannel *)arg;

	if (event == 1) {
		if (ch->state == NETCTL_RAW_STATE_IP_CONFIG)
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_IP_READY);
	} else if (event == 3) {
		ch->lastError = err;
		sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_LINK_ERROR);
	}
}

/* 0x8121375C */
int netCtlCoreConfigurePppoe(NetCtlChannel *ch)
{
	NetCtlPppoeParam pppoeParam;
	SceNetInAddr zeroDns[2];
	int timedOut = 0;

	sceClibMemset(zeroDns, 0, sizeof(zeroDns));
	int ret = SceNetInternal_16222ED6(zeroDns, 0);

	if (ret < 0)
		return ret;

	sceClibMemset(&pppoeParam, 0, sizeof(pppoeParam));
	if (ch->activeConfig.dnsFlag == 1) {
		pppoeParam.flags |= 1u;
		sceNetInetPton(SCE_NET_AF_INET, ch->activeConfig.primary_dns,
			       &pppoeParam.dnsServers[0]);
		sceNetInetPton(SCE_NET_AF_INET, ch->activeConfig.secondary_dns,
			       &pppoeParam.dnsServers[1]);
	}
	sceClibStrncpy(pppoeParam.authName, ch->activeConfig.pppoeAuthName,
		       sizeof(pppoeParam.authName));
	pppoeParam.authName[sizeof(pppoeParam.authName) - 1] = '\0';
	sceClibStrncpy(pppoeParam.authKey, ch->activeConfig.pppoeAuthKey,
		       sizeof(pppoeParam.authKey));
	pppoeParam.authKey[sizeof(pppoeParam.authKey) - 1] = '\0';
	pppoeParam.mtu = ch->activeConfig.mtu;

	ret = sceNetPppoeStart(ch->ifId, NETCTL_IF_PPPOE, netCtlOnPppoeEvent, ch,
			       4096, ch->threadPriority, &pppoeParam);
	if (ret < 0)
		return ret;

	SceUInt32 bits = 0;
	const SceUInt32 waitMask = NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
				   NETCTL_EV_LINK_ERROR | NETCTL_EV_IP_READY |
				   NETCTL_EV_AUTO_DISCONNECT | NETCTL_EV_ADHOC_PREEMPT;

	do {
		bits = 0;
		timedOut = 0;
		netCtlCoreWaitEvent(ch, 0, waitMask, &bits, NETCTL_IP_WAIT_TIMEOUT_USEC, &timedOut);
		if ((bits & NETCTL_EV_SCAN) == 0)
			break;
		ch->scanError = netCtlRunPendingScan(ch);
		netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
		bits &= ~NETCTL_EV_SCAN;
	} while (bits == 0);

	if ((bits & NETCTL_EV_DISCONNECT) != 0) {
		ret = NETCTL_ERR_DISCONNECT_REQUESTED;
	} else if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
		ret = NETCTL_ERR_AUTO_DISCONNECT_REQ;
	} else if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0) {
		ret = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
		ch->lastError = ret;
	} else if ((bits & NETCTL_EV_LINK_ERROR) != 0) {
		ret = ch->lastError;
	} else if (timedOut == 1) {
		ret = SCE_NET_CTL_ERROR_PPPOE_TIMEOUT;
	} else {
		s_pppoeActive = 1;
		return 0;
	}

	sceNetPppoeStop(NETCTL_IF_PPPOE);
	netCtlClearDnsServers();
	return ret;
}

/* 0x81213D32 */
static void netCtlOnWisprEvent(int prevState, int newState, int eventCode,
			       int errorCode, int resultPtr, int userArg)
{
	NetCtlChannel *ch = (NetCtlChannel *)(SceIntPtr)userArg;
	NetCtlWisprStatus status;

	(void)prevState;
	(void)newState;
	if (eventCode == 2) {
		if (ch->state == NETCTL_RAW_STATE_WISPR_AUTH) {
			sceClibMemset(&status, 0, sizeof(status));
			netCtlCoreSetWisprStatus(&status);
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_WISPR_OK);
		}
	} else if (eventCode == 4) {
		if (ch->state == NETCTL_RAW_STATE_WISPR_AUTH) {
			sceClibMemset(&status, 0, sizeof(status));
			if (errorCode == NET_WISPR_ERROR_AUTH_FAILED) {
				const NetWisprResultInfo *res =
					(const NetWisprResultInfo *)(SceIntPtr)resultPtr;

				status.hasWisprResult = 1;
				status.result.responseCode = res->responseCode;
				sceClibStrncpy(status.result.replyMessage, res->replyMessage,
					       sizeof(status.result.replyMessage));
				status.result.replyMessage[sizeof(status.result.replyMessage) - 1] = '\0';
				sceClibStrncpy(status.result.loginUrl, res->loginUrl,
					       sizeof(status.result.loginUrl));
				status.result.loginUrl[sizeof(status.result.loginUrl) - 1] = '\0';
				sceClibStrncpy(status.result.logoffUrl, res->logoffUrl,
					       sizeof(status.result.logoffUrl));
				status.result.logoffUrl[sizeof(status.result.logoffUrl) - 1] = '\0';
			} else {
				status.errorCode = errorCode;
			}
			netCtlCoreSetWisprStatus(&status);
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_WISPR_FAIL);
		}
	}
}

/* 0x81213988 */
int netCtlCoreRunWisprAuth(NetCtlChannel *ch)
{
	if (ch->hostFuncs == NULL || ch->hostFuncs->wisprStart == NULL)
		return 0;

	int ret = ch->hostFuncs->wisprStart(netCtlOnWisprEvent, ch->wisprIspId,
					    ch->mobileNetName, &ch->connectedBss.bssid,
					    (int)(SceIntPtr)ch);
	if (ret < 0)
		return ret;

	SceUInt32 bits = 0;
	const SceUInt32 waitMask = NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
				   NETCTL_EV_LINK_ERROR | NETCTL_EV_AUTO_DISCONNECT |
				   NETCTL_EV_ADHOC_PREEMPT | NETCTL_EV_WISPR_OK |
				   NETCTL_EV_WISPR_FAIL;

	do {
		bits = 0;
		netCtlCoreWaitEvent(ch, 0, waitMask, &bits, 0, NULL);
		if ((bits & NETCTL_EV_SCAN) == 0)
			break;
		ch->scanError = netCtlRunPendingScan(ch);
		netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
		bits &= ~NETCTL_EV_SCAN;
	} while (bits == 0);

	if ((bits & NETCTL_EV_DISCONNECT) != 0) {
		ret = NETCTL_ERR_DISCONNECT_REQUESTED;
	} else if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
		ret = NETCTL_ERR_AUTO_DISCONNECT_REQ;
	} else if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0) {
		ret = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
		ch->lastError = ret;
	} else if ((bits & NETCTL_EV_LINK_ERROR) != 0) {
		ret = ch->lastError;
	} else if ((bits & NETCTL_EV_WISPR_FAIL) != 0) {
		ret = NETCTL_ERR_WISPR_AUTH_FAILED;
	} else {
		ret = 0;
	}

	if (ch->hostFuncs != NULL && ch->hostFuncs->wisprStop != NULL)
		ch->hostFuncs->wisprStop();
	return ret;
}

/* 0x81213AB8 */
int netCtlCoreTeardownIp(NetCtlChannel *ch)
{
	if (s_staticIpActive == 1 && sceNetConfigDelIfaddr(ch->ifId) >= 0) {
		SceNetInAddr ip;

		s_staticIpActive = 0;
		if (netCtlParseStaticIpConfig(&ch->activeConfig, &ip,
					      NULL, NULL, NULL, NULL) >= 0) {
			sceNetConfigDelMRoute(&ip);
			netCtlClearDnsServers();
		}
	}

	if (s_dhcpActive == 1) {
		sceNetDhcpStop(ch->ifId);
		netCtlClearDnsServers();
		s_dhcpActive = 0;
	}

	if (s_pppoeActive == 1) {
		sceNetPppoeStop(NETCTL_IF_PPPOE);
		netCtlClearDnsServers();
		s_pppoeActive = 0;
	}

	sceNetConfigDelIfaddr(ch->ifId);
	sceNetConfigDelDefaultRoute();
	netCtlClearDnsServers();
	return 0;
}

/* 0x81213BD2 */
int netCtlCoreRefreshIpInfo(NetCtlChannel *ch)
{
	NetCtlIfInfo ifInfo;
	NetCtlIfAddrEntry addrs[2];
	SceNetInAddr dnsServers[2];
	int targetIf = (ch->activeConfig.ipConfig == SCE_NET_CTL_IP_PPPOE) ? NETCTL_IF_PPPOE : ch->ifId;
	int ret = SceNetInternal_235DE96C(targetIf, &ifInfo, 1);

	if (ret < 0)
		return ret;

	ch->activeMtu = ifInfo.mtu;
	ret = sceNetConfigGetIfaddrs(targetIf, addrs, 2);
	if (ret < 0)
		return ret;

	int useIndex = (ch->isInet == 0 || addrs[0].addr.s_addr == 0) ? 1 : 0;

	ch->ipAddr = addrs[useIndex].addr;
	ch->netmask = addrs[useIndex].netmask;
	ch->ipAddrStr[0] = '\0';
	sceNetInetNtop(SCE_NET_AF_INET, &ch->ipAddr, ch->ipAddrStr, sizeof(ch->ipAddrStr));

	ret = sceNetConfigGetDefaultRoute(&ch->defaultRoute);
	if (ret == NETCTL_NET_ERR_NO_DEFAULT_ROUTE)
		ch->defaultRoute.s_addr = 0;
	else if (ret < 0)
		return ret;

	ret = SceNetInternal_7F63379E(dnsServers, 0);
	if (ret >= 0) {
		ch->primaryDnsAddr = dnsServers[0];
		ch->secondaryDnsAddr = dnsServers[1];
	}
	return ret;
}
