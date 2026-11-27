/*
 * remote_net.c: Kermit remote network proxy (Adhoc WLAN, Host Discovery, and
 * Infrastructure BSD socket forwarding) for ScePspemu.
 */
#include "host_common.h"
#include <string.h>
#include <stdlib.h>

#define PSPEMU_NET_CHANNEL              10
#define PSPEMU_NET_WLAN_IF_ID           4
#define PSPEMU_NET_ADHOC_SOCK_TYPE      9
#define PSPEMU_NET_ADHOC_ETH_PROTO      0x88C8
#define PSPEMU_NET_ADHOC_RX_SLOTS       64
#define PSPEMU_NET_ADHOC_PACKET_MAX     0xA00
#define PSPEMU_NET_ADHOC_CACHE_MAX      30
#define PSPEMU_NET_ADHOC_IP_ADDR        0x0100A8C0U
#define PSPEMU_NET_ADHOC_NETMASK        0x00FFFFFFU

#define PSPEMU_NET_ERR_RTC_NOT_READY    ((int)0x80251002)
#define PSPEMU_NET_ERR_GENERIC          ((int)0x80000001)
#define PSPEMU_NET_ERR_WLAN_BUSY        ((int)0x80411682)
#define PSPEMU_NET_ERR_NOT_INIT         ((int)0x80410712)
#define PSPEMU_NET_ERR_ADHOC_CREATE     ((int)0x8041071C)
#define PSPEMU_NET_ERR_ADHOC_NO_ENTRY   ((int)0x80410709)
#define PSPEMU_NET_ERR_ADHOC_JOIN_FAIL  ((int)0x80410D0B)
#define PSPEMU_NET_ERR_ADHOC_NOT_FOUND  ((int)0x80410D11)
#define PSPEMU_NET_ERR_INET_ABORTED     ((int)0x80410A08)
#define PSPEMU_NET_ERR_INET_SUSPEND     ((int)0x80410A0A)
#define PSPEMU_NET_ERR_EPOLL_BADF       ((int)0x804101A3)
#define PSPEMU_NET_COMPAT_INTR_RETRY    ((int)0x80010086)

#define PSPEMU_NET_CTL_STATE_IPOBTAINED 4

typedef struct PspemuNetErrorMap {
	int32_t vitaErr;
	int32_t pspErrno;
} PspemuNetErrorMap;

typedef struct PspemuWlanIfAddrInfo {
	uint32_t addr;
	uint32_t bcast;
	uint32_t bcastCopy;
	uint32_t netmask;
} PspemuWlanIfAddrInfo;

typedef struct PspemuWlanIfInfo {
	char                 name[16];
	char                 parentName[16];
	uint32_t             ifIndex;
	uint32_t             parentIfIndex;
	PspemuWlanIfAddrInfo addrs[2];
	uint32_t             flags;
	uint8_t              tail[0x140 - 76];
} PspemuWlanIfInfo;

typedef struct PspemuWlanHostDiscoverEntry {
	uint32_t ssidLen;
	char     ssid[32];
} PspemuWlanHostDiscoverEntry;

typedef struct PspemuWlanWakeSsids {
	uint32_t                    inSize;
	uint32_t                    outSize;
	uint16_t                    status;
	uint16_t                    count;
	PspemuWlanHostDiscoverEntry entries[10];
} PspemuWlanWakeSsids;

typedef struct PspemuHostDiscoverResult {
	uint32_t                    status;
	uint32_t                    count;
	PspemuWlanHostDiscoverEntry entries[10];
} PspemuHostDiscoverResult;

typedef struct PspemuHostDiscoverParam {
	uint32_t mode;
	uint32_t flags;
	uint32_t channel;
	uint32_t ssidLen;
	char     ssid[32];
} PspemuHostDiscoverParam;

typedef struct PspemuWlanBgScanReq {
	uint32_t inSize;
	uint32_t outSize;
	uint32_t scanFlags;
	uint16_t channelListType;
	uint8_t  channel;
	uint8_t  channelTail[13];
	uint8_t  bssType;
	uint8_t  ssidCount;
	uint16_t scanMode;
	uint16_t minChTime;
	uint16_t maxChTime;
	uint32_t probeDelay;
	uint32_t ssidLen;
	char     ssid[32];
	uint8_t  tail[0x1B0 - 0x4C];
} PspemuWlanBgScanReq;

typedef struct PspemuWlanWakeAdhocIn {
	uint32_t enable;
	uint32_t channel;
	uint32_t ssidLen;
	char     ssid[32];
} PspemuWlanWakeAdhocIn;

typedef struct PspemuWlanWakeAdhocReq {
	uint32_t inSize;
	uint32_t outSize;
	uint32_t enable;
	SceUID   processId;
	uint32_t channel;
	uint32_t ssidLen;
	char     ssid[32];
	uint32_t filterSsidLen;
	char     filterSsid[32];
} PspemuWlanWakeAdhocReq;

typedef struct PspemuWlanScanReq {
	uint32_t inSize;
	uint32_t outSize;
	uint32_t flags;
	uint16_t channelListType;
	uint8_t  channels[14];
	uint8_t  bssType;
	uint8_t  body[0x190 - 0x1D];
	uint32_t minChTime;
	uint32_t maxChTime;
	uint8_t  tail[0x1BC - 0x198];
} PspemuWlanScanReq;

typedef struct PspemuWlanBssRaw {
	uint16_t entryLen;
	uint16_t flags;
	uint32_t ssidLen;
	char     ssid[32];
	uint8_t  bssid[6];
	uint8_t  channel;
	uint8_t  networkType;
	uint16_t beaconInterval;
	uint16_t capability;
	int16_t  rssi;
	uint16_t atimWindow;
	uint32_t rateHdr;
	uint32_t rates[4];
	uint8_t  extraInfo[0x84 - 0x4C];
} PspemuWlanBssRaw;

typedef struct PspemuAdhocBssCacheEntry {
	PspemuWlanBssRaw raw;
	int32_t          ttlCount;
	SceUInt64        expireTime;
	int32_t          cachedChannel;
	uint32_t         pad;
} PspemuAdhocBssCacheEntry;

typedef struct PspemuAdhocBssOut {
	uint8_t  bssid[6];
	uint8_t  channel;
	uint8_t  ssidLen;
	char     ssid[32];
	uint32_t networkType;
	uint32_t padAfterType;
	uint32_t zeroWords[3];
	uint16_t capability;
	uint16_t beaconInterval;
	uint8_t  timestamp[8];
	int16_t  rssi;
	uint8_t  tailBytes[3];
	uint8_t  statusByte;
	uint8_t  tailPad[14];
} PspemuAdhocBssOut;

typedef struct PspemuAdhocBssOutSlot {
	uint32_t          nextOffset;
	PspemuAdhocBssOut bss;
} PspemuAdhocBssOutSlot;

typedef struct PspemuAdhocScanReq {
	uint32_t scanType;
	uint8_t  bssid[6];
	uint8_t  channels[14];
	uint8_t  ssidLen;
	uint8_t  pad[3];
	char     ssid[32];
	uint8_t  tail[16];
} PspemuAdhocScanReq;

typedef struct PspemuAdhocJoinReq {
	uint8_t bssid[6];
	uint8_t channel;
	uint8_t ssidLen;
	char    ssid[32];
	uint8_t tail[72];
} PspemuAdhocJoinReq;

typedef struct PspemuWlanAdhocReq {
	uint32_t inSize;
	uint32_t outSize;
	uint32_t mode;
	uint8_t  padHeader[0x90 - 0x0C];
	uint16_t structSize;
	uint16_t padSize;
	uint32_t ssidLen;
	char     ssid[32];
	uint8_t  bssid[6];
	uint8_t  channel;
	uint8_t  networkType;
	uint16_t beaconInterval;
	uint16_t capability;
	int16_t  rssi;
	uint16_t atimWindow;
	uint16_t padRates;
	uint16_t ratePrefix;
	uint32_t rates[4];
	uint8_t  tail[0x114 - 0xDC];
} PspemuWlanAdhocReq;

typedef struct PspemuAdhocRxSlot {
	uint8_t packet[PSPEMU_NET_ADHOC_PACKET_MAX];
	int32_t length;
} PspemuAdhocRxSlot;

typedef struct PspemuAdhocPollArg {
	SceNetId epollId;
	SceNetId sock;
} PspemuAdhocPollArg;

typedef struct PspemuNetOptBuf {
	uint32_t optLen;
	uint32_t pad;
	uint8_t  optVal[4];
} PspemuNetOptBuf;

typedef struct PspemuNetAcceptBuf {
	uint32_t       addrLen;
	int32_t        acceptedSock;
	SceNetSockaddr addr;
} PspemuNetAcceptBuf;

typedef struct PspemuNetSendRecvBuf {
	uint32_t addrLen;
	uint32_t pad;
	uint8_t  addrBuf[252];
	uint32_t dataLen;
	int32_t  flags;
	uint8_t  payload[4];
} PspemuNetSendRecvBuf;

typedef struct PspemuInetInfo {
	uint8_t      bssid[6];
	char         ssid[33];
	uint8_t      ssidLen;
	char         cnfName[64];
	uint16_t     padBeforeRssi;
	uint16_t     rssiPercentage;
	uint16_t     channel;
	uint16_t     padAfterChannel;
	uint32_t     padBeforeSecurity;
	uint32_t     wifiSecurity;
	uint32_t     httpProxyConfig;
	SceNetInAddr ipAddress;
	SceNetInAddr netmask;
	SceNetInAddr defaultRoute;
	SceNetInAddr dnsMain;
	SceNetInAddr dnsFallback;
} PspemuInetInfo;

