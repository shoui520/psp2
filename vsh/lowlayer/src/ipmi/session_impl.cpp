#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#define _Static_assert(expression, message)
#include <processmgr/processmgr.h>
#undef _Static_assert
#include "session_impl.h"
#include "server_impl.h"
#include "assert.h"
#include "ipc_name.h"
#include "ipmi_error.h"
#include "message.h"

void *operator new(size_t size, void *where);

namespace IPMI {
namespace impl {

SceSize SessionImpl::getBufferSize(const Config *pConfig)
{
	return sizeof(AsyncCall) * pConfig->asyncCallCount +
		sizeof(psp2::EventFlag) * pConfig->eventFlagCount +
		sizeof(MsgQueue) * pConfig->msgQueueCount + sizeof(SessionImpl);
}

SessionImpl::SessionImpl()
	: m_syncCallPending(false), m_syncCallOutSize(0), m_syncCallIsMulti(false),
	  m_syncCallOutCount(0), m_pSyncCallOutSizes(NULL)
{
}

SessionImpl::~SessionImpl()
{
}

int SessionImpl::finalize_msg_queue_stuff()
{
	SceUInt32 i;

	for (i = 0; i < m_msgQueueCount; i++) {
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].counter.close());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].pipe.close());
	}
	return 0;
}

/* Finalizes the first count message queues, as finalize_msg_queue_stuff does them all. */
inline void SessionImpl::finalize_msg_queues(SceUInt32 count)
{
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].counter.close());
		IPMI_ASSERT_SUCCESS(m_pMsgQueues[i].pipe.close());
	}
}

int SessionImpl::initialize_msg_queue_stuff(const ClientConfig *pConfig,
					    const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	SceUInt32 i;
	int ret;

	m_msgQueueCount = pConfig->msgQueueCount;
	if (m_msgQueueCount == 0) {
		m_pMsgQueues = NULL;
		return 0;
	}
	m_pMsgQueues = reinterpret_cast<MsgQueue *>(
		reinterpret_cast<psp2::EventFlag *>(
			reinterpret_cast<AsyncCall *>(this + 1) + m_asyncCallCount) +
		m_eventFlagCount);
	for (i = 0; i < m_msgQueueCount; i++)
		new (&m_pMsgQueues[i]) MsgQueue;
	for (i = 0; i < m_msgQueueCount; i++) {
		MsgQueue *pQueue = &m_pMsgQueues[i];

		pQueue->bufferSize = pConfig->msgQueueBufferSize[i];
		ret = pQueue->mutex.create();
		if (ret != 0) {
			finalize_msg_queues(i);
			return ret;
		}
		get_name_of_client_msg_queue_pipe(name, pServiceName, m_processId,
						  pConfig->clientIndex, i);
		ret = pQueue->pipe.open(name);
		if (ret != 0) {
			IPMI_ASSERT_SUCCESS(pQueue->mutex.finalize());
			finalize_msg_queues(i);
			return ret;
		}
		get_name_of_client_msg_queue_counter(name, pServiceName, m_processId,
						     pConfig->clientIndex, i);
		ret = pQueue->counter.open(name);
		if (ret != 0) {
			IPMI_ASSERT_SUCCESS(pQueue->mutex.finalize());
			IPMI_ASSERT_SUCCESS(pQueue->pipe.close());
			finalize_msg_queues(i);
			return ret;
		}
	}
	return 0;
}

int SessionImpl::initialize_event_flag(const ClientConfig *pConfig, const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	SceUInt32 i, j;
	int ret;

	if (m_eventFlagCount == 0) {
		m_pEventFlags = NULL;
		return 0;
	}
	m_pEventFlags = reinterpret_cast<psp2::EventFlag *>(
		reinterpret_cast<AsyncCall *>(this + 1) + m_asyncCallCount);
	for (i = 0; i < m_eventFlagCount; i++)
		new (&m_pEventFlags[i]) psp2::EventFlag;
	for (i = 0; i < m_eventFlagCount; i++) {
		psp2::EventFlag *pEventFlag = &m_pEventFlags[i];

		get_name_of_client_event_flag(name, pServiceName, m_processId,
					      pConfig->clientIndex, i);
		ret = pEventFlag->open(name);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pEventFlags[j].close() == 0);
			return ret;
		}
	}
	return 0;
}

