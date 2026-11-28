/*
 * np_websocket_net.cpp: Network, DNS resolver, epoll, and SceSslInternal TLS
 * transport layer for SceNpWebSocket (SceNpWsNet / SceNpWsNetImpl).
 * Sony path: vsh/app/common/src/np/np_websocket/
 * 3.65-CEX: 0x813FFE68..0x8140211B
 */
#include "np_websocket_internal.h"
#include <std/new>

#define NP_WS_SSL_OPTION_FLAGS		570433531U
#define NP_WS_SSL_SESSION_FLAGS		0x40000U
#define NP_WS_SSL_CTRL_CMD_MODE		44

namespace sce {
namespace Np {
namespace WebSocket {

namespace {

typedef int (*WsSslVerifyCallback)(int unused, void *certChain, int certCount, void *arg,
				   int verifyErr);

struct WsSslVerifyConfig {
	SceUInt32 verifyFlags;
	const char *hostName;
	SceUInt32 *outCertError;
	SceUInt32 *outSslError;
	int (*verifyCb)(void *certChain, int certCount, void *arg, int verifyErr);
	void *verifyArg;
	void *(*allocFn)(void *ctx, SceSize size);
	void *(*reallocFn)(void *ctx, void *ptr, SceSize size);
	void (*freeFn)(void *ctx, void *ptr);
	void *allocCtx;
};

struct WsSslSession {
	int reserved;
	SceNetId socketId;
	int status;
	int unusedFields[13];
	int connectState;
	void *sslCtx;
	void *sslObj;
	const char *hostName;
	WsSslVerifyConfig *verifyConfig;
	int lastReadErr;
	int lastWriteErr;
	SceUInt32 certError;
	SceUInt32 sslError;
	int trailing;
};

static NpPushAllocator *s_netAllocator = NULL;
static WsSslSession *s_activeSslSession = NULL;
static WsSslVerifyCallback s_sslVerifyCallback = NULL;
static void *s_sslVerifyArg = NULL;
static SceUInt32 s_sslVerifyFlags = 0x3FU;

/* 0x8140135C */
static void *SslAlloc(SceSize size)
{
	if (s_netAllocator == NULL)
		return NULL;
	return SceNpCommonPs4_AA980113(s_netAllocator, size);
}

/* 0x81401388 */
static void *SslRealloc(void *pointer, SceSize size)
{
	if (s_netAllocator == NULL)
		return NULL;
	return SceNpCommonPs4_67BBE5F9(s_netAllocator, pointer, size);
}

/* 0x814013B8 */
static void SslFree(void *pointer)
{
	if (s_netAllocator != NULL && pointer != NULL)
		SceNpCommonPs4_6B428829(s_netAllocator, pointer);
}

/* 0x814013DE */
static void *ResolverAllocCallback(SceSize size, SceNetId rid, const char *name, void *user)
{
	(void)rid;
	(void)name;
	(void)user;
	return SslAlloc(size);
}

/* 0x814013F4 */
static void ResolverFreeCallback(void *ptr, SceNetId rid, const char *name, void *user)
{
	(void)rid;
	(void)name;
	(void)user;
	SslFree(ptr);
}

/* 0x8140140A */
static int DefaultSslVerifyCallback(int unused, void *certChain, int certCount, void *arg,
				    int verifyErr)
{
	(void)unused;
	(void)certChain;
	(void)arg;
	(void)verifyErr;
	return (certCount == 0) ? 0 : -1;
}

/* 0x8140209E */
static int SslVerifyBridge(void *certChain, int certCount, void *arg, int verifyErr)
{
	if (s_sslVerifyCallback != NULL)
		return s_sslVerifyCallback(0, certChain, certCount, arg, verifyErr);
	return 0;
}

/* 0x814020E0 */
static void *SslAllocBridge(void *ctx, SceSize size)
{
	(void)ctx;
	return SslAlloc(size);
}

/* 0x814020F2 */
static void *SslReallocBridge(void *ctx, void *ptr, SceSize size)
{
	(void)ctx;
	return SslRealloc(ptr, size);
}

/* 0x81402108 */
static void SslFreeBridge(void *ctx, void *ptr)
{
	(void)ctx;
	SslFree(ptr);
}

/* 0x81401CA8 */
static void SslMsgTraceStub(void)
{
}

/* 0x81401E38 */
static void SslInfoCallback(void *sslObj, SceInt16 where, SceUInt8 alertCode)
{
	(void)sslObj;
	(void)where;
	(void)alertCode;
}

/* 0x8140142C */
static void InitSslSessionFields(WsSslSession *session)
{
	session->status = -1;
	session->socketId = -1;
	session->sslCtx = NULL;
	session->sslObj = NULL;
	session->hostName = NULL;
	session->verifyConfig = NULL;
	session->connectState = -1;
	session->lastReadErr = -1;
	session->lastWriteErr = -1;
}

/* 0x81401BA4 */
static void ReleaseActiveSslSession(void)
{
	if (s_activeSslSession != NULL) {
		SslFree(s_activeSslSession);
		s_activeSslSession = NULL;
		s_sslVerifyCallback = NULL;
		s_sslVerifyArg = NULL;
	}
}

/* 0x8140147C */
static int SslOpenSession(int unusedCtx, SceNetId sock, const char *hostName)
{
	(void)unusedCtx;
	if (s_activeSslSession != NULL)
		return SCE_NP_WS_ERROR_SSL_ALREADY_OPEN;

	WsSslSession *session = static_cast<WsSslSession *>(SslAlloc(sizeof(WsSslSession)));
	if (session == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	sceClibMemset(session, 0, sizeof(*session));
	InitSslSessionFields(session);
	session->socketId = sock;
	session->hostName = hostName;
	s_activeSslSession = session;
	return 1;
}

/* 0x81401502 */
static int SslDisableVerifyFlags(int sslHandle, SceUInt32 flagMask)
{
	(void)sslHandle;
	s_sslVerifyFlags &= ~flagMask;
	return 0;
}

/* 0x8140152A */
static int SslSetVerifyCallback(int sslHandle, WsSslVerifyCallback callback, void *arg)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;
	s_sslVerifyCallback = callback;
	s_sslVerifyArg = arg;
	return 0;
}

/* 0x8140156E */
static int SslHandshakeSession(int sslHandle)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	WsSslSession *session = s_activeSslSession;
	void *method = NULL;
	int ret = SceSslInternal_2185BB05(&method);
	if (ret < 0)
		return ret;

