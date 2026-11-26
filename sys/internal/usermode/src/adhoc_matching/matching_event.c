/*
 * SceNetAdhocMatching event thread and matching protocol state machine (0x81000c20..0x81001a6c).
 */
#include "matching_internal.h"

int matchingEventStart(AdhocMatchingContext *ctx, int priority, SceSize stackSize,
		       int cpuAffinityMask)
{
	SceUID pipeId;
	SceUID threadId;
	SceKernelThreadOptParam optParam;
	AdhocMatchingContext *arg = ctx;
	int ret;

	pipeId = sceKernelCreateMsgPipe("SceNetAdhocMatchingEvent", 64, 12,
					ADHOC_MATCHING_EVENT_PIPE_SIZE, NULL);
	if (pipeId < 0)
		return pipeId;

	arg->eventPipe = pipeId;
	sceClibMemset(&optParam, 0, sizeof(optParam));
	optParam.size = sizeof(optParam);

	threadId = sceKernelCreateThread("SceNetAdhocMatchingEvent", matchingEventThread,
					 priority, stackSize, 0, cpuAffinityMask, &optParam);
	arg->eventThread = threadId;
	if (threadId < 0) {
		sceKernelDeleteMsgPipe(arg->eventPipe);
		return threadId;
	}

	ret = sceKernelStartThread(threadId, sizeof(arg), &arg);
	if (ret < 0) {
		sceKernelDeleteThread(arg->eventThread);
		sceKernelDeleteMsgPipe(arg->eventPipe);
		return ret;
	}
	return 0;
}

int matchingEventStop(AdhocMatchingContext *ctx, int *lockStatus)
{
	sceKernelDeleteMsgPipe(ctx->eventPipe);
	matchingUnlock(*lockStatus);
	sceKernelWaitThreadEnd(ctx->eventThread, NULL, NULL);
	*lockStatus = matchingLock();
	return sceKernelDeleteThread(ctx->eventThread);
}

void matchingInvokeHandler(AdhocMatchingContext *ctx, unsigned int stackPad, int id, int event,
			   SceNetInAddr *peer, int optlen, void *opt)
{
	char margin[stackPad];

	(void)margin;
	ctx->handler(id, event, peer, optlen, opt);
}

static inline __attribute__((always_inline)) void
matchingNotifyEvent(AdhocMatchingContext *ctx, int event, SceNetInAddr *addr,
		    int optlen, void *opt)
{
	if (ctx->handler != NULL) {
		if (optlen > 0)
			matchingInvokeHandler(ctx, 0, ctx->id, event, addr, optlen, opt);
		else
			matchingInvokeHandler(ctx, 0, ctx->id, event, addr, 0, NULL);
	}
}

void matchingProcessPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	unsigned int pktType = peer->recvPacket[1];
	unsigned int sessionId = 0;
	int activeCount;

	switch (ctx->mode) {
	case SCE_NET_ADHOC_MATCHING_MODE_PARENT:
		if (pktType == ADHOC_MATCHING_PKT_HELLO || pktType == ADHOC_MATCHING_PKT_KEEPALIVE)
			return;
		break;
	case SCE_NET_ADHOC_MATCHING_MODE_CHILD:
		if (pktType == ADHOC_MATCHING_PKT_REQUEST ||
		    pktType == ADHOC_MATCHING_PKT_KEEPALIVE_ACK)
			return;
		break;
	case SCE_NET_ADHOC_MATCHING_MODE_P2P:
		if (matchingIsHigherAddr(peer) != 0) {
			if (pktType == ADHOC_MATCHING_PKT_KEEPALIVE)
				return;
		} else {
			if (pktType == ADHOC_MATCHING_PKT_KEEPALIVE_ACK)
				return;
		}
		break;
	default:
		break;
	}

	switch (pktType) {
	case ADHOC_MATCHING_PKT_REQUEST:
	case ADHOC_MATCHING_PKT_ACCEPT:
		if (peer->recvPacketSize - peer->recvPayloadEnd >= 16) {
			sceClibMemcpy_safe(&sessionId, peer->recvPacket + peer->recvPayloadEnd,
					   sizeof(sessionId));
			sessionId = sceNetNtohl(sessionId);
			if (sessionId != peer->remoteSessionId) {
				switch (peer->state) {
				case ADHOC_MATCHING_PEER_INIT:
				case ADHOC_MATCHING_PEER_REQUEST_SENT:
					break;
				case ADHOC_MATCHING_PEER_REQUEST_RECV:
				case ADHOC_MATCHING_PEER_ACCEPT_SENT:
					matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
					matchingStopPeerTimers(ctx, peer);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_CANCEL,
								      &peer->addr, 0, NULL);
					break;
				case ADHOC_MATCHING_PEER_ESTABLISHED:
					matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
					matchingStopPeerTimers(ctx, peer);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_LEAVE,
								      &peer->addr, 0, NULL);
					break;
				default:
					break;
				}
			}
		}
		break;
	default:
		break;
	}

	activeCount = matchingCountPeers(ctx, ADHOC_MATCHING_PEER_ACCEPT_SENT);

	switch (pktType) {
	case ADHOC_MATCHING_PKT_HELLO:
		if (peer->recvPayloadEnd - 4 >= 8) {
			switch (peer->state) {
			case ADHOC_MATCHING_PEER_INIT:
				sceClibMemcpy_safe(&peer->helloInterval, peer->recvPacket + 4, 4);
				peer->helloInterval = sceNetNtohl(peer->helloInterval);
				sceClibMemcpy_safe(&peer->keepaliveInterval, peer->recvPacket + 8,
						   4);
				peer->keepaliveInterval = sceNetNtohl(peer->keepaliveInterval);
				if (peer->recvPacketSize - peer->recvPayloadEnd >= 16) {
					sceClibMemcpy_safe(&peer->remoteCaps,
							   peer->recvPacket + peer->recvPayloadEnd,
							   4);
				}
				if (activeCount + 1 < ctx->maxNum) {
					matchingNotifyEvent(ctx,
							    SCE_NET_ADHOC_MATCHING_EVENT_HELLO,
							    &peer->addr, peer->recvPayloadEnd - 12,
							    peer->recvPacket + 12);
				}
				break;
			case ADHOC_MATCHING_PEER_REQUEST_RECV:
			case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			case ADHOC_MATCHING_PEER_REQUEST_SENT:
			case ADHOC_MATCHING_PEER_ESTABLISHED:
			default:
				break;
			}
		}
		break;

	case ADHOC_MATCHING_PKT_REQUEST:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			if (activeCount + 1 >= ctx->maxNum) {
				matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
							  0, NULL);
			} else {
				matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_REQUEST_RECV);
				peer->remoteSessionId = sessionId;
				matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_REQUEST_ACK,
							  0, NULL);
				matchingNotifyEvent(ctx, SCE_NET_ADHOC_MATCHING_EVENT_REQUEST,
						    &peer->addr, peer->recvPayloadEnd - 4,
						    peer->recvPacket + 4);
			}
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
			if (activeCount + 1 >= ctx->maxNum) {
				matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
				matchingStopPeerTimers(ctx, peer);
				matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
							  0, NULL);
				if (ctx->handler != NULL)
					matchingInvokeHandler(ctx, 0, ctx->id,
							      SCE_NET_ADHOC_MATCHING_EVENT_CANCEL,
							      &peer->addr, 0, NULL);
			} else {
				matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_REQUEST_ACK,
							  0, NULL);
			}
			break;
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_ACCEPT,
						  peer->optLen, peer->opt);
			matchingStartRexmtTimer(ctx, peer);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_ACCEPT_SENT);
			peer->remoteSessionId = sessionId;
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_ACCEPT,
						  peer->optLen, peer->opt);
			matchingStartRexmtTimer(ctx, peer);
			matchingNotifyEvent(ctx, SCE_NET_ADHOC_MATCHING_EVENT_ACCEPT,
					    &peer->addr, peer->recvPayloadEnd - 4,
					    peer->recvPacket + 4);
			break;
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_ACCEPT:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
						  peer->optLen, peer->opt);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_ESTABLISHED);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CONFIRM, 0, NULL);
			matchingStartKeepaliveTimer(ctx, peer);
			peer->keepaliveCount = ctx->initCount;
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ESTABLISHED,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT: {
			void *optPtr;
			int optLen;

			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_ESTABLISHED);
			peer->remoteSessionId = sessionId;
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CONFIRM, 0, NULL);
			matchingStartKeepaliveTimer(ctx, peer);
			peer->keepaliveCount = ctx->initCount;
			optPtr = peer->recvPacket + 4;
			optLen = peer->recvPayloadEnd - 4;
			if (ctx->handler != NULL) {
				if (optLen > 0)
					matchingInvokeHandler(ctx, 0, ctx->id,
							      SCE_NET_ADHOC_MATCHING_EVENT_ACCEPT,
							      &peer->addr, optLen, optPtr);
				else
					matchingInvokeHandler(ctx, 0, ctx->id,
							      SCE_NET_ADHOC_MATCHING_EVENT_ACCEPT,
							      &peer->addr, 0, NULL);
				if (ctx->handler != NULL)
					matchingInvokeHandler(ctx, 0, ctx->id,
							      SCE_NET_ADHOC_MATCHING_EVENT_ESTABLISHED,
							      &peer->addr, 0, NULL);
			}
			break;
		}
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CONFIRM, 0, NULL);
			break;
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_CONFIRM:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
						  peer->optLen, peer->opt);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_ESTABLISHED);
			matchingStartKeepaliveTimer(ctx, peer);
			peer->keepaliveCount = ctx->initCount;
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ESTABLISHED,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ESTABLISHED:
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_CANCEL:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingNotifyEvent(ctx, SCE_NET_ADHOC_MATCHING_EVENT_CANCEL,
					    &peer->addr, peer->recvPayloadEnd - 4,
					    peer->recvPacket + 4);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingNotifyEvent(ctx, SCE_NET_ADHOC_MATCHING_EVENT_DENY,
					    &peer->addr, peer->recvPayloadEnd - 4,
					    peer->recvPacket + 4);
			break;
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingNotifyEvent(ctx, SCE_NET_ADHOC_MATCHING_EVENT_LEAVE,
					    &peer->addr, peer->recvPayloadEnd - 4,
					    peer->recvPacket + 4);
			break;
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_KEEPALIVE:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
						  peer->optLen, peer->opt);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_ESTABLISHED);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_KEEPALIVE_ACK,
						  0, NULL);
			matchingStartKeepaliveTimer(ctx, peer);
			peer->keepaliveCount = ctx->initCount;
			if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD)
				matchingSaveKeepalivePacket(ctx, peer->recvPacket,
							    peer->recvPayloadEnd);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ESTABLISHED,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_KEEPALIVE_ACK,
						  0, NULL);
			peer->keepaliveCount = ctx->initCount;
			if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD)
				matchingSaveKeepalivePacket(ctx, peer->recvPacket,
							    peer->recvPayloadEnd);
			break;
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_KEEPALIVE_ACK:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
						  peer->optLen, peer->opt);
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL, 0, NULL);
			if (ctx->handler != NULL)
				matchingInvokeHandler(ctx, 0, ctx->id,
						      SCE_NET_ADHOC_MATCHING_EVENT_ERROR,
						      &peer->addr, 0, NULL);
			break;
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			peer->keepaliveCount = ctx->initCount;
			break;
		default:
			break;
		}
		break;

	case ADHOC_MATCHING_PKT_BYE:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
			break;
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
			matchingStopPeerTimers(ctx, peer);
			break;
		default:
			break;
		}
		if (ctx->handler != NULL)
			matchingInvokeHandler(ctx, 0, ctx->id, SCE_NET_ADHOC_MATCHING_EVENT_BYE,
					      &peer->addr, 0, NULL);
		peer->byeReceived = 1;
		break;

	case ADHOC_MATCHING_PKT_REQUEST_ACK:
		switch (peer->state) {
		case ADHOC_MATCHING_PEER_INIT:
		case ADHOC_MATCHING_PEER_REQUEST_RECV:
		case ADHOC_MATCHING_PEER_ACCEPT_SENT:
		case ADHOC_MATCHING_PEER_ESTABLISHED:
			break;
		case ADHOC_MATCHING_PEER_REQUEST_SENT:
			peer->rexmtCount = ctx->initCount;
			break;
		default:
			break;
		}
		/* fallthrough: matches original binary */
	case ADHOC_MATCHING_PKT_DATA:
	case ADHOC_MATCHING_PKT_DATA_ACK:
		if (peer->state == ADHOC_MATCHING_PEER_ESTABLISHED) {
			unsigned int seq;

			sceClibMemcpy_safe(&sessionId, peer->recvPacket + 4, sizeof(sessionId));
			sessionId = sceNetNtohl(sessionId);
			sceClibMemcpy_safe(&seq, peer->recvPacket + 8, sizeof(seq));
			seq = sceNetNtohl(seq);
			if (sessionId == peer->remoteSessionId) {
				if (pktType == ADHOC_MATCHING_PKT_DATA) {
					if (seq >= peer->recvDataSeq) {
						peer->recvDataSeq = seq + 1;
						matchingNotifyEvent(ctx,
								    SCE_NET_ADHOC_MATCHING_EVENT_DATA,
								    &peer->addr,
								    peer->recvPayloadEnd - 12,
								    peer->recvPacket + 12);
					}
					matchingSendDataPacket(ctx, peer,
							       ADHOC_MATCHING_PKT_DATA_ACK,
							       0, NULL);
				} else if (peer->dataState == ADHOC_MATCHING_DATA_SENDING &&
					   seq == peer->sendDataSeq) {
					matchingSetPeerDataState(ctx, peer,
								 ADHOC_MATCHING_DATA_IDLE);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_DATA_ACK,
								      &peer->addr, 0, NULL);
				}
			}
		}
		break;

	default:
		break;
	}
}

