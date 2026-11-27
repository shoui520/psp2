/*
 * np_roster_client.cpp: SceNpRoster allocator, context pool, service status
 * cache, HTTPS transport, and public request APIs (0x813083AC..0x81309AC9).
 */
#include <stddef.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <rtc.h>
#include <np_common/np_common.h>
#include "np_roster.h"

namespace {

static const SceUInt32 kRosterMaxContexts = 4;
static const SceUInt64 kServiceStatusTtlUsec = 300000000ULL; /* 0x11E1A300 = 300s */
static const SceSize kTicketBufferSize = 0x600;
static const SceSize kReadChunkSize = 4096;
static const int kHttpErrorAborted = -2143088512; /* 0x80431080 */

enum NpRosterHostKind {
	NP_ROSTER_HOST_VIEW	= 0,
	NP_ROSTER_HOST_UPDATE	= 1
};

struct NpRosterContext {
	SceUInt8 inUse;			/* +0x00 */
	SceUInt8 padHeader[3];		/* +0x01 */
	int activeRefCount;		/* +0x04 */
	int httpTemplateId;		/* +0x08 */
	int httpConnectionId;		/* +0x0C */
	int httpRequestId;		/* +0x10 */
	SceUInt32 padBeforeLen;		/* +0x14 */
	SceUInt64 contentLength;	/* +0x18 */
	SceUInt32 bytesRead;		/* +0x20 */
	SceUInt32 padBeforeAuth;	/* +0x24 */
	int authReqId;			/* +0x28 */
	int serviceStatusReqId;		/* +0x2C */
	SceUInt8 aborted;		/* +0x30 */
	SceUInt8 padTail[7];		/* +0x31 */
};

static SceUInt8 s_rosterInitialized = 0;
static void *(*s_allocFn)(SceSize) = NULL;
static void (*s_freeFn)(void *) = NULL;

static SceKernelLwMutexWork s_contextMutex;
static SceKernelLwMutexWork s_transactionMutex;
static NpRosterContext s_contexts[kRosterMaxContexts];

static NpAuthAccountCache s_accountCache;
static SceUInt8 s_accountCacheLoaded = 0;
static int s_updateServiceStatus = 0;
static int s_viewServiceStatus = 0;
static SceRtcTick s_serviceStatusUpdatedTick;

static SceUInt8 s_ticketRawBuf[kTicketBufferSize];
static SceUInt8 s_readChunkBuf[kReadChunkSize];

static void resetContextFields(NpRosterContext *ctx)
{
	if (ctx == NULL)
		return;
	sceClibMemset(ctx, 0, sizeof(*ctx));
	ctx->httpTemplateId = 0;
	ctx->httpConnectionId = 0;
	ctx->httpRequestId = 0;
	ctx->authReqId = -1;
	ctx->serviceStatusReqId = -1;
}

/* 0x81308C28 */
static void releaseContextResources(NpRosterContext *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->httpRequestId != 0)
		sceHttpDeleteRequest(ctx->httpRequestId);
	if (ctx->httpConnectionId != 0)
		sceHttpDeleteConnection(ctx->httpConnectionId);
	if (ctx->httpTemplateId != 0)
		sceHttpDeleteTemplate(ctx->httpTemplateId);
	if (ctx->authReqId != -1)
		npAccountDestroyRequest(ctx->authReqId);
	if (ctx->serviceStatusReqId != -1)
		npProfileDestroyRequest(ctx->serviceStatusReqId);
	resetContextFields(ctx);
}

static NpRosterContext *acquireContextRef(SceUInt32 reqId)
{
	if (reqId >= kRosterMaxContexts)
		return NULL;
	if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) < 0)
		return NULL;
	NpRosterContext *ctx = NULL;
	if (s_contexts[reqId].inUse != 0) {
		ctx = &s_contexts[reqId];
		++ctx->activeRefCount;
	}
	sceKernelUnlockLwMutex(&s_contextMutex, 1);
	return ctx;
}

static void releaseContextRef(NpRosterContext *ctx)
{
	if (ctx == NULL)
		return;
	if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
		if (ctx->inUse != 0)
			--ctx->activeRefCount;
		sceKernelUnlockLwMutex(&s_contextMutex, 1);
	}
}

