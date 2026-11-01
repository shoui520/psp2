/*
 * SceNetCtl ad-hoc authentication, the peers: their states, the session lock, the TLVs, the
 * packets sent and received, the timers' handlers and the game card authentication requests.
 */

#include "adhoc_auth.h"

static const SceUShort16 g_adhocAuthPayloadSizes[] = {
	0, 0, 0x100, 0x1d0, 0x170, 0x130, 0x60, 0x20, 0x30,
};

SceUID g_adhocAuthLockSema = -1;
SceUID g_adhocAuthLockOwner = -1;

SceUChar8 g_adhocAuthSendBuffer[NETCTL_ADHOC_AUTH_PACKET_SIZE];
SceUChar8 g_adhocAuthRecvBuffer[NETCTL_ADHOC_AUTH_PACKET_SIZE];

__attribute__((noinline))
void netCtlAdhocAuthPeerAdd(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer, SceNetSockaddrIn addr,
			    const NetCtlAdhocPeerRecord *record)
{
	sceClibMemcpy_safe(&peer->addr, &addr, sizeof(peer->addr));
	sceClibMemcpy_safe(&peer->record, record, sizeof(peer->record));
	peer->auth = auth;
	peer->next = &auth->peers;
	peer->prev = auth->peers.prev;
	peer->state = NETCTL_ADHOC_AUTH_PEER_NEW;
	auth->peers.prev = peer;
	peer->prev->next = peer;
	auth->peerCount++;
}

__attribute__((noinline))
NetCtlAdhocAuthPeer *netCtlAdhocAuthPeerFind(NetCtlAdhocAuth *auth, SceNetInAddr addr)
{
	NetCtlAdhocAuthPeer *peer;

	for (peer = auth->peers.next; peer != &auth->peers; peer = peer->next) {
		if (sceClibMemcmp(&peer->addr.sin_addr, &addr, sizeof(addr)) == 0)
			return peer;
	}
	return NULL;
}

__attribute__((noinline))
void netCtlAdhocAuthPeerRemove(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer)
{
	if (peer == NULL)
		return;
	netCtlAdhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
	netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
	netCtlAdhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
	peer->next->prev = peer->prev;
	peer->prev->next = peer->next;
	peer->next = peer;
	peer->prev = peer;
	auth->peerCount--;
	netCtlAdhocAuthPeerFree(peer);
}

