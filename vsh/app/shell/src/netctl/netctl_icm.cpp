/*
 * netctl_icm.cpp: SceNetCtlInt ICM 6-state machine, power callback, WLAN scan & BSS
 * ranking/lookup, inet/adhoc getInfo switches, and WLAN/Adhoc link management.
 * 3.65-CEX: 0x81214BAA - 0x812181CC
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <net.h>
#include "netctl_internal.h"

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

#define NETCTL_POWER_CB_SUSPENDING		0x00020000u
#define NETCTL_POWER_CB_RESUME_COMPLETE		0x00040000u
#define NETCTL_POWER_CB_RESUMING		0x00400000u
#define NETCTL_POWER_CB_STANDBY			0x00800000u

#define NETCTL_WLAN_SCAN_REQ_BASE_SIZE		444u
#define NETCTL_WLAN_BSS_TYPE_IBSS		2u
#define NETCTL_WLAN_CAP_PRIVACY			0x0010u
#define NETCTL_WLAN_CAP_IBSS			0x0022u

typedef int (*NetCtlStateHandler)(NetCtlChannel *ch, int prevState, int *outNextState,
				  int *outEventCode);

struct NetCtlAutoScanCandidate {
	SceUInt32 configId;				/* +0x00 */
	char ssid[33];					/* +0x04 */
	char padSsid[3];				/* +0x25 */
	SceUInt32 security;				/* +0x28 */
	SceUInt32 rssiPercentage;			/* +0x2C */
	SceUInt32 channel;				/* +0x30 */
	SceUInt32 confType;				/* +0x34 */
};

struct NetCtlWlanSsidEntry {
	SceUInt32 length;				/* +0x00 */
	char bytes[32];					/* +0x04 */
};

struct NetCtlWlanScanRequest {
	SceUInt32 reqSize;				/* +0x000 */
	SceUInt32 outBufSize;				/* +0x004 */
	SceUInt32 optionFlags;				/* +0x008 */
	SceUInt16 scanMode;				/* +0x00C */
	SceUInt8 channels[14];				/* +0x00E */
	SceUInt8 bssType;				/* +0x01C */
	SceUInt8 ssidCount;				/* +0x01D */
	SceUInt8 padHeader[6];				/* +0x01E */
	NetCtlWlanSsidEntry ssids[NETCTL_MAX_SCAN_SSIDS]; /* +0x024..+0x18B */
	SceUInt32 dwellMin;				/* +0x18C */
	SceUInt32 dwellMax;				/* +0x190 */
	SceUInt8 channelMask[32];			/* +0x194 */
	SceUInt32 customIeSize;				/* +0x1B4 */
	SceUInt8 customIe[NETCTL_CUSTOM_IE_MAX];	/* +0x1B8..+0x5B7 */
	SceUInt8 reservedTail[8];			/* +0x5B8..+0x5BF */
};

struct NetCtlWlanConnectRequest {
	SceUInt32 reqSize;				/* +0x000 */
	SceUInt32 outBufSize;				/* +0x004 */
	SceUInt32 optionFlags;				/* +0x008 */
	SceUInt32 ssidLen;				/* +0x00C */
	char ssid[32];					/* +0x010 */
	SceUInt32 authMode;				/* +0x030 */
	SceUInt32 securityMode;				/* +0x034 */
	SceUInt32 wepKeyType;				/* +0x038 */
	SceUInt32 wepDefaultKeyIdx;			/* +0x03C */
	SceUInt8 wepKeys[4][13];			/* +0x040..+0x073 */
	SceUInt8 wpaKey[63];				/* +0x074..+0x0B2 */
	SceUInt8 wpaKeyFormat;				/* +0x0B3 */
	SceUInt8 channel;				/* +0x0B4 */
	SceUInt8 padChannel[3];				/* +0x0B5 */
	SceUInt32 customIeProbeSize;			/* +0x0B8 */
	SceUInt32 customIeAssocSize;			/* +0x0BC */
	SceUInt8 customIePayload[NETCTL_CUSTOM_IE_MAX * 2]; /* +0x0C0..+0x8BF */
	SceUInt32 reservedTail;				/* +0x8C0 */
};

struct NetCtlWlanAdhocRequest {
	SceUInt32 reqSize;				/* +0x000 */
	SceUInt32 outBufSize;				/* +0x004 */
	SceUInt32 flags;				/* +0x008 */
	SceUInt8 reservedHead[132];			/* +0x00C..+0x08F */
	NetCtlWlanBssRecord bss;			/* +0x090..+0x113 */
};

struct NetCtlWlanRssiInfo {
	SceUInt32 reqSize;
	SceUInt32 outBufSize;
	SceUInt32 rssiDbm;
	SceUInt32 rssiPercentage;
};

struct NetCtlIfInfoCompact {
	SceUInt8 header[80];
	SceNetEtherAddr etherAddr;
	SceUInt8 trailing[234];
};

