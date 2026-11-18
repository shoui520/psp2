/*
 * The settings ("MTP SETTINGS:"): the interface the content manager uses and the hosts the
 * console is registered with, kept in the registry (/CONFIG/MTP), eight of each kind of host.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <regmgr/regmgr.h>
#include <std/stdio.h>
#include "mtp_internal.h"

#define MTP_TAG_SETTINGS		"MTP SETTINGS:"

#define MTP_REGISTRY_CATEGORY_MTP	"/CONFIG/MTP"
#define MTP_REGISTRY_CATEGORY_NET	"/CONFIG/MTP/NET"
#define MTP_REGISTRY_CATEGORY_SIZE	20

/* the registry's key types */
#define MTP_REGISTRY_TYPE_INT		0
#define MTP_REGISTRY_TYPE_STR		1
#define MTP_REGISTRY_TYPE_BIN		2

/* the kinds of host, each in its own category: PCs, nasne and PS3 */
#define MTP_REGISTRY_HOST_KINDS		3
#define MTP_REGISTRY_HOSTS		(MTP_REGISTRY_HOST_KINDS * NETMTP_MAX_REGISTERED_HOSTS)
#define MTP_REGISTRY_HOST_KEYS		7

/* a registered host: its slot is in use, it is the one the console connects to */
typedef struct MtpRegistryHost {
	SceUInt32 configFlag;
	SceUInt32 assigned;
	MtpNetHost host;
} MtpRegistryHost;

/* what the registry holds (5000 bytes) */
typedef struct MtpRegistrySettings {
	int interfaceType;		/* MTP_IF_PORT_* */
	SceUInt32 numHosts;
	MtpRegistryHost hosts[MTP_REGISTRY_HOSTS];
} MtpRegistrySettings;

static MtpRegistrySettings s_settings;
static SceUID s_settingsMutex;

int loadAllHostsData(void)
{
	RegMgrKeyInfo keys[MTP_REGISTRY_HOST_KEYS];
	char category[MTP_REGISTRY_CATEGORY_SIZE];
	MtpRegistryHost *entry;
	int i;
	int ret;

	entry = &s_settings.hosts[0];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET/%02d", i);
		mtpUtilMemset(keys, 0, sizeof(keys));
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		ret = sceRegMgrGetKeys(category, keys, 6);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 193, "failed to read %s from registry 0x%x\n",
				category, ret);
			return ret;
		}
		entry->configFlag = keys[0].value;
		entry->assigned = keys[1].value;
		entry->host.type = keys[4].value;
		entry++;
	}
	entry = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET2/%02d", i);
		mtpUtilMemset(keys, 0, sizeof(keys));
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		keys[6].size = sizeof(RegMgrKeyInfo);
		keys[6].name = "mac_address";
		keys[6].type = MTP_REGISTRY_TYPE_BIN;
		keys[6].valueSize = 6;
		keys[6].data = entry->host.macAddress;
		ret = sceRegMgrGetKeys(category, keys, 7);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 248, "failed to read %s from registry 0x%x\n",
				category, ret);
			return ret;
		}
		entry->configFlag = keys[0].value;
		entry->assigned = keys[1].value;
		entry->host.type = keys[4].value;
		entry++;
	}
	entry = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS * 2];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET3/%02d", i);
		mtpUtilMemset(keys, 0, sizeof(keys));
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		keys[6].size = sizeof(RegMgrKeyInfo);
		keys[6].name = "mac_address";
		keys[6].type = MTP_REGISTRY_TYPE_BIN;
		keys[6].valueSize = 6;
		keys[6].data = entry->host.macAddress;
		ret = sceRegMgrGetKeys(category, keys, 7);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 303, "failed to read %s from registry 0x%x\n",
				category, ret);
			return ret;
		}
		entry->configFlag = keys[0].value;
		entry->assigned = keys[1].value;
		entry->host.type = keys[4].value;
		entry++;
	}
	return 0;
}