/* installs a peer's keys in the network stack when it becomes authenticated, removes them after */
__attribute__((noinline))
void netCtlAdhocAuthPeerSetState(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer, int state)
{
	SceUChar8 key[16], sendKey[16], receiveKey[16];
	NetCtlAdhocAuthKeyParam param;
	int s;

	sceKernelWaitSema(g_adhocAuthKeySema, 1, NULL);
	if (peer->state != NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED &&
	    state == NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED) {
		peer->lastReceive = sceKernelGetProcessTimeWide();
		if (SceLibKernel_AC57B6A4(peer->context, key, sendKey, receiveKey) >= 0) {
			s = sceNetSocket("SceNetAdhocAuth", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
			if (s >= 0) {
				sceClibMemset(&param, 0, sizeof(param));
				sceClibMemcpy_safe(param.key, key, sizeof(param.key));
				if (SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_SET_KEY,
							    &param) >= 0) {
					sceClibMemset(&param, 0, sizeof(param));
					sceClibMemcpy_safe(param.key, key, sizeof(param.key));
					if (SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_SET_SIGN_KEY,
								    &param) >= 0) {
						sceClibMemset(&param, 0, sizeof(param));
						param.addr = peer->addr.sin_addr;
						param.port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
						sceClibMemcpy_safe(param.key, sendKey, sizeof(param.key));
						param.type = 3;
						param.direction = 0;
						if (SceNetInternal_333F9CB3(s,
								NETCTL_ADHOC_AUTH_IOCTL_ADD_PEER_KEY,
								&param) >= 0) {
							sceClibMemset(&param, 0, sizeof(param));
							param.addr = peer->addr.sin_addr;
							param.port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
							sceClibMemcpy_safe(param.key, sendKey,
									   sizeof(param.key));
							param.direction = 1;
							param.type = 3;
							SceNetInternal_333F9CB3(s,
								NETCTL_ADHOC_AUTH_IOCTL_ADD_PEER_KEY,
								&param);
						}
					}
				}
				sceNetSocketClose(s);
			}
		}
		sceClibMemset(key, 0, sizeof(key));
		sceClibMemset(sendKey, 0, sizeof(sendKey));
		sceClibMemset(receiveKey, 0, sizeof(receiveKey));
		sceClibMemset(&param, 0, sizeof(param));
	}
	if (peer->state == NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED &&
	    state != NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED) {
		if (SceLibKernel_AC57B6A4(peer->context, key, sendKey, receiveKey) >= 0) {
			s = sceNetSocket("SceNetAdhocAuth", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
			if (s >= 0) {
				sceClibMemset(&param, 0, sizeof(param));
				param.addr = peer->addr.sin_addr;
				param.port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
				sceClibMemcpy_safe(param.key, sendKey, sizeof(param.key));
				param.direction = 0;
				param.type = 3;
				if (SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_DELETE_PEER_KEY,
							    &param) >= 0) {
					sceClibMemset(&param, 0, sizeof(param));
					param.addr = peer->addr.sin_addr;
					param.port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
					sceClibMemcpy_safe(param.key, sendKey, sizeof(param.key));
					param.direction = 1;
					param.type = 3;
					SceNetInternal_333F9CB3(s, NETCTL_ADHOC_AUTH_IOCTL_DELETE_PEER_KEY,
								&param);
				}
				sceNetSocketClose(s);
			}
		}
		sceClibMemset(key, 0, sizeof(key));
		sceClibMemset(sendKey, 0, sizeof(sendKey));
		sceClibMemset(receiveKey, 0, sizeof(receiveKey));
		sceClibMemset(&param, 0, sizeof(param));
	}
	sceKernelSignalSema(g_adhocAuthKeySema, 1);
	peer->state = state;
}

__attribute__((noinline))
void netCtlAdhocAuthSetMode(NetCtlAdhocAuth *auth, int mode)
{
	auth->mode = mode;
}

__attribute__((noinline))
int netCtlAdhocAuthLockInit(void)
{
	g_adhocAuthLockSema = sceKernelCreateSema("SceNetAdhocAuthLock", 0, 1, 1, NULL);
	if (g_adhocAuthLockSema < 0)
		return g_adhocAuthLockSema;
	g_adhocAuthLockOwner = -1;
	return 0;
}

__attribute__((noinline))
void netCtlAdhocAuthLockTerm(void)
{
	if (g_adhocAuthLockSema >= 0)
		sceKernelDeleteSema(g_adhocAuthLockSema);
	g_adhocAuthLockSema = -1;
}

/* takes the lock unless this thread holds it: 1 when taken, 0 when already held */
__attribute__((noinline))
int netCtlAdhocAuthLock(void)
{
	SceUID thread;
	int ret;

	thread = sceKernelGetThreadId();
	if (thread < 0)
		return thread;
	if (g_adhocAuthLockOwner == thread)
		return 0;
	ret = sceKernelWaitSema(g_adhocAuthLockSema, 1, NULL);
	if (ret < 0)
		return ret;
	g_adhocAuthLockOwner = thread;
	return 1;
}

__attribute__((noinline))
void netCtlAdhocAuthUnlock(int locked)
{
	if (locked == 0)
		return;
	g_adhocAuthLockOwner = -1;
	sceKernelSignalSema(g_adhocAuthLockSema, 1);
}

