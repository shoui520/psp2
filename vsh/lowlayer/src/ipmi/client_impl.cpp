#include <libkernel/libkernel.h>
#include <sce_atomic.h>
#include <appmgr.h>
#define _Static_assert(expression, message)
#include <appmgr/appmgr.h>
#include <processmgr/processmgr.h>
#undef _Static_assert
#include "client_impl.h"
#include "assert.h"
#include "ipc_name.h"
#include "ipmi_error.h"
#include "message.h"
#include "suspend_lock.h"

void *operator new(size_t size, void *where);

namespace IPMI {
namespace impl {

/* The client indexes in use in this process, and the one handed out last. */
static int32_t s_lastClientIndex = 31;
static volatile int32_t s_clientIndexMap;

static inline int release_client_index(SceUInt32 index)
{
	int32_t bit;
	int32_t map;

	if (index > 32)
		return IPMI_ERROR_INVALID_INDEX;
	bit = 1 << index;
	for (;;) {
		map = s_clientIndexMap;
		if ((map & bit) == 0)
			return IPMI_ERROR_NOT_FOUND;
		if (map == sceAtomicCompareAndSwap32AcqRel(&s_clientIndexMap, map, map & ~bit))
			return 0;
	}
}

SceSize ClientImpl::getBufferSize(const Config *pConfig)
{
	return sizeof(AsyncCall) * pConfig->asyncCallCount +
		sizeof(psp2::EventFlag) * pConfig->eventFlagCount +
		sizeof(MsgQueue) * pConfig->msgQueueCount + sizeof(ClientImpl);
}

ClientImpl::ClientImpl()
{
}

ClientImpl::~ClientImpl()
{
}

int ClientImpl::finalize_msg_queue_stuff()
{
	SceUInt32 i;

	for (i = 0; i < m_msgQueueCount; i++) {
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].pipe.close());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].counter.close());
	}
	return 0;
}

/* Finalizes the first count message queues, as finalize_msg_queue_stuff does them all. */
inline void ClientImpl::finalize_msg_queues(SceUInt32 count)
{
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].pipe.close());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].counter.close());
	}
}

int ClientImpl::finalize_asyncMI_stuff()
{
	SceUInt32 i;

	for (i = 0; i < m_asyncCallCount; i++) {
		IPMI_ASSERT(m_pAsyncCalls[i].responseCounter.close() == 0);
		IPMI_ASSERT(m_pAsyncCalls[i].responsePipe.close() == 0);
	}
	IPMI_ASSERT_SUCCESS(m_asyncCallMutex.finalize());
	return 0;
}

int ClientImpl::initialize_msg_queue_stuff(const Config *pConfig, SceUID pid, int openLimit)
{
	char name[IPC_NAME_SIZE];
	SceUInt32 i;
	int ret;

	if (pConfig->msgQueueCount > 32)
		return IPMI_ERROR_TOO_MANY_MSG_QUEUES;
	for (i = 0; i < pConfig->msgQueueCount; i++) {
		if (pConfig->msgQueueBufferSize[i] == 0)
			return IPMI_ERROR_INVALID_MSG_QUEUE_SIZE;
	}
	m_msgQueueCount = pConfig->msgQueueCount;
	m_pMsgQueues = reinterpret_cast<MsgQueue *>(reinterpret_cast<char *>(this + 1) +
		sizeof(AsyncCall) * pConfig->asyncCallCount +
		sizeof(psp2::EventFlag) * pConfig->eventFlagCount);
	for (i = 0; i < m_msgQueueCount; i++) {
		new (&m_pMsgQueues[i]) MsgQueue;
		m_pMsgQueues[i].bufferSize = pConfig->msgQueueBufferSize[i];
		ret = m_pMsgQueues[i].mutex.create();
		if (ret != 0) {
			finalize_msg_queues(i);
			return ret;
		}
		get_name_of_client_msg_queue_pipe(name, m_serviceName, pid, m_clientIndex, i);
		ret = m_pMsgQueues[i].pipe.create(name, pConfig->msgQueueBufferSize[i],
						  pConfig->budget, openLimit);
		if (ret != 0) {
			IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
			finalize_msg_queues(i);
			return ret;
		}
		get_name_of_client_msg_queue_counter(name, m_serviceName, pid, m_clientIndex, i);
		ret = m_pMsgQueues[i].counter.create(name, 0xFFFFFF, 0);
		if (ret != 0) {
			IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
			IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].pipe.close());
			finalize_msg_queues(i);
			return ret;
		}
	}
	return 0;
}

int ClientImpl::initialize_eventflag_stuff(const Config *pConfig, SceUID pid)
{
	char name[IPC_NAME_SIZE];
	SceUInt32 i, j;
	int ret;

	m_eventFlagCount = pConfig->eventFlagCount;
	m_pEventFlags = reinterpret_cast<psp2::EventFlag *>(reinterpret_cast<char *>(this + 1) +
		sizeof(AsyncCall) * pConfig->asyncCallCount);
	for (i = 0; i < m_eventFlagCount; i++) {
		new (&m_pEventFlags[i]) psp2::EventFlag;
		get_name_of_client_event_flag(name, m_serviceName, pid, m_clientIndex, i);
		ret = m_pEventFlags[i].create(name);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pEventFlags[j].close() == 0);
			return ret;
		}
	}
	return 0;
}

int ClientImpl::initialize_asyncMI_stuff(const Config *pConfig, SceUID pid, int openLimit)
{
	char name[IPC_NAME_SIZE];
	SceSize pipeSize;
	SceUInt32 i, j;
	int ret;

	ret = m_asyncCallMutex.create();
	if (ret != 0)
		return ret;
	m_pendingAsyncCallCount = 0;
	m_asyncCallCount = pConfig->asyncCallCount;
	m_pAsyncCalls = reinterpret_cast<AsyncCall *>(this + 1);
	m_maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	m_maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	pipeSize = pConfig->maxAsyncCallOutSize + 256;
	for (i = 0; i < m_asyncCallCount; i++)
		new (&m_pAsyncCalls[i]) AsyncCall;
	for (i = 0; i < m_asyncCallCount; i++) {
		get_name_of_client_async_response_msg_pipe(name, m_serviceName, pid,
							   m_clientIndex, i);
		ret = m_pAsyncCalls[i].responsePipe.create(name, pipeSize, pConfig->budget,
							   openLimit);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responsePipe.close() == 0);
			IPMI_ASSERT_SUCCESS(m_asyncCallMutex.finalize());
			return ret;
		}
	}
	for (i = 0; i < m_asyncCallCount; i++) {
		get_name_of_client_async_response_counter_semaphore(name, m_serviceName, pid,
								    m_clientIndex, i);
		ret = m_pAsyncCalls[i].responseCounter.create(name, 0xFFFFFF, 0);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responseCounter.close() == 0);
			for (j = 0; j < m_asyncCallCount; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responsePipe.close() == 0);
			IPMI_ASSERT_SUCCESS(m_asyncCallMutex.finalize());
			return ret;
		}
	}
	return 0;
}

