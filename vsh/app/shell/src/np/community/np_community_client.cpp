/*
 * np_community_client.cpp: NP Community client/transaction registry, URL builder,
 * HTTP request opening, Basic Auth callback, and heap wrappers (0x8132A068..0x8132DA11).
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include "np_community_backend.h"

#define NP_COMMUNITY_USER_AGENT		"PSP2Community-agent/1.0.0"
#define NP_COMMUNITY_SSL_ENABLE_OPT	357U
#define NP_COMMUNITY_SSL_DISABLE_OPT	24U
#define NP_COMMUNITY_CONNECT_TIMEOUT	30000000U	/* 30 seconds */
#define NP_COMMUNITY_SEND_TIMEOUT	120000000U	/* 120 seconds */
#define NP_COMMUNITY_RECV_TIMEOUT	120000000U	/* 120 seconds */
#define NP_COMMUNITY_DOMAIN_SUFFIX	".community.playstation.net"
#define NP_COMMUNITY_DL_SUFFIX		".dl.playstation.net"
#define NP_COMMUNITY_TSS_HOST_PREFIX	"a" "0.ww."

/* Global backend state (0x81546818..0x8154682C, 0x815C60F0..0x815C61FC) */
static SceUInt8 s_clientListInitialized = 0;
static SceInt32 s_backendInitialized = 0;
static SceUInt8 s_txListInitialized = 0;
static SceInt32 s_activeTxCount = 0;
static SceInt32 s_nextTxId = NP_COMMUNITY_TX_ID_SENTINEL;
static SceUInt8 s_memoryInitialized = 0;

static SceUID s_clientListMutex = -1;
static NpCommunityBackendClient s_clientListHead;
static SceUID s_txListMutex = -1;
static NpCommunityBackendTx s_txListHead;

/* XOR-0x7F obfuscated HTTP Basic Auth passwords (0x8151D7D4, 0x8151D7F4) */
static const SceUInt8 s_basicRealmXorKey[32] = {
	0x1D, 0x0F, 0x05, 0x44, 0x16, 0x2E, 0x02, 0x1E,
	0x3E, 0x11, 0x21, 0x2F, 0x18, 0x0E, 0x4A, 0x4F,
	0x47, 0x03, 0x0D, 0x48, 0x45, 0x0B, 0x49, 0x44,
	0x58, 0x40, 0x10, 0x1B, 0x35, 0x11, 0x10, 0x3D
};

static const SceUInt8 s_rankingRealmXorKey[32] = {
	0x19, 0x21, 0x43, 0x38, 0x11, 0x12, 0x55, 0x1E,
	0x28, 0x4A, 0x1E, 0x03, 0x4E, 0x0A, 0x12, 0x3E,
	0x4D, 0x48, 0x33, 0x17, 0x1B, 0x03, 0x05, 0x5D,
	0x51, 0x0D, 0x0E, 0x19, 0x4C, 0x28, 0x3F, 0x08
};

/* Forward declarations for internal lifecycle helpers */
static int npCommunityClientListInit(void);
static int npCommunityClientListTerm(void);
static int npCommunityClientCreate(const SceNpOnlineId *onlineId);
static int npCommunityClientRelease(NpCommunityBackendClient *client);
static int npCommunityClientAcquire(const char *onlineIdName,
				    NpCommunityBackendClient **outClient);
static int npCommunityClientDestroy(const char *onlineIdName);
static int npCommunityBackendInitInternal(void);
static int npCommunityBackendIsInitialized(void);
static int npCommunityTxListInit(void);
static int npCommunityTxDestroyByOwner(SceInt32 txId, int ownerKind);
static int npCommunityTxCreateInternal(NpCommunityBackendClient *client, int serviceType,
				       int ownerKind, void *serviceParam);
static int npCommunityMemoryInit(void);
static int npCommunityMemoryTerm(void);

static void npCommunityFreeAllocList(NpCommunityAllocNode *head)
{
	NpCommunityAllocNode *node = head;
	while (node != NULL) {
		NpCommunityAllocNode *nextNode = node->next;
		npCommunityFree(node->ptr);
		npCommunityFree(node);
		node = nextNode;
	}
}

static void npCommunityDestroyClientResources(NpCommunityBackendClient *client)
{
	npCommunityFreeAllocList(client->allocListHead);
	if (client->httpTemplateId > 0) {
		sceHttpDeleteTemplate(client->httpTemplateId);
		client->httpTemplateId = 0;
	}
	if (client->clientMutexId > 0)
		sceKernelDeleteMutex(client->clientMutexId);
	npCommunityFree(client);
}

static void npCommunityDestroyTxNode(NpCommunityBackendTx *tx)
{
	npCommunityCloseTxHttp(tx);
	npCommunityClientRelease(tx->client);
	NpCommunityAllocNode *allocHead = tx->allocListHead;
	tx->allocListHead = NULL;
	tx->client = NULL;
	npCommunityFreeAllocList(allocHead);
	npCommunityFree(tx);
	--s_activeTxCount;
}

static int npCommunityIsHttpsScheme(const char *scheme)
{
	return (scheme[4] == 's');
}