/* the TLVs a beacon carries: application ID, system software version, option */
__attribute__((noinline))
int netCtlAdhocAuthBuildTlv(NetCtlAdhocAuth *auth, SceUChar8 *tlv, int *size)
{
	tlv[0] = 1;
	tlv[1] = 4;
	if (auth->applicationId != NULL)
		sceClibMemcpy_safe(&tlv[2], auth->applicationId, 4);
	tlv[6] = 2;
	tlv[7] = 4;
	if (&auth->swVersion != NULL)
		sceClibMemcpy_safe(&tlv[8], &auth->swVersion, 4);
	tlv[12] = 3;
	tlv[13] = 4;
	if (auth->option != NULL)
		sceClibMemcpy_safe(&tlv[14], auth->option, 4);
	*size = 18;
	return 18;
}

__attribute__((noinline))
void netCtlAdhocAuthParseTlv(NetCtlAdhocAuthPeer *peer, const char *tlv, int size)
{
	int offset = 0;
	SceUChar8 type, length;
	const char *value;

	while (offset < size) {
		if (size - offset < 2)
			break;
		type = tlv[offset];
		length = tlv[offset + 1];
		if (size - offset < length + 2)
			break;
		value = &tlv[offset + 2];
		switch (type) {
		case 1:
			if (length == 4)
				sceClibMemcpy_safe(peer->applicationId, value, length);
			break;
		case 2:
			if (length == 4)
				sceClibMemcpy_safe(peer->swVersion, value, length);
			break;
		case 3:
			if (length == 4)
				sceClibMemcpy_safe(peer->option, value, length);
			break;
		}
		offset += length + 2;
	}
}

__attribute__((noinline))
int netCtlAdhocAuthSend(NetCtlAdhocAuth *auth, SceNetSockaddrIn to, SceUChar8 *packet,
			SceUChar8 type, const void *payload, const void *extra, int extraSize)
{
	SceUShort16 length;
	SceSize size;

	length = g_adhocAuthPayloadSizes[type];
	if (length + extraSize + NETCTL_ADHOC_AUTH_HEADER_SIZE > NETCTL_ADHOC_AUTH_PACKET_SIZE)
		return NETCTL_ERROR_ADHOC_AUTH_FAILED;
	if (payload != NULL)
		sceClibMemcpy_safe(&packet[NETCTL_ADHOC_AUTH_HEADER_SIZE], payload, length);
	if (extra != NULL)
		sceClibMemcpy_safe(&packet[length + NETCTL_ADHOC_AUTH_HEADER_SIZE], extra, extraSize);
	packet[1] = type;
	packet[0] = NETCTL_ADHOC_AUTH_VERSION;
	size = (SceUShort16)(length + extraSize + NETCTL_ADHOC_AUTH_HEADER_SIZE);
	length = sceNetHtons(length);
	sceClibMemcpy_safe(&packet[2], &length, sizeof(length));
	to.sin_port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
	to.sin_vport = sceNetHtons(NETCTL_ADHOC_AUTH_RECV_VPORT);
	return sceNetSendto(auth->sendSocket, packet, size, 0, (const SceNetSockaddr *)&to,
			    sizeof(to));
}

/* sends one peer a packet: its address is read before the socket is set to unicast */
static inline __attribute__((always_inline)) int netCtlAdhocAuthSendPeer(NetCtlAdhocAuth *auth,
		SceNetSockaddrIn to, SceUChar8 type, const void *payload, const void *extra,
		int extraSize)
{
	int optval = 0;

	sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
			 sizeof(optval));
	return netCtlAdhocAuthSend(auth, to, g_adhocAuthSendBuffer, type, payload, extra,
				   extraSize);
}

/* broadcasts a packet to every console on the authentication port */
static inline __attribute__((always_inline)) int netCtlAdhocAuthSendBroadcast(NetCtlAdhocAuth *auth,
		SceUChar8 type, const void *payload, const void *extra, int extraSize)
{
	SceNetSockaddrIn to;
	int optval;

	sceClibMemset(&to, 0, sizeof(to));
	to.sin_len = sizeof(to);
	to.sin_family = SCE_NET_AF_INET;
	to.sin_addr.s_addr = 0xffffffff;
	to.sin_port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
	to.sin_vport = sceNetHtons(NETCTL_ADHOC_AUTH_RECV_VPORT);
	optval = 1;
	sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
			 sizeof(optval));
	return netCtlAdhocAuthSend(auth, to, g_adhocAuthSendBuffer, type, payload, extra,
				   extraSize);
}