/* the settings as the registry has them; without them, none */
int loadSettingsDataFromRegistry(void)
{
	int value;
	int ret;

	createMutex(&s_settingsMutex);
	mutexLock(s_settingsMutex);
	ret = sceRegMgrGetKeyInt(MTP_REGISTRY_CATEGORY_MTP, "interface_type", &value);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SETTINGS, 324, "failed to read %s/%s from registry 0x%x\n",
			MTP_REGISTRY_CATEGORY_MTP, "interface_type", ret);
	} else {
		s_settings.interfaceType = value;
		ret = sceRegMgrGetKeyInt(MTP_REGISTRY_CATEGORY_NET, "num_hosts", &value);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 332,
				"failed to read %s/%s from registry 0x%x\n",
				MTP_REGISTRY_CATEGORY_NET, "num_hosts", ret);
		} else {
			s_settings.numHosts = value;
			ret = loadAllHostsData();
		}
	}
	if (ret < 0) {
		mtpUtilMemset(&s_settings, 0, sizeof(s_settings));
		s_settings.interfaceType = MTP_IF_PORT_USB;
	}
	mutexUnlock(s_settingsMutex);
	return 0;
}

int saveAllHostsData(void)
{
	RegMgrKeyInfo keys[MTP_REGISTRY_HOST_KEYS];
	char category[MTP_REGISTRY_CATEGORY_SIZE];
	MtpRegistryHost *entry;
	int i;
	int ret;

	entry = &s_settings.hosts[0];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET/%02d", i);
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[0].value = entry->configFlag;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[1].value = entry->assigned;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[4].value = entry->host.type;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		ret = sceRegMgrSetKeys(category, keys, 6);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 407, "failed to write registry %s 0x%x\n",
				category, ret);
			return ret;
		}
		entry++;
	}
	entry = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET2/%02d", i);
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[0].value = entry->configFlag;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[1].value = entry->assigned;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[4].value = entry->host.type;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		keys[6].size = sizeof(RegMgrKeyInfo);
		keys[6].name = "mac_address";
		keys[6].type = MTP_REGISTRY_TYPE_BIN;
		keys[6].valueSize = 6;
		keys[6].data = entry->host.macAddress;
		ret = sceRegMgrSetKeys(category, keys, 7);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 460, "failed to write registry %s 0x%x\n",
				category, ret);
			return ret;
		}
		entry++;
	}
	entry = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS * 2];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		sce_paf_snprintf(category, sizeof(category), "/CONFIG/MTP/NET3/%02d", i);
		keys[0].size = sizeof(RegMgrKeyInfo);
		keys[0].name = "config_flag";
		keys[0].type = MTP_REGISTRY_TYPE_INT;
		keys[0].valueSize = 4;
		keys[0].value = entry->configFlag;
		keys[1].size = sizeof(RegMgrKeyInfo);
		keys[1].name = "assigned";
		keys[1].type = MTP_REGISTRY_TYPE_INT;
		keys[1].valueSize = 4;
		keys[1].value = entry->assigned;
		keys[2].size = sizeof(RegMgrKeyInfo);
		keys[2].name = "host_id";
		keys[2].type = MTP_REGISTRY_TYPE_BIN;
		keys[2].valueSize = 16;
		keys[2].data = entry->host.id;
		keys[3].size = sizeof(RegMgrKeyInfo);
		keys[3].name = "host_name";
		keys[3].type = MTP_REGISTRY_TYPE_STR;
		keys[3].valueSize = 126;
		keys[3].data = entry->host.name;
		keys[4].size = sizeof(RegMgrKeyInfo);
		keys[4].name = "host_type";
		keys[4].type = MTP_REGISTRY_TYPE_INT;
		keys[4].valueSize = 4;
		keys[4].value = entry->host.type;
		keys[5].size = sizeof(RegMgrKeyInfo);
		keys[5].name = "last_connected_time";
		keys[5].type = MTP_REGISTRY_TYPE_BIN;
		keys[5].valueSize = 8;
		keys[5].data = &entry->host.lastConnectedTime;
		keys[6].size = sizeof(RegMgrKeyInfo);
		keys[6].name = "mac_address";
		keys[6].type = MTP_REGISTRY_TYPE_BIN;
		keys[6].valueSize = 6;
		keys[6].data = entry->host.macAddress;
		ret = sceRegMgrSetKeys(category, keys, 7);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SETTINGS, 513, "failed to write registry %s 0x%x\n",
				category, ret);
			return ret;
		}
		entry++;
	}
	return 0;
}