	void *clientMethod = SceSslInternal_A374CD7B();
	session->sslCtx = SceSslInternal_E746465E(method, clientMethod);
	if (session->sslCtx == NULL)
		return SceSslInternal_461C5990(1, SCE_NP_WS_ERROR_SSL_HANDSHAKE);

	SceUInt8 randomSeed[64];
	ret = sceKernelGetRandomNumber(randomSeed, sizeof(randomSeed));
	if (ret >= 0) {
		int seedErr = SceSslInternal_67FB1CD3(session->sslCtx, randomSeed,
						      sizeof(randomSeed));
		if (seedErr != 0)
			ret = SceSslInternal_461C5990(0, seedErr);
	}

	if (ret >= 0) {
		SceSslInternal_FBEB1FD7(session->sslCtx, NP_WS_SSL_OPTION_FLAGS);
		session->verifyConfig =
			static_cast<WsSslVerifyConfig *>(SslAlloc(sizeof(WsSslVerifyConfig)));
		if (session->verifyConfig == NULL)
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	if (ret >= 0) {
		sceClibMemset(session->verifyConfig, 0, sizeof(*session->verifyConfig));
		session->verifyConfig->verifyFlags = s_sslVerifyFlags;
		session->verifyConfig->hostName = session->hostName;
		session->verifyConfig->outCertError = &session->certError;
		session->verifyConfig->outSslError = &session->sslError;
		session->verifyConfig->verifyCb = SslVerifyBridge;
		session->verifyConfig->verifyArg = s_sslVerifyArg;
		session->verifyConfig->allocFn = SslAllocBridge;
		session->verifyConfig->reallocFn = SslReallocBridge;
		session->verifyConfig->freeFn = SslFreeBridge;
		session->verifyConfig->allocCtx = NULL;
		ret = SceHttp_A9A39BA1(session->sslCtx, session->verifyConfig);
	}

	if (ret >= 0) {
		SceSslInternal_CC7F4211(session->sslCtx, NP_WS_SSL_CTRL_CMD_MODE, 0, NULL);
		session->sslObj = SceSslInternal_D28834E6(session->sslCtx);
		if (session->sslObj == NULL)
			ret = SceSslInternal_461C5990(1, SCE_NP_WS_ERROR_SSL_HANDSHAKE);
	}

	if (ret >= 0) {
		SceSslInternal_0DB6C206(session->sslObj, NP_WS_SSL_SESSION_FLAGS);
		void *bio = SceSslInternal_FFE2ED51(session->socketId, 0);
		SceSslInternal_4A1557A7(session->sslObj, bio);
		SceSslInternal_F2C9B744(session->sslObj, SslInfoCallback);
		SceSslInternal_F71DB06F(session->sslObj, SslMsgTraceStub, NULL);
		SceSslInternal_CEE46B33(session->sslObj);

		bool done = false;
		while (!done && ret >= 0) {
			*sceNetErrnoLoc() = 0;
			int ioRet = SceSslInternal_E5EDD91B(session->sslObj);
			int sslErr = SceSslInternal_2F0FC3CD(session->sslObj, ioRet);
			switch (sslErr) {
			case 0:
				done = true;
				ret = 0;
				break;
			case 1:
				ret = SceSslInternal_461C5990(1, SCE_NP_WS_ERROR_SSL_HANDSHAKE);
				break;
			case 2:
			case 3:
			case 5: {
				int netErr = *sceNetErrnoLoc();
				ret = (netErr > 0)
					      ? static_cast<int>(static_cast<SceUInt32>(netErr) |
								 0x80410100U)
					      : SCE_NP_WS_ERROR_SSL_HANDSHAKE;
				break;
			}
			case 6:
				ret = SCE_NP_WS_ERROR_SSL_HANDSHAKE;
				break;
			default:
				break;
			}
		}

		if (done && ret >= 0)
			return 0;
	}

	if (session->sslObj != NULL) {
		SceSslInternal_19B2BB58(session->sslObj);
		session->sslObj = NULL;
	}
	if (session->sslCtx != NULL) {
		SceSslInternal_EF9EBAC7(session->sslCtx);
		session->sslCtx = NULL;
	}
	if (session->verifyConfig != NULL) {
		SslFree(session->verifyConfig);
		session->verifyConfig = NULL;
	}
	return ret;
}

/* 0x814018F8 */
static int SslWriteSession(int sslHandle, const void *buf, SceSize len)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	WsSslSession *session = s_activeSslSession;
	int ioRet = sceSslInternalWrite(session->sslObj, buf, static_cast<int>(len));
	int sslErr = SceSslInternal_2F0FC3CD(session->sslObj, ioRet);

