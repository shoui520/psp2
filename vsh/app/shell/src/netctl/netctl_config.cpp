/*
 * netctl_config.cpp: SceNetCtl registry configuration cache (/CONFIG/NET/%02d/*,
 * /CONFIG/NET/ETH/*), system/net keys, vd0:network/ifstat.bin persistence, and
 * exclusive-function completion callback.
 * 3.65-CEX: 0x81213E0C - 0x81214BAA
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <regmgr/regmgr.h>
#include <wlanbt/wlanbt.h>
#include <net.h>
#include "netctl_internal.h"

struct NetCtlRegKeySpec {
	const char *name;
	SceUInt16 type;
	SceUInt16 pad;
};

static const NetCtlRegKeySpec s_commonKeySpecs[] = {		/* 0x81513890 */
	{ "conf_flag",           0, 0 },
	{ "conf_type",           0, 0 },
	{ "conf_name",           1, 0 },
	{ "conf_serial_no",      0, 0 },
	{ "enable_auto_connect", 0, 0 },
	{ "mtu",                 0, 0 },
	{ "device",              0, 0 },
	{ NULL,                  0, 0 },
};

static const NetCtlRegKeySpec s_wifiKeySpecs[] = {		/* 0x815138FC */
	{ "ssid",          1, 0 },
	{ "wifi_security", 0, 0 },
	{ "wep_key",       1, 0 },
	{ "wpa_key",       1, 0 },
	{ NULL,            0, 0 },
};

static const NetCtlRegKeySpec s_ipKeySpecs[] = {		/* 0x81513994 */
	{ "ip_config",     0, 0 },
	{ "dhcp_hostname", 1, 0 },
	{ "auth_name",     1, 0 },
	{ "auth_key",      1, 0 },
	{ "ip_address",    1, 0 },
	{ "netmask",       1, 0 },
	{ "default_route", 1, 0 },
	{ "dns_flag",      0, 0 },
	{ "primary_dns",   1, 0 },
	{ "secondary_dns", 1, 0 },
	{ NULL,            0, 0 },
};

static const NetCtlRegKeySpec s_appKeySpecs[] = {		/* 0x81513A14 */
	{ "http_proxy_flag",   0, 0 },
	{ "http_proxy_server", 1, 0 },
	{ "http_proxy_port",   0, 0 },
	{ NULL,                0, 0 },
};

static int s_ifStatDeleted = 0;					/* 0x815450C4 */
static NetCtlRegConfig s_ethRegConfig;				/* 0x81598230 */
static char s_cachedCurrentIp[16];				/* 0x815986A8 */
static NetCtlRegConfig s_wifiRegConfigs[NETCTL_MAX_CONFIGS + 1]; /* 0x815986B8 (indices 0..30) */

static int netCtlPrepareRegKeys(const NetCtlRegKeySpec *specs, RegMgrKeyInfo *outKeys,
				unsigned int maxKeys)
{
	unsigned int count = 0;

	while (count < maxKeys && specs[count].name != NULL) {
		sceClibMemset(&outKeys[count], 0, sizeof(RegMgrKeyInfo));
		outKeys[count].size = sizeof(RegMgrKeyInfo);
		outKeys[count].name = specs[count].name;
		outKeys[count].type = specs[count].type;
		if (specs[count].type == 0)
			outKeys[count].valueSize = sizeof(int);
		++count;
	}
	return (int)count;
}

