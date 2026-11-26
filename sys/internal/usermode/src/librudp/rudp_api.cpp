/*
 * rudp_api.cpp: SceLibRudp public C API entrypoints and global state.
 */
#include "rudp_internal.h"

RudpCore *g_rudpCore = 0;
RudpContextManager *g_rudpContextManager = 0;
RudpStatus *g_rudpStatus = 0;
bool g_rudpEnding = false;
bool g_rudpInitialized = false;
SceKernelLwMutexWork g_rudpMutex;

struct RudpGlobalLock {
	RudpGlobalLock() { rudpMutexLock(&g_rudpMutex); }
	~RudpGlobalLock() { rudpMutexUnlock(&g_rudpMutex); }
};

int rudpInitInternal(const RudpAllocator *alloc, void *memPool, SceSize memPoolSize)
{
	int ret;

	rudpMutexLock(&g_rudpMutex);
	while (g_rudpEnding) {
		rudpMutexUnlock(&g_rudpMutex);
		rudpMutexLock(&g_rudpMutex);
	}
	if (g_rudpInitialized) {
		ret = SCE_RUDP_ERROR_ALREADY_INITIALIZED;
	} else {
		ret = rudpMemInit(alloc, memPool, memPoolSize);
		if (ret == 0) {
			g_rudpStatus = new RudpStatus();
			if (g_rudpStatus == 0) {
				ret = SCE_RUDP_ERROR_MEMORY;
			} else {
				g_rudpCore = new RudpCore();
				if (g_rudpCore == 0 || !g_rudpCore->isValid()) {
					ret = SCE_RUDP_ERROR_MEMORY;
				} else {
					g_rudpContextManager = g_rudpCore->getContextManager();
					g_rudpInitialized = true;
				}
			}
		}
		if (ret < 0) {
			if (g_rudpStatus != 0) {
				delete g_rudpStatus;
				g_rudpStatus = 0;
			}
			if (g_rudpCore != 0) {
				delete g_rudpCore;
				g_rudpCore = 0;
			}
			g_rudpContextManager = 0;
			rudpMemTerm();
			g_rudpInitialized = false;
		}
	}
	rudpMutexUnlock(&g_rudpMutex);
	return ret;
}

extern "C" {

int sceRudpInit(void *memPool, int memPoolSize)
{
	return rudpInitInternal(0, memPool, (SceSize)memPoolSize);
}

int sceRudpEnableInternalIOThread(uint32_t stackSize, uint32_t priority)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	return g_rudpCore->enableInternalIOThread(stackSize, (int)priority, 0).getCode();
}

int sceRudpEnableInternalIOThread2(uint32_t stackSize, uint32_t priority,
				   uint64_t affinityMask)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	return g_rudpCore->enableInternalIOThread(stackSize, (int)priority, affinityMask).getCode();
}

int sceRudpSetEventHandler(SceRudpEventHandler handler, void *arg)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (handler == 0) {
		return SCE_RUDP_ERROR_NO_EVENT_HANDLER;
	}
	g_rudpCore->setEventHandler(handler, arg);
	return SCE_RUDP_SUCCESS;
}

int sceRudpSetMaxSegmentSize(uint16_t mss)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	return g_rudpCore->setMaxSegmentSize(mss).getCode();
}

int sceRudpGetMaxSegmentSize(uint16_t *mss)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (mss == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	*mss = g_rudpCore->getMaxSegmentSize();
	return SCE_RUDP_SUCCESS;
}

int sceRudpEnd(void)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	g_rudpInitialized = false;
	g_rudpEnding = true;
	if (g_rudpCore != 0) {
		g_rudpCore->cancelAll();
		RudpResult res = g_rudpCore->shutdown();
		if (g_rudpCore != 0) {
			delete g_rudpCore;
		}
		g_rudpCore = 0;
	}
	g_rudpContextManager = 0;
	if (g_rudpStatus != 0) {
		delete g_rudpStatus;
	}
	g_rudpStatus = 0;
	rudpMemTerm();
	g_rudpEnding = false;
	return SCE_RUDP_SUCCESS;
}