	switch (sslErr) {
	case 0:
		return ioRet;
	case 1: {
		int mapped = SceSslInternal_461C5990(1, SCE_NP_WS_ERROR_SSL_WRITE);
		if (mapped == SCE_NP_WS_ERROR_SSL_WRITE && ioRet < 0)
			return ioRet;
		return mapped;
	}
	case 2:
	case 3:
	case 5:
		return (ioRet < 0) ? ioRet : SCE_NP_WS_ERROR_SSL_WRITE;
	case 6:
		return SCE_NP_WS_ERROR_SSL_WRITE;
	default:
		return ioRet;
	}
}

/* 0x81401A14 */
static int SslReadSession(int sslHandle, void *buf, SceSize len)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	WsSslSession *session = s_activeSslSession;
	int ioRet = SceSslInternal_4F781BB6(session->sslObj, buf, static_cast<int>(len));
	int sslErr = SceSslInternal_2F0FC3CD(session->sslObj, ioRet);

	switch (sslErr) {
	case 0:
		return ioRet;
	case 1: {
		int mapped = SceSslInternal_461C5990(1, SCE_NP_WS_ERROR_SSL_READ);
		if (mapped == SCE_NP_WS_ERROR_SSL_READ && ioRet < 0)
			return ioRet;
		return mapped;
	}
	case 2:
	case 3:
	case 5:
		return (ioRet < 0) ? ioRet : SCE_NP_WS_ERROR_SSL_READ;
	case 6:
		return SCE_NP_WS_ERROR_SSL_READ;
	default:
		return ioRet;
	}
}

/* 0x81401B30 */
static int SslShutdownSession(int sslHandle)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	WsSslSession *session = s_activeSslSession;
	if (session->sslObj != NULL) {
		SceSslInternal_EC9FAF42(session->sslObj);
		SceSslInternal_19B2BB58(session->sslObj);
		session->sslObj = NULL;
		SceSslInternal_EF9EBAC7(session->sslCtx);
		session->sslCtx = NULL;
		SslFree(session->verifyConfig);
		session->verifyConfig = NULL;
	}
	return 0;
}

/* 0x81401BF2 */
static int SslCloseSession(int sslHandle)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	WsSslSession *session = s_activeSslSession;
	if (session->sslObj != NULL) {
		SceSslInternal_19B2BB58(session->sslObj);
		SceSslInternal_EF9EBAC7(session->sslCtx);
		SslFree(session->verifyConfig);
	}
	ReleaseActiveSslSession();
	return 0;
}