int ClientImpl::initialize_syncMI_stuff(const Config *pConfig, SceUID pid, int openLimit)
{
	char name[IPC_NAME_SIZE];
	int ret;

	ret = m_syncResponseMutex.create();
	if (ret != 0)
		return ret;
	m_pendingSyncCallCount = 0;
	m_syncCallCount = pConfig->syncCallCount;
	m_maxSyncCallInSize = pConfig->maxSyncCallInSize;
	m_maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	get_name_of_client_sync_response_msg_pipe(name, m_serviceName, pid, m_clientIndex);
	ret = m_syncResponsePipe.create(name, pConfig->maxSyncCallOutSize + 256,
					pConfig->budget, openLimit);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_syncResponseMutex.finalize());
		return ret;
	}
	get_name_of_client_sync_response_counter_semaphore(name, m_serviceName, pid,
							   m_clientIndex);
	ret = m_syncResponseCounter.create(name, 0xFFFFFF, 0);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_syncResponsePipe.close());
		IPMI_ASSERT_SUCCESS(m_syncResponseMutex.finalize());
		return ret;
	}
	return 0;
}

int ClientImpl::initialize_request_stuff(const Config *pConfig, SceUID pid, int openLimit)
{
	char name[IPC_NAME_SIZE];
	SceSize asyncSize;
	int ret;

	ret = m_requestMutex.create();
	if (ret != 0)
		return ret;
	get_name_of_client_request_msg_pipe(name, m_serviceName, pid, m_clientIndex);
	asyncSize = pConfig->maxAsyncCallOutSize > pConfig->maxAsyncCallInSize ?
		pConfig->maxAsyncCallOutSize : pConfig->maxAsyncCallInSize;
	ret = m_requestPipe.create(name, pConfig->maxSyncCallInSize + asyncSize + 512,
				   pConfig->budget, openLimit);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_requestMutex.finalize());
		return ret;
	}
	get_name_of_client_request_counter_semaphore(name, m_serviceName, pid, m_clientIndex);
	ret = m_requestCounter.create(name, 0xFFFFFF, 0);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_requestMutex.finalize());
		IPMI_ASSERT_SUCCESS(m_requestPipe.close());
		return ret;
	}
	return 0;
}

inline void ClientImpl::finalize_request_stuff()
{
	IPMI_ASSERT_SUCCESS(m_requestPipe.close());
	IPMI_ASSERT_SUCCESS(m_requestCounter.close());
	IPMI_ASSERT_SUCCESS(m_requestMutex.finalize());
}

inline int ClientImpl::finalize_syncMI_stuff()
{
	int ret;

	ret = m_syncResponsePipe.close();
	if (ret != 0)
		return ret;
	ret = m_syncResponseCounter.close();
	if (ret != 0)
		return ret;
	IPMI_ASSERT_SUCCESS(m_syncResponseMutex.finalize());
	return 0;
}

inline void ClientImpl::finalize_eventflag_stuff()
{
	SceUInt32 i;

	for (i = 0; i < m_eventFlagCount; i++)
		IPMI_ASSERT(m_pEventFlags[i].close() == 0);
}

int ClientImpl::initialize(const Config *pConfig, void *pUserData)
{
	SceUInt32 index;
	SceUInt32 i;
	int32_t map;
	int32_t last;
	SceUID pid;
	int openLimit;
	int ret;

	if (pConfig->serviceName[0] == '\0')
		return IPMI_ERROR_INVALID_NAME;
	if (pConfig->budget == PHYCONT_BUDGET && !sceKernelIsCDialogAvailable())
		return IPMI_ERROR_BUDGET_UNAVAILABLE;
	if (pConfig->budget == KERNEL_BUDGET && sceKernelIsGameBudget())
		return IPMI_ERROR_BUDGET_UNAVAILABLE;

	map = s_clientIndexMap;
	last = s_lastClientIndex;
	i = 1;
	for (;;) {
		index = (i + last) & 31;
		if ((map & (1 << index)) == 0) {
			if (map == sceAtomicCompareAndSwap32AcqRel(&s_clientIndexMap, map,
								  map | (1 << index)))
				break;
			map = s_clientIndexMap;
			i = 1;
			last = s_lastClientIndex;
		}
		if (++i > 32)
			return IPMI_ERROR_TOO_MANY_CLIENTS;
	}
	m_clientIndex = index;
	s_lastClientIndex = index;
	m_state = IPMI_CLIENT_STATE_DISCONNECTED;
	sceClibStrncpy(m_serviceName, pConfig->serviceName, sizeof(m_serviceName));
	m_pUserData = pUserData;
	m_sessionId = 0;
	m_nextRequestId = 0;
	pid = sceKernelGetProcessId();
	m_serverPid = -1;
	openLimit = -1;
	if (SceLibKernel_9F793F84() >= 0x1800000)
		openLimit = pConfig->openLimit;

	ret = initialize_request_stuff(pConfig, pid, openLimit);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
		return ret;
	}
	ret = initialize_syncMI_stuff(pConfig, pid, openLimit);
	if (ret != 0) {
		finalize_request_stuff();
		IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
		return ret;
	}
	ret = initialize_asyncMI_stuff(pConfig, pid, openLimit);
	if (ret != 0) {
		finalize_request_stuff();
		IPMI_ASSERT_SUCCESS(finalize_syncMI_stuff());
		IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
		return ret;
	}
	ret = initialize_eventflag_stuff(pConfig, pid);
	if (ret != 0) {
		finalize_request_stuff();
		IPMI_ASSERT_SUCCESS(finalize_syncMI_stuff());
		IPMI_ASSERT_SUCCESS(finalize_asyncMI_stuff());
		IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
		return ret;
	}
	ret = initialize_msg_queue_stuff(pConfig, pid, openLimit);
	if (ret != 0) {
		finalize_request_stuff();
		IPMI_ASSERT_SUCCESS(finalize_syncMI_stuff());
		IPMI_ASSERT_SUCCESS(finalize_asyncMI_stuff());
		IPMI_ASSERT_SUCCESS(finalize_msg_queue_stuff());
		IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
		return ret;
	}
	return 0;
}

