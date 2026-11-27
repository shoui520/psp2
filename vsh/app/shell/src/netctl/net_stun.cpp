/*
 * net_stun.cpp: SceNetStun (RFC 3489 STUN NAT type, mapped address, and
 * filtering/hairpin discovery) inside SceShell.
 * 3.65-CEX: 0x812181CC - 0x8121A038
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include "netctl_services.h"

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

#define NET_STUN_MAX_CONTEXTS		4
#define NET_STUN_HOST_LEN		64
#define NET_STUN_HEADER_SIZE		20
#define NET_STUN_ATTR_HDR_SIZE		4
#define NET_STUN_ADDR_ATTR_SIZE		8
#define NET_STUN_HMAC_SIZE		20
#define NET_STUN_DEFAULT_BUF_SIZE	1024
#define NET_STUN_UPNP_WORK_SIZE		0x1F44
#define NET_STUN_THREAD_STACK_SIZE	0x8000
#define NET_STUN_MSGPIPE_SIZE		0x1000
#define NET_STUN_INITIAL_RTO_US		200000
#define NET_STUN_MAX_RTO_US		1600000
#define NET_STUN_MAX_RETRIES		10

#define NET_STUN_MSG_BINDING_REQUEST	0x0001u
#define NET_STUN_MSG_BINDING_RESPONSE	0x0101u

#define NET_STUN_ATTR_MAPPED_ADDRESS	0x0001u
#define NET_STUN_ATTR_RESPONSE_ADDRESS	0x0002u
#define NET_STUN_ATTR_CHANGE_REQUEST	0x0003u
#define NET_STUN_ATTR_SOURCE_ADDRESS	0x0004u
#define NET_STUN_ATTR_CHANGED_ADDRESS	0x0005u
#define NET_STUN_ATTR_USERNAME		0x0006u
#define NET_STUN_ATTR_MESSAGE_INTEGRITY	0x0008u
#define NET_STUN_ATTR_REQ_XOR_MAPPED	0x0021u
#define NET_STUN_ATTR_XOR_MAPPED_ADDR	0x8020u
#define NET_STUN_ATTR_CUSTOM_PAYLOAD	0x8099u

#define NET_STUN_CHANGE_PORT		0x02u
#define NET_STUN_CHANGE_IP		0x04u

#define NET_STUN_FLAG_ABORTING		0x1u

#define NET_STUN_CMD_NAT_TYPE		1
#define NET_STUN_CMD_MAPPED_ADDR	2
#define NET_STUN_CMD_ABORT		4
#define NET_STUN_CMD_EXIT		5
#define NET_STUN_CMD_MAPPED_ON_SOCKET	6
#define NET_STUN_CMD_FILTER_CHECK	7
#define NET_STUN_CMD_NAT_ON_SOCKET	8

typedef struct NetStunHeader {
	SceUInt16 msgType;
	SceUInt16 msgLength;
	SceUInt8 transactionId[16];
} NetStunHeader;

typedef struct NetStunAttrHeader {
	SceUInt16 attrType;
	SceUInt16 attrLength;
} NetStunAttrHeader;

typedef struct NetStunAddrValue {
	SceUInt8 unused;
	SceUInt8 family;
	SceUInt16 port;
	SceNetInAddr addr;
} NetStunAddrValue;

typedef struct NetStunRequestSpec {
	int hasResponseAddr;
	SceUInt8 responsePad;
	SceUInt8 responseFamily;
	SceUInt16 responsePort;
	SceNetInAddr responseAddr;
	SceUInt32 changeRequest;
	int requestXorMapped;
	const void *username;
	SceSize usernameLen;
	const void *hmacKey;
	SceSize hmacKeyLen;
	int hasCustomAttr;
	const void *customAttrData;
	SceSize customAttrLen;
} NetStunRequestSpec;

typedef struct NetStunResponseData {
	NetStunAddrValue mappedAddr;
	NetStunAddrValue sourceAddr;
	NetStunAddrValue changedAddr;
	NetStunAddrValue extraAddr;
	const void *hmacKey;
	SceSize hmacKeyLen;
	int verifyCustomAttr;
	const void *customAttrData;
	SceSize customAttrLen;
} NetStunResponseData;

typedef struct NetStunMessage {
	int command;
	void *userArg;
	int sockId;
	SceUInt16 localPort;
	SceUInt16 pad;
} NetStunMessage;

typedef struct NetStunContext {
	int id;				/* +0x00 */
	SceUInt32 flags;		/* +0x04 */
	SceUID threadId;		/* +0x08 */
	SceUID msgPipeId;		/* +0x0C */
	int resolverId;			/* +0x10 */
	int sockId;			/* +0x14 */
	char serverHost[NET_STUN_HOST_LEN]; /* +0x18 */
	SceUInt16 serverPort;		/* +0x58 */
	SceUInt16 pad;			/* +0x5A */
	NetStunOption option;		/* +0x5C (0x2C bytes) */
	NetStunCallback callback;	/* +0x88 */
	SceUInt8 *packetBuf;		/* +0x8C */
	void *customAttrCopy;		/* +0x90 */
	SceSize packetBufSize;		/* +0x94 */
	void *upnpWorkBuf;		/* +0x98 */
} NetStunContext;

static int g_stunInitialized = 0;		/* 0x815450C8 */
static int (*g_stunRand)(void) = NULL;		/* 0x815450CC */
static SceUID g_stunMemBlockId = -1;		/* 0x815450D0 */
static SceClibMspace g_stunMspace = NULL;	/* 0x815450D4 */
static NetStunContext g_stunContexts[NET_STUN_MAX_CONTEXTS]; /* 0x815A1148 */

static int netStunWorkerThread(SceSize args, void *argp);
static int netStunBuildRequest(SceUInt8 *packet, SceSize capacity,
			       const SceUInt8 *transactionId,
			       const NetStunRequestSpec *spec);
static int netStunParseResponse(SceUInt8 *packet, SceSize packetLen,
				const SceUInt8 *transactionId,
				NetStunResponseData *response);

/* 0x812182D0 */
static int netStunInitInternal(SceSize memorySize, int (*randFn)(void))
{
	void *base = NULL;
	int ret;

	g_stunRand = randFn;
	ret = sceKernelAllocMemBlock("SceNetStun", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
				     memorySize, NULL);
	if (ret >= 0) {
		g_stunMemBlockId = ret;
		ret = sceKernelGetMemBlockBase(g_stunMemBlockId, &base);
		if (ret >= 0) {
			g_stunMspace = sceClibMspaceCreate(base, memorySize);
			if (g_stunMspace != NULL) {
				sceClibMemset(g_stunContexts, 0, sizeof(g_stunContexts));
				for (int i = 0; i < NET_STUN_MAX_CONTEXTS; i++) {
					g_stunContexts[i].id = i + 1;
					g_stunContexts[i].threadId = -1;
					g_stunContexts[i].msgPipeId = -1;
					g_stunContexts[i].resolverId = -1;
				}
				return 0;
			}
			ret = NET_STUN_ERROR_MSPACE_CREATE;
		}
	}

	if (g_stunMspace != NULL) {
		sceClibMspaceDestroy(g_stunMspace);
		g_stunMspace = NULL;
	}
	if (g_stunMemBlockId >= 0) {
		sceKernelFreeMemBlock(g_stunMemBlockId);
		g_stunMemBlockId = -1;
	}
	return ret;
}

