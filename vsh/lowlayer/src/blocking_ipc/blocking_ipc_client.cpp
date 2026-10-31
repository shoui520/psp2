#include <libkernel/libkernel.h>
#include "blocking_ipc_client.h"
#include "../ipmi/assert.h"
#include "../ipmi/ipmi_error.h"
#include "../ipmi/session.h"

void *operator new(size_t size, void *where);

namespace BlockingIpc {

int Client::initialize(const Config *pConfig)
{
	char name[32];
	SceUInt32 i;
	int ret;

	sceClibSnprintf(name, sizeof(name), "%sBlockingIpc", pConfig->name);
	ret = sceKernelCreateLwMutex(&m_mutex, name, 0, 0, NULL);
	if (ret != 0)
		return ret;

	IPMI::Client::Config clientConfig;

	sceClibStrncpy(clientConfig.serviceName, pConfig->name, sizeof(clientConfig.serviceName));
	clientConfig.budget = pConfig->budget;
	clientConfig.syncCallCount = pConfig->syncCallCount;
	clientConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	clientConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	clientConfig.asyncCallCount = pConfig->asyncCallCount + pConfig->blockingIpcCount;
	clientConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize > pConfig->maxBlockingInSize ?
		pConfig->maxAsyncCallInSize : pConfig->maxBlockingInSize;
	clientConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize > pConfig->maxBlockingOutSize ?
		pConfig->maxAsyncCallOutSize : pConfig->maxBlockingOutSize;
	clientConfig.eventFlagCount = pConfig->eventFlagCount + pConfig->blockingIpcCount;
	clientConfig.msgQueueCount = pConfig->msgQueueCount;
	for (i = 0; i < pConfig->msgQueueCount; i++)
		clientConfig.msgQueueBufferSize[i] =
			(pConfig->msgQueue[i].msgSize + 16) * pConfig->msgQueue[i].msgCount;

	ret = IPMI::Client::create(&m_pClient, &clientConfig, NULL, this + 1);
	if (ret != 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		return ret;
	}
	m_connected = 0;
	m_pSlots = (Slot *)((char *)(this + 1) +
			    ((IPMI::Client::getBufferSize(&clientConfig) + 15) & ~15));
	m_maxActive = pConfig->blockingIpcCount;
	m_slotCount = pConfig->slotCount != 0 ? pConfig->slotCount : pConfig->blockingIpcCount;
	for (i = 0; i < m_slotCount; i++) {
		SceUID sema;

		m_pSlots[i].requestId = -1;
		m_pSlots[i].state = SLOT_FREE;
		m_pSlots[i].pNext = NULL;
		sceClibSnprintf(name, sizeof(name), "%sBlockingIpc%u", pConfig->name, i);
		sema = sceKernelCreateSema(name, SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0, 1, NULL);
		if (sema < 0) {
			SceUInt32 j;

			ret = m_pClient->destroy();
			IPMI_ASSERT_SUCCESS(ret);
			m_pClient = NULL;
			for (j = 0; j < i; j++) {
				ret = sceKernelDeleteSema(m_pSlots[j].sema);
				IPMI_ASSERT_SUCCESS(ret);
				m_pSlots[j].sema = -1;
			}
			ret = sceKernelDeleteLwMutex(&m_mutex);
			IPMI_ASSERT_SUCCESS(ret);
			return sema;
		}
		m_pSlots[i].sema = sema;
	}
	m_activeCount = 0;
	m_eventFlagBase = pConfig->eventFlagCount;
	m_pWaitQueue = NULL;
	return 0;
}

int Client::create(Client **ppClient, const Config *pConfig, void *pMemory)
{
	Client *pClient = new (pMemory) Client;
	int ret;

	ret = pClient->initialize(pConfig);
	if (ret == 0)
		*ppClient = pClient;
	return ret;
}

int Client::destroy()
{
	SceUInt32 i;
	int ret;

	if (m_connected != 0)
		return BLOCKING_IPC_ERROR_ALREADY_CONNECTED;
	if (m_pClient != NULL) {
		ret = m_pClient->destroy();
		IPMI_ASSERT_SUCCESS(ret);
		m_pClient = NULL;
	}
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].sema != -1) {
			ret = sceKernelDeleteSema(m_pSlots[i].sema);
			IPMI_ASSERT_SUCCESS(ret);
			m_pSlots[i].sema = -1;
		}
	}
	ret = sceKernelDeleteLwMutex(&m_mutex);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

