/*
 * np_community_ipc_common.cpp: Shared SceNpCommunityIpcServer session,
 * request, work-queue, and ticket management (0x81323B34..0x8132475B).
 *
 * Shared by both SceNpScore (serviceKind = 1) and SceNpTus (serviceKind = 0).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np_common/np_common.h>
#include "np_community_ipc.h"

/* Global SceNpCommunityIpcServer state (0x81546698..0x815466A7, 0x815C56B8..0x815C56F7) */
static NpCommunityClientContext *s_tusClientList = NULL;	/* 0x81546698 */
static NpCommunityClientContext *s_scoreClientList = NULL;	/* 0x8154669C */
static SceInt32 s_scoreIpcInitialized = 0;			/* 0x815466A0 */
static SceInt32 s_tusIpcInitialized = 0;			/* 0x815466A4 */
static SceKernelLwMutexWork s_friendCacheMutex;			/* 0x815C56B8 */
static SceKernelLwMutexWork s_ipcServerMutex;			/* 0x815C56D8 */

/* 0x81323B34 */
int npCommunityIpcInit(int serviceKind)
{
	if (s_scoreIpcInitialized == 0 && s_tusIpcInitialized == 0 &&
	    static_cast<SceUInt32>(serviceKind) <= 1u) {
		int ret = sceKernelCreateLwMutex(&s_ipcServerMutex, "SceNpCommunityIpcServer",
						 2u, 0, NULL);
		if (ret < 0)
			return ret;
		ret = sceKernelCreateLwMutex(&s_friendCacheMutex, "SceNpCommunityFriendCache",
					     2u, 0, NULL);
		if (ret < 0) {
			sceKernelDeleteLwMutex(&s_ipcServerMutex);
			return ret;
		}
	}

	if (serviceKind == NP_COMMUNITY_SERVICE_SCORE) {
		s_scoreIpcInitialized = 1;
		return 0;
	}
	if (serviceKind == NP_COMMUNITY_SERVICE_TUS) {
		s_tusIpcInitialized = 1;
		return 0;
	}
	return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
}

/* 0x81323BE8 */
int npCommunityReleaseRequest(NpCommunityRequestNode *req)
{
	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	req->refCount--;
	if (req->markedForRemoval == 1 && req->refCount == 0) {
		if (req->serverLookupTxId != 0)
			npCommunityAbortTx(req->serverLookupTxId);
		if (req->webApiReqId != 0)
			sceNpWebApiAbortRequest(req->webApiReqId);
		ret = npCommunityDestroyTx(req->transactionId);
		if (ret >= 0) {
			NpCommunityAllocNode *node = req->allocListHead;
			req->allocListHead = NULL;
			while (node != NULL) {
				NpCommunityAllocNode *nextNode = node->next;
				npCommunityFree(node->ptr);
				npCommunityFree(node);
				node = nextNode;
			}
			npCommunityFree(req);
		}
	}

	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	return ret;
}

/* Generic intrusive header shared by NpCommunityClientContext and NpCommunityRequestNode */
struct NpCommunityListNodeHeader {
	SceInt32 key;
	SceInt32 refCount;
	SceInt32 markedForRemoval;
	NpCommunityListNodeHeader *next;
	NpCommunityListNodeHeader *prev;
};