static int netCtlStateIdle(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlStateAutoSelect(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlStateLinkUp(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlStateIpConfig(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlStateConnected(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlStateWisprAuth(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode);
static int netCtlBringUpInterface(NetCtlChannel *ch, int ifId, int isScan);
static int netCtlBringDownInterface(NetCtlChannel *ch, int ifId, int isScan);
static int netCtlLinkConnect(NetCtlChannel *ch, int useRankedList);
static int netCtlLinkDisconnect(NetCtlChannel *ch);

/* 0x81513B3C */
static const NetCtlStateHandler s_stateHandlers[6] = {
	netCtlStateIdle,
	netCtlStateAutoSelect,
	netCtlStateLinkUp,
	netCtlStateIpConfig,
	netCtlStateConnected,
	netCtlStateWisprAuth,
};

static const NetCtlWlanBssRecord *netCtlNextBssRecord(const NetCtlWlanBssRecord *rec)
{
	const SceUInt8 *raw = (const SceUInt8 *)rec;

	return (const NetCtlWlanBssRecord *)(raw + rec->recordSize);
}

static const SceUInt8 *netCtlBssIeBytes(const NetCtlWlanBssRecord *rec)
{
	return (const SceUInt8 *)(rec + 1);
}

static void netCtlNotifyIcmSocketEvent(const char *sockName)
{
	int sock = sceNetSocket(sockName, SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);

	if (sock >= 0) {
		SceNetInternal_93F2FF08(sock, 128);
		sceNetSocketClose(sock);
	}
}

/* 0x81214D94 */
static int netCtlPowerCallback(int notifyId, int notifyCount, int powerInfo, void *common)
{
	NetCtlChannel *ch = (NetCtlChannel *)common;
	SceUInt32 bits = (SceUInt32)powerInfo;
	int resumeComplete = (bits & NETCTL_POWER_CB_RESUME_COMPLETE) != 0;

	(void)notifyId;
	(void)notifyCount;

	if ((bits & (NETCTL_POWER_CB_SUSPENDING | NETCTL_POWER_CB_STANDBY)) != 0 ||
	    ((ch->flags & NETCTL_FLAG_SUSPENDED) == 0 && resumeComplete)) {
		if ((bits & NETCTL_POWER_CB_SUSPENDING) != 0)
			ch->flags |= NETCTL_FLAG_SUSPENDED;
		ch->exclusiveImposeShown = 0;
		ch->exclusiveWifiPromptShown = 0;
		ch->flags &= ~NETCTL_FLAG_IFSTAT_SAVED;
		sceClibMemset(ch->lastConnectedSsid, 0, sizeof(ch->lastConnectedSsid));
		ch->exclusiveResult.selectedConfigId = 0;

		int powerSave = 1;

		netCtlConfigGetWifiPowerSave(&powerSave);
		if (powerSave == 1 && ch->isInet == 1) {
			if ((ch->flags & NETCTL_FLAG_IDLE) != 0) {
				int sock = sceNetSocket("SceNetCtlIcmConnect",
							SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);

				if (sock >= 0) {
					SceNetInternal_93F2FF08(sock, 128);
					sceNetSocketClose(sock);
					ch->flags |= NETCTL_FLAG_ICM_BLOCKED;
				}
			} else {
				ch->flags |= NETCTL_FLAG_ICM_PENDING;
			}
		}
	}

	if (resumeComplete)
		ch->flags &= ~NETCTL_FLAG_SUSPENDED;

	if ((bits & NETCTL_POWER_CB_RESUMING) != 0) {
		ch->exclusiveImposeShown = 1;
		ch->exclusiveWifiPromptShown = 1;
		ch->flags &= ~NETCTL_FLAG_SUSPENDED;
	}
	return 0;
}

/* 0x81214BAA */
int netCtlIcmThreadEntry(SceSize args, void *argp)
{
	NetCtlChannel *ch = *(NetCtlChannel **)argp;
	int powerSave = 1;
	int icmType = 1;
	int nextState = NETCTL_RAW_STATE_IDLE;
	int nextEvent = 0;

	(void)args;
	netCtlConfigGetNetFlag(&ch->netFlag);
	if (ch->isInet == 1)
		netCtlConfigReloadAll();

	if (SceNetInternal_EAC33599(-1, (void *)netCtlCoreNetEventFilter, 0, 0) >= 0 &&
	    SceNetInternal_EAC33599(NETCTL_IF_ETHERNET, (void *)netCtlCoreNetEventFilter, 0, 0) >= 0 &&
	    SceNetInternal_EAC33599(NETCTL_IF_WLAN, (void *)netCtlCoreNetEventFilter, 0, 0) >= 0 &&
	    SceNetInternal_EAC33599(NETCTL_IF_PHONE, (void *)netCtlCoreNetEventFilter, 0, 0) >= 0) {
		if (ch->isInet == 1) {
			netCtlConfigGetFakeMobileIf(&ch->fakeMobileIf);
			netCtlConfigGetIcmType(&icmType);
			if (icmType == 0)
				netCtlNotifyIcmSocketEvent("SceNetCtlIcmTypeDisabled");

			netCtlConfigGetWifiPowerSave(&powerSave);
			if (powerSave == 1)
				ch->flags |= NETCTL_FLAG_ICM_PENDING;
		}

		SceUID cbid = sceKernelCreateCallback("SceNetCtl", 0, netCtlPowerCallback, ch);

		if (cbid >= 0) {
			if (scePowerRegisterCallback(cbid) >= 0) {
				s_stateHandlers[ch->state](ch, 0, &nextState, &nextEvent);
				while (!(ch->state == NETCTL_RAW_STATE_IDLE &&
					 (ch->flags & NETCTL_FLAG_EXIT) != 0)) {
					SceUInt32 outBits = 0;

					if (sceKernelPollEventFlag(ch->eventFlagId, NETCTL_EV_SCAN,
								   SCE_KERNEL_EVF_WAITMODE_OR |
									   SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
								   &outBits) >= 0 &&
					    (outBits & NETCTL_EV_SCAN) != 0) {
						ch->scanError = netCtlRunPendingScan(ch);
						netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
					}

					int prevState = ch->state;

					ch->state = nextState;
					ch->eventCode = nextEvent;
					netCtlCoreNotifyStateChange(ch, prevState);
					s_stateHandlers[ch->state](ch, prevState, &nextState, &nextEvent);
				}

				if (ch->isInet == 1) {
					ch->flags = (ch->flags & ~(NETCTL_FLAG_IFSTAT_SAVED |
								   NETCTL_FLAG_IFSTAT_DIRTY)) |
						    NETCTL_FLAG_IFSTAT_DIRTY;
					netCtlConfigSaveIfStat(ch);
				}
			}
			sceKernelDeleteCallback(cbid);
		}
	}

	SceNetInternal_EDAA3453(-1, 0);
	SceNetInternal_EDAA3453(NETCTL_IF_ETHERNET, 0);
	SceNetInternal_EDAA3453(NETCTL_IF_WLAN, 0);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x81214E88 */
static int netCtlStateIdle(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode)
{
	(void)prevState;
	ch->ipAddrStr[0] = '\0';
	netCtlConfigUpdateCurrentIp(ch->ipAddrStr, sizeof(ch->ipAddrStr));
	sceKernelCheckCallback();

	if (ch->isInet == 1 && (ch->flags & NETCTL_FLAG_IFSTAT_SAVED) == 0)
		netCtlConfigSaveIfStat(ch);

	netCtlCoreResetSleepTimers(ch);

	if (ch->isInet == 1 && netCtlCoreHasEthernetCap() == 1 &&
	    (ch->flags & NETCTL_FLAG_BOOT_DELAY_DONE) == 0) {
		sceKernelDelayThread(4000000);
		ch->flags |= NETCTL_FLAG_BOOT_DELAY_DONE;
	}

	netCtlCoreReleaseMode(ch, 3);
	netCtlCoreFlushEvents(ch);
	sceKernelClearEventFlag(ch->eventFlagId,
				~(NETCTL_EV_LINK_ERROR | NETCTL_EV_IP_READY |
				  NETCTL_EV_AUTO_DISCONNECT | NETCTL_EV_ADHOC_PREEMPT |
				  NETCTL_EV_WISPR_OK | NETCTL_EV_WISPR_FAIL));
	ch->lastError = 0;
	ch->flags &= ~(NETCTL_FLAG_IP_OBTAINED | NETCTL_FLAG_RECONNECTING |
		       NETCTL_FLAG_SUSPEND_DISCONN | NETCTL_FLAG_ICM_BLOCKED |
		       NETCTL_FLAG_POWERSAVE_ACTIVE);

	SceUInt32 bits = 0;

	for (;;) {
		int timedOut = 0;

		do {
			if ((ch->flags & NETCTL_FLAG_ICM_PENDING) != 0) {
				int sock = sceNetSocket("SceNetCtlIcmConnect",
							SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);

				if (sock >= 0) {
					SceNetInternal_93F2FF08(sock, 128);
					sceNetSocketClose(sock);
					ch->flags |= NETCTL_FLAG_ICM_BLOCKED;
				}
				ch->flags &= ~NETCTL_FLAG_ICM_PENDING;
			}

			timedOut = 0;
			bits = 0;
			SceUInt32 waitMask = NETCTL_EV_CONNECT | NETCTL_EV_DISCONNECT |
					     NETCTL_EV_SCAN | NETCTL_EV_EXIT;

			if (ch->isInet == 1) {
				waitMask |= NETCTL_EV_EXCLUSIVE_DONE | NETCTL_EV_EXCLUSIVE_ABORT;
				if (ch->exclusiveRunning == 0)
					waitMask |= NETCTL_EV_AUTO_CONNECT;
			}

			if (ch->isInet == 1 && netCtlCoreIsDevkit() == 1)
				netCtlCoreWaitEvent(ch, 0, waitMask, &bits, 1000000, &timedOut);
			else
				netCtlCoreWaitEvent(ch, 0, waitMask, &bits, 0, NULL);

			if ((bits & NETCTL_EV_EXIT) != 0)
				return 0;

			if ((bits & NETCTL_EV_SCAN) == 0)
				break;
			ch->scanError = netCtlRunPendingScan(ch);
			netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
			bits &= ~NETCTL_EV_SCAN;
		} while (bits == 0);

		if ((bits & NETCTL_EV_DISCONNECT) != 0) {
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 7;
			int lockRet = netCtlCoreLock();

			ch->adhocOwnerPid = -1;
			ch->flags |= NETCTL_FLAG_IDLE;
			if (lockRet >= 0)
				netCtlCoreUnlock();
			netCtlCoreNotifyAutoConnectResult(ch, SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED);
			if ((bits & NETCTL_EV_EXCLUSIVE_DONE) != 0)
				sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_DONE);
			if ((bits & NETCTL_EV_EXCLUSIVE_ABORT) != 0)
				sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_ABORT);
			return 0;
		}

		if (((bits & NETCTL_EV_AUTO_CONNECT) != 0 && ch->exclusiveRunning == 0) ||
		    (bits & NETCTL_EV_CONNECT) != 0)
			break;

		if ((bits & NETCTL_EV_EXCLUSIVE_ABORT) != 0) {
			ch->exclusiveRunning = 0;
			int lockRet = netCtlCoreLock();

			ch->flags &= ~NETCTL_FLAG_IDLE;
			if (lockRet >= 0)
				netCtlCoreUnlock();
			ch->exclusiveImposeShown = 0;
			*outEventCode = 16;
			ch->connectMode = 0;
			sceClibMemset(&ch->activeConfig, 0, sizeof(ch->activeConfig));
			*outNextState = NETCTL_RAW_STATE_AUTO_SELECT;
			return 0;
		}

		if ((bits & NETCTL_EV_EXCLUSIVE_DONE) != 0) {
			int failed = 0;

			if (ch->exclusiveResult.valid != 1) {
				failed = 1;
			} else {
				int lockRet = netCtlCoreLock();

				if (lockRet < 0) {
					ch->exclusiveError = lockRet;
					failed = 1;
				} else {
					SceKernelSysClock clock;
					int procRet = sceKernelGetRemoteProcessTime(
						ch->exclusiveResult.clientPid, &clock);

					if (procRet < 0) {
						netCtlCoreUnlock();
						ch->exclusiveError = procRet;
						failed = 1;
					} else {
						ch->flags |= NETCTL_FLAG_EXCLUSIVE_ACTIVE;
						netCtlCoreUnlock();
					}
				}
			}

			if (failed) {
				if (ch->exclusiveError == NETCTL_EXCL_ERR_BUSY)
					ch->exclusiveImposeShown = 0;
				ch->exclusiveRunning = 0;
			}

			if (ch->exclusiveRunning == 0) {
				int lockRet = netCtlCoreLock();

				ch->flags &= ~NETCTL_FLAG_IDLE;
				if (lockRet >= 0)
					netCtlCoreUnlock();
				*outEventCode = 15;
				ch->connectMode = 0;
				sceClibMemset(&ch->activeConfig, 0, sizeof(ch->activeConfig));
				*outNextState = NETCTL_RAW_STATE_AUTO_SELECT;
				return 0;
			}

			bits &= ~NETCTL_EV_EXCLUSIVE_DONE;
			if (bits == 0)
				continue;
		}

		if (timedOut != 1)
			return 0;

		if (netCtlCoreGetFrozenPid() == -1 && netCtlCoreAnyAutoConnectSuppressed() != 1)
			netCtlNotifyIcmSocketEvent("SceNetCtlIcmConnect");
	}

	if (ch->isInet == 0) {
		int waitRet = netCtlCoreWaitForAdhocMode();

		if (waitRet < 0) {
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			ch->lastError = waitRet;
			int lockRet = netCtlCoreLock();

			ch->adhocOwnerPid = -1;
			ch->flags |= NETCTL_FLAG_IDLE;
			if (lockRet >= 0)
				netCtlCoreUnlock();
			return 0;
		}
	}

	if (ch->isInet == 1 &&
	    (((bits & NETCTL_EV_AUTO_CONNECT) != 0 && (bits & NETCTL_EV_CONNECT) == 0) ||
	     ch->connectMode == 0)) {
		*outEventCode = 2;
		if ((bits & NETCTL_EV_EXCLUSIVE_DONE) != 0)
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_DONE);
		if ((bits & NETCTL_EV_EXCLUSIVE_ABORT) != 0)
			sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_ABORT);
		ch->connectMode = 0;
		sceClibMemset(&ch->activeConfig, 0, sizeof(ch->activeConfig));
		*outNextState = NETCTL_RAW_STATE_AUTO_SELECT;
		return 0;
	}

	*outNextState = NETCTL_RAW_STATE_LINK_UP;
	*outEventCode = 1;
	if ((bits & NETCTL_EV_EXCLUSIVE_DONE) != 0)
		sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_DONE);
	if ((bits & NETCTL_EV_EXCLUSIVE_ABORT) != 0)
		sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_ABORT);
	return 0;
}

static void netCtlFinishAutoSelect(NetCtlChannel *ch, int *outNextState)
{
	int lockRet = netCtlCoreLock();

	ch->exclusiveResetFlag = 0;
	if (lockRet >= 0)
		netCtlCoreUnlock();

	if (*outNextState == NETCTL_RAW_STATE_IDLE) {
		int idleLock = netCtlCoreLock();

		ch->flags |= NETCTL_FLAG_IDLE;
		ch->adhocOwnerPid = -1;
		if (idleLock >= 0)
			netCtlCoreUnlock();
		netCtlCoreNotifyAutoConnectResult(ch, SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED);
		ch->ifId = -1;
	}
}

static int netCtlTryPhoneFallback(NetCtlChannel *ch, int fallbackErr,
				  int *outNextState, int *outEventCode)
{
	int phoneAvail = SceNetInternal_235DE96C(NETCTL_IF_PHONE, NULL, 0);
	int phoneStatus = 0;

	if (sceNetPhoneGetStatus(NETCTL_IF_PHONE, &phoneStatus) < 0)
		phoneStatus = 0;
	if ((phoneStatus & 8) == 0 && ((phoneStatus & 1) == 0 || (phoneStatus & 2) == 0))
		phoneAvail = 0;

	if ((ch->flags & NETCTL_FLAG_ICM_BLOCKED) != 0 || phoneAvail != 1) {
		ch->lastError = fallbackErr;
		*outNextState = NETCTL_RAW_STATE_IDLE;
		*outEventCode = 9;
		return 0;
	}

	sceClibMemset(&ch->activeConfig, 0, sizeof(ch->activeConfig));
	ch->activeConfig.ipConfig = SCE_NET_CTL_IP_DHCP;
	ch->activeConfig.device = SCE_NET_CTL_DEVICE_PHONE;
	*outNextState = NETCTL_RAW_STATE_LINK_UP;
	*outEventCode = 3;
	return 1;
}

static int netCtlPerformWlanScan(NetCtlChannel *ch, const char **ssids, SceUInt32 ssidCount,
				 const NetCtlScanParam *param, void *outBuf, SceSize outBufSize,
				 void **outList, SceUInt32 *outCount, int *busyFlag,
				 SceUInt64 *outSysTime, SceUInt64 *outRtcTime);
static int netCtlMatchAutoScanCandidates(NetCtlChannel *ch, const char **ssids, SceUInt32 count,
					 NetCtlAutoScanCandidate *candidates);
static int netCtlRankAutoScanCandidates(NetCtlChannel *ch, NetCtlAutoScanCandidate *candidates,
					SceUInt32 count);