int Client::connect()
{
	int result;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 0) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_ALREADY_CONNECTED;
	}
	int error = 0;

	ret = m_pClient->connect(NULL, 0, &result);
	if (ret != 0)
		error = ret;
	else if (result != 0)
		error = result;
	else
		m_connected = 1;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return error;
}

int Client::disconnect()
{
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_CONNECTED;
	}
	int error = m_pClient->disconnect();

	if (error == 0)
		m_connected = 0;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return error;
}

int Client::createRequest(int *pRequestId, const void *pParam, SceSize paramSize)
{
	Slot *pSlot;
	SceUInt32 i;
	int requestId;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_READY;
	}
	pSlot = NULL;
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].state == SLOT_FREE) {
			pSlot = &m_pSlots[i];
			break;
		}
	}
	if (pSlot == NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NO_FREE_REQUEST;
	}
	pSlot->state = SLOT_CREATED;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);

	ret = m_pClient->invokeSyncMethod(METHOD_CREATE_REQUEST, pParam, paramSize, &requestId,
					  NULL, NULL, 0);
	IPMI_ASSERT_SUCCESS(ret);

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (requestId < 0) {
		pSlot->state = SLOT_FREE;
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return requestId;
	}
	pSlot->requestId = requestId;
	*pRequestId = requestId;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

int Client::destroyRequest(int requestId)
{
	Slot *pSlot;
	SceUInt32 i;
	int result;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_READY;
	}
	pSlot = NULL;
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].state != SLOT_FREE && m_pSlots[i].requestId == requestId) {
			pSlot = &m_pSlots[i];
			break;
		}
	}
	if (pSlot == NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND;
	}
	if (pSlot->state != SLOT_CREATED && pSlot->state != SLOT_ABORTED) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_BUSY;
	}
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);

	ret = m_pClient->invokeSyncMethod(METHOD_DESTROY_REQUEST, &requestId, sizeof(requestId),
					  &result, NULL, NULL, 0);
	IPMI_ASSERT_SUCCESS(ret);

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (result == 0) {
		pSlot->state = SLOT_FREE;
		pSlot->requestId = -1;
	}
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return result;
}

int Client::abortRequest(int requestId)
{
	Slot *pSlot;
	SceUInt32 i;
	int result;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_READY;
	}
	pSlot = NULL;
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].state != SLOT_FREE && m_pSlots[i].requestId == requestId) {
			pSlot = &m_pSlots[i];
			break;
		}
	}
	if (pSlot == NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND;
	}
	if (pSlot->state == SLOT_ABORTED) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
	} else {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);

		ret = m_pClient->invokeSyncMethod(METHOD_ABORT_REQUEST, &requestId,
						  sizeof(requestId), &result, NULL, NULL, 0);
		IPMI_ASSERT_SUCCESS(ret);

		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		IPMI_ASSERT_SUCCESS(ret);
		if (result == 0 && pSlot->state != SLOT_FREE && pSlot->requestId == requestId)
			pSlot->state = SLOT_ABORTED;
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
	}
	return 0;
}

int Client::beginBlockingIpc(int requestId, Handle *pHandle)
{
	Slot *pSlot;
	SceUInt32 i;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_READY;
	}
	pSlot = NULL;
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].state != SLOT_FREE && m_pSlots[i].requestId == requestId) {
			pSlot = &m_pSlots[i];
			break;
		}
	}
	if (pSlot == NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND;
	}
	if (pSlot->state == SLOT_WAITING || pSlot->state == SLOT_ACTIVE) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_BUSY;
	}
	if (pSlot->state == SLOT_ABORTED) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_ABORTED;
	}
	if (m_activeCount == m_maxActive) {
		pSlot->state = SLOT_WAITING;
		pSlot->pNext = NULL;
		Slot *pLast = m_pWaitQueue;

		if (pLast != NULL) {
			for (; pLast->pNext != NULL; pLast = pLast->pNext)
				;
			pLast->pNext = pSlot;
		} else {
			m_pWaitQueue = pSlot;
		}
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		ret = sceKernelWaitSema(pSlot->sema, 1, NULL);
		IPMI_ASSERT_SUCCESS(ret);
		ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
		IPMI_ASSERT_SUCCESS(ret);
	} else {
		m_activeCount++;
	}
	pSlot->state = SLOT_ACTIVE;
	pHandle->bitPattern = 1;
	pHandle->eventFlagIndex = m_eventFlagBase + i;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