int ClientImpl::destroy()
{
	if (m_sessionId != 0)
		return IPMI_ERROR_BUSY;
	finalize_request_stuff();
	IPMI_ASSERT_SUCCESS(finalize_syncMI_stuff());
	IPMI_ASSERT_SUCCESS(finalize_asyncMI_stuff());
	finalize_eventflag_stuff();
	IPMI_ASSERT_SUCCESS(finalize_msg_queue_stuff());
	IPMI_ASSERT_SUCCESS(release_client_index(m_clientIndex));
	return 0;
}


int ClientImpl::receive_connect_response(int *pResult)
{
	message::ConnectResponse response;
	SceSize size;

	IPMI_ASSERT_SUCCESS(m_requestPipe.tryReceive(&response, &size, sizeof(response)));
	IPMI_ASSERT(size == sizeof(response));
	if (response.error != 0)
		return response.error;
	if (pResult != NULL)
		*pResult = response.result;
	m_sessionId = response.sessionId;
	IPMI_ASSERT(m_sessionId != 0);
	return 0;
}

int ClientImpl::is_server_alive()
{
	SuspendLock lock;
	char name[IPC_NAME_SIZE];

	sceClibSnprintf(name, sizeof(name), "%sServerMP", m_serviceName);
	if (m_serverPid >= 0 && m_serverPid == sceKernelGetMsgPipeCreatorId(name) &&
	    sceAppMgrIsPidShellAndCrashed(m_serverPid) == 0)
		return 1;
	return 0;
}

int ClientImpl::wake_up_server_dispatcher()
{
	psp2::Semaphore counter;
	char name[IPC_NAME_SIZE];
	int ret;

	sceClibSnprintf(name, sizeof(name), "%sServerDC", m_serviceName);
	ret = counter.open(name);
	if (ret == IPMI_ERROR_NOT_FOUND)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);
	IPMI_ASSERT_SUCCESS(counter.signal());
	IPMI_ASSERT_SUCCESS(counter.close());
	return 0;
}

int ClientImpl::send_connect_request(const void *pParam, SceSize paramSize)
{
	SceAppMgrAppStatus status;
	SceSize msgQueueBufferSize[32];
	char name[IPC_NAME_SIZE];
	message::ConnectRequest *pRequest;
	SceSize requestSize;
	SceUInt32 i;
	int ret;

	ret = sceAppMgrGetStatusByName(NULL, &status);
	if (ret != 0)
		return ret;

	SuspendLock lock;

	if (m_sessionId != 0)
		return IPMI_ERROR_BUSY;
	for (i = 0; i < m_msgQueueCount; i++)
		msgQueueBufferSize[i] = m_pMsgQueues[i].bufferSize;
	requestSize = sizeof(message::ConnectRequest) + sizeof(SceSize) * m_msgQueueCount +
		paramSize;

	char requestBuffer[requestSize];

	pRequest = new (requestBuffer) message::ConnectRequest(
		sceKernelGetProcessId(), m_clientIndex, status.appId, m_syncCallCount,
		m_maxSyncCallInSize, m_maxSyncCallOutSize, m_asyncCallCount,
		m_maxAsyncCallInSize, m_maxAsyncCallOutSize, m_eventFlagCount, m_msgQueueCount,
		msgQueueBufferSize, pParam, paramSize);

	psp2::MsgPipe serverPipe;
	psp2::Semaphore serverCounter;

	sceClibSnprintf(name, sizeof(name), "%sServerMP", m_serviceName);
	ret = serverPipe.open(name);
	if (ret != 0) {
		if (ret == IPMI_ERROR_NOT_FOUND)
			return IPMI_ERROR_SERVER_NOT_FOUND;
		return ret;
	}
	m_serverPid = sceKernelGetMsgPipeCreatorId(name);
	sceClibSnprintf(name, sizeof(name), "%sServerMC", m_serviceName);
	ret = serverCounter.open(name);
	if (ret == IPMI_ERROR_NOT_FOUND) {
		IPMI_ASSERT_SUCCESS(serverPipe.close());
		return IPMI_ERROR_SERVER_NOT_FOUND;
	}
	IPMI_ASSERT_SUCCESS(ret);
	IPMI_ASSERT_SUCCESS(serverPipe.send(pRequest, requestSize, NULL));
	IPMI_ASSERT_SUCCESS(serverCounter.signal());
	IPMI_ASSERT_SUCCESS(serverCounter.close());
	IPMI_ASSERT_SUCCESS(serverPipe.close());
	return wake_up_server_dispatcher();
}

/* A client mutex: terminateConnection cancels its waiters, and may unlock it under its holder. */
static inline int lock_mutex(psp2::Mutex &mutex)
{
	int ret = mutex.lock();

	switch (ret) {
	case 0:
		return 0;
	case IPMI_ERROR_CANCELED:
		return IPMI_ERROR_TERMINATED;
	default:
		IPMI_ASSERT_UNEXPECTED(ret);
	}
}

static inline void unlock_mutex(psp2::Mutex &mutex)
{
	int ret = mutex.unlock();

	switch (ret) {
	case 0:
	case IPMI_ERROR_NOT_OWNER:
		break;
	default:
		IPMI_ASSERT_UNEXPECTED(ret);
	}
}

/* Unlocks a client mutex the caller holds when it goes out of scope. */
class MutexUnlocker {
public:
	MutexUnlocker(psp2::Mutex &mutex) : m_mutex(mutex) {}
	~MutexUnlocker() { unlock_mutex(m_mutex); }

private:
	psp2::Mutex &m_mutex;
};