/* sends a peer the last packet again, or gives the peer up after the last retry */
__attribute__((noinline))
void netCtlAdhocAuthResend(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer)
{
	int locked;

	locked = netCtlAdhocAuthLock();
	if (peer == NULL || (peer->state != NETCTL_ADHOC_AUTH_PEER_REQUESTED &&
			     peer->state != NETCTL_ADHOC_AUTH_PEER_CONFIRMED &&
			     peer->state != NETCTL_ADHOC_AUTH_PEER_RESPONDED))
		goto out;
	if (peer->retries >= auth->maxRetries || auth->mode == 2) {
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
		netCtlAdhocAuthPeerRemove(auth, peer);
		netCtlAdhocAuthUnlock(locked);
		return;
	}
	if (peer->state == NETCTL_ADHOC_AUTH_PEER_REQUESTED)
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_REQUEST,
					peer->request, auth->tlv, auth->tlvSize);
	else if (peer->state == NETCTL_ADHOC_AUTH_PEER_CONFIRMED)
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_CONFIRM,
					peer->confirm, NULL, 0);
	peer->retries++;
	netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
	netCtlAdhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
				  netCtlAdhocAuthResendTimer, peer);
out:
	netCtlAdhocAuthUnlock(locked);
}

/* broadcasts this console's record while peers may join, an empty beacon when they are full */
__attribute__((noinline))
void netCtlAdhocAuthBeacon(NetCtlAdhocAuth *auth)
{
	int locked;

	locked = netCtlAdhocAuthLock();
	if (auth->mode == 1) {
		if (auth->peerCount < auth->maxPeers)
			netCtlAdhocAuthSendBroadcast(auth, NETCTL_ADHOC_AUTH_PACKET_HELLO, &auth->record,
						     auth->tlv, auth->tlvSize);
		else
			netCtlAdhocAuthSendBroadcast(auth, NETCTL_ADHOC_AUTH_PACKET_BEACON, NULL, NULL, 0);
		netCtlAdhocAuthCalloutRemove(&auth->beaconTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&auth->beaconTimer, auth->beaconInterval,
					  netCtlAdhocAuthBeaconTimer, auth);
	}
	netCtlAdhocAuthUnlock(locked);
}

/* sends an authenticated peer the next keepalive */
__attribute__((noinline))
void netCtlAdhocAuthKeepalive(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer)
{
	SceUInt32 sequence;
	SceUChar8 payload[16];
	int locked;

	locked = netCtlAdhocAuthLock();
	if (peer != NULL && peer->state == NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED && auth->mode != 2) {
		peer->keepaliveSent++;
		sequence = sceNetHtonl(peer->keepaliveSent);
		sceClibMemset(payload, 0, sizeof(payload));
		sceClibMemcpy_safe(payload, &sequence, sizeof(sequence));
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_KEEPALIVE,
					payload, NULL, 0);
		netCtlAdhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
					  netCtlAdhocAuthKeepaliveTimer, peer);
	}
	netCtlAdhocAuthUnlock(locked);
}

/* says goodbye to a peer that stopped answering and forgets it */
__attribute__((noinline))
void netCtlAdhocAuthTimeout(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer)
{
	int locked;

	locked = netCtlAdhocAuthLock();
	if (peer != NULL && auth->mode != 2) {
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_BYE,
					NULL, NULL, 0);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
		netCtlAdhocAuthPeerRemove(auth, peer);
	}
	netCtlAdhocAuthUnlock(locked);
}