int SessionImpl::initialize_async_call_stuff(const ClientConfig *pConfig,
					     const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	SceUInt32 i, j;
	int ret;

	if (m_asyncCallCount == 0) {
		m_pAsyncCalls = NULL;
		return 0;
	}
	m_pAsyncCalls = reinterpret_cast<AsyncCall *>(this + 1);
	for (i = 0; i < pConfig->asyncCallCount; i++)
		new (&m_pAsyncCalls[i]) AsyncCall;
	for (i = 0; i < pConfig->asyncCallCount; i++) {
		AsyncCall *pCall = &m_pAsyncCalls[i];

		get_name_of_client_async_response_msg_pipe(name, pServiceName, m_processId,
							   pConfig->clientIndex, i);
		ret = pCall->responsePipe.open(name);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responsePipe.close() == 0);
			return ret;
		}
	}
	for (i = 0; i < pConfig->asyncCallCount; i++) {
		AsyncCall *pCall = &m_pAsyncCalls[i];

		get_name_of_client_async_response_counter_semaphore(name, pServiceName,
								    m_processId,
								    pConfig->clientIndex, i);
		ret = pCall->responseCounter.open(name);
		if (ret != 0) {
			for (j = 0; j < i; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responseCounter.close() == 0);
			for (j = 0; j < pConfig->asyncCallCount; j++)
				IPMI_ASSERT(m_pAsyncCalls[j].responsePipe.close() == 0);
			return ret;
		}
	}
	return 0;
}

int SessionImpl::initialize_sync_call_stuff(const ClientConfig *pConfig,
					    const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	int ret;

	if (pConfig->syncCallCount == 0)
		return 0;
	get_name_of_client_sync_response_msg_pipe(name, pServiceName, m_processId,
						  pConfig->clientIndex);
	ret = m_syncResponsePipe.open(name);
	if (ret != 0)
		return ret;
	get_name_of_client_sync_response_counter_semaphore(name, pServiceName, m_processId,
							   pConfig->clientIndex);
	ret = m_syncResponseCounter.open(name);
	if (ret != 0) {
		IPMI_ASSERT(m_syncResponsePipe.close() == 0);
		return ret;
	}
	return 0;
}

int SessionImpl::initialize_request_stuff(const ClientConfig *pConfig,
					  const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	int ret;

	get_name_of_client_request_msg_pipe(name, pServiceName, m_processId,
					    pConfig->clientIndex);
	ret = m_requestPipe.open(name);
	if (ret != 0)
		return ret;
	get_name_of_client_request_counter_semaphore(name, pServiceName, m_processId,
						     pConfig->clientIndex);
	ret = m_requestCounter.open(name);
	if (ret != 0) {
		IPMI_ASSERT(m_requestPipe.close() == 0);
		return ret;
	}
	return 0;
}

inline void SessionImpl::finalize_request_stuff()
{
	IPMI_ASSERT(m_requestPipe.close() == 0);
	IPMI_ASSERT(m_requestCounter.close() == 0);
}

inline void SessionImpl::finalize_sync_call_stuff()
{
	IPMI_ASSERT_SUCCESS(m_syncResponsePipe.close());
	IPMI_ASSERT_SUCCESS(m_syncResponseCounter.close());
}

inline void SessionImpl::finalize_async_call_stuff()
{
	SceUInt32 i;

	for (i = 0; i < m_asyncCallCount; i++) {
		AsyncCall *pCall = &m_pAsyncCalls[i];

		IPMI_ASSERT_SUCCESS(pCall->responseCounter.close());
		IPMI_ASSERT_SUCCESS(pCall->responsePipe.close());
	}
}

inline void SessionImpl::finalize_event_flag()
{
	SceUInt32 i;

	for (i = 0; i < m_eventFlagCount; i++)
		IPMI_ASSERT(m_pEventFlags[i].close() == 0);
}

/* The client's process has exited: let the server's dispatcher drop the session. */
int SessionImpl::process_termination_callback(int notifyId, int notifyCount, int notifyArg,
					      void *pCommon)
{
	SessionImpl *pSession;

	pSession = static_cast<SessionImpl *>(pCommon);
	pSession->m_state |= IPMI_SESSION_STATE_PROCESS_EXITED;
	pSession->m_pServer->notify_process_exit();
	return 0;
}

inline int SessionImpl::initialize_process_term_callback(const char *pServiceName)
{
	char name[IPC_NAME_SIZE];
	SceUID callbackId;
	int ret;

	sceClibSnprintf(name, sizeof(name), "%sIpmi%08x", pServiceName, m_processId);
	callbackId = sceKernelCreateCallback(name, 0, process_termination_callback, this);
	if (callbackId < 0)
		return callbackId;
	ret = sceKernelRegisterProcessTerminationCallback(m_processId, callbackId);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(sceKernelDeleteCallback(callbackId));
		if (ret == SCE_KERNEL_ERROR_INVALID_PID)
			return IPMI_ERROR_NOT_FOUND;
		return ret;
	}
	m_processTerminationCallbackId = callbackId;
	return 0;
}

/* A client that went away while connecting is no client. */
static inline int client_error(int ret)
{
	if (ret == IPMI_ERROR_NOT_FOUND)
		return IPMI_ERROR_NO_CLIENT;
	return ret;
}