/* 0x81213E0C */
int netCtlConfigLoadToNetConfig(int isEth, SceUInt32 configId, NetCtlNetConfig *outCfg,
				int *outExists)
{
	int locked = 0;
	int ret = netCtlCoreLock();

	if (ret >= 0) {
		locked = 1;
		if (isEth == 0 && configId > NETCTL_MAX_CONFIGS) {
			*outExists = 0;
			netCtlCoreUnlock();
			return 0;
		}

		const NetCtlRegConfig *src = (isEth == 1) ? &s_ethRegConfig : &s_wifiRegConfigs[configId];

		sceClibMemset(outCfg, 0, sizeof(NetCtlNetConfig));
		if (isEth != 0 || src->confFlag != 0) {
			*outExists = 1;
			outCfg->confType = (SceUInt32)src->confType;
			sceClibStrncpy(outCfg->cnfName, src->confName, sizeof(outCfg->cnfName));
			outCfg->confSerialNo = src->confSerialNo;
			outCfg->enableAutoConnect = (src->enableAutoConnect != 0) ? 1u : 0u;
			outCfg->mtu = src->mtu;
			outCfg->device = src->device;
			sceClibStrncpy(outCfg->ssid, src->ssid, sizeof(outCfg->ssid));
			outCfg->wifiSecurity = src->wifiSecurity;

			if (src->wifiSecurity == SCE_NET_CTL_WIFI_SECURITY_WEP) {
				sceClibStrncpy(outCfg->securityKey, src->wepKey, sizeof(outCfg->securityKey));
				outCfg->securityKey[sizeof(outCfg->securityKey) - 1] = '\0';
			} else if (src->wifiSecurity == SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK) {
				sceClibStrncpy(outCfg->securityKey, src->wpaKey, sizeof(outCfg->securityKey));
				outCfg->securityKey[sizeof(outCfg->securityKey) - 1] = '\0';
			} else {
				outCfg->securityKey[0] = '\0';
			}

			outCfg->ipConfig = src->ipConfig;
			sceClibStrncpy(outCfg->dhcpHostname, src->dhcpHostname, sizeof(outCfg->dhcpHostname));
			sceClibStrncpy(outCfg->pppoeAuthName, src->authName, sizeof(outCfg->pppoeAuthName));
			sceClibStrncpy(outCfg->pppoeAuthKey, src->authKey, sizeof(outCfg->pppoeAuthKey));
			sceClibStrncpy(outCfg->ipAddress, src->ipAddress, sizeof(outCfg->ipAddress));
			sceClibStrncpy(outCfg->netmask, src->netmask, sizeof(outCfg->netmask));
			sceClibStrncpy(outCfg->defaultRoute, src->defaultRoute, sizeof(outCfg->defaultRoute));
			outCfg->dnsFlag = src->dnsFlag;
			sceClibStrncpy(outCfg->primary_dns, src->primary_dns, sizeof(outCfg->primary_dns));
			sceClibStrncpy(outCfg->secondary_dns, src->secondary_dns, sizeof(outCfg->secondary_dns));
			outCfg->httpProxyFlag = src->httpProxyFlag;
			sceClibStrncpy(outCfg->httpProxyServer, src->httpProxyServer,
				       sizeof(outCfg->httpProxyServer));
			outCfg->httpProxyPort = src->httpProxyPort;

			ret = netCtlCoreValidateConfig(outCfg);
			if (ret >= 0) {
				outCfg->configId = configId;
				netCtlCoreUnlock();
				return 0;
			}
		} else {
			*outExists = 0;
			netCtlCoreUnlock();
			return 0;
		}
	}

	*outExists = 0;
	if (locked == 1)
		netCtlCoreUnlock();
	return 0;
}

/* 0x81213FC4 */
static int netCtlReadRegIpSection(const char *basePath, RegMgrKeyInfo *keys,
				  unsigned int maxKeys, NetCtlRegConfig *outEntry)
{
	char sectionPath[128];
	int count = netCtlPrepareRegKeys(s_ipKeySpecs, keys, maxKeys);

	if (count < 0)
		return count;

	keys[1].valueSize = sizeof(outEntry->dhcpHostname);
	keys[1].value = (int)(SceIntPtr)outEntry->dhcpHostname;
	keys[2].valueSize = sizeof(outEntry->authName);
	keys[2].value = (int)(SceIntPtr)outEntry->authName;
	keys[3].valueSize = sizeof(outEntry->authKey);
	keys[3].value = (int)(SceIntPtr)outEntry->authKey;
	keys[4].valueSize = sizeof(outEntry->ipAddress);
	keys[4].value = (int)(SceIntPtr)outEntry->ipAddress;
	keys[5].valueSize = sizeof(outEntry->netmask);
	keys[5].value = (int)(SceIntPtr)outEntry->netmask;
	keys[6].valueSize = sizeof(outEntry->defaultRoute);
	keys[6].value = (int)(SceIntPtr)outEntry->defaultRoute;
	keys[8].valueSize = sizeof(outEntry->primary_dns);
	keys[8].value = (int)(SceIntPtr)outEntry->primary_dns;
	keys[9].valueSize = sizeof(outEntry->secondary_dns);
	keys[9].value = (int)(SceIntPtr)outEntry->secondary_dns;

	if ((unsigned int)sceClibSnprintf(sectionPath, sizeof(sectionPath), "%s/%s",
					  basePath, "IP") >= sizeof(sectionPath))
		return NETCTL_ERR_PATH_TOO_LONG;

	int ret = sceRegMgrGetKeys(sectionPath, keys, count);

	if (ret >= 0) {
		outEntry->ipConfig = (SceUInt32)keys[0].value;
		outEntry->dnsFlag = (SceUInt32)keys[7].value;
	}
	return ret;
}

