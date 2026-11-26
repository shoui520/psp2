/*
 * net_wlan.cpp: SceNetWlan, SceNetEth, and SceNetPhone driver control ioctl
 * wrappers statically linked into SceShell (0x8137B3E8..0x8137B953 and
 * 0x8137C000..0x8137C0FF).
 */
#include "net_config_internal.h"

/* WLAN driver control ioctl codes (matching sys/internal/driver/src/robin/wlan/wlan_robin.c) */
#define WLAN_CTL_GET_CHANNEL_CONFIG		0x40100001u
#define WLAN_CTL_SET_CHANNEL_CONFIG		0x60100001u
#define WLAN_CTL_SCAN				0x50100001u
#define WLAN_CTL_START_BG_SCAN			0x50100002u
#define WLAN_CTL_STOP_BG_SCAN			0x50100003u
#define WLAN_CTL_READ_BG_SCAN			0x50100004u
#define WLAN_CTL_INFRA_CONNECT			0x50110001u
#define WLAN_CTL_INFRA_DISCONNECT		0x50110002u
#define WLAN_CTL_GET_SIGNAL_INFO		0x40110001u
#define WLAN_CTL_ADHOC_JOIN			0x50120001u
#define WLAN_CTL_ADHOC_LEAVE			0x50120002u
#define WLAN_CTL_ADHOC_CREATE			0x50120003u
#define WLAN_CTL_ADHOC_SCAN_JOIN		0x50120004u
#define WLAN_CTL_SET_MGMT_IE			0x60120002u
#define WLAN_CTL_SET_WAKE_NETWORK		0x60120003u
#define WLAN_CTL_GET_WAKE_PEERS			0x40120003u
#define WLAN_CTL_SET_WAKE_ADHOC			0x60120004u
#define WLAN_CTL_GET_WAKE_SSIDS			0x40120004u
#define WLAN_CTL_SET_TX_RATE			0x60180001u
#define WLAN_CTL_SET_SETTINGS			0x60180002u
#define WLAN_CTL_GET_SETTINGS			0x40180002u
#define WLAN_CTL_SET_WAKE_SCAN			0x60180003u
#define WLAN_CTL_GET_WAKE_STATUS		0x40180003u
#define WLAN_CTL_GET_CARD_INFO			0x40180004u
#define WLAN_CTL_SET_SECURITY_PREF		0x60180005u
#define WLAN_CTL_GET_SECURITY_PREF		0x40180005u
#define WLAN_CTL_GPIO				0x50180001u
#define WLAN_CTL_ENERGY_DETECTION		0x50180002u

/* Ethernet / NetPs / Phone driver control ioctl codes */
#define ETH_CTL_GET_LINK			0x40200001u
#define ETH_CTL_GET_PROMISCUOUS			0x40200002u
#define ETH_CTL_SET_LINK			0x60200001u
#define ETH_CTL_SET_PROMISCUOUS			0x60200002u
#define NET_CTL_NOTIFY_LINK_STATE		0x20000010u
#define PHONE_CTL_GET_LINK_STATE		0x40800003u
#define PHONE_CTL_DISCONNECT			0x50800001u

/* Common header for variable-length WLAN driver ioctl buffers */
typedef struct WlanIoctlHeader {
	SceUInt32 inSize;
	SceUInt32 outSize;
} WlanIoctlHeader;

typedef struct WlanScanResultHeader {
	SceUInt32 inSize;
	SceUInt32 outSize;
	SceUInt32 bssCount;
} WlanScanResultHeader;

typedef struct WlanConnectRequestHeader {
	SceUInt32 inSize;
	SceUInt32 outSize;
	SceUInt32 ieFlags;
	SceUInt32 bodyWords[43];
	SceUInt32 probeIeSize;
	SceUInt32 assocIeSize;
} WlanConnectRequestHeader;

typedef struct WlanGpioControlReq {
	SceUInt32 inSize;
	SceUInt32 outSize;
	SceUInt16 operation;
	SceUInt16 pin;
	SceUInt32 value;
} WlanGpioControlReq;

