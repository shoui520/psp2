/*
 * netctl_ipc_server.cpp: SceNetCtlIpcInt IPMI service dispatcher (0x00020000..0x0002003E)
 * 3.65-CEX: 0x8120CEB4 - 0x8120E704
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <ipmi/session.h>
#include <std/string.h>
#include "../../shell.h"
#include "netctl_internal.h"

#define NETCTL_IPC_MAX_CLIENTS		16
#define NETCTL_IPC_MAX_IE_OUT		0x800u
#define NETCTL_IPC_ERR_UNSUPPORTED	-2147351103	/* 0x800205C1 */
#define NETCTL_EVENT_LOG_CATEGORY	20000

extern int shellSvcNotifyEvent(IPMI::Session *session, int ev);		/* 0x811B7560 */
extern int shellSvcLiveAreaSyncDefault(IPMI::Session *, SceUInt32,
				       const IPMI::DataInfo *, SceUInt32,
				       IPMI::BufferInfo *, SceUInt32);	/* 0x8115B554 */

struct NetCtlIpcClient {
	IPMI::Session *session;		/* +0x00 */
	int inetDisconnectErr;		/* +0x04 */
	int inetConnectErr;		/* +0x08 */
	int inetAutoConnectErr;		/* +0x0C */
	int inetScanErr;		/* +0x10 */
	int adhocDisconnectErr;		/* +0x14 */
	int adhocConnectErr;		/* +0x18 */
};

struct NetCtlEventLogReady {
	int errorCode;			/* +0x00 */
	char ipAddress[16];		/* +0x04 */
	char netmask[16];		/* +0x14 */
	char defaultRoute[16];		/* +0x24 */
	char primary_dns[16];		/* +0x34 */
	char secondary_dns[16];		/* +0x44 */
};

struct NetCtlEventLogEntry {
	const void *payload;
	SceSize size;
	SceSize copySize;
};

static int s_netCtlIpcInitialized = 0;			/* 0x81545064 */
static SceUID s_netCtlIpcMutex = -1;			/* 0x81545068 */
static int s_netCtlIpcInetCbId = -1;			/* 0x8154506C */
static int s_netCtlIpcAdhocCbId = -1;			/* 0x81545070 */
static int s_netCtlIpcFlag = 0;				/* 0x81545074 */

static int s_inetConnectPending = 0;			/* 0x8156F410 */
static int s_adhocConnectPending = 0;			/* 0x8156F414 */
static int s_inetAutoConnectPending = 0;		/* 0x8156F418 */
static int s_inetWasConnected = 0;			/* 0x8156F41C */
static int s_adhocWasConnected = 0;			/* 0x8156F420 */
static NetCtlIpcClient s_ipcClients[NETCTL_IPC_MAX_CLIENTS]; /* 0x8156F428 */

static void netCtlIpcEventLogPut(int category, int eventId, int flags,
				 const void *payload, SceSize size)
{
	NetCtlEventLogEntry entry;

	entry.payload = payload;
	entry.size = size;
	entry.copySize = size;
	_vshEventLogPut(category, eventId, flags, &entry);
}

static NetCtlIpcClient *netCtlIpcFindClient(IPMI::Session *session)
{
	for (unsigned int i = 0; i < NETCTL_IPC_MAX_CLIENTS; ++i) {
		if (s_ipcClients[i].session == session)
			return &s_ipcClients[i];
	}
	return NULL;
}

static void netCtlIpcClearClient(NetCtlIpcClient *client)
{
	client->session = NULL;
	client->inetDisconnectErr = 0;
	client->inetConnectErr = 0;
	client->inetAutoConnectErr = 0;
	client->inetScanErr = 0;
	client->adhocDisconnectErr = 0;
	client->adhocConnectErr = 0;
}