static void closeHttpHandles(NpRosterContext *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->httpRequestId != 0) {
		sceHttpDeleteRequest(ctx->httpRequestId);
		if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
			ctx->httpRequestId = 0;
			sceKernelUnlockLwMutex(&s_contextMutex, 1);
		}
	}
	if (ctx->httpConnectionId != 0) {
		sceHttpDeleteConnection(ctx->httpConnectionId);
		ctx->httpConnectionId = 0;
	}
}

/* 0x81308EC8: acquire global transaction lwmutex while polling context abort flag */
static int lockTransactionWithAbort(NpRosterContext *ctx)
{
	if (ctx == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	for (;;) {
		int lockRet = sceKernelTryLockLwMutex(&s_transactionMutex, 1);
		if (lockRet == 0)
			return 0;
		if (lockRet != SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
			return NP_ROSTER_ERROR_INTERNAL_FAIL;

		sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
		SceUInt8 aborted = ctx->aborted;
		sceKernelUnlockLwMutex(&s_contextMutex, 1);
		if (aborted != 0)
			return NP_ROSTER_ERROR_ABORTED;

		sceKernelDelayThread(100000);
	}
}

/* 0x81308F58: refresh service status (service ID 34) if older than 300s */
static int ensureServiceStatusLoaded(const NpAuthAccountCache *accountCache, NpRosterContext *ctx)
{
	if (accountCache == NULL || ctx == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = lockTransactionWithAbort(ctx);
	if (ret < 0)
		return ret;

	bool needFetch = true;
	SceRtcTick nowTick;
	sceRtcGetCurrentTick(&nowTick);
	sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
	if (s_serviceStatusUpdatedTick.tick != 0 &&
	    (nowTick.tick - s_serviceStatusUpdatedTick.tick) < kServiceStatusTtlUsec) {
		needFetch = false;
	}
	sceKernelUnlockLwMutex(&s_contextMutex, 1);

	if (needFetch) {
		int statusReqId = npProfileCreateRequest(accountCache, 34, NULL);
		ret = statusReqId;
		if (statusReqId >= 0) {
			if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
				if (ctx->aborted == 1) {
					ret = NP_ROSTER_ERROR_ABORTED;
				} else {
					ctx->serviceStatusReqId = statusReqId;
					ret = 0;
				}
				sceKernelUnlockLwMutex(&s_contextMutex, 1);
			} else {
				ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
			}

			if (ret >= 0) {
				NpServiceStatusNode *statusList = NULL;
				ret = npTusBackendGetServerList(statusReqId, reinterpret_cast<NpCommunityServerNode **>(&statusList));
				if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
					ctx->serviceStatusReqId = -1;
					sceKernelUnlockLwMutex(&s_contextMutex, 1);
				}

				if (ret >= 0) {
					NpServiceStatusNode *cursor = statusList;
					for (int count = 0; count < 2; ++count) {
						if (cursor == NULL || cursor->serviceIndex > 1) {
							ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
							break;
						}
						sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
						if (cursor->serviceIndex == 0)
							s_updateServiceStatus = cursor->statusCode;
						else
							s_viewServiceStatus = cursor->statusCode;
						sceKernelUnlockLwMutex(&s_contextMutex, 1);
						cursor = cursor->next;
					}
					if (ret >= 0) {
						sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
						sceRtcGetCurrentTick(&s_serviceStatusUpdatedTick);
						sceKernelUnlockLwMutex(&s_contextMutex, 1);
					}
				}
			}

			npProfileDestroyRequest(statusReqId);
		}
	}

	sceKernelUnlockLwMutex(&s_transactionMutex, 1);
	return ret;
}

static int mapServiceStatusCode(int serviceStatus)
{
	switch (serviceStatus) {
	case 1:
		return NP_ROSTER_ERROR_SERVICE_MAINTENANCE;
	case 2:
		return NP_ROSTER_ERROR_SERVICE_ENDED;
	case 3:
		return 0;
	case 4:
		return NP_ROSTER_ERROR_SERVICE_NOT_STARTED;
	default:
		return NP_ROSTER_ERROR_INTERNAL_FAIL;
	}
}

static int mapHttpStatusCode(int statusCode)
{
	switch (statusCode) {
	case 400:
		return NP_ROSTER_ERROR_HTTP_400;
	case 401:
		return NP_ROSTER_ERROR_HTTP_401;
	case 403:
		return NP_ROSTER_ERROR_HTTP_403;
	case 404:
		return NP_ROSTER_ERROR_HTTP_404;
	case 405:
		return NP_ROSTER_ERROR_HTTP_405;
	case 406:
		return NP_ROSTER_ERROR_HTTP_406;
	case 408:
		return NP_ROSTER_ERROR_HTTP_408;
	default:
		return NP_ROSTER_ERROR_HTTP_BASE;
	}
}

/* 0x81309130: send XML POST request to view or update host */
static int sendRosterPostRequest(SceUInt32 reqId, const char *endpointPath,
				 const void *postData, SceSize postDataSize, int hostKind)
{
	if (postData == NULL || endpointPath == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	NpRosterContext *ctx = acquireContextRef(reqId);
	if (ctx == NULL)
		return NP_ROSTER_ERROR_INVALID_ID;

	char *urlBuf = NULL;
	int ret = lockTransactionWithAbort(ctx);
	if (ret >= 0) {
		if (s_accountCacheLoaded == 0) {
			int authReqId = npAccountCreateRequest();
			ret = authReqId;
			if (authReqId >= 0) {
				if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
					if (ctx->aborted == 1) {
						ret = NP_ROSTER_ERROR_ABORTED;
					} else {
						ctx->authReqId = authReqId;
						ret = 0;
					}
					sceKernelUnlockLwMutex(&s_contextMutex, 1);
				} else {
					ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
				}

				if (ret >= 0) {
					ret = npAccountGetProfileAndMeta(authReqId,
									&s_accountCache, NULL);
					if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
						ctx->authReqId = -1;
						sceKernelUnlockLwMutex(&s_contextMutex, 1);
					}
					if (ret >= 0)
						s_accountCacheLoaded = 1;
				}
				npAccountDestroyRequest(authReqId);
			}
		}
		sceKernelUnlockLwMutex(&s_transactionMutex, 1);
	}

	if (ret >= 0)
		ret = ensureServiceStatusLoaded(&s_accountCache, ctx);

	if (ret >= 0) {
		const char *envName = npRosterGetEnvironment();
		SceSize urlLen = 0;

		if (hostKind == NP_ROSTER_HOST_VIEW) {
			sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
			ret = mapServiceStatusCode(s_viewServiceStatus);
			sceKernelUnlockLwMutex(&s_contextMutex, 1);
			if (ret >= 0) {
				urlLen = sceClibStrnlen("https://", 0x7FFFFFFF) +
					 sceClibStrnlen("friend-list-view.", 0x7FFFFFFF) +
					 sceClibStrnlen(s_accountCache.regionCode, 0x7FFFFFFF) +
					 sceClibStrnlen(".", 0x7FFFFFFF) +
					 sceClibStrnlen(envName, 0x7FFFFFFF) +
					 sceClibStrnlen(".communication.playstation.net", 0x7FFFFFFF) + 1;
			}
		} else if (hostKind == NP_ROSTER_HOST_UPDATE) {
			sceKernelLockLwMutex(&s_contextMutex, 1, NULL);
			ret = mapServiceStatusCode(s_updateServiceStatus);
			sceKernelUnlockLwMutex(&s_contextMutex, 1);
			if (ret >= 0) {
				urlLen = sceClibStrnlen("https://", 0x7FFFFFFF) +
					 sceClibStrnlen("friend-list-update.ww.", 0x7FFFFFFF) +
					 sceClibStrnlen(envName, 0x7FFFFFFF) +
					 sceClibStrnlen(".communication.playstation.net", 0x7FFFFFFF) + 1;
			}
		} else {
			ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
		}

		if (ret >= 0) {
			urlBuf = reinterpret_cast<char *>(npRosterMalloc(urlLen));
			if (urlBuf == NULL) {
				ret = NP_ROSTER_ERROR_OUT_OF_MEMORY;
			} else {
				if (hostKind == NP_ROSTER_HOST_UPDATE) {
					sceClibSnprintf(urlBuf, urlLen, "%s%s%s%s",
							"https://", "friend-list-update.ww.",
							envName, ".communication.playstation.net");
				} else {
					sceClibSnprintf(urlBuf, urlLen, "%s%s%s.%s%s",
							"https://", "friend-list-view.",
							s_accountCache.regionCode, envName,
							".communication.playstation.net");
				}

				int connId = sceHttpCreateConnectionWithURL(ctx->httpTemplateId,
									    urlBuf, 0);
				npRosterFree(urlBuf);
				urlBuf = NULL;
				ret = connId;
				if (connId >= 0) {
					ctx->httpConnectionId = connId;
					int httpReqId = sceHttpCreateRequest(
						connId, SCE_HTTP_METHOD_POST, endpointPath,
						static_cast<SceUInt64>(postDataSize));
					ret = httpReqId;
					if (httpReqId >= 0) {
						if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
							ctx->httpRequestId = httpReqId;
							ret = (ctx->aborted == 1)
								      ? NP_ROSTER_ERROR_ABORTED
								      : 0;
							sceKernelUnlockLwMutex(&s_contextMutex, 1);
						} else {
							ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
						}
					}
				}
			}
		}
	}

	if (ret >= 0)
		ret = sceHttpSendRequest(ctx->httpRequestId, postData, postDataSize);

	int statusCode = 0;
	if (ret >= 0)
		ret = sceHttpGetStatusCode(ctx->httpRequestId, &statusCode);

	if (ret >= 0 && statusCode != 200)
		ret = mapHttpStatusCode(statusCode);

	if (ret >= 0)
		ret = sceHttpGetResponseContentLength(ctx->httpRequestId, &ctx->contentLength);

	if (ret < 0) {
		closeHttpHandles(ctx);
		releaseContextRef(ctx);
		if (urlBuf != NULL)
			npRosterFree(urlBuf);
		return ret;
	}

	ctx->bytesRead = 0;
	releaseContextRef(ctx);
	return 0;
}