int sceRudpProcessEvents(SceRudpUsec timeout)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	return g_rudpCore->processEvents(timeout).getCode();
}

int sceRudpCreateContext(SceRudpContextEventHandler handler, void *arg,
			 int *ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	*ctxId = -1;
	RudpContext *ctx = 0;
	int ret = g_rudpContextManager->createContext(&ctx, g_rudpCore).getCode();
	if (ret < 0) {
		return ret;
	}
	ctx->setEventHandler(handler, arg);
	*ctxId = ctx->m_ctxId;
	return SCE_RUDP_SUCCESS;
}

int sceRudpBind(int ctxId, int soc, uint16_t vport, uint8_t muxMode)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || soc < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	if (muxMode != SCE_RUDP_MUXMODE_P2P && muxMode != SCE_RUDP_MUXMODE_CLIENT_SERVER) {
		return SCE_RUDP_ERROR_INVALID_MUXMODE;
	}
	return g_rudpCore->bind(ctxId, soc, vport, muxMode).getCode();
}

int sceRudpActivate(int ctxId, SceNetSockaddrConstArg to, SceNetSocklen_t toLen)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || to == 0 || toLen > sizeof(RudpSockaddr)) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	if (g_rudpCore->getEventHandler() == 0) {
		return SCE_RUDP_ERROR_NO_EVENT_HANDLER;
	}
	uint8_t family = to->sa_family;
	RudpSockaddr addr;
	rudpMemset(&addr, 0, sizeof(addr));
	rudpMemcpy(&addr, to, toLen);
	addr.sa_len = (uint8_t)toLen;
	addr.sa_family = family;
	return g_rudpCore->activate(ctxId, &addr).getCode();
}

int sceRudpInitiate(int ctxId, SceNetSockaddrConstArg to,
		    SceNetSocklen_t toLen, uint16_t vport)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || to == 0 || toLen > sizeof(RudpSockaddr)) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	if (g_rudpCore->getEventHandler() == 0) {
		return SCE_RUDP_ERROR_NO_EVENT_HANDLER;
	}
	uint8_t family = to->sa_family;
	RudpSockaddr addr;
	rudpMemset(&addr, 0, sizeof(addr));
	rudpMemcpy(&addr, to, toLen);
	addr.sa_len = (uint8_t)toLen;
	addr.sa_family = family;
	return g_rudpCore->initiate(ctxId, &addr, vport).getCode();
}

int sceRudpTerminate(int ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpContextManager->terminateContext(ctxId).getCode();
}

int sceRudpWrite(int ctxId, const void *data, size_t len, uint8_t flags)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || data == 0 || len == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	RudpBuffer buf((void *)data, (uint32_t)len);
	buf.setLength((uint32_t)len);
	return ctx->write(&buf, flags).getCode();
}

int sceRudpRead(int ctxId, void *data, size_t len, uint8_t flags,
		SceRudpReadInfo *info)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || data == 0 || (info != 0 && info->size > sizeof(SceRudpReadInfo))) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	RudpBuffer buf(data, (uint32_t)len);
	int ret;
	if (info != 0) {
		SceRudpReadInfo localInfo;
		ret = ctx->read(&buf, flags, &localInfo).getCode();
		localInfo.size = info->size;
		rudpMemcpy(info, &localInfo, localInfo.size);
	} else {
		ret = ctx->read(&buf, flags, 0).getCode();
	}
	return ret;
}

SceSSize sceRudpGetSizeWritable(int ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return ctx->getSizeWritable();
}

SceSSize sceRudpGetSizeReadable(int ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return ctx->getSizeReadable();
}

int sceRudpFlush(int ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return ctx->flush().getCode();
}

int sceRudpNetReceived(int soc, const uint8_t *data, size_t dataLen,
		       SceNetSockaddrConstArg from, SceNetSocklen_t fromLen)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (soc < 0) {
		return SCE_RUDP_ERROR_INVALID_SOCKET;
	}
	if (data == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	if (dataLen == 0) {
		return SCE_RUDP_SUCCESS;
	}
	if (from == 0 || fromLen > sizeof(RudpSockaddr)) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	uint8_t family = from->sa_family;
	RudpSockaddr addr;
	rudpMemset(&addr, 0, sizeof(addr));
	rudpMemcpy(&addr, from, fromLen);
	addr.sa_len = (uint8_t)fromLen;
	addr.sa_family = family;
	return g_rudpCore->netReceived(soc, data, (uint32_t)dataLen, &addr, 1);
}

