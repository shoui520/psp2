/*
 * The network side's helpers: the IDs, addresses, names and host types as the requests
 * write them, the console's MAC address and user name, and which network it is on.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <libnetctl.h>
#include <regmgr/regmgr.h>
#include <appmgr/appmgr.h>
#include <wlanbt/wlanbt.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "mtp_internal.h"

/* the registry's account and its sign-in name */
#define REGMGR_NP_ACCOUNT_ID		0x450F32
#define REGMGR_SYSTEM_USERNAME		0x598438
#define MTP_USER_NAME_MAX		255

/* the network interface list's entry (SceNetInternal_235DE96C) */
#define NET_IF_TYPE_WLAN		4
#define NET_IF_TYPE_ETHER		1

typedef struct NetMtpIfEntry {
	char name[80];
	SceUInt8 macAddress[6];
	SceUInt8 reserved[234];
} NetMtpIfEntry;

/* the system data file's user part */
typedef struct NetMtpSystemData {
	SceUInt8 reserved[36];
	char userName[16];
	SceUInt8 reserved2[276];
} NetMtpSystemData;

/* vitasdk: none; Sony's message names it sceNetGetIfList */
int SceNetInternal_235DE96C(int type, void *list, int count);

int netMtpIdToString(char *dst, SceSize size, const SceUInt8 *id)
{
	int len;
	int i;

	if (dst == NULL || id == NULL || size < 33)
		return MTP_ERROR_INVALID;
	len = 0;
	for (i = 16; i != 0; i--)
		len += sce_paf_snprintf(dst + len, 3, "%02x", *id++);
	return 0;
}

int netMtpStringToMacAddress(SceUInt8 *mac, const char *str)
{
	int len = sce_paf_strlen(str);
	int pos;
	int n;
	char hex[3];

	if (mac == NULL || str == NULL)
		return MTP_ERROR_INVALID;
	hex[2] = '\0';
	if (len >= 0) {
		n = 0;
		pos = 0;
		do {
			if (*str == '-') {
				str++;
				pos++;
			} else {
				hex[0] = str[0];
				hex[1] = str[1];
				str += 2;
				pos += 2;
				*mac++ = (SceUInt8)sce_paf_strtol(hex, NULL, 16);
				if (++n == 6)
					break;
			}
		} while (pos <= len);
	}
	return 0;
}

int netMtpMacAddressToString(char *dst, SceSize size, const SceUInt8 *mac)
{
	if (dst == NULL || mac == NULL)
		return MTP_ERROR_INVALID;
	sce_paf_snprintf(dst, size, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],
			 mac[4], mac[5]);
	return 0;
}

int netMtpNameToString(char *dst, SceSize size, const char *name)
{
	if (dst == NULL || size < 255 || name == NULL)
		return MTP_ERROR_INVALID;
	mtpUtilMemset(dst, 0, size);
	mtpUtilMemcpy(dst, name, 255);
	return 0;
}

int netMtpPinToString(char *dst, SceSize size, const char *pin)
{
	if (dst == NULL || pin == NULL)
		return MTP_ERROR_INVALID;
	sce_paf_snprintf(dst, size, "%c%c%c%c%c%c%c%c", pin[0], pin[1], pin[2], pin[3], pin[4],
			 pin[5], pin[6], pin[7]);
	return 0;
}

int netMtpStringToUuid(SceUInt8 *id, const char *str)
{
	int pos = 0;
	int n = 0;
	int len = sce_paf_strlen(str);
	char hex[3];

	hex[2] = '\0';
	if (len >= 0) {
		do {
			if (*str == '-') {
				str++;
				pos++;
			} else {
				hex[0] = str[0];
				hex[1] = str[1];
				str += 2;
				pos += 2;
				*id++ = (SceUInt8)sce_paf_strtol(hex, NULL, 16);
				if (++n == 16)
					break;
			}
		} while (pos <= len);
	}
	return 0;
}

int netMtpUuidToString(char *dst, SceSize size, const SceUInt8 *id)
{
	if (dst == NULL || id == NULL)
		return -1;
	sce_paf_snprintf(dst, size,
			 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9],
			 id[10], id[11], id[12], id[13], id[14], id[15]);
	return 0;
}

SceUInt32 netMtpGetTick(void)
{
	SceRtcTick tick;

	if (sceRtcGetCurrentTick(&tick) != 0)
		return 0;
	return (SceUInt32)tick.tick;
}