int ClientImpl::connect(const void *pParam, SceSize paramSize, int *pResult)
{
	SceUInt32 timeout;
	int ret;

	if (sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_DISCONNECTED,
					    IPMI_CLIENT_STATE_CONNECTED) ==
	    IPMI_CLIENT_STATE_TERMINATED)
		return IPMI_ERROR_TERMINATED;
	ret = lock_mutex(m_requestMutex);
	if (ret != 0)
		return ret;

	MutexUnlocker requestUnlocker(m_requestMutex);

	ret = send_connect_request(pParam, paramSize);
	if (ret != 0) {
		sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_CONNECTED,
						IPMI_CLIENT_STATE_DISCONNECTED);
		return ret;
	}
	do {
		timeout = 500000;
		ret = m_syncResponseCounter.wait(&timeout);
		switch (ret) {
		case IPMI_ERROR_TIMEOUT:
			if (!is_server_alive())
				ret = IPMI_ERROR_SERVER_NOT_FOUND;
			break;
		case IPMI_ERROR_CANCELED:
			ret = IPMI_ERROR_TERMINATED;
			break;
		}
	} while (ret == IPMI_ERROR_TIMEOUT);
	if (ret != 0) {
		sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_CONNECTED,
						IPMI_CLIENT_STATE_DISCONNECTED);
		return ret;
	}
	ret = receive_connect_response(pResult);
	if (ret != 0) {
		sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_CONNECTED,
						IPMI_CLIENT_STATE_DISCONNECTED);
		return ret;
	}
	return 0;
}


int ClientImpl::send_disconnect_request()
{
	SuspendLock lock;
	message::Header request;
	int ret;

	request.type = IPMI_MESSAGE_TYPE_DISCONNECT_REQUEST;
	request.size = sizeof(request);
	IPMI_ASSERT_SUCCESS(m_requestPipe.send(&request, sizeof(request), NULL));
	IPMI_ASSERT_SUCCESS(m_requestCounter.signal());
	ret = wake_up_server_dispatcher();
	if (ret != 0)
		return ret;
	return 0;
}

int ClientImpl::disconnect()
{
	SceUInt32 timeout;
	int ret;

	ret = lock_mutex(m_syncResponseMutex);
	if (ret != 0)
		return ret;

	MutexUnlocker syncResponseUnlocker(m_syncResponseMutex);

	if (m_asyncCallMutex.lock() == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;

	MutexUnlocker asyncCallUnlocker(m_asyncCallMutex);

	if (m_pendingSyncCallCount != 0 || m_pendingAsyncCallCount != 0)
		return IPMI_ERROR_BUSY;
	if (m_requestMutex.lock() == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;

	MutexUnlocker requestUnlocker(m_requestMutex);

	if (m_sessionId == 0)
		return IPMI_ERROR_NOT_CONNECTED;
	ret = send_disconnect_request();
	if (ret != 0)
		return ret;
	do {
		timeout = 500000;
		ret = m_syncResponseCounter.wait(&timeout);
	} while (ret != 0 && ret != IPMI_ERROR_CANCELED &&
		 (ret != IPMI_ERROR_TIMEOUT || is_server_alive()));
	m_sessionId = 0;
	m_serverPid = -1;
	sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_CONNECTED,
					IPMI_CLIENT_STATE_DISCONNECTED);
	return 0;
}


int ClientImpl::terminateConnection()
{
	SceUInt32 i;

	if (sceAtomicCompareAndSwap32AcqRel(&m_state, IPMI_CLIENT_STATE_CONNECTED,
					    IPMI_CLIENT_STATE_TERMINATED) ==
	    IPMI_CLIENT_STATE_DISCONNECTED)
		return IPMI_ERROR_NOT_CONNECTED;
	m_sessionId = 0;
	m_serverPid = -1;
	IPMI_ASSERT_SUCCESS(m_syncResponseMutex.destroy());
	IPMI_ASSERT_SUCCESS(m_asyncCallMutex.destroy());
	IPMI_ASSERT_SUCCESS(m_requestMutex.destroy());
	IPMI_ASSERT_SUCCESS(m_syncResponseCounter.destroy());
	for (i = 0; i < m_eventFlagCount; i++)
		IPMI_ASSERT_SUCCESS(m_pEventFlags[i].destroy());
	for (i = 0; i < m_msgQueueCount; i++) {
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.destroy());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].counter.destroy());
	}
	return 0;
}

int ClientImpl::receive_sync_response(int *pResult, void *pOut, SceSize *pOutSize,
				      SceSize outSize)
{
	message::SyncCallResponse response;
	SceSize size;

	IPMI_ASSERT_SUCCESS(m_syncResponsePipe.tryReceive(&response, &size, sizeof(response)));
	IPMI_ASSERT(size == sizeof(response));
	if (response.error != 0) {
		IPMI_ASSERT(response.size == sizeof(response));
		return response.error;
	}
	*pResult = response.result;
	if (response.outSize != 0) {
		IPMI_ASSERT(pOut != NULL);
		IPMI_ASSERT(response.outSize <= outSize);
		IPMI_ASSERT_SUCCESS(m_syncResponsePipe.tryReceive(pOut, pOutSize,
								  response.outSize));
		IPMI_ASSERT(*pOutSize == response.outSize);
	}
	return 0;
}

int ClientImpl::send_sync_call_request(SceUInt32 methodId, const void *pIn, SceSize inSize,
				       SceSize outSize)
{
	SuspendLock lock;
	message::SyncCallRequest request;

	request.type = IPMI_MESSAGE_TYPE_SYNC_CALL_REQUEST;
	request.size = inSize + sizeof(request);
	request.sessionId = m_sessionId;
	request.methodId = methodId;
	request.inSize = inSize;
	request.outSize = outSize;
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(&request, sizeof(request)));
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pIn, inSize));
	IPMI_ASSERT_SUCCESS(m_requestCounter.signal());
	return wake_up_server_dispatcher();
}

