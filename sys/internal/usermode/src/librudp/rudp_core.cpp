/*
 * rudp_core.cpp: RudpCore orchestrating sockets, contexts, scheduler, aggregator, networker, and polls.
 */
#include "rudp_internal.h"

static int rudpCompareSocketInt(const void *lhs, const void *rhs)
{
	return (*(const int *)lhs < *(const int *)rhs) ? 1 : 0;
}

int rudpComparePtr(const void *lhs, const void *rhs)
{
	return (*(const uintptr_t *)lhs < *(const uintptr_t *)rhs) ? 1 : 0;
}

int rudpCompareU16(const void *lhs, const void *rhs)
{
	return (*(const uint16_t *)lhs < *(const uint16_t *)rhs) ? 1 : 0;
}

int rudpCompareEndpointAlt(const void *lhs, const void *rhs)
{
	const RudpEndpoint *a = (const RudpEndpoint *)lhs;
	const RudpEndpoint *b = (const RudpEndpoint *)rhs;
	if (rudpSockaddrEqual(&a->addr, &b->addr) != 0) {
		if (a->vport == b->vport) {
			return 0;
		}
		return (a->vport < b->vport) ? 1 : 0;
	}
	return (rudpSockaddrLess(&a->addr, &b->addr) != 0) ? 1 : 0;
}

