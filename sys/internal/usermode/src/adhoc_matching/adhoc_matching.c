/*
 * SceNetAdhocMatching public API (0x81000000..0x81000c20).
 */
#include "matching_internal.h"

static int s_initialized;
static int s_maxOptLen;
static int s_maxDataLen;
static int s_maxHelloOptLen;

int sceNetAdhocMatchingInit(SceSize poolsize, void *poolptr)
{
	int ret;

	if (s_initialized > 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_INITIALIZED;
	if (poolptr == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	ret = matchingLockInit();
	if (ret < 0)
		return ret;

	ret = matchingPoolCreate(poolsize == 0 ? SCE_NET_ADHOC_MATCHING_POOLSIZE_DEFAULT : poolsize,
				 poolptr);
	if (ret >= 0) {
		ret = matchingContextListInit();
		if (ret >= 0) {
			s_initialized = 1;
			return 0;
		}
		matchingPoolDestroy();
	}
	matchingLockTerm();
	return ret;
}

int sceNetAdhocMatchingDelete(int id)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_STOPPED) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING;
	} else {
		matchingFree(ctx->rxBuf);
		ctx->rxBuf = NULL;
		matchingFreeContext(ctx);
	}

	matchingUnlock(lockStatus);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocMatchingStop(int id)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state == ADHOC_MATCHING_CTX_RUNNING) {
		ctx->state = ADHOC_MATCHING_CTX_STOPPING;
		matchingCalloutTerm(&ctx->calloutContext);
		matchingRecvStop(ctx, &lockStatus);
		matchingEventStop(ctx, &lockStatus);
		if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ||
		    ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_P2P) {
			matchingStopHelloTimer(ctx);
			matchingFreeHelloPacket(ctx);
			ctx->helloEvent.flags &= ~ADHOC_MATCHING_EV_FLAG_QUEUED;
		}
		matchingFreeAllPeers(ctx);
		matchingFreeKeepalivePacket(ctx);
		matchingSendSocketClose(ctx);
		ctx->state = ADHOC_MATCHING_CTX_STOPPED;
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingTerm(void)
{
	int i;
	int ret;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	for (i = 0; i < ADHOC_MATCHING_MAX_CONTEXTS; i++) {
		sceNetAdhocMatchingStop(i);
		sceNetAdhocMatchingDelete(i);
	}

	ret = matchingCheckNotRunning();
	if (ret >= 0) {
		matchingContextListTerm();
		matchingPoolDestroy();
		matchingLockTerm();
		s_initialized = 0;
	}
	return ret;
}