typedef struct WlanEnergyDetectionReq {
	SceUInt32 inSize;
	SceUInt32 outSize;
	SceUInt16 operation;
	SceInt16 offset;
} WlanEnergyDetectionReq;

int sceNetWlanGetChannelConfig(int ifId, void *outConfig)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outConfig;
	hdr->inSize = 0;
	hdr->outSize = 12;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_CHANNEL_CONFIG, outConfig, 12);
}

int sceNetWlanSetChannelConfig(int ifId, void *config)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)config;
	hdr->inSize = 12;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_CHANNEL_CONFIG, config, 12);
}

int sceNetWlanScan(int ifId, const void *req, SceSize reqSize, void *outBuf, SceSize outBufSize,
		   void **outBssList, SceUInt32 *outBssCount)
{
	if (reqSize > outBufSize)
		return NETCFG_ERR_INVALID_ARG;

	sceClibMemcpy_safe(outBuf, req, reqSize);
	WlanScanResultHeader *hdr = (WlanScanResultHeader *)outBuf;
	hdr->inSize = (SceUInt32)reqSize;
	hdr->outSize = (SceUInt32)outBufSize;

	int ret = SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SCAN, outBuf, outBufSize);
	if (ret >= 0) {
		*outBssList = (void *)(hdr + 1);
		*outBssCount = hdr->bssCount;
	}
	return ret;
}

int sceNetWlanStartBackgroundScan(int ifId, void *reqBuf, SceSize reqSize)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)reqBuf;
	hdr->inSize = (SceUInt32)reqSize;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_START_BG_SCAN, reqBuf, reqSize);
}

int sceNetWlanReadBackgroundScan(int ifId, void *outBuf, SceSize outBufSize,
				 void **outBssList, SceUInt32 *outBssCount)
{
	WlanScanResultHeader *hdr = (WlanScanResultHeader *)outBuf;
	hdr->inSize = 0;
	hdr->outSize = (SceUInt32)outBufSize;

	int ret = SceNetInternal_AA414742(ifId, 0, WLAN_CTL_READ_BG_SCAN, outBuf, outBufSize);
	if (ret >= 0) {
		*outBssList = (void *)(hdr + 1);
		*outBssCount = hdr->bssCount;
	}
	return ret;
}

int sceNetWlanStopBackgroundScan(int ifId)
{
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_STOP_BG_SCAN, NULL, 0);
}

int sceNetWlanConnect(int ifId, const void *req, void *outBuf, SceSize outBufSize, void **outBss)
{
	const WlanConnectRequestHeader *reqHdr = (const WlanConnectRequestHeader *)req;
	SceSize totalSize = sizeof(WlanConnectRequestHeader);

	if ((reqHdr->ieFlags & 1u) != 0) {
		if (reqHdr->probeIeSize > 0x400u)
			return NETCFG_ERR_INVALID_ARG;
		totalSize += reqHdr->probeIeSize;
	}
	if ((reqHdr->ieFlags & 2u) != 0) {
		if (reqHdr->assocIeSize > 0x400u)
			return NETCFG_ERR_INVALID_ARG;
		totalSize += reqHdr->assocIeSize;
	}
	if (totalSize > outBufSize)
		return NETCFG_ERR_INVALID_ARG;

	sceClibMemcpy_safe(outBuf, req, totalSize);
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outBuf;
	hdr->inSize = (SceUInt32)totalSize;
	hdr->outSize = (SceUInt32)outBufSize;

	int ret = SceNetInternal_AA414742(ifId, 0, WLAN_CTL_INFRA_CONNECT, outBuf, outBufSize);
	if (ret >= 0)
		*outBss = (void *)(hdr + 1);
	return ret;
}

int sceNetWlanDisconnect(int ifId)
{
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_INFRA_DISCONNECT, NULL, 0);
}

int sceNetWlanGetRssi(int ifId, void *outRssiBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outRssiBuf;
	hdr->inSize = 0;
	hdr->outSize = 16;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_SIGNAL_INFO, outRssiBuf, 16);
}

int sceNetWlanAdhocCreate(int ifId, void *req)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)req;
	hdr->inSize = 0x114;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ADHOC_CREATE, req, 0x114);
}