static void destroyContextPtrTree(RudpRbTree *tree)
{
	RudpContextPtrNode *chain = 0;
	while (true) {
		RudpContextPtrNode *node = (RudpContextPtrNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpContextPtrNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroyEndpointTree(RudpRbTree *tree)
{
	RudpEndpointContextNode *chain = 0;
	while (true) {
		RudpEndpointContextNode *node = (RudpEndpointContextNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpEndpointContextNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroyVportTree(RudpRbTree *tree)
{
	RudpVportNode *chain = 0;
	while (true) {
		RudpVportNode *node = (RudpVportNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpVportNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroySocketEntryTree(RudpRbTree *tree)
{
	RudpSocketEntryNode *chain = 0;
	while (true) {
		RudpSocketEntryNode *node = (RudpSocketEntryNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpSocketEntryNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

RudpSocketEntryNode::RudpSocketEntryNode(RudpRbCompareFn ctxCmp, RudpRbCompareFn epCmp, RudpRbCompareFn vpCmp)
	: m_key(-1),
	  m_sock(-1),
	  m_nextEphemeralVport(RUDP_VPORT_EPHEMERAL_BASE),
	  m_contextSet(ctxCmp),
	  m_endpointMap(epCmp),
	  m_vportMap(vpCmp),
	  m_cleanupNext(0)
{
}

RudpSocketEntryNode::~RudpSocketEntryNode()
{
	destroyVportTree(&m_vportMap);
	destroyEndpointTree(&m_endpointMap);
	destroyContextPtrTree(&m_contextSet);
}

RudpCore::RudpCore()
	: m_scheduler(),
	  m_sockets(rudpCompareSocketInt),
	  m_contextManager(&m_scheduler),
	  m_reservedCore(0),
	  m_eventHandler(0),
	  m_eventHandlerArg(0),
	  m_aggregator(&m_scheduler),
	  m_networker(&m_scheduler),
	  m_ioThreadEnabled(false),
	  m_pollManager(),
	  m_handlerId(0)
{
	m_scheduler.setDelegate(this);
	m_contextManager.setDelegate(this);
	m_aggregator.setDelegate(this);
	m_networker.setDelegate(this);
	if (m_scheduler.isValid()) {
		RudpResult res = m_scheduler.registerHandler(&m_handlerId, this);
		if (res.getCode() < 0) {
			m_handlerId = 0;
		}
	}
}

void RudpCore::cancelAll()
{
	m_scheduler.cancelWaits();
	m_pollManager.cancelAll();
	m_contextManager.cancelAllWaits();
}

RudpCore::~RudpCore()
{
	cancelAll();
	m_aggregator.setDelegate(0);
	m_contextManager.clear();
	destroySocketEntryTree(&m_sockets);
}

bool RudpCore::isValid() const
{
	return m_scheduler.isValid() && m_aggregator.isValid() && m_handlerId != 0;
}

void RudpCore::setEventHandler(SceRudpEventHandler handler, void *arg)
{
	m_eventHandler = handler;
	m_eventHandlerArg = arg;
}

SceRudpEventHandler RudpCore::getEventHandler() const
{
	return m_eventHandler;
}

RudpResult RudpCore::setMaxSegmentSize(uint16_t mss)
{
	RudpResult res(0);
	if (m_contextManager.getContextCount() != 0) {
		res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		return res;
	}
	m_aggregator.setMaxSegmentSize(mss);
	m_networker.setBufferSize(mss);
	return res;
}

uint16_t RudpCore::getMaxSegmentSize() const
{
	return m_aggregator.getMaxSegmentSize();
}

RudpResult RudpCore::bind(int ctxId, int sock, uint16_t vport, uint8_t muxMode)
{
	RudpResult res;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &res);
	if (res.getCode() < 0) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (ctx->m_sock != -1) {
		return RudpResult(SCE_RUDP_ERROR_ALREADY_BOUND);
	}

	bool createdSocketEntry = false;
	RudpSocketEntryNode *sockEntry = (RudpSocketEntryNode *)m_sockets.find(&sock);
	if (sockEntry != 0) {
		if (sockEntry->m_contextSet.getCount() != 0) {
			RudpContextPtrNode *firstNode = (RudpContextPtrNode *)sockEntry->m_contextSet.first(0);
			if (firstNode->m_value->m_muxMode != muxMode) {
				return RudpResult(SCE_RUDP_ERROR_INVALID_MUXMODE);
			}
		}
	} else {
		sockEntry = new RudpSocketEntryNode(rudpComparePtr, rudpCompareEndpointAlt, rudpCompareU16);
		if (sockEntry == 0) {
			return RudpResult(SCE_RUDP_ERROR_MEMORY);
		}
		sockEntry->m_key = sock;
		sockEntry->m_sock = sock;
		m_sockets.insert(sockEntry);
		createdSocketEntry = true;
	}

	if (muxMode == SCE_RUDP_MUXMODE_P2P) {
		if (vport == 0) {
			res = SCE_RUDP_ERROR_INVALID_VPORT;
		}
	} else if (muxMode == SCE_RUDP_MUXMODE_CLIENT_SERVER) {
		if (vport == 0) {
			uint16_t startPort = sockEntry->m_nextEphemeralVport;
			uint16_t cand = (startPort == 0xFFFFU) ? RUDP_VPORT_EPHEMERAL_BASE : (uint16_t)(startPort + 1);
			while (cand != startPort) {
				if (sockEntry->m_vportMap.find(&cand) == 0) {
					break;
				}
				cand = (cand == 0xFFFFU) ? RUDP_VPORT_EPHEMERAL_BASE : (uint16_t)(cand + 1);
			}
			if (cand == startPort) {
				res = SCE_RUDP_ERROR_VPORT_EXHAUSTED;
			} else {
				vport = cand;
				sockEntry->m_nextEphemeralVport = cand;
			}
		}
		if (res.getCode() >= 0) {
			if (sockEntry->m_vportMap.find(&vport) != 0) {
				res = SCE_RUDP_ERROR_VPORT_IN_USE;
			} else {
				RudpVportNode *vpNode = new RudpVportNode();
				if (vpNode != 0) {
					vpNode->m_cleanupNext = 0;
					vpNode->m_vport = vport;
					vpNode->m_context = ctx;
					sockEntry->m_vportMap.insert(vpNode);
					res = 0;
				} else {
					res = SCE_RUDP_ERROR_MEMORY;
				}
			}
		}
	} else if (muxMode != SCE_RUDP_MUXMODE_NONE) {
		res = SCE_RUDP_ERROR_INVALID_MUXMODE;
	}

	if (res.getCode() >= 0) {
		RudpResult bindRes = ctx->bind(sock, vport, muxMode);
		res = bindRes.getCode();
		if (res.getCode() >= 0) {
			RudpContextPtrNode *cnode = new RudpContextPtrNode();
			if (cnode != 0) {
				cnode->m_cleanupNext = 0;
				cnode->m_key = ctx;
				cnode->m_value = ctx;
				sockEntry->m_contextSet.insert(cnode);
				res = 0;
			} else {
				res = SCE_RUDP_ERROR_MEMORY;
			}
			if (res.getCode() >= 0 && m_ioThreadEnabled && createdSocketEntry) {
				RudpResult netRes = m_networker.addSocket(ctx->m_sock);
				res = netRes;
				if (res.getCode() < 0) {
					return res;
				}
			}
		}
	}

	if (res.getCode() < 0 && createdSocketEntry) {
		RudpRbNode *found = m_sockets.find(&sockEntry->m_sock);
		if (found != 0) {
			m_sockets.remove(found);
			delete found;
		}
	}
	return res;
}

RudpResult RudpCore::registerRemoteEndpoint(RudpContext *ctx, const RudpSockaddr *addr, uint16_t vport)
{
	RudpResult res;
	uint32_t state = ctx->getState(0);
	if (state >= SCE_RUDP_STATE_SYN_SENT && state != SCE_RUDP_STATE_LISTEN) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (ctx->m_sock == -1) {
		return RudpResult(SCE_RUDP_ERROR_NOT_BOUND);
	}
	RudpSocketEntryNode *sockEntry = (RudpSocketEntryNode *)m_sockets.find(&ctx->m_sock);
	if (sockEntry == 0) {
		return RudpResult(SCE_RUDP_ERROR_NOT_BOUND);
	}
	if (addr != 0) {
		RudpResult epRes = ctx->setRemoteEndpoint(addr, vport);
		res = epRes;
		if (res.getCode() < 0) {
			return res;
		}
	}

	uint8_t muxMode = ctx->m_muxMode;
	if (muxMode > SCE_RUDP_MUXMODE_P2P) {
		if (muxMode > SCE_RUDP_MUXMODE_CLIENT_SERVER) {
			return RudpResult(SCE_RUDP_ERROR_INVALID_MUXMODE);
		}
		if (ctx->m_localEndpoint.vport == 0) {
			return RudpResult(SCE_RUDP_ERROR_INVALID_VPORT);
		}
		if (sockEntry->m_vportMap.find(&ctx->m_localEndpoint.vport) == 0) {
			return RudpResult(SCE_RUDP_ERROR_NOT_BOUND);
		}
		return RudpResult(0);
	}

	uint16_t localVport = ctx->m_localEndpoint.vport;
	if (muxMode == SCE_RUDP_MUXMODE_P2P && localVport == 0) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_VPORT);
	}

	RudpEndpoint keyEp;
	rudpMemcpy(&keyEp.addr, addr, sizeof(keyEp.addr));
	keyEp.vport = localVport;

	RudpEndpointContextNode *existing = (RudpEndpointContextNode *)sockEntry->m_endpointMap.find(&keyEp);
	if (existing != 0) {
		if (existing->m_context != ctx) {
			bool terminating = true;
			existing->m_context->getState(&terminating);
			if (!terminating) {
				return RudpResult(SCE_RUDP_ERROR_ADDR_IN_USE);
			}
			existing->m_context = ctx;
		}
	} else {
		RudpEndpointContextNode *epNode = new RudpEndpointContextNode();
		if (epNode != 0) {
			epNode->m_cleanupNext = 0;
			epNode->m_endpoint = keyEp;
			epNode->m_context = ctx;
			sockEntry->m_endpointMap.insert(epNode);
		}
	}
	return RudpResult(0);
}

RudpResult RudpCore::activate(int ctxId, const RudpSockaddr *addr)
{
	RudpResult res;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &res);
	if (res.getCode() == 0) {
		switch (ctx->getState(0)) {
		case SCE_RUDP_STATE_IDLE:
		case SCE_RUDP_STATE_CLOSED: {
			RudpResult regRes = registerRemoteEndpoint(ctx, addr, ctx->m_localEndpoint.vport);
			res = regRes;
			break;
		}
		case SCE_RUDP_STATE_SYN_SENT:
		case SCE_RUDP_STATE_SYN_RCVD:
			break;
		case SCE_RUDP_STATE_ESTABLISHED:
			res = SCE_RUDP_ERROR_ALREADY_ESTABLISHED;
			break;
		default:
			res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
			break;
		}
	}
	if (res.getCode() == 0) {
		RudpResult actRes = ctx->activate();
		res = actRes;
	}
	return res;
}

RudpResult RudpCore::initiate(int ctxId, const RudpSockaddr *addr, uint16_t vport)
{
	RudpResult res;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &res);
	if (res.getCode() == 0) {
		switch (ctx->getState(0)) {
		case SCE_RUDP_STATE_IDLE:
		case SCE_RUDP_STATE_CLOSED: {
			RudpResult regRes = registerRemoteEndpoint(ctx, addr, vport);
			res = regRes;
			break;
		}
		case SCE_RUDP_STATE_SYN_SENT:
		case SCE_RUDP_STATE_SYN_RCVD:
			break;
		case SCE_RUDP_STATE_ESTABLISHED:
			res = SCE_RUDP_ERROR_ALREADY_ESTABLISHED;
			break;
		default:
			res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
			break;
		}
	}
	if (res.getCode() == 0) {
		RudpResult initRes = ctx->initiate();
		res = initRes;
	}
	return res;
}

RudpResult RudpCore::pollCreate(uint32_t size)
{
	return m_pollManager.createPoll(size, this);
}

RudpResult RudpCore::pollDestroy(int pollId)
{
	return m_pollManager.destroyPoll(pollId);
}

RudpResult RudpCore::pollControl(int pollId, int op, int ctxId, uint16_t events)
{
	RudpResult res(0);
	RudpPoll *poll = m_pollManager.findPoll(pollId);
	if (poll == 0) {
		res = SCE_RUDP_ERROR_INVALID_POLL_ID;
		return res;
	}
	switch (op) {
	case SCE_RUDP_POLL_OP_ADD: {
		int pid = poll->getId();
		RudpResult findRes;
		uint16_t curEvents = 0;
		RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 1, &findRes);
		if (ctx != 0) {
			RudpResult attachRes = ctx->attachPoll(pid);
			findRes = attachRes;
			if (findRes.getCode() == 0) {
				curEvents = ctx->queryPollEvents();
			}
		}
		res = findRes;
		if (res.getCode() == 0) {
			RudpResult addRes = poll->addContext(ctxId, events, curEvents);
			res = addRes;
			if (res.getCode() == SCE_RUDP_ERROR_TOO_MANY_CONTEXTS) {
				onPollDetachContext(ctxId);
			}
		}
		break;
	}
	case SCE_RUDP_POLL_OP_MODIFY: {
		uint16_t curEvents = 0;
		onPollQueryEvents(ctxId, &curEvents);
		RudpResult modRes = poll->modifyContext(ctxId, events, curEvents);
		res = modRes;
		break;
	}
	case SCE_RUDP_POLL_OP_REMOVE: {
		RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 1, 0);
		if (ctx != 0) {
			ctx->detachPoll();
		}
		RudpResult remRes = poll->removeContext(ctxId);
		res = remRes;
		break;
	}
	default:
		res = SCE_RUDP_ERROR_INVALID_ARGUMENT;
		break;
	}
	return res;
}

RudpResult RudpCore::pollWait(int pollId, SceRudpPollEvent *events, uint32_t maxEvents, uint64_t timeoutUsec)
{
	RudpResult res(0);
	RudpPoll *poll = m_pollManager.findPoll(pollId);
	if (poll == 0) {
		res = SCE_RUDP_ERROR_INVALID_POLL_ID;
		return res;
	}
	res = poll->wait(events, maxEvents, timeoutUsec);
	return res;
}

RudpResult RudpCore::pollCancel(int pollId, uint8_t setPendingFlag)
{
	RudpResult res(0);
	RudpPoll *poll = m_pollManager.findPoll(pollId);
	if (poll != 0) {
		poll->cancel(setPendingFlag != 0, false);
	} else {
		res = SCE_RUDP_ERROR_INVALID_POLL_ID;
	}
	return res;
}

RudpResult RudpCore::enableInternalIOThread(uint32_t stackSize, int priority, uint64_t affinityMask)
{
	RudpResult res;
	if (m_contextManager.getContextCount() != 0) {
		res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
	} else if (m_ioThreadEnabled) {
		res = SCE_RUDP_ERROR_THREAD_IN_USE;
	} else {
		m_networker.setStackSize(stackSize);
		m_networker.setPriority(priority);
		m_networker.setAffinityMask(affinityMask);
		RudpResult startRes = m_networker.start();
		res = startRes;
		if (res.getCode() == 0) {
			m_ioThreadEnabled = true;
		}
	}
	return res;
}

RudpResult RudpCore::shutdown()
{
	RudpResult res;
	m_contextManager.clear();
	if (m_networker.m_running) {
		rudpMutexUnlock(&g_rudpMutex);
		RudpResult stopRes = m_networker.stop();
		res = stopRes;
		rudpMutexLock(&g_rudpMutex);
	}
	return res;
}

RudpResult RudpCore::processEvents(uint64_t timeoutUsec)
{
	if (m_ioThreadEnabled) {
		return RudpResult(SCE_RUDP_ERROR_THREAD_IN_USE);
	}
	return m_scheduler.processEvents(timeoutUsec);
}

int RudpCore::netReceived(int sock, const uint8_t *data, uint32_t len, const RudpSockaddr *from, uint8_t fromExternal)
{
	int err = 0;
	RudpBuffer frameBuf((void *)data, len);
	RudpBuffer sliceBuf;
	frameBuf.setLength(len);

	if (fromExternal != 0 && m_ioThreadEnabled) {
		err = SCE_RUDP_ERROR_THREAD_IN_USE;
	} else {
		RudpSocketEntryNode *sockEntry = (RudpSocketEntryNode *)m_sockets.find(&sock);
		if (sockEntry == 0) {
			err = SCE_RUDP_ERROR_INVALID_SOCKET;
		} else if (sockEntry->m_contextSet.getCount() != 0) {
			while (frameBuf.m_length >= 2) {
				uint16_t frameWord = 0;
				frameBuf.readU16(&frameWord);
				uint8_t headerWords = (uint8_t)(frameWord >> 14);
				uint16_t frameLen = (uint16_t)(frameWord & 0x7FFU);
				if (headerWords == 0 || frameLen < (uint16_t)(2 * headerWords) ||
				    (uint32_t)(frameLen - 2) > frameBuf.m_length) {
					err = SCE_RUDP_ERROR_MSG_MALFORMED;
					break;
				}
				uint16_t payloadLen = (uint16_t)(frameLen - 2 * headerWords);
				RudpEndpoint ep;
				rudpMemcpy(&ep.addr, from, sizeof(ep.addr));
				ep.vport = 0;
				uint8_t muxMode = (uint8_t)(headerWords - 1);
				uint16_t dstVport = 0;

				bool malformed = false;
				if (headerWords != 1) {
					if (muxMode == SCE_RUDP_MUXMODE_P2P) {
						if (frameBuf.m_length < 2) {
							malformed = true;
						} else {
							frameBuf.readU16(&ep.vport);
						}
					} else if (muxMode == SCE_RUDP_MUXMODE_CLIENT_SERVER) {
						if (frameBuf.m_length < 4) {
							malformed = true;
						} else {
							frameBuf.readU16(&ep.vport);
							frameBuf.readU16(&dstVport);
						}
					} else {
						err = SCE_RUDP_ERROR_MSG_MALFORMED;
						if (frameBuf.getBytes(0, payloadLen) < 0) {
							err = SCE_RUDP_ERROR_MSG_MALFORMED;
							break;
						}
						continue;
					}
				}
				if (malformed) {
					err = SCE_RUDP_ERROR_MSG_MALFORMED;
					break;
				}

				bool modeMatch = true;
				if (sockEntry->m_contextSet.getCount() != 0) {
					RudpContextPtrNode *firstNode = (RudpContextPtrNode *)sockEntry->m_contextSet.first(0);
					modeMatch = (firstNode->m_value->m_muxMode == muxMode);
				}

				if (modeMatch) {
					RudpContext *targetCtx = 0;
					if (muxMode == SCE_RUDP_MUXMODE_CLIENT_SERVER) {
						RudpVportNode *vpNode = (RudpVportNode *)sockEntry->m_vportMap.find(&dstVport);
						if (vpNode != 0) {
							targetCtx = vpNode->m_context;
						}
					} else {
						RudpEndpointContextNode *epNode = (RudpEndpointContextNode *)sockEntry->m_endpointMap.find(&ep);
						if (epNode != 0) {
							targetCtx = epNode->m_context;
							if (muxMode == SCE_RUDP_MUXMODE_P2P &&
							    (targetCtx->m_localEndpoint.vport != ep.vport ||
							     targetCtx->m_remoteEndpoint.vport != ep.vport)) {
								targetCtx = 0;
							}
						}
					}

					if (targetCtx != 0) {
						if (frameBuf.m_length < payloadLen) {
							err = SCE_RUDP_ERROR_MSG_MALFORMED;
							break;
						}
						sliceBuf.setBuffer(frameBuf.m_data + frameBuf.m_offset, payloadLen);
						sliceBuf.setLength(payloadLen);
						RudpResult rxRes = targetCtx->onPacketReceived(&sliceBuf, muxMode, dstVport, ep.vport, from);
						err = rxRes.getCode();
						if (err < 0) {
							break;
						}
					}
				}

				if (frameBuf.getBytes(0, payloadLen) < 0) {
					err = SCE_RUDP_ERROR_MSG_MALFORMED;
					break;
				}
			}
		}
	}

	g_rudpStatus->m_data.rcvdUdpPackets++;
	g_rudpStatus->m_data.rcvdUdpBytes += len;
	return err;
}

RudpContextManager *RudpCore::getContextManager()
{
	return &m_contextManager;
}

void RudpCore::onContextDestroyed(RudpContext *ctx)
{
	int pollId = ctx->getPollId();
	if (pollId >= 0) {
		RudpPoll *poll = m_pollManager.findPoll(pollId);
		if (poll != 0) {
			RudpResult remRes = poll->removeContext(ctx->m_ctxId);
			remRes.getCode();
		}
		ctx->detachPoll();
	}

	RudpSocketEntryNode *sockEntry = 0;
	if (ctx->m_sock >= 0) {
		sockEntry = (RudpSocketEntryNode *)m_sockets.find(&ctx->m_sock);
	}
	if (sockEntry != 0) {
		if (ctx->m_muxMode == SCE_RUDP_MUXMODE_CLIENT_SERVER) {
			RudpVportNode *vpNode = (RudpVportNode *)sockEntry->m_vportMap.find(&ctx->m_localEndpoint.vport);
			if (vpNode != 0 && vpNode->m_context == ctx) {
				sockEntry->m_vportMap.remove(vpNode);
				delete vpNode;
			}
		} else {
			RudpEndpoint ep;
			rudpMemcpy(&ep.addr, &ctx->m_remoteEndpoint.addr, sizeof(ep.addr));
			ep.vport = ctx->m_remoteEndpoint.vport;
			RudpEndpointContextNode *epNode = (RudpEndpointContextNode *)sockEntry->m_endpointMap.find(&ep);
			if (epNode != 0 && epNode->m_context == ctx) {
				sockEntry->m_endpointMap.remove(epNode);
				delete epNode;
			}
		}

		RudpContextPtrNode *cnode = (RudpContextPtrNode *)sockEntry->m_contextSet.find(&ctx);
		if (cnode != 0) {
			sockEntry->m_contextSet.remove(cnode);
			delete cnode;
		}

		if (sockEntry->m_contextSet.getCount() == 0) {
			int sock = ctx->m_sock;
			m_sockets.remove(sockEntry);
			delete sockEntry;
			if (m_ioThreadEnabled) {
				RudpResult remRes = m_networker.removeSocket(sock);
				remRes.getCode();
			}
			RudpEvent ev;
			ev.handlerId = m_handlerId;
			ev.type = RUDP_EV_SOCKET_RELEASED;
			ev.detail = 0;
			ev.arg = sock;
			RudpResult postRes = m_scheduler.postEvent(&ev);
			postRes.getCode();
		}
	}
	ctx->setEventHandler(0, 0);
}

void RudpCore::notifySocketError(int sock, int errCode)
{
	RudpSocketEntryNode *sockEntry = (RudpSocketEntryNode *)m_sockets.find(&sock);
	if (sockEntry != 0) {
		for (RudpContextPtrNode *node = (RudpContextPtrNode *)sockEntry->m_contextSet.first(0);
		     node != 0;
		     node = (RudpContextPtrNode *)sockEntry->m_contextSet.next(node, 0)) {
			node->m_value->postSocketError(errCode);
		}
	}
}

int RudpCore::sendUdpPacket(int sock, const RudpSockaddr *addr, RudpBuffer *buf)
{
	int err = 0;
	SceNetSockaddr netAddr;
	rudpMemcpy(&netAddr, addr, sizeof(netAddr));
	netAddr.sa_family = addr->sa_family;

	if (!m_ioThreadEnabled) {
		if (m_eventHandler != 0) {
			err = m_eventHandler(SCE_RUDP_EVENT_SEND, sock, buf->m_data + buf->m_offset,
			                     buf->m_length, &netAddr, addr->sa_len, m_eventHandlerArg);
			if (err < 0) {
				if (err == SCE_RUDP_ERROR_WOULDBLOCK) {
					err = 0;
				} else {
					notifySocketError(sock, err);
				}
			}
		}
	} else {
		if (m_eventHandler != 0) {
			if (m_eventHandler(SCE_RUDP_EVENT_DIAG_SENT, sock, buf->m_data + buf->m_offset,
			                   buf->m_length, &netAddr, addr->sa_len, m_eventHandlerArg) < 0) {
				return (int)buf->m_length;
			}
		}
		int sent = rudpNetSendto(sock, buf->m_data + buf->m_offset, buf->m_length, 0, &netAddr);
		if (sent >= 0) {
			err = 0;
		} else if (rudpNetGetLastError() == RUDP_OSAL_EWOULDBLOCK) {
			err = 0;
		} else {
			err = sent;
			notifySocketError(sock, err);
		}
	}

	g_rudpStatus->m_data.sentUdpPackets++;
	g_rudpStatus->m_data.sentUdpBytes += buf->m_length;
	return err;
}

RudpResult RudpCore::onContextRegisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId, uint32_t aggrTimeoutMs)
{
	(void)ctx;
	RudpAggregatorKey key;
	key.sock = sock;
	key.addr = *addr;
	key.reserved = 0;
	return m_aggregator.registerContext(&key, ctxId, aggrTimeoutMs);
}

void RudpCore::onContextUnregisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId)
{
	(void)ctx;
	RudpAggregatorKey key;
	key.sock = sock;
	key.addr = *addr;
	key.reserved = 0;
	m_aggregator.unregisterContext(&key, ctxId);
}

void RudpCore::onContextSendPacket(RudpContext *ctx, RudpHeader *header, RudpBuffer *payload, uint32_t aggrDelayMs, uint32_t *txTimestampOut)
{
	RudpAggregatorKey key;
	key.sock = ctx->m_sock;
	key.addr = ctx->m_remoteEndpoint.addr;
	key.reserved = 0;
	m_aggregator.enqueuePacket(
		&key,
		ctx->m_ctxId,
		ctx->m_muxMode,
		ctx->m_localEndpoint.vport,
		ctx->m_remoteEndpoint.vport,
		header,
		payload,
		aggrDelayMs,
		txTimestampOut);
}

void RudpCore::onContextSendDirect(RudpContext *ctx, uint8_t muxMode, RudpHeader *header, int sock, const RudpEndpoint *srcEp, const RudpEndpoint *dstEp)
{
	(void)ctx;
	uint8_t rawBuf[72];
	RudpBuffer buf(rawBuf, 70);
	uint16_t frameTag = 0;

	if (buf.writeU16(0) < 0) {
		return;
	}
	switch (muxMode) {
	case SCE_RUDP_MUXMODE_NONE:
		frameTag = 0x4000U;
		break;
	case SCE_RUDP_MUXMODE_P2P:
		frameTag = 0x8000U;
		if (buf.writeU16(srcEp->vport) < 0) {
			return;
		}
		break;
	case SCE_RUDP_MUXMODE_CLIENT_SERVER:
		frameTag = 0xC000U;
		if (buf.writeU16(srcEp->vport) < 0 || buf.writeU16(dstEp->vport) < 0) {
			return;
		}
		break;
	default:
		return;
	}

	RudpResult serRes = header->serialize(&buf);
	if (serRes.getCode() >= 0) {
		uint16_t frameWord = sceNetNtohs((uint16_t)(buf.m_length | frameTag));
		buf.writeAt(0, &frameWord, sizeof(frameWord));
		sendUdpPacket(sock, &dstEp->addr, &buf);
	}
}

void RudpCore::onContextCancelAggregator(RudpContext *ctx)
{
	RudpAggregatorKey key;
	key.sock = ctx->m_sock;
	key.addr = ctx->m_remoteEndpoint.addr;
	key.reserved = 0;
	m_aggregator.invalidateContextTimestamps(&key, ctx->m_localEndpoint.vport);
}

void RudpCore::onContextNotifyPoll(RudpContext *ctx, int ctxId, int pollId, uint16_t events)
{
	RudpPoll *poll = m_pollManager.findPoll(pollId);
	if (poll != 0) {
		poll->notifyContext(ctxId, events);
	} else {
		ctx->detachPoll();
	}
}

int RudpCore::onAggregatorSend(RudpAggregator *aggr, int sock, const RudpSockaddr *addr, RudpBuffer *buf)
{
	(void)aggr;
	return sendUdpPacket(sock, addr, buf);
}

int RudpCore::onNetworkerReceived(RudpNetworker *net, int sock, const uint8_t *data, uint32_t len, const SceNetSockaddr *addr, uint32_t addrLen)
{
	(void)net;
	if (m_eventHandler != 0) {
		if (m_eventHandler(SCE_RUDP_EVENT_DIAG_RCVD, sock, data, len, addr, addrLen, m_eventHandlerArg) < 0) {
			return 0;
		}
	}
	RudpSockaddr localAddr;
	rudpMemcpy(&localAddr, addr, sizeof(localAddr));
	localAddr.sa_len = (uint8_t)addrLen;
	localAddr.sa_family = addr->sa_family;
	return netReceived(sock, data, len, &localAddr, 0);
}

void RudpCore::onNetworkerSocketError(RudpNetworker *net, int sock)
{
	(void)net;
	(void)sock;
}

void RudpCore::onPollQueryEvents(int ctxId, uint16_t *outEvents)
{
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 1, 0);
	if (ctx != 0) {
		*outEvents = ctx->queryPollEvents();
	} else {
		*outEvents = SCE_RUDP_POLL_EV_ERROR;
	}
}

void RudpCore::onPollDetachContext(int ctxId)
{
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 1, 0);
	if (ctx != 0) {
		ctx->detachPoll();
	}
}

void RudpCore::onEvent(const RudpEvent *ev)
{
	if (ev->type == RUDP_EV_SOCKET_RELEASED) {
		int sock = ev->arg;
		if (m_sockets.find(&sock) == 0 && m_eventHandler != 0) {
			m_eventHandler(SCE_RUDP_EVENT_SOCKET_RELEASED, sock, 0, 0, 0, 0, m_eventHandlerArg);
		}
	}
}

void RudpCore::onTimer(uint64_t *nextIntervalUsec)
{
	(void)nextIntervalUsec;
}

void RudpCore::onEventPosted(RudpScheduler *scheduler)
{
	(void)scheduler;
	m_networker.wakeUp();
}