int saveSettingsDataToRegistry(void)
{
	int ret = 0;
	int err;

	err = sceRegMgrSetKeyInt(MTP_REGISTRY_CATEGORY_MTP, "interface_type",
		s_settings.interfaceType);
	if (err < 0) {
		MTP_ERR(MTP_TAG_SETTINGS, 532, "failed to write registry %s/%s 0x%x\n",
			MTP_REGISTRY_CATEGORY_MTP, "interface_type", err);
		return err;
	}
	err = sceRegMgrSetKeyInt(MTP_REGISTRY_CATEGORY_NET, "num_hosts", s_settings.numHosts);
	if (err < 0) {
		MTP_ERR(MTP_TAG_SETTINGS, 539, "failed to write registry %s/%s 0x%x\n",
			MTP_REGISTRY_CATEGORY_NET, "num_hosts", err);
		return err;
	}
	err = saveAllHostsData();
	if (err < 0)
		return err;
	return ret;
}

/* the settings back to the registry, and none in memory */
int mtpRegistryFinalize(void)
{
	mutexLock(s_settingsMutex);
	saveSettingsDataToRegistry();
	mtpUtilMemset(&s_settings, 0, sizeof(s_settings));
	s_settings.interfaceType = MTP_IF_PORT_USB;
	mutexUnlock(s_settingsMutex);
	deleteMutex(s_settingsMutex);
	return 0;
}

int mtpRegistryGetInterfaceType(int *type)
{
	if (type == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	*type = s_settings.interfaceType;
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistrySetInterfaceType(int type)
{
	if (type != MTP_IF_PORT_NET && type != MTP_IF_PORT_USB)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	s_settings.interfaceType = type;
	saveSettingsDataToRegistry();
	mutexUnlock(s_settingsMutex);
	return 0;
}

/* the host the console connects to, of all kinds */
static MtpNetHost *getAssignedHost(void)
{
	MtpRegistryHost *entry = s_settings.hosts;
	SceUInt32 i;

	for (i = 0; i < MTP_REGISTRY_HOSTS; i++, entry++) {
		if (entry->configFlag == 1 && entry->assigned == 1)
			return &entry->host;
	}
	return NULL;
}

int mtpRegistryIsNetMtpHostAssigned(void)
{
	mutexLock(s_settingsMutex);
	if (getAssignedHost() != NULL) {
		mutexUnlock(s_settingsMutex);
		return 1;
	}
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryGetAssignedNetMtpHost(MtpNetHost *host)
{
	MtpNetHost *assigned;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	assigned = getAssignedHost();
	if (assigned == NULL) {
		mutexUnlock(s_settingsMutex);
		return SCE_MTP_ERROR_NET_NOT_ASSIGNED;
	}
	mtpUtilMemcpy(host, assigned, sizeof(*host));
	mutexUnlock(s_settingsMutex);
	return 0;
}

/* the hosts of the host's kind */
static MtpRegistryHost *getHostsOfType(int type)
{
	MtpRegistryHost *hosts = s_settings.hosts;

	if (type == MTP_NET_HOST_TYPE_NASNE)
		hosts = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS];
	else if (type == MTP_NET_HOST_TYPE_PS3)
		hosts = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS * 2];
	return hosts;
}

static MtpRegistryHost *findHost(MtpRegistryHost *entry, const MtpNetHost *host)
{
	SceUInt32 i;

	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1 &&
		    mtpUtilMemcmp(host->id, entry->host.id, sizeof(host->id)) == 0)
			return entry;
	}
	return NULL;
}

static void clearAssigned(void)
{
	MtpRegistryHost *entry = s_settings.hosts;
	SceUInt32 i;

	for (i = 0; i < MTP_REGISTRY_HOSTS; i++, entry++)
		entry->assigned = 0;
}

