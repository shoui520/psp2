/*
 * adhocctl.c: module entrypoints and SceNetAdhocctl public/internal APIs
 * for SceLibPspnetAdhoc (vs0:sys/external/pspnet_adhoc.suprx).
 */

#include "pspnet_adhoc_internal.h"

int g_adhocctlInitialized;
SceNetAdhocctlGroupName g_adhocctlGroupName;
SceNetAdhocctlAdhocId g_adhocctlAdhocId;

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (adhocThreadModuleStart() < 0)
		return 2;
	SceNetCtl_326FEE96(ScePspnetAdhoc_8938A4D0);
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	adhocThreadModuleStop();
	SceNetCtl_326FEE96(NULL);
	ScePspnetAdhoc_8938A4D0();
	return 0;
}

__attribute__((noinline))
void adhocctlGetAdhocIdCopy(SceNetAdhocctlAdhocId *adhocId)
{
	if (adhocId != NULL)
		sceClibMemcpy_safe(adhocId, &g_adhocctlAdhocId, sizeof(*adhocId));
}

__attribute__((noinline))
int ScePspnetAdhoc_5C83C5C8(const SceNetAdhocctlGroupName *groupName)
{
	if (groupName != NULL)
		return (int)sceClibMemcpy_safe(&g_adhocctlGroupName, groupName,
						sizeof(g_adhocctlGroupName));
	return (int)sceClibMemset(&g_adhocctlGroupName, 0, sizeof(g_adhocctlGroupName));
}

__attribute__((noinline))
void adhocctlGetGroupNameCopy(SceNetAdhocctlGroupName *groupName)
{
	sceClibMemcpy_safe(groupName, &g_adhocctlGroupName, sizeof(*groupName));
}

__attribute__((noinline))
int sceNetAdhocctlInit(const struct SceNetAdhocctlAdhocId *adhocId)
{
	int ret;

	if (g_adhocctlInitialized > 0)
		return SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED;
	if (adhocId == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	if (adhocId->type != SCE_NET_ADHOCCTL_ADHOCTYPE_PRODUCT_ID &&
	    adhocId->type != SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED &&
	    adhocId->type != SCE_NET_ADHOCCTL_ADHOCTYPE_SYSTEM)
		return SCE_NET_CTL_ERROR_INVALID_ID;
	ret = adhocAuthKeyInit();
	if (ret < 0)
		return ret;
	ret = adhocAuthStopLockInit();
	if (ret >= 0) {
		ret = adhocAuthMbxCreate("SceNetAdhocAuthMbx");
		if (ret >= 0) {
			sceClibMemcpy_safe(&g_adhocctlAdhocId, adhocId, sizeof(g_adhocctlAdhocId));
			g_adhocctlInitialized = 1;
			return 0;
		}
		adhocAuthStopLockTerm();
	}
	adhocAuthKeyTerm();
	return ret;
}

__attribute__((noinline))
int sceNetAdhocctlTerm(void)
{
	if (g_adhocctlInitialized > 0) {
		adhocAuthStopLockTerm();
		adhocAuthKeyTerm();
		adhocAuthMbxDelete();
		g_adhocctlInitialized = 0;
	}
	return 0;
}

__attribute__((noinline))
int ScePspnetAdhoc_F87F9518(const char *groupName, char *ssid, SceSize size)
{
	char typeChar;
	char layerChar = 'L';
	char prefix[4];
	int ret, len, i;

	if (ssid == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	if (size == 0 || size < 33)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	switch (g_adhocctlAdhocId.type) {
	case SCE_NET_ADHOCCTL_ADHOCTYPE_PRODUCT_ID:
		typeChar = 'A';
		break;
	case SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED:
		typeChar = 'X';
		break;
	case SCE_NET_ADHOCCTL_ADHOCTYPE_SYSTEM:
		typeChar = 'S';
		break;
	default:
		return SCE_NET_CTL_ERROR_INVALID_TYPE;
	}
	sceClibMemset(ssid, 0, size);
	sceClibMemset(prefix, 0, sizeof(prefix));
	ret = sceRegMgrSystemParamGetStr(PSPNET_ADHOC_REG_SSID_PREFIX_ID, prefix, sizeof(prefix));
	if (ret < 0)
		return ret;
	sceClibMemcpy_safe(ssid, prefix, 3);
	sceClibMemcpy_safe(ssid + 3, "_", 1);
	sceClibMemcpy_safe(ssid + 4, &typeChar, 1);
	sceClibMemcpy_safe(ssid + 5, g_adhocctlAdhocId.data, SCE_NET_ADHOCCTL_ADHOCID_LEN);
	sceClibMemcpy_safe(ssid + 14, "_", 1);
	sceClibMemcpy_safe(ssid + 15, &layerChar, 1);
	sceClibMemcpy_safe(ssid + 16, "_", 1);
	len = 17;
	if (groupName != NULL && groupName[0] != '\0') {
		for (i = 0; i < SCE_NET_ADHOCCTL_GROUPNAME_LEN; i++) {
			if (groupName[i] == '\0')
				break;
			sceClibMemcpy_safe(ssid + len, &groupName[i], 1);
			len++;
		}
	}
	return len;
}

__attribute__((noinline))
int sceNetAdhocctlGetAdhocId(struct SceNetAdhocctlAdhocId *adhocId)
{
	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	if (adhocId == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	sceClibMemcpy_safe(adhocId, &g_adhocctlAdhocId, sizeof(*adhocId));
	return 0;
}

__attribute__((noinline))
int sceNetAdhocctlGetPeerList(int *buflen, void *buf)
{
	return adhocAuthGetPeerList(buflen, buf);
}

__attribute__((noinline))
int sceNetAdhocctlGetPeerInfo(const struct SceNetEtherAddr *addr, int size,
			      struct SceNetAdhocctlPeerInfo *peerInfo)
{
	(void)size;
	if (addr == NULL || peerInfo == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	return adhocAuthGetPeerInfo(addr, peerInfo);
}

__attribute__((noinline))
int sceNetAdhocctlGetAddrByName(const struct SceNetAdhocctlNickname *nickname,
				int *buflen, void *buf)
{
	if (nickname == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	return adhocAuthGetAddrByName(nickname, buflen, buf);
}

__attribute__((noinline))
int sceNetAdhocctlGetNameByAddr(const struct SceNetEtherAddr *addr,
				struct SceNetAdhocctlNickname *nickname)
{
	if (addr == NULL || nickname == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	return adhocAuthGetNameByAddr(addr, nickname);
}

__attribute__((noinline))
int sceNetAdhocctlGetParameter(struct SceNetAdhocctlParameter *parameter)
{
	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	if (parameter == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	return adhocAuthGetParameter(parameter);
}

__attribute__((noinline))
int sceNetAdhocctlGetEtherAddr(struct SceNetEtherAddr *addr)
{
	SceUInt32 swVer;
	SceNetEtherAddr mac;
	SceNetCtlInfo info;
	int ret;

	swVer = SceLibKernel_9F793F84();
	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	if (addr == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	if (swVer >= 0x03000000U) {
		ret = SceNetCtl_6D63C34B(&mac);
		if (ret >= 0)
			sceClibMemcpy_safe(addr, &mac, sizeof(*addr));
	} else {
		ret = SceNetCtl_CDEBA500(SCE_NET_CTL_INFO_ETHER_ADDR, &info);
		if (ret >= 0)
			sceClibMemcpy_safe(addr, &info.ether_addr, sizeof(*addr));
	}
	return ret;
}