/* 0x812140F6 */
static int netCtlReadRegAppSection(const char *basePath, RegMgrKeyInfo *keys,
				   unsigned int maxKeys, NetCtlRegConfig *outEntry)
{
	char sectionPath[128];
	int count = netCtlPrepareRegKeys(s_appKeySpecs, keys, maxKeys);

	if (count < 0)
		return count;

	keys[1].valueSize = sizeof(outEntry->httpProxyServer);
	keys[1].value = (int)(SceIntPtr)outEntry->httpProxyServer;

	if ((unsigned int)sceClibSnprintf(sectionPath, sizeof(sectionPath), "%s/%s",
					  basePath, "APP") >= sizeof(sectionPath))
		return NETCTL_ERR_PATH_TOO_LONG;

	int ret = sceRegMgrGetKeys(sectionPath, keys, count);

	if (ret >= 0) {
		outEntry->httpProxyFlag = (SceUInt32)keys[0].value;
		outEntry->httpProxyPort = (SceUInt32)keys[2].value;
	}
	return ret;
}

/* 0x812141DE */
static int netCtlLoadRegConfigSlot(SceUInt32 configId, NetCtlRegConfig *outEntry)
{
	char basePath[32];
	char commonPath[128];
	char wifiPath[128];
	RegMgrKeyInfo keys[16];
	int locked = 0;
	int ok = 0;

	if (netCtlCoreLock() >= 0) {
		locked = 1;
		if (configId <= NETCTL_MAX_CONFIGS &&
		    (unsigned int)sceClibSnprintf(basePath, sizeof(basePath), "%s/%02d",
						  "/CONFIG/NET", configId) < sizeof(basePath)) {
			sceClibMemset(outEntry, 0, sizeof(NetCtlRegConfig));
			if ((unsigned int)sceClibSnprintf(commonPath, sizeof(commonPath), "%s/%s",
							  basePath, "COMMON") < sizeof(commonPath)) {
				int ret = sceRegMgrGetKeyInt(commonPath, "conf_flag", &outEntry->confFlag);

				if (ret >= 0 && outEntry->confFlag == 0) {
					ok = 1;
				} else if (ret >= 0) {
					int count = netCtlPrepareRegKeys(s_commonKeySpecs, keys, 16);

					if (count >= 0) {
						keys[2].valueSize = sizeof(outEntry->confName);
						keys[2].value = (int)(SceIntPtr)outEntry->confName;
						ret = sceRegMgrGetKeys(commonPath, keys, count);
						if (ret >= 0) {
							outEntry->confFlag = keys[0].value;
							outEntry->confType = keys[1].value;
							outEntry->confSerialNo = (SceUInt32)keys[3].value;
							outEntry->enableAutoConnect = keys[4].value;
							outEntry->mtu = (SceUInt32)keys[5].value;
							outEntry->device = (SceUInt32)keys[6].value;
						}
					}

					if (ret >= 0 && outEntry->confFlag == 0) {
						ok = 1;
					} else if (ret >= 0) {
						int wifiCount = netCtlPrepareRegKeys(s_wifiKeySpecs, keys, 16);

						if (wifiCount >= 0 &&
						    (unsigned int)sceClibSnprintf(wifiPath, sizeof(wifiPath),
										  "%s/%s", basePath, "WIFI") < sizeof(wifiPath)) {
							keys[0].valueSize = sizeof(outEntry->ssid);
							keys[0].value = (int)(SceIntPtr)outEntry->ssid;
							keys[2].valueSize = sizeof(outEntry->wepKey);
							keys[2].value = (int)(SceIntPtr)outEntry->wepKey;
							keys[3].valueSize = sizeof(outEntry->wpaKey);
							keys[3].value = (int)(SceIntPtr)outEntry->wpaKey;
							ret = sceRegMgrGetKeys(wifiPath, keys, wifiCount);
							if (ret >= 0) {
								outEntry->wifiSecurity = (SceUInt32)keys[1].value;
								ret = netCtlReadRegIpSection(basePath, keys, 16, outEntry);
								if (ret >= 0) {
									ret = netCtlReadRegAppSection(basePath, keys, 16, outEntry);
									if (ret >= 0)
										ok = 1;
								}
							}
						}
					}
				}
			}
		}
	}

	if (!ok)
		outEntry->confFlag = 0;
	if (locked == 1)
		netCtlCoreUnlock();
	return 0;
}