/* 0x812181CC */
int shellNetStunInit(SceSize memorySize, int (*randFn)(void))
{
	if (g_stunInitialized == 1)
		return NET_STUN_ERROR_ALREADY_INITIALIZED;
	if (memorySize == 0 || randFn == NULL)
		return NET_STUN_ERROR_INVALID_PARAM;

	int ret = netStunInitInternal(memorySize, randFn);
	if (ret >= 0) {
		g_stunInitialized = 1;
		return 0;
	}
	return ret;
}

static void netStunReleaseContext(NetStunContext *ctx)
{
	if (ctx->threadId >= 0) {
		sceKernelWaitThreadEnd(ctx->threadId, NULL, NULL);
		sceKernelDeleteThread(ctx->threadId);
		ctx->threadId = -1;
	}
	if (ctx->msgPipeId >= 0) {
		sceKernelDeleteMsgPipe(ctx->msgPipeId);
		ctx->msgPipeId = -1;
	}
	if (ctx->packetBuf != NULL) {
		sceClibMspaceFree(g_stunMspace, ctx->packetBuf);
		ctx->packetBuf = NULL;
	}
	if (ctx->customAttrCopy != NULL) {
		sceClibMspaceFree(g_stunMspace, ctx->customAttrCopy);
		ctx->customAttrCopy = NULL;
	}
}

/* 0x812183F8 */
static int netStunCreateInternal(int threadPriority, SceUInt32 cpuAffinity,
				 const char *serverHost, SceUInt16 serverPort,
				 const NetStunOption *option,
				 NetStunCallback callback, int *outId)
{
	int slot = 0;
	while (slot < NET_STUN_MAX_CONTEXTS) {
		if (g_stunContexts[slot].threadId < 0)
			break;
		slot++;
	}
	if (slot == NET_STUN_MAX_CONTEXTS)
		return NET_STUN_ERROR_NO_FREE_SLOT;
	if (sceClibStrnlen(serverHost, NET_STUN_HOST_LEN) >= NET_STUN_HOST_LEN)
		return NET_STUN_ERROR_INVALID_PARAM;

	NetStunContext *ctx = &g_stunContexts[slot];
	sceClibMemset(ctx->serverHost, 0, sizeof(ctx->serverHost));
	sceClibStrncpy(ctx->serverHost, serverHost, NET_STUN_HOST_LEN - 1);
	ctx->serverPort = serverPort;
	sceClibMemcpy_safe(&ctx->option, option, sizeof(ctx->option));
	ctx->callback = callback;
	ctx->resolverId = -1;
	ctx->sockId = -1;
	ctx->threadId = -1;
	ctx->msgPipeId = -1;
	ctx->upnpWorkBuf = NULL;

	int ret;
	SceSize bufSize = NET_STUN_DEFAULT_BUF_SIZE;
	if (ctx->option.hasCustomAttr != 0) {
		ctx->customAttrCopy = sceClibMspaceMalloc(g_stunMspace, ctx->option.customAttrSize);
		if (ctx->customAttrCopy == NULL) {
			netStunReleaseContext(ctx);
			return NET_STUN_ERROR_NO_MEMORY;
		}
		sceClibMemcpy_safe(ctx->customAttrCopy, ctx->option.customAttrData,
				   ctx->option.customAttrSize);
		bufSize = ctx->option.customAttrSize + NET_STUN_DEFAULT_BUF_SIZE;
	}

	ctx->packetBufSize = bufSize;
	ctx->packetBuf = (SceUInt8 *)sceClibMspaceMalloc(g_stunMspace, bufSize);
	if (ctx->packetBuf == NULL) {
		netStunReleaseContext(ctx);
		return NET_STUN_ERROR_NO_MEMORY;
	}

	ret = sceKernelCreateMsgPipe("SceNetStun", 64, 12, NET_STUN_MSGPIPE_SIZE, NULL);
	ctx->msgPipeId = ret;
	if (ret >= 0) {
		ret = sceKernelCreateThread("SceNetStun", netStunWorkerThread,
					    threadPriority, NET_STUN_THREAD_STACK_SIZE,
					    0, cpuAffinity, NULL);
		ctx->threadId = ret;
		if (ret >= 0) {
			NetStunContext *threadArg = ctx;
			ret = sceKernelStartThread(ctx->threadId, sizeof(threadArg), &threadArg);
			if (ret >= 0) {
				*outId = ctx->id;
				return ret;
			}
		}
	}

	netStunReleaseContext(ctx);
	return ret;
}

/* 0x8121820C */
int netStunCreate(int threadPriority, SceUInt32 cpuAffinity, const char *serverHost,
		  SceUInt16 serverPort, const NetStunOption *option,
		  NetStunCallback callback, int *outId)
{
	if (g_stunInitialized == 0)
		return NET_STUN_ERROR_NOT_INITIALIZED;
	return netStunCreateInternal(threadPriority, cpuAffinity, serverHost, serverPort,
				     option, callback, outId);
}

/* 0x81218612 */
static int netStunDestroyInternal(unsigned int stunId)
{
	if (stunId == 0 || stunId > NET_STUN_MAX_CONTEXTS)
		return NET_STUN_ERROR_INVALID_PARAM;

	NetStunContext *ctx = &g_stunContexts[stunId - 1];
	if (ctx->threadId < 0)
		return NET_STUN_ERROR_INVALID_PARAM;

	ctx->flags |= NET_STUN_FLAG_ABORTING;
	if (ctx->resolverId >= 0)
		sceNetResolverAbort(ctx->resolverId, 1);
	if (ctx->sockId >= 0)
		sceNetSocketAbort(ctx->sockId, 3);
	if ((ctx->option.upnpFlags & NET_STUN_UPNP_ENABLE) != 0)
		SceNetCtl_68C081D1(ctx->upnpWorkBuf);

	NetStunMessage msg;
	SceSize sentSize = 0;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.command = NET_STUN_CMD_EXIT;
	if (sceKernelSendMsgPipe(ctx->msgPipeId, &msg, sizeof(msg), 1, &sentSize, NULL) >= 0)
		netStunReleaseContext(ctx);
	return 0;
}

/* 0x8121823E */
int netStunDestroy(int stunId)
{
	if (g_stunInitialized == 0)
		return NET_STUN_ERROR_NOT_INITIALIZED;
	return netStunDestroyInternal((unsigned int)stunId);
}

/* 0x81218722 */
static int netStunStartNatOnSocketInternal(unsigned int stunId, int sockId, void *userArg)
{
	NetStunContext *ctx = &g_stunContexts[stunId - 1];
	NetStunMessage msg;
	SceSize sentSize = 0;

	ctx->flags = 0;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.command = NET_STUN_CMD_NAT_ON_SOCKET;
	msg.userArg = userArg;
	msg.sockId = sockId;
	return sceKernelSendMsgPipe(ctx->msgPipeId, &msg, sizeof(msg), 1, &sentSize, NULL);
}

/* 0x81218288 */
int netStunStartNatTypeCheckOnSocket(int stunId, int sockId, void *userArg)
{
	if (g_stunInitialized == 0)
		return NET_STUN_ERROR_NOT_INITIALIZED;
	return netStunStartNatOnSocketInternal((unsigned int)stunId, sockId, userArg);
}

/* 0x8121878C */
static int netStunStartMappedAddrInternal(unsigned int stunId, SceUInt16 localPort, void *userArg)
{
	if (stunId == 0 || stunId > NET_STUN_MAX_CONTEXTS)
		return NET_STUN_ERROR_INVALID_PARAM;

	NetStunContext *ctx = &g_stunContexts[stunId - 1];
	NetStunMessage msg;
	SceSize sentSize = 0;

	ctx->flags = 0;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.command = NET_STUN_CMD_MAPPED_ADDR;
	msg.userArg = userArg;
	msg.localPort = localPort;
	return sceKernelSendMsgPipe(ctx->msgPipeId, &msg, sizeof(msg), 1, &sentSize, NULL);
}