int SessionImpl::initialize(const ClientConfig *pConfig, ServerImpl *pServer, void *pUserData)
{
	const char *pServiceName;
	int ret;

	m_state = 0;
	m_magic = IPMI_SESSION_MAGIC;
	m_pServer = pServer;
	m_pNext = NULL;
	m_processId = pConfig->processId;
	m_appId = pConfig->appId;
	m_sessionId = 0;
	m_pUserData = pUserData;
	m_maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	m_asyncCallCount = pConfig->asyncCallCount;
	m_eventFlagCount = pConfig->eventFlagCount;

	ret = m_mutex.create();
	if (ret != 0)
		return ret;
	pServiceName = pServer->m_serviceName;
	ret = initialize_request_stuff(pConfig, pServiceName);
	if (ret != 0)
		return client_error(ret);
	ret = initialize_sync_call_stuff(pConfig, pServiceName);
	if (ret != 0) {
		finalize_request_stuff();
		return client_error(ret);
	}
	ret = initialize_async_call_stuff(pConfig, pServiceName);
	if (ret != 0) {
		finalize_request_stuff();
		finalize_sync_call_stuff();
		return client_error(ret);
	}
	ret = initialize_event_flag(pConfig, pServiceName);
	if (ret != 0) {
		finalize_request_stuff();
		finalize_sync_call_stuff();
		finalize_async_call_stuff();
		return client_error(ret);
	}
	ret = initialize_msg_queue_stuff(pConfig, pServiceName);
	if (ret != 0) {
		finalize_request_stuff();
		finalize_sync_call_stuff();
		finalize_async_call_stuff();
		finalize_event_flag();
		return client_error(ret);
	}
	ret = initialize_process_term_callback(pServiceName);
	if (ret != 0) {
		finalize_request_stuff();
		finalize_sync_call_stuff();
		finalize_async_call_stuff();
		finalize_event_flag();
		IPMI_ASSERT_SUCCESS(finalize_msg_queue_stuff());
		return client_error(ret);
	}
	return 0;
}

int SessionImpl::destroy()
{
	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if ((m_state & IPMI_SESSION_STATE_PROCESS_EXITED) != 0 && m_sessionId != 0) {
		m_sessionId = 0;
		sceKernelUnregisterProcessTerminationCallback(m_processId,
							      m_processTerminationCallbackId);
		IPMI_ASSERT_SUCCESS(sceKernelDeleteCallback(m_processTerminationCallbackId));
		finalize_request_stuff();
		finalize_async_call_stuff();
		finalize_event_flag();
		IPMI_ASSERT_SUCCESS(finalize_msg_queue_stuff());
		finalize_sync_call_stuff();
	}
	IPMI_ASSERT_SUCCESS(m_mutex.finalize());
	m_magic = 0;
	return 0;
}

SceUID SessionImpl::getProcessId()
{
	if (m_magic != IPMI_SESSION_MAGIC)
		return -1;
	return m_processId;
}

int SessionImpl::getAppId()
{
	if (m_magic != IPMI_SESSION_MAGIC)
		return -1;
	return m_appId;
}

void *SessionImpl::getUserData()
{
	if (m_magic != IPMI_SESSION_MAGIC)
		return NULL;
	return m_pUserData;
}

Server *SessionImpl::getServer()
{
	if (m_magic != IPMI_SESSION_MAGIC)
		return NULL;
	return m_pServer;
}

int SessionImpl::respondToSyncMethodRequest(int result, const void *pOut, SceSize outSize)
{
	message::SyncCallResponse response;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	IPMI_ASSERT(m_mutex.lock() == 0);
	if (!m_syncCallPending || m_syncCallIsMulti) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NOT_INITIALIZED;
	}
	if (pOut == NULL && outSize != 0) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_INVALID_INDEX;
	}
	if (outSize > m_syncCallOutSize) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_RESPONSE_TOO_LARGE;
	}
	m_syncCallPending = false;
	m_syncCallOutSize = 0;
	IPMI_ASSERT(m_mutex.unlock() == 0);

	response.type = IPMI_MESSAGE_TYPE_RESPONSE;
	response.size = sizeof(response);
	response.error = 0;
	response.result = result;
	response.outSize = outSize;
	IPMI_ASSERT(m_syncResponsePipe.trySend(&response, sizeof(response)) == 0);
	if (outSize != 0)
		IPMI_ASSERT(m_syncResponsePipe.trySend(pOut, outSize) == 0);
	IPMI_ASSERT_SUCCESS(m_syncResponseCounter.signal());
	return 0;
}