/* 0x81323C70 */
int npCommunityFindEntry(void **outEntry, SceInt32 key, int listKind,
			 NpCommunityClientContext *clientCtx, int removeFlag)
{
	NpCommunityListNodeHeader *cur = NULL;
	NpCommunityListNodeHeader **headPtr = NULL;

	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (listKind == NP_COMMUNITY_SERVICE_TUS) {
		headPtr = reinterpret_cast<NpCommunityListNodeHeader **>(&s_tusClientList);
		cur = *headPtr;
	} else if (listKind == NP_COMMUNITY_SERVICE_SCORE) {
		headPtr = reinterpret_cast<NpCommunityListNodeHeader **>(&s_scoreClientList);
		cur = *headPtr;
	} else if (listKind == NP_COMMUNITY_LIST_REQUEST && clientCtx != NULL) {
		headPtr = reinterpret_cast<NpCommunityListNodeHeader **>(&clientCtx->requestListHead);
		cur = *headPtr;
	}

	while (cur != NULL) {
		if (cur->key == key) {
			if (removeFlag == 1) {
				cur->markedForRemoval = 1;
				if (cur->prev != NULL)
					cur->prev->next = cur->next;
				else
					*headPtr = cur->next;
				if (cur->next != NULL)
					cur->next->prev = cur->prev;
			}
			cur->refCount++;
			*outEntry = cur;
			ret = 0;
			break;
		}
		cur = cur->next;
	}

	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	if (cur == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	return ret;
}

/* 0x81323D3C */
int npCommunityReleaseClient(NpCommunityClientContext *client)
{
	int shouldDestroy = 0;
	int stat = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (stat < 0)
		return stat;

	client->refCount--;
	if (client->markedForRemoval == 1 && client->refCount == 0) {
		NpCommunityRequestNode *cur = client->requestListHead;
		while (cur != NULL) {
			NpCommunityRequestNode *nextReq = cur->next;
			void *found = NULL;
			stat = npCommunityFindEntry(&found, cur->transactionId,
						    NP_COMMUNITY_LIST_REQUEST, client, 1);
			if (found != NULL)
				npCommunityReleaseRequest(
					static_cast<NpCommunityRequestNode *>(found));
			cur = nextReq;
		}
		shouldDestroy = 1;
	}

	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);

	if (shouldDestroy != 0) {
		sceKernelWaitThreadEnd(client->workerThreadId, &stat, NULL);
		sceKernelDeleteThread(client->workerThreadId);
		stat = sceKernelDeleteSema(client->workSemaId);
		stat = sceKernelDeleteEventFlag(client->eventFlagId);
		npCommunityFree(client);
	}
	return stat;
}

/* 0x81323DF0 */
int npCommunityLinkEntry(void *entry, int listKind, NpCommunityClientContext *clientCtx)
{
	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpCommunityListNodeHeader *node = static_cast<NpCommunityListNodeHeader *>(entry);
	if (listKind == NP_COMMUNITY_SERVICE_TUS) {
		NpCommunityListNodeHeader *oldHead =
			reinterpret_cast<NpCommunityListNodeHeader *>(s_tusClientList);
		node->next = oldHead;
		if (oldHead != NULL)
			oldHead->prev = node;
		s_tusClientList = static_cast<NpCommunityClientContext *>(entry);
		ret = 0;
	} else if (listKind == NP_COMMUNITY_SERVICE_SCORE) {
		NpCommunityListNodeHeader *oldHead =
			reinterpret_cast<NpCommunityListNodeHeader *>(s_scoreClientList);
		node->next = oldHead;
		if (oldHead != NULL)
			oldHead->prev = node;
		s_scoreClientList = static_cast<NpCommunityClientContext *>(entry);
		ret = 0;
	} else if (listKind == NP_COMMUNITY_LIST_REQUEST) {
		NpCommunityListNodeHeader *oldHead =
			reinterpret_cast<NpCommunityListNodeHeader *>(clientCtx->requestListHead);
		node->next = oldHead;
		if (oldHead != NULL)
			oldHead->prev = node;
		clientCtx->requestListHead = static_cast<NpCommunityRequestNode *>(entry);
		ret = 0;
	}

	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	return ret;
}

/* 0x81323E7A */
int npCommunityLookupClient(SceUID processId, int serviceKind,
			    NpCommunityClientContext **outClient)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, processId, serviceKind, NULL, 0);
	if (ret >= 0)
		*outClient = static_cast<NpCommunityClientContext *>(found);
	return ret;
}

/* 0x81323E94 */
int npCommunityLookupRequest(NpCommunityRequestNode **outReq,
			     NpCommunityClientContext *clientCtx, SceInt32 reqId)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, reqId, NP_COMMUNITY_LIST_REQUEST, clientCtx, 0);
	if (ret >= 0)
		*outReq = static_cast<NpCommunityRequestNode *>(found);
	return ret;
}

/* 0x81323EAA */
int npCommunityTrackRequestAlloc(NpCommunityRequestNode *req, void *allocPtr)
{
	if (allocPtr == NULL)
		return 0;

	NpCommunityAllocNode *node =
		static_cast<NpCommunityAllocNode *>(npCommunityAlloc(sizeof(NpCommunityAllocNode)));
	if (node == NULL)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;

	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0) {
		npCommunityFree(node);
		return ret;
	}

	node->ptr = allocPtr;
	node->next = req->allocListHead;
	req->allocListHead = node;
	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	return 0;
}