/* 0x81218262 */
int netStunStartMappedAddrCheck(int stunId, SceUInt16 localPort, void *userArg)
{
	if (g_stunInitialized == 0)
		return NET_STUN_ERROR_NOT_INITIALIZED;
	return netStunStartMappedAddrInternal((unsigned int)stunId, localPort, userArg);
}

/* 0x8121880C */
static int netStunAbortInternal(unsigned int stunId, void *userArg)
{
	if (stunId == 0 || stunId > NET_STUN_MAX_CONTEXTS)
		return NET_STUN_ERROR_INVALID_PARAM;

	NetStunContext *ctx = &g_stunContexts[stunId - 1];
	if (ctx->threadId < 0)
		return NET_STUN_ERROR_INVALID_PARAM;

	ctx->flags |= NET_STUN_FLAG_ABORTING;
	if (ctx->resolverId >= 0)
		sceNetResolverAbort(ctx->resolverId, 1);
	if (ctx->sockId >= 0)
		sceNetSocketAbort(ctx->sockId, 3);
	if ((ctx->option.upnpFlags & NET_STUN_UPNP_ENABLE) != 0)
		SceNetCtl_68C081D1(ctx->upnpWorkBuf);

	NetStunMessage msg;
	SceSize sentSize = 0;
	sceClibMemset(&msg, 0, sizeof(msg));
	msg.command = NET_STUN_CMD_ABORT;
	msg.userArg = userArg;
	return sceKernelSendMsgPipe(ctx->msgPipeId, &msg, sizeof(msg), 1, &sentSize, NULL);
}

/* 0x812182AC */
int netStunAbort(int stunId, void *userArg)
{
	if (g_stunInitialized == 0)
		return NET_STUN_ERROR_NOT_INITIALIZED;
	return netStunAbortInternal((unsigned int)stunId, userArg);
}

/* 0x812188BA */
static int netStunResolveHost(NetStunContext *ctx, const char *host, SceNetSockaddrIn *outAddr)
{
	sceClibMemset(outAddr, 0, sizeof(*outAddr));
	int ptonRet = sceNetInetPton(SCE_NET_AF_INET, host, &outAddr->sin_addr);
	if (ptonRet < 0)
		return NET_STUN_ERROR_INVALID_HOST;

	if (ptonRet == 0) {
		int rid = sceNetResolverCreate("SceNetStun", NULL, 0);
		if (rid < 0)
			return NET_STUN_ERROR_RESOLVER_CREATE;
		ctx->resolverId = rid;
		int resolveRet = sceNetResolverStartNtoa(rid, host, &outAddr->sin_addr,
							 1000000, 5, 0);
		sceNetResolverDestroy(ctx->resolverId);
		ctx->resolverId = -1;
		if (resolveRet < 0)
			return resolveRet;
	}

	outAddr->sin_len = sizeof(*outAddr);
	outAddr->sin_family = SCE_NET_AF_INET;
	return 0;
}

/* 0x81218952 */
static int netStunSendAndRecv(int sendSock, int recvSock,
			      const SceNetSockaddrIn *destAddr,
			      const SceNetSockaddrIn *expectAddr,
			      const NetStunRequestSpec *requestSpec,
			      NetStunResponseData *responseData,
			      SceUInt8 *packetBuf, SceSize packetBufSize)
{
	SceUInt32 txWords[4];
	for (int i = 0; i < 4; i++)
		txWords[i] = (SceUInt32)g_stunRand();

	const SceUInt8 *transactionId = (const SceUInt8 *)txWords;
	int retryCount = 0;
	int rtoUs = NET_STUN_INITIAL_RTO_US;

	while (1) {
		int packetLen = netStunBuildRequest(packetBuf, packetBufSize,
						    transactionId, requestSpec);
		if (packetLen < 0)
			return packetLen;

		int sendRet = sceNetSendto(sendSock, packetBuf, (SceSize)packetLen, 0,
					   (const SceNetSockaddr *)destAddr, sizeof(*destAddr));
		if (sendRet < 0)
			return sendRet;

		SceUInt64 deadline = (SceUInt64)sceKernelGetSystemTimeWide() + (SceUInt64)rtoUs;
		bool timedOut = false;

		while (!timedOut) {
			SceUInt64 now = (SceUInt64)sceKernelGetSystemTimeWide();
			if (now > deadline) {
				timedOut = true;
				break;
			}

			int epollId = sceNetEpollCreate("SceNetStun", 0);
			if (epollId < 0)
				return epollId;

			SceNetEpollEvent ctlEvent;
			SceNetEpollEvent readyEvent;
			sceClibMemset(&ctlEvent, 0, sizeof(ctlEvent));
			ctlEvent.events = SCE_NET_EPOLLIN;

			int ret = sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_ADD,
						     recvSock, &ctlEvent);
			int waitRet = -1;
			if (ret >= 0) {
				waitRet = sceNetEpollWait(epollId, &readyEvent, 1,
							  (int)(deadline - now));
			}
			sceNetEpollDestroy(epollId);
			if (ret < 0)
				return ret;
			if (waitRet < 0)
				return waitRet;
			if (waitRet == 0) {
				timedOut = true;
				break;
			}

			if ((readyEvent.events & SCE_NET_EPOLLIN) == 0) {
				int sockErr = 0;
				SceNetSocklen_t optLen = sizeof(sockErr);
				ret = sceNetGetsockopt(recvSock, SCE_NET_SOL_SOCKET,
						       SCE_NET_SO_ERROR, &sockErr, &optLen);
				if (ret >= 0)
					return (int)((SceUInt32)sockErr | 0x80410100u);
				return ret;
			}

			SceNetSockaddrIn fromAddr;
			SceNetSocklen_t fromLen = sizeof(fromAddr);
			int recvLen = sceNetRecvfrom(recvSock, packetBuf, packetBufSize, 0,
						     (SceNetSockaddr *)&fromAddr, &fromLen);
			if (recvLen < 0)
				continue;
			if (recvLen == 0) {
				sceKernelDelayThread(100000);
				continue;
			}

			if (expectAddr->sin_family == fromAddr.sin_family &&
			    expectAddr->sin_addr.s_addr == fromAddr.sin_addr.s_addr) {
				int parseRet = netStunParseResponse(packetBuf, (SceSize)recvLen,
								    transactionId, responseData);
				if (parseRet >= 0)
					return parseRet;
			}
		}

		retryCount++;
		if (retryCount > 0) {
			if (retryCount > NET_STUN_MAX_RETRIES)
				return NET_STUN_ERROR_TIMEOUT;
			rtoUs *= 2;
			if (rtoUs > NET_STUN_MAX_RTO_US)
				rtoUs = NET_STUN_MAX_RTO_US;
		}
	}
}