/* 0x8130972A: read next HTTP response chunk and close connection at EOF */
static int readRosterResponseChunk(SceUInt32 reqId, void *dstBuf, SceSize bufSize,
				   SceUInt8 *outIsFinal)
{
	if (dstBuf == NULL || outIsFinal == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	NpRosterContext *ctx = acquireContextRef(reqId);
	if (ctx == NULL)
		return NP_ROSTER_ERROR_INVALID_ID;

	int bytes = sceHttpReadData(ctx->httpRequestId, dstBuf, bufSize);
	if (bytes < 0) {
		closeHttpHandles(ctx);
		releaseContextRef(ctx);
		return bytes;
	}
	if (bytes == 0) {
		closeHttpHandles(ctx);
		releaseContextRef(ctx);
		return NP_ROSTER_ERROR_EMPTY_RESPONSE;
	}

	ctx->bytesRead += static_cast<SceUInt32>(bytes);
	if (static_cast<SceUInt64>(ctx->bytesRead) == ctx->contentLength) {
		*outIsFinal = 1;
		closeHttpHandles(ctx);
		releaseContextRef(ctx);
		return bytes;
	}

	*outIsFinal = 0;
	releaseContextRef(ctx);
	return bytes;
}

/* 0x8130988C: fetch binary NP auth ticket into buffer */
static int fetchRawTicket(SceUInt32 reqId, void *dstBuf, SceSize dstCapacity, SceSize *outSize)
{
	if (dstBuf == NULL || outSize == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	NpRosterContext *ctx = acquireContextRef(reqId);
	if (ctx == NULL)
		return NP_ROSTER_ERROR_INVALID_ID;

	int ret;
	if (dstCapacity < kTicketBufferSize) {
		ret = NP_ROSTER_ERROR_BUFFER_TOO_SMALL;
	} else {
		SceSize ticketSize = dstCapacity;
		ret = lockTransactionWithAbort(ctx);
		if (ret >= 0) {
			int authReqId = npAccountCreateRequest();
			ret = authReqId;
			if (authReqId >= 0) {
				if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
					if (ctx->aborted == 1) {
						ret = NP_ROSTER_ERROR_ABORTED;
					} else {
						ctx->authReqId = authReqId;
						ret = 0;
					}
					sceKernelUnlockLwMutex(&s_contextMutex, 1);
				} else {
					ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
				}

				if (ret >= 0) {
					ret = npAccountGetTicket(authReqId, dstBuf, &ticketSize);
					if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
						ctx->authReqId = -1;
						sceKernelUnlockLwMutex(&s_contextMutex, 1);
					}
					if (ret >= 0)
						*outSize = ticketSize;
				}
				npAccountDestroyRequest(authReqId);
			}
			sceKernelUnlockLwMutex(&s_transactionMutex, 1);
		}
	}

	releaseContextRef(ctx);
	return ret;
}