_Static_assert(sizeof(PspemuWlanIfInfo) == 0x140, "PspemuWlanIfInfo size");
_Static_assert(sizeof(PspemuWlanWakeSsids) == 0x174, "PspemuWlanWakeSsids size");
_Static_assert(sizeof(PspemuHostDiscoverResult) == 0x170, "PspemuHostDiscoverResult size");
_Static_assert(sizeof(PspemuWlanBgScanReq) == 0x1B0, "PspemuWlanBgScanReq size");
_Static_assert(sizeof(PspemuWlanWakeAdhocReq) == 0x5C, "PspemuWlanWakeAdhocReq size");
_Static_assert(sizeof(PspemuWlanScanReq) == 0x1BC, "PspemuWlanScanReq size");
_Static_assert(sizeof(PspemuWlanBssRaw) == 0x84, "PspemuWlanBssRaw size");
_Static_assert(sizeof(PspemuAdhocBssCacheEntry) == 0x98, "PspemuAdhocBssCacheEntry size");
_Static_assert(sizeof(PspemuAdhocBssOutSlot) == 0x60, "PspemuAdhocBssOutSlot size");
_Static_assert(sizeof(PspemuWlanAdhocReq) == 0x114, "PspemuWlanAdhocReq size");
_Static_assert(sizeof(PspemuAdhocRxSlot) == 2564, "PspemuAdhocRxSlot size");
_Static_assert(offsetof(PspemuNetSendRecvBuf, dataLen) == 260, "PspemuNetSendRecvBuf dataLen");
_Static_assert(offsetof(PspemuInetInfo, ipAddress) == 124, "PspemuInetInfo ipAddress");

static const PspemuNetErrorMap g_netErrTable[26] = {
	{ 0x00000023, 11 },
	{ (int32_t)0x80410123, 11 },
	{ (int32_t)0x8041012B, 95 },
	{ (int32_t)0x80410118, 24 },
	{ (int32_t)0x80410137, 105 },
	{ (int32_t)0x8041010C, 12 },
	{ (int32_t)0x80410104, 4 },
	{ (int32_t)0x80410131, 125 },
	{ (int32_t)0x80410130, 112 },
	{ (int32_t)0x80410116, 22 },
	{ (int32_t)0x80410139, 128 },
	{ (int32_t)0x80410138, 127 },
	{ (int32_t)0x8041013C, 116 },
	{ (int32_t)0x8041013D, 111 },
	{ (int32_t)0x80410133, 114 },
	{ (int32_t)0x80410124, 119 },
	{ (int32_t)0x80410125, 120 },
	{ (int32_t)0x8041010D, 13 },
	{ (int32_t)0x80410132, 115 },
	{ (int32_t)0x80410140, 117 },
	{ (int32_t)0x80410109, 9 },
	{ (int32_t)0x80410111, 17 },
	{ (int32_t)0x80410106, 6 },
	{ (int32_t)0x80410136, 104 },
	{ (int32_t)0x804101A3, 127 },
	{ 0, 0 },
};

static const char *const g_adhocTitleFilterTable[5] = {
	"ULJS00218",
	"ULES01275",
	"ULES00703",
	"ULES00125",
	"UCJS10003",
};

static SceKernelLwMutexWork g_netEventMutex;
static int g_netInitialized;
static int g_netCtlInetDone;
static int g_netCtlDisconnectEvent;
static SceKernelLwMutexWork g_netOpMutex;
static int g_netSuspended;
static SceUID g_hostDiscoverThreadId;
static int g_hostDiscoverState;
static uint32_t g_netPendingEventBits;
static int g_netCtlAdhocDone;
static int g_adhocShortPktDelayUsec;
static SceUID g_adhocPollThreadId;
static SceNetId g_adhocSocket;
static uint8_t g_adhocScanBuf[3972];
static int g_adhocTxDelayUsec;
static SceNetId g_adhocEpollId;
static int g_adhocSocketOpen;
static PspemuAdhocRxSlot g_adhocRxRing[PSPEMU_NET_ADHOC_RX_SLOTS];
static int g_adhocTxQuotaRem;
static SceUID g_adhocPollReadyEvf;
static void *g_adhocScanBssList;
static SceUInt32 g_adhocScanBssCount;
static uint32_t g_adhocRxWriteIdx;
static int g_adhocRxBurstCount;
static SceUInt64 g_adhocTxWindowEnd;
static PspemuAdhocBssCacheEntry g_adhocBssCache[PSPEMU_NET_ADHOC_CACHE_MAX];
static uint32_t g_adhocRxReadIdx;
static const char *g_adhocTitleFilter;
static int g_adhocScanCompleted;
static int g_adhocTxRateInterval;
static SceKernelLwMutexWork g_adhocMutex;
static int g_adhocModeEnabled;
static int g_adhocThrottleCounter;
static int g_adhocTxQuotaMax;
static uint8_t g_adhocActiveBssid[6];
static int g_inetDisconnected;
static SceUID g_inetMsgDlgEvf;
static int g_inetIpObtained;
static SceUID g_inetConnectSema;
static int g_inetThrottleEnabled;
static int g_inetAbortRequested;
static int g_inetSocketsOpened;
static int g_inetState;

#define NET_NOINLINE __attribute__((noinline, noclone))

/* Forward declarations for internal helpers */
static int NET_NOINLINE remoteNetInetInit(void);
static int NET_NOINLINE remoteNetAdhocInit(void);
static void NET_NOINLINE remoteNetThread(SceSize argSize, void *argp) __attribute__((noreturn));
static int NET_NOINLINE remoteNetAdhocLeave(int withDelay);
static int NET_NOINLINE remoteNetAdhocTerm(void);
static int NET_NOINLINE remoteNetInetDisconnect(void);
static int NET_NOINLINE remoteNetInetAbort(int errorCode);
static int NET_NOINLINE remoteNetInetStartConnect(void);
static int NET_NOINLINE remoteNetInetGetInfo(PspemuInetInfo *outInfo);
static int NET_NOINLINE remoteNetAdhocPollThread(SceSize argSize, void *argp);
static int NET_NOINLINE remoteNetAdhocScan(const PspemuAdhocScanReq *req, PspemuAdhocBssOutSlot *outList, uint32_t outSize);
static int NET_NOINLINE remoteNetAdhocJoin(const PspemuAdhocJoinReq *req);
static int NET_NOINLINE remoteNetAdhocCreate(PspemuAdhocJoinReq *req);
static int NET_NOINLINE remoteNetAdhocSend(const uint8_t *msg, uint32_t len);
static int NET_NOINLINE remoteNetAdhocRecv(void *outBuf);
static int NET_NOINLINE remoteNetAdhocEnsureStarted(void);
static int NET_NOINLINE remoteNetInetSocket(int domain, int type, int protocol, int *outSock);
static int NET_NOINLINE remoteNetInetClose(SceNetId s);
static int NET_NOINLINE remoteNetInetBind(SceNetId s, const SceNetSockaddr *addr, SceNetSocklen_t addrLen);
static int NET_NOINLINE remoteNetInetListen(SceNetId s, int backlog);
static int NET_NOINLINE remoteNetInetShutdown(SceNetId s, int how);
static int NET_NOINLINE remoteNetInetConnect(SceNetId s, const SceNetSockaddr *addr, SceNetSocklen_t addrLen);
static uint32_t NET_NOINLINE remoteNetInetPoll(SceNetId s, int events);
static int NET_NOINLINE remoteNetInetAccept(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen, int *outSock);
static int NET_NOINLINE remoteNetInetGetpeername(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen);
static int NET_NOINLINE remoteNetInetGetsockname(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen);
static int NET_NOINLINE remoteNetInetGetsockopt(SceNetId s, int level, int optname, void *optval, SceNetSocklen_t *optlen);
static int NET_NOINLINE remoteNetInetSetsockopt(SceNetId s, int level, int optname, const void *optval, SceNetSocklen_t optlen);
static int NET_NOINLINE remoteNetInetRecv(SceNetId s, void *buf, uint32_t len, int flags,
                                          SceNetSockaddr *from, SceNetSocklen_t *fromlen, int *outLen);
static int NET_NOINLINE remoteNetInetSend(SceNetId s, const void *buf, uint32_t len, int flags,
                                          const SceNetSockaddr *to, int tolen, int *outLen);
static int NET_NOINLINE remoteNetInetIoctl(SceNetId s, unsigned int cmd, void *arg);
static int NET_NOINLINE remoteNetAdhocLeaveCmd(void);
static int NET_NOINLINE remoteNetNotifyGuestEvent(uint32_t eventBit);
static int NET_NOINLINE remoteNetIsSuspended(void);
static int NET_NOINLINE remoteNetCtlSwitchAdhocMode(int restoreInet);
static PspemuAdhocBssOut *NET_NOINLINE remoteNetCopyBssToOut(PspemuAdhocBssOut *out, const PspemuWlanBssRaw *src);
static int NET_NOINLINE remoteNetTeardownAdhocIf(void);
static int NET_NOINLINE remoteNetAdhocStartMode(void);
static int NET_NOINLINE remoteNetConfigureAdhocIf(const uint32_t *ipAddr, const uint32_t *netmask, const uint32_t *gateway);
static int NET_NOINLINE remoteNetAdhocOpenSocket(void);
static int NET_NOINLINE remoteNetTranslateError(int vitaErr);
static int NET_NOINLINE remoteNetInetConnectCore(int withDisconnectFirst);
static unsigned int NET_NOINLINE remoteNetGetWlanSwitchState(void);
static int NET_NOINLINE remoteNetGetHostDiscoverResult(PspemuHostDiscoverResult *outResult);
static int NET_NOINLINE remoteNetStopHostDiscover(void);

int64_t remoteNetFetchTick(const PspemuKermitRequest *req, int (*tickFn)(SceRtcTick *))
{
	SceRtcTick *outTick;
	int ret;

	outTick = (SceRtcTick *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 2, sizeof(SceRtcTick));
	ret = tickFn(outTick);
	if (ret >= 0) {
		remotefsFlushGuestAddr(outTick, sizeof(SceRtcTick));
		return ret;
	}
	if (ret == PSPEMU_NET_ERR_RTC_NOT_READY)
		return PSPEMU_NET_ERR_GENERIC;
	return ret;
}