/* says goodbye to every peer and forgets them */
__attribute__((noinline))
void netCtlAdhocAuthShutdown(NetCtlAdhocAuth *auth)
{
	SceNetSockaddrIn to;
	NetCtlAdhocAuthPeer *peer;
	int locked, optval;

	locked = netCtlAdhocAuthLock();
	sceClibMemset(&to, 0, sizeof(to));
	to.sin_len = sizeof(to);
	to.sin_family = SCE_NET_AF_INET;
	to.sin_addr.s_addr = 0xffffffff;
	to.sin_port = sceNetHtons(NETCTL_ADHOC_AUTH_PORT);
	to.sin_vport = sceNetHtons(NETCTL_ADHOC_AUTH_RECV_VPORT);
	optval = 1;
	sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
			 sizeof(optval));
	netCtlAdhocAuthSend(auth, to, g_adhocAuthSendBuffer, NETCTL_ADHOC_AUTH_PACKET_BYE, NULL,
			    NULL, 0);
	for (peer = auth->peers.next; peer != &auth->peers; peer = auth->peers.next) {
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
		netCtlAdhocAuthPeerRemove(auth, peer);
	}
	netCtlAdhocAuthUnlock(locked);
}

/* one packet from a peer */
__attribute__((noinline))
void netCtlAdhocAuthInput(NetCtlAdhocAuth *auth, const SceUChar8 *packet, int size,
			  SceNetSockaddrIn from)
{
	SceUShort16 length;
	NetCtlAdhocAuthPeer *peer;
	const char *tlv;
	int tlvSize, locked;

	if (auth->ignoredAddress.s_addr == from.sin_addr.s_addr)
		return;
	if (packet[0] != NETCTL_ADHOC_AUTH_VERSION)
		return;
	if (packet[1] == 0 || packet[1] > NETCTL_ADHOC_AUTH_PACKET_KEEPALIVE)
		return;
	sceClibMemcpy_safe(&length, &packet[2], sizeof(length));
	length = sceNetNtohs(length);
	if (length != g_adhocAuthPayloadSizes[packet[1]] ||
	    size < length + NETCTL_ADHOC_AUTH_HEADER_SIZE)
		return;
	tlvSize = size - length - NETCTL_ADHOC_AUTH_HEADER_SIZE;
	locked = netCtlAdhocAuthLock();
	peer = netCtlAdhocAuthPeerFind(auth, from.sin_addr);
	if (peer != NULL && peer->state == NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED)
		peer->lastReceive = sceKernelGetProcessTimeWide();
	switch (packet[1]) {
	case NETCTL_ADHOC_AUTH_PACKET_BEACON:
		break;
	case NETCTL_ADHOC_AUTH_PACKET_HELLO: {
		SceUChar8 request[0x1d0];

		tlv = (const char *)&NETCTL_ADHOC_AUTH_PAYLOAD(packet)[
			g_adhocAuthPayloadSizes[packet[1]]];
		if (auth->mode != 1 || auth->peerCount >= auth->maxPeers || peer != NULL)
			break;
		peer = netCtlAdhocAuthPeerAlloc();
		if (peer == NULL)
			break;
		netCtlAdhocAuthPeerAdd(auth, peer, from,
				       (const NetCtlAdhocPeerRecord *)
				       NETCTL_ADHOC_AUTH_PAYLOAD(packet));
		netCtlAdhocAuthParseTlv(peer, tlv, tlvSize);
		if (netCtlAdhocAuthGcRequest(peer->context, request) < 0) {
			netCtlAdhocAuthPeerRemove(auth, peer);
			break;
		}
		sceClibMemcpy_safe(&request[0xd0], &auth->record, sizeof(auth->record));
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_REQUEST,
					request, auth->tlv, auth->tlvSize);
		sceClibMemcpy_safe(peer->request, request, sizeof(peer->request));
		peer->retries = 0;
		netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
					  netCtlAdhocAuthResendTimer, peer);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_REQUESTED);
		break;
	}
	case NETCTL_ADHOC_AUTH_PACKET_REQUEST: {
		SceUChar8 context[0x100];
		SceUChar8 response[0x170];

		tlv = (const char *)&NETCTL_ADHOC_AUTH_PAYLOAD(packet)[
			g_adhocAuthPayloadSizes[packet[1]]];
		if (auth->mode != 1 || auth->peerCount >= auth->maxPeers)
			break;
		if (netCtlAdhocAuthGcResponse(context, NETCTL_ADHOC_AUTH_PAYLOAD(packet),
					      response) < 0)
			break;
		if (peer != NULL) {
			/* a new request, or one winning over ours: start again as responder */
			if ((peer->state == NETCTL_ADHOC_AUTH_PEER_RESPONDED &&
			     sceClibMemcmp(peer->request, NETCTL_ADHOC_AUTH_PAYLOAD(packet),
					   sizeof(peer->request)) != 0) ||
			    (peer->state == NETCTL_ADHOC_AUTH_PEER_REQUESTED &&
			     auth->ignoredAddress.s_addr >= peer->addr.sin_addr.s_addr) ||
			    peer->state == NETCTL_ADHOC_AUTH_PEER_CONFIRMED) {
				netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
				netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
			} else if (peer->state == NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED) {
				netCtlAdhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
				netCtlAdhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
				netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
			}
			sceClibMemcpy_safe(&peer->record, &NETCTL_ADHOC_AUTH_PAYLOAD(packet)[0xd0],
					   sizeof(peer->record));
			netCtlAdhocAuthParseTlv(peer, tlv, tlvSize);
		} else {
			peer = netCtlAdhocAuthPeerAlloc();
			if (peer == NULL)
				break;
			netCtlAdhocAuthPeerAdd(auth, peer, from,
					       (const NetCtlAdhocPeerRecord *)
					       &NETCTL_ADHOC_AUTH_PAYLOAD(packet)[0xd0]);
			netCtlAdhocAuthParseTlv(peer, tlv, tlvSize);
		}
		if (peer->state == NETCTL_ADHOC_AUTH_PEER_NEW) {
			sceClibMemcpy_safe(peer->context, context, sizeof(peer->context));
			netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_RESPONSE,
						response, NULL, 0);
			peer->retries = 0;
			netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
			netCtlAdhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
						  netCtlAdhocAuthResendTimer, peer);
			sceClibMemcpy_safe(peer->request, NETCTL_ADHOC_AUTH_PAYLOAD(packet),
					   sizeof(peer->request));
			sceClibMemcpy_safe(peer->response, response, sizeof(peer->response));
			netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_RESPONDED);
		} else if (peer->state == NETCTL_ADHOC_AUTH_PEER_RESPONDED) {
			netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_RESPONSE,
						peer->response, NULL, 0);
			peer->retries = 0;
			netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
			netCtlAdhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
						  netCtlAdhocAuthResendTimer, peer);
		}
		break;
	}
	case NETCTL_ADHOC_AUTH_PACKET_RESPONSE: {
		SceUChar8 context[0x100];
		SceUChar8 confirm[0x130];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != NETCTL_ADHOC_AUTH_PEER_REQUESTED)
			break;
		sceClibMemcpy_safe(context, peer->context, sizeof(context));
		if (netCtlAdhocAuthGcConfirm(context, NETCTL_ADHOC_AUTH_PAYLOAD(packet),
					     confirm) < 0)
			break;
		netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->context, context, sizeof(peer->context));
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_CONFIRM,
					confirm, NULL, 0);
		peer->retries = 0;
		netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
					  netCtlAdhocAuthResendTimer, peer);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_CONFIRMED);
		break;
	}
	case NETCTL_ADHOC_AUTH_PACKET_CONFIRM: {
		SceUChar8 context[0x100];
		SceUChar8 finish[0x60];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != NETCTL_ADHOC_AUTH_PEER_RESPONDED)
			break;
		sceClibMemcpy_safe(context, peer->context, sizeof(context));
		if (netCtlAdhocAuthGcFinish(context, NETCTL_ADHOC_AUTH_PAYLOAD(packet), finish) < 0)
			break;
		netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->context, context, sizeof(peer->context));
		netCtlAdhocAuthSendPeer(auth, peer->addr, NETCTL_ADHOC_AUTH_PACKET_FINISH,
					finish, NULL, 0);
		peer->keepaliveSent = 0;
		peer->keepaliveReceived = 0;
		netCtlAdhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
					  netCtlAdhocAuthTimeoutTimer, peer);
		netCtlAdhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
					  netCtlAdhocAuthKeepaliveTimer, peer);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED);
		break;
	}
	case NETCTL_ADHOC_AUTH_PACKET_FINISH: {
		SceUChar8 context[0x100];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != NETCTL_ADHOC_AUTH_PEER_CONFIRMED)
			break;
		sceClibMemcpy_safe(context, peer->context, sizeof(context));
		if (netCtlAdhocAuthGcVerify(context, NETCTL_ADHOC_AUTH_PAYLOAD(packet)) < 0)
			break;
		netCtlAdhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->context, context, sizeof(peer->context));
		peer->keepaliveSent = 0;
		peer->keepaliveReceived = 0;
		netCtlAdhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
					  netCtlAdhocAuthTimeoutTimer, peer);
		netCtlAdhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		netCtlAdhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
					  netCtlAdhocAuthKeepaliveTimer, peer);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED);
		break;
	}
	case NETCTL_ADHOC_AUTH_PACKET_BYE:
		if (peer == NULL)
			break;
		if (from.sin_addr.s_addr == 0xffffffff)
			netCtlAdhocAuthGcSignBroadcast(peer->context, NULL, 0,
						       NETCTL_ADHOC_AUTH_PAYLOAD(packet), 0, 0);
		else
			netCtlAdhocAuthGcSign(peer->context, NULL, 0,
					      NETCTL_ADHOC_AUTH_PAYLOAD(packet), 0, 0);
		netCtlAdhocAuthPeerSetState(auth, peer, NETCTL_ADHOC_AUTH_PEER_NEW);
		netCtlAdhocAuthPeerRemove(auth, peer);
		break;
	case NETCTL_ADHOC_AUTH_PACKET_KEEPALIVE: {
		SceUInt32 sequence;

		if (peer == NULL || peer->state != NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED)
			break;
		if (netCtlAdhocAuthGcSign(peer->context,
					  (void *)&NETCTL_ADHOC_AUTH_PAYLOAD(packet)[32], 16,
					  NETCTL_ADHOC_AUTH_PAYLOAD(packet), 1, 0) < 0)
			break;
		sceClibMemcpy_safe(&sequence, NETCTL_ADHOC_AUTH_PAYLOAD(packet), sizeof(sequence));
		sequence = sceNetNtohl(sequence);
		if (sequence > peer->keepaliveReceived) {
			peer->keepaliveReceived = sequence;
			netCtlAdhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
			netCtlAdhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
						  netCtlAdhocAuthTimeoutTimer, peer);
		}
		break;
	}
	}
	netCtlAdhocAuthUnlock(locked);
}