/* 0x81401C4C */
static int SslCheckPending(int sslHandle, bool *outHasPending)
{
	if (s_activeSslSession == NULL || sslHandle != 1)
		return SCE_NP_WS_ERROR_INTERNAL;
	if (s_activeSslSession->sslObj != NULL)
		*outHasPending = (SceSslInternal_D776E2F8(s_activeSslSession->sslObj) > 0);
	else
		*outHasPending = false;
	return 0;
}

} /* anonymous namespace */

/* ============================================================================
 * WsNetImpl (0x81400C9A..0x8140135B)
 * ============================================================================ */

WsNetImpl::WsNetImpl(NpPushAllocator *allocator) : m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_sslLock);
	s_netAllocator = allocator;
}

WsNetImpl::~WsNetImpl()
{
	Finalize();
	SceNpCommonPs4_294F0B1A(&m_sslLock);
}

/* 0x81400D1E */
int WsNetImpl::Init(void)
{
	int ret = SceNpCommonPs4_8AA7E995(&m_sslLock, "SceNpWsNetImpl", 0);
	if (ret >= 0)
		m_initialized = true;
	else
		Finalize();
	return ret;
}

/* 0x81400D64 */
void WsNetImpl::Finalize(void)
{
	if (SceNpCommonPs4_C78F5DF5(&m_sslLock) != 0)
		SceNpCommonPs4_63EED478(&m_sslLock);
	m_initialized = false;
}

/* 0x81400DA2 */
SceNetId WsNetImpl::CreateResolver(const char *name)
{
	SceNetResolverParam param;
	param.allocate = ResolverAllocCallback;
	param.free = ResolverFreeCallback;
	param.user = this;
	return sceNetResolverCreate(name, &param, 0);
}

/* 0x81400E02 */
int WsNetImpl::ResolveName(SceNetId resolverId, const char *hostname, SceNetInAddr *outAddr,
			   int timeoutUsec, int retryCount, int flags)
{
	return sceNetResolverStartNtoa(resolverId, hostname, outAddr, timeoutUsec, retryCount,
				       flags);
}

/* 0x81400E48 */
int WsNetImpl::DestroyResolver(SceNetId resolverId)
{
	return sceNetResolverDestroy(resolverId);
}