int ClientImpl::invokeSyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				 int *pResult, void *pOut, SceSize *pOutSize, SceSize outSize)
{
	SceUInt32 timeout;
	int ret;

	if ((inSize != 0 && pIn == NULL) || pResult == NULL ||
	    (outSize != 0 && (pOut == NULL || pOutSize == NULL)))
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (inSize > m_maxSyncCallInSize)
		return IPMI_ERROR_INPUT_TOO_LARGE;
	if (outSize > m_maxSyncCallOutSize)
		return IPMI_ERROR_OUTPUT_TOO_LARGE;
	ret = lock_mutex(m_syncResponseMutex);
	if (ret != 0)
		return ret;

	MutexUnlocker syncResponseUnlocker(m_syncResponseMutex);

	{
		ret = m_requestMutex.lock();
		if (ret == IPMI_ERROR_CANCELED)
			return IPMI_ERROR_TERMINATED;
		IPMI_ASSERT_SUCCESS(ret);

		MutexUnlocker requestUnlocker(m_requestMutex);

		if (m_sessionId == 0)
			return IPMI_ERROR_NOT_CONNECTED;
		m_pendingSyncCallCount++;
		ret = send_sync_call_request(methodId, pIn, inSize, outSize);
		if (ret != 0) {
			m_pendingSyncCallCount--;
			return ret;
		}
	}
	for (;;) {
		timeout = 500000;
		ret = m_syncResponseCounter.wait(&timeout);
		if (ret == 0)
			break;
		if (ret == IPMI_ERROR_CANCELED ||
		    (ret == IPMI_ERROR_TIMEOUT && !is_server_alive())) {
			m_pendingSyncCallCount--;
			return IPMI_ERROR_TERMINATED;
		}
	}
	ret = receive_sync_response(pResult, pOut, pOutSize, outSize);
	m_pendingSyncCallCount--;
	return ret;
}

int ClientImpl::send_sync_multi_call_request(SceUInt32 methodId, const DataInfo *pInList,
					     SceUInt32 inCount, const SceSize *pInSizes,
					     SceSize inTotal, const BufferInfo *pOutList,
					     SceUInt32 outCount, const SceSize *pOutSizes)
{
	static const char padding[16] = { 0 };
	SuspendLock lock;
	message::SyncMultiCallRequest request;
	SceSize sizesSize, sizesPadded, paddingSize;
	SceUInt32 i;

	sizesSize = sizeof(SceSize) * (inCount + outCount);
	sizesPadded = (sizesSize + 15) & ~15;
	request.type = IPMI_MESSAGE_TYPE_SYNC_MULTI_CALL_REQUEST;
	request.size = sizesPadded + inTotal + sizeof(request);
	request.sessionId = m_sessionId;
	request.methodId = methodId;
	request.inCount = inCount;
	request.outCount = outCount;
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(&request, sizeof(request)));
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pInSizes, sizeof(SceSize) * inCount));
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pOutSizes, sizeof(SceSize) * outCount));
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(padding, sizesPadded - sizesSize));
	for (i = 0; i < inCount; i++) {
		IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pInList[i].pData, pInList[i].dataSize));
		paddingSize = ((pInList[i].dataSize + 15) & ~15) - pInList[i].dataSize;
		if (paddingSize != 0)
			IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(padding, paddingSize));
	}
	IPMI_ASSERT_SUCCESS(m_requestCounter.signal());
	return wake_up_server_dispatcher();
}

int ClientImpl::invokeSyncMethod(SceUInt32 methodId, const DataInfo *pInList, SceUInt32 inCount,
				 int *pResult, BufferInfo *pOutList, SceUInt32 outCount)
{
	message::SyncMultiCallResponse response;
	SceSize size, inTotal, outTotal, outSizesSize;
	SceUInt32 timeout;
	SceUInt32 i;
	int ret;

	if ((inCount != 0 && pInList == NULL) || pResult == NULL ||
	    (outCount != 0 && pOutList == NULL))
		return IPMI_ERROR_INVALID_ARGUMENT;

	SceSize inSizes[inCount];
	SceSize receivedSizes[outCount];
	SceSize outSizes[outCount];

	outSizesSize = sizeof(SceSize) * outCount;
	inTotal = 0;
	for (i = 0; i < inCount; i++) {
		inSizes[i] = pInList[i].dataSize;
		inTotal += (pInList[i].dataSize + 15) & ~15;
	}
	if (inTotal + ((sizeof(SceSize) * (inCount + outCount) + 15) & ~15) > m_maxSyncCallInSize)
		return IPMI_ERROR_INPUT_TOO_LARGE;
	outTotal = 0;
	for (i = 0; i < outCount; i++) {
		outSizes[i] = pOutList[i].bufferSize;
		outTotal += (pOutList[i].bufferSize + 15) & ~15;
	}
	if (outTotal + ((outSizesSize + 15) & ~15) > m_maxSyncCallOutSize)
		return IPMI_ERROR_OUTPUT_TOO_LARGE;

	ret = m_syncResponseMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);

	MutexUnlocker syncResponseUnlocker(m_syncResponseMutex);

	{
		ret = m_requestMutex.lock();
		if (ret == IPMI_ERROR_CANCELED)
			return IPMI_ERROR_TERMINATED;
		IPMI_ASSERT_SUCCESS(ret);

		MutexUnlocker requestUnlocker(m_requestMutex);

		if (m_sessionId == 0)
			return IPMI_ERROR_NOT_CONNECTED;
		m_pendingSyncCallCount++;
		ret = send_sync_multi_call_request(methodId, pInList, inCount, inSizes, inTotal,
						   pOutList, outCount, outSizes);
		if (ret != 0) {
			m_pendingSyncCallCount--;
			return ret;
		}
	}
	for (;;) {
		timeout = 500000;
		ret = m_syncResponseCounter.wait(&timeout);
		if (ret == 0)
			break;
		if (ret == IPMI_ERROR_CANCELED ||
		    (ret == IPMI_ERROR_TIMEOUT && !is_server_alive())) {
			m_pendingSyncCallCount--;
			return IPMI_ERROR_TERMINATED;
		}
	}
	IPMI_ASSERT_SUCCESS(m_syncResponsePipe.tryReceive(&response, &size, sizeof(response)));
	if (size != sizeof(response))
		sceClibPrintf("[SceIpmi] Detected bug#63938\n");
	IPMI_ASSERT(size == sizeof(response));
	if (response.error != 0) {
		IPMI_ASSERT(response.error != IPMI_ERROR_INTERNAL);
		IPMI_ASSERT(response.size == sizeof(response));
		m_pendingSyncCallCount--;
		return response.error;
	}
	*pResult = response.result;
	if (response.outCount != 0) {
		IPMI_ASSERT(response.outCount == outCount);
		IPMI_ASSERT_SUCCESS(m_syncResponsePipe.tryReceive(receivedSizes, &size,
								  outSizesSize));
		IPMI_ASSERT(size == outSizesSize);
		for (i = 0; i < outCount; i++) {
			IPMI_ASSERT(pOutList[i].bufferSize >= receivedSizes[i]);
			IPMI_ASSERT_SUCCESS(m_syncResponsePipe.tryReceive(pOutList[i].pBuffer, &size,
									  receivedSizes[i]));
			IPMI_ASSERT(size == receivedSizes[i]);
			pOutList[i].outSize = size;
		}
	}
	m_pendingSyncCallCount--;
	return 0;
}

