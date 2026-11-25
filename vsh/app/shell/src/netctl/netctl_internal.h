/*
 * netctl_internal.h: internal structures, constants, and cross-file prototypes
 * for SceNetCtlInt and SceNetCtlIpcInt inside SceShell.
 */
#ifndef SHELL_NETCTL_INTERNAL_H
#define SHELL_NETCTL_INTERNAL_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <net.h>
#include <libnetctl.h>
#include <libnet/netinternal.h>
#include "netctl_services.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal SceNetCtl error codes beyond public libnetctl.h */
#define NETCTL_ERR_NOT_INITIALIZED		-2143215231	/* 0x80412181 */
#define NETCTL_ERR_ALREADY_INITIALIZED		-2143215230	/* 0x80412182 */
#define NETCTL_ERR_INVALID_ADDR			-2143215228	/* 0x80412184 */
#define NETCTL_ERR_CALLBACK_MAX			-2143215227	/* 0x80412185 */
#define NETCTL_ERR_INVALID_ID			-2143215226	/* 0x80412186 */
#define NETCTL_ERR_BUSY				-2143215225	/* 0x80412187 */
#define NETCTL_ERR_INVALID_DEVICE		-2143215224	/* 0x80412188 */
#define NETCTL_ERR_INVALID_MTU			-2143215223	/* 0x80412189 */
#define NETCTL_ERR_INVALID_SECURITY		-2143215222	/* 0x8041218A */
#define NETCTL_ERR_INVALID_KEY_LEN		-2143215221	/* 0x8041218B */
#define NETCTL_ERR_INVALID_IP_CONFIG		-2143215220	/* 0x8041218C */
#define NETCTL_ERR_INVALID_DNS_FLAG		-2143215219	/* 0x8041218D */
#define NETCTL_ERR_INVALID_PROXY_FLAG		-2143215218	/* 0x8041218E */
#define NETCTL_ERR_INVALID_IP_ADDR		-2143215217	/* 0x8041218F */
#define NETCTL_ERR_INVALID_SSID			-2143215216	/* 0x80412190 */
#define NETCTL_ERR_INVALID_SSID_COUNT		-2143215215	/* 0x80412191 */
#define NETCTL_ERR_INVALID_KEY_CHAR		-2143215213	/* 0x80412193 */
#define NETCTL_ERR_INVALID_CONFIG_ID		-2143215212	/* 0x80412194 */
#define NETCTL_ERR_CONFIG_NOT_SET		-2143215211	/* 0x80412195 */
#define NETCTL_ERR_MODE_CONFLICT		-2143215210	/* 0x80412196 */
#define NETCTL_ERR_PATH_TOO_LONG		-2143215209	/* 0x80412197 */
#define NETCTL_ERR_SCAN_INDEX_OOB		-2143215208	/* 0x80412198 */
#define NETCTL_ERR_SCAN_IN_PROGRESS		-2143215207	/* 0x80412199 */
#define NETCTL_ERR_INET_IN_USE			-2143215206	/* 0x8041219A */
#define NETCTL_ERR_BSSID_NOT_FOUND		-2143215203	/* 0x8041219D */
#define NETCTL_ERR_INVALID_SIZE			-2143215202	/* 0x8041219E */
#define NETCTL_ERR_SCAN_NO_RESULT		-2143215201	/* 0x8041219F */
#define NETCTL_ERR_DISCONNECT_REQUESTED		-2143215200	/* 0x804121A0 */
#define NETCTL_ERR_LINK_CHANGED			-2143215196	/* 0x804121A4 */
#define NETCTL_ERR_AUTO_DISCONNECT_REQ		-2143215195	/* 0x804121A5 */
#define NETCTL_ERR_MUTEX_NOT_READY		-2143215166	/* 0x804121C2 */
#define NETCTL_ERR_INVALID_CHANNEL		-2143215164	/* 0x804121C4 */
#define NETCTL_ERR_NOT_ALLOWED_PROCESS		-2143215165	/* 0x804121C3 */
#define NETCTL_ERR_MOBILE_LINK_DOWN		-2143215162	/* 0x804121C6 */
#define NETCTL_ERR_MOBILE_DISCONNECTED		-2143215161	/* 0x804121C7 */
#define NETCTL_ERR_WISPR_AUTH_FAILED		-2143215160	/* 0x804121C8 */
#define NETCTL_ERR_TABLE_FULL			-2143215159	/* 0x804121C9 */
#define NETCTL_ERR_ALREADY_LINKED		-2143215158	/* 0x804121CA */
#define NETCTL_ERR_NET_FLAG_DISABLED		-2143215153	/* 0x804121CF */
#define NETCTL_ERR_NO_INTERFACE			-2143215152	/* 0x804121D0 */

#define NETCTL_WLAN_ERR_SCAN_RETRY		-2143218175	/* 0x80411601 */
#define NETCTL_WLAN_ERR_WEP_AUTH_FAIL		-2143218173	/* 0x80411603 */
#define NETCTL_NET_ERR_FLIGHT_MODE		-2143218047	/* 0x80411681 */
#define NETCTL_NET_ERR_WIFI_DISABLED		-2143218046	/* 0x80411682 */
#define NETCTL_NET_ERR_NO_DEFAULT_ROUTE		-2143218932	/* 0x8041130C */
#define NETCTL_EXCL_ERR_CANCELED		-2146425851	/* 0x80102405 */
#define NETCTL_EXCL_ERR_BUSY			-2146425848	/* 0x80102408 */