/* waits for a packet (handled at once) or a timer's message */
__attribute__((noinline))
int netCtlAdhocAuthReceive(NetCtlAdhocAuth *auth, NetCtlAdhocAuthMessage *message, int *received,
			   const SceUInt32 *timeout)
{
	SceNetSockaddrIn from;
	unsigned int fromlen;
	int ret;

	*received = 0;
	if (timeout != NULL && *timeout != 0) {
		ret = sceNetSetsockopt(auth->recvSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
				       timeout, sizeof(*timeout));
		if (ret < 0)
			return ret;
	}
	do {
		for (;;) {
			fromlen = sizeof(from);
			ret = sceNetRecvfrom(auth->recvSocket, g_adhocAuthRecvBuffer,
					     sizeof(g_adhocAuthRecvBuffer), 0,
					     (SceNetSockaddr *)&from, &fromlen);
			if (ret == 0)
				ret = NETCTL_ERROR_ADHOC_AUTH_FAILED;
			if (ret < 0)
				break;
			if (auth->localAddress.s_addr != from.sin_addr.s_addr) {
				netCtlAdhocAuthInput(auth, g_adhocAuthRecvBuffer, ret, from);
				break;
			}
		}
		if (ret < 0 && ret != SCE_NET_ERROR_EAGAIN) {
			*received = 0;
			return ret;
		}
		if (ret > 0) {
			*received = 0;
			return ret;
		}
		if (auth->stop != 0) {
			*received = 0;
			return 0;
		}
		ret = netCtlAdhocAuthMbxReceive(message);
	} while (ret != 0);
	*received = 1;
	return ret;
}