static int remoteNetHostDiscoverEventCb(int ifId, int eventType, int64_t eventCode)
{
	(void)eventType;
	if (ifId == PSPEMU_NET_WLAN_IF_ID && eventCode == 16)
		g_hostDiscoverState = 1;
	return 0;
}

static void remoteNetCtlAdhocSwitchCb(int eventType, void *arg)
{
	int state;
	(void)arg;

	if (SceNetCtl_E8E8DF18(&state) >= 0 && eventType == SCE_NET_CTL_EVENT_TYPE_IPOBTAINED)
		g_netCtlAdhocDone = 1;
}

static void remoteNetCtlInetSwitchCb(int eventType, void *arg)
{
	int state;
	(void)arg;

	if (SceNetCtl_E8E8DF18(&state) >= 0 && eventType == SCE_NET_CTL_EVENT_TYPE_IPOBTAINED)
		g_netCtlInetDone = 1;
}

static void remoteNetCtlInetConnectCb(int eventType, void *arg)
{
	int state;
	(void)arg;

	if (SceNetCtl_E8E8DF18(&state) >= 0) {
		if (eventType == 1 || eventType == 3)
			g_netCtlDisconnectEvent = 1;
		if (state == PSPEMU_NET_CTL_STATE_IPOBTAINED) {
			g_inetIpObtained = 1;
			g_inetState = state;
		} else if ((unsigned int)(state - 1) <= 3U) {
			g_inetState = state;
		}
		if (eventType == SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED)
			g_inetDisconnected = 1;
	}
}

static unsigned int remoteNetGetWlanSwitchState(void)
{
	unsigned int compatVal;
	int config;
	unsigned int state;

	compatVal = (unsigned int)pspemuCompatGetParam(21);
	if (compatVal <= 1U)
		return compatVal;

	config = sceWlanGetConfiguration();
	state = (config != 0) ? (unsigned int)(config & 1) : 0U;
	if (pspemuCompatHasFlag(64U) != 0)
		state |= 2U;
	return state;
}

static int remoteNetGetHostDiscoverResult(PspemuHostDiscoverResult *outResult)
{
	PspemuWlanWakeSsids wakeSsids;
	int retry;
	int ret;
	int i;

	if (outResult == NULL)
		return 0;

	retry = 10;
	for (;;) {
		ret = sceNetWlanGetWakeDiscoveredSsids(PSPEMU_NET_WLAN_IF_ID, &wakeSsids);
		if (ret != PSPEMU_NET_ERR_WLAN_BUSY)
			break;
		if ((remoteNetGetWlanSwitchState() & 1U) == 0U)
			return ret;
		sceKernelDelayThread(1000000U);
		if (--retry == 0)
			return ret;
	}

	if (ret == 0) {
		outResult->status = wakeSsids.status;
		outResult->count = wakeSsids.count;
		for (i = 0; i < 10; i++)
			outResult->entries[i] = wakeSsids.entries[i];
		sceNetWlanClearWakeAdhoc(PSPEMU_NET_WLAN_IF_ID);
	}
	return ret;
}

static int remoteNetStopHostDiscover(void)
{
	int stat;
	SceUInt timeout = 0xFFFFFFFFU;

	if (g_hostDiscoverThreadId != 0) {
		g_hostDiscoverState = -1;
		sceKernelWaitThreadEnd(g_hostDiscoverThreadId, &stat, &timeout);
		sceKernelDeleteThread(g_hostDiscoverThreadId);
		g_hostDiscoverThreadId = 0;
	}
	return 0;
}

int remoteNetInit(void)
{
	int ret;
	SceUID thid;

	ret = remoteNetInetInit();
	if (ret != 0)
		return ret;

	ret = sceKernelCreateLwMutex(&g_netOpMutex, "ScePspemuNetOp", 0x2000U, 0, NULL);
	if (ret != 0)
		return ret;

	thid = sceKernelCreateThread("ScePspemuRemoteNet",
	                             (SceKernelThreadEntry)remoteNetThread,
	                             96, 0x1000U, 0, 0x10000, NULL);
	if (thid < 0)
		return thid;

	ret = sceKernelStartThread(thid, 0, NULL);
	if (ret != 0)
		return ret;

	ret = sceKernelCreateLwMutex(&g_netEventMutex, "ScePspemuNet", 0, 0, NULL);
	if (ret != 0)
		return ret;

	ret = remoteNetAdhocInit();
	if (ret == 0)
		g_netInitialized = 1;
	return ret;
}