int getMacAddr(SceUInt8 *mac)
{
	int ret = 0;
	int res;
	NetMtpIfEntry entry;

	res = SceNetInternal_235DE96C(NET_IF_TYPE_WLAN, &entry, 1);
	if (res <= 0 && SceNetInternal_235DE96C(NET_IF_TYPE_ETHER, &entry, 1) <= 0) {
		MTP_ERR("", 246, "sceNetGetIfList() 0x%x\n", res);
		ret = res;
	} else {
		mtpUtilMemcpy(mac, entry.macAddress, sizeof(entry.macAddress));
	}
	return ret;
}

void netMtpMakeDeviceId(SceUInt8 *id)
{
	sce_paf_memset(id, 1, 16);
	getMacAddr(id);
}

int getSystemParamUserName(char *name, int withModel)
{
	SceUInt64 accountId;
	NetMtpSystemData data;
	SceSize len;
	int res;

	res = sceRegMgrUtilityGetBin(REGMGR_NP_ACCOUNT_ID, &accountId, sizeof(accountId));
	if (res != 0) {
		MTP_ERR("", 279, "sceRegMgrUtilityGetBin() failed 0x%08x\n", res);
	} else if (accountId != 0) {
		/* signed in: the account's name */
		sceClibMemset(&data, 0, sizeof(data));
		res = sceAppMgrGetSystemDataFile(0, &data, sizeof(data));
		if (res == sizeof(data)) {
			sceClibMemcpy_safe(name, data.userName, sizeof(data.userName));
			name[sceClibStrnlen(data.userName, sizeof(data.userName))] = '\0';
			goto model;
		}
		MTP_ERR("", 293, "sceAppMgrGetSystemDataFile() failed 0x%08x\n", res);
	}
	res = sceRegMgrUtilityGetStr(REGMGR_SYSTEM_USERNAME, name, MTP_USER_NAME_MAX);
	if (res != 0) {
		MTP_ERR("", 304, "sceRegMgrUtilityGetStr() failed 0x%08x\n", res);
		sceClibStrncpy(name, "User999", MTP_USER_NAME_MAX);
	}
model:
	if (withModel != 0) {
		len = sce_paf_strlen(name);
		if (len + 11 < MTP_USER_NAME_MAX)
			sce_paf_strncpy(name + len, " (PS Vita)", MTP_USER_NAME_MAX - len);
	}
	return 0;
}

int netMtpStringToHostType(int *type, const char *str)
{
	if (type == NULL || str == NULL)
		return MTP_ERROR_INVALID;
	if (sce_paf_strcmp("win", str) == 0)
		*type = MTP_NET_HOST_TYPE_WIN;
	else if (sce_paf_strcmp("mac", str) == 0)
		*type = MTP_NET_HOST_TYPE_MAC;
	else if (sce_paf_strcmp("nasne", str) == 0)
		*type = MTP_NET_HOST_TYPE_NASNE;
	else if (sce_paf_strcmp("ps3", str) == 0)
		*type = MTP_NET_HOST_TYPE_PS3;
	else
		*type = 0;
	return 0;
}

int netMtpHostTypeToString(char *dst, SceSize size, int type)
{
	if (dst == NULL)
		return MTP_ERROR_INVALID;
	sce_paf_memset(dst, 0, size);
	switch (type) {
	case MTP_NET_HOST_TYPE_WIN:
		sce_paf_strncpy(dst, "win", size);
		break;
	case MTP_NET_HOST_TYPE_MAC:
		sce_paf_strncpy(dst, "mac", size);
		break;
	case MTP_NET_HOST_TYPE_NASNE:
		sce_paf_strncpy(dst, "nasne", size);
		break;
	case MTP_NET_HOST_TYPE_PS3:
		sce_paf_strncpy(dst, "ps3", size);
		break;
	default:
		sce_paf_strncpy(dst, "unknown", size);
		break;
	}
	return 0;
}

/* Wi-Fi is on, and the console is not on a phone network */
int netMtpIsWlanAvailable(void)
{
	SceNetCtlInfo info;

	return (sceWlanGetConfiguration() & 5) != 0 &&
	       (sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info) != 0 ||
		info.device < SCE_NET_CTL_DEVICE_PHONE);
}

int netMtpIsPhoneConnection(void)
{
	SceNetCtlInfo info;

	return sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info) == 0 &&
	       info.device >= SCE_NET_CTL_DEVICE_PHONE;
}