__attribute__((noinline))
void netCtlAdhocAuthBeaconTimer(void *arg)
{
	NetCtlAdhocAuth *auth = arg;

	if (auth->flags & NETCTL_ADHOC_AUTH_FLAG_BEACON_QUEUED)
		return;
	auth->flags |= NETCTL_ADHOC_AUTH_FLAG_BEACON_QUEUED;
	netCtlAdhocAuthMbxSend(&auth->beaconMessage, 0);
}

__attribute__((noinline))
void netCtlAdhocAuthResendTimer(void *arg)
{
	NetCtlAdhocAuthPeer *peer = arg;
	NetCtlAdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = NETCTL_ADHOC_AUTH_MESSAGE_RESEND;
	sceClibMemcpy_safe(&message.from, &peer->addr, sizeof(message.from));
	netCtlAdhocAuthMbxSend(&message, 0);
}

__attribute__((noinline))
void netCtlAdhocAuthTimeoutTimer(void *arg)
{
	NetCtlAdhocAuthPeer *peer = arg;
	NetCtlAdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = NETCTL_ADHOC_AUTH_MESSAGE_TIMEOUT;
	sceClibMemcpy_safe(&message.from, &peer->addr, sizeof(message.from));
	netCtlAdhocAuthMbxSend(&message, 0);
}