/* 0x81218B96 */
static int netStunDrainSocket(int sockId, void *scratchBuf, SceSize scratchSize)
{
	while (1) {
		int epollId = sceNetEpollCreate("SceNetStun", 0);
		if (epollId < 0)
			return epollId;

		SceNetEpollEvent ctlEvent;
		SceNetEpollEvent readyEvent;
		sceClibMemset(&ctlEvent, 0, sizeof(ctlEvent));
		ctlEvent.events = SCE_NET_EPOLLIN;

		int ret = sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_ADD, sockId, &ctlEvent);
		if (ret < 0) {
			sceNetEpollDestroy(epollId);
			return ret;
		}
		int waitRet = sceNetEpollWait(epollId, &readyEvent, 1, 0);
		sceNetEpollDestroy(epollId);
		if (waitRet <= 0)
			return waitRet < 0 ? waitRet : ret;

		int recvLen = sceNetRecvfrom(sockId, scratchBuf, scratchSize, 0, NULL, NULL);
		if (recvLen <= 0)
			return recvLen;
	}
}

/* 0x81218C4C */
static int netStunSetupUpnpPortMap(void *upnpWorkBuf, SceUInt32 upnpFlags,
				   SceUInt16 port, int *outStatus, int *outResult,
				   SceNetInAddr *outExternalAddr)
{
	char *externalIpStr = NULL;

	*outResult = 0;
	*outStatus = 0;
	outExternalAddr->s_addr = 0;

	int ret = SceNetCtl_743E9FCE(upnpWorkBuf);
	if (ret < 0)
		return ret;

	SceNetCtl_9CB6FEE1(upnpWorkBuf, 0, port, port, &externalIpStr, outStatus, outResult);
	if ((*outStatus & 2) != 0 && (*outResult & 2) != 0)
		sceNetInetPton(SCE_NET_AF_INET, externalIpStr, outExternalAddr);

	SceNetCtl_9B678FEE(upnpWorkBuf);
	if ((upnpFlags & NET_STUN_UPNP_STRICT) != 0 &&
	    (*outStatus & 4) != 0 && (*outResult & 2) != 0 && (*outResult & 4) == 0) {
		return NET_STUN_ERROR_UPNP_PORT_MAP_FAILED;
	}
	return 0;
}

static void netStunInitAuthSpecs(const NetStunContext *ctx,
				 NetStunRequestSpec *reqSpec,
				 NetStunResponseData *respData,
				 bool includeCustom)
{
	sceClibMemset(reqSpec, 0, sizeof(*reqSpec));
	if (ctx->option.username != 0) {
		reqSpec->username = &ctx->option.username;
		reqSpec->usernameLen = sizeof(ctx->option.username);
		reqSpec->hmacKey = ctx->option.hmacKey;
		reqSpec->hmacKeyLen = sizeof(ctx->option.hmacKey);
	}
	if (ctx->option.requestXorMapped != 0)
		reqSpec->requestXorMapped = 1;
	if (includeCustom && ctx->option.hasCustomAttr != 0) {
		reqSpec->hasCustomAttr = 1;
		reqSpec->customAttrData = ctx->customAttrCopy;
		reqSpec->customAttrLen = ctx->option.customAttrSize;
	}

	if (respData != NULL) {
		sceClibMemset(respData, 0, sizeof(*respData));
		if (ctx->option.username != 0) {
			respData->hmacKey = ctx->option.hmacKey;
			respData->hmacKeyLen = sizeof(ctx->option.hmacKey);
		}
		if (includeCustom && ctx->option.hasCustomAttr != 0) {
			respData->verifyCustomAttr = 1;
			respData->customAttrData = ctx->customAttrCopy;
			respData->customAttrLen = ctx->option.customAttrSize;
		}
	}
}

/* 0x81218CF8 */
static void netStunRunMappedAddrCheck(NetStunContext *ctx, SceUInt16 localPort,
				      int externalSock, void *userArg)
{
	int upnpStatus = 0;
	int upnpResult = 0;
	SceNetInAddr upnpExternalAddr;
	NetStunMappedResult mappedResult;
	SceNetSockaddrIn serverAddr;
	SceNetSockaddrIn localAddr;
	SceNetSockaddrIn boundAddr;
	NetStunRequestSpec reqSpec;
	NetStunResponseData respData;

	upnpExternalAddr.s_addr = 0;
	sceClibMemset(&mappedResult, 0, sizeof(mappedResult));

	int ret = netStunResolveHost(ctx, ctx->serverHost, &serverAddr);
	if (ret >= 0) {
		serverAddr.sin_port = sceNetHtons(ctx->serverPort);

		int activeSock = externalSock;
		if (externalSock == -1) {
			ret = sceNetSocket("SceNetStun", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
			if (ret >= 0) {
				ctx->sockId = ret;
				sceClibMemset(&localAddr, 0, sizeof(localAddr));
				localAddr.sin_family = SCE_NET_AF_INET;
				localAddr.sin_port = sceNetHtons(localPort);
				localAddr.sin_addr.s_addr = 0;
				ret = sceNetBind(ctx->sockId, (const SceNetSockaddr *)&localAddr,
						 sizeof(localAddr));
				activeSock = ctx->sockId;
			}
		} else {
			ret = netStunDrainSocket(externalSock, ctx->packetBuf, ctx->packetBufSize);
			if (ret >= 0)
				ctx->sockId = externalSock;
		}

		if (ret >= 0 && localPort == 0) {
			SceNetSocklen_t nameLen = sizeof(boundAddr);
			ret = sceNetGetsockname(activeSock, (SceNetSockaddr *)&boundAddr, &nameLen);
			if (ret >= 0)
				localPort = sceNetNtohs(boundAddr.sin_port);
		}

		if (ret >= 0 && (ctx->option.upnpFlags & NET_STUN_UPNP_ENABLE) != 0) {
			ret = netStunSetupUpnpPortMap(ctx->upnpWorkBuf, ctx->option.upnpFlags,
						      localPort, &upnpStatus, &upnpResult,
						      &upnpExternalAddr);
		}

		if (ret >= 0) {
			netStunInitAuthSpecs(ctx, &reqSpec, &respData, true);
			ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
						 &serverAddr, &serverAddr,
						 &reqSpec, &respData,
						 ctx->packetBuf, ctx->packetBufSize);
			if (ret >= 0) {
				mappedResult.mappedAddr = respData.mappedAddr.addr;
				mappedResult.mappedPort = sceNetNtohs(respData.mappedAddr.port);
			}
		}
	}

	if (ctx->sockId >= 0) {
		if (externalSock == -1)
			sceNetSocketClose(ctx->sockId);
		ctx->sockId = -1;
	}

	if ((ctx->flags & NET_STUN_FLAG_ABORTING) != 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_ABORTED, NULL);
	} else if (ret >= 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_MAPPED_ADDR_DONE, &mappedResult);
	} else {
		NetStunErrorDetail errDetail;
		sceClibMemset(&errDetail, 0, sizeof(errDetail));
		errDetail.upnpStatus = upnpStatus;
		errDetail.upnpResult = upnpResult;
		errDetail.upnpExternalAddr = upnpExternalAddr;
		ctx->callback(ctx->id, userArg, ret, &errDetail);
	}
	ctx->flags = 0;
}