/* 0x81400E66 */
int WsNetImpl::AbortResolver(SceNetId resolverId, int flags)
{
	int ret = sceNetResolverAbort(resolverId, flags);
	return (ret == SCE_NET_ERROR_EABORT_MASKED) ? 0 : ret;
}

/* 0x81400E9E */
const char *WsNetImpl::InetNtop(int af, const void *src, char *dst, SceSize size)
{
	return sceNetInetNtop(af, src, dst, size);
}

/* 0x81400ECC */
int WsNetImpl::InetPton(int af, const char *src, void *dst)
{
	return sceNetInetPton(af, src, dst);
}

/* 0x81400EF2 */
SceNetId WsNetImpl::CreateTcpSocket(const char *name)
{
	return sceNetSocket(name, SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
}

/* 0x81400F1C */
int WsNetImpl::ShutdownSocket(SceNetId sock, int how)
{
	return sceNetShutdown(sock, how);
}

/* 0x81400F3E */
int WsNetImpl::CloseSocket(SceNetId sock)
{
	return sceNetSocketClose(sock);
}

/* 0x81400F5C */
int WsNetImpl::AbortSocket(SceNetId sock, int flags)
{
	int ret = sceNetSocketAbort(sock, flags);
	return (ret == SCE_NET_ERROR_EABORT_MASKED) ? 0 : ret;
}

/* 0x81400F94 */
int WsNetImpl::SetSockOpt(SceNetId sock, int level, int optName, const void *optVal,
			  SceSize optLen)
{
	return sceNetSetsockopt(sock, level, optName, optVal, optLen);
}

/* 0x81400FCE */
int WsNetImpl::GetSockOpt(SceNetId sock, int level, int optName, void *optVal, SceSize *optLen)
{
	return sceNetGetsockopt(sock, level, optName, optVal, optLen);
}

/* 0x81401008 */
int WsNetImpl::ConnectSocket(SceNetId sock, const SceNetSockaddr *addr, SceSize addrLen)
{
	return sceNetConnect(sock, addr, addrLen);
}

/* 0x8140102E */
int WsNetImpl::RecvSocket(SceNetId sock, void *buf, SceSize len, int flags)
{
	return sceNetRecv(sock, buf, len, flags);
}

/* 0x8140105C */
int WsNetImpl::SendSocket(SceNetId sock, const void *buf, SceSize len, int flags)
{
	return sceNetSend(sock, buf, len, flags);
}

/* 0x8140108A */
int WsNetImpl::CreateSslSession(int unusedCtx, SceNetId sock, const char *hostName)
{
	return SslOpenSession(unusedCtx, sock, hostName);
}

/* 0x814010B0 */
int WsNetImpl::DestroySslSession(int sslHandle)
{
	return SslCloseSession(sslHandle);
}

/* 0x814010CE */
int WsNetImpl::PerformSslHandshake(int sslHandle)
{
	return SslHandshakeSession(sslHandle);
}

/* 0x814010EC */
int WsNetImpl::RecvSsl(int sslHandle, void *buf, SceSize len)
{
	Push::ScopedLock lock(&m_sslLock);
	return SslReadSession(sslHandle, buf, len);
}

/* 0x8140114A */
int WsNetImpl::CheckSslPending(int sslHandle, bool *outHasPending)
{
	Push::ScopedLock lock(&m_sslLock);
	int ret = SslCheckPending(sslHandle, outHasPending);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814011AE */
int WsNetImpl::SendSsl(int sslHandle, const void *buf, SceSize len)
{
	Push::ScopedLock lock(&m_sslLock);
	return SslWriteSession(sslHandle, buf, len);
}

/* 0x8140120C */
int WsNetImpl::ShutdownSsl(int sslHandle)
{
	Push::ScopedLock lock(&m_sslLock);
	return SslShutdownSession(sslHandle);
}

/* 0x8140125E */
int WsNetImpl::ConfigureSslVerify(int sslHandle, bool disableVerify)
{
	if (disableVerify)
		return SslSetVerifyCallback(sslHandle, DefaultSslVerifyCallback, NULL);
	return SslDisableVerifyFlags(sslHandle, 1U);
}

/* 0x8140129E */
SceNetId WsNetImpl::CreateEpoll(const char *name)
{
	return sceNetEpollCreate(name, 0);
}

/* 0x814012C0 */
int WsNetImpl::DestroyEpoll(SceNetId epollId)
{
	return sceNetEpollDestroy(epollId);
}

/* 0x814012DE */
int WsNetImpl::AbortEpoll(SceNetId epollId, int flags)
{
	return sceNetEpollAbort(epollId, flags);
}

/* 0x81401300 */
int WsNetImpl::ControlEpoll(SceNetId epollId, int op, SceNetId fd, SceNetEpollEvent *event)
{
	return sceNetEpollControl(epollId, op, fd, event);
}

/* 0x8140132E */
int WsNetImpl::WaitEpoll(SceNetId epollId, SceNetEpollEvent *events, int maxEvents,
			 int timeoutUsec)
{
	return sceNetEpollWait(epollId, events, maxEvents, timeoutUsec);
}

/* ============================================================================
 * WsNet (0x813FFE68..0x81400C99)
 * ============================================================================ */

WsNet::WsNet(WsNetImpl *impl)
	: m_impl(impl),
	  m_aborted(0),
	  m_nonBlocking(0),
	  m_resolverId(0),
	  m_socketId(0),
	  m_sslHandle(0),
	  m_resolverParam(0),
	  m_sslContextParam(0),
	  m_tlsMode(0),
	  m_useTls(0),
	  m_disableVerify(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	sceClibMemset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
}

WsNet::~WsNet()
{
	Finalize();
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813FFF1A */
int WsNet::Init(int resolverParam, int sslContextParam)
{
	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpWsNet", 0);
	if (ret >= 0) {
		m_resolverParam = resolverParam;
		m_sslContextParam = sslContextParam;
	} else {
		Finalize();
	}
	return ret;
}

/* 0x813FFF64 */
void WsNet::Finalize(void)
{
	if (m_resolverId > 0) {
		m_impl->DestroyResolver(m_resolverId);
		m_resolverId = 0;
	}
	if (m_useTls != 0 && m_sslHandle > 0) {
		m_impl->ShutdownSsl(m_sslHandle);
		m_impl->DestroySslSession(m_sslHandle);
		m_sslHandle = 0;
	}
	if (m_socketId > 0) {
		m_impl->CloseSocket(m_socketId);
		m_socketId = 0;
	}
	if (SceNpCommonPs4_C78F5DF5(&m_mutex) != 0)
		SceNpCommonPs4_63EED478(&m_mutex);
}

/* 0x81400026 */
void WsNet::SetNonBlocking(int nonBlocking)
{
	m_nonBlocking = nonBlocking;
	SceNetId sock = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		sock = m_socketId;
	}
	if (sock != 0) {
		m_impl->SetSockOpt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &m_nonBlocking,
				   sizeof(m_nonBlocking));
	}
}

/* 0x814000B4 */
int WsNet::GetSocketError(void)
{
	SceNetId sock = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		sock = m_socketId;
	}
	if (sock == 0)
		return 0;

	int sockErr = 0;
	SceSize optLen = sizeof(sockErr);
	int ret = m_impl->GetSockOpt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR, &sockErr, &optLen);
	return (ret >= 0) ? sockErr : ret;
}