int SessionImpl::respondToSyncMethodRequest(int result, const BufferInfo *pOutList,
					    SceUInt32 outCount)
{
	message::SyncMultiCallResponse response;
	SceSize total;
	SceUInt32 i;
	int ret;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	IPMI_ASSERT(m_mutex.lock() == 0);
	if (!m_syncCallPending || !m_syncCallIsMulti) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NOT_INITIALIZED;
	}
	if ((pOutList == NULL && outCount != 0) ||
	    (outCount != 0 && outCount != m_syncCallOutCount)) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_INVALID_INDEX;
	}

	SceSize sizesSize = sizeof(SceSize) * outCount;
	SceSize outSizes[outCount];

	total = 0;
	for (i = 0; i < outCount; i++) {
		outSizes[i] = pOutList[i].outSize;
		total += outSizes[i];
		if (outSizes[i] > m_pSyncCallOutSizes[i]) {
			IPMI_ASSERT(m_mutex.unlock() == 0);
			return IPMI_ERROR_RESPONSE_TOO_LARGE;
		}
	}
	ret = 0;
	m_syncCallPending = false;
	m_syncCallOutSize = 0;
	IPMI_ASSERT(m_mutex.unlock() == 0);

	response.type = IPMI_MESSAGE_TYPE_RESPONSE;
	response.size = ((sizeof(SceSize) * m_syncCallOutCount + 15) & ~15) + total +
		sizeof(response);
	response.error = ret;
	response.result = result;
	response.outCount = m_syncCallOutCount;
	IPMI_ASSERT(m_syncResponsePipe.trySend(&response, sizeof(response)) == 0);
	if (outCount != 0) {
		IPMI_ASSERT(m_syncResponsePipe.trySend(outSizes, sizesSize) == 0);
		for (i = ret; i < outCount; i++)
			IPMI_ASSERT(m_syncResponsePipe.trySend(pOutList[i].pBuffer,
							       pOutList[i].outSize) == 0);
	}
	IPMI_ASSERT(m_syncResponseCounter.signal() == 0);
	return ret;
}

int SessionImpl::respondToAsyncMethodRequest(int requestId, int result, const void *pOut,
					     SceSize outSize)
{
	message::AsyncCallResponse response;
	AsyncCall *pCall;
	SceUInt32 i;
	int eventFlagIndex;
	SceUInt32 eventFlagBitPattern;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (pOut == NULL && outSize != 0)
		return IPMI_ERROR_INVALID_INDEX;
	if (outSize > m_maxAsyncCallOutSize)
		return IPMI_ERROR_RESPONSE_TOO_LARGE;
	IPMI_ASSERT(m_mutex.lock() == 0);
	pCall = NULL;
	for (i = 0; i < m_asyncCallCount; i++) {
		if (m_pAsyncCalls[i].requestId == requestId) {
			pCall = &m_pAsyncCalls[i];
			break;
		}
	}
	if (pCall == NULL) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_INVALID_REQUEST_ID;
	}
	switch (pCall->state) {
	case IPMI_ASYNC_CALL_PENDING:
		break;
	case IPMI_ASYNC_CALL_IDLE:
	case IPMI_ASYNC_CALL_RESPONDING:
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NO_PENDING_REQUEST;
	case IPMI_ASYNC_CALL_MULTI_PENDING:
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NOT_INITIALIZED;
	default:
		IPMI_ASSERT(0);
		break;
	}
	pCall->state = IPMI_ASYNC_CALL_RESPONDING;
	IPMI_ASSERT(m_mutex.unlock() == 0);

	response.type = IPMI_MESSAGE_TYPE_RESPONSE;
	response.size = sizeof(response);
	response.reserved0 = 0;
	response.result = result;
	response.outSize = outSize;
	response.requestId = requestId;
	IPMI_ASSERT(pCall->responsePipe.trySend(&response, sizeof(response)) == 0);
	if (outSize != 0)
		IPMI_ASSERT(pCall->responsePipe.trySend(pOut, outSize) == 0);

	IPMI_ASSERT(m_mutex.lock() == 0);
	pCall->state = IPMI_ASYNC_CALL_IDLE;
	eventFlagIndex = pCall->eventFlagIndex;
	eventFlagBitPattern = pCall->eventFlagBitPattern;
	IPMI_ASSERT(pCall->responseCounter.signal() == 0);
	IPMI_ASSERT(m_mutex.unlock() == 0);
	if (eventFlagIndex != -1)
		IPMI_ASSERT(setEventFlag(eventFlagIndex, eventFlagBitPattern) == 0);
	return 0;
}