/* 0x81218F28 */
static void netStunRunNatTypeCheck(NetStunContext *ctx, int externalSock, void *userArg)
{
	int passIndex = 0;
	int upnpStatus = 0;
	int upnpResult = 0;
	SceNetInAddr upnpExternalAddr;
	NetStunNatResult natResult;
	SceNetSockaddrIn serverAddr;
	SceNetSockaddrIn altServerAddr;
	SceNetSockaddrIn localAddr;
	NetStunRequestSpec reqSpec;
	NetStunResponseData respData;
	SceUInt16 savedDeltaAfterChange = 0;
	SceInt16 savedDeltaFirst = 0;
	int ret = 0;

	upnpExternalAddr.s_addr = 0;
	sceClibMemset(&serverAddr, 0, sizeof(serverAddr));

	while (1) {
		if (ctx->sockId >= 0) {
			if (externalSock == -1)
				sceNetSocketClose(ctx->sockId);
			ctx->sockId = -1;
		}

		sceClibMemset(&natResult, 0, sizeof(natResult));

		if (passIndex == 0) {
			ret = netStunResolveHost(ctx, ctx->serverHost, &serverAddr);
			if (ret < 0)
				break;
			serverAddr.sin_port = sceNetHtons(ctx->serverPort);
		}

		if (externalSock == -1) {
			ret = sceNetSocket("SceNetStun", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
			if (ret < 0)
				break;
			ctx->sockId = ret;
			sceClibMemset(&localAddr, 0, sizeof(localAddr));
			localAddr.sin_family = SCE_NET_AF_INET;
			localAddr.sin_port = 0;
			localAddr.sin_addr.s_addr = 0;
			ret = sceNetBind(ctx->sockId, (const SceNetSockaddr *)&localAddr,
					 sizeof(localAddr));
			if (ret < 0)
				break;
		} else {
			ret = netStunDrainSocket(externalSock, ctx->packetBuf, ctx->packetBufSize);
			if (ret < 0)
				break;
			ctx->sockId = externalSock;
			sceClibMemset(&localAddr, 0, sizeof(localAddr));
			localAddr.sin_family = SCE_NET_AF_INET;
			localAddr.sin_addr.s_addr = 0;
		}

		SceNetSocklen_t nameLen = sizeof(localAddr);
		ret = sceNetGetsockname(ctx->sockId, (SceNetSockaddr *)&localAddr, &nameLen);
		if (ret < 0)
			break;

		if ((ctx->option.upnpFlags & NET_STUN_UPNP_ENABLE) != 0) {
			SceUInt16 boundPort = sceNetNtohs(localAddr.sin_port);
			ret = netStunSetupUpnpPortMap(ctx->upnpWorkBuf, ctx->option.upnpFlags,
						      boundPort, &upnpStatus, &upnpResult,
						      &upnpExternalAddr);
			if (ret < 0)
				break;
			natResult.upnpResult = upnpResult;
			natResult.upnpExternalAddr = upnpExternalAddr;
		}

		/* Probe 1: original STUN server IP and port */
		netStunInitAuthSpecs(ctx, &reqSpec, &respData, false);
		ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
					 &serverAddr, &serverAddr,
					 &reqSpec, &respData,
					 ctx->packetBuf, ctx->packetBufSize);
		if (ret < 0)
			break;

		SceNetInAddr firstMappedIp = respData.mappedAddr.addr;
		SceUInt16 firstMappedPort = respData.mappedAddr.port;
		if (passIndex == 0) {
			NetStunMappedResult notify;
			sceClibMemset(&notify, 0, sizeof(notify));
			notify.mappedAddr = firstMappedIp;
			notify.mappedPort = sceNetNtohs(firstMappedPort);
			ctx->callback(ctx->id, userArg, NET_STUN_EVENT_MAPPED_ADDR_NOTIFY, &notify);
		}

		SceNetInAddr sourceIp = respData.sourceAddr.addr;
		SceNetInAddr changedIp = respData.changedAddr.addr;
		SceUInt16 changedPort = respData.changedAddr.port;

		natResult.mappedAddr = firstMappedIp;
		natResult.portPreservation = (localAddr.sin_port == firstMappedPort) ? 1 : 0;

		/* Probe 2: same server IP, changed port */
		sceClibMemset(&altServerAddr, 0, sizeof(altServerAddr));
		altServerAddr.sin_family = SCE_NET_AF_INET;
		altServerAddr.sin_addr = sourceIp;
		altServerAddr.sin_port = changedPort;

		netStunInitAuthSpecs(ctx, &reqSpec, &respData, false);
		ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
					 &altServerAddr, &altServerAddr,
					 &reqSpec, &respData,
					 ctx->packetBuf, ctx->packetBufSize);
		if (ret < 0)
			break;

		SceUInt16 secondMappedPort = respData.mappedAddr.port;
		natResult.portDelta = (int)sceNetNtohs(secondMappedPort) -
				      (int)sceNetNtohs(firstMappedPort);
		if (secondMappedPort != firstMappedPort) {
			natResult.natMappingType = 2;
			if (natResult.portPreservation == 1) {
				natResult.portDelta = 1;
				break;
			}
		}

		/* Probe 3: changed server IP, changed port */
		altServerAddr.sin_addr = changedIp;
		netStunInitAuthSpecs(ctx, &reqSpec, &respData, false);
		ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
					 &altServerAddr, &altServerAddr,
					 &reqSpec, &respData,
					 ctx->packetBuf, ctx->packetBufSize);
		if (ret < 0)
			break;

		if (respData.mappedAddr.port == firstMappedPort) {
			natResult.natMappingType = 0;
			natResult.portDelta = 0;
			break;
		}

		if (natResult.natMappingType != 2) {
			natResult.natMappingType = 1;
			if (natResult.portPreservation == 1) {
				natResult.portDelta = 1;
				break;
			}
		}

		SceUInt16 thirdDelta = (SceUInt16)(sceNetNtohs(respData.mappedAddr.port) -
						   sceNetNtohs(secondMappedPort));
		if ((natResult.portDelta | thirdDelta) == 0)
			break;
		if (thirdDelta == 1) {
			natResult.portDelta = 1;
			break;
		}
		if (natResult.natMappingType == 2 && natResult.portDelta == 1) {
			natResult.portDelta = 1;
			break;
		}

		if (passIndex != 0) {
			if (savedDeltaAfterChange == 2 || thirdDelta == 2) {
				natResult.portDelta = 2;
			} else if (natResult.natMappingType == 2 &&
				   (savedDeltaFirst == 2 || natResult.portDelta == 2)) {
				natResult.portDelta = 2;
			} else {
				natResult.portDelta = 1;
			}
			break;
		}

		savedDeltaAfterChange = thirdDelta;
		savedDeltaFirst = (SceInt16)natResult.portDelta;
		passIndex = 1;
	}

	if (ctx->sockId >= 0) {
		if (externalSock == -1)
			sceNetSocketClose(ctx->sockId);
		ctx->sockId = -1;
	}

	if ((ctx->flags & NET_STUN_FLAG_ABORTING) != 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_ABORTED, NULL);
	} else if (ret >= 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_NAT_TYPE_DONE, &natResult);
	} else {
		NetStunErrorDetail errDetail;
		sceClibMemset(&errDetail, 0, sizeof(errDetail));
		errDetail.upnpStatus = upnpStatus;
		errDetail.upnpResult = upnpResult;
		errDetail.upnpExternalAddr = upnpExternalAddr;
		ctx->callback(ctx->id, userArg, ret, &errDetail);
	}
	ctx->flags = 0;
}