/* A free asynchronous call slot; one is free while fewer calls than slots are pending. */
inline ClientImpl::AsyncCall *ClientImpl::find_free_async_call(SceUInt32 *pIndex)
{
	AsyncCall *pCall, *pFree;
	SceUInt32 index, i;

	pFree = NULL;
	index = 0;
	for (i = 0, pCall = m_pAsyncCalls; i < m_asyncCallCount; i++, pCall++) {
		if (pCall->requestId == -1) {
			pFree = pCall;
			index = i;
			break;
		}
	}
	IPMI_ASSERT(pFree != NULL);
	*pIndex = index;
	return pFree;
}

/* The next request ID no pending call uses (never -1, which marks a free slot). */
inline int ClientImpl::new_request_id()
{
	AsyncCall *pCall;
	SceUInt32 i;
	int requestId;

	do {
		requestId = m_nextRequestId++;
		if (m_nextRequestId == (SceUInt32)-1)
			m_nextRequestId++;
		for (i = 0, pCall = m_pAsyncCalls; i < m_asyncCallCount; i++, pCall++) {
			if (pCall->requestId == requestId)
				requestId = -1;
		}
	} while (requestId == -1);
	return requestId;
}

int ClientImpl::invokeAsyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				  int *pRequestId, const EventFlagInfo *pEventFlag)
{
	SuspendLock lock;
	message::AsyncCallRequest request;
	AsyncCall *pCall;
	SceUInt32 index;
	int requestId, ret;

	if (pRequestId == NULL || (inSize != 0 && pIn == NULL))
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (inSize > m_maxAsyncCallInSize)
		return IPMI_ERROR_INPUT_TOO_LARGE;
	if (pEventFlag != NULL && pEventFlag->index >= m_eventFlagCount)
		return IPMI_ERROR_INVALID_EVENT_FLAG;
	ret = m_asyncCallMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);

	MutexUnlocker asyncCallUnlocker(m_asyncCallMutex);

	ret = m_requestMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);

	MutexUnlocker requestUnlocker(m_requestMutex);

	if (m_sessionId == 0)
		return IPMI_ERROR_NOT_CONNECTED;
	if (m_pendingAsyncCallCount == m_asyncCallCount)
		return IPMI_ERROR_TOO_MANY_ASYNC_CALLS;
	m_pendingAsyncCallCount++;
	pCall = find_free_async_call(&index);
	requestId = new_request_id();
	*pRequestId = requestId;
	pCall->requestId = requestId;
	request.type = IPMI_MESSAGE_TYPE_ASYNC_CALL_REQUEST;
	request.size = inSize + sizeof(request);
	request.sessionId = m_sessionId;
	request.methodId = methodId;
	request.requestId = requestId;
	request.inSize = inSize;
	request.eventFlagIndex = pEventFlag != NULL ? pEventFlag->index : -1;
	request.eventFlagBitPattern = pEventFlag != NULL ? pEventFlag->bitPattern : 0;
	request.asyncCallIndex = index;
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(&request, sizeof(request)));
	if (inSize != 0)
		IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pIn, inSize));
	IPMI_ASSERT_SUCCESS(m_requestCounter.signal());
	ret = wake_up_server_dispatcher();
	if (ret != 0)
		return ret;
	return 0;
}

int ClientImpl::invokeAsyncMethod(SceUInt32 methodId, const DataInfo *pInList, SceUInt32 inCount,
				  int *pRequestId, const EventFlagInfo *pEventFlag)
{
	static const char padding[16] = { 0 };
	SuspendLock lock;
	message::AsyncMultiCallRequest request;
	AsyncCall *pCall;
	SceSize sizesSize, sizesPadded, inTotal, paddingSize;
	SceUInt32 index, i;
	int requestId, ret;

	if (pRequestId == NULL || (inCount != 0 && pInList == NULL))
		return IPMI_ERROR_INVALID_ARGUMENT;

	SceSize inSizes[inCount];

	sizesSize = sizeof(SceSize) * inCount;
	sizesPadded = (sizesSize + 15) & ~15;
	inTotal = 0;
	for (i = 0; i < inCount; i++) {
		inSizes[i] = pInList[i].dataSize;
		inTotal += (pInList[i].dataSize + 15) & ~15;
	}
	inTotal += sizesPadded;
	if (inTotal > m_maxAsyncCallInSize)
		return IPMI_ERROR_INPUT_TOO_LARGE;
	if (pEventFlag != NULL && pEventFlag->index >= m_eventFlagCount)
		return IPMI_ERROR_INVALID_EVENT_FLAG;
	ret = m_asyncCallMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);

	MutexUnlocker asyncCallUnlocker(m_asyncCallMutex);

	ret = m_requestMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);

	MutexUnlocker requestUnlocker(m_requestMutex);

	if (m_sessionId == 0)
		return IPMI_ERROR_NOT_CONNECTED;
	if (m_pendingAsyncCallCount == m_asyncCallCount)
		return IPMI_ERROR_TOO_MANY_ASYNC_CALLS;
	m_pendingAsyncCallCount++;
	pCall = find_free_async_call(&index);
	requestId = new_request_id();
	*pRequestId = requestId;
	pCall->requestId = requestId;
	pCall->state |= IPMI_ASYNC_CALL_STATE_MULTI;
	request.type = IPMI_MESSAGE_TYPE_ASYNC_MULTI_CALL_REQUEST;
	request.size = inTotal + sizeof(request);
	request.sessionId = m_sessionId;
	request.methodId = methodId;
	request.requestId = requestId;
	request.inCount = inCount;
	request.eventFlagIndex = pEventFlag != NULL ? pEventFlag->index : -1;
	request.eventFlagBitPattern = pEventFlag != NULL ? pEventFlag->bitPattern : 0;
	request.asyncCallIndex = index;
	IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(&request, sizeof(request)));
	if (inCount != 0) {
		IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(inSizes, sizesSize));
		if (sizesPadded != sizesSize)
			IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(padding,
								  sizesPadded - sizesSize));
		for (i = 0; i < inCount; i++) {
			IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(pInList[i].pData,
								  pInList[i].dataSize));
			paddingSize = ((pInList[i].dataSize + 15) & ~15) - pInList[i].dataSize;
			if (paddingSize != 0)
				IPMI_ASSERT_SUCCESS(m_requestPipe.trySend(padding, paddingSize));
		}
	}
	IPMI_ASSERT_SUCCESS(m_requestCounter.signal());
	ret = wake_up_server_dispatcher();
	if (ret != 0)
		return ret;
	return 0;
}

