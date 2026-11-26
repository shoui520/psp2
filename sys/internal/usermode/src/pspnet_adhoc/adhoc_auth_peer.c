/*
 * adhoc_auth_peer.c: ad-hoc authentication peers, session lock, TLVs, raw Ethernet 0x88C8
 * handshake state machine, timers, and SceLibKernel GC auth wrappers for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

static const SceUChar8 g_adhocAuthSubType[2] = { 0x00, 0x01 };
static const SceUChar8 g_adhocAuthDataSubType[2] = { 0x00, 0x02 };
const SceUShort16 g_adhocAuthPayloadSizes[9] = {
	0, 0, 0x80, 0x120, 0x110, 0x100, 0x60, 0x20, 0x30,
};

SceUID g_adhocAuthLockSema = -1;
SceUID g_adhocAuthLockOwner = -1;

SceUChar8 g_adhocAuthSendBuffer[PSPNET_ADHOC_AUTH_FRAME_SIZE];
SceUChar8 g_adhocAuthRecvBuffer[PSPNET_ADHOC_AUTH_FRAME_SIZE];

typedef struct AdhocRawEtherSockaddr {
	SceUChar8 header[14];
	SceNetEtherAddr addr;
} AdhocRawEtherSockaddr;

__attribute__((noinline))
void adhocAuthPeerAdd(AdhocAuth *auth, AdhocAuthPeer *peer, const SceUChar8 *addr,
		      const SceNetAdhocctlNickname *nickname)
{
	sceClibMemcpy_safe(&peer->key.addr, addr, sizeof(peer->key.addr));
	sceClibMemcpy_safe(&peer->addr, addr, sizeof(peer->addr));
	sceClibMemcpy_safe(&peer->nickname, nickname, sizeof(peer->nickname));
	peer->auth = auth;
	peer->next = &auth->peers;
	peer->prev = auth->peers.prev;
	peer->state = PSPNET_ADHOC_AUTH_PEER_NEW;
	auth->peers.prev = peer;
	peer->prev->next = peer;
	auth->peerCount++;
}

__attribute__((noinline))
AdhocAuthPeer *adhocAuthPeerFind(AdhocAuth *auth, const SceUChar8 *addr)
{
	AdhocAuthPeer *peer;

	for (peer = auth->peers.next; peer != &auth->peers; peer = peer->next) {
		if (sceClibMemcmp(&peer->addr, addr, sizeof(peer->addr)) == 0)
			return peer;
	}
	return NULL;
}

__attribute__((noinline))
void adhocAuthPeerRemove(AdhocAuth *auth, AdhocAuthPeer *peer)
{
	if (peer == NULL)
		return;
	adhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
	adhocAuthCalloutRemove(&peer->resendTimer, NULL);
	adhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
	peer->next->prev = peer->prev;
	peer->prev->next = peer->next;
	peer->next = peer;
	peer->prev = peer;
	auth->peerCount--;
	adhocAuthPeerFree(peer);
}

__attribute__((noinline))
void adhocAuthPeerSetState(AdhocAuth *auth, AdhocAuthPeer *peer, int state)
{
	(void)auth;
	sceKernelWaitSema(g_adhocAuthKeySema, 1, NULL);
	if (peer->state != PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED &&
	    state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED) {
		adhocAuthKeyAdd(&peer->key);
		peer->key.lastReceive = sceKernelGetProcessTimeWide();
	}
	if (peer->state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED &&
	    state != PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED) {
		adhocAuthKeyRemove(&peer->key);
	}
	sceKernelSignalSema(g_adhocAuthKeySema, 1);
	peer->state = state;
}

__attribute__((noinline))
void adhocAuthSetMode(AdhocAuth *auth, int mode)
{
	auth->mode = mode;
}

__attribute__((noinline))
int adhocAuthLockInit(void)
{
	g_adhocAuthLockSema = sceKernelCreateSema("SceNetAdhocAuthLock", 0, 1, 1, NULL);
	if (g_adhocAuthLockSema < 0)
		return g_adhocAuthLockSema;
	g_adhocAuthLockOwner = -1;
	return 0;
}

__attribute__((noinline))
void adhocAuthLockTerm(void)
{
	if (g_adhocAuthLockSema >= 0)
		sceKernelDeleteSema(g_adhocAuthLockSema);
	g_adhocAuthLockSema = -1;
}

__attribute__((noinline))
int adhocAuthLock(void)
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
void adhocAuthUnlock(int locked)
{
	if (locked == 0)
		return;
	g_adhocAuthLockOwner = -1;
	sceKernelSignalSema(g_adhocAuthLockSema, 1);
}

__attribute__((noinline))
int adhocAuthBuildTlv(AdhocAuth *auth, SceUChar8 *tlv, int *size)
{
	tlv[0] = 1;
	tlv[1] = 4;
	if (&auth->applicationId != NULL)
		sceClibMemcpy_safe(&tlv[2], &auth->applicationId, 4);
	tlv[6] = 2;
	tlv[7] = 4;
	if (&auth->swVersion != NULL)
		sceClibMemcpy_safe(&tlv[8], &auth->swVersion, 4);
	tlv[12] = 3;
	tlv[13] = 4;
	if (&auth->option != NULL)
		sceClibMemcpy_safe(&tlv[14], &auth->option, 4);
	*size = 18;
	return 18;
}

__attribute__((noinline))
void adhocAuthParseTlv(AdhocAuthPeer *peer, const char *tlv, int size)
{
	int offset = 0;
	SceUChar8 type, length;
	const char *value;

	while (offset < size) {
		if (size - offset < 2)
			break;
		type = (SceUChar8)tlv[offset];
		length = (SceUChar8)tlv[offset + 1];
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
int adhocAuthSend(AdhocAuth *auth, const SceUChar8 *to, SceUChar8 *frame,
		  SceUChar8 type, const void *payload, const void *extra, SceSize extraSize)
{
	SceUShort16 length, ethType;
	SceSize totalSize;

	length = g_adhocAuthPayloadSizes[type];
	totalSize = (SceUShort16)(length + extraSize + PSPNET_ADHOC_AUTH_ENC_HDR_SIZE);
	if (totalSize > PSPNET_ADHOC_AUTH_FRAME_SIZE)
		return PSPNET_ADHOC_AUTH_ERROR_FAILED;
	if (payload != NULL)
		sceClibMemcpy_safe(&frame[PSPNET_ADHOC_AUTH_ENC_HDR_SIZE], payload, length);
	if (extra != NULL)
		sceClibMemcpy_safe(&frame[PSPNET_ADHOC_AUTH_ENC_HDR_SIZE + length], extra,
				   extraSize);
	frame[17] = type;
	frame[16] = PSPNET_ADHOC_AUTH_VERSION;
	length = sceNetHtons(length);
	sceClibMemcpy_safe(&frame[18], &length, sizeof(length));
	sceClibMemcpy_safe(&frame[0], to, 6);
	sceClibMemcpy_safe(&frame[6], &auth->localAddr, 6);
	ethType = sceNetHtons(PSPNET_ADHOC_ETHER_PROTO);
	{
		SceUShort16 *ethTypePtr = (SceUShort16 *)&frame[12];
		*ethTypePtr = ethType;
	}
	sceClibMemcpy_safe(&frame[14], g_adhocAuthSubType, sizeof(g_adhocAuthSubType));
	return sceNetSend(auth->sendSocket, frame, totalSize, 0);
}

static inline __attribute__((always_inline)) int adhocAuthSendPeer(AdhocAuth *auth,
		const SceUChar8 *to, SceUChar8 type, const void *payload,
		const void *extra, SceSize extraSize)
{
	int optval = 0;

	sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST,
			 &optval, sizeof(optval));
	return adhocAuthSend(auth, to, g_adhocAuthSendBuffer, type, payload, extra, extraSize);
}

static inline __attribute__((always_inline)) int adhocAuthSendBroadcast(AdhocAuth *auth,
		SceUChar8 type, const void *payload, const void *extra, SceSize extraSize)
{
	SceUChar8 bcast[6];
	int optval;

	sceClibMemset(bcast, 0xff, sizeof(bcast));
	optval = 1;
	sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST,
			 &optval, sizeof(optval));
	return adhocAuthSend(auth, bcast, g_adhocAuthSendBuffer, type, payload, extra, extraSize);
}

__attribute__((noinline))
void adhocAuthResend(AdhocAuth *auth, AdhocAuthPeer *peer)
{
	int locked;

	locked = adhocAuthLock();
	if (peer != NULL && (peer->state == PSPNET_ADHOC_AUTH_PEER_REQUESTED ||
			     peer->state == PSPNET_ADHOC_AUTH_PEER_CONFIRMED ||
			     peer->state == PSPNET_ADHOC_AUTH_PEER_RESPONDED)) {
		if (peer->retries >= auth->maxRetries || auth->mode == 2) {
			adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
			adhocAuthPeerRemove(auth, peer);
		} else {
			if (peer->state == PSPNET_ADHOC_AUTH_PEER_REQUESTED)
				adhocAuthSendPeer(auth, peer->addr.data,
						  PSPNET_ADHOC_AUTH_PKT_REQUEST,
						  peer->request, auth->tlv,
						  (SceSize)auth->tlvSize);
			else if (peer->state == PSPNET_ADHOC_AUTH_PEER_CONFIRMED)
				adhocAuthSendPeer(auth, peer->addr.data,
						  PSPNET_ADHOC_AUTH_PKT_CONFIRM,
						  peer->confirm, NULL, 0);
			peer->retries++;
			adhocAuthCalloutRemove(&peer->resendTimer, NULL);
			adhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
					    adhocAuthResendTimer, peer);
		}
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthBeacon(AdhocAuth *auth)
{
	int locked;

	locked = adhocAuthLock();
	if (auth->mode == 1) {
		if (auth->peerCount < auth->maxPeers)
			adhocAuthSendBroadcast(auth, PSPNET_ADHOC_AUTH_PKT_HELLO,
					       &auth->nickname, auth->tlv,
					       (SceSize)auth->tlvSize);
		else
			adhocAuthSendBroadcast(auth, PSPNET_ADHOC_AUTH_PKT_BEACON, NULL, NULL, 0);
		adhocAuthCalloutRemove(&auth->beaconTimer, NULL);
		adhocAuthCalloutAdd(&auth->beaconTimer, auth->beaconInterval,
				    adhocAuthBeaconTimer, auth);
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthKeepalive(AdhocAuth *auth, AdhocAuthPeer *peer)
{
	SceUInt32 seq;
	SceUChar8 payload[0x30];
	int locked;

	locked = adhocAuthLock();
	if (peer != NULL && peer->state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED && auth->mode != 2) {
		peer->keepaliveSent++;
		seq = sceNetHtonl(peer->keepaliveSent);
		sceClibMemset(payload, 0, sizeof(payload));
		sceClibMemcpy_safe(&payload[32], &seq, sizeof(seq));
		if (adhocAuthGcSign(peer->key.context, &payload[32], 16, payload, 1) >= 0)
			adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_KEEPALIVE,
					  payload, NULL, 0);
		adhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		adhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
				    adhocAuthKeepaliveTimer, peer);
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthTimeout(AdhocAuth *auth, AdhocAuthPeer *peer)
{
	SceUChar8 mac[32];
	int locked;

	locked = adhocAuthLock();
	if (peer != NULL && auth->mode != 2) {
		if (adhocAuthGcSign(peer->key.context, NULL, 0, mac, 0) >= 0)
			adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_BYE,
					  mac, NULL, 0);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
		adhocAuthPeerRemove(auth, peer);
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthShutdown(AdhocAuth *auth)
{
	AdhocAuthPeer *peer;
	SceUChar8 bcast[6];
	SceUChar8 mac[32];
	int locked, optval;

	locked = adhocAuthLock();
	if (adhocAuthGcSignBroadcast(NULL, 0, mac, 0) >= 0) {
		sceClibMemset(bcast, 0xff, sizeof(bcast));
		optval = 1;
		sceNetSetsockopt(auth->sendSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST,
				 &optval, sizeof(optval));
		adhocAuthSend(auth, bcast, g_adhocAuthSendBuffer, PSPNET_ADHOC_AUTH_PKT_BYE,
			      mac, NULL, 0);
	}
	for (peer = auth->peers.next; peer != &auth->peers; peer = auth->peers.next) {
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
		adhocAuthPeerRemove(auth, peer);
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
void adhocAuthHandleRequest(AdhocAuth *auth, AdhocAuthPeer *peer, const SceUChar8 *packet,
			    const SceUChar8 *from, const char *tlv, int tlvSize)
{
	SceUChar8 context[0xC0];
	SceUChar8 response[0x110];
	int i;

	if (auth->mode != 1 || auth->peerCount >= auth->maxPeers)
		return;
	if (adhocAuthGcResponse(context, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], response) < 0)
		return;
	if (peer != NULL) {
		if (peer->state == PSPNET_ADHOC_AUTH_PEER_RESPONDED &&
		    sceClibMemcmp(peer->request, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE],
				  sizeof(peer->request)) != 0) {
			adhocAuthCalloutRemove(&peer->resendTimer, NULL);
			adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
		} else {
			switch (peer->state) {
			case PSPNET_ADHOC_AUTH_PEER_REQUESTED:
				for (i = 0; i < 6; i++) {
					if (auth->localAddr.data[i] < peer->key.addr.data[i])
						break;
					if (auth->localAddr.data[i] > peer->key.addr.data[i]) {
						adhocAuthCalloutRemove(&peer->resendTimer, NULL);
						adhocAuthPeerSetState(auth, peer,
								      PSPNET_ADHOC_AUTH_PEER_NEW);
						break;
					}
				}
				break;
			case PSPNET_ADHOC_AUTH_PEER_CONFIRMED:
				adhocAuthCalloutRemove(&peer->resendTimer, NULL);
				adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
				break;
			case PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED:
				adhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
				adhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
				adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
				break;
			default:
				break;
			}
		}
		sceClibMemcpy_safe(&peer->nickname, &packet[164], sizeof(peer->nickname));
		adhocAuthParseTlv(peer, tlv, tlvSize);
	} else {
		peer = adhocAuthPeerAlloc();
		if (peer == NULL)
			return;
		adhocAuthPeerAdd(auth, peer, from, (const SceNetAdhocctlNickname *)&packet[164]);
		adhocAuthParseTlv(peer, tlv, tlvSize);
	}
	if (peer->state == PSPNET_ADHOC_AUTH_PEER_NEW) {
		sceClibMemcpy_safe(peer->key.context, context, sizeof(peer->key.context));
		adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_RESPONSE,
				  response, NULL, 0);
		peer->retries = 0;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		adhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
				    adhocAuthResendTimer, peer);
		sceClibMemcpy_safe(peer->request, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE],
				   sizeof(peer->request));
		sceClibMemcpy_safe(peer->response, response, sizeof(peer->response));
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_RESPONDED);
	} else if (peer->state == PSPNET_ADHOC_AUTH_PEER_RESPONDED) {
		adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_RESPONSE,
				  peer->response, NULL, 0);
		peer->retries = 0;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		adhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
				    adhocAuthResendTimer, peer);
	}
}

__attribute__((noinline))
void adhocAuthInput(AdhocAuth *auth, const SceUChar8 *packet, int size, const SceUChar8 *from)
{
	SceUShort16 length;
	AdhocAuthPeer *peer;
	const char *tlv;
	int tlvSize, locked;

	if (sceClibMemcmp(&auth->localAddr, from, sizeof(auth->localAddr)) == 0)
		return;
	if (packet[0] != PSPNET_ADHOC_AUTH_VERSION)
		return;
	if (packet[1] == 0 || packet[1] > PSPNET_ADHOC_AUTH_PKT_KEEPALIVE)
		return;
	sceClibMemcpy_safe(&length, &packet[2], sizeof(length));
	length = sceNetNtohs(length);
	if (length != g_adhocAuthPayloadSizes[packet[1]] ||
	    size < (int)length + PSPNET_ADHOC_AUTH_HDR_SIZE)
		return;
	tlvSize = size - length - PSPNET_ADHOC_AUTH_HDR_SIZE;
	locked = adhocAuthLock();
	peer = adhocAuthPeerFind(auth, from);
	if (peer != NULL && peer->state == PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED)
		peer->key.lastReceive = sceKernelGetProcessTimeWide();
	switch (packet[1]) {
	case PSPNET_ADHOC_AUTH_PKT_BEACON:
		break;
	case PSPNET_ADHOC_AUTH_PKT_HELLO: {
		SceUChar8 request[0x120];

		tlv = (const char *)&packet[PSPNET_ADHOC_AUTH_HDR_SIZE +
					    g_adhocAuthPayloadSizes[packet[1]]];
		if (auth->mode != 1 || auth->peerCount >= auth->maxPeers || peer != NULL)
			break;
		peer = adhocAuthPeerAlloc();
		if (peer == NULL)
			break;
		adhocAuthPeerAdd(auth, peer, from,
				 (const SceNetAdhocctlNickname *)&packet[PSPNET_ADHOC_AUTH_HDR_SIZE]);
		adhocAuthParseTlv(peer, tlv, tlvSize);
		if (adhocAuthGcRequest(peer->key.context, request) < 0) {
			adhocAuthPeerRemove(auth, peer);
			break;
		}
		sceClibMemcpy_safe(&request[0xA0], &auth->nickname, sizeof(auth->nickname));
		adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_REQUEST,
				  request, auth->tlv, (SceSize)auth->tlvSize);
		sceClibMemcpy_safe(peer->request, request, sizeof(peer->request));
		peer->retries = 0;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		adhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
				    adhocAuthResendTimer, peer);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_REQUESTED);
		break;
	}
	case PSPNET_ADHOC_AUTH_PKT_REQUEST:
		tlv = (const char *)&packet[PSPNET_ADHOC_AUTH_HDR_SIZE +
					    g_adhocAuthPayloadSizes[packet[1]]];
		adhocAuthHandleRequest(auth, peer, packet, from, tlv, tlvSize);
		break;
	case PSPNET_ADHOC_AUTH_PKT_RESPONSE: {
		SceUChar8 context[0xC0];
		SceUChar8 confirm[0x100];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != PSPNET_ADHOC_AUTH_PEER_REQUESTED)
			break;
		sceClibMemcpy_safe(context, peer->key.context, sizeof(context));
		if (adhocAuthGcConfirm(context, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], confirm) < 0)
			break;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->key.context, context, sizeof(peer->key.context));
		adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_CONFIRM,
				  confirm, NULL, 0);
		peer->retries = 0;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		adhocAuthCalloutAdd(&peer->resendTimer, auth->resendInterval,
				    adhocAuthResendTimer, peer);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_CONFIRMED);
		break;
	}
	case PSPNET_ADHOC_AUTH_PKT_CONFIRM: {
		SceUChar8 context[0xC0];
		SceUChar8 finish[0x60];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != PSPNET_ADHOC_AUTH_PEER_RESPONDED)
			break;
		sceClibMemcpy_safe(context, peer->key.context, sizeof(context));
		if (adhocAuthGcFinish(context, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], finish) < 0)
			break;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->key.context, context, sizeof(peer->key.context));
		adhocAuthSendPeer(auth, peer->addr.data, PSPNET_ADHOC_AUTH_PKT_FINISH,
				  finish, NULL, 0);
		peer->keepaliveSent = 0;
		peer->keepaliveReceived = 0;
		adhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
		adhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
				    adhocAuthTimeoutTimer, peer);
		adhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		adhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
				    adhocAuthKeepaliveTimer, peer);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED);
		break;
	}
	case PSPNET_ADHOC_AUTH_PKT_FINISH: {
		SceUChar8 context[0xC0];

		if (auth->mode != 1 || peer == NULL ||
		    peer->state != PSPNET_ADHOC_AUTH_PEER_CONFIRMED)
			break;
		sceClibMemcpy_safe(context, peer->key.context, sizeof(context));
		if (adhocAuthGcVerify(context, &packet[PSPNET_ADHOC_AUTH_HDR_SIZE]) < 0)
			break;
		adhocAuthCalloutRemove(&peer->resendTimer, NULL);
		sceClibMemcpy_safe(peer->key.context, context, sizeof(peer->key.context));
		peer->keepaliveSent = 0;
		peer->keepaliveReceived = 0;
		adhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
		adhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
				    adhocAuthTimeoutTimer, peer);
		adhocAuthCalloutRemove(&peer->keepaliveTimer, NULL);
		adhocAuthCalloutAdd(&peer->keepaliveTimer, auth->keepaliveInterval,
				    adhocAuthKeepaliveTimer, peer);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED);
		break;
	}
	case PSPNET_ADHOC_AUTH_PKT_BYE: {
		SceUChar8 bcast[6];

		if (peer == NULL)
			break;
		sceClibMemset(bcast, 0xff, sizeof(bcast));
		if (sceClibMemcmp(from, bcast, sizeof(bcast)) == 0)
			adhocAuthGcDecryptBroadcast(peer->key.context, NULL, 0,
						    &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], 0, 0);
		else
			adhocAuthGcDecrypt(peer->key.context, NULL, 0,
					   &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], 0, 0);
		adhocAuthPeerSetState(auth, peer, PSPNET_ADHOC_AUTH_PEER_NEW);
		adhocAuthPeerRemove(auth, peer);
		break;
	}
	case PSPNET_ADHOC_AUTH_PKT_KEEPALIVE: {
		SceUInt32 seq;

		if (peer == NULL || peer->state != PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED)
			break;
		if (adhocAuthGcDecrypt(peer->key.context, (void *)&packet[36], 16,
				       &packet[PSPNET_ADHOC_AUTH_HDR_SIZE], 1, 0) < 0)
			break;
		sceClibMemcpy_safe(&seq, &packet[36], sizeof(seq));
		seq = sceNetNtohl(seq);
		if (seq > peer->keepaliveReceived) {
			peer->keepaliveReceived = seq;
			adhocAuthCalloutRemove(&peer->timeoutTimer, NULL);
			adhocAuthCalloutAdd(&peer->timeoutTimer, auth->peerTimeout,
					    adhocAuthTimeoutTimer, peer);
		}
		break;
	}
	}
	adhocAuthUnlock(locked);
}

__attribute__((noinline))
int adhocAuthReceive(AdhocAuth *auth, AdhocAuthMessage *message, int *received,
		     const SceUInt32 *timeout)
{
	AdhocRawEtherSockaddr from;
	unsigned int fromlen;
	SceNetEtherAddr srcAddr;
	int ret, s;
	AdhocMbuf *m;

	*received = 0;
	if (timeout != NULL && *timeout != 0) {
		ret = sceNetSetsockopt(auth->recvSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
				       timeout, sizeof(*timeout));
		if (ret < 0)
			return ret;
	}
	do {
		fromlen = sizeof(from);
		ret = sceNetRecvfrom(auth->recvSocket, g_adhocAuthRecvBuffer,
				     sizeof(g_adhocAuthRecvBuffer), 0,
				     (SceNetSockaddr *)&from, &fromlen);
		if (ret == 0) {
			ret = PSPNET_ADHOC_AUTH_ERROR_FAILED;
		} else if (ret > 0) {
			if (ret <= 14 ||
			    sceClibMemcmp(&from.addr, &auth->localAddr, sizeof(auth->localAddr)) == 0) {
				ret = 0;
			} else if (sceClibMemcmp(&g_adhocAuthRecvBuffer[14], g_adhocAuthSubType,
						 sizeof(g_adhocAuthSubType)) == 0) {
				sceClibMemcpy_safe(&srcAddr, &from.addr, sizeof(srcAddr));
				adhocAuthInput(auth, &g_adhocAuthRecvBuffer[16], ret - 14,
					       srcAddr.data);
			} else if (sceClibMemcmp(&g_adhocAuthRecvBuffer[14], g_adhocAuthDataSubType,
						 sizeof(g_adhocAuthDataSubType)) == 0) {
				s = adhocSplNet();
				m = adhocMbufDevGet(g_adhocAuthRecvBuffer, (SceSize)ret);
				if (m != NULL) {
					ret = adhocAuthDecryptFrame(m);
					if (ret >= 0) {
						adhocEnqueueInput(NULL, m);
						adhocProcessInput();
					} else {
						adhocMbufFree(m);
						ret = 0;
					}
				}
				adhocSplX(s);
			} else {
				ret = 0;
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
		ret = adhocAuthMbxReceive(message);
	} while (ret != 0);
	*received = 1;
	return ret;
}

__attribute__((noinline))
void adhocAuthBeaconTimer(void *arg)
{
	AdhocAuth *auth = (AdhocAuth *)arg;

	if (auth->flags & PSPNET_ADHOC_AUTH_FLAG_BEACON_QUEUED)
		return;
	auth->flags |= PSPNET_ADHOC_AUTH_FLAG_BEACON_QUEUED;
	adhocAuthMbxSend(&auth->beaconMessage, 0);
}

__attribute__((noinline))
void adhocAuthResendTimer(void *arg)
{
	AdhocAuthPeer *peer = (AdhocAuthPeer *)arg;
	AdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = PSPNET_ADHOC_AUTH_MSG_RESEND;
	sceClibMemcpy_safe(&message.addr, &peer->addr, sizeof(message.addr));
	adhocAuthMbxSend(&message, 0);
}

__attribute__((noinline))
void adhocAuthTimeoutTimer(void *arg)
{
	AdhocAuthPeer *peer = (AdhocAuthPeer *)arg;
	AdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = PSPNET_ADHOC_AUTH_MSG_TIMEOUT;
	sceClibMemcpy_safe(&message.addr, &peer->addr, sizeof(message.addr));
	adhocAuthMbxSend(&message, 0);
}

__attribute__((noinline))
void adhocAuthKeepaliveTimer(void *arg)
{
	AdhocAuthPeer *peer = (AdhocAuthPeer *)arg;
	AdhocAuthMessage message;

	sceClibMemset(&message, 0, sizeof(message));
	message.type = PSPNET_ADHOC_AUTH_MSG_KEEPALIVE;
	sceClibMemcpy_safe(&message.addr, &peer->addr, sizeof(message.addr));
	adhocAuthMbxSend(&message, 0);
}

__attribute__((noinline))
int adhocAuthGcInit(void)
{
	return 0;
}

__attribute__((noinline))
int adhocAuthGcTerm(void)
{
	return 0;
}

__attribute__((noinline))
int adhocAuthGcRequest(void *context, void *output)
{
	return SceLibKernel_EFD76235(context, output);
}

__attribute__((noinline))
int adhocAuthGcResponse(void *context, const void *input, void *output)
{
	return SceLibKernel_1F4DF829(context, input, output);
}

__attribute__((noinline))
int adhocAuthGcConfirm(void *context, const void *input, void *output)
{
	return SceLibKernel_C362ECD6(context, input, output);
}

__attribute__((noinline))
int adhocAuthGcFinish(void *context, const void *input, void *output)
{
	return SceLibKernel_6314CAA3(context, input, output);
}

__attribute__((noinline))
int adhocAuthGcVerify(void *context, const void *input)
{
	return SceLibKernel_3B595E1D(context, input);
}

__attribute__((noinline))
int adhocAuthGcSign(void *context, const void *data, int len, void *mac, int send)
{
	return SceLibKernel_A9002567(context, data, len, mac, send);
}

__attribute__((noinline))
int adhocAuthGcDecrypt(void *context, void *data, int len, const void *mac, int send, int verify)
{
	return SceLibKernel_01E00CBF(context, data, len, mac, send, verify);
}

__attribute__((noinline))
int adhocAuthGcSignBroadcast(const void *data, int len, void *mac, int send)
{
	return SceLibKernel_88E72157(data, len, mac, send);
}

__attribute__((noinline))
int adhocAuthGcDecryptBroadcast(void *context, void *data, int len, const void *mac,
				int send, int verify)
{
	return SceLibKernel_E2984A54(context, data, len, mac, send, verify);
}