/* 0x812193A0 */
static void netStunRunFilterCheck(NetStunContext *ctx, void *userArg)
{
	int upnpStatus = 0;
	int upnpResult = 0;
	SceNetInAddr upnpExternalAddr;
	NetStunFilterResult filterResult;
	SceNetSockaddrIn serverAddr;
	SceNetSockaddrIn localAddr;
	SceNetSockaddrIn sendAddr;
	SceNetSockaddrIn expectAddr;
	SceNetSockaddrIn hairpinAddr;
	NetStunRequestSpec reqSpec;
	NetStunResponseData respData;
	SceUInt8 hairpinPayload[64];

	upnpExternalAddr.s_addr = 0;
	sceClibMemset(&filterResult, 0, sizeof(filterResult));
	sceClibMemset(hairpinPayload, 0, sizeof(hairpinPayload));

	int ret = netStunResolveHost(ctx, ctx->serverHost, &serverAddr);
	if (ret >= 0) {
		serverAddr.sin_port = sceNetHtons(ctx->serverPort);
		ret = sceNetSocket("SceNetStun", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
		if (ret >= 0) {
			ctx->sockId = ret;
			sceClibMemset(&localAddr, 0, sizeof(localAddr));
			localAddr.sin_family = SCE_NET_AF_INET;
			localAddr.sin_port = 0;
			localAddr.sin_addr.s_addr = 0;
			ret = sceNetBind(ctx->sockId, (const SceNetSockaddr *)&localAddr,
					 sizeof(localAddr));
			if (ret >= 0) {
				SceNetSocklen_t nameLen = sizeof(localAddr);
				ret = sceNetGetsockname(ctx->sockId, (SceNetSockaddr *)&localAddr,
							&nameLen);
			}
			if (ret >= 0 && (ctx->option.upnpFlags & NET_STUN_UPNP_ENABLE) != 0) {
				SceUInt16 boundPort = sceNetNtohs(localAddr.sin_port);
				ret = netStunSetupUpnpPortMap(ctx->upnpWorkBuf,
							      ctx->option.upnpFlags, boundPort,
							      &upnpStatus, &upnpResult,
							      &upnpExternalAddr);
			}
			if (ret >= 0) {
				netStunInitAuthSpecs(ctx, &reqSpec, &respData, false);
				ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
							 &serverAddr, &serverAddr,
							 &reqSpec, &respData,
							 ctx->packetBuf, ctx->packetBufSize);
			}
			if (ret >= 0) {
				SceNetInAddr sourceIp = respData.sourceAddr.addr;
				SceUInt16 sourcePort = respData.sourceAddr.port;
				SceNetInAddr changedIp = respData.changedAddr.addr;
				SceUInt16 changedPort = respData.changedAddr.port;
				SceNetInAddr mappedIp = respData.mappedAddr.addr;
				SceUInt16 mappedPort = respData.mappedAddr.port;

				sceClibMemset(&sendAddr, 0, sizeof(sendAddr));
				sendAddr.sin_family = SCE_NET_AF_INET;
				sendAddr.sin_addr = sourceIp;
				sendAddr.sin_port = sourcePort;

				sceClibMemset(&expectAddr, 0, sizeof(expectAddr));
				expectAddr.sin_family = SCE_NET_AF_INET;
				expectAddr.sin_addr = sourceIp;
				expectAddr.sin_port = changedPort;

				netStunInitAuthSpecs(ctx, &reqSpec, NULL, false);
				reqSpec.changeRequest = NET_STUN_CHANGE_PORT;
				ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
							 &sendAddr, &expectAddr,
							 &reqSpec, &respData,
							 ctx->packetBuf, ctx->packetBufSize);
				bool gotFilter = false;
				if (ret == NET_STUN_ERROR_TIMEOUT) {
					filterResult.filterBehavior = 2;
					ret = 0;
					gotFilter = true;
				} else if (ret >= 0) {
					expectAddr.sin_addr = changedIp;
					netStunInitAuthSpecs(ctx, &reqSpec, NULL, false);
					reqSpec.changeRequest = NET_STUN_CHANGE_IP |
								NET_STUN_CHANGE_PORT;
					ret = netStunSendAndRecv(ctx->sockId, ctx->sockId,
								 &sendAddr, &expectAddr,
								 &reqSpec, &respData,
								 ctx->packetBuf,
								 ctx->packetBufSize);
					if (ret == NET_STUN_ERROR_TIMEOUT) {
						filterResult.filterBehavior = 1;
						ret = 0;
						gotFilter = true;
					} else if (ret >= 0) {
						filterResult.filterBehavior = 0;
						gotFilter = true;
					}
				}

				if (gotFilter) {
					sceClibMemset(&hairpinAddr, 0, sizeof(hairpinAddr));
					hairpinAddr.sin_family = SCE_NET_AF_INET;
					hairpinAddr.sin_port = mappedPort;
					hairpinAddr.sin_addr = mappedIp;

					int attempt = 0;
					while (attempt < 5) {
						ret = sceNetSendto(ctx->sockId, hairpinPayload,
								   sizeof(hairpinPayload), 0,
								   (const SceNetSockaddr *)&hairpinAddr,
								   sizeof(hairpinAddr));
						if (ret < 0)
							break;

						int epollId = sceNetEpollCreate("SceNetStun", 0);
						if (epollId < 0) {
							ret = epollId;
							break;
						}

						SceNetEpollEvent ctlEvent;
						SceNetEpollEvent readyEvent;
						sceClibMemset(&ctlEvent, 0, sizeof(ctlEvent));
						ctlEvent.events = SCE_NET_EPOLLIN;

						ret = sceNetEpollControl(epollId,
									 SCE_NET_EPOLL_CTL_ADD,
									 ctx->sockId, &ctlEvent);
						int waitRet = -1;
						if (ret >= 0) {
							waitRet = sceNetEpollWait(epollId,
										  &readyEvent,
										  1, 500000);
						}
						sceNetEpollDestroy(epollId);
						if (ret < 0)
							break;
						if (waitRet < 0) {
							ret = waitRet;
							break;
						}
						if (waitRet != 0) {
							if ((readyEvent.events & SCE_NET_EPOLLIN) == 0) {
								int sockErr = 0;
								SceNetSocklen_t optLen = sizeof(sockErr);
								ret = sceNetGetsockopt(ctx->sockId,
										       SCE_NET_SOL_SOCKET,
										       SCE_NET_SO_ERROR,
										       &sockErr, &optLen);
								if (ret >= 0)
									ret = (int)((SceUInt32)sockErr | 0x80410100u);
								break;
							}
							filterResult.hairpinSupported = 1;
							break;
						}
						attempt++;
					}
					if (ret >= 0 && attempt >= 5)
						filterResult.hairpinSupported = 0;
				}
			}
		}
	}

	if (ctx->sockId >= 0) {
		sceNetSocketClose(ctx->sockId);
		ctx->sockId = -1;
	}

	if ((ctx->flags & NET_STUN_FLAG_ABORTING) != 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_ABORTED, NULL);
	} else if (ret >= 0) {
		ctx->callback(ctx->id, userArg, NET_STUN_EVENT_FILTER_CHECK_DONE, &filterResult);
	} else {
		NetStunErrorDetail errDetail;
		sceClibMemset(&errDetail, 0, sizeof(errDetail));
		errDetail.upnpStatus = upnpStatus;
		errDetail.upnpResult = upnpResult;
		errDetail.upnpExternalAddr = upnpExternalAddr;
		ctx->callback(ctx->id, userArg, ret, &errDetail);
	}
	ctx->flags = 0;
}

