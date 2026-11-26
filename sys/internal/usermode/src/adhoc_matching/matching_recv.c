/*
 * SceNetAdhocMatching receive socket, input thread, and protocol timers
 * (0x81002750..0x81002d60).
 */
#include "matching_internal.h"

int matchingRecvStart(AdhocMatchingContext *ctx, int priority, SceSize stackSize,
		      int cpuAffinityMask)
{
	SceNetSockaddrIn sin;
	SceKernelThreadOptParam optParam;
	AdhocMatchingContext *arg = ctx;
	int sock;
	int ret;

	sock = sceNetSocket("SceNetAdhocMatchingRecv", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	if (sock < 0)
		return sock;

	arg->recvSock = sock;
	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons(arg->port);

	ret = sceNetBind(arg->recvSock, (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret >= 0) {
		sceClibMemset(&optParam, 0, sizeof(optParam));
		optParam.size = sizeof(optParam);
		arg->inputThread = sceKernelCreateThread("SceNetAdhocMatchingInput",
							 matchingInputThread, priority, stackSize,
							 0, cpuAffinityMask, &optParam);
		if (arg->inputThread < 0) {
			ret = arg->inputThread;
		} else {
			ret = sceKernelStartThread(arg->inputThread, sizeof(arg), &arg);
			if (ret >= 0)
				return 0;
			sceKernelDeleteThread(arg->inputThread);
		}
	}
	sceNetSocketClose(arg->recvSock);
	return ret;
}

void matchingRecvStop(AdhocMatchingContext *ctx, int *lockStatus)
{
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		sceNetSocketAbort(ctx->sendSock, 0);
	sceNetSocketAbort(ctx->recvSock, 1);
	matchingUnlock(*lockStatus);
	sceKernelWaitThreadEnd(ctx->inputThread, NULL, NULL);
	*lockStatus = matchingLock();
	sceKernelDeleteThread(ctx->inputThread);
	sceNetShutdown(ctx->recvSock, 0);
	sceNetSocketClose(ctx->recvSock);
	ctx->recvSock = 0;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150) {
		sceNetShutdown(ctx->sendSock, 0);
		sceNetSocketClose(ctx->sendSock);
		ctx->sendSock = 0;
	}
}

int matchingInputThread(SceSize argSize, void *argBlock)
{
	AdhocMatchingContext *ctx = *(AdhocMatchingContext **)argBlock;
	SceNetSockaddrIn from;
	SceSize sentSize;
	SceNetSocklen_t fromLen;
	SceUShort16 netLen;

	(void)argSize;
	for (;;) {
		int recvLen;
		int rxBufLen;
		unsigned char *buf;
		int payloadEnd;
		int lockStatus;
		AdhocMatchingPeer *peer;

		rxBufLen = ctx->rxBufLen;
		fromLen = sizeof(from);
		recvLen = sceNetRecvfrom(ctx->recvSock, ctx->rxBuf, rxBufLen, 0,
					 (SceNetSockaddr *)&from, &fromLen);
		if (recvLen < 0)
			break;
		if (g_adhocMatchingLocalAddr.s_addr == from.sin_addr.s_addr)
			continue;

		buf = (unsigned char *)ctx->rxBuf;
		if (buf[0] != ADHOC_MATCHING_PROTOCOL_VERSION)
			continue;
		if (buf[1] <= 0 && buf[1] >= ADHOC_MATCHING_PKT_MAX)
			continue;

		sceClibMemcpy_safe(&netLen, buf + 2, sizeof(netLen));
		payloadEnd = (int)sceNetNtohs(netLen) + 4;
		if (payloadEnd > recvLen)
			continue;

		lockStatus = matchingLock();
		peer = matchingFindPeer(ctx, &from.sin_addr);
		if (peer == NULL) {
			unsigned char pktType = ((unsigned char *)ctx->rxBuf)[1];

			if ((pktType == ADHOC_MATCHING_PKT_REQUEST &&
			     (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ||
			      ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_P2P)) ||
			    (pktType == ADHOC_MATCHING_PKT_HELLO &&
			     (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD ||
			      ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_P2P))) {
				peer = matchingAllocPeer(ctx, &from.sin_addr);
			}
		}
		if (peer != NULL && (peer->recvEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
			unsigned char *copy = matchingAlloc(recvLen);

			if (copy != NULL) {
				sceClibMemcpy_safe(copy, ctx->rxBuf, recvLen);
				peer->recvPacketSize = recvLen;
				peer->recvPacket = copy;
				peer->recvPayloadEnd = payloadEnd;
				peer->recvEvent.type = ADHOC_MATCHING_EV_RECV_PACKET;
				peer->recvEvent.peer = peer;
				peer->recvEvent.flags |= ADHOC_MATCHING_EV_FLAG_QUEUED;
				peer->keepaliveCount = ctx->initCount;
				sceKernelSendMsgPipe(ctx->eventPipe, &peer->recvEvent,
						     sizeof(peer->recvEvent), 1, &sentSize, NULL);
			}
		}
		matchingUnlock(lockStatus);
	}
	return 0;
}

int matchingStartRexmtTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	int lockStatus = matchingLock();

	if (peer->timerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->protocolCallout, NULL);
		peer->timerActive = -1;
	}
	matchingCalloutAdd(&ctx->calloutContext, &peer->protocolCallout, ctx->rexmtInterval,
			   matchingOnRexmtTimeout, peer);
	peer->timerActive = 1;
	return matchingUnlock(lockStatus);
}

int matchingStartDataTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	int lockStatus = matchingLock();

	if (peer->dataTimerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->dataCallout, NULL);
		peer->dataTimerActive = -1;
	}
	matchingCalloutAdd(&ctx->calloutContext, &peer->dataCallout, ctx->rexmtInterval,
			   matchingOnDataTimeout, peer);
	peer->dataTimerActive = 1;
	return matchingUnlock(lockStatus);
}

int matchingStartKeepaliveTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	unsigned int interval;
	int lockStatus;

	interval = (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD) ? peer->keepaliveInterval :
								      ctx->keepaliveInterval;
	lockStatus = matchingLock();
	if (peer->timerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->protocolCallout, NULL);
		peer->timerActive = -1;
	}
	matchingCalloutAdd(&ctx->calloutContext, &peer->protocolCallout, interval,
			   matchingOnKeepaliveTimeout, peer);
	peer->timerActive = 1;
	return matchingUnlock(lockStatus);
}

int matchingStartHelloTimer(AdhocMatchingContext *ctx, unsigned int interval)
{
	int lockStatus = matchingLock();

	if (ctx->helloTimerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &ctx->helloCallout, NULL);
		ctx->helloTimerActive = -1;
	}
	matchingCalloutAdd(&ctx->calloutContext, &ctx->helloCallout, interval,
			   matchingOnHelloTimeout, ctx);
	ctx->helloTimerActive = 1;
	return matchingUnlock(lockStatus);
}

int matchingStopPeerTimers(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	int lockStatus = matchingLock();

	if (peer->timerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->protocolCallout, NULL);
		peer->timerActive = -1;
	}
	if (peer->dataTimerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->dataCallout, NULL);
		peer->dataTimerActive = -1;
	}
	return matchingUnlock(lockStatus);
}

int matchingStopDataTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	int lockStatus = matchingLock();

	if (peer->dataTimerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &peer->dataCallout, NULL);
		peer->dataTimerActive = -1;
	}
	return matchingUnlock(lockStatus);
}

int matchingStopHelloTimer(AdhocMatchingContext *ctx)
{
	int lockStatus = matchingLock();

	if (ctx->helloTimerActive >= 0) {
		matchingCalloutRemove(&ctx->calloutContext, &ctx->helloCallout, NULL);
		ctx->helloTimerActive = -1;
	}
	return matchingUnlock(lockStatus);
}

void matchingOnRexmtTimeout(void *arg)
{
	AdhocMatchingPeer *peer = (AdhocMatchingPeer *)arg;
	SceSize sentSize;

	if ((peer->timerEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
		peer->timerEvent.type = ADHOC_MATCHING_EV_REXMT_TIMEOUT;
		peer->timerEvent.peer = peer;
		peer->timerEvent.flags |= ADHOC_MATCHING_EV_FLAG_QUEUED;
		sceKernelSendMsgPipe(peer->eventPipe, &peer->timerEvent, sizeof(peer->timerEvent),
				     1, &sentSize, NULL);
	}
	peer->timerActive = -1;
}

void matchingOnDataTimeout(void *arg)
{
	AdhocMatchingPeer *peer = (AdhocMatchingPeer *)arg;
	SceSize sentSize;

	if ((peer->dataTimerEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
		peer->dataTimerEvent.type = ADHOC_MATCHING_EV_DATA_TIMEOUT;
		peer->dataTimerEvent.peer = peer;
		peer->dataTimerEvent.flags |= ADHOC_MATCHING_EV_FLAG_QUEUED;
		sceKernelSendMsgPipe(peer->eventPipe, &peer->dataTimerEvent,
				     sizeof(peer->dataTimerEvent), 1, &sentSize, NULL);
	}
	peer->dataTimerActive = -1;
}

void matchingOnKeepaliveTimeout(void *arg)
{
	AdhocMatchingPeer *peer = (AdhocMatchingPeer *)arg;
	SceSize sentSize;

	if ((peer->timerEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
		peer->timerEvent.type = ADHOC_MATCHING_EV_KEEPALIVE_TIMEOUT;
		peer->timerEvent.peer = peer;
		peer->timerEvent.flags |= ADHOC_MATCHING_EV_FLAG_QUEUED;
		sceKernelSendMsgPipe(peer->eventPipe, &peer->timerEvent, sizeof(peer->timerEvent),
				     1, &sentSize, NULL);
	}
	peer->timerActive = -1;
}

void matchingOnHelloTimeout(void *arg)
{
	AdhocMatchingContext *ctx = (AdhocMatchingContext *)arg;
	SceSize sentSize;

	if ((ctx->helloEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
		ctx->helloEvent.type = ADHOC_MATCHING_EV_HELLO_TIMEOUT;
		ctx->helloEvent.peer = NULL;
		ctx->helloEvent.flags |= ADHOC_MATCHING_EV_FLAG_QUEUED;
		sceKernelSendMsgPipe(ctx->eventPipe, &ctx->helloEvent, sizeof(ctx->helloEvent),
				     1, &sentSize, NULL);
	}
	ctx->helloTimerActive = -1;
}