/* The pending call with this request ID. */
inline ClientImpl::AsyncCall *ClientImpl::find_async_call(int requestId)
{
	AsyncCall *pCall;
	SceUInt32 i;

	for (i = 0, pCall = m_pAsyncCalls; i < m_asyncCallCount; i++, pCall++) {
		if (pCall->requestId == requestId)
			return pCall;
	}
	return NULL;
}

/* Frees a call slot once its result has been taken. */
inline void ClientImpl::release_async_call(AsyncCall *pCall)
{
	pCall->requestId = -1;
	pCall->state = 0;
	pCall->outSize = 0;
	pCall->result = 0xDEADBEEF;
	pCall->methodId = -1;
	pCall->outCount = 0;
	m_pendingAsyncCallCount--;
}

int ClientImpl::tryGetResult(int requestId, int *pResult, void *pOut, SceSize *pOutSize,
			     SceSize outSize)
{
	message::AsyncCallResponse response;
	AsyncCall *pCall;
	SceSize size;
	int ret;

	if (pResult == NULL || (outSize != 0 && (pOut == NULL || pOutSize == NULL)))
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (outSize > m_maxAsyncCallOutSize)
		return IPMI_ERROR_OUTPUT_TOO_LARGE;
	if (requestId == -1)
		return IPMI_ERROR_INVALID_REQUEST_ID;
	ret = m_asyncCallMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);
	if (m_sessionId == 0) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_NOT_CONNECTED;
	}
	pCall = find_async_call(requestId);
	if (pCall == NULL) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_INVALID_REQUEST_ID;
	}
	if (pCall->state & IPMI_ASYNC_CALL_STATE_MULTI) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_INVALID_CALL_TYPE;
	}
	if (!(pCall->state & IPMI_ASYNC_CALL_STATE_RECEIVED)) {
		ret = pCall->responseCounter.tryWait();
		if (ret == IPMI_ERROR_WOULD_BLOCK) {
			unlock_mutex(m_asyncCallMutex);
			return is_server_alive() ? IPMI_ERROR_WOULD_BLOCK : IPMI_ERROR_TERMINATED;
		}
		IPMI_ASSERT_SUCCESS(ret);
		IPMI_ASSERT_SUCCESS(pCall->responsePipe.tryReceive(&response, &size,
								    sizeof(response)));
		IPMI_ASSERT(size == sizeof(response));
		IPMI_ASSERT(response.requestId == requestId);
		pCall->result = response.result;
		pCall->state |= IPMI_ASYNC_CALL_STATE_RECEIVED;
		pCall->outSize = response.outSize;
	}
	if (pCall->outSize != 0) {
		if (pCall->outSize > outSize) {
			unlock_mutex(m_asyncCallMutex);
			return IPMI_ERROR_OUTPUT_BUFFER_TOO_SMALL;
		}
		IPMI_ASSERT_SUCCESS(pCall->responsePipe.tryReceive(pOut, &size, pCall->outSize));
		IPMI_ASSERT(size == pCall->outSize);
		*pOutSize = pCall->outSize;
	}
	*pResult = pCall->result;
	release_async_call(pCall);
	unlock_mutex(m_asyncCallMutex);
	return 0;
}

int ClientImpl::tryGetResult(SceUInt32 methodId, int requestId, int *pResult,
			     BufferInfo *pOutList, SceUInt32 outCount)
{
	message::AsyncMultiCallResponse response;
	AsyncCall *pCall;
	SceSize size;
	SceUInt32 i;
	int ret;

	if (pResult == NULL || (outCount != 0 && pOutList == NULL))
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (requestId == -1)
		return IPMI_ERROR_INVALID_REQUEST_ID;
	ret = m_asyncCallMutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);
	if (m_sessionId == 0) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_NOT_CONNECTED;
	}
	pCall = find_async_call(requestId);
	if (pCall == NULL) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_INVALID_REQUEST_ID;
	}
	if (!(pCall->state & IPMI_ASYNC_CALL_STATE_MULTI)) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_INVALID_CALL_TYPE;
	}
	if (!(pCall->state & IPMI_ASYNC_CALL_STATE_RECEIVED)) {
		ret = pCall->responseCounter.tryWait();
		if (ret == IPMI_ERROR_WOULD_BLOCK) {
			unlock_mutex(m_asyncCallMutex);
			return is_server_alive() ? IPMI_ERROR_WOULD_BLOCK : IPMI_ERROR_TERMINATED;
		}
		IPMI_ASSERT_SUCCESS(ret);
		IPMI_ASSERT_SUCCESS(pCall->responsePipe.tryReceive(&response, &size,
								    sizeof(response)));
		IPMI_ASSERT(size == sizeof(response));
		IPMI_ASSERT(response.requestId == requestId);
		pCall->methodId = response.methodId;
		pCall->outCount = response.outCount;
		pCall->result = response.result;
		pCall->state |= IPMI_ASYNC_CALL_STATE_RECEIVED;
	}
	if (pCall->methodId != methodId) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_METHOD_MISMATCH;
	}
	if (pCall->state & IPMI_ASYNC_CALL_STATE_LOST) {
		unlock_mutex(m_asyncCallMutex);
		return IPMI_ERROR_INTERNAL;
	}
	if (pCall->outCount != 0) {
		if (pCall->outCount != outCount) {
			unlock_mutex(m_asyncCallMutex);
			return IPMI_ERROR_INVALID_OUT_COUNT;
		}

		SceSize outSizes[outCount];

		IPMI_ASSERT_SUCCESS(pCall->responsePipe.tryReceive(outSizes, &size,
								    sizeof(SceSize) * outCount));
		for (i = 0; i < outCount; i++) {
			if (pOutList[i].bufferSize < outSizes[i]) {
				pCall->state |= IPMI_ASYNC_CALL_STATE_LOST;
				unlock_mutex(m_asyncCallMutex);
				return IPMI_ERROR_OUTPUT_BUFFER_TOO_SMALL;
			}
		}
		for (i = 0; i < outCount; i++)
			IPMI_ASSERT_SUCCESS(pCall->responsePipe.tryReceive(pOutList[i].pBuffer,
									    &pOutList[i].outSize,
									    outSizes[i]));
	}
	*pResult = pCall->result;
	release_async_call(pCall);
	unlock_mutex(m_asyncCallMutex);
	return 0;
}