int SessionImpl::respondToAsyncMethodRequest(SceUInt32 methodId, int requestId, int result,
					     const DataInfo *pOutList, SceUInt32 outCount)
{
	message::AsyncMultiCallResponse response;
	AsyncCall *pCall;
	SceSize total;
	SceUInt32 i;
	int eventFlagIndex;
	SceUInt32 eventFlagBitPattern;
	int ret;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (pOutList == NULL && outCount != 0)
		return IPMI_ERROR_INVALID_INDEX;

	SceSize outSizes[outCount];

	total = 0;
	ret = 0;
	for (i = 0; i < outCount; i++) {
		outSizes[i] = pOutList[i].dataSize;
		total += outSizes[i];
	}
	if (total + sizeof(SceSize) * outCount > m_maxAsyncCallOutSize)
		return IPMI_ERROR_RESPONSE_TOO_LARGE;
	IPMI_ASSERT(m_mutex.lock() == 0);
	pCall = NULL;
	for (i = ret; i < m_asyncCallCount; i++) {
		if (m_pAsyncCalls[i].requestId == requestId) {
			pCall = &m_pAsyncCalls[i];
			break;
		}
	}
	if (pCall == NULL) {
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_INVALID_REQUEST_ID;
	}
	switch (pCall->state) {
	case IPMI_ASYNC_CALL_MULTI_PENDING:
		break;
	case IPMI_ASYNC_CALL_IDLE:
	case IPMI_ASYNC_CALL_RESPONDING:
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NO_PENDING_REQUEST;
	case IPMI_ASYNC_CALL_PENDING:
		IPMI_ASSERT(m_mutex.unlock() == 0);
		return IPMI_ERROR_NOT_INITIALIZED;
	default:
		IPMI_ASSERT(0);
		break;
	}
	pCall->state = IPMI_ASYNC_CALL_RESPONDING;
	IPMI_ASSERT(m_mutex.unlock() == 0);

	response.type = IPMI_MESSAGE_TYPE_ASYNC_MULTI_CALL_RESPONSE;
	response.size = ((sizeof(SceSize) * outCount + 15) & ~15) + total + sizeof(response);
	response.methodId = methodId;
	response.requestId = requestId;
	response.reserved0 = ret;
	response.result = result;
	response.outCount = outCount;
	IPMI_ASSERT(pCall->responsePipe.trySend(&response, sizeof(response)) == 0);
	if (outCount != 0) {
		IPMI_ASSERT(pCall->responsePipe.trySend(outSizes, sizeof(SceSize) * outCount) == 0);
		for (i = ret; i < outCount; i++)
			IPMI_ASSERT(pCall->responsePipe.trySend(pOutList[i].pData,
								pOutList[i].dataSize) == 0);
	}

	IPMI_ASSERT(m_mutex.lock() == 0);
	pCall->state = ret;
	eventFlagIndex = pCall->eventFlagIndex;
	eventFlagBitPattern = pCall->eventFlagBitPattern;
	IPMI_ASSERT(pCall->responseCounter.signal() == 0);
	IPMI_ASSERT(m_mutex.unlock() == 0);
	if (eventFlagIndex != -1)
		IPMI_ASSERT(setEventFlag(eventFlagIndex, eventFlagBitPattern) == 0);
	return ret;
}

int SessionImpl::setEventFlag(SceUInt32 index, SceUInt32 bitPattern)
{
	int ret;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (index >= m_eventFlagCount)
		return IPMI_ERROR_INVALID_EVENT_FLAG;
	ret = m_pEventFlags[index].set(bitPattern);
	if (ret != 0)
		return ret;
	return 0;
}