/* 0x81309A28, 0x81309AC2: fetch base64-encoded NP ticket */
static int fetchBase64Ticket(SceUInt32 reqId, char **outTicketStr)
{
	if (outTicketStr == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	SceSize rawLen = 0;
	int ret = fetchRawTicket(reqId, s_ticketRawBuf, sizeof(s_ticketRawBuf), &rawLen);
	if (ret < 0)
		return ret;

	SceSize encodedCap = ((rawLen + 2) / 3) * 4 + 1;
	char *encoded = reinterpret_cast<char *>(npRosterMalloc(encodedCap));
	if (encoded == NULL)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;

	int encodedLen = static_cast<int>(sceNpCommonBase64Encode(s_ticketRawBuf, encoded, rawLen));
	if (encodedLen < 0) {
		npRosterFree(encoded);
		return encodedLen;
	}
	encoded[encodedLen] = '\0';
	*outTicketStr = encoded;
	return encodedLen;
}

static void freeBase64Ticket(char *ticketStr)
{
	if (ticketStr != NULL)
		npRosterFree(ticketStr);
}

/* Shared chunk-streaming response reader loop */
static int streamAndParseResponse(SceUInt32 reqId)
{
	SceUInt8 isFinal = 0;
	for (;;) {
		int chunkBytes = readRosterResponseChunk(reqId, s_readChunkBuf,
							 sizeof(s_readChunkBuf), &isFinal);
		if (chunkBytes < 0) {
			if (chunkBytes == kHttpErrorAborted)
				return NP_ROSTER_ERROR_ABORTED;
			return chunkBytes;
		}
		int parseRet = npRosterXmlParseChunk(s_readChunkBuf,
						     static_cast<SceSize>(chunkBytes), isFinal);
		if (parseRet < 0 || isFinal != 0)
			return parseRet;
	}
}

/* 0x81308B58, 0x81308C08, 0x81308D7E: context pool lifecycle */
static int contextPoolInit()
{
	for (SceUInt32 pos = 0; pos < kRosterMaxContexts; ++pos)
		resetContextFields(&s_contexts[pos]);

	int ret = sceKernelCreateLwMutex(&s_contextMutex, "SceNpRosterContext", 2, 0, NULL);
	if (ret >= 0)
		ret = sceKernelCreateLwMutex(&s_transactionMutex, "SceNpRosterTransaction",
					     2, 0, NULL);
	if (ret < 0)
		return ret;

	sceClibMemset(&s_accountCache, 0, sizeof(s_accountCache));
	s_accountCacheLoaded = 0;
	s_updateServiceStatus = 0;
	s_viewServiceStatus = 0;
	s_serviceStatusUpdatedTick.tick = 0;
	return 0;
}

static int contextPoolTerm()
{
	int ret = sceKernelDeleteLwMutex(&s_transactionMutex);
	if (ret >= 0)
		ret = sceKernelDeleteLwMutex(&s_contextMutex);
	return ret;
}

static void destroyAllContexts()
{
	for (SceUInt32 pos = 0; pos < kRosterMaxContexts; ++pos) {
		if (s_contexts[pos].inUse == 1) {
			if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
				if (s_contexts[pos].inUse != 0)
					releaseContextResources(&s_contexts[pos]);
				sceKernelUnlockLwMutex(&s_contextMutex, 1);
			}
		}
	}
}

} /* anonymous namespace */

