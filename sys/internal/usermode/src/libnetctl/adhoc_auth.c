/*
 * SceNetCtl ad-hoc authentication: the handshake consoles run with each other before an
 * ad-hoc game talks. A beacon announces this console (its peer record and TLVs) on the
 * authentication port; each peer that answers goes through four exchanges, computed by the
 * game card authentication manager (LibKernel's ad-hoc requests), and the keys of a finished
 * handshake are installed in the network stack for that peer. A callout thread runs the
 * timers (beacon, resend, keepalive, peer timeout); the authentication thread receives the
 * packets and the timers' messages.
 */

#include "adhoc_auth.h"

int g_adhocAuthPeerPoolCount = 0;
NetCtlAdhocAuthPeer *g_adhocAuthPeerPool = NULL;
NetCtlAdhocAuth *g_adhocAuth = NULL;
SceKernelLwMutexWork *g_adhocAuthStopLock = NULL;
int g_adhocAuthSwVersion = 0;

int g_adhocAuthInitialized;
NetCtlAdhocAuth g_adhocAuthContext;

/* an empty list of peers */
static inline __attribute__((always_inline)) void netCtlAdhocAuthPeerListInit(NetCtlAdhocAuthPeer *head)
{
	head->next = head;
	head->prev = head;
}

/* stops the authentication thread of the session; the caller holds the stop lock */
static inline __attribute__((always_inline)) void netCtlAdhocAuthStopThread(void)
{
	int locked;

	g_adhocAuth->stop = 1;
	netCtlAdhocAuthMbxCancel();
	sceKernelWaitThreadEnd(g_adhocAuth->thread, NULL, NULL);
	sceKernelDeleteThread(g_adhocAuth->thread);
	locked = netCtlAdhocAuthLock();
	netCtlAdhocAuthCalloutRemove(&g_adhocAuth->beaconTimer, NULL);
	netCtlAdhocAuthCalloutTerm();
	g_adhocAuth = NULL;
	netCtlAdhocAuthUnlock(locked);
}

/* stops the session if one runs */
static inline __attribute__((always_inline)) void netCtlAdhocAuthAbort(void)
{
	if (g_adhocAuthInitialized > 0) {
		sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
		if (g_adhocAuth != NULL)
			netCtlAdhocAuthStopThread();
		sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	}
}