int SessionImpl::sendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize,
			 SceUInt32 *pTimeout)
{
	DataInfo info[2];
	MsgQueue *pQueue;
	int ret;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (pMsg == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (index >= m_msgQueueCount)
		return IPMI_ERROR_INVALID_MSG_QUEUE;
	pQueue = &m_pMsgQueues[index];
	if (msgSize > pQueue->bufferSize - sizeof(message::MsgQueueHeader))
		return IPMI_ERROR_MSG_TOO_LARGE;

	message::MsgQueueHeader header;

	header.size = msgSize;
	header.reserved[0] = 0;
	header.reserved[1] = 0;
	header.reserved[2] = 0;
	IPMI_ASSERT_SUCCESS(pQueue->mutex.lock());
	info[0].pData = &header;
	info[0].dataSize = sizeof(header);
	info[1].pData = pMsg;
	info[1].dataSize = msgSize;
	ret = pQueue->pipe.sendVector(info, 2, pTimeout);
	if (ret == 0)
		IPMI_ASSERT_SUCCESS(pQueue->counter.signal());
	IPMI_ASSERT_SUCCESS(pQueue->mutex.unlock());
	return ret;
}

int SessionImpl::trySendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize)
{
	DataInfo info[2];
	MsgQueue *pQueue;
	int ret;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (pMsg == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (index >= m_msgQueueCount)
		return IPMI_ERROR_INVALID_MSG_QUEUE;
	pQueue = &m_pMsgQueues[index];
	if (msgSize > pQueue->bufferSize - sizeof(message::MsgQueueHeader))
		return IPMI_ERROR_MSG_TOO_LARGE;

	message::MsgQueueHeader header;

	header.size = msgSize;
	header.reserved[0] = 0;
	header.reserved[1] = 0;
	header.reserved[2] = 0;
	IPMI_ASSERT_SUCCESS(pQueue->mutex.lock());
	info[0].pData = &header;
	info[0].dataSize = sizeof(header);
	info[1].pData = pMsg;
	info[1].dataSize = msgSize;
	ret = pQueue->pipe.trySendVector(info, 2);
	if (ret == 0)
		IPMI_ASSERT_SUCCESS(pQueue->counter.signal());
	IPMI_ASSERT_SUCCESS(pQueue->mutex.unlock());
	return ret;
}

int SessionImpl::emptyMsgQueue(SceUInt32 index)
{
	MsgQueue *pQueue;

	if (m_magic != IPMI_SESSION_MAGIC)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (index >= m_msgQueueCount)
		return IPMI_ERROR_INVALID_MSG_QUEUE;
	pQueue = &m_pMsgQueues[index];
	IPMI_ASSERT_SUCCESS(pQueue->mutex.lock());
	IPMI_ASSERT_SUCCESS(pQueue->counter.cancel(0));
	IPMI_ASSERT_SUCCESS(pQueue->pipe.cancel());
	IPMI_ASSERT_SUCCESS(pQueue->mutex.unlock());
	return 0;
}

int SessionImpl::handle_session_close_request()
{
	m_sessionId = 0;
	sceKernelUnregisterProcessTerminationCallback(m_processId, m_processTerminationCallbackId);
	IPMI_ASSERT_SUCCESS(sceKernelDeleteCallback(m_processTerminationCallbackId));
	finalize_request_stuff();
	finalize_async_call_stuff();
	finalize_event_flag();
	IPMI_ASSERT_SUCCESS(finalize_msg_queue_stuff());
	IPMI_ASSERT_SUCCESS(m_syncResponseCounter.signal());
	finalize_sync_call_stuff();
	m_pServer->m_pEventHandler->onSessionClosedByClient(this);
	return 0;
}

int SessionImpl::handle_async_multi_data_call_request(void *pBuffer, SceSize bufferSize)
{
	message::AsyncMultiCallRequest *pRequest =
		static_cast<message::AsyncMultiCallRequest *>(pBuffer);
	const SceSize *pInSizes;
	char *pData;
	AsyncCall *pCall;
	SceSize received;
	SceUInt32 i;
	int ret;

	IPMI_ASSERT(pRequest->type == IPMI_MESSAGE_TYPE_ASYNC_MULTI_CALL_REQUEST);
	IPMI_ASSERT(pRequest->size <= bufferSize);
	ret = m_requestPipe.tryReceive(static_cast<message::Header *>(pRequest) + 1, &received,
				       pRequest->size - sizeof(message::Header));
	if (ret == IPMI_ERROR_INVALID_ARGUMENT)
		return ret;
	IPMI_ASSERT(ret == 0);
	if (pRequest->sessionId != m_sessionId)
		return IPMI_ERROR_SESSION_NOT_INITIALIZED;
	if (pRequest->inCount > IPMI_MULTI_DATA_MAX)
		return IPMI_ERROR_TOO_MANY_INPUTS;

	ret = 0;
	pInSizes = NULL;
	pData = reinterpret_cast<char *>(pRequest + 1);
	if (pRequest->inCount != 0) {
		pInSizes = reinterpret_cast<const SceSize *>(pData);
		pData += (sizeof(SceSize) * pRequest->inCount + 15) & ~15;
	}

	DataInfo inList[pRequest->inCount];

	for (i = 0; i < pRequest->inCount; i++) {
		IPMI_ASSERT_SUCCESS((reinterpret_cast<SceUIntPtr>(pData) & 15) != 0);
		inList[i].pData = pData;
		inList[i].dataSize = pInSizes[i];
		pData += (pInSizes[i] + 15) & ~15;
	}

	IPMI_ASSERT(pRequest->asyncCallIndex < m_asyncCallCount);
	pCall = &m_pAsyncCalls[pRequest->asyncCallIndex];
	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	IPMI_ASSERT(pCall->state == IPMI_ASYNC_CALL_IDLE);
	pCall->state = IPMI_ASYNC_CALL_MULTI_PENDING;
	pCall->requestId = pRequest->requestId;
	pCall->eventFlagIndex = pRequest->eventFlagIndex;
	pCall->eventFlagBitPattern = pRequest->eventFlagBitPattern;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());
	m_pServer->m_pEventHandler->onAsyncMethodDispatch(this, pRequest->requestId,
							  pRequest->methodId, inList,
							  pRequest->inCount);
	return ret;
}

