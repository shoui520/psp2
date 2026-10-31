#include <libkernel/libkernel.h>
#include "ipmi_wrapper_client.h"
#include "../ipmi/assert.h"

void *operator new(size_t size, void *where);

namespace IpmiWrapper {

int Client::initialize(const Config *pConfig)
{
	char name[32];
	SceUInt32 i;
	int ret;

	sceClibSnprintf(name, sizeof(name), "%sIpmiWrapper", pConfig->name);
	ret = sceKernelCreateLwMutex(&m_mutex, name, 0, 0, NULL);
	if (ret != 0)
		return ret;

	IPMI::Client::Config clientConfig;

	sceClibStrncpy(clientConfig.serviceName, pConfig->name, sizeof(clientConfig.serviceName));
	clientConfig.budget = pConfig->budget;
	clientConfig.syncCallCount = pConfig->syncCallCount;
	clientConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	clientConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	clientConfig.asyncCallCount = pConfig->asyncCallCount;
	clientConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	clientConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	clientConfig.eventFlagCount = (pConfig->callbackCount + 31) >> 5;
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
	m_pCallbacks = (CallbackEntry *)((char *)(this + 1) +
					 ((IPMI::Client::getBufferSize(&clientConfig) + 15) & ~15));
	m_callbackCount = pConfig->callbackCount;
	for (i = 0; i < m_callbackCount; i++) {
		m_pCallbacks[i].callback = NULL;
		m_pCallbacks[i].pArg = NULL;
	}
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
	int ret;

	if (m_connected != 0)
		return IPMI_WRAPPER_ERROR_ALREADY_CONNECTED;
	ret = m_pClient->destroy();
	IPMI_ASSERT_SUCCESS(ret);
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
		return IPMI_WRAPPER_ERROR_ALREADY_CONNECTED;
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
		return IPMI_WRAPPER_ERROR_NOT_CONNECTED;
	}
	int error = m_pClient->disconnect();

	if (error == 0)
		m_connected = 0;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return error;
}

/* Clears the callback's bit; the caller holds the mutex. */
void Client::cancel_callback_nolock(SceUInt32 index)
{
	SceUInt32 resultPattern;
	int ret;

	ret = m_pClient->pollEventFlag(index >> 5, 1 << (index & 31),
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       &resultPattern);
	IPMI_ASSERT(ret == 0 || ret == SCE_KERNEL_ERROR_EVF_COND);
}

int Client::registerCallback(SceUInt32 index, Callback callback, void *pArg)
{
	int ret;

	if (index >= m_callbackCount)
		return IPMI_WRAPPER_ERROR_INVALID_INDEX;
	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	if (m_pCallbacks[index].callback != NULL) {
		ret = sceKernelUnlockLwMutex(&m_mutex, 1);
		IPMI_ASSERT_SUCCESS(ret);
		return IPMI_WRAPPER_ERROR_ALREADY_CONNECTED;
	}
	cancel_callback_nolock(index);
	m_pCallbacks[index].callback = callback;
	m_pCallbacks[index].pArg = pArg;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

int Client::unregisterCallback(SceUInt32 index)
{
	int ret;

	if (index >= m_callbackCount)
		return IPMI_WRAPPER_ERROR_INVALID_INDEX;
	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	cancel_callback_nolock(index);
	m_pCallbacks[index].callback = NULL;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

int Client::checkCallback(SceUInt32 index)
{
	SceUInt32 resultPattern;
	SceUInt32 bitPattern;
	Callback callback;
	void *pArg;
	int error;
	int ret;

	if (index >= m_callbackCount)
		return IPMI_WRAPPER_ERROR_INVALID_INDEX;
	if (m_pCallbacks[index].callback == NULL)
		return IPMI_WRAPPER_ERROR_NO_CALLBACK;
	bitPattern = 1 << (index & 31);
	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	error = m_pClient->pollEventFlag(index >> 5, bitPattern,
					 SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					 &resultPattern);
	callback = m_pCallbacks[index].callback;
	pArg = m_pCallbacks[index].pArg;
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	if (error != 0) {
		IPMI_ASSERT(error == SCE_KERNEL_ERROR_EVF_COND);
		error = IPMI_WRAPPER_ERROR_NOT_SIGNALED;
	} else if ((resultPattern & bitPattern) != 0) {
		error = callback(index, pArg);
	}
	return error;
}

int Client::cancelCallback(SceUInt32 index)
{
	int ret;

	if (index >= m_callbackCount)
		return IPMI_WRAPPER_ERROR_INVALID_INDEX;
	ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	IPMI_ASSERT_SUCCESS(ret);
	cancel_callback_nolock(index);
	ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	IPMI_ASSERT_SUCCESS(ret);
	return 0;
}

IPMI::Client *Client::getClient()
{
	return m_pClient;
}

SceSize Client::getBufferSize(const Config *pConfig)
{
	IPMI::Client::Config clientConfig;
	int i;

	clientConfig.syncCallCount = pConfig->syncCallCount;
	clientConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	clientConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	clientConfig.asyncCallCount = pConfig->asyncCallCount;
	clientConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	clientConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	clientConfig.eventFlagCount = (pConfig->callbackCount + 31) >> 5;
	clientConfig.msgQueueCount = pConfig->msgQueueCount;
	for (i = 0; i < 32; i++)
		clientConfig.msgQueueBufferSize[i] =
			(pConfig->msgQueue[i].msgSize + 16) * pConfig->msgQueue[i].msgCount;
	/* 16 bytes per callback, twice what a CallbackEntry takes. */
	return ((IPMI::Client::getBufferSize(&clientConfig) + 15) & ~15) +
		16 * pConfig->callbackCount + sizeof(Client);
}

}