int sceNetWlanAdhocJoin(int ifId, void *req)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)req;
	hdr->inSize = 0x114;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ADHOC_JOIN, req, 0x114);
}

int sceNetWlanAdhocLeave(int ifId)
{
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ADHOC_LEAVE, NULL, 0);
}

int sceNetWlanAdhocScanAndJoin(int ifId, const void *req, void *outBuf, SceSize outBufSize,
			       void **outBss)
{
	if (outBufSize < 0xB8u)
		return NETCFG_ERR_INVALID_ARG;

	sceClibMemcpy_safe(outBuf, req, 0xB8u);
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outBuf;
	hdr->inSize = 0xB8;
	hdr->outSize = (SceUInt32)outBufSize;

	int ret = SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ADHOC_SCAN_JOIN, outBuf, outBufSize);
	if (ret >= 0)
		*outBss = (void *)(hdr + 1);
	return ret;
}

int sceNetWlanSetManagementIe(int ifId, void *ieBuf, SceSize ieBufSize)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)ieBuf;
	hdr->inSize = (SceUInt32)ieBufSize;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_MGMT_IE, ieBuf, ieBufSize);
}

int sceNetWlanSetWakeNetwork(int ifId, void *reqBuf, SceSize reqSize)
{
	if (reqBuf == NULL || reqSize == 0)
		return NETCFG_ERR_INVALID_ARG;

	WlanIoctlHeader *hdr = (WlanIoctlHeader *)reqBuf;
	hdr->inSize = (SceUInt32)reqSize;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_WAKE_NETWORK, reqBuf, reqSize);
}

int sceNetWlanClearWakeNetwork(int ifId)
{
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_WAKE_NETWORK, NULL, 0);
}

int sceNetWlanGetWakePeers(int ifId, void *outPeerList)
{
	if (outPeerList == NULL)
		return NETCFG_ERR_INVALID_ARG;

	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outPeerList;
	hdr->inSize = 0;
	hdr->outSize = 0x108C;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_WAKE_PEERS, outPeerList, 0x108C);
}

int sceNetWlanSetWakeAdhoc(int ifId, void *config)
{
	if (config == NULL)
		return NETCFG_ERR_INVALID_ARG;

	WlanIoctlHeader *hdr = (WlanIoctlHeader *)config;
	hdr->inSize = 0x5C;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_WAKE_ADHOC, config, 0x5C);
}

int sceNetWlanClearWakeAdhoc(int ifId)
{
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_WAKE_ADHOC, NULL, 0);
}

int sceNetWlanGetWakeDiscoveredSsids(int ifId, void *outSsidList)
{
	if (outSsidList == NULL)
		return NETCFG_ERR_INVALID_ARG;

	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outSsidList;
	hdr->inSize = 0;
	hdr->outSize = 0x174;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_WAKE_SSIDS, outSsidList, 0x174);
}

int sceNetWlanSetTransmitRate(int ifId, void *rateBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)rateBuf;
	hdr->inSize = 12;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_TX_RATE, rateBuf, 12);
}

int sceNetWlanWriteSettings(int ifId, void *settingsBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)settingsBuf;
	hdr->inSize = 12;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_SETTINGS, settingsBuf, 12);
}

int sceNetWlanReadSettings(int ifId, void *outSettingsBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outSettingsBuf;
	hdr->inSize = 0;
	hdr->outSize = 12;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_SETTINGS, outSettingsBuf, 12);
}

int sceNetWlanSetWakeScan(int ifId, void *wakeScanBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)wakeScanBuf;
	hdr->inSize = 0x18C;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_WAKE_SCAN, wakeScanBuf, 0x18C);
}

int sceNetWlanGetWakeStatus(int ifId, void *outStatusBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outStatusBuf;
	hdr->inSize = 0;
	hdr->outSize = 12;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_WAKE_STATUS, outStatusBuf, 12);
}

int sceNetWlanGetCardInfo(int ifId, void *outCardInfo)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outCardInfo;
	hdr->inSize = 0;
	hdr->outSize = 24;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_CARD_INFO, outCardInfo, 24);
}

int sceNetWlanSetSecurityPreference(int ifId, void *prefBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)prefBuf;
	hdr->inSize = 12;
	hdr->outSize = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_SET_SECURITY_PREF, prefBuf, 12);
}