/* --------------------------------------------------------------------------
 * Allocator helpers (0x81308B08..0x81308B57)
 * -------------------------------------------------------------------------- */
int npRosterAllocatorInit(const NpRosterInitParam *param)
{
	if (param == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;
	s_allocFn = param->allocFn;
	s_freeFn = param->freeFn;
	return 0;
}

void npRosterAllocatorTerm(void)
{
	s_allocFn = NULL;
	s_freeFn = NULL;
}

void *npRosterMalloc(SceSize size)
{
	return (s_allocFn != NULL) ? s_allocFn(size) : NULL;
}

void npRosterFree(void *ptr)
{
	if (s_freeFn != NULL)
		s_freeFn(ptr);
}

/* --------------------------------------------------------------------------
 * Public SceNpRoster APIs (0x813083AC..0x81308B07)
 * -------------------------------------------------------------------------- */
/* 0x813083AC */
int npRosterInit(const NpRosterInitParam *param)
{
	if (s_rosterInitialized == 1)
		return NP_ROSTER_ERROR_ALREADY_INITIALIZED;
	if (param == NULL)
		return NP_ROSTER_ERROR_INVALID_ARGUMENT;

	int ret = npRosterAllocatorInit(param);
	if (ret < 0)
		return ret;

	ret = npRosterXmlInit();
	if (ret < 0) {
		npRosterAllocatorTerm();
		return ret;
	}

	ret = contextPoolInit();
	if (ret < 0) {
		npRosterXmlTerm();
		npRosterAllocatorTerm();
		return ret;
	}

	s_rosterInitialized = 1;
	return 0;
}

/* 0x8130840C */
int npRosterTerm(void)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	destroyAllContexts();
	contextPoolTerm();
	npRosterXmlTerm();
	npRosterAllocatorTerm();
	s_rosterInitialized = 0;
	return 0;
}