/* 0x81214436 */
int netCtlConfigReloadAll(void)
{
	RegMgrKeyInfo keys[16];
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	for (SceUInt32 i = 1; i <= NETCTL_MAX_CONFIGS; ++i) {
		ret = netCtlLoadRegConfigSlot(i, &s_wifiRegConfigs[i]);
		if (ret < 0)
			break;
	}

	if (ret >= 0 && netCtlCoreHasEthernetCap() == 1) {
		ret = netCtlCoreLock();
		if (ret >= 0) {
			sceClibMemset(&s_ethRegConfig, 0, sizeof(s_ethRegConfig));
			ret = sceRegMgrGetKeyInt("/CONFIG/NET/ETH/COMMON", "mtu",
						 (int *)&s_ethRegConfig.mtu);
			if (ret >= 0) {
				s_ethRegConfig.device = SCE_NET_CTL_DEVICE_WIRED;
				sceClibStrlcpy(s_ethRegConfig.confName, "Ethernet",
					       sizeof(s_ethRegConfig.confName));
				ret = netCtlReadRegIpSection("/CONFIG/NET/ETH", keys, 16,
							     &s_ethRegConfig);
				if (ret >= 0) {
					ret = netCtlReadRegAppSection("/CONFIG/NET/ETH", keys, 16,
								      &s_ethRegConfig);
					if (ret >= 0)
						ret = 0;
				}
			}
			netCtlCoreUnlock();
		}
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x81214534 */
int netCtlConfigSetWifiEntries(const NetCtlRegConfig *entries, SceUInt32 count)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	for (SceUInt32 i = 0; i < count; ++i) {
		SceUInt32 id = entries[i].configId;

		ret = netCtlCoreLock();
		if (ret >= 0) {
			if (id <= NETCTL_MAX_CONFIGS)
				sceClibMemcpy_safe(&s_wifiRegConfigs[id], &entries[i],
						   sizeof(NetCtlRegConfig));
			else
				ret = NETCTL_ERR_INVALID_CONFIG_ID;
			netCtlCoreUnlock();
		}
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x8121459A */
int netCtlConfigSetEthEntry(const NetCtlRegConfig *entry)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	ret = netCtlCoreLock();
	if (ret >= 0) {
		sceClibMemcpy_safe(&s_ethRegConfig, entry, sizeof(NetCtlRegConfig));
		netCtlCoreUnlock();
	}
	netCtlCoreUnlock();
	return ret;
}

/* 0x812145D0 */
int netCtlConfigGetWifiEntries(NetCtlRegConfig *entries, SceUInt32 count)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	for (SceUInt32 i = 0; i < count; ++i) {
		SceUInt32 id = entries[i].configId;

		ret = netCtlCoreLock();
		if (ret >= 0) {
			if (id <= NETCTL_MAX_CONFIGS) {
				sceClibMemcpy_safe(&entries[i], &s_wifiRegConfigs[id],
						   sizeof(NetCtlRegConfig));
				entries[i].configId = id;
			} else {
				ret = NETCTL_ERR_INVALID_CONFIG_ID;
			}
			netCtlCoreUnlock();
		}
	}

	netCtlCoreUnlock();
	return ret;
}

/* 0x8121463A */
int netCtlConfigGetEthEntry(NetCtlRegConfig *entry)
{
	int ret = netCtlCoreLock();

	if (ret < 0)
		return ret;

	sceClibMemcpy_safe(entry, &s_ethRegConfig, sizeof(NetCtlRegConfig));
	netCtlCoreUnlock();
	return ret;
}

/* 0x81214662 */
int netCtlConfigGetFlightMode(int *outEnabled)
{
	int value = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "flight_mode", &value);

	if (ret >= 0)
		*outEnabled = (value != 0) ? 1 : 0;
	return ret;
}

