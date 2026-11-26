/*
 * SceNetAdhocMatching context list, memory pool, lock, and peer management
 * (0x81001a6c..0x81002040).
 */
#include "matching_internal.h"

static int s_lockInitialized;
static AdhocMatchingContext *s_contextList;
static int s_lastContextId;
static SceKernelLwMutexWork s_lwMutex;
static void *s_mspace;
SceNetInAddr g_adhocMatchingLocalAddr;

void matchingValidateAddr(const SceNetInAddr *addr)
{
	(void)addr;
}

int matchingUpdateLocalAddr(void)
{
	return sceNetCtlAdhocGetInAddr(&g_adhocMatchingLocalAddr);
}

int matchingLockInit(void)
{
	int ret;

	ret = sceKernelCreateLwMutex(&s_lwMutex, "SceNetAdhocMatching",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE |
				     SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0)
		return ret;
	s_lockInitialized = 1;
	return 0;
}

void matchingLockTerm(void)
{
	if (s_lockInitialized != 0)
		sceKernelDeleteLwMutex(&s_lwMutex);
	s_lockInitialized = 0;
}

int matchingLock(void)
{
	return sceKernelLockLwMutex(&s_lwMutex, 1, NULL);
}

int matchingUnlock(int lockStatus)
{
	if (lockStatus == 0)
		return sceKernelUnlockLwMutex(&s_lwMutex, 1);
	return lockStatus;
}

int matchingContextListInit(void)
{
	s_contextList = NULL;
	return 0;
}

void matchingFree(void *ptr)
{
	sceClibMspaceFree(s_mspace, ptr);
}

int matchingContextListTerm(void)
{
	AdhocMatchingContext *ctx = s_contextList;
	AdhocMatchingContext *next;

	while (ctx != NULL) {
		next = ctx->next;
		sceClibMspaceFree(s_mspace, ctx->rxBuf);
		ctx->rxBuf = NULL;
		sceClibMspaceFree(s_mspace, ctx);
		ctx = next;
	}
	return 0;
}

int matchingCheckNotRunning(void)
{
	AdhocMatchingContext *ctx;

	for (ctx = s_contextList; ctx != NULL; ctx = ctx->next) {
		if (ctx->state != ADHOC_MATCHING_CTX_STOPPED)
			return SCE_NET_ADHOC_MATCHING_ERROR_BUSY;
	}
	return 0;
}

int matchingPoolCreate(SceSize size, void *memblock)
{
	s_mspace = sceClibMspaceCreate(memblock, size);
	return 0;
}

void matchingPoolDestroy(void)
{
	sceClibMspaceDestroy(s_mspace);
}

void *matchingAlloc(SceSize size)
{
	return sceClibMspaceMalloc(s_mspace, size);
}

AdhocMatchingContext *matchingFindContext(int id)
{
	AdhocMatchingContext *ctx;

	for (ctx = s_contextList; ctx != NULL; ctx = ctx->next) {
		if (ctx->id == id)
			break;
	}
	return ctx;
}