/* 0x81308442 -> 0x81308C7A */
int npRosterCreateRequest(void)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) < 0)
		return NP_ROSTER_ERROR_INTERNAL_FAIL;

	SceUInt32 freeIndex = kRosterMaxContexts;
	for (SceUInt32 pos = 0; pos < kRosterMaxContexts; ++pos) {
		if (s_contexts[pos].inUse == 0) {
			freeIndex = pos;
			break;
		}
	}

	int ret;
	if (freeIndex == kRosterMaxContexts) {
		ret = NP_ROSTER_ERROR_TOO_MANY_REQUESTS;
	} else {
		NpRosterContext *ctx = &s_contexts[freeIndex];
		int tmplId = sceHttpCreateTemplate("PSP2", SCE_HTTP_VERSION_1_1, 1);
		if (tmplId >= 0) {
			ctx->httpTemplateId = tmplId;
			ctx->inUse = 1;
			sceKernelUnlockLwMutex(&s_contextMutex, 1);
			return static_cast<int>(freeIndex);
		}
		releaseContextResources(ctx);
		ret = tmplId;
	}

	sceKernelUnlockLwMutex(&s_contextMutex, 1);
	return ret;
}

/* 0x81308460 -> 0x81308D18 */
int npRosterDestroyRequest(int reqId)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;
	if (reqId < 0 || static_cast<SceUInt32>(reqId) >= kRosterMaxContexts)
		return NP_ROSTER_ERROR_INVALID_ID;
	if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) < 0)
		return NP_ROSTER_ERROR_INTERNAL_FAIL;

	NpRosterContext *ctx = &s_contexts[reqId];
	if (ctx->inUse != 0)
		releaseContextResources(ctx);

	sceKernelUnlockLwMutex(&s_contextMutex, 1);
	return 0;
}

/* 0x8130847E -> 0x81308DCE */
int npRosterAbortRequest(int reqId)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;
	if (reqId < 0 || static_cast<SceUInt32>(reqId) >= kRosterMaxContexts)
		return NP_ROSTER_ERROR_INVALID_ID;

	NpRosterContext *ctx = acquireContextRef(static_cast<SceUInt32>(reqId));
	if (ctx == NULL)
		return NP_ROSTER_ERROR_INVALID_ID;

	int ret;
	if (sceKernelLockLwMutex(&s_contextMutex, 1, NULL) >= 0) {
		ctx->aborted = 1;
		if (ctx->httpRequestId != 0) {
			ret = sceHttpAbortRequest(ctx->httpRequestId);
		} else if (ctx->authReqId != -1) {
			ret = npAccountAbortRequest(ctx->authReqId);
			ctx->authReqId = -1;
		} else if (ctx->serviceStatusReqId != -1) {
			ret = npProfileAbortRequest(ctx->serviceStatusReqId);
			ctx->serviceStatusReqId = -1;
		} else {
			ret = 0;
		}
		sceKernelUnlockLwMutex(&s_contextMutex, 1);
	} else {
		ret = NP_ROSTER_ERROR_INTERNAL_FAIL;
	}

	releaseContextRef(ctx);
	return ret;
}