/* 0x812197AC */
static int netStunWorkerThread(SceSize args, void *argp)
{
	NetStunContext *ctx = *(NetStunContext **)argp;
	SceUInt8 upnpWork[NET_STUN_UPNP_WORK_SIZE];
	NetStunMessage msg;
	SceSize recvSize = 0;

	(void)args;
	sceClibMemset(upnpWork, 0, sizeof(upnpWork));
	ctx->upnpWorkBuf = upnpWork;

	while (sceKernelReceiveMsgPipe(ctx->msgPipeId, &msg, sizeof(msg),
				       1, &recvSize, NULL) >= 0) {
		bool shouldExit = false;
		switch (msg.command) {
		case NET_STUN_CMD_NAT_TYPE:
			netStunRunNatTypeCheck(ctx, -1, msg.userArg);
			break;
		case NET_STUN_CMD_MAPPED_ADDR:
			netStunRunMappedAddrCheck(ctx, msg.localPort, -1, msg.userArg);
			break;
		case NET_STUN_CMD_ABORT:
			if ((ctx->flags & NET_STUN_FLAG_ABORTING) != 0) {
				ctx->callback(ctx->id, msg.userArg, NET_STUN_EVENT_ABORTED, NULL);
				ctx->flags = 0;
			}
			break;
		case NET_STUN_CMD_EXIT:
			shouldExit = true;
			break;
		case NET_STUN_CMD_MAPPED_ON_SOCKET:
			netStunRunMappedAddrCheck(ctx, 0, msg.sockId, msg.userArg);
			break;
		case NET_STUN_CMD_FILTER_CHECK:
			netStunRunFilterCheck(ctx, msg.userArg);
			break;
		case NET_STUN_CMD_NAT_ON_SOCKET:
			netStunRunNatTypeCheck(ctx, msg.sockId, msg.userArg);
			break;
		default:
			break;
		}
		if (shouldExit)
			break;
		sceClibMemset(upnpWork, 0, sizeof(upnpWork));
	}

	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x812198A4 */
static int netStunBuildRequest(SceUInt8 *packet, SceSize capacity,
			       const SceUInt8 *transactionId,
			       const NetStunRequestSpec *spec)
{
	SceSize remaining = capacity - NET_STUN_HEADER_SIZE;
	SceUInt8 *cursor = packet + NET_STUN_HEADER_SIZE;

	if (spec->hasResponseAddr == 1) {
		if (remaining < NET_STUN_ATTR_HDR_SIZE + NET_STUN_ADDR_ATTR_SIZE)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		NetStunAttrHeader attrHdr;
		NetStunAddrValue addrVal;
		sceClibMemset(&attrHdr, 0, sizeof(attrHdr));
		attrHdr.attrType = sceNetHtons(NET_STUN_ATTR_RESPONSE_ADDRESS);
		attrHdr.attrLength = sceNetHtons(NET_STUN_ADDR_ATTR_SIZE);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));

		sceClibMemset(&addrVal, 0, sizeof(addrVal));
		addrVal.family = 1;
		addrVal.port = spec->responsePort;
		addrVal.addr = spec->responseAddr;
		sceClibMemcpy_safe(cursor + NET_STUN_ATTR_HDR_SIZE, &addrVal, sizeof(addrVal));

		cursor += NET_STUN_ATTR_HDR_SIZE + NET_STUN_ADDR_ATTR_SIZE;
		remaining -= NET_STUN_ATTR_HDR_SIZE + NET_STUN_ADDR_ATTR_SIZE;
	}

	if (spec->changeRequest != 0) {
		if (remaining < NET_STUN_ATTR_HDR_SIZE + 4)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		NetStunAttrHeader attrHdr;
		SceUInt32 netChange = sceNetHtonl(spec->changeRequest & 6u);
		sceClibMemset(&attrHdr, 0, sizeof(attrHdr));
		attrHdr.attrType = sceNetHtons(NET_STUN_ATTR_CHANGE_REQUEST);
		attrHdr.attrLength = sceNetHtons(4);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));
		sceClibMemcpy_safe(cursor + NET_STUN_ATTR_HDR_SIZE, &netChange, sizeof(netChange));

		cursor += NET_STUN_ATTR_HDR_SIZE + 4;
		remaining -= NET_STUN_ATTR_HDR_SIZE + 4;
	}

	if (spec->requestXorMapped == 1) {
		if (remaining < NET_STUN_ATTR_HDR_SIZE)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		NetStunAttrHeader attrHdr;
		sceClibMemset(&attrHdr, 0, sizeof(attrHdr));
		attrHdr.attrType = sceNetHtons(NET_STUN_ATTR_REQ_XOR_MAPPED);
		attrHdr.attrLength = sceNetHtons(0);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));

		cursor += NET_STUN_ATTR_HDR_SIZE;
		remaining -= NET_STUN_ATTR_HDR_SIZE;
	}

	if (spec->username != NULL) {
		if (remaining < spec->usernameLen + NET_STUN_ATTR_HDR_SIZE)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		NetStunAttrHeader attrHdr;
		sceClibMemset(&attrHdr, 0, sizeof(attrHdr));
		attrHdr.attrType = sceNetHtons(NET_STUN_ATTR_USERNAME);
		attrHdr.attrLength = sceNetHtons((SceUInt16)spec->usernameLen);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));
		if (spec->usernameLen != 0)
			sceClibMemcpy_safe(cursor + NET_STUN_ATTR_HDR_SIZE,
					   spec->username, spec->usernameLen);

		cursor += spec->usernameLen + NET_STUN_ATTR_HDR_SIZE;
		remaining -= spec->usernameLen + NET_STUN_ATTR_HDR_SIZE;
	}

	if (spec->hasCustomAttr == 1) {
		if (remaining < spec->customAttrLen + NET_STUN_ATTR_HDR_SIZE)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		NetStunAttrHeader attrHdr;
		sceClibMemset(&attrHdr, 0, sizeof(attrHdr));
		attrHdr.attrType = sceNetHtons(NET_STUN_ATTR_CUSTOM_PAYLOAD);
		attrHdr.attrLength = sceNetHtons((SceUInt16)spec->customAttrLen);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));
		if (spec->customAttrLen != 0)
			sceClibMemcpy_safe(cursor + NET_STUN_ATTR_HDR_SIZE,
					   spec->customAttrData, spec->customAttrLen);

		cursor += spec->customAttrLen + NET_STUN_ATTR_HDR_SIZE;
	}

	SceSize bodyEndOffset = (SceSize)(cursor - packet);
	NetStunHeader header;
	sceClibMemset(&header, 0, sizeof(header));
	header.msgType = sceNetHtons(NET_STUN_MSG_BINDING_REQUEST);
	sceClibMemcpy_safe(header.transactionId, transactionId, sizeof(header.transactionId));

	if (spec->hmacKey != NULL) {
		SceSize totalLen = bodyEndOffset + NET_STUN_ATTR_HDR_SIZE + NET_STUN_HMAC_SIZE;
		header.msgLength = sceNetHtons((SceUInt16)(bodyEndOffset + 4));
		sceClibMemcpy_safe(packet, &header, sizeof(header));

		SceSize paddedLen = 64;
		if (bodyEndOffset > 0)
			paddedLen = ((bodyEndOffset - 1) | 0x3Fu) + 1;
		if (capacity < paddedLen || capacity < totalLen)
			return NET_STUN_ERROR_BUFFER_TOO_SMALL;

		sceClibMemset(cursor, 0, paddedLen - bodyEndOffset);
		SceUInt8 digest[NET_STUN_HMAC_SIZE];
		sceHmacSha1Digest(spec->hmacKey, spec->hmacKeyLen, packet, paddedLen, digest);

		NetStunAttrHeader hmacHdr;
		sceClibMemset(&hmacHdr, 0, sizeof(hmacHdr));
		hmacHdr.attrType = sceNetHtons(NET_STUN_ATTR_MESSAGE_INTEGRITY);
		hmacHdr.attrLength = sceNetHtons(NET_STUN_HMAC_SIZE);
		sceClibMemcpy_safe(cursor, &hmacHdr, sizeof(hmacHdr));
		sceClibMemcpy_safe(cursor + NET_STUN_ATTR_HDR_SIZE, digest, sizeof(digest));
		return (int)totalLen;
	}

	header.msgLength = sceNetHtons((SceUInt16)(bodyEndOffset - NET_STUN_HEADER_SIZE));
	sceClibMemcpy_safe(packet, &header, sizeof(header));
	return (int)bodyEndOffset;
}