int matchingAllocContext(SceUShort16 port)
{
	AdhocMatchingContext *ctx;
	int id;

	for (ctx = s_contextList; ctx != NULL; ctx = ctx->next) {
		if (ctx->port == port)
			return SCE_NET_ADHOC_MATCHING_ERROR_PORT_IN_USE;
	}

	id = (s_lastContextId == ADHOC_MATCHING_MAX_CONTEXTS) ? 1 : s_lastContextId + 1;
	for (;;) {
		if (id == s_lastContextId)
			return SCE_NET_ADHOC_MATCHING_ERROR_ID_NOT_AVAIL;
		for (ctx = s_contextList; ctx != NULL; ctx = ctx->next) {
			if (ctx->id == id)
				break;
		}
		if (ctx == NULL)
			break;
		id = (id == ADHOC_MATCHING_MAX_CONTEXTS) ? 1 : id + 1;
	}

	s_lastContextId = id;
	if (id < 0)
		return id;

	ctx = sceClibMspaceMalloc(s_mspace, sizeof(*ctx));
	if (ctx == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	sceClibMemset(ctx, 0, sizeof(*ctx));
	ctx->id = id;
	ctx->next = s_contextList;
	s_contextList = ctx;
	return id;
}

void matchingFreeContext(AdhocMatchingContext *target)
{
	AdhocMatchingContext *ctx = s_contextList;
	AdhocMatchingContext *prev = NULL;

	while (ctx != NULL) {
		if (ctx == target) {
			if (prev != NULL)
				prev->next = ctx->next;
			else
				s_contextList = ctx->next;
			ctx->next = NULL;
			break;
		}
		prev = ctx;
		ctx = ctx->next;
	}
	sceClibMspaceFree(s_mspace, target);
}

void matchingSetPeerDataState(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int state)
{
	(void)ctx;
	if (peer->dataState == state)
		return;

	if (peer->dataState == ADHOC_MATCHING_DATA_SENDING && state == ADHOC_MATCHING_DATA_IDLE) {
		if (peer->sendDataLen > 0) {
			sceClibMspaceFree(s_mspace, peer->sendData);
			peer->sendDataLen = 0;
			peer->sendData = NULL;
		}
	}
	peer->dataState = state;
}

void matchingSetPeerState(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int state)
{
	if (peer->state == state)
		return;

	if ((peer->state == ADHOC_MATCHING_PEER_REQUEST_SENT ||
	     peer->state == ADHOC_MATCHING_PEER_ACCEPT_SENT) &&
	    state != ADHOC_MATCHING_PEER_REQUEST_SENT &&
	    state != ADHOC_MATCHING_PEER_ACCEPT_SENT) {
		if (peer->optLen > 0) {
			sceClibMspaceFree(s_mspace, peer->opt);
			peer->optLen = 0;
			peer->opt = NULL;
		}
	}

	if (peer->state != ADHOC_MATCHING_PEER_ESTABLISHED &&
	    state == ADHOC_MATCHING_PEER_ESTABLISHED) {
		peer->sendDataSeq = 0;
		peer->recvDataSeq = 0;
	}

	if (peer->state == ADHOC_MATCHING_PEER_ESTABLISHED &&
	    state != ADHOC_MATCHING_PEER_ESTABLISHED) {
		matchingSetPeerDataState(ctx, peer, ADHOC_MATCHING_DATA_IDLE);
	}

	if (peer->state == ADHOC_MATCHING_PEER_ESTABLISHED ||
	    state == ADHOC_MATCHING_PEER_ESTABLISHED) {
		peer->state = state;
		matchingBuildKeepalivePacket(ctx);
	} else {
		peer->state = state;
	}
}

AdhocMatchingPeer *matchingAllocPeer(AdhocMatchingContext *ctx, const SceNetInAddr *addr)
{
	AdhocMatchingPeer *peer;

	peer = sceClibMspaceMalloc(s_mspace, sizeof(*peer));
	if (peer == NULL)
		return NULL;

	sceClibMemset(peer, 0, sizeof(*peer));
	matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
	sceClibMemcpy_safe(&peer->addr, addr, sizeof(peer->addr));
	peer->eventPipe = ctx->eventPipe;
	SceNetInternal_689B9D7D(&peer->localSessionId);
	if (peer->localSessionId == 0)
		peer->localSessionId = 1;
	peer->timerActive = -1;
	matchingSetPeerDataState(ctx, peer, ADHOC_MATCHING_DATA_IDLE);
	peer->dataTimerActive = -1;
	peer->next = ctx->peers;
	ctx->peers = peer;
	return peer;
}

AdhocMatchingPeer *matchingFindPeer(AdhocMatchingContext *ctx, const SceNetInAddr *addr)
{
	AdhocMatchingPeer *peer;

	for (peer = ctx->peers; peer != NULL; peer = peer->next) {
		if (peer->addr.s_addr == addr->s_addr && peer->byeReceived == 0)
			break;
	}
	return peer;
}

void matchingFreePeer(AdhocMatchingContext *ctx, AdhocMatchingPeer *target)
{
	AdhocMatchingPeer *peer = ctx->peers;
	AdhocMatchingPeer *prev = NULL;

	while (peer != NULL) {
		if (peer == target) {
			if (prev != NULL)
				prev->next = peer->next;
			else
				ctx->peers = peer->next;
			if (target->optLen > 0)
				sceClibMspaceFree(s_mspace, target->opt);
			if (target->recvPacket != NULL)
				sceClibMspaceFree(s_mspace, target->recvPacket);
			if (target->sendDataLen > 0)
				sceClibMspaceFree(s_mspace, target->sendData);
			sceClibMspaceFree(s_mspace, target);
			return;
		}
		prev = peer;
		peer = peer->next;
	}
}

int matchingFreeAllPeers(AdhocMatchingContext *ctx)
{
	AdhocMatchingPeer *peer = ctx->peers;
	AdhocMatchingPeer *next;

	while (peer != NULL) {
		matchingStopPeerTimers(ctx, peer);
		if (peer->optLen > 0)
			sceClibMspaceFree(s_mspace, peer->opt);
		if (peer->sendDataLen > 0)
			sceClibMspaceFree(s_mspace, peer->sendData);
		if (peer->recvPacket != NULL)
			sceClibMspaceFree(s_mspace, peer->recvPacket);
		next = peer->next;
		sceClibMspaceFree(s_mspace, peer);
		peer = next;
	}
	ctx->peers = NULL;
	return matchingSendBye(ctx);
}

void matchingCollectPeers(AdhocMatchingContext *ctx, int minState, SceNetInAddr *outAddrs,
			  int *count)
{
	AdhocMatchingPeer *peer;
	int found = 0;

	for (peer = ctx->peers; peer != NULL; peer = peer->next) {
		if (peer->state >= minState) {
			if (outAddrs != NULL) {
				if (*count <= found)
					break;
				sceClibMemcpy_safe(&outAddrs[found], &peer->addr,
						   sizeof(peer->addr));
			}
			found++;
		}
	}
	*count = found;
}

int matchingCountPeers(AdhocMatchingContext *ctx, int minState)
{
	AdhocMatchingPeer *peer;
	int count = 0;

	for (peer = ctx->peers; peer != NULL; peer = peer->next) {
		if (peer->state >= minState)
			count++;
	}
	return count;
}

void matchingCopyMembers(AdhocMatchingContext *ctx, SceSize *memberNum,
			 SceNetAdhocMatchingMember *members)
{
	SceSize maxCount = *memberNum;
	SceSize count = 0;
	int offset = 4;

	while (offset < ctx->keepalivePacketLen) {
		if (members != NULL && count < maxCount) {
			sceClibMemcpy_safe(&members[count], ctx->keepalivePacket + offset,
					   sizeof(SceNetInAddr));
		}
		count++;
		offset += sizeof(SceNetInAddr);
	}
	*memberNum = count;
}

int matchingIsHigherAddr(const AdhocMatchingPeer *peer)
{
	return g_adhocMatchingLocalAddr.s_addr < peer->addr.s_addr;
}