/* 0x8130849C */
int npRosterRequestFriend(int reqId, const SceNpId *npId, const char *comment)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildRequestFriend(npId, comment, ticketStr, &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_update/sec/request_friend",
						    xmlBody, xmlLen, NP_ROSTER_HOST_UPDATE);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseRequestFriend(&serverResult);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0 && serverResult < 0)
						ret = serverResult;
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}

/* 0x813085AE */
int npRosterAcceptFriend(int reqId, const SceNpId *npId)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildAcceptFriend(npId, ticketStr, &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_update/sec/accept_friend",
						    xmlBody, xmlLen, NP_ROSTER_HOST_UPDATE);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseAcceptFriend(&serverResult);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0 && serverResult < 0)
						ret = serverResult;
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}

/* 0x813086B2 */
int npRosterDeleteFriend(int reqId, const SceNpId *npId)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildDeleteFriend(npId, ticketStr, &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_update/sec/delete_friend",
						    xmlBody, xmlLen, NP_ROSTER_HOST_UPDATE);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseDeleteFriend(&serverResult);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0 && serverResult < 0)
						ret = serverResult;
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}

/* 0x813087B6 */
int npRosterGetBlockList(int reqId, SceUInt32 startIndex, SceNpId *outEntries,
			 SceUInt32 maxCount, SceUInt32 *outTotal)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;
	SceUInt32 fetchedCount = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildGetBlockList(startIndex, maxCount, ticketStr,
						   &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_view/sec/get_block_list",
						    xmlBody, xmlLen, NP_ROSTER_HOST_VIEW);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseGetBlockList(&serverResult, outEntries,
									maxCount, &fetchedCount,
									outTotal);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0) {
						ret = (serverResult < 0)
							      ? serverResult
							      : static_cast<int>(fetchedCount);
					}
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}

/* 0x813088D6 */
int npRosterUpdateBlockList(int reqId, const NpRosterBlockListEntry *entries, SceSize count)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildUpdateBlockList(entries, count, ticketStr,
						      &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_update/sec/update_block_list",
						    xmlBody, xmlLen, NP_ROSTER_HOST_UPDATE);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseUpdateBlockList(&serverResult);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0 && serverResult < 0)
						ret = serverResult;
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}

/* 0x813089E8 */
int npRosterGetFriendList(int reqId, int optionFlags, NpRosterFriendEntry *outEntries,
			  SceUInt32 maxCount, SceUInt32 *outTotal)
{
	if (s_rosterInitialized == 0)
		return NP_ROSTER_ERROR_NOT_INITIALIZED;

	char *ticketStr = NULL;
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int serverResult = 0;
	SceUInt32 fetchedCount = 0;

	int ret = fetchBase64Ticket(static_cast<SceUInt32>(reqId), &ticketStr);
	if (ret >= 0) {
		ret = npRosterXmlBuildGetFriendList(maxCount, optionFlags, ticketStr,
						    &xmlBody, &xmlLen);
		if (ret >= 0) {
			ret = sendRosterPostRequest(static_cast<SceUInt32>(reqId),
						    "/friend_list_view/sec/get_friend_list",
						    xmlBody, xmlLen, NP_ROSTER_HOST_VIEW);
			if (ret >= 0) {
				ret = npRosterXmlBeginParseGetFriendList(&serverResult, outEntries,
									 maxCount, &fetchedCount,
									 outTotal);
				if (ret >= 0) {
					ret = streamAndParseResponse(static_cast<SceUInt32>(reqId));
					if (ret >= 0) {
						ret = (serverResult < 0)
							      ? serverResult
							      : static_cast<int>(fetchedCount);
					}
					npRosterXmlEndParse();
				}
			}
		}
		if (xmlBody != NULL)
			npRosterXmlFreeSerialized(xmlBody);
		freeBase64Ticket(ticketStr);
	}
	return ret;
}