int ClientImpl::pollEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
			      SceUInt32 *pResultPattern)
{
	int ret;

	if (index >= m_eventFlagCount)
		return IPMI_ERROR_INVALID_EVENT_FLAG;
	ret = m_pEventFlags[index].poll(bitPattern, waitMode, pResultPattern);
	if (ret != 0)
		return ret;
	if (!is_server_alive())
		return IPMI_ERROR_TERMINATED;
	return 0;
}

int ClientImpl::waitEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
			      SceUInt32 *pResultPattern, SceUInt32 *pTimeout)
{
	SceUInt32 timeout;
	int ret;

	if (index >= m_eventFlagCount)
		return IPMI_ERROR_INVALID_EVENT_FLAG;
	for (;;) {
		timeout = 500000;
		ret = m_pEventFlags[index].wait(bitPattern, waitMode, pResultPattern,
						pTimeout != NULL ? pTimeout : &timeout);
		if (ret == 0)
			return 0;
		if (ret != IPMI_ERROR_TIMEOUT)
			return ret;
		if (!is_server_alive())
			return IPMI_ERROR_TERMINATED;
		if (pTimeout != NULL)
			return ret;
	}
}

void *ClientImpl::getUserData()
{
	return m_pUserData;
}

int ClientImpl::getMsg(SceUInt32 index, void *pBuffer, SceSize *pSize, SceSize bufferSize,
		       SceUInt32 *pTimeout)
{
	MsgQueue *pQueue;
	SceUInt32 timeout;
	SceSize size;
	int ret;

	if (pBuffer == NULL || pSize == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (index >= m_msgQueueCount)
		return IPMI_ERROR_INVALID_MSG_QUEUE;
	pQueue = &m_pMsgQueues[index];
	ret = pQueue->mutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);
	for (;;) {
		while (pQueue->header.size == 0) {
			timeout = 500000;
			ret = pQueue->counter.wait(pTimeout != NULL ? pTimeout : &timeout);
			if (ret == 0) {
				ret = pQueue->pipe.tryReceive(&pQueue->header, &size,
							      sizeof(pQueue->header));
				if (ret != IPMI_ERROR_WOULD_BLOCK)
					IPMI_ASSERT(size == sizeof(pQueue->header));
				continue;
			}
			if (ret == IPMI_ERROR_TIMEOUT) {
				if (!is_server_alive())
					ret = IPMI_ERROR_TERMINATED;
				else if (pTimeout == NULL)
					continue;
			}
			if (ret == IPMI_ERROR_CANCELED) {
				if (m_sessionId == 0) {
					unlock_mutex(pQueue->mutex);
					return IPMI_ERROR_TERMINATED;
				}
				continue;
			}
			unlock_mutex(pQueue->mutex);
			return ret;
		}
		if (pQueue->header.size > bufferSize) {
			unlock_mutex(pQueue->mutex);
			return IPMI_ERROR_OUTPUT_BUFFER_TOO_SMALL;
		}
		ret = pQueue->pipe.tryReceive(pBuffer, &size, pQueue->header.size);
		if (ret == 0)
			break;
		IPMI_ASSERT(ret == IPMI_ERROR_WOULD_BLOCK);
		pQueue->header.size = 0;
	}
	*pSize = size;
	pQueue->header.size = 0;
	unlock_mutex(pQueue->mutex);
	return 0;
}

int ClientImpl::tryGetMsg(SceUInt32 index, void *pBuffer, SceSize *pSize, SceSize bufferSize)
{
	MsgQueue *pQueue;
	SceSize size;
	int ret;

	if (pBuffer == NULL || pSize == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (index >= m_msgQueueCount)
		return IPMI_ERROR_INVALID_MSG_QUEUE;
	pQueue = &m_pMsgQueues[index];
	ret = pQueue->mutex.lock();
	if (ret == IPMI_ERROR_CANCELED)
		return IPMI_ERROR_TERMINATED;
	IPMI_ASSERT_SUCCESS(ret);
	if (pQueue->header.size == 0) {
		ret = pQueue->counter.tryWait();
		if (ret == 0) {
			IPMI_ASSERT_SUCCESS(pQueue->pipe.tryReceive(&pQueue->header, &size,
								     sizeof(pQueue->header)));
			IPMI_ASSERT(size == sizeof(pQueue->header));
		} else {
			unlock_mutex(pQueue->mutex);
			IPMI_ASSERT(ret == IPMI_ERROR_WOULD_BLOCK);
			if (!is_server_alive())
				return IPMI_ERROR_TERMINATED;
			return ret;
		}
	}
	if (pQueue->header.size > bufferSize) {
		unlock_mutex(pQueue->mutex);
		return IPMI_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}
	IPMI_ASSERT_SUCCESS(pQueue->pipe.tryReceive(pBuffer, &size, pQueue->header.size));
	*pSize = size;
	pQueue->header.size = 0;
	unlock_mutex(pQueue->mutex);
	return 0;
}

}
}
