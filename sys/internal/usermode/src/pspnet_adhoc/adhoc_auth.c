/*
 * adhoc_auth.c: ad-hoc Ethernet frame encryption/decryption, authentication session lifecycle,
 * peer pool, and peer query APIs for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

int g_adhocAuthPeerPoolCount = 0;
AdhocAuthPeer *g_adhocAuthPeerPool = NULL;
AdhocAuth *g_adhocAuth = NULL;
SceKernelLwMutexWork *g_adhocAuthStopLock = NULL;

AdhocAuth g_adhocAuthContext;

static inline __attribute__((always_inline)) void adhocAuthPeerListInit(AdhocAuthPeer *head)
{
	head->next = head;
	head->prev = head;
}

static inline __attribute__((always_inline)) void adhocAuthStopThread(void)
{
	int locked;

	sceNetSocketAbort(g_adhocAuth->sendSocket, SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION);
	g_adhocAuth->stop = 1;
	adhocAuthMbxCancel();
	sceKernelWaitThreadEnd(g_adhocAuth->thread, NULL, NULL);
	sceKernelDeleteThread(g_adhocAuth->thread);
	locked = adhocAuthLock();
	adhocAuthCalloutRemove(&g_adhocAuth->beaconTimer, NULL);
	adhocAuthCalloutTerm();
	g_adhocAuth = NULL;
	adhocAuthUnlock(locked);
}

static inline __attribute__((always_inline)) void adhocAuthAbort(void)
{
	if (g_adhocctlInitialized > 0) {
		sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
		if (g_adhocAuth != NULL)
			adhocAuthStopThread();
		sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	}
}

__attribute__((noinline))
int adhocAuthEncryptFrame(AdhocMbuf *m)
{
	char *frame;
	AdhocAuthKeyEntry *entry;

	frame = m->data;
	if (frame == NULL)
		return -1;
	if (m->flags & PSPNET_ADHOC_M_BCAST) {
		if (adhocAuthGcSignBroadcast(frame + PSPNET_ADHOC_FRAME_HDR_LEN,
					     m->storage.mh.pktHdr.len - PSPNET_ADHOC_FRAME_HDR_LEN,
					     frame + PSPNET_ADHOC_ETHER_HDR_LEN, 1) >= 0)
			return 0;
	} else if ((m->flags & PSPNET_ADHOC_M_MCAST) == 0) {
		sceKernelWaitSema(g_adhocAuthKeySema, 1, NULL);
		entry = adhocAuthKeyFind((const SceUChar8 *)frame);
		if (entry != NULL &&
		    adhocAuthGcSign(entry->context, frame + PSPNET_ADHOC_FRAME_HDR_LEN,
				    m->storage.mh.pktHdr.len - PSPNET_ADHOC_FRAME_HDR_LEN,
				    frame + PSPNET_ADHOC_ETHER_HDR_LEN, 1) >= 0) {
			sceKernelSignalSema(g_adhocAuthKeySema, 1);
			return 0;
		}
		sceKernelSignalSema(g_adhocAuthKeySema, 1);
	}
	return -1;
}

__attribute__((noinline))
int adhocAuthDecryptFrame(AdhocMbuf *m)
{
	char *tempBuf = NULL;
	char *frame;
	char *encBuf;
	AdhocAuthKeyEntry *entry;
	SceUChar8 bcast[6];
	int ok = 0;

	sceClibMemset(bcast, 0xff, sizeof(bcast));
	frame = m->data;
	if (frame == NULL)
		return -1;
	if (m->storage.mh.pktHdr.len == m->len) {
		encBuf = frame + PSPNET_ADHOC_ETHER_HDR_LEN;
	} else {
		tempBuf = (char *)adhocMalloc((SceSize)(m->storage.mh.pktHdr.len -
							PSPNET_ADHOC_ETHER_HDR_LEN));
		if (tempBuf == NULL)
			return -1;
		adhocMbufCopyData(m, PSPNET_ADHOC_ETHER_HDR_LEN,
				  m->storage.mh.pktHdr.len - PSPNET_ADHOC_ETHER_HDR_LEN, tempBuf);
		encBuf = tempBuf;
	}
	sceKernelWaitSema(g_adhocAuthKeySema, 1, NULL);
	entry = adhocAuthKeyFind((const SceUChar8 *)(frame + 6));
	if (entry != NULL) {
		if (sceClibMemcmp(bcast, frame, sizeof(bcast)) == 0) {
			if (adhocAuthGcDecryptBroadcast(entry->context,
							encBuf + PSPNET_ADHOC_AUTH_MAC_LEN,
							m->storage.mh.pktHdr.len -
								PSPNET_ADHOC_FRAME_HDR_LEN,
							encBuf, 1, 1) >= 0)
				ok = 1;
		} else if ((m->flags & PSPNET_ADHOC_M_MCAST) == 0) {
			if (adhocAuthGcDecrypt(entry->context,
					       encBuf + PSPNET_ADHOC_AUTH_MAC_LEN,
					       m->storage.mh.pktHdr.len -
						       PSPNET_ADHOC_FRAME_HDR_LEN,
					       encBuf, 1, 1) >= 0)
				ok = 1;
		}
	}
	if (ok) {
		if (tempBuf != NULL) {
			adhocMbufCopyBack(m, PSPNET_ADHOC_FRAME_HDR_LEN,
					  m->storage.mh.pktHdr.len - PSPNET_ADHOC_FRAME_HDR_LEN,
					  encBuf);
			adhocFree(tempBuf);
		}
		entry->lastReceive = sceKernelGetProcessTimeWide();
		sceKernelSignalSema(g_adhocAuthKeySema, 1);
		return 0;
	}
	sceKernelSignalSema(g_adhocAuthKeySema, 1);
	if (tempBuf != NULL)
		adhocFree(tempBuf);
	return -1;
}

__attribute__((noinline))
AdhocAuthPeer *adhocAuthPeerAlloc(void)
{
	int locked, i;

	locked = adhocAuthLock();
	for (i = 0; i < g_adhocAuthPeerPoolCount; i++) {
		if (g_adhocAuthPeerPool[i].used == 0) {
			sceClibMemset(&g_adhocAuthPeerPool[i], 0, sizeof(AdhocAuthPeer));
			g_adhocAuthPeerPool[i].used = 1;
			return &g_adhocAuthPeerPool[i];
		}
	}
	adhocAuthUnlock(locked);
	return NULL;
}

__attribute__((noinline))
void adhocAuthPeerFree(AdhocAuthPeer *peer)
{
	int locked, i;

	if (peer == NULL)
		return;
	locked = adhocAuthLock();
	for (i = 0; i < g_adhocAuthPeerPoolCount; i++) {
		if (&g_adhocAuthPeerPool[i] == peer)
			sceClibMemset(peer, 0, sizeof(AdhocAuthPeer));
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthPeerPoolTerm(void)
{
	if (g_adhocAuthPeerPool != NULL) {
		adhocFree(g_adhocAuthPeerPool);
		g_adhocAuthPeerPool = NULL;
	}
	g_adhocAuthPeerPoolCount = 0;
}

__attribute__((noinline))
int adhocAuthStopLockInit(void)
{
	int ret;

	ret = adhocAuthLockInit();
	if (ret < 0)
		return ret;
	g_adhocAuthStopLock = (SceKernelLwMutexWork *)SceNetInternal_A2C2BC5F(
		sizeof(SceKernelLwMutexWork), 0);
	if (g_adhocAuthStopLock == NULL)
		return SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY;
	ret = sceKernelCreateLwMutex(g_adhocAuthStopLock, "SceNetAdhocAuthStopLock",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	if (ret < 0) {
		SceNetInternal_E62A2FAA(g_adhocAuthStopLock);
		g_adhocAuthStopLock = NULL;
		return ret;
	}
	return 0;
}

__attribute__((noinline))
int ScePspnetAdhoc_8938A4D0(void)
{
	int ret = 0;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
	if (g_adhocAuth != NULL)
		adhocAuthStopThread();
	else
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return ret;
}

__attribute__((noinline))
void adhocAuthStopLockTerm(void)
{
	adhocAuthAbort();
	if (g_adhocAuthStopLock != NULL) {
		sceKernelDeleteLwMutex(g_adhocAuthStopLock);
		SceNetInternal_E62A2FAA(g_adhocAuthStopLock);
		g_adhocAuthStopLock = NULL;
	}
	adhocAuthLockTerm();
}

__attribute__((noinline))
int ScePspnetAdhoc_A3EC45B9(int priority, SceSize stackSize, const AdhocAuthParam *param,
			    int maxPeers, const SceNetAdhocctlNickname *nickname)
{
	int optval;
	int locked, ret;
	SceUID thread;
	SceNetAdhocctlAdhocId adhocId;
	AdhocAuthNotification notification;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	sceKernelLockLwMutex(g_adhocAuthStopLock, 1, NULL);
	if (g_adhocAuth != NULL)
		adhocAuthAbort();
	g_adhocAuthPeerPool = (AdhocAuthPeer *)adhocMalloc((SceSize)maxPeers *
							   sizeof(AdhocAuthPeer));
	if (g_adhocAuthPeerPool == NULL) {
		ret = SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY;
	} else {
		sceClibMemset(g_adhocAuthPeerPool, 0, (SceSize)maxPeers * sizeof(AdhocAuthPeer));
		g_adhocAuthPeerPoolCount = maxPeers;
		ret = adhocAuthCalloutInit(0x2000, 0x10000100);
		if (ret >= 0) {
			locked = adhocAuthLock();
			g_adhocAuth = &g_adhocAuthContext;
			sceClibMemset(&g_adhocAuthContext, 0, sizeof(g_adhocAuthContext));
			g_adhocAuth->mode = 0;
			adhocAuthPeerListInit(&g_adhocAuth->peers);
			sceNetAdhocctlGetEtherAddr(&g_adhocAuth->localAddr);
			sceClibMemcpy_safe(&g_adhocAuth->nickname, nickname,
					   sizeof(g_adhocAuth->nickname));
			g_adhocAuth->maxPeers = maxPeers;
			g_adhocAuth->beaconCount = 0;
			g_adhocAuth->beaconMessage.type = PSPNET_ADHOC_AUTH_MSG_BEACON;
			g_adhocAuth->beaconInterval = param->beaconInterval;
			g_adhocAuth->resendInterval = param->resendInterval;
			g_adhocAuth->maxRetries = param->maxRetries;
			g_adhocAuth->keepaliveInterval = param->keepaliveInterval;
			g_adhocAuth->peerTimeout = param->peerTimeout;
			g_adhocAuth->applicationId = 0;
			g_adhocAuth->swVersion = SceLibKernel_9F793F84();
			g_adhocAuth->option = 0x100;
			adhocAuthBuildTlv(g_adhocAuth, g_adhocAuth->tlv, &g_adhocAuth->tlvSize);
			ret = sceNetSocket("ScePspnetAdhocAuthRecv", SCE_NET_AF_INET,
					   PSPNET_ADHOC_SOCK_RAW_ETHER, PSPNET_ADHOC_ETHER_PROTO);
			if (ret >= 0) {
				g_adhocAuth->recvSocket = ret;
				optval = 100000;
				ret = sceNetSetsockopt(g_adhocAuth->recvSocket, SCE_NET_SOL_SOCKET,
						       SCE_NET_SO_RCVTIMEO, &optval,
						       sizeof(optval));
				if (ret >= 0) {
					ret = sceNetSocket("SceNetAdhocAuthSend", SCE_NET_AF_INET,
							   PSPNET_ADHOC_SOCK_RAW_ETHER,
							   PSPNET_ADHOC_ETHER_PROTO);
					if (ret >= 0) {
						g_adhocAuth->sendSocket = ret;
						adhocAuthMbxCancel();
						g_adhocAuth->stop = 0;
						thread = sceKernelCreateThread(
							"SceNetAdhocAuth", adhocAuthThread,
							priority, stackSize, 0, 0, NULL);
						ret = thread;
						if (ret >= 0) {
							g_adhocAuth->thread = thread;
							ret = sceKernelStartThread(
								g_adhocAuth->thread,
								sizeof(g_adhocAuth), &g_adhocAuth);
							if (ret >= 0) {
								adhocAuthSetMode(g_adhocAuth, 1);
								adhocAuthCalloutRemove(
									&g_adhocAuth->beaconTimer,
									NULL);
								adhocAuthCalloutAdd(
									&g_adhocAuth->beaconTimer,
									g_adhocAuth->beaconInterval,
									adhocAuthBeaconTimer,
									g_adhocAuth);
								sceClibMemset(&notification, 0,
									      sizeof(notification));
								adhocctlGetAdhocIdCopy(&adhocId);
								adhocId.padding[0] = 0;
								sceClibSnprintf(
									notification.text,
									sizeof(notification.text) - 1,
									"%s\nAdhocId=%s\n",
									"PSPNETAdhoc:",
									adhocId.data);
								sceAppMgrSendNotificationRequest(
									&notification, 1);
								adhocAuthUnlock(locked);
								sceKernelUnlockLwMutex(
									g_adhocAuthStopLock, 1);
								return 0;
							}
							sceKernelDeleteThread(g_adhocAuth->thread);
						}
						sceNetSocketClose(g_adhocAuth->sendSocket);
					}
				}
				sceNetSocketClose(g_adhocAuth->recvSocket);
			}
			adhocAuthCalloutTerm();
			g_adhocAuth = NULL;
			adhocAuthUnlock(locked);
		}
	}
	if (g_adhocAuthPeerPool != NULL) {
		adhocFree(g_adhocAuthPeerPool);
		g_adhocAuthPeerPool = NULL;
	}
	g_adhocAuthPeerPoolCount = 0;
	sceKernelUnlockLwMutex(g_adhocAuthStopLock, 1);
	return ret;
}

__attribute__((noinline))
int adhocAuthGetPeerList(int *buflen, void *buf)
{
	AdhocAuthPeer *peer;
	SceNetAdhocctlPeerInfo *cur = (SceNetAdhocctlPeerInfo *)buf;
	SceNetAdhocctlPeerInfo *prev = NULL;
	int locked, total = 0, ret = 0;

	if (buflen == NULL)
		return SCE_ERROR_NET_ADHOCCTL_INVALID_ARG;
	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	locked = adhocAuthLock();
	if (locked < 0)
		return locked;
	if (g_adhocAuth == NULL) {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	} else {
		for (peer = g_adhocAuth->peers.next; peer != &g_adhocAuth->peers; peer = peer->next) {
			if (peer->state != PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED)
				continue;
			if (buf != NULL) {
				if ((unsigned int)(total + (int)sizeof(SceNetAdhocctlPeerInfo)) >
				    (unsigned int)*buflen)
					break;
				cur->next = NULL;
				sceClibMemcpy_safe(&cur->macAddr, &peer->key.addr,
						   sizeof(cur->macAddr));
				sceClibMemcpy_safe(&cur->nickname, &peer->nickname,
						   sizeof(cur->nickname));
				cur->lastRecv = peer->key.lastReceive;
				if (prev != NULL)
					prev->next = cur;
				prev = cur;
			}
			cur++;
			total += (int)sizeof(SceNetAdhocctlPeerInfo);
		}
	}
	adhocAuthUnlock(locked);
	*buflen = total;
	return ret;
}

__attribute__((noinline))
int adhocAuthGetPeerInfo(const SceNetEtherAddr *addr, SceNetAdhocctlPeerInfo *peerInfo)
{
	AdhocAuthPeer *peer;
	int locked, ret = 0;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	locked = adhocAuthLock();
	if (locked < 0)
		return locked;
	if (g_adhocAuth == NULL) {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	} else {
		ret = SCE_ERROR_NET_ADHOC_NO_ENTRY;
		for (peer = g_adhocAuth->peers.next; peer != &g_adhocAuth->peers; peer = peer->next) {
			if (peer->state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED &&
			    sceClibMemcmp(&peer->addr, addr, sizeof(*addr)) == 0) {
				peerInfo->next = NULL;
				sceClibMemcpy_safe(&peerInfo->macAddr, &peer->key.addr,
						   sizeof(peerInfo->macAddr));
				sceClibMemcpy_safe(&peerInfo->nickname, &peer->nickname,
						   sizeof(peerInfo->nickname));
				peerInfo->lastRecv = peer->key.lastReceive;
				ret = 0;
				break;
			}
		}
	}
	adhocAuthUnlock(locked);
	return ret;
}

__attribute__((noinline))
int adhocAuthGetAddrByName(const SceNetAdhocctlNickname *nickname, int *buflen, void *buf)
{
	AdhocAuthPeer *peer;
	SceNetAdhocctlPeerInfo *cur = (SceNetAdhocctlPeerInfo *)buf;
	SceNetAdhocctlPeerInfo *prev = NULL;
	int locked, count = 0, total = 0, ret = 0;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	locked = adhocAuthLock();
	if (locked < 0)
		return locked;
	if (g_adhocAuth == NULL) {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	} else {
		for (peer = g_adhocAuth->peers.next; peer != &g_adhocAuth->peers; peer = peer->next) {
			if (peer->state != PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED)
				continue;
			if (sceClibMemcmp(&peer->nickname, nickname, sizeof(*nickname)) != 0)
				continue;
			count++;
			if (buf != NULL) {
				if ((unsigned int)(total + (int)sizeof(SceNetAdhocctlPeerInfo)) >
				    (unsigned int)*buflen)
					break;
				cur->next = NULL;
				sceClibMemcpy_safe(&cur->macAddr, &peer->key.addr,
						   sizeof(cur->macAddr));
				sceClibMemcpy_safe(&cur->nickname, &peer->nickname,
						   sizeof(cur->nickname));
				if (prev != NULL)
					prev->next = cur;
				prev = cur;
			}
			cur++;
			total += (int)sizeof(SceNetAdhocctlPeerInfo);
		}
	}
	adhocAuthUnlock(locked);
	*buflen = total;
	if (count == 0)
		return SCE_ERROR_NET_ADHOC_NO_ENTRY;
	return ret;
}

__attribute__((noinline))
int adhocAuthGetNameByAddr(const SceNetEtherAddr *addr, SceNetAdhocctlNickname *nickname)
{
	AdhocAuthPeer *peer;
	int locked, ret = 0;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	locked = adhocAuthLock();
	if (locked < 0)
		return locked;
	if (g_adhocAuth == NULL) {
		ret = SCE_NET_CTL_ERROR_NOT_CONNECTED;
	} else {
		ret = SCE_ERROR_NET_ADHOC_NO_ENTRY;
		for (peer = g_adhocAuth->peers.next; peer != &g_adhocAuth->peers; peer = peer->next) {
			if (peer->state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED &&
			    sceClibMemcmp(&peer->addr, addr, sizeof(*addr)) == 0) {
				sceClibMemcpy_safe(nickname, &peer->nickname, sizeof(*nickname));
				ret = 0;
				break;
			}
		}
	}
	adhocAuthUnlock(locked);
	return ret;
}

__attribute__((noinline))
int adhocAuthGetParameter(SceNetAdhocctlParameter *parameter)
{
	SceNetCtlInfo info;
	int ret;

	if (g_adhocctlInitialized <= 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	if (g_adhocAuth == NULL)
		return SCE_NET_CTL_ERROR_NOT_CONNECTED;
	ret = SceNetCtl_CDEBA500(SCE_NET_CTL_INFO_BSSID, &info);
	if (ret < 0)
		return ret;
	sceClibMemcpy_safe(&parameter->bssid, &info.bssid, SCE_NET_ADHOCCTL_BSSID_LEN);
	ret = SceNetCtl_CDEBA500(SCE_NET_CTL_INFO_CHANNEL, &info);
	if (ret < 0)
		return ret;
	parameter->channel = (int)info.channel;
	adhocctlGetGroupNameCopy(&parameter->groupName);
	sceClibMemcpy_safe(&parameter->nickname, &g_adhocAuth->nickname,
			   sizeof(parameter->nickname));
	return 0;
}

__attribute__((noinline))
int adhocAuthGetLocalAddr(SceNetEtherAddr *addr)
{
	sceClibMemcpy_safe(addr, &g_adhocAuth->localAddr, sizeof(*addr));
	return 0;
}