#define NETCTL_MAX_CONFIGS			30
#define NETCTL_MAX_CALLBACKS			16
#define NETCTL_MAX_PROCESS_SLOTS		8
#define NETCTL_MAX_PROCESS_LINKS		8
#define NETCTL_MAX_SCAN_SSIDS			10
#define NETCTL_CUSTOM_IE_MAX			0x400u
#define NETCTL_ADHOC_COOLDOWN_USEC		5000000ULL

/* Channel flags (+0x28) */
#define NETCTL_FLAG_IDLE			0x00000001u
#define NETCTL_FLAG_EXIT			0x00000002u
#define NETCTL_FLAG_LINK_UP			0x00000004u
#define NETCTL_FLAG_IP_OBTAINED			0x00000008u
#define NETCTL_FLAG_RECONNECTING		0x00000010u
#define NETCTL_FLAG_EXCLUSIVE_ACTIVE		0x00000100u
#define NETCTL_FLAG_IFSTAT_SAVED		0x00000200u
#define NETCTL_FLAG_IFSTAT_DIRTY		0x00000400u
#define NETCTL_FLAG_ETH_IFSTAT_DIRTY		0x00000800u
#define NETCTL_FLAG_SUSPENDED			0x00001000u
#define NETCTL_FLAG_SUSPEND_DISCONN		0x00002000u
#define NETCTL_FLAG_ICM_BLOCKED			0x00004000u
#define NETCTL_FLAG_ICM_PENDING			0x00008000u
#define NETCTL_FLAG_POWERSAVE_ACTIVE		0x00010000u
#define NETCTL_FLAG_POWERSAVE_ON		0x00020000u
#define NETCTL_FLAG_BOOT_DELAY_DONE		0x00040000u
#define NETCTL_FLAG_FLUSH_IFSTAT_NOW		0x00080000u

/* Event flag bits on channel->eventFlagId */
#define NETCTL_EV_CONNECT			0x00000001u
#define NETCTL_EV_AUTO_CONNECT			0x00000002u
#define NETCTL_EV_DISCONNECT			0x00000004u
#define NETCTL_EV_SCAN				0x00000008u
#define NETCTL_EV_LINK_ERROR			0x00000010u
#define NETCTL_EV_IP_READY			0x00000020u
#define NETCTL_EV_AUTO_DISCONNECT		0x00000040u
#define NETCTL_EV_EXIT				0x00000080u
#define NETCTL_EV_ADHOC_PREEMPT			0x00000400u
#define NETCTL_EV_EXCLUSIVE_DONE		0x00000800u
#define NETCTL_EV_WISPR_OK			0x00001000u
#define NETCTL_EV_WISPR_FAIL			0x00002000u
#define NETCTL_EV_EXCLUSIVE_ABORT		0x00004000u
#define NETCTL_EV_WLAN_IF_UP			0x00008000u
#define NETCTL_EV_WLAN_IF_DOWN			0x00010000u
#define NETCTL_EV_WLAN_IF_ERR			0x00020000u
#define NETCTL_EV_OTHER_IF_UP			0x00040000u
#define NETCTL_EV_OTHER_IF_DOWN			0x00080000u
#define NETCTL_EV_OTHER_IF_ERR			0x00100000u

/* Raw internal channel states */
#define NETCTL_RAW_STATE_IDLE			0
#define NETCTL_RAW_STATE_AUTO_SELECT		1
#define NETCTL_RAW_STATE_LINK_UP		2
#define NETCTL_RAW_STATE_IP_CONFIG		3
#define NETCTL_RAW_STATE_CONNECTED		4
#define NETCTL_RAW_STATE_WISPR_AUTH		5

/* Interface IDs in SceNetInternal */
#define NETCTL_IF_LOOPBACK			0
#define NETCTL_IF_ETHERNET			1
#define NETCTL_IF_WLAN				4
#define NETCTL_IF_PHONE_ALT			7
#define NETCTL_IF_PHONE				8
#define NETCTL_IF_PPPOE				9

/* SceNetInternal imports used by SceNetCtl */
int SceNetInternal_01B090F1(int ifId, void *ifInfo, int flags);
int SceNetInternal_04E6136D(int flags, void *filterCtx, int unused);
int SceNetInternal_16222ED6(const void *dnsPair, int flags);
int SceNetInternal_235DE96C(int ifId, void *ifInfo, int count);
int SceNetInternal_26EFF46D(int ifId, int errCode, int isSuspend);
int SceNetInternal_4F5E0E8A(int ifId, int reason, int flags);
int SceNetInternal_689B9D7D(int *outRandom);
int SceNetInternal_7F63379E(void *outDnsPair, int flags);
int SceNetInternal_8157DE3E(int *outIfIds, int maxCount, int flags);
int SceNetInternal_93F2FF08(int sockId, int option);
int SceNetInternal_A6B9F5D2(const void *tcpMssPair, int flags);
int SceNetInternal_AA414742(int ifId, int flags, SceUInt32 ioctlCode, void *buf, SceSize size);
int SceNetInternal_B01ABD0A(int ifId, int *outError, int flags);
int SceNetInternal_E03F6A77(int ifId, const void *sleepParam, int flags);
int SceNetInternal_EAC33599(int ifId, void *eventFilter, int arg, int flags);
int SceNetInternal_EDAA3453(int ifId, int flags);
int SceNetInternal_EE94BA15(int ifId, void *outStat, int clearAfterRead);