static int npCommunityAllocUrlParts(char **outUrl, const char *scheme, const char *partA,
				    const char *partB, const char *partC, const char *partD,
				    const char *partE, const char *partF)
{
	SceSize totalLen = sceClibStrnlen(scheme, 0x7FFFFFFFU) + 1U;
	const char *parts[6] = { partA, partB, partC, partD, partE, partF };
	for (int idx = 0; idx < 6; ++idx) {
		if (parts[idx] != NULL)
			totalLen += sceClibStrnlen(parts[idx], 0x7FFFFFFFU);
	}

	char *url = static_cast<char *>(npCommunityAlloc(totalLen));
	if (url == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(url, 0, totalLen);
	SceSize schemeLen = npCommunityIsHttpsScheme(scheme) ? 8U : 7U;
	sceClibStrncpy(url, scheme, schemeLen);
	for (int idx = 0; idx < 6; ++idx) {
		if (parts[idx] != NULL)
			npCommunityAppendBounded(url, parts[idx], totalLen);
	}
	*outUrl = url;
	return 0;
}

/* 0x8132A068 */
int npCommunityHttpSend(NpCommunityBackendTx *tx, const void *body, SceSize size,
			SceSize *outSentSize)
{
	if (tx->aborted == 1)
		return NP_COMMUNITY_ERR_ABORTED;

	int ret = sceHttpSendRequest(tx->httpReqId, body, size);
	if (ret >= 0 && outSentSize != NULL)
		*outSentSize = size;
	return ret;
}

/* 0x8132A090 */
int npCommunityInitBackend(void)
{
	return npCommunityBackendInitInternal();
}

/* 0x8132A098 */
int npCommunityEnsureUser(const SceNpOnlineId *onlineId)
{
	if (npCommunityBackendIsInitialized() != 1)
		return NP_COMMUNITY_ERR_NOT_INIT;
	return npCommunityClientCreate(onlineId);
}

/* 0x8132A0B6 */
int npCommunitySetUserEnv(const SceNpOnlineId *onlineId, const char *envName,
			  const char *countryCode)
{
	NpCommunityBackendClient *client = NULL;
	int ret;

	if (npCommunityBackendIsInitialized() != 1) {
		ret = NP_COMMUNITY_ERR_NOT_INIT;
	} else if (envName == NULL || countryCode == NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityClientAcquire(onlineId->data, &client);
		if (ret >= 0) {
			sceClibStrncpy(client->envName, envName, 15U);
			sceClibStrncpy(client->countryCode, countryCode, 2U);
			ret = 0;
		}
	}

	if (client != NULL)
		npCommunityClientRelease(client);
	return ret;
}

/* 0x8132A13A */
static int npCommunityCreateTxForClient(NpCommunityBackendClient *client, int serviceType,
					int ownerKind, const void *serviceParam)
{
	void *copiedParam = NULL;
	int ret = 0;

	switch (serviceType) {
	case 1:
		if (sceClibStrnlen(client->countryCode, 0x7FFFFFFFU) == 0U)
			return NP_COMMUNITY_ERR_NO_COUNTRY;
		break;
	case 4:
		if (serviceParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		{
			const char *text = static_cast<const char *>(serviceParam);
			SceSize len = sceClibStrnlen(text, 0x7FFFFFFFU) + 1U;
			copiedParam = npCommunityAlloc(len);
			if (copiedParam == NULL)
				return NP_COMMUNITY_ERR_NO_MEMORY;
			sceClibMemcpy_safe(copiedParam, text, len);
		}
		break;
	case 19:
	case 21:
		if (serviceParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		copiedParam = npCommunityAlloc(sizeof(SceNpCommunicationId));
		if (copiedParam == NULL)
			return NP_COMMUNITY_ERR_NO_MEMORY;
		sceClibMemcpy_safe(copiedParam, serviceParam, sizeof(SceNpCommunicationId));
		break;
	case 20:
		if (serviceParam != NULL)
			return NP_COMMUNITY_ERR_UNEXPECTED_PARAM;
		break;
	default:
		break;
	}

	ret = npCommunityTxCreateInternal(client, serviceType, ownerKind, copiedParam);
	if (ret < 0 && copiedParam != NULL)
		npCommunityFree(copiedParam);
	return ret;
}

/* 0x8132A252 */
int npProfileCreateRequest(const void *onlineId, int reservedFlag, const char *optName)
{
	NpCommunityBackendClient *client = NULL;
	int ret;

	if (npCommunityBackendIsInitialized() != 1) {
		ret = NP_COMMUNITY_ERR_NOT_INIT;
	} else {
		const SceNpOnlineId *id = static_cast<const SceNpOnlineId *>(onlineId);
		ret = npCommunityClientAcquire((id != NULL) ? id->data : NULL, &client);
		if (ret >= 0) {
			ret = npCommunityCreateTxForClient(client, reservedFlag,
							   NP_COMMUNITY_TX_OWNER_PROFILE, optName);
			if (ret >= 0)
				return ret;
		}
	}

	if (client != NULL)
		npCommunityClientRelease(client);
	return ret;
}

/* 0x8132A2BA */
int npCommunityCreateTx(const SceNpId *selfNpId, int serviceType,
			const SceNpCommunicationId *commId)
{
	NpCommunityBackendClient *client = NULL;
	int ret;

	if (npCommunityBackendIsInitialized() != 1) {
		ret = NP_COMMUNITY_ERR_NOT_INIT;
	} else {
		ret = npCommunityClientAcquire((selfNpId != NULL) ? selfNpId->handle.data : NULL,
					       &client);
		if (ret >= 0) {
			ret = npCommunityCreateTxForClient(client, serviceType,
							   NP_COMMUNITY_TX_OWNER_COMMUNITY,
							   commId);
			if (ret >= 0)
				return ret;
		}
	}

	if (client != NULL)
		npCommunityClientRelease(client);
	return ret;
}

/* 0x8132A322 */
int npCommunityAbortTx(SceInt32 txId)
{
	NpCommunityBackendTx *tx = NULL;
	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0) {
		tx->aborted = 1;
		if (tx->httpReqId != 0)
			ret = sceHttpAbortRequest(tx->httpReqId);
	}
	if (tx != NULL)
		npCommunityReleaseBackendTx(tx);
	return ret;
}

/* Alias for npmanager_ipc_server.cpp (0x8132A322) */
int npProfileAbortRequest(int reqHandle)
{
	return npCommunityAbortTx(reqHandle);
}

/* 0x8132A370 */
int npCommunityRemoveUser(const SceNpOnlineId *onlineId)
{
	return npCommunityClientDestroy((onlineId != NULL) ? onlineId->data : NULL);
}

/* 0x8132A378 */
int npProfileDestroyRequest(int reqHandle)
{
	return npCommunityTxDestroyByOwner(reqHandle, NP_COMMUNITY_TX_OWNER_PROFILE);
}

/* 0x8132A382 */
int npCommunityDestroyTx(SceInt32 txId)
{
	return npCommunityTxDestroyByOwner(txId, NP_COMMUNITY_TX_OWNER_COMMUNITY);
}

/* 0x8132A38C */
int npCommunitySetTxTimeout(SceInt32 txId, const NpCommunityTimeoutConfig *timeoutCfg)
{
	if (timeoutCfg == NULL)
		return NP_COMMUNITY_ERR_NULL_POINTER;

	NpCommunityBackendTx *tx = NULL;
	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0)
		sceClibMemcpy_safe(&tx->timeoutConfig, timeoutCfg,
				   sizeof(NpCommunityTimeoutConfig));
	if (tx != NULL)
		npCommunityReleaseBackendTx(tx);
	return ret;
}

/* 0x8132A3E2 */
int npCommunityGetTxSessionKey(SceInt32 txId, void *outKeyBuf)
{
	NpCommunityBackendTx *tx = NULL;
	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0)
		sceClibMemcpy_safe(outKeyBuf, &tx->client->onlineName, sizeof(SceNpOnlineName));
	if (tx != NULL)
		npCommunityReleaseBackendTx(tx);
	return ret;
}