int sceNetAdhocMatchingCreate(int mode, int maxnum, SceUShort16 port, int rxbuflen,
			      unsigned int helloInterval, unsigned int keepaliveInterval,
			      int initCount, unsigned int rexmtInterval,
			      SceNetAdhocMatchingHandler handler)
{
	int lockStatus;
	int ret;
	int id;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	if (mode <= 0 || mode > SCE_NET_ADHOC_MATCHING_MODE_P2P) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE;
	} else if (maxnum < 2 || maxnum > SCE_NET_ADHOC_MATCHING_MAXNUM) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MAXNUM;
	} else if ((unsigned int)rxbuflen < (unsigned int)(maxnum * 4 + 4)) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_RXBUF_TOO_SHORT;
	} else if ((mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ||
		    mode == SCE_NET_ADHOC_MATCHING_MODE_P2P) &&
		   (helloInterval == 0 || keepaliveInterval == 0)) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;
	} else if (initCount < 0 || rexmtInterval == 0) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;
	} else {
		id = matchingAllocContext(port);
		if (id < 0) {
			ret = id;
		} else {
			ctx = matchingFindContext(id);
			ctx->mode = mode;
			ctx->maxNum = (mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT) ? maxnum : 2;
			ctx->port = port;
			ctx->rxBufLen = rxbuflen;
			ctx->rxBuf = matchingAlloc(rxbuflen);
			if (ctx->rxBuf == NULL) {
				matchingFreeContext(ctx);
				ret = SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
			} else {
				ctx->helloInterval = helloInterval;
				ctx->keepaliveInterval = keepaliveInterval;
				ctx->initCount = initCount;
				ctx->rexmtInterval = rexmtInterval;
				ctx->handler = handler;
				ctx->helloTimerActive = -1;
				ctx->localCaps[0] = 1;
				ctx->localCaps[1] = 0;
				ctx->localCaps[2] = 0;
				ctx->localCaps[3] = 0;
				ctx->peers = NULL;
				ret = id;
			}
		}
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingStart(int id, int threadPriority, int threadStackSize,
			     int threadCpuAffinityMask, int helloOptlen, const void *helloOpt)
{
	int lockStatus;
	int ret = 0;
	int priority;
	int stackSize;
	int affinity;
	unsigned int randVal;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V310) {
		s_maxOptLen = ADHOC_MATCHING_LEGACY_MAXOPTLEN;
		s_maxDataLen = ADHOC_MATCHING_LEGACY_MAXDATALEN;
		s_maxHelloOptLen = ADHOC_MATCHING_LEGACY_MAXHELLOOPTLEN;
	} else {
		s_maxOptLen = SCE_NET_ADHOC_MATCHING_MAXOPTLEN;
		s_maxDataLen = SCE_NET_ADHOC_MATCHING_MAXDATALEN;
		s_maxHelloOptLen = SCE_NET_ADHOC_MATCHING_MAXHELLOOPTLEN;
	}

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_STOPPED) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING;
	} else if (helloOptlen < 0 || helloOptlen > s_maxHelloOptLen ||
		   (helloOptlen > 0 && helloOpt == NULL)) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN;
	} else {
		ret = matchingSendSocketOpen(ctx);
		if (ret >= 0) {
			priority = (threadPriority == 0) ?
				   SCE_NET_ADHOC_MATCHING_THREAD_PRIORITY_DEFAULT : threadPriority;
			stackSize = (threadStackSize == 0) ?
				    SCE_NET_ADHOC_MATCHING_THREAD_STACK_SIZE_DEFAULT :
				    threadStackSize;
			affinity = (threadCpuAffinityMask == 0) ?
				   SCE_NET_ADHOC_MATCHING_THREAD_CPU_AFFINITY_MASK :
				   threadCpuAffinityMask;

			ret = matchingEventStart(ctx, priority, stackSize, affinity);
			if (ret >= 0) {
				priority = (threadPriority == 0) ?
					   SCE_NET_ADHOC_MATCHING_THREAD_PRIORITY_DEFAULT :
					   threadPriority;
				affinity = (threadCpuAffinityMask == 0) ?
					   SCE_NET_ADHOC_MATCHING_THREAD_CPU_AFFINITY_MASK :
					   threadCpuAffinityMask;
				ret = matchingRecvStart(ctx, priority,
							ADHOC_MATCHING_INPUT_STACK_SIZE, affinity);
				if (ret >= 0) {
					priority = (threadPriority == 0) ?
						   SCE_NET_ADHOC_MATCHING_THREAD_PRIORITY_DEFAULT :
						   threadPriority;
					affinity = (threadCpuAffinityMask == 0) ?
						   SCE_NET_ADHOC_MATCHING_THREAD_CPU_AFFINITY_MASK :
						   threadCpuAffinityMask;
					ret = matchingCalloutInit(&ctx->calloutContext,
								  "SceNetAdhocMatchingCallout",
								  ADHOC_MATCHING_CALLOUT_STACK_SIZE,
								  priority, affinity);
					if (ret >= 0) {
						if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT ||
						    ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_P2P) {
							ret = matchingBuildHelloPacket(ctx,
										       helloOptlen,
										       helloOpt);
							if (ret >= 0) {
								SceNetInternal_689B9D7D(&randVal);
								randVal = randVal /
									  ((0x7fffffffU /
									    ctx->helloInterval) +
									   1U);
								matchingStartHelloTimer(ctx,
											randVal);
							}
						}
						if (ret >= 0) {
							matchingBuildKeepalivePacket(ctx);
							ctx->state = ADHOC_MATCHING_CTX_RUNNING;
							matchingUnlock(lockStatus);
							return 0;
						}
						matchingCalloutTerm(&ctx->calloutContext);
					}
					matchingRecvStop(ctx, &lockStatus);
				}
				matchingEventStop(ctx, &lockStatus);
			}
			matchingSendSocketClose(ctx);
		}
	}
	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingSelectTarget(int id, const struct SceNetInAddr *target,
				    int optlen, const void *opt)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;
	AdhocMatchingPeer *peer;
	int activeCount;

	matchingValidateAddr(target);
	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (target == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		peer = matchingFindPeer(ctx, target);
		if (peer == NULL) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET;
		} else if (optlen < 0 || optlen > s_maxOptLen || (optlen > 0 && opt == NULL)) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN;
		} else {
			activeCount = matchingCountPeers(ctx, ADHOC_MATCHING_PEER_ACCEPT_SENT);
			switch (peer->state) {
			case ADHOC_MATCHING_PEER_INIT:
				if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_PARENT) {
					ret = SCE_NET_ADHOC_MATCHING_ERROR_TARGET_NOT_READY;
				} else if (activeCount + 1 >= ctx->maxNum) {
					ret = SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM;
				} else {
					if (peer->optLen > 0) {
						matchingFree(peer->opt);
						peer->optLen = 0;
						peer->opt = NULL;
					}
					if (optlen > 0) {
						peer->opt = matchingAlloc(optlen);
						if (peer->opt == NULL) {
							ret = SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
							break;
						}
						sceClibMemcpy_safe(peer->opt, opt, optlen);
						peer->optLen = optlen;
					}
					peer->localSessionId++;
					if (peer->localSessionId == 0)
						peer->localSessionId = 1;
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_REQUEST,
								  peer->optLen, peer->opt);
					matchingStartRexmtTimer(ctx, peer);
					matchingSetPeerState(ctx, peer,
							     ADHOC_MATCHING_PEER_REQUEST_SENT);
					peer->rexmtCount = ctx->initCount;
				}
				break;
			case ADHOC_MATCHING_PEER_REQUEST_RECV:
				if (activeCount + 1 >= ctx->maxNum) {
					ret = SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM;
				} else {
					if (peer->optLen > 0) {
						matchingFree(peer->opt);
						peer->optLen = 0;
						peer->opt = NULL;
					}
					if (optlen > 0) {
						peer->opt = matchingAlloc(optlen);
						if (peer->opt == NULL) {
							ret = SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
							break;
						}
						sceClibMemcpy_safe(peer->opt, opt, optlen);
						peer->optLen = optlen;
					}
					peer->localSessionId++;
					if (peer->localSessionId == 0)
						peer->localSessionId = 1;
					matchingSendControlPacket(ctx, peer,
								  ADHOC_MATCHING_PKT_ACCEPT,
								  peer->optLen, peer->opt);
					matchingStartRexmtTimer(ctx, peer);
					matchingSetPeerState(ctx, peer,
							     ADHOC_MATCHING_PEER_ACCEPT_SENT);
					peer->rexmtCount = ctx->initCount;
				}
				break;
			case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			case ADHOC_MATCHING_PEER_REQUEST_SENT:
				ret = SCE_NET_ADHOC_MATCHING_ERROR_REQUEST_IN_PROGRESS;
				break;
			case ADHOC_MATCHING_PEER_ESTABLISHED:
				ret = SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_ESTABLISHED;
				break;
			default:
				break;
			}
		}
	}

	matchingUnlock(lockStatus);
	return ret;
}