int SessionImpl::handle_async_call_request(void *pBuffer, SceSize bufferSize)
{
	message::AsyncCallRequest *pRequest = static_cast<message::AsyncCallRequest *>(pBuffer);
	void *pWork;
	SceSize workSize;
	AsyncCall *pCall;
	SceSize received;
	int ret;

	IPMI_ASSERT(pRequest->type == IPMI_MESSAGE_TYPE_ASYNC_CALL_REQUEST);
	IPMI_ASSERT(pRequest->size <= bufferSize);
	ret = m_requestPipe.tryReceive(static_cast<message::Header *>(pRequest) + 1, &received,
				       pRequest->size - sizeof(message::Header));
	if (ret == IPMI_ERROR_INVALID_ARGUMENT)
		return ret;
	IPMI_ASSERT(ret == 0);
	pWork = static_cast<char *>(pBuffer) + pRequest->size;
	workSize = bufferSize - pRequest->size;

	IPMI_ASSERT(pRequest->asyncCallIndex < m_asyncCallCount);
	pCall = &m_pAsyncCalls[pRequest->asyncCallIndex];
	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	IPMI_ASSERT(pCall->state == IPMI_ASYNC_CALL_IDLE);
	pCall->state = IPMI_ASYNC_CALL_PENDING;
	pCall->requestId = pRequest->requestId;
	pCall->eventFlagIndex = pRequest->eventFlagIndex;
	pCall->eventFlagBitPattern = pRequest->eventFlagBitPattern;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());
	m_pServer->m_pEventHandler->onAsyncMethodDispatch(this, pRequest->requestId,
							  pRequest->methodId,
							  pRequest->inSize != 0 ? pRequest + 1 : NULL,
							  pRequest->inSize, pWork, workSize);
	return 0;
}

int SessionImpl::handle_sync_multi_data_call_request(void *pBuffer, SceSize bufferSize)
{
	message::SyncMultiCallRequest *pRequest =
		static_cast<message::SyncMultiCallRequest *>(pBuffer);
	const SceSize *pSizes;
	const SceSize *pInSizes;
	const SceSize *pOutSizes;
	char *pData;
	SceSize outTotal;
	SceSize received;
	SceUInt32 i;
	int ret;

	IPMI_ASSERT(pRequest->type == IPMI_MESSAGE_TYPE_SYNC_MULTI_CALL_REQUEST);
	IPMI_ASSERT(pRequest->size <= bufferSize);
	ret = m_requestPipe.tryReceive(static_cast<message::Header *>(pRequest) + 1, &received,
				       pRequest->size - sizeof(message::Header));
	if (ret == IPMI_ERROR_INVALID_ARGUMENT)
		return ret;
	IPMI_ASSERT(ret == 0);
	if (pRequest->sessionId != m_sessionId || pRequest->inCount > IPMI_MULTI_DATA_MAX ||
	    pRequest->outCount > IPMI_MULTI_DATA_MAX) {
		message::SyncCallResponse response;

		response.type = IPMI_MESSAGE_TYPE_RESPONSE;
		response.size = sizeof(response);
		response.error = IPMI_ERROR_INTERNAL;
		response.result = -1;
		response.outSize = 0;
		IPMI_ASSERT_SUCCESS(m_syncResponsePipe.trySend(&response, sizeof(response)));
		IPMI_ASSERT_SUCCESS(m_syncResponseCounter.signal());
		return 0;
	}

	m_syncCallOutCount = pRequest->outCount;
	m_syncCallIsMulti = true;
	m_syncCallPending = true;
	pSizes = reinterpret_cast<const SceSize *>(pRequest + 1);
	pInSizes = NULL;
	if (pRequest->inCount != 0) {
		pInSizes = pSizes;
		pSizes += pRequest->inCount;
	}
	pOutSizes = NULL;
	if (pRequest->outCount != 0) {
		pOutSizes = pSizes;
		pSizes += pRequest->outCount;
	}
	m_pSyncCallOutSizes = pOutSizes;
	pData = reinterpret_cast<char *>(pRequest + 1) +
		((sizeof(SceSize) * (pRequest->inCount + pRequest->outCount) + 15) & ~15);

	DataInfo inList[pRequest->inCount];
	BufferInfo outList[pRequest->outCount];

	for (i = 0; i < pRequest->inCount; i++) {
		IPMI_ASSERT_SUCCESS((reinterpret_cast<SceUIntPtr>(pData) & 15) != 0);
		inList[i].pData = pData;
		inList[i].dataSize = pInSizes[i];
		pData += (pInSizes[i] + 15) & ~15;
	}
	IPMI_ASSERT(static_cast<char *>(pBuffer) + pRequest->size == pData);
	outTotal = 0;
	for (i = 0; i < pRequest->outCount; i++) {
		IPMI_ASSERT_SUCCESS((reinterpret_cast<SceUIntPtr>(pData) & 15) != 0);
		outList[i].outSize = 0;
		outList[i].pBuffer = pData;
		outList[i].bufferSize = pOutSizes[i];
		pData += (pOutSizes[i] + 15) & ~15;
		outTotal += (pOutSizes[i] + 15) & ~15;
	}

	if (outTotal > bufferSize || pRequest->size + outTotal > bufferSize) {
		ret = IPMI_ERROR_INPUT_TOO_LARGE;
	} else {
		m_pServer->m_pEventHandler->onSyncMethodDispatch(this, pRequest->methodId, inList,
								 pRequest->inCount, outList,
								 pRequest->outCount);
		if (!m_syncCallPending)
			return 0;
		ret = IPMI_ERROR_NO_RESPONSE;
	}

	message::SyncCallResponse response;

	response.type = IPMI_MESSAGE_TYPE_RESPONSE;
	response.size = sizeof(response);
	response.error = ret;
	response.result = -1;
	response.outSize = 0;
	IPMI_ASSERT_SUCCESS(m_syncResponsePipe.trySend(&response, sizeof(response)));
	IPMI_ASSERT_SUCCESS(m_syncResponseCounter.signal());
	m_syncCallPending = false;
	return 0;
}