/* VSH / kernel imports */
int vshKernelCheckModelCapability(int cap);
SceUInt32 vshSblAimgrIsDolce(void);
int _vshEventLogPut(int category, int eventId, int flags, const void *entry);
int sceKernelGetRemoteProcessTimeForUser(SceUID processId, SceKernelSysClock *pClock);
SceUInt64 sceKernelGetSystemTimeWideForUser(void);
SceUInt64 sceRtcGetAccumulativeTimeForUser(void);
int scePowerRegisterCallbackForUser(SceUID cbid);
int sceWlanSetConfiguration(int enable);

/* Statically linked SceNetConfig / SceNetDhcp / SceNetPppoe / SceNetWlan (0x81378940..0x8137C0D8) */
int sceNetConfigUpInterface(int ifId);
int sceNetConfigDownInterface(int ifId);
int sceNetConfigCleanUpAllInterfaces(void);
int sceNetConfigSetIfmtu(int ifId, int mtu);
int sceNetConfigGetIfaddrs(int ifId, void *outAddrs, int maxCount);
int sceNetConfigDelIfaddr(int ifId);
int sceNetConfigSetIfaddr(int ifId, const void *ipAddr, const void *peerAddr, const void *netmask);
int sceNetConfigAddRoute(const void *dstAddr, const void *gateway, const void *netmask, int isHost);
int sceNetConfigAddMRoute(const void *gateway);
int sceNetConfigDelMRoute(const void *gateway);
int sceNetConfigDelDefaultRoute(void);
int sceNetConfigSetDefaultRoute(const void *gateway);
int sceNetConfigGetDefaultRoute(SceNetInAddr *outGateway);
int sceNetDhcpStart(int ifId, void (*callback)(void *arg, int event, int err), void *arg,
		    SceSize stackSize, int priority, const void *dhcpParam);
int sceNetDhcpStop(int ifId);
int sceNetPppoeStart(int ifId, int pppoeIfId, void (*callback)(void *arg, int event, int err),
		     void *arg, SceSize stackSize, int priority, const void *pppoeParam);
int sceNetPppoeStop(int pppoeIfId);
int sceNetWlanScan(int ifId, const void *req, SceSize reqSize, void *outBuf, SceSize outBufSize,
		   void **outBssList, SceUInt32 *outBssCount);
int sceNetWlanConnect(int ifId, const void *req, void *outBuf, SceSize outBufSize, void **outBss);
int sceNetWlanDisconnect(int ifId);
int sceNetWlanGetRssi(int ifId, void *outRssiBuf);
int sceNetWlanAdhocCreate(int ifId, void *req);
int sceNetWlanAdhocJoin(int ifId, void *req);
int sceNetWlanAdhocLeave(int ifId);
int sceNetEthGetLinkStatus(int ifId, int *outStatus);
int sceNetPhoneGetStatus(int ifId, int *outStatus);
int sceNetPhoneNotifyEvent(int ifId, int eventType);

typedef void (*NetCtlEventCallback)(int prevState, int newState, int eventCode,
				    int errorCode, int reserved, void *userArg);

typedef struct NetCtlCallbackSlot {
	int id;
	NetCtlEventCallback callback;
	void *userArg;
} NetCtlCallbackSlot;

/* Registry config entry: 1144 bytes (0x478) */
typedef struct NetCtlRegConfig {
	SceUInt32 configId;				/* +0x000 */
	int confFlag;					/* +0x004 */
	int confType;					/* +0x008 */
	char confName[65];				/* +0x00C */
	char padConfName[3];				/* +0x04D */
	SceUInt32 confSerialNo;				/* +0x050 */
	int enableAutoConnect;				/* +0x054 */
	SceUInt32 mtu;					/* +0x058 */
	SceUInt32 device;				/* +0x05C */
	char ssid[33];					/* +0x060 */
	char padSsid[3];				/* +0x081 */
	SceUInt32 wifiSecurity;				/* +0x084 */
	char wepKey[27];				/* +0x088 */
	char padWepKey;					/* +0x0A3 */
	char wpaKey[65];				/* +0x0A4 */
	char padWpaKey[3];				/* +0x0E5 */
	SceUInt32 ipConfig;				/* +0x0E8 */
	char dhcpHostname[256];				/* +0x0EC */
	char authName[128];				/* +0x1EC */
	char authKey[128];				/* +0x26C */
	char ipAddress[16];				/* +0x2EC */
	char netmask[16];				/* +0x2FC */
	char defaultRoute[16];				/* +0x30C */
	SceUInt32 dnsFlag;				/* +0x31C */
	char primary_dns[16];				/* +0x320 */
	char secondary_dns[16];				/* +0x330 */
	SceUInt32 httpProxyFlag;			/* +0x340 */
	char httpProxyServer[256];			/* +0x344 */
	SceUInt32 httpProxyPort;			/* +0x444 */
	SceUInt32 reservedTail[12];			/* +0x448..+0x477 */
} NetCtlRegConfig;