int matchingEventThread(SceSize argSize, void *argBlock)
{
	AdhocMatchingContext *ctx = *(AdhocMatchingContext **)argBlock;
	AdhocMatchingEventMessage message;
	SceSize recvSize;

	(void)argSize;
	for (;;) {
		int lockStatus;
		int innerLock;
		int evType;
		AdhocMatchingPeer *peer;

		if (sceKernelReceiveMsgPipe(ctx->eventPipe, &message, sizeof(message), 1,
					    &recvSize, NULL) < 0)
			break;

		lockStatus = matchingLock();
		evType = message.type;
		peer = message.peer;

		innerLock = matchingLock();
		message.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
		matchingUnlock(innerLock);

		switch (evType) {
		case ADHOC_MATCHING_EV_RECV_PACKET:
			peer->recvEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
			matchingProcessPacket(ctx, peer);
			if (peer->recvPacket != NULL)
				matchingFree(peer->recvPacket);
			peer->recvPacketSize = 0;
			peer->recvPacket = NULL;
			peer->recvPayloadEnd = 0;
			break;

		case ADHOC_MATCHING_EV_REXMT_TIMEOUT:
			peer->timerEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
			if (peer->state == ADHOC_MATCHING_PEER_REQUEST_SENT) {
				if ((peer->remoteCaps[0] & ADHOC_MATCHING_CAP_REXMT_TIMEOUT) != 0 &&
				    --peer->rexmtCount <= 0) {
					matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_CANCEL,
								  0, NULL);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_TIMEOUT,
								      &peer->addr, 0, NULL);
				} else {
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_REQUEST,
								  peer->optLen, peer->opt);
					matchingStartRexmtTimer(ctx, peer);
				}
			}
			if (peer->state == ADHOC_MATCHING_PEER_ACCEPT_SENT) {
				if (--peer->rexmtCount <= 0) {
					matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_CANCEL,
								  0, NULL);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_TIMEOUT,
								      &peer->addr, 0, NULL);
				} else {
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_ACCEPT,
								  peer->optLen, peer->opt);
					matchingStartRexmtTimer(ctx, peer);
				}
			}
			break;

		case ADHOC_MATCHING_EV_KEEPALIVE_TIMEOUT:
			peer->timerEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
			if (peer->state == ADHOC_MATCHING_PEER_ESTABLISHED) {
				if (--peer->keepaliveCount <= 0) {
					matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_CANCEL,
								  0, NULL);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_TIMEOUT,
								      &peer->addr, 0, NULL);
				} else {
					if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ||
					    (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_P2P &&
					     matchingIsHigherAddr(peer) != 0)) {
						matchingSendKeepalive(ctx, peer);
					}
					matchingStartKeepaliveTimer(ctx, peer);
				}
			}
			break;

		case ADHOC_MATCHING_EV_HELLO_TIMEOUT:
			ctx->helloEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
			if (matchingCountPeers(ctx, ADHOC_MATCHING_PEER_ACCEPT_SENT) + 1 <
			    ctx->maxNum)
				matchingSendHello(ctx);
			matchingStartHelloTimer(ctx, ctx->helloInterval);
			break;

		case ADHOC_MATCHING_EV_DATA_TIMEOUT:
			peer->dataTimerEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
			if (peer->dataState == ADHOC_MATCHING_DATA_SENDING) {
				if (--peer->dataRexmtCount <= 0) {
					matchingSetPeerDataState(ctx, peer,
								 ADHOC_MATCHING_DATA_IDLE);
					if (ctx->handler != NULL)
						matchingInvokeHandler(ctx, 0, ctx->id,
								      SCE_NET_ADHOC_MATCHING_EVENT_DATA_TIMEOUT,
								      &peer->addr, 0, NULL);
				} else {
					matchingSendDataPacket(ctx, peer, ADHOC_MATCHING_PKT_DATA,
							       peer->sendDataLen, peer->sendData);
					matchingStartDataTimer(ctx, peer);
				}
			}
			break;

		default:
			break;
		}

		if (peer != NULL && peer->byeReceived == 1 &&
		    (peer->recvEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0 &&
		    (peer->timerEvent.flags & ADHOC_MATCHING_EV_FLAG_QUEUED) == 0) {
			matchingFreePeer(ctx, peer);
		}
		matchingUnlock(lockStatus);
	}
	return 0;
}