/* 0x81323F0A */
int npCommunityCreateClientSession(int serviceKind, SceUID processId,
				   const NpCommunityWorkerThreadConfig *threadCfg,
				   NpCommunityClientContext **outClient)
{
	const char *prefix = NULL;
	if (serviceKind == NP_COMMUNITY_SERVICE_SCORE) {
		prefix = "SceNpScoreIpc";
	} else if (serviceKind == NP_COMMUNITY_SERVICE_TUS) {
		prefix = "SceNpTusIpc";
	} else if (serviceKind == NP_COMMUNITY_LIST_REQUEST) {
		return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
	}

	char syncName[32];
	sceClibMemset(syncName, 0, sizeof(syncName));
	sceClibSnprintf(syncName, sizeof(syncName) - 1, "%s_%08x", prefix, processId);

	NpCommunityClientContext *client = static_cast<NpCommunityClientContext *>(
		npCommunityAlloc(sizeof(NpCommunityClientContext)));
	if (client == NULL)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;

	sceClibMemset(client, 0, sizeof(NpCommunityClientContext));
	client->processId = processId;

	int delayMutexCreated = 0;
	int ret = sceKernelCreateEventFlag(syncName, 0x1080u, 0, NULL);
	if (ret < 0)
		ret = sceKernelOpenEventFlag(syncName);

	if (ret >= 0) {
		client->eventFlagId = ret;
		ret = sceKernelCreateSema(syncName, 0, 0, NP_COMMUNITY_WORK_QUEUE_DEPTH, NULL);
	}
	if (ret >= 0) {
		client->workSemaId = ret;
		ret = sceKernelCreateLwMutex(&client->delayMutex, "SceNpCommunityDelay", 0, 1, NULL);
		if (ret >= 0) {
			delayMutexCreated = 1;
			ret = sceKernelCreateThread(syncName, threadCfg->entry,
						    threadCfg->initPriority,
						    threadCfg->stackSize, threadCfg->attr,
						    threadCfg->cpuAffinityMask, NULL);
		}
	}
	if (ret >= 0) {
		client->workerThreadId = ret;
		client->refCount++;
		ret = npCommunityLinkEntry(client, serviceKind, NULL);
	}

	if (ret < 0) {
		if (delayMutexCreated != 0)
			sceKernelDeleteLwMutex(&client->delayMutex);
		if (client->eventFlagId != 0)
			sceKernelDeleteEventFlag(client->eventFlagId);
		if (client->workerThreadId != 0)
			sceKernelDeleteThread(client->workerThreadId);
		if (client->workSemaId != 0)
			sceKernelDeleteSema(client->workSemaId);
		npCommunityFree(client);
		return ret;
	}

	if (outClient != NULL)
		*outClient = client;
	return 0;
}

/* 0x813240BC */
int npCommunityEnqueueWork(SceInt32 opCode, NpCommunityClientContext *clientCtx,
			   SceInt32 requestId, const NpCommunityWorkPayloads *payloads)
{
	NpCommunityWorkPayloads copied;
	sceClibMemset(&copied, 0, sizeof(copied));

	if (payloads != NULL) {
		copied = *payloads;
		for (int idx = 0; idx < NP_COMMUNITY_WORK_PAYLOAD_COUNT; idx++) {
			if (copied.slices[idx].buf != NULL && copied.slices[idx].size != 0) {
				void *copyBuf = npCommunityAlloc(copied.slices[idx].size);
				if (copyBuf == NULL) {
					for (int cleanup = 0; cleanup < idx; cleanup++) {
						if (copied.slices[cleanup].buf != NULL &&
						    payloads->slices[cleanup].buf != NULL &&
						    payloads->slices[cleanup].size != 0)
							npCommunityFree(copied.slices[cleanup].buf);
					}
					return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
				}
				sceClibMemcpy_safe(copyBuf, payloads->slices[idx].buf,
						   copied.slices[idx].size);
				copied.slices[idx].buf = copyBuf;
			}
		}
	}

	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpCommunityWorkItem *slot = &clientCtx->workQueue[clientCtx->queueWriteIndex];
	slot->opCode = opCode;
	slot->requestId = requestId;
	slot->payloads = copied;

	clientCtx->queueWriteIndex++;
	if (clientCtx->queueWriteIndex == NP_COMMUNITY_WORK_QUEUE_DEPTH)
		clientCtx->queueWriteIndex = 0;

	sceKernelSignalSema(clientCtx->workSemaId, 1);
	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	return 0;
}