/* Active network config: 1544 bytes (0x608) */
typedef struct NetCtlNetConfig {
	SceUInt32 size;					/* +0x000 */
	char cnfName[65];				/* +0x004 */
	char padCnfName[3];				/* +0x045 */
	SceUInt32 device;				/* +0x048 */
	SceUInt32 mtu;					/* +0x04C */
	char ssid[33];					/* +0x050 */
	SceUInt8 channel;				/* +0x071 */
	SceUInt8 padChannel[2];				/* +0x072 */
	SceUInt32 wifiSecurity;				/* +0x074 */
	char securityKey[65];				/* +0x078 */
	char padSecurityKey[3];				/* +0x0B9 */
	SceUInt32 ipConfig;				/* +0x0BC */
	char dhcpHostname[256];				/* +0x0C0 */
	char pppoeAuthName[128];			/* +0x1C0 */
	char pppoeAuthKey[128];				/* +0x240 */
	char ipAddress[16];				/* +0x2C0 */
	char netmask[16];				/* +0x2D0 */
	char defaultRoute[16];				/* +0x2E0 */
	SceUInt32 dnsFlag;				/* +0x2F0 */
	char primary_dns[16];				/* +0x2F4 */
	char secondary_dns[16];				/* +0x304 */
	SceUInt32 httpProxyFlag;			/* +0x314 */
	char httpProxyServer[256];			/* +0x318 */
	SceUInt32 httpProxyPort;			/* +0x418 */
	SceUInt32 enableAutoConnect;			/* +0x41C */
	SceUInt32 reservedMiddle[98];			/* +0x420..+0x5A7 */
	SceUInt32 configId;				/* +0x5A8 */
	SceUInt32 confSerialNo;				/* +0x5AC */
	SceUInt32 reservedMeta[6];			/* +0x5B0..+0x5C7 */
	const void *customIeProbe;			/* +0x5C8 */
	SceSize customIeProbeSize;			/* +0x5CC */
	const void *customIeAssoc;			/* +0x5D0 */
	SceSize customIeAssocSize;			/* +0x5D4 */
	SceUInt32 confType;				/* +0x5D8 */
	SceUInt32 reservedTail[11];			/* +0x5DC..+0x607 */
} NetCtlNetConfig;

/* Scan parameter: 56 bytes (0x38) */
typedef struct NetCtlScanParam {
	SceSize size;					/* +0x00 */
	SceUInt32 optionFlags;				/* +0x04 */
	SceUInt8 channelMask[32];			/* +0x08 */
	const void *customIeData;			/* +0x28 */
	SceSize customIeSize;				/* +0x2C */
	SceUInt32 reserved[2];				/* +0x30 */
} NetCtlScanParam;

/* Scan result info returned to callers: 96 bytes (0x60) */
typedef struct NetCtlScanInfo {
	SceUInt64 reservedHead;				/* +0x00 */
	SceUInt64 systemTime;				/* +0x08 */
	SceNetEtherAddr bssid;				/* +0x10 */
	SceUInt8 padBssid[2];				/* +0x16 */
	char ssid[32];					/* +0x18 */
	SceUInt8 padSsid[4];				/* +0x38 */
	SceUInt32 ssidLen;				/* +0x3C */
	SceUInt32 security;				/* +0x40 */
	SceUInt32 rssiDbm;				/* +0x44 */
	SceUInt32 rssiPercentage;			/* +0x48 */
	SceUInt32 channel;				/* +0x4C */
	SceUInt64 accumulativeTime;			/* +0x50 */
	SceUInt64 reservedTail;				/* +0x58 */
} NetCtlScanInfo;

/* WPA/RSN cipher info embedded in driver BSS record: 12 bytes */
typedef struct NetCtlWlanCipherInfo {
	SceUInt32 present;				/* +0x00 */
	SceUInt8 groupCipher;				/* +0x04 */
	SceUInt8 pairwiseCipher;			/* +0x05 */
	SceUInt8 akmSuite;				/* +0x06 */
	SceUInt8 pad;					/* +0x07 */
	SceUInt32 extra;				/* +0x08 */
} NetCtlWlanCipherInfo;

/* Driver WLAN BSS record header: 132 bytes (0x84), followed by ieBytes[ieLength] */
typedef struct NetCtlWlanBssRecord {
	SceUInt16 recordSize;				/* +0x00 */
	SceUInt16 ieLength;				/* +0x02 */
	SceUInt32 ssidLen;				/* +0x04 */
	char ssid[32];					/* +0x08 */
	SceNetEtherAddr bssid;				/* +0x28 */
	SceUInt8 channel;				/* +0x2E */
	SceUInt8 bssType;				/* +0x2F */
	SceUInt16 capability;				/* +0x30 */
	SceUInt16 rssiDbm;				/* +0x32 */
	SceUInt16 rssiPercentage;			/* +0x34 */
	SceUInt16 beaconInterval;			/* +0x36 */
	SceUInt8 supportedRates[36];			/* +0x38 */
	NetCtlWlanCipherInfo wpa;			/* +0x5C */
	NetCtlWlanCipherInfo rsn;			/* +0x68 */
	SceUInt8 trailing[16];				/* +0x74 */
} NetCtlWlanBssRecord;

/* WISPr status block cached for IPC query: 780 bytes (0x30C) */
typedef struct NetCtlWisprStatus {
	int errorCode;					/* +0x000 */
	int hasWisprResult;				/* +0x004 */
	NetWisprResultInfo result;			/* +0x008..+0x30B */
} NetCtlWisprStatus;

/* Process reference slot: 28 bytes (0x1C) */
typedef struct NetCtlProcessSlot {
	SceUID processId;				/* +0x00 */
	int disconnectOnExit;				/* +0x04 */
	int autoConnectSuppressed;			/* +0x08 */
	int imposePromptForced;				/* +0x0C */
	int allowedRefCount;				/* +0x10 */
	int frontRefCount;				/* +0x14 */
	SceUInt16 refToken;				/* +0x18 */
	SceUInt16 pad;					/* +0x1A */
} NetCtlProcessSlot;

/* Linked process entry: 36 bytes (9 ints) */
typedef struct NetCtlProcessLink {
	SceUID targetProcessId;				/* +0x00 */
	SceUID linkedProcesses[NETCTL_MAX_PROCESS_LINKS]; /* +0x04..+0x23 */
} NetCtlProcessLink;