/* the host becomes the one the console connects to; time is when it connected */
int mtpRegistryAssignNetMtpHost(const MtpNetHost *host, const SceUInt64 *time)
{
	MtpRegistryHost *entry;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	entry = findHost(getHostsOfType(host->type), host);
	if (entry == NULL) {
		mutexUnlock(s_settingsMutex);
		return SCE_MTP_ERROR_DEVICE;
	}
	clearAssigned();
	entry->assigned = 1;
	if (time != NULL)
		entry->host.lastConnectedTime = *time;
	saveSettingsDataToRegistry();
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryUnassignNetMtpHosts(void)
{
	mutexLock(s_settingsMutex);
	clearAssigned();
	saveSettingsDataToRegistry();
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryGetNumRegisteredNetMtpHosts(SceUInt32 *num)
{
	MtpRegistryHost *entry;
	SceUInt32 count;
	int i;

	if (num == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	count = 0;
	entry = s_settings.hosts;
	for (i = 0; i < MTP_REGISTRY_HOSTS; i++, entry++) {
		if (entry->configFlag == 1)
			count++;
	}
	*num = count;
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryGetNumRegisteredPcHosts(SceUInt32 *num)
{
	MtpRegistryHost *entry;
	SceUInt32 count;
	int i;

	if (num == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	count = 0;
	entry = &s_settings.hosts[0];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1)
			count++;
	}
	*num = count;
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryGetNumRegisteredNasneHosts(SceUInt32 *num)
{
	MtpRegistryHost *entry;
	SceUInt32 count;
	int i;

	if (num == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	count = 0;
	entry = &s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS];
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1)
			count++;
	}
	*num = count;
	mutexUnlock(s_settingsMutex);
	return 0;
}

int mtpRegistryGetNumRegisteredPs3Hosts(SceUInt32 *num)
{
	SceUInt32 count;
	int i;

	if (num == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	count = 0;
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++) {
		if (s_settings.hosts[NETMTP_MAX_REGISTERED_HOSTS * 2 + i].configFlag == 1)
			count++;
	}
	*num = count;
	mutexUnlock(s_settingsMutex);
	return 0;
}

/* the index-th registered host, of all kinds */
int mtpRegistryGetRegisteredNetMtpHost(SceUInt32 index, MtpNetHost *host)
{
	MtpRegistryHost *entry;
	SceUInt32 n;
	SceUInt32 i;

	if (index > MTP_REGISTRY_HOSTS || host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	n = 0;
	entry = s_settings.hosts;
	for (i = 0; i < MTP_REGISTRY_HOSTS; i++, entry++) {
		if (entry->configFlag == 1) {
			if (n == index) {
				mtpUtilMemcpy(host, &entry->host, sizeof(*host));
				mutexUnlock(s_settingsMutex);
				return 0;
			}
			n++;
		}
	}
	mutexUnlock(s_settingsMutex);
	return SCE_MTP_ERROR_NET_BAD_TIMING;
}

/* a registered host's new description */
int mtpRegistryUpdateNetMtpHost(const MtpNetHost *host)
{
	MtpRegistryHost *entry;
	SceUInt32 i;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	entry = getHostsOfType(host->type);
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1 &&
		    mtpUtilMemcmp(host->id, entry->host.id, sizeof(host->id)) == 0) {
			mtpUtilMemcpy(&entry->host, host, sizeof(*host));
			saveSettingsDataToRegistry();
			mutexUnlock(s_settingsMutex);
			return 0;
		}
	}
	mutexUnlock(s_settingsMutex);
	return SCE_MTP_ERROR_NET_BAD_TIMING;
}

/* a host into a free slot of its kind, unless it is registered already */
int mtpRegistryRegisterNetMtpHost(const MtpNetHost *host)
{
	MtpRegistryHost *hosts;
	MtpRegistryHost *entry;
	SceUInt32 i;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	hosts = getHostsOfType(host->type);
	entry = hosts;
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1 &&
		    mtpUtilMemcmp(host->id, entry->host.id, sizeof(host->id)) == 0) {
			mutexUnlock(s_settingsMutex);
			return 0;
		}
	}
	entry = hosts;
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 0) {
			mtpUtilMemcpy(&entry->host, host, sizeof(*host));
			s_settings.numHosts++;
			entry->host.lastConnectedTime = 0;
			entry->configFlag = 1;
			entry->assigned = 0;
			saveSettingsDataToRegistry();
			mutexUnlock(s_settingsMutex);
			return 0;
		}
	}
	mutexUnlock(s_settingsMutex);
	return SCE_MTP_ERROR_NET_MAX_HOSTS;
}

int mtpRegistryUnregisterNetMtpHost(const MtpNetHost *host)
{
	MtpRegistryHost *entry;
	SceUInt32 i;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mutexLock(s_settingsMutex);
	entry = getHostsOfType(host->type);
	for (i = 0; i < NETMTP_MAX_REGISTERED_HOSTS; i++, entry++) {
		if (entry->configFlag == 1 &&
		    mtpUtilMemcmp(host->id, entry->host.id, sizeof(host->id)) == 0) {
			s_settings.numHosts--;
			entry->configFlag = 0;
			saveSettingsDataToRegistry();
			mutexUnlock(s_settingsMutex);
			return 0;
		}
	}
	mutexUnlock(s_settingsMutex);
	return SCE_MTP_ERROR_ALREADY;
}