/* 0x8132422E */
int npCommunityCloseClientSession(SceUID processId, int serviceKind)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, processId, serviceKind, NULL, 1);
	if (ret < 0)
		return ret;

	NpCommunityClientContext *client = static_cast<NpCommunityClientContext *>(found);
	ret = sceKernelDeleteLwMutex(&client->delayMutex);
	if (ret < 0)
		return ret;

	if (serviceKind != NP_COMMUNITY_SERVICE_TUS)
		npCommunityEnqueueWork(NP_SCORE_WORK_SHUTDOWN, client, 0, NULL);
	else
		npCommunityEnqueueWork(NP_TUS_WORK_SHUTDOWN, client, 0, NULL);

	npCommunityReleaseClient(client);
	return 0;
}

/* 0x813242A0 */
int npCommunityCreateRequest(NpCommunityClientContext *clientCtx, int txServiceType,
			     SceInt32 bindMode, const SceNpId *selfNpId,
			     const SceNpCommunicationId *commId,
			     const SceNpCommunicationPassphrase *passphrase,
			     const NpCommunityTimeoutConfig *timeoutCfg)
{
	int txId = npCommunityCreateTx(selfNpId, txServiceType, NULL);
	if (txId < 0)
		return txId;

	NpCommunityRequestNode *req = static_cast<NpCommunityRequestNode *>(
		npCommunityAlloc(sizeof(NpCommunityRequestNode)));
	int ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	if (req != NULL) {
		sceClibMemset(req, 0, sizeof(NpCommunityRequestNode));
		sceClibMemcpy_safe(&req->commId, commId, sizeof(SceNpCommunicationId));
		sceClibMemcpy_safe(&req->passphrase, passphrase,
				   sizeof(SceNpCommunicationPassphrase));
		sceClibMemcpy_safe(&req->selfNpId, selfNpId, sizeof(SceNpId));
		sceClibMemcpy_safe(&req->timeoutConfig, timeoutCfg,
				   sizeof(NpCommunityTimeoutConfig));
		req->savedataBindMode = bindMode;
		req->transactionId = txId;

		ret = npCommunitySetTxTimeout(txId, timeoutCfg);
		if (ret >= 0) {
			ret = npCommunityLinkEntry(req, NP_COMMUNITY_LIST_REQUEST, clientCtx);
			if (ret >= 0)
				return txId;
		}
		npCommunityFree(req);
	}

	if (txId != 0)
		npCommunityDestroyTx(txId);
	return ret;
}

/* 0x8132435A */
int npCommunityDestroyRequest(NpCommunityClientContext *clientCtx, SceInt32 requestId)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, requestId, NP_COMMUNITY_LIST_REQUEST, clientCtx, 1);
	if (ret >= 0) {
		ret = npCommunityReleaseRequest(static_cast<NpCommunityRequestNode *>(found));
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* 0x8132439E */
int npCommunityCheckNotAborted(NpCommunityRequestNode *req)
{
	int ret = sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	ret = (req->aborted == 1) ? SCE_NP_COMMUNITY_ERROR_ABORTED : 0;
	sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
	return ret;
}

/* 0x813243DA */
int npCommunityAbortRequest(NpCommunityClientContext *clientCtx, SceInt32 requestId)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, requestId, NP_COMMUNITY_LIST_REQUEST, clientCtx, 0);
	NpCommunityRequestNode *req = static_cast<NpCommunityRequestNode *>(found);
	if (ret >= 0) {
		if (sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL) >= 0) {
			req->aborted = 1;
			sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
		}
		if (req->serverLookupTxId != 0)
			npCommunityAbortTx(req->serverLookupTxId);
		if (req->webApiReqId != 0)
			sceNpWebApiAbortRequest(req->webApiReqId);
		sceKernelUnlockLwMutex(&clientCtx->delayMutex, 1);
		ret = npCommunityAbortTx(requestId);
		if (ret >= 0)
			ret = 0;
	}
	if (req != NULL)
		npCommunityReleaseRequest(req);
	return ret;
}

/* 0x8132448E */
int npCommunityChangeBindMode(NpCommunityClientContext *clientCtx, SceInt32 requestId,
			      SceInt32 bindMode)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, requestId, NP_COMMUNITY_LIST_REQUEST, clientCtx, 0);
	NpCommunityRequestNode *req = static_cast<NpCommunityRequestNode *>(found);
	if (ret >= 0) {
		req->savedataBindMode = bindMode;
		ret = 0;
	}
	if (req != NULL)
		npCommunityReleaseRequest(req);
	return ret;
}