/* Exclusive function result block (written by netCtlExclusiveCallback) */
typedef struct NetCtlExclusiveResult {
	int selectedConfigId;				/* +0x00 (channel +0x13B14) */
	SceUID clientPid;				/* +0x04 (channel +0x13B18) */
	int valid;					/* +0x08 (channel +0x13B1C) */
} NetCtlExclusiveResult;

/* Per-channel (Inet / Adhoc) state block: 0x142A8 (82600) bytes */
typedef struct NetCtlChannel {
	int isInet;					/* +0x00000 */
	int ifId;					/* +0x00004 */
	int state;					/* +0x00008 */
	int eventCode;					/* +0x0000C */
	SceUID threadId;				/* +0x00010 */
	SceUID eventFlagId;				/* +0x00014 */
	int threadPriority;				/* +0x00018 */
	int reservedHeader;				/* +0x0001C */
	int lastError;					/* +0x00020 */
	int scanError;					/* +0x00024 */
	SceUInt32 flags;				/* +0x00028 */
	int connectMode;				/* +0x0002C */
	int netFlag;					/* +0x00030 */
	int reservedState;				/* +0x00034 */
	int scanInProgress;				/* +0x00038 */
	char scanSsids[NETCTL_MAX_SCAN_SSIDS][33];	/* +0x0003C */
	char padScanSsids[2];				/* +0x00186 */
	SceUInt32 scanSsidCount;			/* +0x00188 */
	NetCtlScanParam scanParam;			/* +0x0018C */
	SceUInt8 scanCustomIe[NETCTL_CUSTOM_IE_MAX];	/* +0x001C4 */

	/* Broadcast scan results */
	SceUInt8 bcastScanBuf[35008];			/* +0x005C4 */
	void *bcastScanList;				/* +0x08E84 */
	SceUInt32 bcastScanCount;			/* +0x08E88 */
	int bcastScanBusy;				/* +0x08E8C */
	SceUInt64 bcastScanSysTime;			/* +0x08E90 */
	SceUInt64 bcastScanRtcTime;			/* +0x08E98 */

	/* Single-SSID scan results */
	SceUInt8 singleScanBuf[4288];			/* +0x08EA0 */
	void *singleScanList;				/* +0x09F60 */
	SceUInt32 singleScanCount;			/* +0x09F64 */
	int singleScanBusy;				/* +0x09F68 */
	int padSingleScan;				/* +0x09F6C */
	SceUInt64 singleScanSysTime;			/* +0x09F70 */
	SceUInt64 singleScanRtcTime;			/* +0x09F78 */

	/* Multi-SSID scan results */
	SceUInt8 multiScanBuf[35008];			/* +0x09F80 */
	void *multiScanList;				/* +0x12840 */
	SceUInt32 multiScanCount;			/* +0x12844 */
	int multiScanBusy;				/* +0x12848 */
	int padMultiScan;				/* +0x1284C */
	SceUInt64 multiScanSysTime;			/* +0x12850 */
	SceUInt64 multiScanRtcTime;			/* +0x12858 */

	/* Auto-connect ranking & scan buffer */
	SceUInt32 rankedConfigIds[NETCTL_MAX_CONFIGS];	/* +0x12860 */
	SceUInt32 rankedCount;				/* +0x128D8 */
	SceUInt32 rankedChannels[NETCTL_MAX_CONFIGS];	/* +0x128DC */
	SceUInt8 autoScanBuf[2240];			/* +0x12954 */
	void *autoScanList;				/* +0x13214 */
	SceUInt32 autoScanCount;			/* +0x13218 */

	char lastConnectedSsid[33];			/* +0x1321C */
	char padLastSsid[3];				/* +0x1323D */

	/* Interface statistics */
	SceNetCtlIfStat ifStatWifi;			/* +0x13240 */
	SceNetCtlIfStat ifStatEth;			/* +0x13280 */
	SceNetCtlIfStat ifStatPhone;			/* +0x132C0 */

	/* Custom IE copies for direct connect */
	SceUInt8 connectCustomIeProbe[NETCTL_CUSTOM_IE_MAX]; /* +0x13300 */
	SceUInt8 connectCustomIeAssoc[NETCTL_CUSTOM_IE_MAX]; /* +0x13700 */

	/* Exclusive function, host callbacks, and mobile/WISPr state */
	int exclusiveRunning;				/* +0x13B00 */
	int exclusiveResetFlag;				/* +0x13B04 */
	int exclusiveImposeShown;			/* +0x13B08 */
	int exclusiveWifiPromptShown;			/* +0x13B0C */
	int exclusiveError;				/* +0x13B10 */
	NetCtlExclusiveResult exclusiveResult;		/* +0x13B14..+0x13B1F */
	SceUID exclusiveRequestPid;			/* +0x13B20 */
	int activeProcessRefCount;			/* +0x13B24 */
	const ShellNetCtlHostFuncs *hostFuncs;		/* +0x13B28 */
	int autoConnectDisabled;			/* +0x13B2C */
	int fakeMobileIf;				/* +0x13B30 */
	int wisprIspId;					/* +0x13B34 */
	char mobileNetName[33];				/* +0x13B38 */
	char padMobileName[3];				/* +0x13B59 */
	SceNetInAddr mobileDns[2];			/* +0x13B5C */
	int reservedMobileTail;				/* +0x13B64 */

	/* Active network configuration */
	NetCtlNetConfig activeConfig;			/* +0x13B68 */

	/* Parsed security keys & resolved link/IP info */
	int resolvedWifiSecurity;			/* +0x14170 */
	SceUInt8 parsedKey[64];				/* +0x14174 */
	SceSize parsedKeyLen;				/* +0x141B4 */
	int parsedKeyIsHex;				/* +0x141B8 */
	SceNetEtherAddr etherAddr;			/* +0x141BC */
	SceUInt8 padEtherAddr[2];			/* +0x141C2 */
	NetCtlWlanBssRecord connectedBss;		/* +0x141C4 */
	SceUInt32 activeMtu;				/* +0x14248 */
	SceNetInAddr ipAddr;				/* +0x1424C */
	SceNetInAddr netmask;				/* +0x14250 */
	SceNetInAddr defaultRoute;			/* +0x14254 */
	SceNetInAddr primaryDnsAddr;			/* +0x14258 */
	SceNetInAddr secondaryDnsAddr;			/* +0x1425C */
	char ipAddrStr[16];				/* +0x14260 */
	int reservedIpTail[2];				/* +0x14270 */
	SceUID adhocOwnerPid;				/* +0x14278 */
	int adhocChannel;				/* +0x1427C */
	int adhocJoinTimeout;				/* +0x14280 */
	char adhocSsid[33];				/* +0x14284 */
	char padAdhocSsid[3];				/* +0x142A5 */
} NetCtlChannel;