int SessionImpl::handle_sync_call_request(void *pBuffer, SceSize bufferSize)
{
	message::SyncCallRequest *pRequest = static_cast<message::SyncCallRequest *>(pBuffer);
	SceSize outSize;
	SceSize received;
	int ret;

	IPMI_ASSERT(pRequest->type == IPMI_MESSAGE_TYPE_SYNC_CALL_REQUEST);
	IPMI_ASSERT(pRequest->size <= bufferSize);
	ret = m_requestPipe.tryReceive(static_cast<message::Header *>(pRequest) + 1, &received,
				       pRequest->size - sizeof(message::Header));
	if (ret == IPMI_ERROR_INVALID_ARGUMENT)
		return ret;
	IPMI_ASSERT(ret == 0);
	m_syncCallPending = true;
	outSize = pRequest->outSize;
	m_syncCallIsMulti = false;
	m_syncCallOutSize = outSize;
	m_pServer->m_pEventHandler->onSyncMethodDispatch(this, pRequest->methodId,
							 pRequest->inSize != 0 ? pRequest + 1 : NULL,
							 pRequest->inSize, outSize,
							 static_cast<char *>(pBuffer) + pRequest->size,
							 bufferSize - pRequest->size);
	if (m_syncCallPending) {
		message::SyncCallResponse response;

		response.type = IPMI_MESSAGE_TYPE_RESPONSE;
		response.size = sizeof(response);
		response.error = IPMI_ERROR_NO_RESPONSE;
		response.result = -1;
		response.outSize = 0;
		IPMI_ASSERT_SUCCESS(m_syncResponsePipe.trySend(&response, sizeof(response)));
		IPMI_ASSERT_SUCCESS(m_syncResponseCounter.signal());
		m_syncCallPending = false;
		m_syncCallOutSize = 0;
	}
	return 0;
}

int SessionImpl::tryDispatch(void *pBuffer, SceSize bufferSize)
{
	message::Header *pHeader = static_cast<message::Header *>(pBuffer);
	SceSize received;
	int ret;

	if (m_requestCounter.tryWait() != 0)
		return IPMI_ERROR_WOULD_BLOCK;
	ret = m_requestPipe.tryReceive(pBuffer, &received, sizeof(message::Header));
	if (ret == IPMI_ERROR_INVALID_ARGUMENT)
		return ret;
	IPMI_ASSERT(ret == 0);
	IPMI_ASSERT(received == sizeof(message::Header));
	IPMI_ASSERT(pHeader->size <= bufferSize && pHeader->size >= sizeof(message::Header));
	switch (pHeader->type) {
	case IPMI_MESSAGE_TYPE_DISCONNECT_REQUEST:
		ret = handle_session_close_request();
		if (ret == 0)
			ret = IPMI_ERROR_SESSION_CLOSED;
		break;
	case IPMI_MESSAGE_TYPE_SYNC_CALL_REQUEST:
		ret = handle_sync_call_request(pBuffer, bufferSize);
		break;
	case IPMI_MESSAGE_TYPE_ASYNC_CALL_REQUEST:
		ret = handle_async_call_request(pBuffer, bufferSize);
		break;
	case IPMI_MESSAGE_TYPE_SYNC_MULTI_CALL_REQUEST:
		ret = handle_sync_multi_data_call_request(pBuffer, bufferSize);
		break;
	case IPMI_MESSAGE_TYPE_ASYNC_MULTI_CALL_REQUEST:
		ret = handle_async_multi_data_call_request(pBuffer, bufferSize);
		break;
	default:
		IPMI_ASSERT(0);
		break;
	}
	return ret;
}

}
}

/* An empty function the original ends this file with; nothing refers to it. The name is ours. */
void sessionImplEnd()
{
}