/* 0x8120D9FE */
static void netCtlIpcNotifyConnectResult(unsigned int index, int isAdhoc, int isAuto,
					 int prevState, int newState, int eventCode,
					 int errorCode, int *outCompleted)
{
	NetCtlIpcClient *client = &s_ipcClients[index];

	*outCompleted = 0;
	if (newState == NETCTL_RAW_STATE_CONNECTED && (eventCode == 6 || prevState == NETCTL_RAW_STATE_WISPR_AUTH)) {
		if (isAdhoc == 1) {
			client->adhocConnectErr = 0;
			shellSvcNotifyEvent(client->session, 15);
			shellSvcNotifyEvent(client->session, 16);
		} else if (isAuto == 1) {
			client->inetAutoConnectErr = 0;
			shellSvcNotifyEvent(client->session, 3);
			shellSvcNotifyEvent(client->session, 4);
		} else {
			client->inetConnectErr = 0;
			shellSvcNotifyEvent(client->session, 1);
			shellSvcNotifyEvent(client->session, 2);
		}
		*outCompleted = 1;
	} else if (newState == NETCTL_RAW_STATE_IDLE && (eventCode == 7 || eventCode == 9)) {
		int reportedErr = (eventCode == 7) ? SCE_NET_CTL_ERROR_DISCONNECT_REQ : errorCode;

		if (isAdhoc == 1) {
			client->adhocConnectErr = reportedErr;
			shellSvcNotifyEvent(client->session, 15);
			shellSvcNotifyEvent(client->session, 16);
		} else if (isAuto == 1) {
			client->inetAutoConnectErr = reportedErr;
			shellSvcNotifyEvent(client->session, 3);
			shellSvcNotifyEvent(client->session, 4);
		} else {
			client->inetConnectErr = reportedErr;
			shellSvcNotifyEvent(client->session, 1);
			shellSvcNotifyEvent(client->session, 2);
		}
		*outCompleted = 1;
	}
}