/* Shared NetCtl state globals defined in netctl_core.cpp */
extern NetCtlChannel g_netCtlInet;
extern NetCtlChannel g_netCtlAdhoc;
extern SceKernelLwMutexWork g_netCtlMutex;
extern int g_netCtlMutexReady;
extern SceUInt64 g_netCtlAdhocLeaveTime;

/* netctl_core.cpp */
int netCtlCoreInit(SceSize inetStackSize, int inetPriority, SceSize adhocStackSize,
		   int adhocPriority, const ShellNetCtlHostFuncs *host);
int netCtlCoreTerm(void);
int netCtlCoreGetMacAddress(SceNetEtherAddr *outAddr);
int netCtlCoreHasEthernetCap(void);
int netCtlCoreIsDevkit(void);
int netCtlCoreLock(void);
int netCtlCoreUnlock(void);
int netCtlCoreResetSleepTimers(NetCtlChannel *ch);
int netCtlCoreRegisterInetCallback(NetCtlEventCallback cb, void *arg, int *outId);
int netCtlCoreRegisterAdhocCallback(NetCtlEventCallback cb, void *arg, int *outId);
int netCtlCoreUnregisterInetCallback(int cid);
int netCtlCoreUnregisterAdhocCallback(int cid);
int netCtlCoreNotifyCallbacks(NetCtlChannel *ch, int prevState, int newState, int eventCode);
int netCtlCoreNotifyStateChange(NetCtlChannel *ch, int prevState);
int netCtlCoreParseActiveKey(NetCtlChannel *ch);
int netCtlCoreValidateConfig(const NetCtlNetConfig *cfg);
int netCtlCoreReleaseMode(NetCtlChannel *ch, int token);
int netCtlCoreAcquireInetMode(NetCtlChannel *ch, int *outToken);
int netCtlCoreInetConnect(unsigned int mode, int configId, const NetCtlNetConfig *cfg);
int netCtlCoreInetConnectWithPid(unsigned int mode, int configId, const NetCtlNetConfig *cfg,
				 SceUID pid);
int netCtlCoreInetAutoConnect(SceUID requestPid);
int netCtlCoreAdhocConnect(unsigned int mode, SceUID ownerPid, const char *ssid, int timeout);
int netCtlCoreInetDisconnect(void);
int netCtlCoreAdhocDisconnect(void);
int netCtlCoreStartScan(const char **ssids, SceUInt32 count, const NetCtlScanParam *param);
int netCtlCoreGetScanResultCount(void);
int netCtlCoreGetScanResultByIndex(SceUInt32 index, NetCtlScanInfo *outInfo);
int netCtlCoreGetBcastScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount);
int netCtlCoreGetBcastScanInfoByBssid(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
				      void *outIe, SceSize *ioIeSize);
int netCtlCoreGetSingleScanInfo(NetCtlScanInfo *outInfo, void *outIe, SceSize *ioIeSize);
int netCtlCoreGetMultiScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount);
int netCtlCoreGetMultiScanInfoByBssid(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
				      void *outIe, SceSize *ioIeSize);