/* 0x8140014C */
int WsNet::Abort(void)
{
	Push::ScopedLock lock(&m_mutex);
	int ret = 0;
	if (m_resolverId > 0)
		ret = m_impl->AbortResolver(m_resolverId, 0);
	if (m_socketId > 0)
		ret = m_impl->AbortSocket(m_socketId, 0);
	m_aborted = 1;
	return ret;
}

/* 0x814001DA */
void WsNet::ClearAbort(void)
{
	Push::ScopedLock lock(&m_mutex);
	m_aborted = 0;
}

/* 0x81400C3C */
int WsNet::CheckAborted(void)
{
	Push::ScopedLock lock(&m_mutex);
	return (m_aborted != 0) ? SCE_NP_WS_ERROR_ABORTED : 0;
}

/* 0x81400226 */
int WsNet::CreateResolver(void)
{
	SceNetId rid = m_impl->CreateResolver("SceNpWsNet");
	if (rid < 0)
		return rid;
	Push::ScopedLock lock(&m_mutex);
	m_resolverId = rid;
	return 0;
}

/* 0x814002C0 */
int WsNet::ResolveHost(const char *hostName, int timeoutUsec, int retryCount)
{
	int flags = (m_nonBlocking != 0) ? 1 : 0;
	SceNetId rid = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		rid = m_resolverId;
	}
	if (rid == 0)
		return SCE_NP_WS_ERROR_INVALID_STATE;

	int ret = CheckAborted();
	if (ret < 0)
		return ret;

	sceClibMemset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
	m_remoteAddr.sin_len = sizeof(m_remoteAddr);
	m_remoteAddr.sin_family = SCE_NET_AF_INET;

	if (m_impl->InetPton(SCE_NET_AF_INET, hostName, &m_remoteAddr.sin_addr) == 0) {
		ret = m_impl->ResolveName(rid, hostName, &m_remoteAddr.sin_addr, timeoutUsec,
					  retryCount, flags);
	}
	if (ret >= 0) {
		char addrStr[16];
		if (m_impl->InetNtop(SCE_NET_AF_INET, &m_remoteAddr.sin_addr, addrStr,
				     sizeof(addrStr)) == NULL) {
			sceClibStrncpy(addrStr, "---", sizeof(addrStr));
		}
	}
	return ret;
}