/* 0x8120DB24 */
static void netCtlIpcOnNetEvent(int prevState, int newState, int eventCode,
				int errorCode, int reserved, void *userArg)
{
	int isAdhoc = (int)(SceIntPtr)userArg;
	int disconnectedAfterReady = 0;
	int connectDone = 0;
	int autoConnectDone = 0;

	(void)reserved;
	if (sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL) < 0)
		return;

	if (newState == NETCTL_RAW_STATE_CONNECTED) {
		if (isAdhoc == 1)
			s_adhocWasConnected = 1;
		else
			s_inetWasConnected = 1;
	}

	if (isAdhoc == 1) {
		if (s_adhocWasConnected == 1 && newState == NETCTL_RAW_STATE_IDLE) {
			s_adhocWasConnected = 0;
			disconnectedAfterReady = 1;
		}
	} else {
		if (s_inetWasConnected == 1 && newState == NETCTL_RAW_STATE_IDLE) {
			s_inetWasConnected = 0;
			disconnectedAfterReady = 1;
		}
	}

	if (isAdhoc == 0 && eventCode == 2)
		s_inetAutoConnectPending = 1;

	int *pendingConnect = (isAdhoc == 1) ? &s_adhocConnectPending : &s_inetConnectPending;

	for (unsigned int i = 0; i < NETCTL_IPC_MAX_CLIENTS; ++i) {
		NetCtlIpcClient *client = &s_ipcClients[i];

		if (client->session == NULL)
			continue;

		if (*pendingConnect == 1) {
			netCtlIpcNotifyConnectResult(i, isAdhoc, 0, prevState, newState,
						     eventCode, errorCode, &connectDone);
		}
		if (isAdhoc == 0 && s_inetAutoConnectPending == 1) {
			netCtlIpcNotifyConnectResult(i, 0, 1, prevState, newState,
						     eventCode, errorCode, &autoConnectDone);
		}

		if (newState == NETCTL_RAW_STATE_IDLE && eventCode == 7) {
			if (isAdhoc == 1) {
				shellSvcNotifyEvent(client->session, 17);
				shellSvcNotifyEvent(client->session, 18);
			} else {
				shellSvcNotifyEvent(client->session, 5);
				shellSvcNotifyEvent(client->session, 6);
			}
		}

		if (eventCode == 0 && isAdhoc == 0) {
			client->inetScanErr = errorCode;
			shellSvcNotifyEvent(client->session, 7);
			shellSvcNotifyEvent(client->session, 8);
		}

		if (disconnectedAfterReady == 1) {
			int disconnErr;

			if (eventCode == 8)
				disconnErr = SCE_NET_CTL_ERROR_AUTO_DISCONNECT;
			else if (eventCode == 7)
				disconnErr = SCE_NET_CTL_ERROR_DISCONNECT_REQ;
			else
				disconnErr = errorCode;

			if (isAdhoc == 1) {
				client->adhocDisconnectErr = disconnErr;
				shellSvcNotifyEvent(client->session, 19);
				shellSvcNotifyEvent(client->session, 20);
			} else {
				client->inetDisconnectErr = disconnErr;
				shellSvcNotifyEvent(client->session, 9);
				shellSvcNotifyEvent(client->session, 10);
			}
		}

		if (prevState != NETCTL_RAW_STATE_IP_CONFIG &&
		    newState == NETCTL_RAW_STATE_IP_CONFIG && isAdhoc == 0) {
			shellSvcNotifyEvent(client->session, 11);
			shellSvcNotifyEvent(client->session, 12);
		}

		if (newState == NETCTL_RAW_STATE_CONNECTED &&
		    (eventCode == 6 || prevState == NETCTL_RAW_STATE_WISPR_AUTH) &&
		    isAdhoc == 0) {
			shellSvcNotifyEvent(client->session, 13);
			shellSvcNotifyEvent(client->session, 14);
		}
	}

	if (connectDone == 1)
		*pendingConnect = 0;
	if (autoConnectDone == 1)
		s_inetAutoConnectPending = 0;

	if (newState == NETCTL_RAW_STATE_CONNECTED &&
	    (eventCode == 6 || prevState == NETCTL_RAW_STATE_WISPR_AUTH)) {
		NetCtlEventLogReady logReady;
		SceNetCtlInfo info;

		sce_paf_memset(&logReady, 0, sizeof(logReady));
		SceShell_7AD86CE1(SCE_NET_CTL_INFO_IP_ADDRESS, &info);
		sceClibStrlcpy(logReady.ipAddress, info.ip_address, sizeof(logReady.ipAddress));
		SceShell_7AD86CE1(SCE_NET_CTL_INFO_NETMASK, &info);
		sceClibStrlcpy(logReady.netmask, info.netmask, sizeof(logReady.netmask));
		SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEFAULT_ROUTE, &info);
		sceClibStrlcpy(logReady.defaultRoute, info.default_route, sizeof(logReady.defaultRoute));
		SceShell_7AD86CE1(SCE_NET_CTL_INFO_PRIMARY_DNS, &info);
		sceClibStrlcpy(logReady.primary_dns, info.primary_dns, sizeof(logReady.primary_dns));
		SceShell_7AD86CE1(SCE_NET_CTL_INFO_SECONDARY_DNS, &info);
		sceClibStrlcpy(logReady.secondary_dns, info.secondary_dns, sizeof(logReady.secondary_dns));
		netCtlIpcEventLogPut(NETCTL_EVENT_LOG_CATEGORY, 1, 0, &logReady, sizeof(logReady));
	} else if (disconnectedAfterReady == 1) {
		int logErr;

		if (eventCode == 8)
			logErr = SCE_NET_CTL_ERROR_AUTO_DISCONNECT;
		else if (eventCode == 7)
			logErr = SCE_NET_CTL_ERROR_DISCONNECT_REQ;
		else
			logErr = errorCode;

		netCtlIpcEventLogPut(NETCTL_EVENT_LOG_CATEGORY, 2, 0, &logErr, sizeof(logErr));
	} else if (newState == NETCTL_RAW_STATE_IDLE && prevState != NETCTL_RAW_STATE_IDLE) {
		NetCtlEventLogReady logFail;
		int failErr;

		if (eventCode == 8)
			failErr = SCE_NET_CTL_ERROR_AUTO_DISCONNECT;
		else if (eventCode == 7)
			failErr = SCE_NET_CTL_ERROR_DISCONNECT_REQ;
		else
			failErr = errorCode;

		sce_paf_memset(&logFail, 0, sizeof(logFail));
		logFail.errorCode = failErr;
		netCtlIpcEventLogPut(NETCTL_EVENT_LOG_CATEGORY, 1, 0, &logFail, sizeof(logFail));
	}

	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
}