int sceRudpSetOption(int ctxId, int option, const void *optVal, size_t optLen)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || optVal == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	int ret = SCE_RUDP_ERROR_INVALID_ARGUMENT;
	const uint32_t *optU32 = (const uint32_t *)optVal;
	switch (option) {
	case SCE_RUDP_OPTION_MAX_PAYLOAD:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			uint32_t mss = *optU32 + RUDP_HEADER_OVERHEAD;
			if (mss <= g_rudpCore->getMaxSegmentSize()) {
				ret = ctx->setMaxSegmentSize(mss).getCode();
			} else {
				ret = SCE_RUDP_ERROR_PAYLOAD_TOO_LARGE;
			}
		}
		break;
	case SCE_RUDP_OPTION_SNDBUF:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setSendBufferSize(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_RCVBUF:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setRecvBufferSize(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_NODELAY:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setNoDelay(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_DELIVERY_CRITICAL:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setDeliveryCritical(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_ORDER_CRITICAL:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setOrderCritical(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_NONBLOCK:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setNonBlock(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_STREAM:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setStreamMode(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_CONNECTION_TIMEOUT:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setConnectionTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setCloseWaitTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_AGGREGATION_TIMEOUT:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setAggregationTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_SACK:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setSackEnabled(*optU32 != 0).getCode();
		}
		break;
	case SCE_RUDP_OPTION_COOKIE_TIMEOUT:
		if (ctx->getState(0) != SCE_RUDP_STATE_IDLE) {
			return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		}
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setCookieTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_READ_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setReadTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_WRITE_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setWriteTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_FLUSH_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setFlushTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setKeepAliveInterval(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->setKeepAliveTimeout(*optU32).getCode();
		}
		break;
	case SCE_RUDP_OPTION_INTERNAL_DEBUG:
		if (optLen == sizeof(uint32_t)) {
			ctx->setDebugFlag((uint8_t)(*optU32 != 0));
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	default:
		ret = SCE_RUDP_ERROR_INVALID_OPTION;
		break;
	}
	return ret;
}

int sceRudpGetOption(int ctxId, int option, void *optVal, size_t optLen)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || optVal == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	int ret = SCE_RUDP_ERROR_INVALID_ARGUMENT;
	uint32_t *optU32 = (uint32_t *)optVal;
	uint32_t timeoutMs;
	switch (option) {
	case SCE_RUDP_OPTION_MAX_PAYLOAD:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getMaxSegmentSize() - RUDP_HEADER_OVERHEAD;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_SNDBUF:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getSendBufferSize();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_RCVBUF:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getRecvBufferSize();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_NODELAY:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getNoDelay() ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_DELIVERY_CRITICAL:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getDeliveryCritical() ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_ORDER_CRITICAL:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getOrderCritical() ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_NONBLOCK:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getNonBlock() ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_STREAM:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = (ctx->getStreamMode() == true) ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_CONNECTION_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getConnectionTimeout();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getCloseWaitTimeout();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_AGGREGATION_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getAggregationTimeout();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_SACK:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getSackEnabled() ? 1 : 0;
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_COOKIE_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getCookieTimeout();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_LAST_ERROR:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = (uint32_t)ctx->getLastError();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_READ_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->getReadTimeout(&timeoutMs).getCode();
			if (ret >= 0) {
				*optU32 = timeoutMs;
				ret = SCE_RUDP_SUCCESS;
			}
		}
		break;
	case SCE_RUDP_OPTION_WRITE_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->getWriteTimeout(&timeoutMs).getCode();
			if (ret >= 0) {
				*optU32 = timeoutMs;
				ret = SCE_RUDP_SUCCESS;
			}
		}
		break;
	case SCE_RUDP_OPTION_FLUSH_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			ret = ctx->getFlushTimeout(&timeoutMs).getCode();
			if (ret >= 0) {
				*optU32 = timeoutMs;
				ret = SCE_RUDP_SUCCESS;
			}
		}
		break;
	case SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getKeepAliveInterval();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	case SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT:
		if (optLen == sizeof(uint32_t)) {
			*optU32 = ctx->getKeepAliveTimeout();
			ret = SCE_RUDP_SUCCESS;
		}
		break;
	default:
		ret = SCE_RUDP_ERROR_INVALID_OPTION;
		break;
	}
	return ret;
}

int sceRudpGetLocalInfo(int ctxId, int *soc, SceNetSockaddrArg addr,
			SceNetSocklen_t *addrLen, uint16_t *vport,
			uint8_t *muxMode)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return ctx->getLocalInfo(soc, addr, (uint32_t *)addrLen, vport, muxMode).getCode();
}