/* 0x81400408 */
int WsNet::DestroyResolver(void)
{
	Push::ScopedLock lock(&m_mutex);
	if (m_resolverId == 0)
		return 0;
	int ret = m_impl->DestroyResolver(m_resolverId);
	m_resolverId = 0;
	return ret;
}

/* 0x8140047E */
int WsNet::CreateSocket(void)
{
	SceNetId sock = m_impl->CreateTcpSocket("SceNpWsNet");
	if (sock < 0)
		return sock;
	Push::ScopedLock lock(&m_mutex);
	m_socketId = sock;
	return 0;
}

/* 0x8140051A */
int WsNet::Connect(const char *hostName, SceUShort16 port, int tlsMode, bool disableVerify)
{
	m_tlsMode = tlsMode;
	m_disableVerify = disableVerify;
	m_useTls = (tlsMode != 0) ? 1 : 0;
	m_remoteAddr.sin_port = sceNetHtons(port);

	SceNetId sock = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		sock = m_socketId;
	}
	if (sock == 0)
		return SCE_NP_WS_ERROR_INVALID_STATE;

	int ret = m_impl->ConnectSocket(sock, reinterpret_cast<const SceNetSockaddr *>(&m_remoteAddr),
					sizeof(m_remoteAddr));
	if (ret < 0)
		return ret;

	int sslHandle = 0;
	if (m_useTls != 0) {
		ret = m_impl->CreateSslSession(m_sslContextParam, sock, hostName);
		if (ret < 0)
			return ret;
		sslHandle = ret;
		ret = m_impl->ConfigureSslVerify(sslHandle, m_disableVerify);
		if (ret >= 0)
			ret = m_impl->PerformSslHandshake(sslHandle);
		if (ret < 0) {
			m_impl->DestroySslSession(sslHandle);
			return ret;
		}
	}

	Push::ScopedLock lock(&m_mutex);
	m_sslHandle = sslHandle;
	return 0;
}

/* 0x814006AA */
int WsNet::Shutdown(void)
{
	SceNetId sock = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		sock = m_socketId;
	}
	if (sock == 0)
		return SCE_NP_WS_ERROR_INVALID_STATE;
	return m_impl->ShutdownSocket(sock, SCE_NET_SHUT_RDWR);
}