__attribute__((noinline))
void netCtlAdhocAuthKeepaliveTimer(void *arg)
{
	NetCtlAdhocAuthPeer *peer = arg;
	NetCtlAdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = NETCTL_ADHOC_AUTH_MESSAGE_KEEPALIVE;
	sceClibMemcpy_safe(&message.from, &peer->addr, sizeof(message.from));
	netCtlAdhocAuthMbxSend(&message, 0);
}

/* the game card authentication manager's ad-hoc requests (LibKernel) */
__attribute__((noinline))
int netCtlAdhocAuthGcInit(void)
{
	return SceLibKernel_C8082804();
}

__attribute__((noinline))
int netCtlAdhocAuthGcTerm(void)
{
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthGcRequest(void *context, void *output)
{
	return SceLibKernel_211BEDE8(context, output);
}

__attribute__((noinline))
int netCtlAdhocAuthGcResponse(void *context, const void *input, void *output)
{
	return SceLibKernel_DEAD6277(context, input, output);
}

__attribute__((noinline))
int netCtlAdhocAuthGcConfirm(void *context, const void *input, void *output)
{
	return SceLibKernel_104D802F(context, input, output);
}

__attribute__((noinline))
int netCtlAdhocAuthGcFinish(void *context, const void *input, void *output)
{
	return SceLibKernel_65126005(context, input, output);
}

__attribute__((noinline))
int netCtlAdhocAuthGcVerify(void *context, const void *input)
{
	return SceLibKernel_4AC7EFC9(context, input);
}

/* the packets' signatures: not checked in this build */
__attribute__((noinline))
int netCtlAdhocAuthGcSign(void *context, void *mac, SceSize macSize, const void *data,
			  int send, int flags)
{
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthGcSignBroadcast(void *context, void *mac, SceSize macSize, const void *data,
				   int send, int flags)
{
	return 0;
}