static int matchingCancelTarget(int id, const struct SceNetInAddr *target,
				int optlen, const void *opt)
{
	int ret = 0;
	int lockStatus;
	void *optCopy = NULL;
	AdhocMatchingContext *ctx;
	AdhocMatchingPeer *peer;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (target == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		peer = matchingFindPeer(ctx, target);
		if (peer == NULL) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET;
		} else if (optlen < 0 || optlen > s_maxOptLen || (optlen > 0 && opt == NULL)) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN;
		} else {
			int shouldCancel = 1;

			switch (peer->state) {
			case ADHOC_MATCHING_PEER_INIT:
				shouldCancel = 0;
				break;
			case ADHOC_MATCHING_PEER_REQUEST_RECV:
			case ADHOC_MATCHING_PEER_ACCEPT_SENT:
			case ADHOC_MATCHING_PEER_REQUEST_SENT:
			case ADHOC_MATCHING_PEER_ESTABLISHED:
				if (optlen > 0) {
					optCopy = matchingAlloc(optlen);
					if (optCopy == NULL) {
						ret = SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
						shouldCancel = 0;
					} else {
						sceClibMemcpy_safe(optCopy, opt, optlen);
					}
				}
				break;
			default:
				break;
			}

			if (shouldCancel) {
				matchingStopPeerTimers(ctx, peer);
				matchingSendControlPacket(ctx, peer, ADHOC_MATCHING_PKT_CANCEL,
							  optlen, opt);
				matchingSetPeerState(ctx, peer, ADHOC_MATCHING_PEER_INIT);
				if (peer->optLen > 0) {
					matchingFree(peer->opt);
					peer->optLen = 0;
					peer->opt = NULL;
				}
				if (optlen > 0) {
					peer->optLen = optlen;
					peer->opt = optCopy;
				}
			}
		}
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingCancelTarget(int id, const struct SceNetInAddr *target)
{
	return matchingCancelTarget(id, target, 0, NULL);
}