/* 0x81400732 */
int WsNet::Close(void)
{
	Push::ScopedLock lock(&m_mutex);
	if (m_socketId == 0)
		return 0;

	if (m_useTls != 0 && m_sslHandle > 0)
		m_impl->ShutdownSsl(m_sslHandle);

	int ret = m_impl->CloseSocket(m_socketId);
	m_socketId = 0;

	if (m_useTls != 0 && m_sslHandle > 0) {
		ret = m_impl->DestroySslSession(m_sslHandle);
		m_sslHandle = 0;
	}
	return ret;
}

/* 0x814007F8 */
int WsNet::Recv(void *buf, SceSize len)
{
	SceNetId sock = 0;
	int sslHandle = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		if (m_socketId == 0)
			return SCE_NP_WS_ERROR_INVALID_STATE;
		sock = m_socketId;
		sslHandle = m_sslHandle;
	}

	if (m_useTls != 0)
		return m_impl->RecvSsl(sslHandle, buf, len);
	return m_impl->RecvSocket(sock, buf, len, 0);
}

/* 0x814008C0 */
int WsNet::HasPendingData(bool *outHasPending)
{
	if (m_useTls != 0)
		return m_impl->CheckSslPending(m_sslHandle, outHasPending);
	*outHasPending = false;
	return 0;
}

/* 0x814008F4 */
int WsNet::Send(const void *buf, SceSize len)
{
	SceNetId sock = 0;
	int sslHandle = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		if (m_socketId == 0)
			return SCE_NP_WS_ERROR_INVALID_STATE;
		sock = m_socketId;
		sslHandle = m_sslHandle;
	}

	if (m_useTls != 0)
		return m_impl->SendSsl(sslHandle, buf, len);
	return m_impl->SendSocket(sock, buf, len, 0);
}

/* 0x814009BC */
SceNetId WsNet::CreateEpoll(const char *name)
{
	return m_impl->CreateEpoll(name);
}

/* 0x814009EC */
int WsNet::DestroyEpoll(SceNetId epollId)
{
	return m_impl->DestroyEpoll(epollId);
}

/* 0x81400A14 */
int WsNet::ControlResolverEpoll(SceNetId epollId, SceNetEpollEvent *event, int opMode)
{
	SceNetId rid = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		if (m_resolverId == 0)
			return SCE_NP_WS_ERROR_INVALID_STATE;
		rid = m_resolverId;
	}

	int epollOp = SCE_NET_EPOLL_CTL_ADD;
	if (opMode == 2)
		epollOp = SCE_NET_EPOLL_CTL_MOD;
	else if (opMode == 3)
		epollOp = SCE_NET_EPOLL_CTL_DEL;
	return m_impl->ControlEpoll(epollId, epollOp, rid, event);
}

/* 0x81400AEA */
int WsNet::ControlSocketEpoll(SceNetId epollId, SceNetEpollEvent *event, int opMode)
{
	SceNetId sock = 0;
	{
		Push::ScopedLock lock(&m_mutex);
		if (m_socketId == 0)
			return SCE_NP_WS_ERROR_INVALID_STATE;
		sock = m_socketId;
	}

	int epollOp = SCE_NET_EPOLL_CTL_ADD;
	if (opMode == 2)
		epollOp = SCE_NET_EPOLL_CTL_MOD;
	else if (opMode == 3)
		epollOp = SCE_NET_EPOLL_CTL_DEL;
	return m_impl->ControlEpoll(epollId, epollOp, sock, event);
}

/* 0x81400BC0 */
int WsNet::WaitEpoll(SceNetId epollId, SceNetEpollEvent *events, int maxEvents, int timeoutUsec)
{
	return m_impl->WaitEpoll(epollId, events, maxEvents, timeoutUsec);
}

/* 0x81400C08 */
int WsNet::AbortEpoll(SceNetId epollId)
{
	return m_impl->AbortEpoll(epollId, 0);
}

} /* namespace WebSocket */
} /* namespace Np */
} /* namespace sce */