/* 0x812146AA */
int netCtlConfigGetWifiEnabled(int *outEnabled)
{
	*outEnabled = ((sceWlanGetConfiguration() & 1u) != 0) ? 1 : 0;
	return 0;
}

/* 0x812146C4 */
int netCtlConfigSetWifiEnabled(int enable)
{
	int flag = (enable != 0) ? 1 : 0;
	int ret = sceRegMgrSetKeyInt("/CONFIG/NET", "wifi_flag", flag);

	if (ret >= 0)
		sceWlanSetConfiguration(flag);
	return ret;
}

/* 0x812146F8 */
int netCtlConfigGetImposeScanListEnabled(int *outEnabled)
{
	int value = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/NET", "enable_impose_scanlist", &value);

	if (ret >= 0)
		*outEnabled = (value != 0) ? 1 : 0;
	return ret;
}

/* 0x81214740 */
int netCtlConfigGetAdhocChannel(int *outChannel)
{
	int value = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/NET", "adhoc_channel", &value);

	if (ret >= 0)
		*outChannel = (value == 0) ? 11 : value;
	return ret;
}

/* 0x81214788 */
int netCtlConfigGetWifiPowerSave(int *outEnabled)
{
	int value = 1;

	if (sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_power_save", &value) < 0)
		value = 1;
	*outEnabled = (value != 0) ? 1 : 0;
	return 0;
}

/* 0x812147D8 */
int netCtlConfigGetNetFlag(int *outEnabled)
{
	int value = 1;

	if (sceRegMgrGetKeyInt("/CONFIG/NET", "net_flag", &value) < 0)
		value = 1;
	*outEnabled = (value != 0) ? 1 : 0;
	return 0;
}

/* 0x81214828 */
int netCtlConfigGetIcmType(int *outType)
{
	*outType = 1;
	int ret = sceRegMgrGetKeyInt("/CONFIG/NET", "icm_type", outType);

	if (ret < 0) {
		*outType = 1;
		return 0;
	}
	return ret;
}

/* 0x81214852 */
int netCtlConfigGetFakeMobileIf(int *outFake)
{
	*outFake = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/NET", "fake_3g_if", outFake);

	if (ret < 0) {
		*outFake = 0;
		return 0;
	}
	return ret;
}

/* 0x8121487A */
int netCtlConfigGetMobileDns(SceNetInAddr outDns[2])
{
	char dnsStr[16];

	sceClibMemset(outDns, 0, sizeof(SceNetInAddr) * 2);
	sceClibMemset(dnsStr, 0, sizeof(dnsStr));
	sceRegMgrGetKeyStr("/CONFIG/NET", "primary_dns_for_3g", dnsStr, sizeof(dnsStr));
	sceNetInetPton(SCE_NET_AF_INET, dnsStr, &outDns[0]);

	sceClibMemset(dnsStr, 0, sizeof(dnsStr));
	sceRegMgrGetKeyStr("/CONFIG/NET", "secondary_dns_for_3g", dnsStr, sizeof(dnsStr));
	sceNetInetPton(SCE_NET_AF_INET, dnsStr, &outDns[1]);
	return 0;
}

/* 0x812148FE */
int netCtlConfigUpdateCurrentIp(const char *ipStr, SceSize len)
{
	if (sceClibStrncmp(s_cachedCurrentIp, ipStr, len) == 0)
		return 0;
	int ret = sceRegMgrSetKeyStr("/DEVENV/HOST", "current_ip_address", ipStr, len);

	if (ret >= 0)
		sceClibStrlcpy(s_cachedCurrentIp, ipStr, sizeof(s_cachedCurrentIp));
	return ret;
}

/* 0x8121494E */
int netCtlConfigLoadIfStat(NetCtlChannel *ch)
{
	if (s_ifStatDeleted == 1)
		return 0;

	SceUID fd = sceIoOpen("vd0:network/ifstat.bin", SCE_O_RDONLY, 0);

	if (fd >= 0) {
		sceClibMemset(&ch->ifStatEth, 0, sizeof(ch->ifStatEth));
		sceIoRead(fd, &ch->ifStatEth, sizeof(ch->ifStatEth));
		sceClibMemset(&ch->ifStatWifi, 0, sizeof(ch->ifStatWifi));
		sceIoRead(fd, &ch->ifStatWifi, sizeof(ch->ifStatWifi));
		sceClibMemset(&ch->ifStatPhone, 0, sizeof(ch->ifStatPhone));
		sceIoRead(fd, &ch->ifStatPhone, sizeof(ch->ifStatPhone));
		sceIoClose(fd);
		return fd;
	}

	sceIoRemove("vd0:network/ifstat.bin");
	sceIoRmdir("vd0:network");
	return fd;
}

/* 0x81214A0C */
int netCtlConfigSaveIfStat(NetCtlChannel *ch)
{
	if (s_ifStatDeleted == 1)
		return 0;

	int ret = netCtlCoreLock();
	int locked = (ret >= 0);

	if ((ch->flags & NETCTL_FLAG_IFSTAT_DIRTY) == 0) {
		if (locked)
			netCtlCoreUnlock();
		return ret;
	}

	if (locked)
		netCtlCoreUnlock();

	int fileLocked = (netCtlCoreLockFileIo() >= 0);

	sceIoMkdir("vd0:network", 6);
	SceUID fd = sceIoOpen("vd0:network/ifstat.bin",
			      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6);
	if (fd < 0) {
		if (fileLocked)
			netCtlCoreUnlockFileIo();
		return fd;
	}

	sceIoWrite(fd, &ch->ifStatEth, sizeof(ch->ifStatEth));
	sceIoWrite(fd, &ch->ifStatWifi, sizeof(ch->ifStatWifi));
	sceIoWrite(fd, &ch->ifStatPhone, sizeof(ch->ifStatPhone));
	sceIoClose(fd);

	if (fileLocked)
		netCtlCoreUnlockFileIo();

	int relocked = (netCtlCoreLock() >= 0);

	ch->flags = (ch->flags & ~(NETCTL_FLAG_IFSTAT_DIRTY | NETCTL_FLAG_FLUSH_IFSTAT_NOW |
				   NETCTL_FLAG_IFSTAT_SAVED)) |
		    NETCTL_FLAG_IFSTAT_SAVED;
	if (relocked)
		netCtlCoreUnlock();
	return 0;
}

/* 0x81214B0A */
int netCtlConfigDeleteIfStat(void)
{
	int fileLocked = (netCtlCoreLockFileIo() >= 0);

	sceIoRemove("vd0:network/ifstat.bin");
	sceIoRmdir("vd0:network");
	s_ifStatDeleted = 1;
	if (fileLocked)
		netCtlCoreUnlockFileIo();
	return 0;
}

/* 0x81214B4C */
void netCtlExclusiveCallback(int selectedConfigId, int clientPid, int valid, SceUID errorCode,
			     NetCtlChannel *ch)
{
	if (ch->exclusiveRunning == 1) {
		ch->exclusiveError = errorCode;
		ch->exclusiveResult.selectedConfigId = selectedConfigId;
		ch->exclusiveResult.clientPid = (SceUID)clientPid;
		ch->exclusiveResult.valid = valid;
		sceKernelSetEventFlag(ch->eventFlagId, NETCTL_EV_EXCLUSIVE_DONE);
	}
}