/* 0x813244D8 */
int npCommunityReadResult(NpCommunityClientContext *clientCtx, SceInt32 requestId,
			  SceSize headMaxSize, SceSize *outHeadSize, void *headDst,
			  SceUInt32 headOffset, SceSize bodyMaxSize, SceSize *outBodySize,
			  void *bodyDst, SceUInt32 bodyOffset, int clearOnRead)
{
	void *found = NULL;
	int ret = npCommunityFindEntry(&found, requestId, NP_COMMUNITY_LIST_REQUEST, clientCtx, 0);
	NpCommunityRequestNode *req = static_cast<NpCommunityRequestNode *>(found);

	if (ret >= 0) {
		if (req->resultCode < 0) {
			ret = req->resultCode;
		} else if (req->resultReady == 0) {
			ret = SCE_NP_COMMUNITY_ERROR_REQUEST_BEFORE_END;
		} else {
			NpCommunityResultPacket *pkt = req->resultPacket;
			bool failed = false;
			SceSize copiedHead = 0;

			if (pkt != NULL && pkt->headBuf != NULL) {
				if (pkt->headSize < headOffset) {
					ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
					failed = true;
				} else {
					SceSize avail = pkt->headSize - headOffset;
					copiedHead = (headMaxSize > avail) ? avail : headMaxSize;
					const SceUInt8 *srcBytes =
						static_cast<const SceUInt8 *>(pkt->headBuf);
					sceClibMemcpy_safe(headDst, &srcBytes[headOffset],
							   copiedHead);
				}
			}

			if (!failed) {
				*outHeadSize = copiedHead;
				if (bodyDst != NULL) {
					if (pkt != NULL && pkt->bodyBuf != NULL) {
						if (pkt->bodySize < bodyOffset) {
							ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
							failed = true;
						} else {
							SceSize avail = pkt->bodySize - bodyOffset;
							SceSize copiedBody =
								(bodyMaxSize > avail)
									? avail
									: bodyMaxSize;
							const SceUInt8 *srcBytes =
								static_cast<const SceUInt8 *>(
									pkt->bodyBuf);
							sceClibMemcpy_safe(bodyDst,
									   &srcBytes[bodyOffset],
									   copiedBody);
							*outBodySize = copiedBody;
						}
					} else {
						*outBodySize = 0;
					}
				}
			}

			if (!failed)
				ret = req->resultCode;
		}
	}

	if (req != NULL) {
		if (req->resultReady != 0 && clearOnRead != 0) {
			if (sceKernelLockLwMutex(&s_ipcServerMutex, 1, NULL) >= 0) {
				NpCommunityAllocNode *node = req->allocListHead;
				while (node != NULL) {
					NpCommunityAllocNode *nextNode = node->next;
					npCommunityFree(node->ptr);
					npCommunityFree(node);
					node = nextNode;
				}
				req->allocListHead = NULL;
				sceKernelUnlockLwMutex(&s_ipcServerMutex, 1);
			}
			req->resultReady = 0;
			req->resultPacket = NULL;
		}
		npCommunityReleaseRequest(req);
	}
	return ret;
}

/* 0x81324656 */
int npCommunityFetchTicket(void **outTicketBuf, SceSize *outTicketSize)
{
	int accountReq = npAccountCreateRequest();
	if (accountReq < 0)
		return accountReq;

	SceSize ticketSize = 0;
	int ret = npAccountGetTicket(accountReq, NULL, &ticketSize);
	if (ret >= 0) {
		void *ticketBuf = npCommunityAlloc(ticketSize);
		if (ticketBuf == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			ret = npAccountGetTicket(accountReq, ticketBuf, &ticketSize);
			if (ret < 0) {
				npCommunityFree(ticketBuf);
			} else {
				*outTicketBuf = ticketBuf;
				*outTicketSize = ticketSize;
				ret = 0;
			}
		}
	}

	npAccountDestroyRequest(accountReq);
	return ret;
}

/* 0x813246F0 */
void npCommunityLogServiceNotConfigured(const char *tag, const SceNpCommunicationId *commId)
{
	char commIdText[16];
	char message[128];

	sceClibMemset(commIdText, 0, 13u);
	sceClibMemset(message, 0, sizeof(message));
	if (SceNpCommon_E426FEE8(commId, commIdText, 13u) >= 0) {
		sceClibSnprintf(message, sizeof(message), "Error ServiceNotConfigured: %s",
				commIdText);
		SceNpManager_9554802B(tag, message, NULL);
	}
}