/* 0x8121526E */
static int netCtlStateAutoSelect(NetCtlChannel *ch, int prevState, int *outNextState,
				 int *outEventCode)
{
	NetCtlAutoScanCandidate candidates[NETCTL_MAX_CONFIGS];
	const char *ssidPtrs[NETCTL_MAX_CONFIGS];
	NetCtlNetConfig tempCfg;
	int flightMode = 1;
	int wifiEnabled = 0;
	int ethPresent = SceNetInternal_235DE96C(NETCTL_IF_ETHERNET, NULL, 0);
	int ethLink = 0;

	(void)prevState;
	if (ethPresent == 1 &&
	    (sceNetEthGetLinkStatus(NETCTL_IF_ETHERNET, &ethLink) < 0 || (ethLink & 1) == 0))
		ethPresent = 0;

	SceUID reqPid = ch->exclusiveRequestPid;

	ch->exclusiveRequestPid = -1;

	if (netCtlCoreHasEthernetCap() == 1) {
		int netFlag = 0;

		netCtlConfigGetNetFlag(&netFlag);
		if (netFlag == 0) {
			ch->lastError = NETCTL_ERR_NET_FLAG_DISABLED;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
	}

	netCtlConfigGetFlightMode(&flightMode);
	netCtlConfigGetWifiEnabled(&wifiEnabled);
	if (flightMode == 1 && wifiEnabled == 0) {
		ch->lastError = SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED;
		*outNextState = NETCTL_RAW_STATE_IDLE;
		*outEventCode = 9;
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	if (ch->hostFuncs != NULL && ch->hostFuncs->isProcessConnecting != NULL &&
	    ch->hostFuncs->isProcessConnecting(netCtlCoreGetFrozenPid(), reqPid) == 0) {
		ch->lastError = NETCTL_ERR_NOT_ALLOWED_PROCESS;
		*outNextState = NETCTL_RAW_STATE_IDLE;
		*outEventCode = 9;
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	if (*outEventCode == 16) {
		int token = 0;

		if (netCtlCoreAcquireInetMode(ch, &token) < 0) {
			ch->lastError = SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
		if (netCtlCoreStartExclusiveDialog(ch, 3, reqPid, 1, &ch->exclusiveImposeShown) == 1) {
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 14;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
		netCtlTryPhoneFallback(ch, SCE_NET_CTL_ERROR_NO_SUITABLE_SETTING_FOR_AUTO_CONNECT,
				       outNextState, outEventCode);
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	if (*outEventCode == 15) {
		int token = 0;

		if (netCtlCoreAcquireInetMode(ch, &token) < 0) {
			ch->lastError = SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
		netCtlTryPhoneFallback(ch, SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED,
				       outNextState, outEventCode);
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	int mode = 0;
	int needPrompt = 0;
	int promptErr = 0;
	unsigned int promptReason = 0;

	SceShell_36788D70(&mode);
	if (wifiEnabled != 0 || ethPresent == 1) {
		if (mode == 3) {
			promptErr = SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE;
			promptReason = 2;
			needPrompt = 1;
		} else if (mode == 1) {
			int token = 0;

			if (netCtlCoreAcquireInetMode(ch, &token) < 0) {
				ch->lastError = SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				*outEventCode = 9;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
		}
	} else {
		promptErr = (flightMode == 1) ? SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED
					      : SCE_NET_CTL_ERROR_WIFI_DISABLED;
		promptReason = 1;
		needPrompt = 1;
	}

	if (netCtlCoreIsDevkit() == 1 && ch->isInet == 1) {
		SceUInt64 now = sceKernelGetSystemTimeWide();

		if (g_netCtlAdhocLeaveTime != 0 &&
		    (now - g_netCtlAdhocLeaveTime) <= NETCTL_ADHOC_COOLDOWN_USEC) {
			SceUInt32 bits = 0;
			SceUInt32 remain = (SceUInt32)(NETCTL_ADHOC_COOLDOWN_USEC -
						       (now - g_netCtlAdhocLeaveTime));

			netCtlCoreWaitEvent(ch, 0,
					    NETCTL_EV_DISCONNECT | NETCTL_EV_AUTO_DISCONNECT |
						    NETCTL_EV_ADHOC_PREEMPT,
					    &bits, remain, NULL);
			if ((bits & NETCTL_EV_DISCONNECT) != 0) {
				*outEventCode = 7;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
				*outEventCode = 8;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0) {
				ch->lastError = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
				*outEventCode = 9;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
		}
	}

	if (needPrompt == 1) {
		int promptFailed = 0;

		if (netCtlCoreStartExclusiveDialog(ch, promptReason, reqPid, 0,
						   &ch->exclusiveWifiPromptShown) == 0) {
			promptFailed = 1;
		} else {
			SceUInt32 bits = 0;

			do {
				bits = 0;
				netCtlCoreWaitEvent(ch, 0,
						    NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
							    NETCTL_EV_AUTO_DISCONNECT |
							    NETCTL_EV_ADHOC_PREEMPT |
							    NETCTL_EV_EXCLUSIVE_DONE,
						    &bits, 0, NULL);
				if ((bits & NETCTL_EV_SCAN) == 0)
					break;
				ch->scanError = netCtlRunPendingScan(ch);
				netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
				bits &= ~NETCTL_EV_SCAN;
			} while (bits == 0);

			if ((bits & NETCTL_EV_DISCONNECT) != 0) {
				*outEventCode = 7;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				ch->exclusiveRunning = 0;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
				*outEventCode = 8;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				ch->exclusiveRunning = 0;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0) {
				ch->lastError = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
				*outEventCode = 9;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				ch->exclusiveRunning = 0;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}

			int exclErr = ch->exclusiveError;

			ch->exclusiveRunning = 0;
			if (exclErr < 0) {
				if (exclErr == NETCTL_EXCL_ERR_BUSY ||
				    exclErr == NETCTL_EXCL_ERR_CANCELED)
					ch->exclusiveWifiPromptShown = 0;
				promptFailed = 1;
			} else if (mode == 3) {
				int token = 0;

				if (netCtlCoreAcquireInetMode(ch, &token) < 0)
					promptFailed = 1;
			}
			if (!promptFailed)
				ch->exclusiveImposeShown = 0;
		}

		if (promptFailed) {
			if (promptReason != 1) {
				ch->lastError = promptErr;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				*outEventCode = 9;
			} else {
				netCtlTryPhoneFallback(ch, promptErr, outNextState, outEventCode);
			}
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
	}

	sceClibMemset(candidates, 0, sizeof(candidates));
	if (netCtlCoreHasEthernetCap() == 1 && ethPresent == 1) {
		int exists = 0;
		int loadRet = netCtlConfigLoadToNetConfig(1, 0, &tempCfg, &exists);

		if (loadRet < 0) {
			ch->lastError = loadRet;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
		} else {
			sceClibMemcpy_safe(&ch->activeConfig, &tempCfg, sizeof(NetCtlNetConfig));
			*outNextState = NETCTL_RAW_STATE_LINK_UP;
			*outEventCode = 3;
		}
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	SceUInt32 candCount = 0;

	for (SceUInt32 i = 1; i <= NETCTL_MAX_CONFIGS; ++i) {
		int exists = 0;
		int loadRet = netCtlConfigLoadToNetConfig(0, i, &tempCfg, &exists);

		if (loadRet < 0) {
			ch->lastError = loadRet;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}
		if (exists == 0)
			continue;

		if (tempCfg.device == SCE_NET_CTL_DEVICE_WIRED) {
			if (ethPresent == 1) {
				sceClibMemcpy_safe(&ch->activeConfig, &tempCfg, sizeof(NetCtlNetConfig));
				*outNextState = NETCTL_RAW_STATE_LINK_UP;
				*outEventCode = 3;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
		} else if (tempCfg.enableAutoConnect == 1 ||
			   sceClibStrncmp(ch->lastConnectedSsid, tempCfg.ssid, 33) == 0 ||
			   i == (SceUInt32)ch->exclusiveResult.selectedConfigId) {
			NetCtlAutoScanCandidate *cand = &candidates[candCount];

			cand->configId = i;
			sceClibMemcpy_safe(cand->ssid, tempCfg.ssid, sizeof(cand->ssid));
			cand->security = tempCfg.wifiSecurity;
			cand->confType = tempCfg.confType;
			ssidPtrs[candCount] = cand->ssid;
			++candCount;
		}
	}

	if (ethPresent == 1 || candCount == 0) {
		if (netCtlCoreStartExclusiveDialog(ch, 3, reqPid, 0, &ch->exclusiveImposeShown) == 1) {
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 14;
		} else {
			netCtlTryPhoneFallback(ch, SCE_NET_CTL_ERROR_NO_SUITABLE_SETTING_FOR_AUTO_CONNECT,
					       outNextState, outEventCode);
		}
		netCtlFinishAutoSelect(ch, outNextState);
		return 0;
	}

	for (SceUInt32 attempt = 0; attempt < 3; ++attempt) {
		for (SceUInt32 i = 0; i < NETCTL_MAX_CONFIGS; ++i) {
			candidates[i].rssiPercentage = 0;
			candidates[i].channel = 0;
		}

		SceUInt32 scanned = 0;

		while (scanned < candCount) {
			SceUInt32 batch = candCount - scanned;

			if (batch > NETCTL_MAX_SCAN_SSIDS)
				batch = NETCTL_MAX_SCAN_SSIDS;

			int scanRet = netCtlPerformWlanScan(ch, &ssidPtrs[scanned], batch, NULL,
							    ch->autoScanBuf, sizeof(ch->autoScanBuf),
							    &ch->autoScanList, &ch->autoScanCount,
							    NULL, NULL, NULL);
			if (scanRet < 0) {
				ch->lastError = scanRet;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				*outEventCode = 9;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}

			SceUInt32 pollBits = 0;

			netCtlCorePollEvent(ch, NETCTL_EV_DISCONNECT | NETCTL_EV_AUTO_DISCONNECT,
					    &pollBits);
			if (pollBits != 0) {
				if ((pollBits & NETCTL_EV_DISCONNECT) != 0) {
					*outEventCode = 7;
				} else if ((pollBits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
					*outEventCode = 8;
				} else {
					ch->lastError = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
					*outEventCode = 9;
				}
				*outNextState = NETCTL_RAW_STATE_IDLE;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}

			int matchRet = netCtlMatchAutoScanCandidates(ch, &ssidPtrs[scanned], batch,
								     &candidates[scanned]);
			if (matchRet < 0) {
				ch->lastError = matchRet;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				*outEventCode = 9;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			scanned += batch;
		}

		int rankRet = netCtlRankAutoScanCandidates(ch, candidates, candCount);

		if (rankRet < 0) {
			ch->lastError = rankRet;
			*outNextState = NETCTL_RAW_STATE_IDLE;
			*outEventCode = 9;
			netCtlFinishAutoSelect(ch, outNextState);
			return 0;
		}

		for (SceUInt32 j = 0; j < ch->rankedCount; ++j) {
			int exists = 0;
			SceUInt32 id = ch->rankedConfigIds[j];
			int loadRet = netCtlConfigLoadToNetConfig(0, id, &tempCfg, &exists);

			if (loadRet < 0) {
				ch->lastError = loadRet;
				*outNextState = NETCTL_RAW_STATE_IDLE;
				*outEventCode = 9;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
			if (exists == 1 &&
			    (tempCfg.enableAutoConnect == 1 ||
			     sceClibStrncmp(ch->lastConnectedSsid, tempCfg.ssid, 33) == 0 ||
			     id == (SceUInt32)ch->exclusiveResult.selectedConfigId)) {
				sceClibMemcpy_safe(&ch->activeConfig, &tempCfg, sizeof(NetCtlNetConfig));
				*outNextState = NETCTL_RAW_STATE_LINK_UP;
				*outEventCode = 3;
				netCtlFinishAutoSelect(ch, outNextState);
				return 0;
			}
		}
	}

	if (netCtlCoreStartExclusiveDialog(ch, 3, reqPid, 0, &ch->exclusiveImposeShown) == 1) {
		*outNextState = NETCTL_RAW_STATE_IDLE;
		*outEventCode = 14;
	} else {
		netCtlTryPhoneFallback(ch, SCE_NET_CTL_ERROR_NO_SUITABLE_SETTING_FOR_AUTO_CONNECT,
				       outNextState, outEventCode);
	}
	netCtlFinishAutoSelect(ch, outNextState);
	return 0;
}

/* 0x8121638E */
static int netCtlRefreshInterfaceMac(NetCtlChannel *ch)
{
	NetCtlIfInfoCompact ifInfo;
	int ret = SceNetInternal_235DE96C(ch->ifId, &ifInfo, 1);

	if (ret >= 0)
		sceClibMemcpy_safe(&ch->etherAddr, &ifInfo.etherAddr, sizeof(SceNetEtherAddr));
	return ret;
}

static void netCtlTeardownToIdle(NetCtlChannel *ch, int notifyAutoConnect)
{
	ch->flags &= ~NETCTL_FLAG_RECONNECTING;
	netCtlCoreTeardownIp(ch);
	netCtlLinkDisconnect(ch);
	int lockRet = netCtlCoreLock();

	ch->flags |= NETCTL_FLAG_IDLE;
	ch->adhocOwnerPid = -1;
	if (lockRet >= 0)
		netCtlCoreUnlock();
	if (notifyAutoConnect)
		netCtlCoreNotifyAutoConnectResult(ch, SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED);
	ch->ifId = -1;
}

/* 0x81215B30 */
static int netCtlStateLinkUp(NetCtlChannel *ch, int prevState, int *outNextState, int *outEventCode)
{
	int ifId;

	if (ch->isInet != 0) {
		if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRED)
			ifId = NETCTL_IF_ETHERNET;
		else if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_PHONE)
			ifId = NETCTL_IF_PHONE;
		else
			ifId = NETCTL_IF_WLAN;
	} else {
		int ethPresent = SceNetInternal_235DE96C(NETCTL_IF_ETHERNET, NULL, 0);

		if (netCtlCoreHasEthernetCap() == 0 && ethPresent == 1) {
			ch->activeConfig.device = SCE_NET_CTL_DEVICE_WIRED;
			ifId = NETCTL_IF_ETHERNET;
		} else {
			ch->activeConfig.device = SCE_NET_CTL_DEVICE_WIRELESS;
			ifId = NETCTL_IF_WLAN;
		}
	}

	if (netCtlCoreClaimActiveOwner(ch) == 1) {
		netCtlCoreApplyPowerSave(ch, ifId);
		if (ch->isInet == 0) {
			SceUInt32 adhocSleepParam[5];

			sceClibMemset(adhocSleepParam, 0, sizeof(adhocSleepParam));
			adhocSleepParam[3] = 0xFFFFFFFFu;
			SceNetInternal_E03F6A77(ifId, adhocSleepParam, 0);
		}
	}

	if (ch->isInet != 0) {
		int useRanked = (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRELESS &&
				 prevState == NETCTL_RAW_STATE_AUTO_SELECT) ? 1 : 0;
		int ret = netCtlLinkConnect(ch, useRanked);

		if (ret >= 0) {
			sceClibMemcpy_safe(ch->lastConnectedSsid, ch->activeConfig.ssid,
					   sizeof(ch->lastConnectedSsid));
			netCtlRefreshInterfaceMac(ch);
			if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) != 0) {
				*outNextState = NETCTL_RAW_STATE_CONNECTED;
				ch->flags &= ~NETCTL_FLAG_RECONNECTING;
			} else {
				*outNextState = NETCTL_RAW_STATE_IP_CONFIG;
			}
			*outEventCode = 4;
			return 0;
		}

		*outNextState = NETCTL_RAW_STATE_IDLE;
		if (ret == NETCTL_ERR_DISCONNECT_REQUESTED) {
			*outEventCode = 7;
		} else if (ret == NETCTL_ERR_AUTO_DISCONNECT_REQ) {
			*outEventCode = 8;
		} else {
			*outEventCode = 9;
			ch->lastError = ret;
		}
		netCtlTeardownToIdle(ch, 1);
		return 0;
	}

	int ret = netCtlLinkConnect(ch, 0);

	if (ret >= 0) {
		netCtlRefreshInterfaceMac(ch);
		*outNextState = NETCTL_RAW_STATE_IP_CONFIG;
		*outEventCode = 4;
		return 0;
	}

	*outNextState = NETCTL_RAW_STATE_IDLE;
	if (ret == NETCTL_ERR_DISCONNECT_REQUESTED) {
		*outEventCode = 7;
	} else {
		*outEventCode = 9;
		ch->lastError = ret;
	}
	netCtlTeardownToIdle(ch, 1);
	return 0;
}

/* 0x81215D1A */
static int netCtlStateIpConfig(NetCtlChannel *ch, int prevState, int *outNextState,
			       int *outEventCode)
{
	int ret;

	(void)prevState;
	if (ch->isInet == 0 || ch->activeConfig.ipConfig == SCE_NET_CTL_IP_DHCP)
		ret = netCtlCoreConfigureDhcp(ch);
	else if (ch->activeConfig.ipConfig == SCE_NET_CTL_IP_STATIC)
		ret = netCtlCoreConfigureStaticIp(ch);
	else if (ch->activeConfig.ipConfig == SCE_NET_CTL_IP_PPPOE)
		ret = netCtlCoreConfigurePppoe(ch);
	else
		ret = NETCTL_ERR_INVALID_IP_CONFIG;

	if (ret >= 0) {
		netCtlCoreRefreshIpInfo(ch);
		netCtlConfigUpdateCurrentIp(ch->ipAddrStr, sizeof(ch->ipAddrStr));
		ch->flags = (ch->flags & ~NETCTL_FLAG_RECONNECTING) | NETCTL_FLAG_IP_OBTAINED;

		if (ch->isInet == 1) {
			NetCtlWisprStatus emptyStatus;

			sceClibMemset(&emptyStatus, 0, sizeof(emptyStatus));
			netCtlCoreSetWisprStatus(&emptyStatus);
			if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRELESS &&
			    sceClibStrncmp("attwifi", ch->activeConfig.ssid, 8) == 0 &&
			    ch->hostFuncs != NULL &&
			    ch->hostFuncs->getMobileNetworkName != NULL &&
			    ch->hostFuncs->getMobileNetworkName(ch->mobileNetName,
								sizeof(ch->mobileNetName)) >= 0 &&
			    ch->mobileNetName[0] != '\0') {
				ch->wisprIspId = 0;
				*outNextState = NETCTL_RAW_STATE_WISPR_AUTH;
				*outEventCode = 6;
				return 0;
			}
		}

		*outNextState = NETCTL_RAW_STATE_CONNECTED;
		*outEventCode = 6;
		netCtlCoreNotifyAutoConnectResult(ch, 0);
		return 0;
	}

	*outNextState = NETCTL_RAW_STATE_IDLE;
	if (ret == NETCTL_ERR_DISCONNECT_REQUESTED) {
		*outEventCode = 7;
	} else if (ret == NETCTL_ERR_AUTO_DISCONNECT_REQ) {
		*outEventCode = 8;
	} else {
		*outEventCode = 9;
		ch->lastError = ret;
	}

	if (*outEventCode == 9 &&
	    (ch->lastError == SCE_NET_CTL_ERROR_WIFI_DEAUTHED ||
	     ch->lastError == SCE_NET_CTL_ERROR_WIFI_BEACON_LOST)) {
		ch->flags |= NETCTL_FLAG_RECONNECTING;
		netCtlLinkDisconnect(ch);
		*outNextState = NETCTL_RAW_STATE_LINK_UP;
		*outEventCode = 10;
		return 0;
	}

	netCtlTeardownToIdle(ch, 1);
	return 0;
}

/* 0x81215EFE */
static int netCtlStateConnected(NetCtlChannel *ch, int prevState, int *outNextState,
				int *outEventCode)
{
	SceUInt32 bits = 0;

	(void)prevState;
	do {
		for (;;) {
			SceUInt32 waitMask = NETCTL_EV_DISCONNECT | NETCTL_EV_SCAN |
					     NETCTL_EV_LINK_ERROR;

			if (ch->isInet == 1)
				waitMask |= NETCTL_EV_AUTO_CONNECT | NETCTL_EV_AUTO_DISCONNECT |
					    NETCTL_EV_ADHOC_PREEMPT | NETCTL_EV_EXCLUSIVE_DONE |
					    NETCTL_EV_EXCLUSIVE_ABORT;

			bits = 0;
			netCtlCoreWaitEvent(ch, 0, waitMask, &bits, 0, NULL);

			if ((bits & NETCTL_EV_SCAN) != 0) {
				ch->scanError = netCtlRunPendingScan(ch);
				netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
				bits &= ~NETCTL_EV_SCAN;
				if (bits == 0)
					continue;
			}

			if ((bits & NETCTL_EV_AUTO_CONNECT) != 0) {
				netCtlCoreNotifyAutoConnectResult(ch, 0);
				bits &= ~NETCTL_EV_AUTO_CONNECT;
				if (bits == 0)
					continue;
			}

			if ((bits & NETCTL_EV_EXCLUSIVE_ABORT) == 0)
				break;

			ch->exclusiveRunning = 0;
			netCtlCoreNotifyAutoConnectResult(ch, 0);
			bits &= ~NETCTL_EV_EXCLUSIVE_ABORT;
			if (bits != 0)
				break;
		}

		if ((bits & NETCTL_EV_EXCLUSIVE_DONE) == 0)
			break;

		int failed = 0;

		if (ch->exclusiveResult.valid != 1) {
			failed = 1;
		} else {
			int lockRet = netCtlCoreLock();

			if (lockRet < 0) {
				ch->exclusiveError = lockRet;
				failed = 1;
			} else {
				SceKernelSysClock clock;
				int procRet = sceKernelGetRemoteProcessTime(
					ch->exclusiveResult.clientPid, &clock);

				if (procRet < 0) {
					netCtlCoreUnlock();
					ch->exclusiveError = procRet;
					failed = 1;
				} else {
					ch->flags |= NETCTL_FLAG_EXCLUSIVE_ACTIVE;
					netCtlCoreUnlock();
				}
			}
		}

		if (failed) {
			if (ch->exclusiveError == NETCTL_EXCL_ERR_BUSY)
				ch->exclusiveImposeShown = 0;
			ch->exclusiveRunning = 0;
			netCtlCoreNotifyAutoConnectResult(ch, 0);
		}
		bits &= ~NETCTL_EV_EXCLUSIVE_DONE;
	} while (bits == 0);

	if ((bits & NETCTL_EV_DISCONNECT) != 0) {
		*outEventCode = 7;
	} else if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0) {
		*outEventCode = 8;
	} else if ((bits & (NETCTL_EV_ADHOC_PREEMPT | NETCTL_EV_LINK_ERROR)) != 0) {
		if ((bits & NETCTL_EV_ADHOC_PREEMPT) != 0)
			ch->lastError = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
		if (ch->lastError == SCE_NET_CTL_ERROR_WIFI_DEAUTHED ||
		    ch->lastError == SCE_NET_CTL_ERROR_WIFI_BEACON_LOST) {
			ch->flags |= NETCTL_FLAG_RECONNECTING;
			netCtlLinkDisconnect(ch);
			*outNextState = NETCTL_RAW_STATE_LINK_UP;
			*outEventCode = 10;
			return 0;
		}
		*outEventCode = 9;
	}

	*outNextState = NETCTL_RAW_STATE_IDLE;
	netCtlTeardownToIdle(ch, 0);
	return 0;
}

/* 0x812160D6 */
static int netCtlStateWisprAuth(NetCtlChannel *ch, int prevState, int *outNextState,
				int *outEventCode)
{
	int ret = netCtlCoreRunWisprAuth(ch);

	(void)prevState;
	if (ret >= 0) {
		*outNextState = NETCTL_RAW_STATE_CONNECTED;
		*outEventCode = 12;
		netCtlCoreNotifyAutoConnectResult(ch, 0);
		return 0;
	}
	if (ret == NETCTL_ERR_WISPR_AUTH_FAILED) {
		*outNextState = NETCTL_RAW_STATE_CONNECTED;
		*outEventCode = 13;
		netCtlCoreNotifyAutoConnectResult(ch, 0);
		return 0;
	}

	*outNextState = NETCTL_RAW_STATE_IDLE;
	if (ret == NETCTL_ERR_DISCONNECT_REQUESTED) {
		*outEventCode = 7;
	} else if (ret == NETCTL_ERR_AUTO_DISCONNECT_REQ) {
		*outEventCode = 8;
	} else {
		*outEventCode = 9;
		ch->lastError = ret;
	}

	if (*outEventCode == 9 &&
	    (ch->lastError == SCE_NET_CTL_ERROR_WIFI_DEAUTHED ||
	     ch->lastError == SCE_NET_CTL_ERROR_WIFI_BEACON_LOST)) {
		ch->flags |= NETCTL_FLAG_RECONNECTING;
		netCtlLinkDisconnect(ch);
		*outNextState = NETCTL_RAW_STATE_LINK_UP;
		*outEventCode = 10;
		return 0;
	}

	netCtlTeardownToIdle(ch, 1);
	return 0;
}

/* 0x812161D0 */
int netCtlParseAsciiKey(SceUInt8 *outBuf, const char *src, SceUInt32 len)
{
	for (SceUInt32 i = 0; i < len; ++i) {
		if ((sceClibLookCtypeTable(src[i]) & 0x97) == 0)
			return NETCTL_ERR_INVALID_KEY_CHAR;
		outBuf[i] = (SceUInt8)src[i];
	}
	return 0;
}

static int netCtlHexNibble(char ch, SceUInt8 *outVal)
{
	if (ch >= '0' && ch <= '9') {
		*outVal = (SceUInt8)(ch - '0');
		return 0;
	}
	if (ch >= 'A' && ch <= 'F') {
		*outVal = (SceUInt8)(ch - 'A' + 10);
		return 0;
	}
	if (ch >= 'a' && ch <= 'f') {
		*outVal = (SceUInt8)(ch - 'a' + 10);
		return 0;
	}
	return NETCTL_ERR_INVALID_KEY_CHAR;
}

/* 0x8121620E */
int netCtlParseHexKey(SceUInt8 *outBuf, const char *src, SceUInt32 len)
{
	for (SceUInt32 i = 0; i < len; i += 2) {
		SceUInt8 hi = 0;
		SceUInt8 lo = 0;

		if (netCtlHexNibble(src[i], &hi) < 0 ||
		    netCtlHexNibble(src[i + 1], &lo) < 0)
			return NETCTL_ERR_INVALID_KEY_CHAR;
		outBuf[i / 2] = (SceUInt8)((hi << 4) | lo);
	}
	return 0;
}

/* 0x812162C4 */
int netCtlSetupLoopback(void)
{
	SceNetInAddr ip;
	SceNetInAddr mask;
	SceNetInAddr dst;
	SceNetInAddr gw;
	int ret = sceNetConfigUpInterface(NETCTL_IF_LOOPBACK);

	if (ret < 0)
		return ret;
	if (sceNetInetPton(SCE_NET_AF_INET, "127.0.0.1", &ip) <= 0 ||
	    sceNetInetPton(SCE_NET_AF_INET, "255.0.0.0", &mask) <= 0)
		return NETCTL_ERR_INVALID_IP_ADDR;

	ret = sceNetConfigSetIfaddr(NETCTL_IF_LOOPBACK, &ip, NULL, &mask);
	if (ret < 0)
		return ret;
	if (sceNetInetPton(SCE_NET_AF_INET, "127.0.0.0", &dst) <= 0 ||
	    sceNetInetPton(SCE_NET_AF_INET, "127.0.0.1", &gw) <= 0)
		return NETCTL_ERR_INVALID_IP_ADDR;
	return sceNetConfigAddRoute(&dst, &gw, &mask, 1);
}

/* 0x81216384 */
int netCtlTeardownLoopback(void)
{
	return sceNetConfigDownInterface(NETCTL_IF_LOOPBACK);
}

/* 0x812163D6 */
static int netCtlBringDownInterface(NetCtlChannel *ch, int ifId, int isScan)
{
	netCtlCoreFlushIfDownEvents(ch, ifId);

	SceUInt32 waitMask = (ifId == NETCTL_IF_WLAN)
				     ? (NETCTL_EV_WLAN_IF_DOWN | NETCTL_EV_WLAN_IF_ERR)
				     : (NETCTL_EV_OTHER_IF_DOWN | NETCTL_EV_OTHER_IF_ERR);

	sceKernelClearEventFlag(ch->eventFlagId, ~waitMask);

	int ret = sceNetConfigDownInterface(ifId);

	if (ret >= 0) {
		SceUInt32 bits = 0;

		do {
			bits = 0;
			SceUInt32 mask = (ifId != NETCTL_IF_WLAN) ? (waitMask | NETCTL_EV_SCAN) : waitMask;

			netCtlCoreWaitEvent(ch, isScan, mask, &bits, 0, NULL);
			if ((bits & NETCTL_EV_SCAN) == 0)
				break;
			ch->scanError = netCtlRunPendingScan(ch);
			netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
		} while ((bits & ~NETCTL_EV_SCAN) == 0);
	}
	return ret;
}

/* 0x81217286 */
static int netCtlBringUpInterface(NetCtlChannel *ch, int ifId, int isScan)
{
	netCtlCoreFlushIfUpEvents(ch, ifId);

	SceUInt32 waitMask = (ifId == NETCTL_IF_WLAN)
				     ? (NETCTL_EV_WLAN_IF_UP | NETCTL_EV_WLAN_IF_ERR)
				     : (NETCTL_EV_OTHER_IF_UP | NETCTL_EV_OTHER_IF_ERR);

	sceKernelClearEventFlag(ch->eventFlagId, ~waitMask);

	int ret = sceNetConfigUpInterface(ifId);

	if (ret >= 0) {
		SceUInt32 bits = 0;

		do {
			bits = 0;
			SceUInt32 mask = (ifId != NETCTL_IF_WLAN) ? (waitMask | NETCTL_EV_SCAN) : waitMask;

			netCtlCoreWaitEvent(ch, isScan, mask, &bits, 0, NULL);
			if ((bits & NETCTL_EV_SCAN) == 0)
				break;
			ch->scanError = netCtlRunPendingScan(ch);
			netCtlCoreNotifyCallbacks(ch, ch->state, ch->state, 0);
			bits &= ~NETCTL_EV_SCAN;
		} while (bits == 0);

		if ((bits & (NETCTL_EV_WLAN_IF_ERR | NETCTL_EV_OTHER_IF_ERR)) != 0)
			return (isScan == 1) ? ch->scanError : ch->lastError;
	}
	return ret;
}

/* 0x81216472 */
static int netCtlPerformWlanScan(NetCtlChannel *ch, const char **ssids, SceUInt32 ssidCount,
				 const NetCtlScanParam *param, void *outBuf, SceSize outBufSize,
				 void **outList, SceUInt32 *outCount, int *busyFlag,
				 SceUInt64 *outSysTime, SceUInt64 *outRtcTime)
{
	NetCtlWlanScanRequest req;
	int needUp = 0;
	int ret = netCtlCoreAcquireInterface(ch->isInet, NETCTL_IF_WLAN, &needUp);

	if (ret < 0)
		return ret;

	if (needUp != 1 || (ret = netCtlBringUpInterface(ch, NETCTL_IF_WLAN, 1)) >= 0) {
		sceClibMemset(&req, 0, sizeof(req));
		req.bssType = 1;
		for (int i = 0; i < 14; ++i)
			req.channels[i] = (SceUInt8)(i + 1);
		req.ssidCount = (SceUInt8)ssidCount;

		for (SceUInt32 i = 0; i < ssidCount; ++i) {
			SceSize len = sceClibStrnlen(ssids[i], 32);

			req.ssids[i].length = len;
			sceClibMemcpy_safe(req.ssids[i].bytes, ssids[i], len);
		}

		req.scanMode = 1;
		req.dwellMin = 100;
		req.dwellMax = 100;

		if (param != NULL) {
			if ((param->optionFlags & 2u) != 0 &&
			    param->customIeSize != 0 &&
			    param->customIeData != NULL &&
			    param->customIeSize <= NETCTL_CUSTOM_IE_MAX) {
				req.customIeSize = param->customIeSize;
				req.optionFlags |= 1u;
				sceClibMemcpy_safe(req.customIe, param->customIeData,
						   param->customIeSize);
			}
			if ((param->optionFlags & 1u) != 0) {
				req.optionFlags |= 2u;
				sceClibMemcpy_safe(req.channelMask, param->channelMask,
						   sizeof(req.channelMask));
			}
		}

		ret = netCtlCoreLock();
		if (ret >= 0) {
			if (busyFlag != NULL)
				*busyFlag = 1;
			netCtlCoreUnlock();

			ret = sceNetWlanScan(NETCTL_IF_WLAN, &req,
					     req.customIeSize + NETCTL_WLAN_SCAN_REQ_BASE_SIZE,
					     outBuf, outBufSize, outList, outCount);
			if (ret >= 0) {
				if (outSysTime != NULL)
					*outSysTime = sceKernelGetSystemTimeWide();
				if (outRtcTime != NULL)
					*outRtcTime = sceRtcGetAccumulativeTime();
			} else {
				*outCount = 0;
			}

			int lockRet = netCtlCoreLock();

			if (busyFlag != NULL)
				*busyFlag = 0;
			if (lockRet >= 0)
				netCtlCoreUnlock();
		}
	}

	int needDown = 0;

	if (netCtlCoreReleaseInterface(ch->isInet, NETCTL_IF_WLAN, &needDown) >= 0 && needDown == 1)
		netCtlBringDownInterface(ch, NETCTL_IF_WLAN, 1);

	return ret;
}

/* 0x812166BC */
int netCtlRunPendingScan(NetCtlChannel *ch)
{
	const char *ssidPtrs[NETCTL_MAX_SCAN_SSIDS];
	SceUInt32 count = ch->scanSsidCount;

	for (SceUInt32 i = 0; i < count; ++i)
		ssidPtrs[i] = ch->scanSsids[i];

	void *buf;
	SceSize bufSize;
	void **outList;
	SceUInt32 *outCount;
	int *busyFlag;
	SceUInt64 *sysTime;
	SceUInt64 *rtcTime;

	if (count == 0) {
		buf = ch->bcastScanBuf;
		bufSize = sizeof(ch->bcastScanBuf);
		outList = &ch->bcastScanList;
		outCount = &ch->bcastScanCount;
		busyFlag = &ch->bcastScanBusy;
		sysTime = &ch->bcastScanSysTime;
		rtcTime = &ch->bcastScanRtcTime;
	} else if (count == 1) {
		buf = ch->singleScanBuf;
		bufSize = sizeof(ch->singleScanBuf);
		outList = &ch->singleScanList;
		outCount = &ch->singleScanCount;
		busyFlag = &ch->singleScanBusy;
		sysTime = &ch->singleScanSysTime;
		rtcTime = &ch->singleScanRtcTime;
	} else {
		buf = ch->multiScanBuf;
		bufSize = sizeof(ch->multiScanBuf);
		outList = &ch->multiScanList;
		outCount = &ch->multiScanCount;
		busyFlag = &ch->multiScanBusy;
		sysTime = &ch->multiScanSysTime;
		rtcTime = &ch->multiScanRtcTime;
	}

	int ret = netCtlPerformWlanScan(ch, ssidPtrs, count, &ch->scanParam,
					buf, bufSize, outList, outCount,
					busyFlag, sysTime, rtcTime);
	int lockRet = netCtlCoreLock();

	ch->scanInProgress = 0;
	if (lockRet >= 0)
		netCtlCoreUnlock();
	return ret;
}

/* 0x812167D0 */
static int netCtlClassifyWpaSecurity(int isRsn, const NetCtlWlanCipherInfo *cipher,
				     SceUInt32 *outSecurity)
{
	if ((cipher->pairwiseCipher & 8u) != 0 && (cipher->akmSuite & 2u) != 0)
		*outSecurity = (isRsn == 1) ? SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES
					    : SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES;
	else if ((cipher->pairwiseCipher & 4u) != 0 && (cipher->akmSuite & 2u) != 0)
		*outSecurity = (isRsn == 1) ? SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP
					    : SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP;
	else if ((cipher->groupCipher & 3u) != 0)
		*outSecurity = SCE_NET_CTL_WIFI_SECURITY_WEP;
	else
		*outSecurity = SCE_NET_CTL_WIFI_SECURITY_UNSUPPORTED;
	return 0;
}

static void netCtlResolveBssSecurity(const NetCtlWlanBssRecord *rec, SceUInt32 *outSecurity)
{
	if ((rec->capability & NETCTL_WLAN_CAP_PRIVACY) == 0) {
		*outSecurity = SCE_NET_CTL_WIFI_SECURITY_NOAUTH;
	} else if (rec->rsn.present == 1) {
		netCtlClassifyWpaSecurity(1, &rec->rsn, outSecurity);
	} else if (rec->wpa.present == 1) {
		netCtlClassifyWpaSecurity(0, &rec->wpa, outSecurity);
	} else {
		*outSecurity = SCE_NET_CTL_WIFI_SECURITY_WEP;
	}
}

/* 0x8121682C */
int netCtlGetScanInfoByIndex(NetCtlChannel *ch, SceUInt32 index, NetCtlScanInfo *outInfo,
			     void *outIe, SceSize *ioIeSize, const void *bssList,
			     int busyFlag, int unused, SceUInt64 sysTime, SceUInt64 rtcTime)
{
	(void)ch;
	(void)unused;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (busyFlag == 1)
		ret = NETCTL_ERR_SCAN_IN_PROGRESS;
	netCtlCoreUnlock();
	if (ret < 0)
		return ret;

	const NetCtlWlanBssRecord *rec = (const NetCtlWlanBssRecord *)bssList;

	for (SceUInt32 i = 0; i < index; ++i)
		rec = netCtlNextBssRecord(rec);

	sceClibMemset(outInfo, 0, sizeof(NetCtlScanInfo));
	outInfo->systemTime = sysTime;
	outInfo->accumulativeTime = rtcTime;
	sceClibMemcpy_safe(&outInfo->bssid, &rec->bssid, sizeof(SceNetEtherAddr));
	sceClibMemcpy_safe(outInfo->ssid, rec->ssid, sizeof(outInfo->ssid));
	outInfo->ssidLen = (rec->ssidLen <= 33) ? rec->ssidLen : 33;
	outInfo->rssiDbm = rec->rssiDbm;
	outInfo->rssiPercentage = rec->rssiPercentage;
	outInfo->channel = rec->channel;
	netCtlResolveBssSecurity(rec, &outInfo->security);

	if (outIe != NULL && ioIeSize != NULL && *ioIeSize != 0) {
		SceSize copyLen = rec->ieLength;

		if (*ioIeSize < copyLen)
			copyLen = *ioIeSize;
		sceClibMemcpy_safe(outIe, netCtlBssIeBytes(rec), copyLen);
		*ioIeSize = copyLen;
	}
	return 0;
}

/* 0x81216964 */
int netCtlGetScanBssidList(NetCtlChannel *ch, SceNetEtherAddr *outList, SceUInt32 maxCount,
			   const void *bssList, SceUInt32 bssCount, int busyFlag)
{
	(void)ch;
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (busyFlag == 1)
		ret = NETCTL_ERR_SCAN_IN_PROGRESS;
	netCtlCoreUnlock();
	if (ret < 0)
		return ret;

	const NetCtlWlanBssRecord *rec = (const NetCtlWlanBssRecord *)bssList;
	SceUInt32 copied = 0;

	while (copied < bssCount && copied < maxCount) {
		sceClibMemcpy_safe(&outList[copied], &rec->bssid, sizeof(SceNetEtherAddr));
		rec = netCtlNextBssRecord(rec);
		++copied;
	}
	return (int)copied;
}

/* 0x812169C6 */
int netCtlGetScanInfoByBssid(NetCtlChannel *ch, const SceNetEtherAddr *bssid,
			     NetCtlScanInfo *outInfo, void *outIe, SceSize *ioIeSize,
			     const void *bssList, SceUInt32 bssCount, int busyFlag,
			     SceUInt64 sysTime, SceUInt64 rtcTime)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (busyFlag == 1)
		ret = NETCTL_ERR_SCAN_IN_PROGRESS;
	netCtlCoreUnlock();
	if (ret < 0)
		return ret;

	const NetCtlWlanBssRecord *rec = (const NetCtlWlanBssRecord *)bssList;

	for (SceUInt32 i = 0; i < bssCount; ++i) {
		if (sceClibMemcmp(bssid, &rec->bssid, sizeof(SceNetEtherAddr)) == 0)
			return netCtlGetScanInfoByIndex(ch, i, outInfo, outIe, ioIeSize,
							bssList, busyFlag, 0, sysTime, rtcTime);
		rec = netCtlNextBssRecord(rec);
	}
	return NETCTL_ERR_BSSID_NOT_FOUND;
}

/* 0x81216A80 */
int netCtlGetSingleSsidScanInfo(NetCtlChannel *ch, NetCtlScanInfo *outInfo, void *outIe,
				SceSize *ioIeSize)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;
	if (ch->singleScanBusy == 1)
		ret = NETCTL_ERR_SCAN_IN_PROGRESS;
	netCtlCoreUnlock();
	if (ret < 0)
		return ret;

	if (ch->singleScanCount == 0)
		return NETCTL_ERR_SCAN_NO_RESULT;

	return netCtlGetScanInfoByIndex(ch, 0, outInfo, outIe, ioIeSize,
					ch->singleScanList, ch->singleScanBusy, 0,
					ch->singleScanSysTime, ch->singleScanRtcTime);
}

/* 0x81216B0A */
static int netCtlMatchAutoScanCandidates(NetCtlChannel *ch, const char **ssids, SceUInt32 count,
					 NetCtlAutoScanCandidate *candidates)
{
	const NetCtlWlanBssRecord *rec = (const NetCtlWlanBssRecord *)ch->autoScanList;

	for (SceUInt32 i = 0; i < ch->autoScanCount; ++i) {
		for (SceUInt32 j = 0; j < count; ++j) {
			if (rec->ssidLen == sceClibStrnlen(ssids[j], 32) &&
			    sceClibMemcmp(rec->ssid, ssids[j], rec->ssidLen) == 0 &&
			    candidates[j].channel == 0) {
				candidates[j].rssiPercentage = rec->rssiPercentage;
				candidates[j].channel = rec->channel;
				break;
			}
		}
		rec = netCtlNextBssRecord(rec);
	}
	return 0;
}

/* 0x81216BB0 */
static int netCtlRankAutoScanCandidates(NetCtlChannel *ch, NetCtlAutoScanCandidate *candidates,
					SceUInt32 count)
{
	int bestByType[NETCTL_MAX_CONFIGS + 1];

	sceClibMemset(ch->rankedConfigIds, 0, sizeof(ch->rankedConfigIds));
	sceClibMemset(ch->rankedChannels, 0, sizeof(ch->rankedChannels));
	ch->rankedCount = 0;

	do {
		sceClibMemset(bestByType, -1, sizeof(bestByType));
		NetCtlAutoScanCandidate *best = &candidates[0];

		for (SceUInt32 i = 0; i < count; ++i) {
			SceUInt32 confType = candidates[i].confType;
			SceUInt32 rssi = candidates[i].rssiPercentage;

			if (confType != 0 && confType <= NETCTL_MAX_CONFIGS && rssi != 0) {
				int prevIdx = bestByType[confType];

				if (prevIdx < 0) {
					bestByType[confType] = (int)i;
				} else {
					SceUInt32 prevSec = candidates[prevIdx].security;
					SceUInt32 curSec = candidates[i].security;

					if (prevSec < curSec ||
					    (prevSec == curSec &&
					     candidates[prevIdx].rssiPercentage < rssi))
						bestByType[confType] = (int)i;
				}
			}

			if (best->rssiPercentage < rssi)
				best = &candidates[i];
		}

		if (best->confType != 0 && best->confType <= NETCTL_MAX_CONFIGS &&
		    bestByType[best->confType] >= 0)
			best = &candidates[bestByType[best->confType]];

		if (best->rssiPercentage == 0)
			break;

		best->rssiPercentage = 0;
		for (SceUInt32 k = 0; k < count; ++k) {
			if (ch->rankedConfigIds[k] == 0) {
				ch->rankedConfigIds[k] = best->configId;
				ch->rankedChannels[k] = best->channel;
				ch->rankedCount = k + 1;
				break;
			}
		}
	} while (ch->rankedCount < count);

	return 0;
}

/* 0x81216D2C */
int netCtlInetGetInfoImpl(NetCtlChannel *ch, int state, int code, void *outInfo)
{
	SceNetCtlInfo *info = (SceNetCtlInfo *)outInfo;

	if ((ch->flags & NETCTL_FLAG_IDLE) != 0 || state == NETCTL_RAW_STATE_IDLE)
		return SCE_NET_CTL_ERROR_NOT_CONNECTED;
	if (ch->connectMode == 0 && state <= NETCTL_RAW_STATE_LINK_UP)
		return SCE_NET_CTL_ERROR_NOT_CONNECTED;

	switch (code) {
	case SCE_NET_CTL_INFO_CNF_NAME:
		sceClibMemcpy_safe(info->cnf_name, ch->activeConfig.cnfName, sizeof(info->cnf_name));
		return 0;

	case SCE_NET_CTL_INFO_DEVICE:
		if (ch->fakeMobileIf > 0)
			info->device = SCE_NET_CTL_DEVICE_PHONE;
		else
			info->device = ch->activeConfig.device;
		return 0;

	case SCE_NET_CTL_INFO_ETHER_ADDR:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		if (ch->fakeMobileIf > 0 || ch->ifId == NETCTL_IF_PHONE)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		sceClibMemcpy_safe(&info->ether_addr, &ch->etherAddr, sizeof(SceNetEtherAddr));
		return 0;

	case SCE_NET_CTL_INFO_MTU:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		info->mtu = ch->activeMtu;
		return 0;

	case SCE_NET_CTL_INFO_LINK:
		info->link = ((ch->flags & NETCTL_FLAG_LINK_UP) != 0)
				     ? SCE_NET_CTL_LINK_CONNECTED
				     : SCE_NET_CTL_LINK_DISCONNECTED;
		return 0;

	case SCE_NET_CTL_INFO_BSSID:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		if (ch->fakeMobileIf > 0 || ch->ifId != NETCTL_IF_WLAN)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		sceClibMemcpy_safe(&info->bssid, &ch->connectedBss.bssid, sizeof(SceNetEtherAddr));
		return 0;

	case SCE_NET_CTL_INFO_SSID:
		if (ch->fakeMobileIf > 0 || ch->ifId != NETCTL_IF_WLAN)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		sceClibMemcpy_safe(info->ssid, ch->activeConfig.ssid, sizeof(info->ssid));
		return 0;

	case SCE_NET_CTL_INFO_WIFI_SECURITY:
		if (ch->fakeMobileIf > 0 || ch->ifId != NETCTL_IF_WLAN)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (state == NETCTL_RAW_STATE_LINK_UP)
			info->wifi_security = ch->activeConfig.wifiSecurity;
		else
			info->wifi_security = (SceUInt)ch->resolvedWifiSecurity;
		return 0;

	case SCE_NET_CTL_INFO_RSSI_DBM:
	case SCE_NET_CTL_INFO_RSSI_PERCENTAGE: {
		NetCtlWlanRssiInfo rssi;

		if (ch->fakeMobileIf > 0 || ch->ifId != NETCTL_IF_WLAN)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		int ret = sceNetWlanGetRssi(NETCTL_IF_WLAN, &rssi);

		if (ret < 0)
			return ret;
		if (code == SCE_NET_CTL_INFO_RSSI_DBM)
			info->rssi_dbm = rssi.rssiDbm;
		else
			info->rssi_percentage = rssi.rssiPercentage;
		return 0;
	}

	case SCE_NET_CTL_INFO_CHANNEL:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		if (ch->fakeMobileIf > 0 || ch->ifId != NETCTL_IF_WLAN)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		info->channel = ch->connectedBss.channel;
		return 0;

	case SCE_NET_CTL_INFO_IP_CONFIG:
		info->ip_config = ch->activeConfig.ipConfig;
		return 0;

	case SCE_NET_CTL_INFO_DHCP_HOSTNAME:
		sceClibMemcpy_safe(info->dhcp_hostname, ch->activeConfig.dhcpHostname,
				   sizeof(info->dhcp_hostname));
		return 0;

	case SCE_NET_CTL_INFO_PPPOE_AUTH_NAME:
		sceClibMemcpy_safe(info->pppoe_auth_name, ch->activeConfig.pppoeAuthName,
				   sizeof(info->pppoe_auth_name));
		return 0;

	case SCE_NET_CTL_INFO_IP_ADDRESS:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->ipAddr, info->ip_address,
				   sizeof(info->ip_address)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_NETMASK:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->netmask, info->netmask,
				   sizeof(info->netmask)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_DEFAULT_ROUTE:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->defaultRoute, info->default_route,
				   sizeof(info->default_route)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_PRIMARY_DNS:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->primaryDnsAddr, info->primary_dns,
				   sizeof(info->primary_dns)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_SECONDARY_DNS:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->secondaryDnsAddr, info->secondary_dns,
				   sizeof(info->secondary_dns)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_HTTP_PROXY_CONFIG:
		info->http_proxy_config = ch->activeConfig.httpProxyFlag;
		return 0;

	case SCE_NET_CTL_INFO_HTTP_PROXY_SERVER:
		sceClibMemcpy_safe(info->http_proxy_server, ch->activeConfig.httpProxyServer,
				   sizeof(info->http_proxy_server));
		return 0;

	case SCE_NET_CTL_INFO_HTTP_PROXY_PORT:
		info->http_proxy_port = ch->activeConfig.httpProxyPort;
		return 0;

	case SCE_NET_CTL_INFO_RESERVED1:
		*(SceUInt32 *)outInfo = ch->activeConfig.confType;
		return 0;

	default:
		return SCE_NET_CTL_ERROR_INVALID_CODE;
	}
}

/* 0x8121712C */
int netCtlAdhocGetInfoImpl(NetCtlChannel *ch, int state, int code, void *outInfo)
{
	SceNetCtlInfo *info = (SceNetCtlInfo *)outInfo;

	if ((ch->flags & NETCTL_FLAG_IDLE) != 0 || state == NETCTL_RAW_STATE_IDLE)
		return SCE_NET_CTL_ERROR_NOT_CONNECTED;

	switch (code) {
	case SCE_NET_CTL_INFO_ETHER_ADDR:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		sceClibMemcpy_safe(&info->ether_addr, &ch->etherAddr, sizeof(SceNetEtherAddr));
		return 0;

	case SCE_NET_CTL_INFO_BSSID:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		sceClibMemcpy_safe(&info->bssid, &ch->connectedBss.bssid, sizeof(SceNetEtherAddr));
		return 0;

	case SCE_NET_CTL_INFO_SSID:
		sceClibMemset(info->ssid, 0, sizeof(info->ssid));
		sceClibMemcpy_safe(info->ssid, ch->connectedBss.ssid, 32);
		return 0;

	case SCE_NET_CTL_INFO_CHANNEL:
		if (state != NETCTL_RAW_STATE_IP_CONFIG &&
		    state != NETCTL_RAW_STATE_CONNECTED &&
		    state != NETCTL_RAW_STATE_WISPR_AUTH)
			return SCE_NET_CTL_ERROR_NOT_CONNECTED;
		info->channel = ch->connectedBss.channel;
		return 0;

	case SCE_NET_CTL_INFO_IP_ADDRESS:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->ipAddr, info->ip_address,
				   sizeof(info->ip_address)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	case SCE_NET_CTL_INFO_NETMASK:
		if ((ch->flags & NETCTL_FLAG_IP_OBTAINED) == 0)
			return SCE_NET_CTL_ERROR_NOT_AVAIL;
		if (sceNetInetNtop(SCE_NET_AF_INET, &ch->netmask, info->netmask,
				   sizeof(info->netmask)) == NULL)
			return NETCTL_ERR_INVALID_IP_ADDR;
		return 0;

	default:
		return SCE_NET_CTL_ERROR_INVALID_CODE;
	}
}

/* 0x81217332 */
static int netCtlWlanConnectOnce(NetCtlChannel *ch)
{
	NetCtlWlanConnectRequest req;
	SceUInt8 outBuf[2240];
	void *outBssPtr = NULL;
	int sharedKeyRetry = 0;
	int ret;

	for (;;) {
		int isWep = 0;

		sceClibMemset(&req, 0, sizeof(req));
		switch (ch->activeConfig.wifiSecurity) {
		case SCE_NET_CTL_WIFI_SECURITY_WEP:
			req.authMode = (sharedKeyRetry != 0) ? 1u : 0u;
			req.securityMode = 1;
			req.wepKeyType = (ch->parsedKeyLen == 5) ? 1u : 2u;
			for (int k = 0; k < 4; ++k)
				sceClibMemcpy_safe(req.wepKeys[k], ch->parsedKey, ch->parsedKeyLen);
			isWep = 1;
			break;
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK:
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP:
		case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES:
		case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP:
		case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES:
			req.authMode = 0;
			req.securityMode = 2;
			sceClibMemcpy_safe(req.wpaKey, ch->parsedKey, ch->parsedKeyLen);
			req.wpaKeyFormat = (ch->parsedKeyIsHex != 0) ? 2u : 1u;
			break;
		default:
			req.authMode = 0;
			req.securityMode = 0;
			break;
		}

		req.ssidLen = sceClibStrnlen(ch->activeConfig.ssid, 32);
		sceClibMemcpy_safe(req.ssid, ch->activeConfig.ssid, req.ssidLen);
		req.channel = ch->activeConfig.channel;

		if (ch->connectMode == 2) {
			if (ch->activeConfig.customIeProbeSize != 0 &&
			    ch->activeConfig.customIeProbe != NULL &&
			    ch->activeConfig.customIeProbeSize < NETCTL_CUSTOM_IE_MAX) {
				req.optionFlags |= 1u;
				req.customIeProbeSize = ch->activeConfig.customIeProbeSize;
				sceClibMemcpy_safe(req.customIePayload,
						   ch->activeConfig.customIeProbe,
						   req.customIeProbeSize);
			}
			if (ch->activeConfig.customIeAssocSize != 0 &&
			    ch->activeConfig.customIeAssoc != NULL &&
			    ch->activeConfig.customIeAssocSize < NETCTL_CUSTOM_IE_MAX) {
				req.optionFlags |= 2u;
				req.customIeAssocSize = ch->activeConfig.customIeAssocSize;
				sceClibMemcpy_safe(&req.customIePayload[req.customIeProbeSize],
						   ch->activeConfig.customIeAssoc,
						   req.customIeAssocSize);
			}
		}

		ret = sceNetWlanConnect(NETCTL_IF_WLAN, &req, outBuf, sizeof(outBuf), &outBssPtr);
		if (isWep != 1 || ret != NETCTL_WLAN_ERR_WEP_AUTH_FAIL || sharedKeyRetry != 0)
			break;
		sharedKeyRetry = 1;
	}

	if (ret >= 0) {
		const NetCtlWlanBssRecord *bss = (const NetCtlWlanBssRecord *)outBssPtr;
		SceUInt32 resolved = 0;

		netCtlResolveBssSecurity(bss, &resolved);
		ch->resolvedWifiSecurity = (int)resolved;
		sceClibMemcpy_safe(&ch->connectedBss, bss, sizeof(NetCtlWlanBssRecord));
		return 0;
	}
	return ret;
}

/* 0x812175EC */
static int netCtlWlanConnectWithRetry(NetCtlChannel *ch)
{
	int ret = netCtlCoreParseActiveKey(ch);

	if (ret < 0)
		return ret;

	for (unsigned int attempt = 0; attempt < 4; ++attempt) {
		ret = netCtlWlanConnectOnce(ch);
		if (ret != NETCTL_WLAN_ERR_SCAN_RETRY)
			break;

		SceUInt32 bits = 0;

		netCtlCorePollEvent(ch, NETCTL_EV_DISCONNECT | NETCTL_EV_AUTO_DISCONNECT, &bits);
		if (bits != 0) {
			if ((bits & NETCTL_EV_DISCONNECT) != 0)
				return NETCTL_ERR_DISCONNECT_REQUESTED;
			if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0)
				return NETCTL_ERR_AUTO_DISCONNECT_REQ;
			return SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
		}
	}
	return ret;
}

/* 0x8121767C */
static int netCtlIsBssidLower(const SceNetEtherAddr *lhs, const SceNetEtherAddr *rhs)
{
	SceUInt32 lhsHi = ((SceUInt32)lhs->data[0] << 16) |
			  ((SceUInt32)lhs->data[1] << 8) | lhs->data[2];
	SceUInt32 rhsHi = ((SceUInt32)rhs->data[0] << 16) |
			  ((SceUInt32)rhs->data[1] << 8) | rhs->data[2];

	if (lhsHi < rhsHi)
		return 1;
	if (lhsHi > rhsHi)
		return 0;

	SceUInt32 lhsLo = ((SceUInt32)lhs->data[3] << 16) |
			  ((SceUInt32)lhs->data[4] << 8) | lhs->data[5];
	SceUInt32 rhsLo = ((SceUInt32)rhs->data[3] << 16) |
			  ((SceUInt32)rhs->data[4] << 8) | rhs->data[5];

	return (lhsLo < rhsLo) ? 1 : 0;
}

static void netCtlInitAdhocScanReq(NetCtlWlanScanRequest *req, SceUInt32 channel,
				   const char *ssid, SceSize ssidLen, int isPspMode)
{
	sceClibMemset(req, 0, NETCTL_WLAN_SCAN_REQ_BASE_SIZE);
	req->scanMode = 1;
	req->channels[0] = (SceUInt8)channel;
	req->channels[1] = 0;
	req->bssType = NETCTL_WLAN_BSS_TYPE_IBSS;
	if (isPspMode != 0) {
		req->ssidCount = 0;
	} else {
		req->ssidCount = 1;
		req->ssids[0].length = ssidLen;
		sceClibMemcpy_safe(req->ssids[0].bytes, ssid, ssidLen);
	}
	req->dwellMin = 6;
	req->dwellMax = 100;
}

static const NetCtlWlanBssRecord *netCtlFindLowestAdhocBss(const void *bssList, SceUInt32 count,
							   const char *ssid, SceSize ssidLen,
							   const SceNetEtherAddr *ceilBssid)
{
	const NetCtlWlanBssRecord *best = NULL;
	const NetCtlWlanBssRecord *cur = (const NetCtlWlanBssRecord *)bssList;

	for (SceUInt32 i = 0; i < count; ++i) {
		if (cur->ssidLen == ssidLen &&
		    sceClibMemcmp(cur->ssid, ssid, ssidLen) == 0 &&
		    cur->bssType == NETCTL_WLAN_BSS_TYPE_IBSS) {
			if (best != NULL) {
				if (netCtlIsBssidLower(&cur->bssid, &best->bssid))
					best = cur;
			} else if (ceilBssid == NULL ||
				   netCtlIsBssidLower(&cur->bssid, ceilBssid)) {
				best = cur;
			}
		}
		cur = netCtlNextBssRecord(cur);
	}
	return best;
}

/* 0x812176D2 */
static int netCtlAdhocStartOrMerge(int ifId, const char *ssid, SceSize ssidLen,
				   SceUInt32 channel, NetCtlWlanBssRecord *outBss, int isPspMode)
{
	NetCtlWlanScanRequest scanReq;
	NetCtlWlanAdhocRequest adhocReq;
	SceUInt8 scanOutBuf[2240];
	void *bssList = NULL;
	SceUInt32 bssCount = 0;

	if (channel == 0 || channel > 14)
		return NETCTL_ERR_INVALID_CHANNEL;
	if (ssid == NULL)
		return NETCTL_ERR_INVALID_SSID;
	if (ssidLen == 0)
		return NETCTL_ERR_INVALID_SSID_COUNT;

	netCtlInitAdhocScanReq(&scanReq, channel, ssid, ssidLen, isPspMode);

	SceUInt32 tStart = sceKernelGetProcessTimeLow();
	int ret = sceNetWlanScan(ifId, &scanReq, NETCTL_WLAN_SCAN_REQ_BASE_SIZE,
				 scanOutBuf, sizeof(scanOutBuf), &bssList, &bssCount);
	if (ret < 0)
		return ret;

	SceUInt32 elapsed = sceKernelGetProcessTimeLow() - tStart;
	const NetCtlWlanBssRecord *best =
		(bssCount != 0) ? netCtlFindLowestAdhocBss(bssList, bssCount, ssid, ssidLen, NULL)
				: NULL;

	if (best != NULL) {
		sceClibMemset(&adhocReq, 0, sizeof(adhocReq));
		sceClibMemcpy_safe(&adhocReq.bss, best, sizeof(NetCtlWlanBssRecord));
		if (isPspMode == 1)
			adhocReq.flags |= 3u;
		sceClibMemcpy_safe(outBss, &adhocReq.bss, sizeof(NetCtlWlanBssRecord));
		ret = sceNetWlanAdhocJoin(ifId, &adhocReq);
	} else {
		static const SceUInt8 s_pspRates[4] = { 0x82, 0x84, 0x0B, 0x16 };

		sceClibMemset(&adhocReq, 0, sizeof(adhocReq));
		adhocReq.bss.ssidLen = ssidLen;
		sceClibMemcpy_safe(adhocReq.bss.ssid, ssid, ssidLen);
		adhocReq.bss.channel = (SceUInt8)channel;
		adhocReq.bss.bssType = NETCTL_WLAN_BSS_TYPE_IBSS;
		adhocReq.bss.capability = NETCTL_WLAN_CAP_IBSS;
		adhocReq.bss.beaconInterval = 100;
		if (isPspMode == 1) {
			sceClibMemcpy_safe(adhocReq.bss.supportedRates, s_pspRates,
					   sizeof(s_pspRates));
			adhocReq.flags |= 3u;
		}
		ret = sceNetWlanAdhocCreate(ifId, &adhocReq);
		if (ret == 0)
			sceKernelDelayThread(500000);
		sceClibMemcpy_safe(outBss, &adhocReq.bss, sizeof(NetCtlWlanBssRecord));
	}

	if (ret < 0) {
		sceNetWlanAdhocLeave(ifId);
		return ret;
	}

	SceUInt32 remainWindow = (elapsed < 1000000u) ? (1000000u - elapsed) : 0;

	for (int round = 0; round < 3; ++round) {
		SceUInt32 jitter = 0;

		if (elapsed < 1000000u) {
			unsigned int randVal = 0;

			SceNetInternal_689B9D7D(&randVal);
			SceUInt32 divisor = (0xFFFFFFFFu / remainWindow) + 1u;

			jitter = (SceUInt32)randVal / divisor;
			sceKernelDelayThread(jitter);
		}

		netCtlInitAdhocScanReq(&scanReq, channel, ssid, ssidLen, isPspMode);
		ret = sceNetWlanScan(ifId, &scanReq, NETCTL_WLAN_SCAN_REQ_BASE_SIZE,
				     scanOutBuf, sizeof(scanOutBuf), &bssList, &bssCount);
		if (ret < 0) {
			sceNetWlanAdhocLeave(ifId);
			return ret;
		}

		const NetCtlWlanBssRecord *better =
			(bssCount != 0)
				? netCtlFindLowestAdhocBss(bssList, bssCount,
							   outBss->ssid, outBss->ssidLen,
							   &outBss->bssid)
				: NULL;

		if (better != NULL) {
			ret = sceNetWlanAdhocLeave(ifId);
			if (ret < 0) {
				sceNetWlanAdhocLeave(ifId);
				return ret;
			}
			sceKernelDelayThread(3000000);
			sceClibMemset(&adhocReq, 0, sizeof(adhocReq));
			sceClibMemcpy_safe(&adhocReq.bss, better, sizeof(NetCtlWlanBssRecord));
			if (isPspMode == 1)
				adhocReq.flags |= 3u;
			sceClibMemcpy_safe(outBss, &adhocReq.bss, sizeof(NetCtlWlanBssRecord));
			ret = sceNetWlanAdhocJoin(ifId, &adhocReq);
			if (ret < 0) {
				sceNetWlanAdhocLeave(ifId);
				return ret;
			}
		} else if (round < 2 && elapsed < 1000000u) {
			sceKernelDelayThread(1000000u - elapsed - jitter);
		}
	}
	return 0;
}

/* 0x81217B50 */
static int netCtlAdhocConnectWlan(NetCtlChannel *ch)
{
	SceSize ssidLen = sceClibStrnlen(ch->adhocSsid, sizeof(ch->adhocSsid));
	SceUInt32 channel = (SceUInt32)ch->adhocChannel;

	if (ch->connectMode == 5) {
		NetCtlWlanAdhocRequest adhocReq;
		static const SceUInt8 s_pspRates[4] = { 0x82, 0x84, 0x0B, 0x16 };

		if (channel == 0 || channel > 14)
			return NETCTL_ERR_INVALID_CHANNEL;
		if (ssidLen == 0)
			return NETCTL_ERR_INVALID_SSID_COUNT;

		sceClibMemset(&adhocReq, 0, sizeof(adhocReq));
		adhocReq.bss.ssidLen = ssidLen;
		sceClibMemcpy_safe(adhocReq.bss.ssid, ch->adhocSsid, ssidLen);
		adhocReq.bss.channel = (SceUInt8)channel;
		adhocReq.bss.bssType = NETCTL_WLAN_BSS_TYPE_IBSS;
		adhocReq.bss.capability = NETCTL_WLAN_CAP_IBSS;
		adhocReq.bss.beaconInterval = 100;
		sceClibMemcpy_safe(adhocReq.bss.supportedRates, s_pspRates, sizeof(s_pspRates));
		adhocReq.flags |= 3u;

		int ret = sceNetWlanAdhocCreate(NETCTL_IF_WLAN, &adhocReq);

		if (ret < 0)
			return ret;
		if (ret == 0)
			sceKernelDelayThread(500000);
		sceClibMemcpy_safe(&ch->connectedBss, &adhocReq.bss, sizeof(NetCtlWlanBssRecord));
		return 0;
	}

	if (ch->connectMode == 6) {
		NetCtlWlanScanRequest scanReq;
		NetCtlWlanAdhocRequest adhocReq;
		SceUInt8 scanOutBuf[2240];
		void *bssList = NULL;
		SceUInt32 bssCount = 0;

		if (channel == 0 || channel > 14)
			return NETCTL_ERR_INVALID_CHANNEL;
		if (ssidLen == 0)
			return NETCTL_ERR_INVALID_SSID_COUNT;

		SceUInt64 deadline = sceKernelGetProcessTimeWide() + (SceUInt32)ch->adhocJoinTimeout;

		for (;;) {
			netCtlInitAdhocScanReq(&scanReq, channel, ch->adhocSsid, ssidLen, 1);
			sceKernelGetProcessTimeLow();
			int ret = sceNetWlanScan(NETCTL_IF_WLAN, &scanReq,
						 NETCTL_WLAN_SCAN_REQ_BASE_SIZE,
						 scanOutBuf, sizeof(scanOutBuf),
						 &bssList, &bssCount);
			if (ret < 0)
				return ret;
			sceKernelGetProcessTimeLow();

			const NetCtlWlanBssRecord *best =
				(bssCount != 0)
					? netCtlFindLowestAdhocBss(bssList, bssCount,
								   ch->adhocSsid, ssidLen, NULL)
					: NULL;

			SceUInt32 pollBits = 0;

			netCtlCorePollEvent(ch, NETCTL_EV_DISCONNECT, &pollBits);
			if ((pollBits & NETCTL_EV_DISCONNECT) != 0)
				return NETCTL_ERR_DISCONNECT_REQUESTED;

			if (sceKernelGetProcessTimeWide() >= deadline)
				return SCE_NET_CTL_ERROR_PSP_ADHOC_JOIN_TIMEOUT;

			if (best != NULL) {
				sceClibMemset(&adhocReq, 0, sizeof(adhocReq));
				sceClibMemcpy_safe(&adhocReq.bss, best, sizeof(NetCtlWlanBssRecord));
				adhocReq.flags |= 3u;
				ret = sceNetWlanAdhocJoin(NETCTL_IF_WLAN, &adhocReq);
				if (ret >= 0) {
					sceClibMemcpy_safe(&ch->connectedBss, &adhocReq.bss,
							   sizeof(NetCtlWlanBssRecord));
					return 0;
				}
				return ret;
			}
		}
	}

	return netCtlAdhocStartOrMerge(NETCTL_IF_WLAN, ch->adhocSsid, ssidLen, channel,
				       &ch->connectedBss, (ch->connectMode == 4) ? 1 : 0);
}

/* 0x81217EC2 */
static int netCtlLinkConnect(NetCtlChannel *ch, int useRankedList)
{
	if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRED) {
		ch->ifId = NETCTL_IF_ETHERNET;
	} else if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_PHONE) {
		ch->ifId = NETCTL_IF_PHONE;
	} else {
		ch->ifId = NETCTL_IF_WLAN;
		if (ch->hostFuncs == NULL || ch->hostFuncs->isMobileNetworkAvailable == NULL ||
		    ch->hostFuncs->isMobileNetworkAvailable())
			sceNetPhoneNotifyEvent(NETCTL_IF_PHONE, 0);
	}

	if ((ch->flags & NETCTL_FLAG_RECONNECTING) == 0) {
		int needUp = 0;
		int ret = netCtlCoreAcquireInterface(ch->isInet, ch->ifId, &needUp);

		if (ret < 0)
			return ret;
		if (needUp == 1) {
			ret = netCtlBringUpInterface(ch, ch->ifId, 0);
			if (ret < 0)
				return ret;
		}
	}

	if (ch->isInet == 1) {
		SceUInt32 mtu = (ch->activeConfig.ipConfig == SCE_NET_CTL_IP_PPPOE ||
				 ch->activeConfig.mtu == 0)
					? 1500u
					: ch->activeConfig.mtu;

		sceNetConfigSetIfmtu(ch->ifId, (int)mtu);

		if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRED) {
			int linkStatus = 0;

			if (sceNetEthGetLinkStatus(NETCTL_IF_ETHERNET, &linkStatus) < 0 ||
			    (linkStatus & 1) == 0)
				return SCE_NET_CTL_ERROR_ETHERNET_PLUGOUT;
		} else if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRELESS) {
			int ret = 0;

			if (useRankedList == 1) {
				for (SceUInt32 i = 0; i < ch->rankedCount; ++i) {
					int exists = 0;

					ret = netCtlConfigLoadToNetConfig(0, ch->rankedConfigIds[i],
									  &ch->activeConfig, &exists);
					if (ret < 0)
						break;
					if (exists == 0)
						continue;

					ch->activeConfig.channel = (SceUInt8)ch->rankedChannels[i];
					ret = netCtlWlanConnectWithRetry(ch);
					if (ret >= 0)
						break;

					SceUInt32 bits = 0;

					netCtlCorePollEvent(ch,
							    NETCTL_EV_DISCONNECT |
								    NETCTL_EV_AUTO_DISCONNECT,
							    &bits);
					if (bits != 0) {
						if ((bits & NETCTL_EV_DISCONNECT) != 0)
							ret = NETCTL_ERR_DISCONNECT_REQUESTED;
						else if ((bits & NETCTL_EV_AUTO_DISCONNECT) != 0)
							ret = NETCTL_ERR_AUTO_DISCONNECT_REQ;
						else
							ret = SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE;
						break;
					}
				}
			} else {
				ret = netCtlWlanConnectWithRetry(ch);
			}
			if (ret < 0)
				return ret;
		}

		SceUInt32 tcpMss[2];
		SceUInt32 mssVal = (ch->activeConfig.device == SCE_NET_CTL_DEVICE_PHONE &&
				    ch->hostFuncs != NULL &&
				    ch->hostFuncs->isMobileNetworkAvailable != NULL &&
				    ch->hostFuncs->isMobileNetworkAvailable())
					   ? 1370u
					   : 0u;

		tcpMss[0] = mssVal;
		tcpMss[1] = mssVal;
		SceNetInternal_A6B9F5D2(tcpMss, 0);
		ch->flags |= NETCTL_FLAG_LINK_UP;
		return 0;
	}

	sceNetConfigSetIfmtu(ch->ifId, 1500);
	if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRELESS) {
		int ret = netCtlAdhocConnectWlan(ch);

		if (ret < 0)
			return ret;
		ch->flags |= NETCTL_FLAG_LINK_UP;
		return 0;
	}

	int linkStatus = 0;

	if (sceNetEthGetLinkStatus(NETCTL_IF_ETHERNET, &linkStatus) < 0 || (linkStatus & 1) == 0)
		return SCE_NET_CTL_ERROR_ETHERNET_PLUGOUT;
	ch->flags |= NETCTL_FLAG_LINK_UP;
	return 0;
}

/* 0x81218122 */
static int netCtlLinkDisconnect(NetCtlChannel *ch)
{
	ch->flags &= ~NETCTL_FLAG_LINK_UP;
	if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_WIRELESS) {
		if (ch->isInet == 1) {
			sceNetWlanDisconnect(ch->ifId);
		} else {
			g_netCtlAdhocLeaveTime = sceKernelGetSystemTimeWide();
			sceNetWlanAdhocLeave(ch->ifId);
		}
	}

	int needDown = 0;

	if ((ch->flags & NETCTL_FLAG_RECONNECTING) == 0 &&
	    netCtlCoreReleaseInterface(ch->isInet, ch->ifId, &needDown) >= 0 &&
	    needDown == 1)
		netCtlBringDownInterface(ch, ch->ifId, 0);

	if (ch->activeConfig.device == SCE_NET_CTL_DEVICE_PHONE) {
		if (ch->hostFuncs == NULL || ch->hostFuncs->isMobileNetworkAvailable == NULL ||
		    !ch->hostFuncs->isMobileNetworkAvailable())
			sceNetPhoneNotifyEvent(NETCTL_IF_PHONE, 0);
	}
	return 0;
}