static int netStunReadAddrAttr(const SceUInt8 *attrPtr, SceSize remaining,
			       NetStunAddrValue *outAddr, bool *outIgnored)
{
	if (remaining < NET_STUN_ATTR_HDR_SIZE + NET_STUN_ADDR_ATTR_SIZE)
		return NET_STUN_ERROR_BUFFER_TOO_SMALL;

	NetStunAttrHeader hdr;
	sceClibMemcpy_safe(&hdr, attrPtr, sizeof(hdr));
	SceUInt16 len = sceNetNtohs(hdr.attrLength);
	if (len == NET_STUN_ADDR_ATTR_SIZE) {
		NetStunAddrValue raw;
		sceClibMemcpy_safe(&raw, attrPtr + NET_STUN_ATTR_HDR_SIZE, sizeof(raw));
		if (raw.family == 1) {
			sceClibMemset(outAddr, 0, sizeof(*outAddr));
			outAddr->family = 1;
			outAddr->port = raw.port;
			outAddr->addr = raw.addr;
			*outIgnored = false;
			return NET_STUN_ATTR_HDR_SIZE + NET_STUN_ADDR_ATTR_SIZE;
		}
	}

	*outIgnored = true;
	return (int)len + NET_STUN_ATTR_HDR_SIZE;
}

/* 0x81219C06 */
static int netStunParseResponse(SceUInt8 *packet, SceSize packetLen,
				const SceUInt8 *transactionId,
				NetStunResponseData *response)
{
	if (packetLen < NET_STUN_HEADER_SIZE)
		return NET_STUN_ERROR_INVALID_PACKET;

	NetStunHeader header;
	sceClibMemcpy_safe(&header, packet, sizeof(header));
	if (sceClibMemcmp(transactionId, header.transactionId, sizeof(header.transactionId)) != 0)
		return NET_STUN_ERROR_INVALID_PACKET;
	if (header.msgType != sceNetHtons(NET_STUN_MSG_BINDING_RESPONSE))
		return NET_STUN_ERROR_INVALID_PACKET;

	SceUInt16 bodyLen = sceNetNtohs(header.msgLength);
	if (packetLen < (SceSize)bodyLen + NET_STUN_HEADER_SIZE)
		return NET_STUN_ERROR_INVALID_PACKET;

	header.msgLength = sceNetHtons(bodyLen);
	sceClibMemcpy_safe(packet, &header, sizeof(header));

	if (bodyLen < NET_STUN_ATTR_HDR_SIZE)
		return NET_STUN_ERROR_MISSING_ATTR;

	bool hasMapped = false;
	bool hasSource = false;
	bool hasChanged = false;
	SceSize remaining = bodyLen;
	SceUInt8 *cursor = packet + NET_STUN_HEADER_SIZE;

	while (remaining >= NET_STUN_ATTR_HDR_SIZE) {
		NetStunAttrHeader attrHdr;
		sceClibMemcpy_safe(&attrHdr, cursor, sizeof(attrHdr));
		SceUInt16 attrLen = sceNetNtohs(attrHdr.attrLength);
		if (remaining - NET_STUN_ATTR_HDR_SIZE < attrLen)
			break;

		SceUInt16 attrType = sceNetNtohs(attrHdr.attrType);
		if (attrType == NET_STUN_ATTR_MAPPED_ADDRESS ||
		    attrType == NET_STUN_ATTR_XOR_MAPPED_ADDR) {
			bool ignored = false;
			int ret = netStunReadAddrAttr(cursor, remaining,
						      &response->mappedAddr, &ignored);
			if (ret < 0)
				return ret;
			if (!ignored) {
				if (attrType == NET_STUN_ATTR_XOR_MAPPED_ADDR) {
					SceUInt32 xorIp = (SceUInt32)transactionId[0] |
							  ((SceUInt32)transactionId[1] << 8) |
							  ((SceUInt32)transactionId[2] << 16) |
							  ((SceUInt32)transactionId[3] << 24);
					SceUInt16 xorPort = (SceUInt16)((SceUInt16)transactionId[0] |
									((SceUInt16)transactionId[1] << 8));
					response->mappedAddr.port ^= xorPort;
					response->mappedAddr.addr.s_addr ^= xorIp;
				}
				hasMapped = true;
			}
		} else if (attrType == NET_STUN_ATTR_SOURCE_ADDRESS) {
			bool ignored = false;
			int ret = netStunReadAddrAttr(cursor, remaining,
						      &response->sourceAddr, &ignored);
			if (ret < 0)
				return ret;
			if (!ignored)
				hasSource = true;
		} else if (attrType == NET_STUN_ATTR_CHANGED_ADDRESS) {
			bool ignored = false;
			int ret = netStunReadAddrAttr(cursor, remaining,
						      &response->changedAddr, &ignored);
			if (ret < 0)
				return ret;
			if (!ignored)
				hasChanged = true;
		} else if (attrType == NET_STUN_ATTR_MESSAGE_INTEGRITY) {
			SceSize prefixLen = (SceSize)(cursor - packet);
			SceSize paddedLen = 64;
			if (prefixLen > 0)
				paddedLen = ((prefixLen - 1) | 0x3Fu) + 1;
			if (packetLen < paddedLen)
				return NET_STUN_ERROR_BUFFER_TOO_SMALL;

			SceUInt8 expectedDigest[NET_STUN_HMAC_SIZE];
			SceUInt8 computedDigest[NET_STUN_HMAC_SIZE];
			sceClibMemcpy_safe(expectedDigest, cursor + NET_STUN_ATTR_HDR_SIZE,
					   sizeof(expectedDigest));
			sceClibMemset(cursor, 0, paddedLen - prefixLen);
			sceHmacSha1Digest(response->hmacKey, response->hmacKeyLen,
					    packet, paddedLen, computedDigest);
			if (sceClibMemcmp(expectedDigest, computedDigest, sizeof(expectedDigest)) != 0)
				return NET_STUN_ERROR_INTEGRITY_MISMATCH;
			break;
		} else if (attrType == NET_STUN_ATTR_CUSTOM_PAYLOAD) {
			if (response->verifyCustomAttr != 0) {
				if (attrLen != response->customAttrLen)
					return NET_STUN_ERROR_EXT_ATTR_LEN_MISMATCH;
				if (sceClibMemcmp(cursor + NET_STUN_ATTR_HDR_SIZE,
						  response->customAttrData,
						  response->customAttrLen) != 0) {
					return NET_STUN_ERROR_EXT_ATTR_MISMATCH;
				}
			}
		}

		attrHdr.attrLength = sceNetHtons(attrLen);
		sceClibMemcpy_safe(cursor, &attrHdr, sizeof(attrHdr));
		remaining -= (SceSize)attrLen + NET_STUN_ATTR_HDR_SIZE;
		cursor += (SceSize)attrLen + NET_STUN_ATTR_HDR_SIZE;
	}

	if (hasMapped && hasSource && hasChanged)
		return 0;
	return NET_STUN_ERROR_MISSING_ATTR;
}