int sceNetWlanGetSecurityPreference(int ifId, void *outPrefBuf)
{
	WlanIoctlHeader *hdr = (WlanIoctlHeader *)outPrefBuf;
	hdr->inSize = 0;
	hdr->outSize = 12;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GET_SECURITY_PREF, outPrefBuf, 12);
}

int sceNetWlanSetGpioOutput(int ifId, SceUInt16 pin, SceUInt32 value)
{
	WlanGpioControlReq req;
	sceClibMemset(&req, 0, sizeof(req));
	req.inSize = sizeof(req);
	req.outSize = sizeof(req);
	req.operation = 1;
	req.pin = pin;
	req.value = value;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GPIO, &req, sizeof(req));
}

int sceNetWlanGetGpioInput(int ifId, SceUInt16 pin, SceUInt32 *outValue)
{
	WlanGpioControlReq req;
	sceClibMemset(&req, 0, sizeof(req));
	req.inSize = sizeof(req);
	req.outSize = sizeof(req);
	req.operation = 0;
	req.pin = pin;

	int ret = SceNetInternal_AA414742(ifId, 0, WLAN_CTL_GPIO, &req, sizeof(req));
	if (ret >= 0)
		*outValue = req.value;
	return ret;
}

int sceNetWlanSetEnergyDetectionOffset(int ifId, SceInt16 offset)
{
	WlanEnergyDetectionReq req;
	sceClibMemset(&req, 0, sizeof(req));
	req.inSize = sizeof(req);
	req.outSize = sizeof(req);
	req.operation = 1;
	req.offset = offset;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ENERGY_DETECTION, &req, sizeof(req));
}

int sceNetWlanDisableEnergyDetection(int ifId)
{
	WlanEnergyDetectionReq req;
	sceClibMemset(&req, 0, sizeof(req));
	req.inSize = sizeof(req);
	req.outSize = sizeof(req);
	req.operation = 0;
	return SceNetInternal_AA414742(ifId, 0, WLAN_CTL_ENERGY_DETECTION, &req, sizeof(req));
}

int sceNetEthGetLinkStatus(int ifId, int *outStatus)
{
	int status = 0;
	int ret = SceNetInternal_AA414742(ifId, 0, ETH_CTL_GET_LINK, &status, sizeof(status));
	if (ret >= 0 && outStatus != NULL)
		*outStatus = status;
	return ret;
}

int sceNetEthSetLinkMode(int ifId, int linkMode)
{
	return SceNetInternal_AA414742(ifId, 0, ETH_CTL_SET_LINK, &linkMode, sizeof(linkMode));
}

int sceNetEthGetPromiscuous(int ifId, int *outEnabled)
{
	int enabled = 0;
	int ret = SceNetInternal_AA414742(ifId, 0, ETH_CTL_GET_PROMISCUOUS,
					  &enabled, sizeof(enabled));
	if (ret >= 0 && outEnabled != NULL)
		*outEnabled = enabled;
	return ret;
}

int sceNetEthSetPromiscuous(int ifId, int enabled)
{
	return SceNetInternal_AA414742(ifId, 0, ETH_CTL_SET_PROMISCUOUS,
				       &enabled, sizeof(enabled));
}

int sceNetNotifyLinkState(int ifId)
{
	int zero = 0;
	return SceNetInternal_AA414742(ifId, 0, NET_CTL_NOTIFY_LINK_STATE, &zero, sizeof(zero));
}

int sceNetPhoneGetStatus(int ifId, int *outStatus)
{
	int status = 0;
	int ret = SceNetInternal_AA414742(ifId, 0, PHONE_CTL_GET_LINK_STATE,
					  &status, sizeof(status));
	if (outStatus != NULL)
		*outStatus = status;
	return ret;
}

int sceNetPhoneNotifyEvent(int ifId, int eventType)
{
	int zero = 0;
	if (eventType != 0)
		return NETCFG_ERR_INVALID_ARG;
	return SceNetInternal_AA414742(ifId, 0, PHONE_CTL_DISCONNECT, &zero, sizeof(zero));
}