int netCtlCoreGetWirelessMode(int *outMode);
int netCtlCoreInetGetRawState(int *outState);
int netCtlCoreInetGetInfo(int code, SceNetCtlInfo *outInfo);
int netCtlCoreAdhocGetRawState(int *outState);
int netCtlCoreAdhocGetInfo(int code, void *outInfo);
int netCtlCoreInetGetState(int *outState);
int netCtlCoreAdhocGetState(int *outState);
int netCtlCoreGetMobileStatus(int *outStatus);
unsigned int netCtlCoreAcquireProcessRef(SceUID pid);
int netCtlCoreReleaseProcessRef(unsigned int token);
int netCtlCoreGetAllowedRefCount(void);
int netCtlCoreGetFrontRefCount(void);
int netCtlCoreIncActiveRef(void);
int netCtlCoreDecActiveRef(void);
int netCtlCoreGetActiveRef(void);
int netCtlCoreApplyPowerSave(NetCtlChannel *ch, int ifId);
int netCtlCoreFreeze(SceUID pid);
int netCtlCoreUnfreeze(void);
SceUID netCtlCoreGetFrozenPid(void);
int netCtlCoreSuppressAutoConnect(SceUID pid);
int netCtlCoreResumeAutoConnect(SceUID pid);
int netCtlCoreForceImposePrompt(SceUID pid);
int netCtlCoreClearImposePrompt(SceUID pid);
int netCtlCoreIsAutoConnectSuppressed(SceUID pid);
int netCtlCoreHasForcedImposePrompt(void);
int netCtlCoreAnyAutoConnectSuppressed(void);
int netCtlCoreResetExclusivePrompt(void);
int netCtlCoreGetWisprStatus(NetCtlWisprStatus *outStatus);
int netCtlCoreSetWisprStatus(const NetCtlWisprStatus *status);
int netCtlCoreAbortExclusive(void);
int netCtlCoreGetIfStat(int device, SceNetCtlIfStat *outStat);
int netCtlCoreClearIfStat(int device);
int netCtlCoreLinkProcess(SceUID childPid, SceUID parentPid);
int netCtlCoreUnlinkProcess(SceUID childPid, SceUID parentPid);
int netCtlCoreIsProcessLinked(SceUID childPid, SceUID parentPid);
int netCtlCoreIsAnyProcessAllowed(const SceUID *pids, SceUInt32 count);
int netCtlCoreOnProcessEnded(SceUID pid);
int netCtlCoreStartExclusiveDialog(NetCtlChannel *ch, unsigned int reason, SceUID pid,
				   int forceShow, int *shownFlag);
int netCtlCoreNotifyAutoConnectResult(NetCtlChannel *ch, int errCode);
int netCtlCoreWaitForAdhocMode(void);
int netCtlCoreClaimActiveOwner(NetCtlChannel *ch);
int netCtlCoreWaitEvent(NetCtlChannel *ch, int isScan, SceUInt32 mask, SceUInt32 *outBits,
			SceUInt32 timeoutUsec, int *outTimedOut);
int netCtlCorePollEvent(NetCtlChannel *ch, SceUInt32 mask, SceUInt32 *outBits);
int netCtlCoreFlushEvents(NetCtlChannel *ch);
int netCtlCoreFlushIfUpEvents(NetCtlChannel *ch, int ifId);
int netCtlCoreFlushIfDownEvents(NetCtlChannel *ch, int ifId);
int netCtlCoreAdhocGetInAddr(SceNetInAddr *outAddr);
int netCtlCoreLockFileIo(void);
int netCtlCoreUnlockFileIo(void);
int netCtlCoreAcquireInterface(int isInet, int ifId, int *outNeedUp);
int netCtlCoreReleaseInterface(int isInet, int ifId, int *outNeedDown);
int netCtlCoreConfigureStaticIp(NetCtlChannel *ch);
int netCtlCoreConfigureDhcp(NetCtlChannel *ch);
int netCtlCoreConfigurePppoe(NetCtlChannel *ch);
int netCtlCoreRunWisprAuth(NetCtlChannel *ch);
int netCtlCoreTeardownIp(NetCtlChannel *ch);
int netCtlCoreRefreshIpInfo(NetCtlChannel *ch);
unsigned int netCtlCoreNetEventFilter(int ifId, int flags, SceUInt32 evLow, SceUInt32 evHigh,
				      int unused, void *filterCtx);
int netCtlCoreIsAdhocCooldownExpired(void);

/* netctl_config.cpp */
int netCtlConfigLoadToNetConfig(int isEth, SceUInt32 configId, NetCtlNetConfig *outCfg,
				int *outExists);
int netCtlConfigReloadAll(void);
int netCtlConfigSetWifiEntries(const NetCtlRegConfig *entries, SceUInt32 count);
int netCtlConfigSetEthEntry(const NetCtlRegConfig *entry);
int netCtlConfigGetWifiEntries(NetCtlRegConfig *entries, SceUInt32 count);
int netCtlConfigGetEthEntry(NetCtlRegConfig *entry);
int netCtlConfigGetFlightMode(int *outEnabled);
int netCtlConfigGetWifiEnabled(int *outEnabled);
int netCtlConfigSetWifiEnabled(int enable);
int netCtlConfigGetImposeScanListEnabled(int *outEnabled);
int netCtlConfigGetAdhocChannel(int *outChannel);
int netCtlConfigGetWifiPowerSave(int *outEnabled);
int netCtlConfigGetNetFlag(int *outEnabled);
int netCtlConfigGetIcmType(int *outType);
int netCtlConfigGetFakeMobileIf(int *outFake);
int netCtlConfigGetMobileDns(SceNetInAddr outDns[2]);
int netCtlConfigUpdateCurrentIp(const char *ipStr, SceSize len);
int netCtlConfigLoadIfStat(NetCtlChannel *ch);
int netCtlConfigSaveIfStat(NetCtlChannel *ch);
int netCtlConfigDeleteIfStat(void);
void netCtlExclusiveCallback(int status, int configId, int extra, SceUID clientPid,
			     NetCtlChannel *ch);

/* netctl_icm.cpp */
int netCtlIcmThreadEntry(SceSize args, void *argp);
int netCtlParseAsciiKey(SceUInt8 *outBuf, const char *src, SceUInt32 len);
int netCtlParseHexKey(SceUInt8 *outBuf, const char *src, SceUInt32 len);
int netCtlSetupLoopback(void);
int netCtlTeardownLoopback(void);
int netCtlRunPendingScan(NetCtlChannel *ch);
int netCtlGetScanInfoByIndex(NetCtlChannel *ch, SceUInt32 index, NetCtlScanInfo *outInfo,
			     void *outIe, SceSize *ioIeSize, const void *bssList,
			     int busyFlag, int unused, SceUInt64 sysTime, SceUInt64 rtcTime);