int sceRudpGetRemoteInfo(int ctxId, SceNetSockaddrArg addr,
			 SceNetSocklen_t *addrLen, uint16_t *vport)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return ctx->getRemoteInfo(addr, (int *)addrLen, vport).getCode();
}

int sceRudpGetContextStatus(int ctxId, SceRudpContextStatus *status,
			    size_t statusSize)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0 || status == 0 || statusSize == 0 || statusSize > sizeof(SceRudpContextStatus)) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	SceRudpContextStatus localStatus;
	RudpProtocolStats stats;
	localStatus.state = ctx->getState(0);
	localStatus.parentId = ctx->getParentId();
	localStatus.children = ctx->getChildCount();
	ctx->getProtocolStats(&stats);
	localStatus.lostPackets = stats.lostPackets;
	localStatus.sentPackets = stats.sentPackets;
	localStatus.rcvdPackets = stats.rcvdPackets;
	localStatus.sentBytes = stats.sentBytes;
	localStatus.rcvdBytes = stats.rcvdBytes;
	localStatus.retransmissions = stats.retransmissions;
	localStatus.rtt = (uint32_t)stats.smoothedRttUsec;
	rudpMemcpy(status, &localStatus, statusSize);
	return SCE_RUDP_SUCCESS;
}

int sceRudpPollCreate(size_t size)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (size == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpCore->pollCreate((uint32_t)size).getCode();
}

int sceRudpPollDestroy(int pollId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (pollId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpCore->pollDestroy(pollId).getCode();
}

int sceRudpPollControl(int pollId, int op, int ctxId, uint16_t events)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (pollId < 0 || ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpCore->pollControl(pollId, op, ctxId, events).getCode();
}

int sceRudpPollWait(int pollId, SceRudpPollEvent *events, size_t eventLen,
		    SceRudpUsec timeout)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (pollId < 0 || events == 0 || eventLen == 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpCore->pollWait(pollId, events, (uint32_t)eventLen, timeout).getCode();
}

int sceRudpPollCancel(int pollId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (pollId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	return g_rudpCore->pollCancel(pollId, 1).getCode();
}

int sceRudpGetStatus(SceRudpStatus *status, size_t statusSize)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (status == 0 || statusSize == 0 || statusSize > sizeof(SceRudpStatus)) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	rudpMemcpy(status, &g_rudpStatus->m_data, statusSize);
	status->currentContexts = g_rudpContextManager->getContextCount();
	rudpMemGetStats(&status->allocs, &status->frees, &status->memCurrent, &status->memPeak);
	return SCE_RUDP_SUCCESS;
}

int sceRudpGetNumberOfPacketsToRead(int ctxId)
{
	RudpGlobalLock lock;

	if (!g_rudpInitialized) {
		return SCE_RUDP_ERROR_NOT_INITIALIZED;
	}
	if (ctxId < 0) {
		return SCE_RUDP_ERROR_INVALID_ARGUMENT;
	}
	RudpResult findRes;
	RudpContext *ctx = g_rudpContextManager->findContext(ctxId, 0, &findRes);
	if (findRes.getCode() < 0) {
		return findRes.getCode();
	}
	return (int)ctx->getPacketsToRead();
}

} /* extern "C" */