/* 0x8132A430 */
static int npCommunityClientListInit(void)
{
	if (s_clientListInitialized != 0)
		return NP_COMMUNITY_ERR_ALREADY_INIT;

	sceClibMemset(&s_clientListHead, 0, sizeof(s_clientListHead));
	s_clientListHead.clientId = NP_COMMUNITY_CLIENT_ID_SENTINEL;
	SceUID mutexId = sceKernelCreateMutex("SceNpCommunityClientList", 2U, 0, NULL);
	if (mutexId < 0)
		return mutexId;

	s_clientListMutex = mutexId;
	s_clientListInitialized = 1;
	return 0;
}

/* 0x8132A494 */
static int npCommunityClientListTerm(void)
{
	if (s_clientListInitialized != 1)
		return NP_COMMUNITY_ERR_NOT_INIT;

	int ret = sceKernelLockMutex(s_clientListMutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpCommunityBackendClient *node = s_clientListHead.next;
	while (node != NULL) {
		NpCommunityBackendClient *nextNode = node->next;
		if (node->prev != NULL)
			node->prev->next = nextNode;
		if (nextNode != NULL)
			nextNode->prev = node->prev;
		npCommunityDestroyClientResources(node);
		node = nextNode;
	}

	sceKernelUnlockMutex(s_clientListMutex, 1);
	sceKernelDeleteMutex(s_clientListMutex);
	s_clientListInitialized = 0;
	return 0;
}

/* 0x8132A554 */
static int npCommunityClientCreate(const SceNpOnlineId *onlineId)
{
	NpCommunityBackendClient *client = static_cast<NpCommunityBackendClient *>(
		npCommunityAlloc(sizeof(NpCommunityBackendClient)));
	if (client == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(client, 0, sizeof(NpCommunityBackendClient));
	sceClibMemcpy_safe(&client->onlineId, onlineId, sizeof(SceNpOnlineId));

	int ret = sceKernelCreateMutex("SceNpCommunityClient", 2U, 0, NULL);
	if (ret >= 0) {
		client->clientMutexId = ret;
		int tmplId = sceHttpCreateTemplate(NP_COMMUNITY_USER_AGENT, 2, SCE_FALSE);
		ret = tmplId;
		if (tmplId >= 0) {
			client->httpTemplateId = tmplId;
			sceHttpsEnableOptionPrivate(tmplId, NP_COMMUNITY_SSL_ENABLE_OPT);
			sceHttpsDisableOptionPrivate(tmplId, NP_COMMUNITY_SSL_DISABLE_OPT);
			sceHttpSetConnectTimeOut(tmplId, NP_COMMUNITY_CONNECT_TIMEOUT);
			sceHttpSetSendTimeOut(tmplId, NP_COMMUNITY_SEND_TIMEOUT);
			sceHttpSetRecvTimeOut(tmplId, NP_COMMUNITY_RECV_TIMEOUT);
			sceHttpSetAcceptEncodingGZIPEnabled(tmplId, SCE_FALSE);
			sceHttpRemoveRequestHeader(tmplId, "Accept");
			sceHttpRemoveRequestHeader(tmplId, "Accept-Charset");

			ret = sceKernelLockMutex(s_clientListMutex, 1, NULL);
			if (ret >= 0) {
				bool duplicate = false;
				for (NpCommunityBackendClient *cur = s_clientListHead.next;
				     cur != NULL; cur = cur->next) {
					if (sceClibStrncasecmp(cur->onlineId.data,
							       client->onlineId.data, 16U) == 0 &&
					    cur->markedForRemoval == 0) {
						duplicate = true;
						break;
					}
				}

				if (duplicate) {
					ret = NP_COMMUNITY_ERR_CLIENT_EXISTS;
				} else {
					NpCommunityBackendClient *prevNode = &s_clientListHead;
					NpCommunityBackendClient *scan = s_clientListHead.next;
					while (scan != NULL &&
					       scan->clientId == prevNode->clientId + 1) {
						prevNode = scan;
						scan = scan->next;
					}
					if (prevNode->clientId <= NP_COMMUNITY_CLIENT_ID_MAX) {
						client->clientId = prevNode->clientId + 1;
						client->prev = prevNode;
						client->next = prevNode->next;
						if (prevNode->next != NULL)
							prevNode->next->prev = client;
						prevNode->next = client;
						ret = client->clientId;
					} else {
						ret = NP_COMMUNITY_ERR_ID_EXHAUSTED;
					}
				}
				sceKernelUnlockMutex(s_clientListMutex, 1);
			}
			if (ret >= 0)
				return client->clientId;
		}
	}

	npCommunityDestroyClientResources(client);
	return ret;
}

/* 0x8132A708 */
static int npCommunityClientRelease(NpCommunityBackendClient *client)
{
	if (client == NULL)
		return 0;

	int ret = sceKernelLockMutex(s_clientListMutex, 1, NULL);
	if (ret < 0)
		return ret;

	--client->refCount;
	if (client->markedForRemoval == 1 && client->refCount == 0) {
		if (client->prev != NULL)
			client->prev->next = client->next;
		if (client->next != NULL)
			client->next->prev = client->prev;
		npCommunityDestroyClientResources(client);
	}
	return sceKernelUnlockMutex(s_clientListMutex, 1);
}

/* 0x8132A794 */
static int npCommunityClientAcquire(const char *onlineIdName,
				    NpCommunityBackendClient **outClient)
{
	*outClient = NULL;
	if (onlineIdName == NULL)
		return NP_COMMUNITY_ERR_INVALID_ARG;

	int ret = sceKernelLockMutex(s_clientListMutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpCommunityBackendClient *found = NULL;
	for (NpCommunityBackendClient *cur = s_clientListHead.next; cur != NULL; cur = cur->next) {
		if (sceClibStrncasecmp(cur->onlineId.data, onlineIdName, 16U) == 0 &&
		    cur->markedForRemoval == 0) {
			++cur->refCount;
			*outClient = cur;
			found = cur;
			ret = 0;
			break;
		}
	}

	sceKernelUnlockMutex(s_clientListMutex, 1);
	if (found == NULL)
		return NP_COMMUNITY_ERR_CLIENT_NOT_FOUND;
	return ret;
}

/* 0x8132A81C */
static int npCommunityClientDestroy(const char *onlineIdName)
{
	NpCommunityBackendClient *client = NULL;
	int ret = npCommunityClientAcquire(onlineIdName, &client);
	if (ret < 0) {
		if (client != NULL)
			npCommunityClientRelease(client);
		return ret;
	}

	ret = sceKernelLockMutex(s_clientListMutex, 1, NULL);
	if (ret < 0) {
		npCommunityClientRelease(client);
		return ret;
	}

	client->markedForRemoval = 1;
	sceKernelUnlockMutex(s_clientListMutex, 1);
	npCommunityClientRelease(client);
	return 0;
}

/* 0x8132A8F4 */
static int npCommunityBackendInitInternal(void)
{
	if (s_backendInitialized == 1)
		return NP_COMMUNITY_ERR_ALREADY_INIT;

	int ret = npCommunityMemoryInit();
	if (ret < 0)
		return ret;

	ret = npCommunityClientListInit();
	if (ret < 0) {
		npCommunityMemoryTerm();
		return ret;
	}

	ret = npCommunityTxListInit();
	if (ret < 0) {
		npCommunityClientListTerm();
		npCommunityMemoryTerm();
		return ret;
	}

	s_backendInitialized = 1;
	return 0;
}

/* 0x8132A94C */
static int npCommunityBackendIsInitialized(void)
{
	return s_backendInitialized;
}

/* 0x8132A958 */
static int npCommunityTxListInit(void)
{
	if (s_txListInitialized != 0)
		return NP_COMMUNITY_ERR_ALREADY_INIT;

	sceClibMemset(&s_txListHead, 0, sizeof(s_txListHead));
	s_txListHead.txId = NP_COMMUNITY_TX_ID_SENTINEL;
	SceUID mutexId = sceKernelCreateMutex("SceNpCommunityTransactionList", 2U, 0, NULL);
	if (mutexId < 0)
		return mutexId;

	s_txListMutex = mutexId;
	s_txListInitialized = 1;
	return 0;
}

/* 0x8132A9BC */
int npCommunityReleaseBackendTx(NpCommunityBackendTx *tx)
{
	int ret = sceKernelLockMutex(s_txListMutex, 1, NULL);
	if (ret < 0)
		return ret;

	--tx->refCount;
	if (tx->markedForRemoval == 1 && tx->refCount == 0)
		npCommunityDestroyTxNode(tx);
	return sceKernelUnlockMutex(s_txListMutex, 1);
}

/* 0x8132AA32 */
int npCommunityAcquireBackendTx(SceInt32 txId, NpCommunityBackendTx **outTx)
{
	*outTx = NULL;
	if (txId == 0)
		return NP_COMMUNITY_ERR_TX_NOT_FOUND;

	int ret = sceKernelLockMutex(s_txListMutex, 1, NULL);
	if (ret < 0)
		return ret;

	NpCommunityBackendTx *cur = &s_txListHead;
	while (cur != NULL) {
		if (cur->txId == txId) {
			++cur->refCount;
			*outTx = cur;
			ret = 0;
			break;
		}
		if (cur->txId > txId) {
			*outTx = NULL;
			ret = NP_COMMUNITY_ERR_TX_NOT_FOUND;
			break;
		}
		cur = cur->next;
	}

	sceKernelUnlockMutex(s_txListMutex, 1);
	if (cur == NULL)
		return NP_COMMUNITY_ERR_TX_NOT_FOUND;
	return ret;
}

/* 0x8132AAB8 */
static int npCommunityTxDestroyByOwner(SceInt32 txId, int ownerKind)
{
	NpCommunityBackendTx *tx = NULL;
	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0) {
		if (tx->ownerKind != ownerKind) {
			ret = NP_COMMUNITY_ERR_TX_NOT_FOUND;
		} else {
			npCommunityAbortTx(txId);
			ret = sceKernelLockMutex(s_txListMutex, 1, NULL);
			if (ret >= 0) {
				if (tx->prev != NULL)
					tx->prev->next = tx->next;
				if (tx->next != NULL)
					tx->next->prev = tx->prev;
				tx->markedForRemoval = 1;
				sceKernelUnlockMutex(s_txListMutex, 1);
				ret = 0;
			}
		}
	}

	if (tx != NULL)
		npCommunityReleaseBackendTx(tx);
	return ret;
}

/* 0x8132AC8A */
int npCommunityTrackTxAlloc(NpCommunityBackendTx *tx, void *allocPtr)
{
	if (allocPtr == NULL)
		return 0;

	NpCommunityAllocNode *node = static_cast<NpCommunityAllocNode *>(
		npCommunityAlloc(sizeof(NpCommunityAllocNode)));
	if (node == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	int ret = sceKernelLockMutex(s_txListMutex, 1, NULL);
	if (ret < 0) {
		npCommunityFree(node);
		return ret;
	}

	node->ptr = allocPtr;
	node->next = tx->allocListHead;
	tx->allocListHead = node;
	sceKernelUnlockMutex(s_txListMutex, 1);
	return 0;
}

/* 0x8132ACEE */
static int npCommunityTxCreateInternal(NpCommunityBackendClient *client, int serviceType,
				       int ownerKind, void *serviceParam)
{
	NpCommunityBackendTx *tx = static_cast<NpCommunityBackendTx *>(
		npCommunityAlloc(sizeof(NpCommunityBackendTx)));
	if (tx == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(tx, 0, sizeof(NpCommunityBackendTx));
	tx->client = client;
	tx->serviceType = serviceType;
	tx->serviceParam = serviceParam;
	tx->ownerKind = ownerKind;

	int ret = 0;
	if (serviceParam != NULL)
		ret = npCommunityTrackTxAlloc(tx, serviceParam);

	if (ret >= 0) {
		ret = sceKernelLockMutex(s_txListMutex, 1, NULL);
		if (ret >= 0) {
			NpCommunityBackendTx *tail = &s_txListHead;
			while (tail->next != NULL)
				tail = tail->next;

			++s_nextTxId;
			++s_activeTxCount;
			if (tail->txId > NP_COMMUNITY_TX_ID_WRAP_LIMIT)
				s_nextTxId = NP_COMMUNITY_TX_ID_START;

			tx->txId = s_nextTxId;
			tx->next = NULL;
			tx->prev = tail;
			tail->next = tx;
			sceKernelUnlockMutex(s_txListMutex, 1);
			return tx->txId;
		}
	}

	NpCommunityAllocNode *allocHead = tx->allocListHead;
	tx->allocListHead = NULL;
	npCommunityFreeAllocList(allocHead);
	npCommunityFree(tx);
	return ret;
}

/* 0x8132AE28 */
int npCommunityGetTxHttpTemplate(SceInt32 txId)
{
	NpCommunityBackendTx *tx = NULL;
	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0)
		ret = tx->client->httpTemplateId;
	if (tx != NULL)
		npCommunityReleaseBackendTx(tx);
	return ret;
}

/* 0x8132AF28 */
int npCommunityCloseTxHttp(NpCommunityBackendTx *tx)
{
	if (tx->httpReqId > 0) {
		sceHttpDeleteRequest(tx->httpReqId);
		tx->httpReqId = 0;
	}
	if (tx->httpConnId > 0) {
		sceHttpDeleteConnection(tx->httpConnId);
		tx->httpConnId = 0;
	}
	return 0;
}

/* 0x8132AF4C */
int npCommunityBuildUrl(char **outUrl, SceInt32 *outMethod, NpCommunityBackendTx *tx,
			int endpointId, const void *endpointParam)
{
	const char *envName = tx->client->envName;
	const char *countryCode = tx->client->countryCode;
	const char *hostParam = static_cast<const char *>(endpointParam);
	char *url = NULL;
	SceInt32 method = SCE_HTTP_METHOD_POST;
	int ret = 0;

	switch (endpointId) {
	case NP_COMMUNITY_EP_GET_SELF_PROFILE:
		ret = npCommunityAllocUrlParts(&url, "https://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/sec/get_self_profile");
		break;
	case NP_COMMUNITY_EP_REGISTER_SELF_PROFILE:
		ret = npCommunityAllocUrlParts(&url, "https://", "profile.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/register_self_profile");
		break;
	case NP_COMMUNITY_EP_GET_PROFILE:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", hostParam, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_profile");
		break;
	case NP_COMMUNITY_EP_SEARCH_JID_RES:
		method = SCE_HTTP_METHOD_GET;
		ret = npCommunityAllocUrlParts(&url, "http://", "static-resource.", envName,
					       NP_COMMUNITY_DOMAIN_SUFFIX, "/np/resource/basic/",
					       countryCode, "_searchjid.xml");
		break;
	case NP_COMMUNITY_EP_SEARCH_JID:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/basic_view/func/search_jid", NULL, NULL, NULL,
					       NULL);
		break;
	case NP_COMMUNITY_EP_TROPHY_STATUS:
		method = SCE_HTTP_METHOD_GET;
		if (endpointParam != NULL)
			return NP_COMMUNITY_ERR_UNEXPECTED_PARAM;
		ret = npCommunityAllocUrlParts(&url, "http://", "static-resource.", envName,
					       NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/np/resource/trophy/trophy-status.xml", NULL, NULL);
		break;
	case NP_COMMUNITY_EP_RANKING_SERVER_LIST:
	case NP_COMMUNITY_EP_STORAGE_SERVER_LIST:
		method = SCE_HTTP_METHOD_GET;
		if (endpointParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		{
			char commIdText[16];
			sceClibMemset(commIdText, 0, 13U);
			ret = SceNpCommon_E426FEE8(
				static_cast<const SceNpCommunicationId *>(endpointParam),
				commIdText, 13U);
			if (ret < 0)
				return ret;

			const char *subDir = (endpointId == NP_COMMUNITY_EP_RANKING_SERVER_LIST)
						     ? "/ranking/"
						     : "/storage/";
			const char *suffix = (endpointId == NP_COMMUNITY_EP_RANKING_SERVER_LIST)
						     ? "-ranking.xml"
						     : "-storage.xml";
			SceSize totalLen = sceClibStrnlen("http://", 0x7FFFFFFFU) +
					   sceClibStrnlen("static-resource.", 0x7FFFFFFFU) +
					   sceClibStrnlen(envName, 0x7FFFFFFFU) +
					   sceClibStrnlen(NP_COMMUNITY_DOMAIN_SUFFIX, 0x7FFFFFFFU) +
					   sceClibStrnlen("/np/resource/psp2-title/", 0x7FFFFFFFU) +
					   sceClibStrnlen(commIdText, 0x7FFFFFFFU) +
					   sceClibStrnlen(subDir, 0x7FFFFFFFU) +
					   sceClibStrnlen(commIdText, 0x7FFFFFFFU) +
					   sceClibStrnlen(suffix, 0x7FFFFFFFU) + 1U;
			url = static_cast<char *>(npCommunityAlloc(totalLen));
			if (url == NULL)
				return NP_COMMUNITY_ERR_NO_MEMORY;
			sceClibMemset(url, 0, totalLen);
			sceClibStrncpy(url, "http://", 7U);
			npCommunityAppendBounded(url, "static-resource.", totalLen);
			npCommunityAppendBounded(url, envName, totalLen);
			npCommunityAppendBounded(url, NP_COMMUNITY_DOMAIN_SUFFIX, totalLen);
			npCommunityAppendBounded(url, "/np/resource/psp2-title/", totalLen);
			npCommunityAppendBounded(url, commIdText, totalLen);
			npCommunityAppendBounded(url, subDir, totalLen);
			npCommunityAppendBounded(url, commIdText, totalLen);
			npCommunityAppendBounded(url, suffix, totalLen);
		}
		break;
	case NP_COMMUNITY_EP_RECORD_SCORE:
		if (endpointParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(
			&url, "https://",
			static_cast<const NpScoreServerHosts *>(endpointParam)->recordHost,
			"/ranking_update/sec/record_score", NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_RECORD_GAME_DATA:
		if (endpointParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(
			&url, "https://",
			static_cast<const NpScoreServerHosts *>(endpointParam)->recordHost,
			"/ranking_update/sec/record_data", NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_GET_GAME_DATA:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/get_game_data", NULL, NULL,
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_GET_PLAYER_RANK:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/get_player_rank", NULL, NULL,
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_GET_RANKING:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/get_ranking", NULL, NULL, NULL,
					       NULL);
		break;
	case NP_COMMUNITY_EP_GET_BOARD_INFO:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/get_board_info", NULL, NULL,
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_UPLOADER_UPLOAD:
		ret = npCommunityAllocUrlParts(&url, "https://", "uploader.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/uploader/sec/upload_data");
		break;
	case NP_COMMUNITY_EP_UPLOADER_DOWNLOAD:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "https://", "uploader.", hostParam, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/uploader/sec/download_data");
		break;
	case NP_COMMUNITY_EP_UPLOADER_LIST:
		ret = npCommunityAllocUrlParts(&url, "https://", "uploader.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/uploader/sec/get_data_list");
		break;
	case NP_COMMUNITY_EP_UPLOADER_QUOTA:
		ret = npCommunityAllocUrlParts(&url, "https://", "uploader.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/uploader/sec/get_quota");
		break;
	case NP_COMMUNITY_EP_UPLOADER_DELETE:
		ret = npCommunityAllocUrlParts(&url, "https://", "uploader.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/uploader/sec/delete_data");
		break;
	case NP_COMMUNITY_EP_UPDATE_PTL:
		ret = npCommunityAllocUrlParts(&url, "https://", "updptl.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/update_ptl");
		break;
	case NP_COMMUNITY_EP_UPDATE_CI:
		ret = npCommunityAllocUrlParts(&url, "https://", "updptl.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/update_ci");
		break;
	case NP_COMMUNITY_EP_UPDATE_MI:
		ret = npCommunityAllocUrlParts(&url, "https://", "updptl.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/update_mi");
		break;
	case NP_COMMUNITY_EP_CENSOR_COMMENT:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/censor_comment", NULL, NULL,
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_SANITIZE_COMMENT:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_view/func/sanitize_comment", NULL, NULL,
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_TSS_GET_DATA:
		method = SCE_HTTP_METHOD_GET;
		if (endpointParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		{
			const NpCommunityTssUrlParam *tssParam =
				static_cast<const NpCommunityTssUrlParam *>(endpointParam);
			char commIdText[16];
			char slotText[12];
			sceClibMemset(commIdText, 0, 13U);
			ret = SceNpCommon_E426FEE8(tssParam->commId, commIdText, 13U);
			if (ret < 0)
				return ret;

			sceClibMemset(slotText, 0, 11U);
			npCommunityFormatInt32(slotText, tssParam->slotId);

			SceSize totalLen = sceClibStrnlen("https://", 0x7FFFFFFFU) +
					   sceClibStrnlen(NP_COMMUNITY_TSS_HOST_PREFIX, 0x7FFFFFFFU) +
					   sceClibStrnlen(envName, 0x7FFFFFFFU) +
					   sceClibStrnlen(NP_COMMUNITY_DL_SUFFIX, 0x7FFFFFFFU) +
					   sceClibStrnlen("/tss/", 0x7FFFFFFFU) +
					   sceClibStrnlen(envName, 0x7FFFFFFFU) + 1U +
					   sceClibStrnlen(commIdText, 0x7FFFFFFFU) + 1U +
					   sceClibStrnlen(commIdText, 0x7FFFFFFFU) + 1U +
					   sceClibStrnlen(slotText, 0x7FFFFFFFU) +
					   sceClibStrnlen(".tss", 0x7FFFFFFFU) + 1U;
			url = static_cast<char *>(npCommunityAlloc(totalLen));
			if (url == NULL)
				return NP_COMMUNITY_ERR_NO_MEMORY;
			sceClibMemset(url, 0, totalLen);
			sceClibStrncpy(url, "https://", 8U);
			npCommunityAppendBounded(url, NP_COMMUNITY_TSS_HOST_PREFIX, totalLen);
			npCommunityAppendBounded(url, envName, totalLen);
			npCommunityAppendBounded(url, NP_COMMUNITY_DL_SUFFIX, totalLen);
			npCommunityAppendBounded(url, "/tss/", totalLen);
			npCommunityAppendBounded(url, envName, totalLen);
			npCommunityAppendBounded(url, "/", totalLen);
			npCommunityAppendBounded(url, commIdText, totalLen);
			npCommunityAppendBounded(url, "/", totalLen);
			npCommunityAppendBounded(url, commIdText, totalLen);
			npCommunityAppendBounded(url, "-", totalLen);
			npCommunityAppendBounded(url, slotText, totalLen);
			npCommunityAppendBounded(url, ".tss", totalLen);
		}
		break;
	case NP_COMMUNITY_EP_CLAN_GET_MEMBER_DATA:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_clan_view/func/get_member_game_data",
					       NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_CLAN_GET_MEMBER_RANK_BY_ID:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_clan_view/func/get_member_ranking_by_id",
					       NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_CLAN_GET_MEMBER_RANK_BY_RANGE:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(
			&url, "http://", hostParam,
			"/ranking_clan_view/func/get_member_ranking_by_range", NULL, NULL, NULL,
			NULL);
		break;
	case NP_COMMUNITY_EP_CLAN_GET_RANK_BY_ID:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_clan_view/func/get_clan_ranking_by_id",
					       NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_CLAN_GET_RANK_BY_RANGE:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "http://", hostParam,
					       "/ranking_clan_view/func/get_clan_ranking_by_range",
					       NULL, NULL, NULL, NULL);
		break;
	case NP_COMMUNITY_EP_TUS_SET_DATA:
	case NP_COMMUNITY_EP_TUS_GET_DATA:
	case NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_DATA_STATUS:
	case NP_COMMUNITY_EP_TUS_GET_MULTI_USER_DATA_STATUS:
	case NP_COMMUNITY_EP_TUS_SET_MULTI_SLOT_VAR:
	case NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_VAR:
	case NP_COMMUNITY_EP_TUS_GET_MULTI_USER_VAR:
	case NP_COMMUNITY_EP_TUS_TRY_AND_SET_VAR:
	case NP_COMMUNITY_EP_TUS_ADD_AND_GET_VAR:
	case NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_VAR:
	case NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_DATA:
	case NP_COMMUNITY_EP_TUS_GET_FRIEND_VAR:
	case NP_COMMUNITY_EP_TUS_GET_FRIEND_DATA_STATUS:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		{
			const char *tusPath = NULL;
			switch (endpointId) {
			case NP_COMMUNITY_EP_TUS_SET_DATA:
				tusPath = "/user_storage/sec/set_data";
				break;
			case NP_COMMUNITY_EP_TUS_GET_DATA:
				tusPath = "/user_storage/sec/get_data";
				break;
			case NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_DATA_STATUS:
				tusPath = "/user_storage/sec/get_multi_slot_data_status";
				break;
			case NP_COMMUNITY_EP_TUS_GET_MULTI_USER_DATA_STATUS:
				tusPath = "/user_storage/sec/get_multi_user_data_status";
				break;
			case NP_COMMUNITY_EP_TUS_SET_MULTI_SLOT_VAR:
				tusPath = "/user_storage/sec/set_multi_slot_variable";
				break;
			case NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_VAR:
				tusPath = "/user_storage/sec/get_multi_slot_variable";
				break;
			case NP_COMMUNITY_EP_TUS_GET_MULTI_USER_VAR:
				tusPath = "/user_storage/sec/get_multi_user_variable";
				break;
			case NP_COMMUNITY_EP_TUS_TRY_AND_SET_VAR:
				tusPath = "/user_storage/sec/tryset_variable";
				break;
			case NP_COMMUNITY_EP_TUS_ADD_AND_GET_VAR:
				tusPath = "/user_storage/sec/addget_variable";
				break;
			case NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_VAR:
				tusPath = "/user_storage/sec/delete_multi_slot_variable";
				break;
			case NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_DATA:
				tusPath = "/user_storage/sec/delete_multi_slot_data";
				break;
			case NP_COMMUNITY_EP_TUS_GET_FRIEND_VAR:
				tusPath = "/user_storage/sec/get_friend_user_variable";
				break;
			case NP_COMMUNITY_EP_TUS_GET_FRIEND_DATA_STATUS:
				tusPath = "/user_storage/sec/get_friend_user_data_status";
				break;
			default:
				return NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
			ret = npCommunityAllocUrlParts(&url, "https://", hostParam, tusPath, NULL,
						       NULL, NULL, NULL);
		}
		break;
	case NP_COMMUNITY_EP_GET_AVATAR_CATEGORY:
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_avatar_category");
		break;
	case NP_COMMUNITY_EP_GET_AVATAR_CATEGORY_LIST:
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_avatar_category_list");
		break;
	case NP_COMMUNITY_EP_GET_AVATAR_LIST:
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_avatar_list");
		break;
	case NP_COMMUNITY_EP_GET_PANEL_CATEGORY:
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_panel_category");
		break;
	case NP_COMMUNITY_EP_GET_PANEL_LIST:
		ret = npCommunityAllocUrlParts(&url, "http://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/func/get_panel_list");
		break;
	case NP_COMMUNITY_EP_UPDATE_SELF_PROFILE:
		ret = npCommunityAllocUrlParts(&url, "https://", "profile.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/update_self_profile");
		break;
	case NP_COMMUNITY_EP_BASIC_TTL:
		method = SCE_HTTP_METHOD_GET;
		ret = npCommunityAllocUrlParts(&url, "http://", "static-resource.", envName,
					       NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/np/resource/basic/ttl.xml", NULL, NULL);
		break;
	case NP_COMMUNITY_EP_FRIENDLIST_STATUS:
		method = SCE_HTTP_METHOD_GET;
		ret = npCommunityAllocUrlParts(&url, "http://", "static-resource.", envName,
					       NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/np/resource/friendlist/friendlist-status.xml",
					       NULL, NULL);
		break;
	case NP_COMMUNITY_EP_GET_AVAILABLE_AVATAR_LIST:
		ret = npCommunityAllocUrlParts(&url, "https://", "getprof.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_view/sec/get_available_avatar_list");
		break;
	case NP_COMMUNITY_EP_UPDATE_SELF_AVATAR:
		ret = npCommunityAllocUrlParts(&url, "https://", "profile.", countryCode, ".",
					       envName, NP_COMMUNITY_DOMAIN_SUFFIX,
					       "/basic_update/sec/update_self_avatar");
		break;
	case NP_COMMUNITY_EP_GET_FRIEND_PLAYER_RANK:
		if (hostParam == NULL)
			return NP_COMMUNITY_ERR_NULL_POINTER;
		ret = npCommunityAllocUrlParts(&url, "https://", hostParam,
					       "/ranking_view/sec/get_friend_player_rank", NULL,
					       NULL, NULL, NULL);
		break;
	default:
		return NP_COMMUNITY_ERR_INVALID_ENDPOINT;
	}

	if (ret < 0) {
		if (url != NULL)
			npCommunityFree(url);
		return ret;
	}

	*outUrl = url;
	*outMethod = method;
	return 0;
}

/* 0x8132D8B8 */
static SceInt32 npCommunityHttpAuthCallback(SceInt32 request, SceHttpAuthType authType,
					    const char *realm, char *username, char *password,
					    SceBool needEntity, SceUChar8 **entityBody,
					    SceSize *entitySize, SceBool *save, void *userArg)
{
	(void)request;
	(void)needEntity;
	(void)entityBody;
	(void)entitySize;
	(void)userArg;

	if (save != NULL)
		*save = SCE_FALSE;

	if (authType != SCE_HTTP_AUTH_BASIC || username == NULL || password == NULL)
		return -1;

	if (realm == NULL)
		return -1;

	const char *userText = NULL;
	const SceUInt8 *xorKey = NULL;
	SceSize basicLen = sceClibStrnlen("c7y-basic", 0x7FFFFFFFU);
	SceSize rankingLen = sceClibStrnlen("c7y-ranking", 0x7FFFFFFFU);

	if (sceClibStrncmp(realm, "c7y-basic", basicLen) == 0) {
		userText = "c7y-basic07";
		xorKey = s_basicRealmXorKey;
	} else if (sceClibStrncmp(realm, "c7y-ranking", rankingLen) == 0) {
		userText = "c7y-ranking03";
		xorKey = s_rankingRealmXorKey;
	} else {
		return -1;
	}

	SceSize userLen = sceClibStrnlen(userText, 0x7FFFFFFFU);
	sceClibStrncpy(username, userText, userLen + 1U);
	for (SceSize idx = 0; idx < 32U; ++idx)
		password[idx] = static_cast<char>(xorKey[idx] ^ 0x7FU);
	password[32] = '\0';
	return 0;
}

/* 0x8132D6FA */
static int npCommunityHttpOpenUrl(const char *url, SceInt32 method, SceInt32 connId,
				  NpCommunityBackendTx *tx, SceSize contentLength)
{
	if (tx->httpConnId > 0)
		npCommunityCloseTxHttp(tx);

	SceInt32 activeConnId = connId;
	int ret = 0;
	if (activeConnId == 0) {
		activeConnId = sceHttpCreateConnectionWithURL(tx->client->httpTemplateId, url,
							      SCE_TRUE);
		if (activeConnId < 0) {
			ret = activeConnId;
			npCommunityCloseTxHttp(tx);
			return ret;
		}
		tx->httpConnId = activeConnId;
	}

	SceInt32 reqId = sceHttpCreateRequestWithURL(activeConnId, method, url, 0ULL);
	if (reqId < 0) {
		npCommunityCloseTxHttp(tx);
		return reqId;
	}

	tx->httpReqId = reqId;
	ret = sceHttpSetAuthInfoCallback(reqId, npCommunityHttpAuthCallback, NULL);
	if (ret < 0) {
		npCommunityCloseTxHttp(tx);
		return ret;
	}

	if (tx->timeoutConfig.resolveRetry != 0)
		sceHttpSetResolveRetry(tx->httpReqId, tx->timeoutConfig.resolveRetry);
	if (tx->timeoutConfig.resolveTimeout != 0)
		sceHttpSetResolveTimeOut(tx->httpReqId,
					 static_cast<SceUInt32>(tx->timeoutConfig.resolveTimeout));
	if (tx->timeoutConfig.connTimeout != 0)
		sceHttpSetConnectTimeOut(tx->httpReqId,
					 static_cast<SceUInt32>(tx->timeoutConfig.connTimeout));
	if (tx->timeoutConfig.sendTimeout != 0)
		sceHttpSetSendTimeOut(tx->httpReqId,
				      static_cast<SceUInt32>(tx->timeoutConfig.sendTimeout));
	if (tx->timeoutConfig.recvTimeout != 0)
		sceHttpSetRecvTimeOut(tx->httpReqId,
				      static_cast<SceUInt32>(tx->timeoutConfig.recvTimeout));
	if (contentLength != 0U)
		sceHttpSetRequestContentLength(tx->httpReqId,
					       static_cast<SceULong64>(contentLength));
	return 0;
}

/* 0x8132D80E */
int npCommunityHttpOpenOnConn(NpCommunityBackendTx *tx, SceInt32 connId, int endpointId,
			      const void *endpointParam, SceSize contentLength)
{
	char *url = NULL;
	SceInt32 method = SCE_HTTP_METHOD_GET;
	npCommunityBuildUrl(&url, &method, tx, endpointId, endpointParam);
	int ret = npCommunityHttpOpenUrl(url, method, connId, tx, contentLength);
	if (url != NULL)
		npCommunityFree(url);
	return ret;
}

/* 0x8132D866 */
int npCommunityHttpOpen(NpCommunityBackendTx *tx, int endpointId, const void *endpointParam,
			SceSize contentLength)
{
	char *url = NULL;
	SceInt32 method = SCE_HTTP_METHOD_GET;
	npCommunityBuildUrl(&url, &method, tx, endpointId, endpointParam);
	int ret = npCommunityHttpOpenUrl(url, method, 0, tx, contentLength);
	if (url != NULL)
		npCommunityFree(url);
	return ret;
}

/* 0x8132D9C8 */
static int npCommunityMemoryInit(void)
{
	if (s_memoryInitialized == 1)
		return NP_COMMUNITY_ERR_ALREADY_INIT;
	s_memoryInitialized = 1;
	return 0;
}

/* 0x8132D9E6 */
static int npCommunityMemoryTerm(void)
{
	if (s_memoryInitialized == 0)
		return NP_COMMUNITY_ERR_NOT_INIT;
	s_memoryInitialized = 0;
	return 0;
}

/* 0x8132DA00 */
void *npCommunityAlloc(SceSize size)
{
	return sceNpCommonMallocNpServerName(size);
}

/* 0x8132DA08 */
void npCommunityFree(void *ptr)
{
	sceNpCommonFreeNpServerName(ptr);
}