/* 0x8120CEB4 */
int shellNetCtlIpcInit(void)
{
	int ret;

	if (s_netCtlIpcInitialized != 0) {
		ret = NETCTL_ERR_ALREADY_INITIALIZED;
	} else {
		sce_paf_memset(s_ipcClients, 0, sizeof(s_ipcClients));
		s_netCtlIpcMutex = -1;
		ret = sceKernelCreateMutex("SceNetCtlIpcInt", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
		if (ret >= 0) {
			s_netCtlIpcMutex = ret;
			ret = SceShell_2E0B6FA6(netCtlIpcOnNetEvent, (void *)0, &s_netCtlIpcInetCbId);
			if (ret >= 0) {
				ret = SceShell_42053414(netCtlIpcOnNetEvent, (void *)1, &s_netCtlIpcAdhocCbId);
				if (ret >= 0) {
					s_netCtlIpcInitialized = 1;
					return 0;
				}
			}
		}
	}

	if (s_netCtlIpcInetCbId >= 0) {
		SceShell_B4D888D5(s_netCtlIpcInetCbId);
		s_netCtlIpcInetCbId = -1;
	}
	if (s_netCtlIpcAdhocCbId >= 0) {
		SceShell_FDAA5D2E(s_netCtlIpcAdhocCbId);
		s_netCtlIpcAdhocCbId = -1;
	}
	if (s_netCtlIpcMutex >= 0) {
		sceKernelDeleteMutex(s_netCtlIpcMutex);
		s_netCtlIpcMutex = -1;
	}
	return ret;
}

/* 0x8120CFBC */
static int netCtlIpcRegisterClient(IPMI::Session *session)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	unsigned int i;

	for (i = 0; i < NETCTL_IPC_MAX_CLIENTS; ++i) {
		if (s_ipcClients[i].session == NULL)
			break;
	}
	if (i == NETCTL_IPC_MAX_CLIENTS)
		ret = SCE_NET_CTL_ERROR_NOT_AVAIL;
	else
		s_ipcClients[i].session = session;

	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D01E */
static int netCtlIpcUnregisterClient(IPMI::Session *session)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	NetCtlIpcClient *client = netCtlIpcFindClient(session);

	if (client == NULL) {
		ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
	} else {
		netCtlIpcClearClient(client);
		ret = 0;
	}
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D090 */
static int netCtlIpcInetGetResult(IPMI::Session *session, int eventType, int *outError)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	NetCtlIpcClient *client = netCtlIpcFindClient(session);

	if (client == NULL) {
		ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
	} else if (eventType == SCE_NET_CTL_EVENT_TYPE_DISCONNECTED) {
		*outError = client->inetDisconnectErr;
		ret = 0;
	} else {
		ret = SCE_NET_CTL_ERROR_INVALID_TYPE;
	}
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D11E */
static int netCtlIpcInetGetResultEx(IPMI::Session *session, int eventType, int *outError)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	NetCtlIpcClient *client = netCtlIpcFindClient(session);

	if (client == NULL) {
		ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
	} else {
		ret = 0;
		switch (eventType) {
		case 1:
			*outError = client->inetDisconnectErr;
			break;
		case 2:
			*outError = client->inetConnectErr;
			break;
		case 4:
			*outError = client->inetScanErr;
			break;
		case 7:
			*outError = client->inetAutoConnectErr;
			break;
		default:
			ret = SCE_NET_CTL_ERROR_INVALID_TYPE;
			break;
		}
	}
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D1D0 */
static int netCtlIpcAdhocGetResult(IPMI::Session *session, int eventType, int *outError)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	NetCtlIpcClient *client = netCtlIpcFindClient(session);

	if (client == NULL) {
		ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
	} else if (eventType == SCE_NET_CTL_EVENT_TYPE_DISCONNECTED) {
		*outError = client->adhocDisconnectErr;
		ret = 0;
	} else {
		ret = SCE_NET_CTL_ERROR_INVALID_TYPE;
	}
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D25E */
static int netCtlIpcAdhocGetResultEx(IPMI::Session *session, int eventType, int *outError)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	NetCtlIpcClient *client = netCtlIpcFindClient(session);

	if (client == NULL) {
		ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
	} else if (eventType == 1) {
		*outError = client->adhocDisconnectErr;
		ret = 0;
	} else if (eventType == 2) {
		*outError = client->adhocConnectErr;
		ret = 0;
	} else {
		ret = SCE_NET_CTL_ERROR_INVALID_TYPE;
	}
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D2FE */
static int netCtlIpcInetAutoConnect(IPMI::Session *session)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiInetAutoConnect(session->getProcessId());
	if (ret >= 0)
		s_inetConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D34C */
static int netCtlIpcInetConnectById(int configId)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiInetConnectById(configId);
	if (ret >= 0)
		s_inetConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D394 */
static int netCtlIpcInetConnectWithConfig(const NetCtlNetConfig *inCfg, const void *ieProbe,
					  const void *ieAssoc)
{
	NetCtlNetConfig localCfg;
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	sceClibMemcpy_safe(&localCfg, inCfg, sizeof(localCfg));
	if (localCfg.customIeProbeSize != 0)
		localCfg.customIeProbe = ieProbe;
	if (localCfg.customIeAssocSize != 0)
		localCfg.customIeAssoc = ieAssoc;

	ret = SceShell_8C1D932A(&localCfg);
	if (ret >= 0)
		s_inetConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D42C */
static int netCtlIpcInetConnectWithConfigEx(const NetCtlNetConfig *inCfg, const void *ieProbe,
					    const void *ieAssoc, SceUID targetPid)
{
	NetCtlNetConfig localCfg;
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	sceClibMemcpy_safe(&localCfg, inCfg, sizeof(localCfg));
	if (localCfg.customIeProbeSize != 0)
		localCfg.customIeProbe = ieProbe;
	if (localCfg.customIeAssocSize != 0)
		localCfg.customIeAssoc = ieAssoc;

	if (targetPid == -1)
		ret = SceShell_8C1D932A(&localCfg);
	else
		ret = netCtlApiInetConnectWithPid(&localCfg, targetPid);

	if (ret >= 0)
		s_inetConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D4EE..0x8120D5E4 */
static int netCtlIpcAdhocCreateGroup(IPMI::Session *session, const char *ssid)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiAdhocCreateGroup(session->getProcessId(), ssid);
	if (ret >= 0)
		s_adhocConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

static int netCtlIpcAdhocJoinGroup(IPMI::Session *session, const char *ssid)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiAdhocJoinGroup(session->getProcessId(), ssid);
	if (ret >= 0)
		s_adhocConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

static int netCtlIpcAdhocCreatePspGroup(IPMI::Session *session, const char *ssid)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiAdhocCreatePspGroup(session->getProcessId(), ssid);
	if (ret >= 0)
		s_adhocConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

static int netCtlIpcAdhocJoinPspGroup(IPMI::Session *session, const char *ssid, int timeout)
{
	int ret = sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL);

	if (ret < 0)
		return ret;

	ret = netCtlApiAdhocJoinPspGroup(session->getProcessId(), ssid, timeout);
	if (ret >= 0)
		s_adhocConnectPending = 1;
	sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	return ret;
}

/* 0x8120D696 */
static int netCtlIpcStartScan(const char *packedSsids, SceUInt32 count,
			      const NetCtlScanParam *inParam, const void *customIe)
{
	NetCtlScanParam localParam;
	const char *ssidPtrs[NETCTL_MAX_SCAN_SSIDS];

	if (count > NETCTL_MAX_SCAN_SSIDS)
		return NETCTL_ERR_INVALID_SSID_COUNT;

	for (SceUInt32 i = 0; i < count; ++i)
		ssidPtrs[i] = packedSsids + (i * 33);

	sceClibMemcpy_safe(&localParam, inParam, sizeof(localParam));
	if (localParam.customIeSize != 0)
		localParam.customIeData = customIe;

	return SceShell_449348B6(ssidPtrs, count, &localParam);
}

/* 0x8120D712 */
static int netCtlIpcGetBcastScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount,
				       int *outActualCount)
{
	*outActualCount = 0;
	int ret = SceShell_0025417F(outList, maxCount);

	if (ret >= 0)
		*outActualCount = ret;
	return ret;
}

/* 0x8120D72A */
static int netCtlIpcGetBcastScanInfoByBssid(const SceNetEtherAddr *bssid,
					    NetCtlScanInfo *outInfo, void *outIe,
					    SceSize maxIeSize, SceSize *ioIeSize)
{
	if (maxIeSize > NETCTL_IPC_MAX_IE_OUT)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	*ioIeSize = maxIeSize;
	return SceShell_EC75C2F3(bssid, outInfo, outIe, ioIeSize);
}

/* 0x8120D752 */
static int netCtlIpcGetSingleScanInfo(NetCtlScanInfo *outInfo, void *outIe,
				      SceSize maxIeSize, SceSize *ioIeSize)
{
	if (maxIeSize > NETCTL_IPC_MAX_IE_OUT)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	*ioIeSize = maxIeSize;
	return SceShell_05D6FCB0(outInfo, outIe, ioIeSize);
}

/* 0x8120D772 */
static int netCtlIpcGetMultiScanBssids(SceNetEtherAddr *outList, SceUInt32 maxCount,
				       int *outActualCount)
{
	*outActualCount = 0;
	int ret = SceShell_3059EEBC(outList, maxCount);

	if (ret >= 0)
		*outActualCount = ret;
	return ret;
}

/* 0x8120D78A */
static int netCtlIpcGetMultiScanInfoByBssid(const SceNetEtherAddr *bssid,
					    NetCtlScanInfo *outInfo, void *outIe,
					    SceSize maxIeSize, SceSize *ioIeSize)
{
	if (maxIeSize > NETCTL_IPC_MAX_IE_OUT)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	*ioIeSize = maxIeSize;
	return SceShell_3E737FBE(bssid, outInfo, outIe, ioIeSize);
}

/* 0x8120D866 / 0x8120D890 */
static int netCtlIpcGetIfStat(int device, SceSize size, SceNetCtlIfStat *outStat)
{
	if (size != sizeof(SceNetCtlIfStat))
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	sceClibMemset(outStat, 0, sizeof(SceNetCtlIfStat));
	outStat->size = sizeof(SceNetCtlIfStat);
	return netCtlApiGetIfStat(device, outStat);
}

/* 0x8120D8F0 */
static int netCtlIpcGetWifiRegConfigs(const SceUInt32 *configIds, NetCtlRegConfig *outEntries,
				      SceUInt32 count)
{
	for (SceUInt32 i = 0; i < count; ++i)
		outEntries[i].configId = configIds[i];
	return SceShell_A25CD3F2(outEntries, count);
}

/* 0x8120DEEA */
static int netCtlIpcDispatchSync(IPMI::Session *session, SceUInt32 methodId,
				 const IPMI::DataInfo *inList, SceUInt32 inCount,
				 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	int ret;

	(void)inCount;
	switch (methodId) {
	case 0x20000:
		ret = netCtlIpcRegisterClient(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20001:
		ret = netCtlIpcUnregisterClient(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20002:
		ret = netCtlIpcInetGetResult(session, *(const int *)inList[0].pData,
					     (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20003:
		ret = netCtlIpcInetGetResultEx(session, *(const int *)inList[0].pData,
					       (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20004:
		ret = netCtlIpcAdhocGetResult(session, *(const int *)inList[0].pData,
					      (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20005:
		ret = netCtlIpcAdhocGetResultEx(session, *(const int *)inList[0].pData,
						(int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20006:
		ret = netCtlIpcInetAutoConnect(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20007:
		ret = netCtlIpcInetConnectById(*(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20008:
		ret = netCtlIpcInetConnectWithConfig((const NetCtlNetConfig *)inList[1].pData,
						     inList[2].pData, inList[3].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20009: {
		const int *args = (const int *)inList[0].pData;

		ret = netCtlIpcInetConnectWithConfigEx((const NetCtlNetConfig *)inList[1].pData,
						       inList[2].pData, inList[3].pData, args[2]);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x2000A:
		ret = SceShell_3F41E440();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2000B:
		ret = SceShell_7AD86CE1(*(const int *)inList[0].pData,
					(SceNetCtlInfo *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceNetCtlInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2000C:
		ret = netCtlApiGetMacAddress((SceNetEtherAddr *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceNetEtherAddr);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2000D:
		ret = netCtlApiInetGetState((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2000E:
		ret = SceShell_77642C92((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2000F:
		ret = netCtlApiGetMobileStatus((int *)outList[0].pBuffer);
		outList[0].outSize = 8;
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20010:
		ret = netCtlIpcStartScan((const char *)inList[1].pData,
					 *(const SceUInt32 *)inList[0].pData,
					 (const NetCtlScanParam *)inList[2].pData,
					 inList[3].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20011:
		ret = netCtlApiGetScanResultCount();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20012:
		ret = netCtlApiGetScanResultByIndex(*(const SceUInt32 *)inList[0].pData,
						    (NetCtlScanInfo *)outList[0].pBuffer);
		outList[0].outSize = sizeof(NetCtlScanInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20013: {
		int *outCountPtr = (int *)outList[1].pBuffer;

		ret = netCtlIpcGetBcastScanBssids((SceNetEtherAddr *)outList[0].pBuffer,
						  *(const SceUInt32 *)inList[0].pData,
						  outCountPtr);
		outList[0].outSize = (SceSize)(6 * (*outCountPtr));
		outList[1].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20014: {
		SceSize *ioIeSize = (SceSize *)outList[2].pBuffer;

		ret = netCtlIpcGetBcastScanInfoByBssid((const SceNetEtherAddr *)inList[1].pData,
						       (NetCtlScanInfo *)outList[0].pBuffer,
						       outList[1].pBuffer,
						       *(const SceSize *)inList[0].pData,
						       ioIeSize);
		outList[0].outSize = sizeof(NetCtlScanInfo);
		outList[1].outSize = *ioIeSize;
		outList[2].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20015: {
		SceSize *ioIeSize = (SceSize *)outList[2].pBuffer;

		ret = netCtlIpcGetSingleScanInfo((NetCtlScanInfo *)outList[0].pBuffer,
						 outList[1].pBuffer,
						 *(const SceSize *)inList[0].pData,
						 ioIeSize);
		outList[0].outSize = sizeof(NetCtlScanInfo);
		outList[1].outSize = *ioIeSize;
		outList[2].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20016: {
		int *outCountPtr = (int *)outList[1].pBuffer;

		ret = netCtlIpcGetMultiScanBssids((SceNetEtherAddr *)outList[0].pBuffer,
						  *(const SceUInt32 *)inList[0].pData,
						  outCountPtr);
		outList[0].outSize = (SceSize)(6 * (*outCountPtr));
		outList[1].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20017: {
		SceSize *ioIeSize = (SceSize *)outList[2].pBuffer;

		ret = netCtlIpcGetMultiScanInfoByBssid((const SceNetEtherAddr *)inList[1].pData,
						       (NetCtlScanInfo *)outList[0].pBuffer,
						       outList[1].pBuffer,
						       *(const SceSize *)inList[0].pData,
						       ioIeSize);
		outList[0].outSize = sizeof(NetCtlScanInfo);
		outList[1].outSize = *ioIeSize;
		outList[2].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20018:
		ret = SceShell_36788D70((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20019:
		ret = SceShell_8858B876((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001A:
		ret = SceShell_29A14252(*(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001B:
		ret = netCtlIpcAdhocCreateGroup(session, (const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001C:
		ret = netCtlIpcAdhocJoinGroup(session, (const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001D:
		ret = netCtlIpcAdhocCreatePspGroup(session, (const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001E:
		ret = netCtlIpcAdhocJoinPspGroup(session, (const char *)inList[1].pData,
						 *(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2001F:
		ret = SceShell_C1886828();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20020:
		ret = netCtlApiAdhocGetState((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20021:
		ret = netCtlApiAdhocGetInfo(*(const int *)inList[0].pData, outList[0].pBuffer);
		outList[0].outSize = 36;
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20022:
		ret = SceShell_5DC4A753((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20023:
		ret = netCtlApiAdhocGetInAddr((SceNetInAddr *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceNetInAddr);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20024: {
		SceNetCtlNatInfo *nat = (SceNetCtlNatInfo *)outList[0].pBuffer;

		nat->size = *(const SceSize *)inList[0].pData;
		ret = netCtlApiGetNatInfo(nat);
		outList[0].outSize = sizeof(SceNetCtlNatInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20025: {
		NetCtlNatDetailInfo *nat = (NetCtlNatDetailInfo *)outList[0].pBuffer;

		nat->size = *(const SceSize *)inList[0].pData;
		ret = netCtlApiGetNatDetailInfo(nat);
		outList[0].outSize = sizeof(NetCtlNatDetailInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20026:
		ret = netUpnpGetLastError((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20027:
		ret = netCtlApiFreeze(session->getProcessId());
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20028:
		ret = netCtlApiUnfreeze();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20029:
		ret = netCtlApiSuppressAutoConnect(session->getProcessId());
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002A:
		ret = netCtlApiResumeAutoConnect(session->getProcessId());
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002B:
		ret = netCtlApiForceImposePrompt(session->getProcessId());
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002C:
		ret = netCtlApiClearImposePrompt(session->getProcessId());
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002D:
		ret = netCtlApiResetExclusivePrompt();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002E:
		ret = netCtlApiGetWisprStatus((NetCtlWisprStatus *)outList[0].pBuffer);
		outList[0].outSize = sizeof(NetCtlWisprStatus);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2002F:
		ret = netCtlApiAbortExclusive();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20030:
	case 0x20031: {
		const int *args = (const int *)inList[0].pData;

		ret = netCtlIpcGetIfStat(args[0], (SceSize)args[1],
					 (SceNetCtlIfStat *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceNetCtlIfStat);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20032:
		ret = netCtlApiClearIfStat(*(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20033:
		ret = netCtlApiDeleteIfStat();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20034: {
		const SceUID *args = (const SceUID *)inList[0].pData;

		ret = netCtlApiLinkProcess(args[0], args[1]);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20035: {
		const SceUID *args = (const SceUID *)inList[0].pData;

		ret = netCtlApiUnlinkProcess(args[0], args[1]);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20036:
		ret = SceShell_60129092((const NetCtlRegConfig *)inList[1].pData,
					*(const SceUInt32 *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20037: {
		SceUInt32 count = *(const SceUInt32 *)inList[0].pData;

		ret = netCtlIpcGetWifiRegConfigs((const SceUInt32 *)inList[1].pData,
						 (NetCtlRegConfig *)outList[0].pBuffer, count);
		outList[0].outSize = sizeof(NetCtlRegConfig) * count;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}

	case 0x20038:
		ret = netCtlApiSetEthRegConfig((const NetCtlRegConfig *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x20039:
		ret = netCtlApiGetEthRegConfig((NetCtlRegConfig *)outList[0].pBuffer);
		outList[0].outSize = sizeof(NetCtlRegConfig);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2003A:
		ret = netUpnpEnablePortCheck(session);
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2003B:
		ret = netUpnpDisablePortCheck();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	case 0x2003C:
		*(int *)outList[0].pBuffer = s_netCtlIpcFlag;
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(0, outList, outCount);

	case 0x2003D:
		s_netCtlIpcFlag = *(const int *)inList[0].pData;
		return session->respondToSyncMethodRequest(0, outList, outCount);

	case 0x2003E:
		ret = netCtlApiIsAdhocCooldownExpired();
		return session->respondToSyncMethodRequest(ret, outList, outCount);

	default:
		return NETCTL_IPC_ERR_UNSUPPORTED;
	}
}

/* 0x8120D958 */
int shellSvcDisp_02_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return netCtlIpcDispatchSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x8120D96C */
int shellSvcDisp_02_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)reqId;
	return shellSvcLiveAreaSyncDefault(session, methodId, inList, inCount, NULL, 0);
}

/* 0x8120D97C */
void shellSvcDisp_02_Disconn(IPMI::Session *session)
{
	if (sceKernelLockMutex(s_netCtlIpcMutex, 1, NULL) >= 0) {
		NetCtlIpcClient *client = netCtlIpcFindClient(session);

		if (client != NULL)
			netCtlIpcClearClient(client);
		sceKernelUnlockMutex(s_netCtlIpcMutex, 1);
	}
	netUpnpReleaseOwnerSession(session);
}