int netCtlGetScanBssidList(NetCtlChannel *ch, SceNetEtherAddr *outList, SceUInt32 maxCount,
			   const void *bssList, SceUInt32 bssCount, int busyFlag);
int netCtlGetScanInfoByBssid(NetCtlChannel *ch, const SceNetEtherAddr *bssid,
			     NetCtlScanInfo *outInfo, void *outIe, SceSize *ioIeSize,
			     const void *bssList, SceUInt32 bssCount, int busyFlag,
			     SceUInt64 sysTime, SceUInt64 rtcTime);
int netCtlGetSingleSsidScanInfo(NetCtlChannel *ch, NetCtlScanInfo *outInfo, void *outIe,
				SceSize *ioIeSize);
int netCtlInetGetInfoImpl(NetCtlChannel *ch, int state, int code, void *outInfo);
int netCtlAdhocGetInfoImpl(NetCtlChannel *ch, int state, int code, void *outInfo);

/* netctl_api.cpp — SceShell exports (extern "C") and internal API */
int SceShell_2E0B6FA6(NetCtlEventCallback callback, void *arg, int *outId);
int SceShell_B4D888D5(int cid);
int SceShell_8C1D932A(const NetCtlNetConfig *cfg);
int SceShell_3F41E440(void);
int SceShell_C1886828(void);
int SceShell_7AD86CE1(int code, SceNetCtlInfo *info);
int SceShell_77642C92(int *outRawState);
int SceShell_449348B6(const char **ssids, SceUInt32 count, const NetCtlScanParam *param);
int SceShell_0025417F(SceNetEtherAddr *outList, SceUInt32 maxCount);
int SceShell_EC75C2F3(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
		      void *outIe, SceSize *ioIeSize);
int SceShell_05D6FCB0(NetCtlScanInfo *outInfo, void *outIe, SceSize *ioIeSize);
int SceShell_3059EEBC(SceNetEtherAddr *outList, SceUInt32 maxCount);
int SceShell_3E737FBE(const SceNetEtherAddr *bssid, NetCtlScanInfo *outInfo,
		      void *outIe, SceSize *ioIeSize);
int SceShell_36788D70(int *outMode);
int SceShell_8858B876(int *outEnabled);
int SceShell_29A14252(int enable);
int SceShell_42053414(NetCtlEventCallback callback, void *arg, int *outId);
int SceShell_FDAA5D2E(int cid);
int SceShell_5DC4A753(int *outRawState);
int SceShell_6B8772A2(SceUID pid);
int SceShell_794BF5BE(unsigned int token);
int SceShell_5928D835(void);
int SceShell_871A8E92(void);
int SceShell_60129092(const NetCtlRegConfig *entries, SceUInt32 count);
int SceShell_A25CD3F2(NetCtlRegConfig *entries, SceUInt32 count);

/* Internal NetCtl API entrypoints called by SceNetCtlIpcInt */
int netCtlApiGetNatInfo(SceNetCtlNatInfo *outInfo);
int netCtlApiGetNatDetailInfo(NetCtlNatDetailInfo *outInfo);
int netCtlApiInetAutoConnect(SceUID pid);
int netCtlApiInetConnectById(int configId);
int netCtlApiInetConnectWithPid(const NetCtlNetConfig *cfg, SceUID pid);
int netCtlApiAdhocCreateGroup(SceUID pid, const char *ssid);
int netCtlApiAdhocJoinGroup(SceUID pid, const char *ssid);
int netCtlApiAdhocCreatePspGroup(SceUID pid, const char *ssid);
int netCtlApiAdhocJoinPspGroup(SceUID pid, const char *ssid, int timeout);
int netCtlApiInetGetState(int *outState);
int netCtlApiGetMobileStatus(int *outStatus);
int netCtlApiGetScanResultCount(void);
int netCtlApiGetScanResultByIndex(SceUInt32 index, NetCtlScanInfo *outInfo);
int netCtlApiAdhocGetInfo(int code, void *outInfo);
int netCtlApiAdhocGetState(int *outState);
int netCtlApiAdhocGetInAddr(SceNetInAddr *outAddr);
int netCtlApiFreeze(SceUID pid);
int netCtlApiUnfreeze(void);
int netCtlApiSuppressAutoConnect(SceUID pid);
int netCtlApiResumeAutoConnect(SceUID pid);
int netCtlApiForceImposePrompt(SceUID pid);
int netCtlApiClearImposePrompt(SceUID pid);
int netCtlApiResetExclusivePrompt(void);
int netCtlApiGetWisprStatus(NetCtlWisprStatus *outStatus);
int netCtlApiAbortExclusive(void);
int netCtlApiGetIfStat(int device, SceNetCtlIfStat *outStat);
int netCtlApiClearIfStat(int device);
int netCtlApiDeleteIfStat(void);
int netCtlApiLinkProcess(SceUID childPid, SceUID parentPid);
int netCtlApiUnlinkProcess(SceUID childPid, SceUID parentPid);
int netCtlApiSetEthRegConfig(const NetCtlRegConfig *entry);
int netCtlApiGetEthRegConfig(NetCtlRegConfig *entry);
int netCtlApiGetMacAddress(SceNetEtherAddr *outAddr);
int netCtlApiIsAdhocCooldownExpired(void);

#ifdef __cplusplus
}
#endif

#endif