int sceNetAdhocMatchingCancelTargetWithOpt(int id, const struct SceNetInAddr *target,
					   int optlen, const void *opt)
{
	return matchingCancelTarget(id, target, optlen, opt);
}

int sceNetAdhocMatchingSetHelloOpt(int id, int optlen, const void *opt)
{
	int lockStatus;
	int ret;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else if (optlen < 0 || optlen > s_maxHelloOptLen || (optlen > 0 && opt == NULL)) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN;
	} else {
		ret = matchingBuildHelloPacket(ctx, optlen, opt);
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingGetHelloOpt(int id, int *buflen, void *buf)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;
	int optLen;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (buflen == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->mode == SCE_NET_ADHOC_MATCHING_MODE_CHILD) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		optLen = ctx->helloPacketLen - 12;
		if (buf != NULL && optLen > 0) {
			if (optLen > *buflen)
				optLen = *buflen;
			sceClibMemcpy_safe(buf, ctx->helloPacket + 12, optLen);
		}
		*buflen = optLen;
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingGetMembers(int id, SceSize *memberNum, SceNetAdhocMatchingMember *members)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (memberNum == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		matchingCopyMembers(ctx, memberNum, members);
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingSendData(int id, const struct SceNetInAddr *target,
				int datalen, const void *data)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;
	AdhocMatchingPeer *peer;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (target == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		peer = matchingFindPeer(ctx, target);
		if (peer == NULL) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET;
		} else if (datalen <= 0 || datalen > s_maxDataLen) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_DATALEN;
		} else if (data == NULL) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;
		} else if (peer->state != ADHOC_MATCHING_PEER_ESTABLISHED) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_ESTABLISHED;
		} else {
			switch (peer->dataState) {
			case ADHOC_MATCHING_DATA_IDLE:
				if (peer->sendDataLen > 0) {
					matchingFree(peer->sendData);
					peer->sendDataLen = 0;
					peer->sendData = NULL;
				}
				peer->sendData = matchingAlloc(datalen);
				if (peer->sendData == NULL) {
					ret = SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;
				} else {
					sceClibMemcpy_safe(peer->sendData, data, datalen);
					peer->sendDataLen = datalen;
					peer->sendDataSeq++;
					matchingSendDataPacket(ctx, peer, ADHOC_MATCHING_PKT_DATA,
							       peer->sendDataLen, peer->sendData);
					matchingStartDataTimer(ctx, peer);
					matchingSetPeerDataState(ctx, peer,
								 ADHOC_MATCHING_DATA_SENDING);
					peer->dataRexmtCount = ctx->initCount;
				}
				break;
			case ADHOC_MATCHING_DATA_SENDING:
				ret = SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY;
				break;
			default:
				break;
			}
		}
	}

	matchingUnlock(lockStatus);
	return ret;
}

int sceNetAdhocMatchingAbortSendData(int id, const struct SceNetInAddr *target)
{
	int ret = 0;
	int lockStatus;
	AdhocMatchingContext *ctx;
	AdhocMatchingPeer *peer;

	if (s_initialized == 0)
		return SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED;
	if (target == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG;

	lockStatus = matchingLock();
	if (lockStatus < 0)
		return lockStatus;

	ctx = matchingFindContext(id);
	if (ctx == NULL) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID;
	} else if (ctx->state != ADHOC_MATCHING_CTX_RUNNING) {
		ret = SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING;
	} else {
		peer = matchingFindPeer(ctx, target);
		if (peer == NULL) {
			ret = SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET;
		} else {
			matchingStopDataTimer(ctx, peer);
			matchingSetPeerDataState(ctx, peer, ADHOC_MATCHING_DATA_IDLE);
		}
	}

	matchingUnlock(lockStatus);
	return ret;
}