__attribute__((noinline))
int netCtlAdhocAuthInit(void)
{
	int ret;

	if (g_adhocAuthInitialized > 0)
		return NETCTL_ERROR_ADHOC_AUTH_ALREADY_INITIALIZED;
	ret = netCtlAdhocAuthKeyInit();
	if (ret < 0)
		return ret;
	ret = netCtlAdhocAuthStopLockInit();
	if (ret >= 0) {
		ret = netCtlAdhocAuthMbxCreate("SceNetAdhocAuthMbx");
		if (ret >= 0) {
			g_adhocAuthInitialized = 1;
			return 0;
		}
		netCtlAdhocAuthStopLockTerm();
	}
	netCtlAdhocAuthKeyTerm();
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthTerm(void)
{
	if (g_adhocAuthInitialized > 0) {
		netCtlAdhocAuthStopLockTerm();
		netCtlAdhocAuthKeyTerm();
		netCtlAdhocAuthMbxDelete();
		g_adhocAuthInitialized = 0;
	}
	return 0;
}

/* name unknown: the network stack's name of a communication ID */
__attribute__((noinline))
int SceNetCtl_FFDC123A(const SceNpCommunicationId *commId, char *name, SceSize size)
{
	if (size <= 32)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	sceClibMemset(name, 0, size);
	sceClibSnprintf(name, size - 1, "SCE_%s_%02d", commId->data, commId->num);
	return 0;
}

__attribute__((noinline))
NetCtlAdhocAuthPeer *netCtlAdhocAuthPeerAlloc(void)
{
	int locked, i;

	if (g_adhocAuthSwVersion < NETCTL_ADHOC_AUTH_POOL_VERSION)
		return SceNetInternal_A2C2BC5F(sizeof(NetCtlAdhocAuthPeer), 0);
	locked = netCtlAdhocAuthLock();
	for (i = 0; i < g_adhocAuthPeerPoolCount; i++) {
		if (g_adhocAuthPeerPool[i].used == 0) {
			sceClibMemset(&g_adhocAuthPeerPool[i], 0, sizeof(NetCtlAdhocAuthPeer));
			g_adhocAuthPeerPool[i].used = 1;
			return &g_adhocAuthPeerPool[i];
		}
	}
	netCtlAdhocAuthUnlock(locked);
	return NULL;
}

__attribute__((noinline))
void netCtlAdhocAuthPeerFree(NetCtlAdhocAuthPeer *peer)
{
	int locked, i;

	if (g_adhocAuthSwVersion < NETCTL_ADHOC_AUTH_POOL_VERSION) {
		SceNetInternal_E62A2FAA(peer);
		return;
	}
	if (peer == NULL)
		return;
	locked = netCtlAdhocAuthLock();
	for (i = 0; i < g_adhocAuthPeerPoolCount; i++) {
		if (&g_adhocAuthPeerPool[i] == peer)
			sceClibMemset(peer, 0, sizeof(NetCtlAdhocAuthPeer));
	}
	netCtlAdhocAuthUnlock(locked);
}

__attribute__((noinline))
void netCtlAdhocAuthPeerPoolTerm(void)
{
	if (g_adhocAuthSwVersion < NETCTL_ADHOC_AUTH_POOL_VERSION)
		return;
	if (g_adhocAuthPeerPool != NULL) {
		SceNetInternal_E62A2FAA(g_adhocAuthPeerPool);
		g_adhocAuthPeerPool = NULL;
	}
	g_adhocAuthPeerPoolCount = 0;
}

__attribute__((noinline))
void netCtlAdhocAuthDeleteCommId(void)
{
	char name[16];
	int s;

	s = sceNetSocket("SceNetAdhocAuthDelComId", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	sceClibMemset(name, 0, sizeof(name));
	SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_DELETE_COMM_ID, name);
	sceNetSocketClose(s);
}

__attribute__((noinline))
int netCtlAdhocAuthStopLockInit(void)
{
	int ret;

	ret = netCtlAdhocAuthLockInit();
	if (ret < 0)
		return ret;
	g_adhocAuthStopLock = SceNetInternal_A2C2BC5F(sizeof(SceKernelLwMutexWork), 0);
	if (g_adhocAuthStopLock == NULL)
		return SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY;
	ret = sceKernelCreateLwMutex(g_adhocAuthStopLock, "SceNetAdhocAuthStopLock",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	if (ret < 0) {
		SceNetInternal_E62A2FAA(g_adhocAuthStopLock);
		g_adhocAuthStopLock = NULL;
		return ret;
	}
	return 0;
}

/* name unknown: stops the ad-hoc authentication session */
__attribute__((noinline))
int SceNetCtl_15BEEEF5(void)
{
	int ret = 0;

	if (g_adhocAuthInitialized <= 0)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
	if (g_adhocAuth != NULL)
		netCtlAdhocAuthStopThread();
	else
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return ret;
}

__attribute__((noinline))
void netCtlAdhocAuthStopLockTerm(void)
{
	NetCtlAdhocAuthKeyParam param;
	int s;

	netCtlAdhocAuthAbort();
	if (g_adhocAuthStopLock != NULL) {
		sceKernelDeleteLwMutex(g_adhocAuthStopLock);
		SceNetInternal_E62A2FAA(g_adhocAuthStopLock);
		g_adhocAuthStopLock = NULL;
	}
	s = sceNetSocket("SceNetAdhocAuthComId", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	if (s >= 0) {
		sceClibMemset(&param, 0, sizeof(param));
		param.type = 3;
		SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_CLEAR_KEYS, &param);
		sceNetSocketClose(s);
	}
	netCtlAdhocAuthLockTerm();
}

/*
 * name unknown: starts an ad-hoc authentication session for the game's communication ID:
 * LOCAL is this console's address, and the session's thread runs at PRIORITY with STACKSIZE
 */
__attribute__((noinline))
int SceNetCtl_1172C5FC(const SceNetInAddr *local, int priority, SceSize stackSize,
		       const NetCtlAdhocAuthParam *param, int maxPeers, int reserved,
		       const SceNpCommunicationId *commId)
{
	NetCtlAdhocAuthKeyParam keyParam;
	NetCtlAdhocAuthNotification notification;
	SceNetSockaddrIn addr;
	char name[16];
	char commIdName[16];
	char deleteName[16];
	int optval;
	int locked, s, ret;
	SceUID thread;

	if (g_adhocAuthInitialized <= 0)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
	if (g_adhocAuth != NULL)
		netCtlAdhocAuthAbort();
	g_adhocAuthSwVersion = SceLibKernel_9F793F84();
	if (g_adhocAuthSwVersion >= NETCTL_ADHOC_AUTH_POOL_VERSION) {
		g_adhocAuthPeerPool = SceNetInternal_A2C2BC5F(maxPeers * sizeof(NetCtlAdhocAuthPeer), 0);
		if (g_adhocAuthPeerPool == NULL) {
			ret = SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY;
			goto fail;
		}
		sceClibMemset(g_adhocAuthPeerPool, 0, maxPeers * sizeof(NetCtlAdhocAuthPeer));
		g_adhocAuthPeerPoolCount = maxPeers;
	}
	if (commId == NULL) {
		ret = SCE_NET_CTL_ERROR_COMMUNICATION_ID_NOT_EXIST;
		goto fail;
	}
	s = sceNetSocket("SceNetAdhocAuthComId", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	ret = s;
	if (ret < 0)
		goto fail;
	sceClibMemset(&keyParam, 0, sizeof(keyParam));
	keyParam.type = 3;
	ret = SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_CLEAR_KEYS, &keyParam);
	if (ret < 0) {
		sceNetSocketClose(s);
		goto fail;
	}
	sceClibMemset(name, 0, sizeof(name));
	sceClibSnprintf(name, sizeof(name) - 1, "%s_%d", commId->data, commId->num);
	ret = SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_SET_COMM_ID, name);
	if (ret < 0) {
		sceNetSocketClose(s);
		goto fail;
	}
	sceClibMemset(&keyParam, 0xaa, sizeof(keyParam));
	ret = SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_SET_KEY, &keyParam);
	if (ret < 0) {
		sceNetSocketClose(s);
		goto fail;
	}
	sceNetSocketClose(s);
	sceClibMemset(&notification, 0, sizeof(notification));
	sceClibMemset(commIdName, 0, sizeof(commIdName));
	sceClibSnprintf(commIdName, sizeof(commIdName) - 1, "%s_%02d", commId->data, commId->num);
	sceClibSnprintf(notification.text, sizeof(notification.text) - 1, "%s\nNpCommId=%s\n",
			"Adhoc:", commIdName);
	sceAppMgrSendNotificationRequest(&notification, 1);
	ret = netCtlAdhocAuthCalloutInit(0x2000, 0x10000100);
	if (ret < 0)
		goto fail;
	locked = netCtlAdhocAuthLock();
	g_adhocAuth = &g_adhocAuthContext;
	sceClibMemset(&g_adhocAuthContext, 0, sizeof(g_adhocAuthContext));
	g_adhocAuth->mode = 0;
	netCtlAdhocAuthPeerListInit(&g_adhocAuth->peers);
	ret = netCtlAdhocInfoPack(&g_adhocAuth->record);
	if (ret < 0)
		goto stop;
	g_adhocAuth->maxPeers = maxPeers;
	g_adhocAuth->beaconCount = 0;
	g_adhocAuth->beaconMessage.type = NETCTL_ADHOC_AUTH_MESSAGE_BEACON;
	g_adhocAuth->beaconInterval = param->beaconInterval;
	g_adhocAuth->resendInterval = param->resendInterval;
	g_adhocAuth->maxRetries = param->maxRetries;
	g_adhocAuth->keepaliveInterval = param->keepaliveInterval;
	g_adhocAuth->peerTimeout = param->peerTimeout;
	g_adhocAuth->swVersion = g_adhocAuthSwVersion;
	netCtlAdhocAuthBuildTlv(g_adhocAuth, g_adhocAuth->tlv, &g_adhocAuth->tlvSize);
	ret = sceNetSocket("SceNetAdhocAuthRecv", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	if (ret < 0)
		goto stop;
	g_adhocAuth->recvSocket = ret;
	optval = 100000;
	ret = sceNetSetsockopt(g_adhocAuth->recvSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
			       &optval, sizeof(optval));
	if (ret < 0)
		goto recv;
	optval = 1;
	ret = sceNetSetsockopt(g_adhocAuth->recvSocket, SCE_NET_SOL_SOCKET,
			       NETCTL_ADHOC_AUTH_SO_USEAUTH, &optval, sizeof(optval));
	if (ret < 0)
		goto recv;
	sceClibMemset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
	addr.sin_vport = sceNetHtons(NETCTL_ADHOC_AUTH_RECV_VPORT);
	ret = sceNetBind(g_adhocAuth->recvSocket, (const SceNetSockaddr *)&addr, sizeof(addr));
	if (ret < 0)
		goto recv;
	ret = sceNetSocket("SceNetAdhocAuthSend", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	if (ret < 0)
		goto recv;
	g_adhocAuth->sendSocket = ret;
	optval = 1;
	ret = sceNetSetsockopt(g_adhocAuth->sendSocket, SCE_NET_SOL_SOCKET,
			       NETCTL_ADHOC_AUTH_SO_USEAUTH, &optval, sizeof(optval));
	if (ret < 0)
		goto send;
	sceClibMemset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
	addr.sin_vport = sceNetHtons(NETCTL_ADHOC_AUTH_SEND_VPORT);
	ret = sceNetBind(g_adhocAuth->sendSocket, (const SceNetSockaddr *)&addr, sizeof(addr));
	if (ret < 0)
		goto send;
	netCtlAdhocAuthMbxCancel();
	g_adhocAuth->stop = 0;
	thread = sceKernelCreateThread("SceNetAdhocAuth", netCtlAdhocAuthThread, priority,
				       stackSize, 0, 0, NULL);
	ret = thread;
	if (ret < 0)
		goto send;
	g_adhocAuth->thread = thread;
	ret = sceKernelStartThread(g_adhocAuth->thread, sizeof(g_adhocAuth), &g_adhocAuth);
	if (ret < 0) {
		sceKernelDeleteThread(g_adhocAuth->thread);
		goto send;
	}
	sceClibMemcpy_safe(&g_adhocAuth->localAddress, local, sizeof(g_adhocAuth->localAddress));
	netCtlAdhocAuthSetMode(g_adhocAuth, 1);
	netCtlAdhocAuthCalloutRemove(&g_adhocAuth->beaconTimer, NULL);
	netCtlAdhocAuthCalloutAdd(&g_adhocAuth->beaconTimer, g_adhocAuth->beaconInterval,
				  netCtlAdhocAuthBeaconTimer, g_adhocAuth);
	netCtlAdhocAuthUnlock(locked);
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return 0;

send:
	sceNetSocketClose(g_adhocAuth->sendSocket);
recv:
	sceNetSocketClose(g_adhocAuth->recvSocket);
stop:
	netCtlAdhocAuthCalloutTerm();
	g_adhocAuth = NULL;
	netCtlAdhocAuthUnlock(locked);
fail:
	s = sceNetSocket("SceNetAdhocAuthDelComId", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	sceClibMemset(deleteName, 0, sizeof(deleteName));
	SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_DELETE_COMM_ID, deleteName);
	sceNetSocketClose(s);
	if (g_adhocAuthPeerPool != NULL) {
		SceNetInternal_E62A2FAA(g_adhocAuthPeerPool);
		g_adhocAuthPeerPool = NULL;
	}
	g_adhocAuthPeerPoolCount = 0;
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthDisconnect(void)
{
	int locked, ret = 0;

	if (g_adhocAuthInitialized <= 0)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
	if (g_adhocAuth != NULL) {
		g_adhocAuth->stop = 1;
		netCtlAdhocAuthMbxCancel();
		locked = netCtlAdhocAuthLock();
		netCtlAdhocAuthCalloutRemove(&g_adhocAuth->beaconTimer, NULL);
		netCtlAdhocAuthCalloutTerm();
		netCtlAdhocAuthUnlock(locked);
	} else {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	}
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthGetPeerList(SceSize *peerInfoNum, SceNetCtlAdhocPeerInfo *peerInfo)
{
	NetCtlAdhocAuthPeer *peer;
	SceNpId npId;
	SceUInt64 swVersion;
	char swVersionString[NETCTL_PEER_VERSION_STRING_SIZE];
	SceUInt32 appVersion;
	int locked, max, count = 0, ret = 0;

	if (peerInfoNum == NULL)
		return NETCTL_ERROR_ADHOC_AUTH_INVALID_POINTER;
	max = *peerInfoNum;
	if (g_adhocAuthInitialized <= 0) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
		return ret;
	}
	locked = netCtlAdhocAuthLock();
	if (locked < 0)
		return locked;
	if (g_adhocAuth == NULL) {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	} else {
		for (peer = g_adhocAuth->peers.next; peer != &g_adhocAuth->peers; peer = peer->next) {
			if (peer->state != NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED)
				continue;
			if (peerInfo != NULL && max > count) {
				sceClibMemcpy_safe(&peerInfo->inetAddr, &peer->addr.sin_addr,
						   sizeof(peerInfo->inetAddr));
				peerInfo->lastRecv = peer->lastReceive;
				netCtlAdhocInfoUnpack(&peer->record, (char *)&npId, &swVersion,
						      swVersionString, &appVersion);
				sceClibStrncpy(peerInfo->username, (char *)&npId, 16);
				if (npId.opt[0] != 0) {
					peerInfo->isValidNpId = 1;
					sceClibMemcpy_safe(&peerInfo->npId, &npId, sizeof(peerInfo->npId));
				} else {
					peerInfo->isValidNpId = 0;
					sceClibMemset(&peerInfo->npId, 0, sizeof(peerInfo->npId));
				}
				peerInfo->appVer = appVersion;
			}
			count++;
			peerInfo++;
		}
	}
	*peerInfoNum = count;
	netCtlAdhocAuthUnlock(locked);
	return ret;
}