int Client::endBlockingIpc(int requestId)
{
	Slot *pSlot;
	SceUInt32 i;
	int error;
	int ret;

	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_connected != 1) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_NOT_READY;
	}
	pSlot = NULL;
	for (i = 0; i < m_slotCount; i++) {
		if (m_pSlots[i].state != SLOT_FREE && m_pSlots[i].requestId == requestId) {
			pSlot = &m_pSlots[i];
			break;
		}
	}
	if (pSlot == NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND;
	}
	switch (pSlot->state) {
	case SLOT_CREATED:
		error = BLOCKING_IPC_ERROR_NOT_BEGUN;
		break;
	case SLOT_ACTIVE:
		pSlot->state = SLOT_CREATED;
		/* fall through */
	case SLOT_ABORTED:
		error = 0;
		if (m_pWaitQueue != NULL) {
			Slot *pNext = m_pWaitQueue;

			m_pWaitQueue = pNext->pNext;
			ret = sceKernelSignalSema(pNext->sema, 1);
			IPMI_ASSERT_SUCCESS(ret);
		} else {
			m_activeCount--;
		}
		break;
	default:
		error = BLOCKING_IPC_ERROR_INVALID_STATE;
		break;
	}
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return error;
}

int Client::waitBlockingIpc(const Handle *pHandle, SceUInt32 *pTimeout)
{
	SceUInt32 resultPattern;
	int ret;

	if (m_connected != 1)
		return BLOCKING_IPC_ERROR_NOT_READY;
	ret = m_pClient->waitEventFlag(pHandle->eventFlagIndex, pHandle->bitPattern,
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
				       &resultPattern, pTimeout);
	if (ret == IPMI_ERROR_TIMEOUT)
		ret = BLOCKING_IPC_ERROR_TIMEOUT;
	return ret;
}

IPMI::Client *Client::getClient()
{
	return m_pClient;
}

SceSize Client::getBufferSize(const Config *pConfig)
{
	SceUInt32 slotCount = pConfig->slotCount != 0 ? pConfig->slotCount :
		pConfig->blockingIpcCount;
	IPMI::Client::Config clientConfig;
	int i;

	clientConfig.syncCallCount = pConfig->syncCallCount;
	clientConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	clientConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	clientConfig.asyncCallCount = pConfig->asyncCallCount + pConfig->blockingIpcCount;
	clientConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize > pConfig->maxBlockingInSize ?
		pConfig->maxAsyncCallInSize : pConfig->maxBlockingInSize;
	clientConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize > pConfig->maxBlockingOutSize ?
		pConfig->maxAsyncCallOutSize : pConfig->maxBlockingOutSize;
	clientConfig.eventFlagCount = pConfig->eventFlagCount + slotCount;
	clientConfig.msgQueueCount = pConfig->msgQueueCount;
	for (i = 0; i < 32; i++)
		clientConfig.msgQueueBufferSize[i] =
			(pConfig->msgQueue[i].msgSize + 16) * pConfig->msgQueue[i].msgCount;
	return ((IPMI::Client::getBufferSize(&clientConfig) + 15) & ~15) +
		slotCount * sizeof(Slot) + sizeof(Client);
}

SceSize Client::getSessionBufferSize(const Config *pConfig)
{
	IPMI::Session::Config sessionConfig;

	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount + pConfig->blockingIpcCount;
	sessionConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize > pConfig->maxBlockingInSize ?
		pConfig->maxAsyncCallInSize : pConfig->maxBlockingInSize;
	sessionConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize > pConfig->maxBlockingOutSize ?
		pConfig->maxAsyncCallOutSize : pConfig->maxBlockingOutSize;
	sessionConfig.eventFlagCount = pConfig->eventFlagCount + pConfig->blockingIpcCount;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	return IPMI::Session::getBufferSize(&sessionConfig);
}

}