static int remoteNetNotifyGuestEvent(uint32_t eventBit)
{
	int ret;

	if (eventBit == 2U && !pspemuIsRebootReady())
		return 0;

	if (sceKernelLockLwMutex(&g_netEventMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	g_netPendingEventBits |= eventBit;
	if (sceKernelUnlockLwMutex(&g_netEventMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	do {
		ret = sceCompatInterrupt(8);
		if (ret == PSPEMU_NET_COMPAT_INTR_RETRY)
			sceKernelDelayThread(10000U);
	} while (ret == PSPEMU_NET_COMPAT_INTR_RETRY);

	return ret;
}

static int remoteNetHostDiscoverPollThread(SceSize argSize, void *argp)
{
	const PspemuHostDiscoverParam *param = (const PspemuHostDiscoverParam *)argp;
	PspemuWlanBgScanReq bgReq;
	uint8_t bgOut[2640];
	void *bssList;
	SceUInt32 bssCount;
	int ret;

	(void)argSize;
	memset(&bgReq, 0, sizeof(bgReq));
	ret = SceNetInternal_EAC33599(PSPEMU_NET_WLAN_IF_ID, remoteNetHostDiscoverEventCb, NULL, 0);
	if (ret >= 0) {
		bgReq.scanFlags = (param->flags != 0U) ? 4U : 0U;
		bgReq.channelListType = 1U;
		bgReq.channel = (uint8_t)param->channel;
		bgReq.bssType = 2U;
		bgReq.ssidCount = 1U;
		bgReq.scanMode = 1U;
		bgReq.minChTime = 100U;
		bgReq.maxChTime = 100U;
		bgReq.probeDelay = 110U;
		bgReq.ssidLen = param->ssidLen;
		memcpy(bgReq.ssid, param->ssid, sizeof(bgReq.ssid));

		ret = sceNetWlanStartBackgroundScan(PSPEMU_NET_WLAN_IF_ID, &bgReq, sizeof(bgReq));
		if (ret >= 0) {
			while (g_hostDiscoverState == 0)
				ret = SceNetInternal_04E6136D(500000, NULL, 0);

			bssCount = 0;
			sceNetWlanReadBackgroundScan(PSPEMU_NET_WLAN_IF_ID, bgOut, sizeof(bgOut), &bssList, &bssCount);
			remoteNetAdhocLeave(0);
			sceNetWlanStopBackgroundScan(PSPEMU_NET_WLAN_IF_ID);
			SceNetInternal_EDAA3453(PSPEMU_NET_WLAN_IF_ID, 0);
			if (g_hostDiscoverState > 0)
				ret = remoteNetNotifyGuestEvent(1U);
		}
	}
	return ret;
}

int remoteNetSuspend(void)
{
	int ret;

	ret = remoteMsfsSetBootAllowed(0);
	if (g_netInitialized != 0) {
		sceKernelLockLwMutex(&g_netOpMutex, 1, NULL);
		g_netSuspended = 1;
		remoteNetInetAbort(PSPEMU_NET_ERR_INET_SUSPEND);
		return sceKernelUnlockLwMutex(&g_netOpMutex, 1);
	}
	return ret;
}

static int remoteNetIsSuspended(void)
{
	return g_netSuspended;
}

int remoteNetResetOnReboot(void)
{
	remoteNetAdhocLeave(0);
	remoteNetAdhocTerm();
	return remoteNetInetDisconnect();
}

static void remoteNetThread(SceSize argSize, void *argp)
{
	PspemuKermitRequest *req;
	int64_t retVal = 0;

	(void)argSize;
	(void)argp;

	for (;;) {
		int cmd = remotefsWaitAndGetRequest(PSPEMU_NET_CHANNEL, &req);
		sceKernelLockLwMutex(&g_netOpMutex, 1, NULL);

		switch (cmd) {
		case '0': {
			uint32_t bits;
			if (sceKernelLockLwMutex(&g_netEventMutex, 1, NULL) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			bits = g_netPendingEventBits;
			g_netPendingEventBits = 0;
			retVal = (int64_t)(uint64_t)bits;
			if (sceKernelUnlockLwMutex(&g_netEventMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			break;
		}
		case '1': {
			uint32_t bufSize = (uint32_t)req->args[3];
			PspemuNetOptBuf *buf = (PspemuNetOptBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[4], 3, bufSize);
			int res = remoteNetInetGetsockopt((SceNetId)req->args[0], (int)req->args[1],
			                                  (int)req->args[2], buf->optVal, &buf->optLen);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case '2': {
			uint32_t bufSize = (uint32_t)req->args[1];
			PspemuNetOptBuf *buf = (PspemuNetOptBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 3, bufSize);
			int res = remoteNetInetGetsockname((SceNetId)req->args[0],
			                                   (SceNetSockaddr *)buf->optVal, &buf->optLen);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case '3': {
			void *arg = remotefsTranslateGuestAddr((uint32_t)req->args[2], 3, 36U);
			int res = remoteNetInetIoctl((SceNetId)req->args[0], (unsigned int)req->args[1], arg);
			retVal = res;
			remotefsFlushGuestAddr(arg, 36U);
			break;
		}
		case '4':
			if (sceKernelLockLwMutex(&g_netEventMutex, 1, NULL) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			g_netPendingEventBits = 0;
			if (sceKernelUnlockLwMutex(&g_netEventMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			retVal = 0;
			remoteNetAdhocLeave(0);
			break;
		case '5': {
			uint32_t bufSize = (uint32_t)req->args[1];
			PspemuNetOptBuf *buf = (PspemuNetOptBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 3, bufSize);
			int res = remoteNetInetGetpeername((SceNetId)req->args[0],
			                                   (SceNetSockaddr *)buf->optVal, &buf->optLen);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case '6': {
			uint32_t bufSize = (uint32_t)req->args[1];
			PspemuNetSendRecvBuf *buf = (PspemuNetSendRecvBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 3, bufSize);
			int res = remoteNetInetSend((SceNetId)req->args[0], buf->payload, buf->dataLen,
			                            buf->flags, (const SceNetSockaddr *)buf->addrBuf,
			                            (int)buf->addrLen, (int *)&buf->dataLen);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case '7': {
			void *param = remotefsTranslateGuestAddr((uint32_t)req->args[0], 1,
			                                         sizeof(PspemuHostDiscoverParam));
			int res;
			if (param != NULL) {
				SceUID thid;
				g_hostDiscoverState = 0;
				thid = sceKernelCreateThread("ScePspemuHstDcvrPoll",
				                             (SceKernelThreadEntry)remoteNetHostDiscoverPollThread,
				                             96, 0x1000U, 0, 0x10000, NULL);
				res = thid;
				if (thid >= 0) {
					g_hostDiscoverThreadId = thid;
					res = sceKernelStartThread(thid, sizeof(PspemuHostDiscoverParam), param);
				}
			} else {
				res = remoteNetStopHostDiscover();
			}
			retVal = res;
			break;
		}
		case '8': {
			PspemuHostDiscoverResult *outRes = (PspemuHostDiscoverResult *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 2, sizeof(PspemuHostDiscoverResult));
			int res = remoteNetGetHostDiscoverResult(outRes);
			if (res >= 0)
				remotefsFlushGuestAddr(outRes, sizeof(PspemuHostDiscoverResult));
			retVal = res;
			break;
		}
		case '9': {
			uint32_t bufSize = (uint32_t)req->args[1];
			PspemuNetSendRecvBuf *buf = (PspemuNetSendRecvBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 3, bufSize);
			int res = remoteNetInetRecv((SceNetId)req->args[0], buf->payload, buf->dataLen,
			                            buf->flags, (SceNetSockaddr *)buf->addrBuf,
			                            &buf->addrLen, (int *)&buf->dataLen);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case ':': {
			const PspemuAdhocJoinReq *joinReq = (const PspemuAdhocJoinReq *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuAdhocJoinReq));
			retVal = remoteNetAdhocJoin(joinReq);
			break;
		}
		case ';':
			retVal = remoteNetInetListen((SceNetId)req->args[0], (int)req->args[1]);
			break;
		case '<': {
			uint32_t bufSize = (uint32_t)req->args[1];
			PspemuNetAcceptBuf *buf = (PspemuNetAcceptBuf *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 3, bufSize);
			int res = remoteNetInetAccept((SceNetId)req->args[0], &buf->addr,
			                              &buf->addrLen, &buf->acceptedSock);
			retVal = res;
			remotefsFlushGuestAddr(buf, bufSize);
			break;
		}
		case '=': {
			const PspemuWlanWakeAdhocIn *inReq = (const PspemuWlanWakeAdhocIn *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuWlanWakeAdhocIn));
			int res;
			if (inReq != NULL) {
				PspemuWlanWakeAdhocReq wakeReq;
				memset(&wakeReq, 0, sizeof(wakeReq));
				wakeReq.enable = (inReq->enable != 0U) ? 1U : 0U;
				wakeReq.processId = sceKernelGetProcessId();
				wakeReq.channel = inReq->channel;
				wakeReq.ssidLen = inReq->ssidLen;
				memcpy(wakeReq.ssid, inReq->ssid, sizeof(wakeReq.ssid));
				wakeReq.filterSsidLen = inReq->ssidLen;
				memcpy(wakeReq.filterSsid, inReq->ssid, sizeof(wakeReq.filterSsid));
				remoteNetAdhocLeave(0);
				res = sceNetWlanSetWakeAdhoc(PSPEMU_NET_WLAN_IF_ID, &wakeReq);
			} else {
				res = sceNetWlanClearWakeAdhoc(PSPEMU_NET_WLAN_IF_ID);
			}
			retVal = res;
			break;
		}
		case '>':
			retVal = remoteNetAdhocLeave(1);
			break;
		case '?':
			remoteNetStopHostDiscover();
			remoteNetResetOnReboot();
			retVal = 0;
			break;
		case '@': {
			uint32_t bufSize = (uint32_t)req->args[3];
			int *outSock = (int *)remotefsTranslateGuestAddr((uint32_t)req->args[4], 2, bufSize);
			int res = remoteNetInetSocket((int)req->args[0], (int)req->args[1],
			                              (int)req->args[2], outSock);
			retVal = res;
			remotefsFlushGuestAddr(outSock, bufSize);
			break;
		}
		case 'A':
			retVal = remoteNetInetClose((SceNetId)req->args[0]);
			break;
		case 'B':
			retVal = remoteNetInetShutdown((SceNetId)req->args[0], (int)req->args[1]);
			break;
		case 'C':
			retVal = (int64_t)(int32_t)remoteNetInetPoll((SceNetId)req->args[0], (int)req->args[1]);
			break;
		case 'D': {
			uint32_t optLen = (uint32_t)req->args[3];
			const void *optVal = remotefsTranslateGuestAddr((uint32_t)req->args[4], 1, optLen);
			retVal = remoteNetInetSetsockopt((SceNetId)req->args[0], (int)req->args[1],
			                                 (int)req->args[2], optVal, optLen);
			break;
		}
		case 'E': {
			SceNetEtherAddr *mac = (SceNetEtherAddr *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 2, sizeof(SceNetEtherAddr));
			retVal = sceNetGetMacAddress(mac, 0);
			remotefsFlushGuestAddr(mac, sizeof(SceNetEtherAddr));
			break;
		}
		case 'F':
			retVal = remoteNetAdhocEnsureStarted();
			break;
		case 'G': {
			uint32_t addrLen = (uint32_t)req->args[1];
			const SceNetSockaddr *addr = (const SceNetSockaddr *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 1, addrLen);
			retVal = remoteNetInetConnect((SceNetId)req->args[0], addr, addrLen);
			break;
		}
		case 'H':
			retVal = remoteNetAdhocLeaveCmd();
			break;
		case 'I':
			retVal = (int64_t)(int32_t)remoteNetGetWlanSwitchState();
			break;
		case 'J': {
			uint32_t bufSize = (uint32_t)req->args[1];
			void *outBuf = remotefsTranslateGuestAddr((uint32_t)req->args[0], 2, bufSize);
			int res = remoteNetAdhocRecv(outBuf);
			retVal = res;
			remotefsFlushGuestAddr(outBuf, bufSize);
			break;
		}
		case 'K': {
			uint32_t addrLen = (uint32_t)req->args[1];
			const SceNetSockaddr *addr = (const SceNetSockaddr *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[2], 1, addrLen);
			retVal = remoteNetInetBind((SceNetId)req->args[0], addr, addrLen);
			break;
		}
		case 'L': {
			const PspemuAdhocScanReq *scanReq = (const PspemuAdhocScanReq *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuAdhocScanReq));
			uint32_t outSize = (uint32_t)req->args[2];
			PspemuAdhocBssOutSlot *outList = (PspemuAdhocBssOutSlot *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[1], 2, outSize);
			int res = remoteNetAdhocScan(scanReq, outList, outSize);
			retVal = res;
			remotefsFlushGuestAddr(outList, outSize);
			break;
		}
		case 'M':
			retVal = remoteNetInetDisconnect();
			remoteMsfsSetBootAllowed(0);
			break;
		case 'N': {
			PspemuAdhocJoinReq *createReq = (PspemuAdhocJoinReq *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 3, sizeof(PspemuAdhocJoinReq));
			retVal = remoteNetAdhocCreate(createReq);
			remotefsFlushGuestAddr(createReq, sizeof(PspemuAdhocJoinReq));
			break;
		}
		case 'O': {
			uint32_t msgLen = (uint32_t)req->args[1];
			const uint8_t *msg = (const uint8_t *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, msgLen);
			retVal = remoteNetAdhocSend(msg, msgLen);
			break;
		}
		case 'P': {
			uint32_t subCmd = (uint32_t)req->args[0];
			if (subCmd == 0U) {
				retVal = remoteNetInetAbort(0);
			} else if (subCmd == 1U) {
				retVal = remoteNetInetStartConnect();
			} else {
				PspemuInetInfo *outInfo = (PspemuInetInfo *)remotefsTranslateGuestAddr(
					(uint32_t)req->args[1], 2, subCmd);
				int res = remoteNetInetGetInfo(outInfo);
				retVal = res;
				remotefsFlushGuestAddr(outInfo, subCmd);
			}
			break;
		}
		case 'Q':
			retVal = remoteNetAdhocTerm();
			break;
		default:
			pspemuRecordTitleListFlags(16U);
			break;
		}

		remotefsReturnValue(PSPEMU_NET_CHANNEL, req, retVal);
		sceKernelUnlockLwMutex(&g_netOpMutex, 1);
	}
}

int remoteNetResume(void)
{
	if (g_netInitialized != 0) {
		sceKernelLockLwMutex(&g_netOpMutex, 1, NULL);
		g_netSuspended = 0;
		return sceKernelUnlockLwMutex(&g_netOpMutex, 1);
	}
	return 0;
}

static int remoteNetCtlSwitchAdhocMode(int restoreInet)
{
	int inetCid;
	int adhocCid;
	int ret;

	g_netCtlInetDone = 0;
	g_netCtlAdhocDone = 0;

	ret = SceNetCtl_AC326DB7(remoteNetCtlInetSwitchCb, NULL, &inetCid);
	if (ret >= 0) {
		ret = SceNetCtl_713394C0(remoteNetCtlAdhocSwitchCb, NULL, &adhocCid);
		if (ret >= 0) {
			if (restoreInet != 0)
				ret = SceNetCtl_8F09A038();
			else
				ret = SceNetCtl_9629B72F();
			if (ret >= 0) {
				int tries = 10;
				while (g_netCtlInetDone == 0 || g_netCtlAdhocDone == 0) {
					SceNetInternal_104E4820(10000);
					sceNetCtlCheckCallback();
					if (restoreInet == 1 && --tries == 0)
						break;
				}
			}
			SceNetCtl_3AB4D791(adhocCid);
		}
		SceNetCtl_229EAA64(inetCid);
	}
	return ret;
}

static PspemuAdhocBssOut *remoteNetCopyBssToOut(PspemuAdhocBssOut *out, const PspemuWlanBssRaw *src)
{
	int rssi;
	int i;

	for (i = 0; i < 6; i++)
		out->bssid[i] = src->bssid[i];
	out->channel = src->channel;
	out->ssidLen = (uint8_t)src->ssidLen;
	for (i = 0; i < 32; i++)
		out->ssid[i] = src->ssid[i];
	out->networkType = src->networkType;
	out->zeroWords[0] = 0;
	out->zeroWords[1] = 0;
	out->zeroWords[2] = 0;
	out->capability = 0;
	out->beaconInterval = src->beaconInterval;
	for (i = 0; i < 8; i++)
		out->timestamp[i] = 0;
	rssi = src->rssi + 40;
	if (rssi > 100)
		rssi = 100;
	out->rssi = (int16_t)rssi;
	out->statusByte = 0;
	return out;
}

static int remoteNetTeardownAdhocIf(void)
{
	int ret;

	ret = sceNetConfigDelIfaddr(PSPEMU_NET_WLAN_IF_ID);
	if (ret >= 0) {
		ret = sceNetConfigDelDefaultRoute();
		if (ret >= 0)
			return sceNetConfigDownInterface(PSPEMU_NET_WLAN_IF_ID);
	}
	return ret;
}

static int remoteNetAdhocStartMode(void)
{
	int ret;

	memset(g_adhocBssCache, 0, sizeof(g_adhocBssCache));
	ret = remoteNetCtlSwitchAdhocMode(0);
	if (ret >= 0)
		g_adhocModeEnabled = 1;
	return ret;
}

static int remoteNetConfigureAdhocIf(const uint32_t *ipAddr, const uint32_t *netmask, const uint32_t *gateway)
{
	PspemuWlanIfInfo ifInfo;
	int ret;

	memset(&ifInfo, 0, sizeof(ifInfo));
	ret = sceNetConfigUpInterface(PSPEMU_NET_WLAN_IF_ID);
	if (ret >= 0) {
		ret = sceNetConfigSetIfaddr(PSPEMU_NET_WLAN_IF_ID, ipAddr, NULL, netmask);
		if (ret >= 0 && (*gateway == 0U || (ret = sceNetConfigSetDefaultRoute(gateway)) >= 0)) {
			for (;;) {
				ret = SceNetInternal_235DE96C(PSPEMU_NET_WLAN_IF_ID, &ifInfo, 1);
				if (ret >= 0 && ifInfo.addrs[0].addr != 0U && (ifInfo.flags & 0x40U) != 0U)
					break;
				sceKernelDelayThread(500000U);
			}
		}
	}
	return ret;
}

static int remoteNetAdhocOpenSocket(void)
{
	uint32_t ipAddr;
	uint32_t netmask;
	uint32_t gateway;
	PspemuAdhocPollArg pollArg;
	SceNetId sock;
	SceNetId epollId;
	SceUID thid;
	int ret;

	sceKernelLockLwMutex(&g_adhocMutex, 1, NULL);
	if (g_adhocSocketOpen == 1) {
		sceKernelUnlockLwMutex(&g_adhocMutex, 1);
		return 0;
	}

	netmask = PSPEMU_NET_ADHOC_NETMASK;
	gateway = PSPEMU_NET_ADHOC_IP_ADDR;
	ipAddr = PSPEMU_NET_ADHOC_IP_ADDR;
	ret = remoteNetConfigureAdhocIf(&ipAddr, &netmask, &gateway);
	if (ret < 0) {
		sceKernelUnlockLwMutex(&g_adhocMutex, 1);
		return ret;
	}

	sock = sceNetSocket("ScePspemuAdhoc", SCE_NET_AF_INET, PSPEMU_NET_ADHOC_SOCK_TYPE, PSPEMU_NET_ADHOC_ETH_PROTO);
	if (sock < 0) {
		remoteNetTeardownAdhocIf();
		sceKernelUnlockLwMutex(&g_adhocMutex, 1);
		return sock;
	}

	g_adhocSocket = sock;
	g_adhocTxWindowEnd = 0;

	epollId = sceNetEpollCreate("ScePspemuAdhoc", 0);
	pollArg.epollId = epollId;
	if (epollId >= 0) {
		thid = sceKernelCreateThread("ScePspemuRemoteNetAdhocPoll",
		                             (SceKernelThreadEntry)remoteNetAdhocPollThread,
		                             96, 0x1000U, 0, 0x10000, NULL);
		if (thid < 0) {
			sceNetEpollAbort(epollId, 0);
			sceNetEpollDestroy(epollId);
			ret = thid;
		} else {
			pollArg.sock = g_adhocSocket;
			ret = sceKernelStartThread(thid, sizeof(pollArg), &pollArg);
			if (ret == 0) {
				g_adhocEpollId = epollId;
				g_adhocPollThreadId = thid;
				g_adhocSocketOpen = 1;
				sceKernelUnlockLwMutex(&g_adhocMutex, 1);
				sceKernelWaitEventFlag(g_adhocPollReadyEvf, 1U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				                       NULL, NULL);
				return 0;
			}
			sceKernelDeleteThread(thid);
			sceNetEpollAbort(epollId, 0);
			sceNetEpollDestroy(epollId);
			if (ret >= 0) {
				g_adhocSocketOpen = 1;
				sceKernelUnlockLwMutex(&g_adhocMutex, 1);
				sceKernelWaitEventFlag(g_adhocPollReadyEvf, 1U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				                       NULL, NULL);
				return 0;
			}
		}
	} else {
		ret = epollId;
	}

	remoteNetTeardownAdhocIf();
	sceNetSocketClose(g_adhocSocket);
	sceKernelUnlockLwMutex(&g_adhocMutex, 1);
	return ret;
}

static int remoteNetAdhocPollThread(SceSize argSize, void *argp)
{
	const PspemuAdhocPollArg *arg = (const PspemuAdhocPollArg *)argp;
	SceNetEpollEvent ctrlEv;
	SceNetEpollEvent waitEv;
	SceNetSockaddr fromAddr;
	SceNetSocklen_t fromLen;
	int dropCount;
	int ret;
	int i;

	for (i = 0; i < PSPEMU_NET_ADHOC_RX_SLOTS; i++)
		g_adhocRxRing[i].length = 0;

	g_adhocRxReadIdx = 0;
	g_adhocRxWriteIdx = 0;
	g_adhocRxBurstCount = 0;
	sceKernelSetEventFlag(g_adhocPollReadyEvf, 1U);

	if (argSize != sizeof(PspemuAdhocPollArg))
		return PSPEMU_ERROR_INVALID_ARG;

	memset(&ctrlEv, 0, sizeof(ctrlEv));
	ctrlEv.events = SCE_NET_EPOLLIN;
	ret = sceNetEpollControl(arg->epollId, SCE_NET_EPOLL_CTL_ADD, arg->sock, &ctrlEv);
	if (ret < 0)
		return ret;

	dropCount = pspemuCompatGetParam(15);
	if (dropCount != -1)
		dropCount &= 0xFF;

	for (;;) {
		int recvLen;
		PspemuAdhocRxSlot *slot;

		do {
			if (sceNetEpollWait(arg->epollId, &waitEv, 1, -1) < 0 ||
			    (waitEv.events & (SCE_NET_EPOLLERR | SCE_NET_EPOLLHUP)) != 0U)
				return 0;
		} while ((waitEv.events & SCE_NET_EPOLLIN) == 0U);

		slot = &g_adhocRxRing[g_adhocRxWriteIdx];
		recvLen = sceNetRecvfrom(arg->sock, slot->packet, PSPEMU_NET_ADHOC_PACKET_MAX,
		                         SCE_NET_MSG_DONTWAIT, &fromAddr, &fromLen);
		if (recvLen < 0)
			return 0;

		if (dropCount > 0) {
			dropCount--;
			continue;
		}

		if (slot->length != 0) {
			if (g_adhocTxRateInterval > 0)
				continue;
			for (i = 0; i < PSPEMU_NET_ADHOC_RX_SLOTS; i++)
				g_adhocRxRing[i].length = 0;
			g_adhocRxReadIdx = 0;
			g_adhocRxRing[0].length = recvLen;
			g_adhocRxWriteIdx = 1U;
			g_adhocRxBurstCount = 1;
			while (sceCompatInterrupt(7) == PSPEMU_NET_COMPAT_INTR_RETRY)
				sceKernelDelayThread(10000U);
		} else {
			int burst = g_adhocRxBurstCount + 1;
			slot->length = recvLen;
			g_adhocRxWriteIdx = (g_adhocRxWriteIdx + 1U) & (PSPEMU_NET_ADHOC_RX_SLOTS - 1U);
			if (burst == 1 || burst > 3) {
				g_adhocRxBurstCount = 1;
				while (sceCompatInterrupt(7) == PSPEMU_NET_COMPAT_INTR_RETRY)
					sceKernelDelayThread(10000U);
			} else {
				g_adhocRxBurstCount = burst;
			}
		}
	}
}

int remoteNetIsThrottleActive(void)
{
	int cur = g_adhocThrottleCounter;

	if ((unsigned int)(cur - 1) <= 998U) {
		g_adhocThrottleCounter = cur - 1;
		cur--;
	}
	return cur != 0;
}

static int remoteNetAdhocScan(const PspemuAdhocScanReq *req, PspemuAdhocBssOutSlot *outList, uint32_t outSize)
{
	PspemuWlanScanReq wlanReq;
	int ret;
	int i;

	if (g_adhocModeEnabled == 0)
		remoteNetAdhocStartMode();

	memset(&wlanReq, 0, sizeof(wlanReq));
	wlanReq.channelListType = 1U;
	wlanReq.bssType = (uint8_t)req->scanType;
	wlanReq.minChTime = 100U;
	wlanReq.maxChTime = 100U;
	for (i = 0; i < 14; i++)
		wlanReq.channels[i] = req->channels[i];

	g_adhocScanBssCount = 0;
	ret = sceNetWlanScan(PSPEMU_NET_WLAN_IF_ID, &wlanReq, sizeof(wlanReq),
	                     g_adhocScanBuf, sizeof(g_adhocScanBuf),
	                     &g_adhocScanBssList, &g_adhocScanBssCount);
	if (ret < 0)
		return 0;

	if (wlanReq.bssType == 1U) {
		const uint8_t *cursor = (const uint8_t *)g_adhocScanBssList;
		uint32_t remain = outSize;
		uint32_t count = 0;

		if (g_adhocScanBssCount == 0U)
			return 0;

		for (;;) {
			const PspemuWlanBssRaw *raw = (const PspemuWlanBssRaw *)cursor;
			remoteNetCopyBssToOut(&outList[count].bss, raw);
			remain -= 76U;
			if (remain == 0U)
				return (int)(count + 1U);
			count++;
			cursor += raw->entryLen;
			if (count >= g_adhocScanBssCount || count == 17U)
				return (int)count;
		}
	}

	if (g_adhocScanBssCount != 0U) {
		const uint8_t *cursor = (const uint8_t *)g_adhocScanBssList;
		uint32_t idx;

		for (idx = 0; idx < g_adhocScanBssCount; idx++) {
			const PspemuWlanBssRaw *raw = (const PspemuWlanBssRaw *)cursor;
			PspemuAdhocBssCacheEntry *dst = NULL;

			for (i = 0; i < PSPEMU_NET_ADHOC_CACHE_MAX; i++) {
				if (strncmp((const char *)raw->bssid,
				            (const char *)g_adhocBssCache[i].raw.bssid, 6U) == 0) {
					dst = &g_adhocBssCache[i];
					break;
				}
			}
			if (dst == NULL) {
				for (i = 0; i < PSPEMU_NET_ADHOC_CACHE_MAX; i++) {
					if (g_adhocBssCache[i].ttlCount == 0) {
						dst = &g_adhocBssCache[i];
						break;
					}
				}
			}
			if (dst != NULL) {
				memcpy(&dst->raw, raw, sizeof(dst->raw));
				dst->ttlCount = 4;
				dst->cachedChannel = raw->channel;
				dst->expireTime = sceKernelGetProcessTimeWide() + 1000000ULL;
			}
			cursor += raw->entryLen;
		}
	}

	{
		SceUInt64 now = sceKernelGetProcessTimeWide();
		uint32_t remain = outSize;
		int outCount = 0;

		for (i = 0; i < PSPEMU_NET_ADHOC_CACHE_MAX; i++) {
			PspemuAdhocBssCacheEntry *entry = &g_adhocBssCache[i];

			if (entry->ttlCount == 0)
				continue;

			entry->ttlCount--;
			if (entry->expireTime < now) {
				entry->ttlCount = 0;
				continue;
			}

			if (entry->cachedChannel != req->channels[0] &&
			    entry->cachedChannel != req->channels[1] &&
			    entry->cachedChannel != req->channels[2])
				continue;

			if (req->ssidLen != 0U) {
				if (strncmp(req->ssid, entry->raw.ssid, req->ssidLen) != 0)
					continue;
			} else if (g_adhocTitleFilter != NULL) {
				if (strncmp(&entry->raw.ssid[5], g_adhocTitleFilter, 9U) != 0 ||
				    memcmp(entry->raw.bssid, g_adhocActiveBssid, 6U) == 0)
					continue;
			}

			remain -= sizeof(PspemuAdhocBssOutSlot);
			remoteNetCopyBssToOut(&outList[outCount].bss, &entry->raw);
			outCount++;
			if (remain < sizeof(PspemuAdhocBssOutSlot) || outCount > 15) {
				g_adhocScanCompleted = 1;
				return outCount;
			}
		}

		g_adhocScanCompleted = 1;
		if (outCount != 0)
			return outCount;

		{
			int delayMs = pspemuCompatGetParam(1);
			if (delayMs > 0)
				sceKernelDelayThread((SceUInt)(delayMs * 1000));
		}
		if (req->ssidLen == 0U) {
			int delayMs = pspemuCompatGetParam(10);
			if (delayMs > 0 &&
			    (req->channels[0] != 1U || req->channels[1] != 0U || req->channels[2] != 0U)) {
				g_adhocThrottleCounter = 1000;
				sceKernelDelayThread((SceUInt)(delayMs * 1000));
			}
		}
		return 0;
	}
}

static int remoteNetAdhocJoin(const PspemuAdhocJoinReq *req)
{
	PspemuWlanAdhocReq joinReq;
	int matchIdx;
	int ret;
	int i;

	if (g_adhocModeEnabled == 0)
		return PSPEMU_NET_ERR_NOT_INIT;

	if (g_adhocScanCompleted == 0) {
		PspemuAdhocScanReq scanReq;
		PspemuAdhocBssOutSlot probeOut;
		int tries = 3;

		memset(&scanReq, 0, sizeof(scanReq));
		scanReq.scanType = 2U;
		memcpy(scanReq.bssid, req->bssid, sizeof(scanReq.bssid));
		scanReq.channels[0] = req->channel;
		scanReq.ssidLen = req->ssidLen;
		memcpy(scanReq.ssid, req->ssid, sizeof(scanReq.ssid));

		do {
			if (remoteNetAdhocScan(&scanReq, &probeOut, sizeof(probeOut)) != 0)
				break;
		} while (--tries != 0);
	}

	matchIdx = -1;
	for (i = 0; i < PSPEMU_NET_ADHOC_CACHE_MAX; i++) {
		const PspemuWlanBssRaw *raw = &g_adhocBssCache[i].raw;

		if ((req->bssid[0] | req->bssid[1] | req->bssid[2] |
		     req->bssid[3] | req->bssid[4] | req->bssid[5]) != 0U) {
			if (memcmp(req->bssid, raw->bssid, 6U) == 0) {
				matchIdx = i;
				break;
			}
		}
		if (req->ssidLen == raw->ssidLen &&
		    memcmp(req->ssid, raw->ssid, req->ssidLen) == 0) {
			matchIdx = i;
			break;
		}
	}

	if (matchIdx < 0)
		return PSPEMU_NET_ERR_ADHOC_NOT_FOUND;

	{
		const PspemuWlanBssRaw *src = &g_adhocBssCache[matchIdx].raw;
		memset(&joinReq, 0, sizeof(joinReq));
		joinReq.mode = 3U;
		joinReq.structSize = sizeof(joinReq);
		joinReq.ssidLen = src->ssidLen;
		memcpy(joinReq.ssid, src->ssid, src->ssidLen);
		memcpy(joinReq.bssid, src->bssid, sizeof(joinReq.bssid));
		joinReq.channel = src->channel;
		joinReq.networkType = 2U;
		joinReq.beaconInterval = src->beaconInterval;
		joinReq.atimWindow = src->atimWindow;
		memcpy(joinReq.rates, src->rates, sizeof(joinReq.rates));
	}

	if (sceNetWlanAdhocJoin(PSPEMU_NET_WLAN_IF_ID, &joinReq) != 0 ||
	    remoteNetAdhocOpenSocket() != 0) {
		ret = PSPEMU_NET_ERR_ADHOC_JOIN_FAIL;
	} else {
		int waitMs;
		memcpy(g_adhocActiveBssid, req->bssid, sizeof(g_adhocActiveBssid));
		g_adhocThrottleCounter = 1000;
		waitMs = pspemuCompatGetParam(8);
		if (waitMs > 0) {
			int rem = waitMs;
			while (rem > 0 && !remoteNetIsSuspended()) {
				sceKernelDelayThread(1000U);
				rem--;
			}
		}
		ret = 0;
	}
	return ret;
}

static int remoteNetAdhocCreate(PspemuAdhocJoinReq *req)
{
	PspemuWlanAdhocReq createReq;
	int ret;

	if (g_adhocModeEnabled == 0)
		remoteNetAdhocStartMode();

	memset(&createReq, 0, sizeof(createReq));
	createReq.mode = 3U;
	createReq.structSize = sizeof(createReq);
	createReq.ssidLen = req->ssidLen;
	memcpy(createReq.ssid, req->ssid, req->ssidLen);
	createReq.channel = req->channel;
	createReq.networkType = 2U;
	createReq.beaconInterval = 34U;
	createReq.atimWindow = 100U;
	createReq.ratePrefix = 0U;
	createReq.rates[0] = 0x160B8482U;

	if (sceNetWlanAdhocCreate(PSPEMU_NET_WLAN_IF_ID, &createReq) != 0) {
		ret = PSPEMU_NET_ERR_ADHOC_CREATE;
	} else {
		memcpy(req->bssid, createReq.bssid, sizeof(req->bssid));
		if (remoteNetAdhocOpenSocket() != 0) {
			ret = PSPEMU_NET_ERR_ADHOC_CREATE;
		} else {
			memcpy(g_adhocActiveBssid, req->bssid, sizeof(g_adhocActiveBssid));
			g_adhocThrottleCounter = 1000;
			ret = 0;
		}
	}

	if (pspemuCompatGetParam(21) == 1)
		return 0;
	return ret;
}

static int remoteNetAdhocLeave(int withDelay)
{
	int stat;
	SceUInt timeout;
	int leaveDelayMs;
	int i;

	g_adhocThrottleCounter = ((unsigned int)g_adhocThrottleCounter >= 1000U) ? 50 : 0;
	if (g_adhocModeEnabled == 0)
		return PSPEMU_NET_ERR_NOT_INIT;

	if (withDelay != 0)
		sceKernelDelayThread(5000U);

	sceNetWlanAdhocLeave(PSPEMU_NET_WLAN_IF_ID);

	sceKernelLockLwMutex(&g_adhocMutex, 1, NULL);
	if (g_adhocSocketOpen != 0) {
		timeout = 0xFFFFFFFFU;
		sceNetEpollAbort(g_adhocEpollId, 0);
		sceKernelWaitThreadEnd(g_adhocPollThreadId, &stat, &timeout);
		sceKernelDeleteThread(g_adhocPollThreadId);
		sceNetEpollDestroy(g_adhocEpollId);
		sceNetSocketClose(g_adhocSocket);
		remoteNetTeardownAdhocIf();
		for (i = 0; i < PSPEMU_NET_ADHOC_RX_SLOTS; i++)
			g_adhocRxRing[i].length = 0;
		g_adhocSocketOpen = 0;
		g_adhocRxReadIdx = 0;
		g_adhocRxWriteIdx = 0;
		g_adhocRxBurstCount = 0;
	}
	sceKernelUnlockLwMutex(&g_adhocMutex, 1);

	memset(g_adhocBssCache, 0, sizeof(g_adhocBssCache));
	g_adhocScanCompleted = 0;

	leaveDelayMs = pspemuCompatGetParam(13);
	if (leaveDelayMs != -1 && withDelay != 0)
		sceKernelDelayThread((SceUInt)(leaveDelayMs * 1000));
	return 0;
}

static int remoteNetAdhocSend(const uint8_t *msg, uint32_t len)
{
	int ret;

	if (g_adhocModeEnabled == 0)
		return (int)len;
	if (g_adhocSocketOpen == 0)
		return PSPEMU_NET_ERR_NOT_INIT;

	if (g_adhocTxRateInterval > 0 && msg[15] != 1U &&
	    ((g_adhocTxRateInterval & 1) != 0 || msg[0] == 0xFFU)) {
		SceUInt64 now = sceKernelGetProcessTimeWide();
		int quota;

		if (g_adhocTxWindowEnd < now) {
			g_adhocTxQuotaRem = g_adhocTxQuotaMax;
			g_adhocTxWindowEnd = now + (SceUInt64)(int64_t)g_adhocTxRateInterval;
			quota = g_adhocTxQuotaMax;
		} else {
			quota = g_adhocTxQuotaRem;
		}
		if (quota < 0)
			return (int)len;
		g_adhocTxQuotaRem = quota - 1;
	}

	if (g_adhocShortPktDelayUsec != 0 && len == 176U)
		sceKernelDelayThread((SceUInt)g_adhocShortPktDelayUsec);

	ret = sceNetSend(g_adhocSocket, msg, len, 0);
	if (g_adhocTxDelayUsec > 0)
		sceKernelDelayThread((SceUInt)g_adhocTxDelayUsec);
	return ret;
}

static int remoteNetAdhocRecv(void *outBuf)
{
	uint32_t idx;
	PspemuAdhocRxSlot *slot;
	int pktLen;

	if (g_adhocModeEnabled == 0 || g_adhocSocketOpen == 0)
		return PSPEMU_NET_ERR_NOT_INIT;

	idx = g_adhocRxReadIdx;
	slot = &g_adhocRxRing[idx];
	if (slot->length != 0) {
		pktLen = slot->length;
		memcpy(outBuf, slot->packet, (size_t)pktLen);
		g_adhocRxReadIdx = (idx + 1U) & (PSPEMU_NET_ADHOC_RX_SLOTS - 1U);
		slot->length = 0;
		return pktLen;
	}

	g_adhocRxBurstCount = 0;
	return PSPEMU_NET_ERR_ADHOC_NO_ENTRY;
}

static int remoteNetAdhocEnsureStarted(void)
{
	if (g_adhocModeEnabled != 0)
		return 0;
	return remoteNetAdhocStartMode();
}

static int remoteNetAdhocTerm(void)
{
	if (g_adhocModeEnabled != 0) {
		remoteNetCtlSwitchAdhocMode(1);
		g_adhocModeEnabled = 0;
	}
	return 0;
}

static int remoteNetAdhocInit(void)
{
	int ret;
	int filterIdx;
	int rateParam;
	int delayParam;

	ret = sceKernelCreateLwMutex(&g_adhocMutex, "ScePspemuNetAdhoc", 0x2000U, 0, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelCreateEventFlag("ScePspemuNetAdhoc", 0, 0, NULL);
	g_adhocPollReadyEvf = ret;
	if (ret < 0)
		return ret;

	filterIdx = pspemuCompatGetParam(3);
	if (filterIdx < 0) {
		if (pspemuCompatGetParam(1) > 0 || pspemuCompatGetParam(10) > 0)
			g_adhocTitleFilter = pspemuCompatGetTitleId();
	} else if (filterIdx <= 4) {
		g_adhocTitleFilter = g_adhocTitleFilterTable[filterIdx];
	}

	rateParam = pspemuCompatGetParam(7);
	g_adhocTxRateInterval = rateParam;
	if (rateParam > 0) {
		g_adhocTxQuotaMax = rateParam >> 22;
		g_adhocTxRateInterval = rateParam & 0x3FFFFF;
	}

	delayParam = pspemuCompatGetParam(5);
	g_adhocTxDelayUsec = delayParam;
	if (delayParam > 0) {
		if ((delayParam & 1) != 0) {
			g_adhocTxDelayUsec = 0;
			g_adhocShortPktDelayUsec = delayParam;
			return 0;
		}
		return 0;
	}
	return 0;
}

static int remoteNetTranslateError(int vitaErr)
{
	PspemuNetErrorMap table[26];
	int i;

	memcpy(table, g_netErrTable, sizeof(table));
	for (i = 0; table[i].vitaErr != 0; i++) {
		if (table[i].vitaErr == vitaErr)
			return table[i].pspErrno;
	}
	return vitaErr;
}

static int remoteNetInetMsgDlgThread(SceSize argSize, void *argp)
{
	SceDisplayFrameBuf fb;
	int ret;

	(void)argSize;
	(void)argp;

	for (;;) {
		ret = sceKernelWaitEventFlag(g_inetMsgDlgEvf, 1U, 0, NULL, NULL);
		if (ret < 0)
			break;

		pspemuSettingsSetMenuEnabled(0);
		pspemuSettingsShowGuideDialog(8);
		fb.size = sizeof(fb);
		fb.base = pspemuGetActiveFrameBuffer();
		fb.width = PSPEMU_SCREEN_WIDTH;
		fb.height = PSPEMU_SCREEN_HEIGHT;
		fb.pitch = PSPEMU_SCREEN_PITCH;
		fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
		sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
		pspemuSettingsSetMenuEnabled(1);

		ret = sceKernelClearEventFlag(g_inetMsgDlgEvf, ~1U);
		if (ret < 0)
			break;
	}
	return ret;
}

static int remoteNetInetConnectCore(int withDisconnectFirst)
{
	int cid;
	int ret;

	remoteNetAdhocLeave(0);
	if (withDisconnectFirst != 0)
		SceNetCtl_2792E1B6();

	SceNetCtl_AB1462C5(2);
	g_inetIpObtained = 0;
	g_netCtlDisconnectEvent = 0;
	g_inetDisconnected = 0;

	ret = SceNetCtl_AC326DB7(remoteNetCtlInetConnectCb, NULL, &cid);
	if (ret >= 0) {
		ret = SceNetCtl_DD96DA85();
		while (g_netCtlDisconnectEvent == 0 && g_inetAbortRequested == 0) {
			SceNetInternal_104E4820(10000);
			sceNetCtlCheckCallback();
		}

		if (g_netCtlDisconnectEvent != 0) {
			ret = SceNetCtl_2CF683AD();
			while (g_inetIpObtained == 0 && g_inetAbortRequested == 0) {
				if (g_inetDisconnected != 0) {
					ret = PSPEMU_NET_ERR_INET_ABORTED;
					break;
				}
				sceKernelDelayThread(100000U);
				sceNetCtlCheckCallback();
			}
			if (g_inetIpObtained != 0 && ret >= 0)
				ret = 0;
		}
		SceNetCtl_229EAA64(cid);
	}
	return ret;
}

static int remoteNetInetConnectThread(SceSize argSize, void *argp)
{
	int ret;

	(void)argSize;
	(void)argp;

	for (;;) {
		ret = sceKernelWaitSema(g_inetConnectSema, 1, NULL);
		if (ret != 0)
			break;
		if (remoteNetInetConnectCore(1) < 0) {
			int fallback = remoteNetInetConnectCore(0);
			if (fallback < 0)
				g_inetState = fallback;
		}
	}
	return ret;
}

int remoteNetIsAdhocActive(void)
{
	return sceKernelPollEventFlag(g_inetMsgDlgEvf, 1U, 0, NULL) == 0;
}

int remoteNetSetThrottle(int enabled)
{
	if (enabled == 1) {
		if (!pspemuSettingsIsGuideDialogIdle())
			return 0;
		sceKernelSetEventFlag(g_inetMsgDlgEvf, 1U);
	}
	g_inetThrottleEnabled = enabled;
	return 0;
}

static int remoteNetInetSocket(int domain, int type, int protocol, int *outSock)
{
	int nbio = 1;
	int ret;

	ret = sceNetSocket("ScePspemuInet", domain, type, protocol);
	if (ret >= 0) {
		*outSock = ret;
		ret = sceNetSetsockopt(ret, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &nbio, sizeof(nbio));
	}
	return remoteNetTranslateError(ret);
}

static int remoteNetInetClose(SceNetId s)
{
	return remoteNetTranslateError(sceNetSocketClose(s));
}

static int remoteNetInetBind(SceNetId s, const SceNetSockaddr *addr, SceNetSocklen_t addrLen)
{
	return remoteNetTranslateError(sceNetBind(s, addr, addrLen));
}

static int remoteNetInetListen(SceNetId s, int backlog)
{
	return remoteNetTranslateError(sceNetListen(s, backlog));
}

static int remoteNetInetShutdown(SceNetId s, int how)
{
	return remoteNetTranslateError(sceNetShutdown(s, how));
}

static int remoteNetInetConnect(SceNetId s, const SceNetSockaddr *addr, SceNetSocklen_t addrLen)
{
	return remoteNetTranslateError(sceNetConnect(s, addr, addrLen));
}

static uint32_t remoteNetInetPoll(SceNetId s, int events)
{
	SceNetEpollEvent ctrlEv;
	SceNetEpollEvent waitEv;
	SceNetId epollId;
	int ret;

	epollId = sceNetEpollCreate("ScePspemuInet", 0);
	if (epollId < 0)
		return 25U;

	memset(&ctrlEv, 0, sizeof(ctrlEv));
	ctrlEv.events = ((events & 0xC1) != 0) ? SCE_NET_EPOLLIN : 0U;
	if ((events & 0x104) != 0)
		ctrlEv.events |= SCE_NET_EPOLLOUT;

	ret = sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_ADD, s, &ctrlEv);
	if (ret < 0) {
		sceNetEpollDestroy(epollId);
		if (ret == PSPEMU_NET_ERR_EPOLL_BADF)
			return (uint32_t)(events | 8);
		return 0U;
	}

	ret = sceNetEpollWait(epollId, &waitEv, 1, 0);
	sceNetEpollDestroy(epollId);
	if ((waitEv.events & SCE_NET_EPOLLOUT) != 0U)
		waitEv.events = (waitEv.events & ~6U) | 4U;
	if (ret != 0)
		return waitEv.events;
	return 0U;
}

static int remoteNetInetAccept(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen, int *outSock)
{
	int ret;

	ret = sceNetAccept(s, addr, addrLen);
	if (ret < 0)
		return remoteNetTranslateError(ret);
	*outSock = ret;
	return 0;
}

static int remoteNetInetGetpeername(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen)
{
	return remoteNetTranslateError(sceNetGetpeername(s, addr, addrLen));
}

static int remoteNetInetGetsockname(SceNetId s, SceNetSockaddr *addr, SceNetSocklen_t *addrLen)
{
	return remoteNetTranslateError(sceNetGetsockname(s, addr, addrLen));
}

static int remoteNetInetGetsockopt(SceNetId s, int level, int optname, void *optval, SceNetSocklen_t *optlen)
{
	return remoteNetTranslateError(sceNetGetsockopt(s, level, optname, optval, optlen));
}

static int remoteNetInetSetsockopt(SceNetId s, int level, int optname, const void *optval, SceNetSocklen_t optlen)
{
	return remoteNetTranslateError(sceNetSetsockopt(s, level, optname, optval, optlen));
}

static int remoteNetInetRecv(SceNetId s, void *buf, uint32_t len, int flags,
                             SceNetSockaddr *from, SceNetSocklen_t *fromlen, int *outLen)
{
	int ret;

	if (remoteNetIsSuspended() != 0)
		return 110;
	if (len == 0U) {
		*outLen = 0;
		return 0;
	}

	if (fromlen != NULL && *fromlen != 0U)
		ret = sceNetRecvfrom(s, buf, len, flags | SCE_NET_MSG_DONTWAIT, from, fromlen);
	else
		ret = sceNetRecv(s, buf, len, flags | SCE_NET_MSG_DONTWAIT);
	if (ret < 0)
		return remoteNetTranslateError(ret);
	*outLen = ret;
	return 0;
}

static int remoteNetInetSend(SceNetId s, const void *buf, uint32_t len, int flags,
                             const SceNetSockaddr *to, int tolen, int *outLen)
{
	int ret;

	if (remoteNetIsSuspended() != 0)
		return 110;
	if (g_inetThrottleEnabled != 0)
		return 128;

	if (to != NULL && tolen > 0)
		ret = sceNetSendto(s, buf, len, flags | SCE_NET_MSG_DONTWAIT, to, (SceNetSocklen_t)tolen);
	else
		ret = sceNetSend(s, buf, len, flags | SCE_NET_MSG_DONTWAIT);
	if (ret < 0)
		return remoteNetTranslateError(ret);
	*outLen = ret;
	return 0;
}

static int remoteNetInetIoctl(SceNetId s, unsigned int cmd, void *arg)
{
	return remoteNetTranslateError(SceNetInternal_333F9CB3(s, cmd, arg));
}

static int remoteNetAdhocLeaveCmd(void)
{
	remoteNetAdhocLeave(0);
	return 0;
}

static int remoteNetInetInit(void)
{
	SceUID sema;
	SceUID thid;
	SceUID evf;
	int ret;

	sema = sceKernelCreateSema("ScePspemuInetConnect", 0, 0, 1, NULL);
	if (sema < 0)
		return sema;
	g_inetConnectSema = sema;

	thid = sceKernelCreateThread("ScePspemuInetConnect",
	                             (SceKernelThreadEntry)remoteNetInetConnectThread,
	                             95, 0x1000U, 0, 0x10000, NULL);
	if (thid < 0)
		return thid;

	ret = sceKernelStartThread(thid, 0, NULL);
	if (ret != 0)
		return ret;

	evf = sceKernelCreateEventFlag("ScePspemuInetMsgDlg", 0, 0, NULL);
	g_inetMsgDlgEvf = evf;
	if (evf < 0)
		return evf;

	thid = sceKernelCreateThread("ScePspemuInetMsgDlg",
	                             (SceKernelThreadEntry)remoteNetInetMsgDlgThread,
	                             95, 0x1000U, 0, 0x10000, NULL);
	if (thid < 0)
		return thid;
	return sceKernelStartThread(thid, 0, NULL);
}

static int remoteNetInetStartConnect(void)
{
	if (g_inetState == PSPEMU_NET_CTL_STATE_IPOBTAINED)
		return 0;
	g_inetAbortRequested = 0;
	return sceKernelSignalSema(g_inetConnectSema, 1);
}

static int remoteNetInetGetInfo(PspemuInetInfo *outInfo)
{
	SceNetCtlInfo info;

	if (g_inetState != PSPEMU_NET_CTL_STATE_IPOBTAINED)
		return g_inetState;

	g_inetSocketsOpened = 1;

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &info) == 0)
		sceNetInetPton(SCE_NET_AF_INET, info.ip_address, &outInfo->ipAddress);
	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEFAULT_ROUTE, &info) == 0)
		sceNetInetPton(SCE_NET_AF_INET, info.default_route, &outInfo->defaultRoute);
	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_NETMASK, &info) == 0)
		sceNetInetPton(SCE_NET_AF_INET, info.netmask, &outInfo->netmask);
	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_PRIMARY_DNS, &info) == 0)
		sceNetInetPton(SCE_NET_AF_INET, info.primary_dns, &outInfo->dnsMain);
	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_SECONDARY_DNS, &info) == 0)
		sceNetInetPton(SCE_NET_AF_INET, info.secondary_dns, &outInfo->dnsFallback);

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_BSSID, &info) == 0) {
		memcpy(outInfo->bssid, &info.bssid, sizeof(outInfo->bssid));
	} else {
		memset(outInfo->bssid, 0xFF, sizeof(outInfo->bssid));
	}

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_CNF_NAME, &info) == 0)
		memcpy(outInfo->cnfName, info.cnf_name, sizeof(outInfo->cnfName));

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_RSSI_PERCENTAGE, &info) == 0)
		outInfo->rssiPercentage = (uint16_t)info.rssi_percentage;
	else
		outInfo->rssiPercentage = 100U;

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_SSID, &info) == 0) {
		memcpy(outInfo->ssid, info.ssid, sizeof(outInfo->ssid));
		outInfo->ssidLen = (uint8_t)strlen(info.ssid);
	} else {
		strncpy(outInfo->ssid, "(USB Connection)", 32U);
		outInfo->ssidLen = 16U;
	}

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_CHANNEL, &info) == 0)
		outInfo->channel = (uint16_t)info.channel;
	else
		outInfo->channel = 0U;

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_WIFI_SECURITY, &info) == 0)
		outInfo->wifiSecurity = info.wifi_security;
	else
		outInfo->wifiSecurity = 0U;

	if (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_HTTP_PROXY_CONFIG, &info) == 0)
		outInfo->httpProxyConfig = info.http_proxy_config;

	return g_inetState;
}

static int remoteNetInetAbort(int errorCode)
{
	if (errorCode != 0)
		g_inetState = errorCode;
	g_inetAbortRequested = 1;
	return 0;
}

static int remoteNetInetDisconnect(void)
{
	if (g_inetSocketsOpened != 0 && pspemuCompatHasFlag(0x8000U) != 0) {
		int sockCount;

		g_inetSocketsOpened = 0;
		sockCount = sceNetGetSockInfo(-1, NULL, 0, 0);
		if (sockCount > 0) {
			SceNetSockInfo *list = (SceNetSockInfo *)malloc((size_t)sockCount * sizeof(SceNetSockInfo));
			if (list != NULL) {
				int got = sceNetGetSockInfo(-1, list, sockCount, 0);
				int i;

				if (got < sockCount)
					sockCount = got;
				for (i = 0; i < sockCount; i++) {
					if (list[i].state == 2 && list[i].s >= 0)
						sceNetSocketClose(list[i].s);
				}
				free(list);
			}
		}
	}
	g_inetState = 0;
	return 0;
}
