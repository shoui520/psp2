#ifndef IPMI_MESSAGE_H
#define IPMI_MESSAGE_H

#include <kernel.h>
#include <libkernel/libkernel.h>

namespace IPMI {
namespace impl {
namespace message {

/* Sony's IPMI::impl::message::Header: what every message between client and server starts with. */
struct Header {
	SceUInt32 type;
	SceSize size;
};

#define IPMI_MESSAGE_TYPE_CONNECT_REQUEST	1
#define IPMI_MESSAGE_TYPE_CONNECT_RESPONSE	2
#define IPMI_MESSAGE_TYPE_DISCONNECT_REQUEST	3
#define IPMI_MESSAGE_TYPE_SYNC_CALL_REQUEST	4
#define IPMI_MESSAGE_TYPE_ASYNC_CALL_REQUEST	6
#define IPMI_MESSAGE_TYPE_SYNC_MULTI_CALL_REQUEST	8
#define IPMI_MESSAGE_TYPE_ASYNC_MULTI_CALL_REQUEST	10
#define IPMI_MESSAGE_TYPE_PROCESS_EXIT		12

/*
 * A client asking to connect, sent to the server's pipe: the client's limits, then its message
 * queues' sizes and the caller's parameter. size counts the header and the parameter.
 */
struct ConnectRequest : Header {
	SceUID pid;
	SceUInt32 clientIndex;
	int appId;
	SceUInt32 syncCallCount;
	SceSize maxSyncCallInSize;
	SceSize maxSyncCallOutSize;
	SceUInt32 asyncCallCount;
	SceSize maxAsyncCallInSize;
	SceSize maxAsyncCallOutSize;
	SceUInt32 eventFlagCount;
	SceUInt32 msgQueueCount;
	SceSize paramSize;
	SceUInt32 reserved[2];

	ConnectRequest(SceUID pid, SceUInt32 clientIndex, int appId, SceUInt32 syncCallCount,
		       SceSize maxSyncCallInSize, SceSize maxSyncCallOutSize,
		       SceUInt32 asyncCallCount, SceSize maxAsyncCallInSize,
		       SceSize maxAsyncCallOutSize, SceUInt32 eventFlagCount,
		       SceUInt32 msgQueueCount, const SceSize *pMsgQueueBufferSize,
		       const void *pParam, SceSize paramSize)
	{
		this->type = IPMI_MESSAGE_TYPE_CONNECT_REQUEST;
		this->size = paramSize + sizeof(ConnectRequest);
		this->pid = pid;
		this->clientIndex = clientIndex;
		this->appId = appId;
		this->syncCallCount = syncCallCount;
		this->maxSyncCallInSize = maxSyncCallInSize;
		this->maxSyncCallOutSize = maxSyncCallOutSize;
		this->asyncCallCount = asyncCallCount;
		this->maxAsyncCallInSize = maxAsyncCallInSize;
		this->maxAsyncCallOutSize = maxAsyncCallOutSize;
		this->eventFlagCount = eventFlagCount;
		this->msgQueueCount = msgQueueCount;
		this->paramSize = paramSize;
		if (msgQueueCount != 0)
			sceClibMemcpy_safe(getMsgQueueBufferSize(), pMsgQueueBufferSize,
					   sizeof(SceSize) * msgQueueCount);
		if (pParam != NULL && paramSize != 0)
			sceClibMemcpy_safe(&getMsgQueueBufferSize()[this->msgQueueCount], pParam,
					   paramSize);
	}

	SceSize *getMsgQueueBufferSize() { return reinterpret_cast<SceSize *>(this + 1); }
};

/* The server's answer to a ConnectRequest, in the client's request pipe. */
struct ConnectResponse : Header {
	int error;
	int result;
	SceUInt32 sessionId;
};

/*
 * A client's process has exited: the server sends this into its own pipe, so that its
 * dispatcher drops the sessions of every exited process (ServerImpl::handle_process_exit).
 */
struct ProcessExitNotification : Header {
	SceUInt32 reserved[2];
};

/* A synchronous method call, followed in the pipe by its inSize bytes of input. */
struct SyncCallRequest : Header {
	SceUInt32 sessionId;
	SceUInt32 methodId;
	SceSize inSize;
	SceSize outSize;
	SceUInt32 reserved[2];
};

/* The server's answer to a SyncCallRequest, followed by its outSize bytes of output. */
struct SyncCallResponse : Header {
	int error;
	int result;
	SceSize outSize;
	SceUInt32 reserved[3];
};

/*
 * A synchronous call with several inputs and outputs. The pipe carries after it the input
 * sizes, the output buffer sizes (padded to 16 bytes together), then each input padded to 16.
 */
struct SyncMultiCallRequest : Header {
	SceUInt32 sessionId;
	SceUInt32 methodId;
	SceUInt32 inCount;
	SceUInt32 outCount;
	SceUInt32 reserved[2];
};

/*
 * An asynchronous method call, followed by its input. The answer goes to the client's
 * asyncCallIndex response pipe; eventFlagIndex (-1: none) gets eventFlagBitPattern set.
 */
struct AsyncCallRequest : Header {
	SceUInt32 sessionId;
	SceUInt32 methodId;
	int requestId;
	SceSize inSize;
	SceUInt32 eventFlagIndex;
	SceUInt32 eventFlagBitPattern;
	SceUInt32 asyncCallIndex;
	SceUInt32 reserved[3];
};

/* An asynchronous call with several inputs: the input sizes padded to 16, then each input padded to 16. */
struct AsyncMultiCallRequest : Header {
	SceUInt32 sessionId;
	SceUInt32 methodId;
	int requestId;
	SceUInt32 inCount;
	SceUInt32 reserved0;
	SceUInt32 eventFlagIndex;
	SceUInt32 eventFlagBitPattern;
	SceUInt32 asyncCallIndex;
	SceUInt32 reserved[2];
};

/* The server's answer to an AsyncCallRequest, in the call's response pipe, then its output. */
struct AsyncCallResponse : Header {
	SceUInt32 reserved0;
	int result;
	SceSize outSize;
	int requestId;
	SceUInt32 reserved[2];
};

/* The server's answer to an AsyncMultiCallRequest: then the outCount output sizes and outputs. */
struct AsyncMultiCallResponse : Header {
	SceUInt32 methodId;
	int requestId;
	SceUInt32 reserved0;
	int result;
	SceUInt32 outCount;
	SceUInt32 reserved1;
};

/* What the server sends into a client message queue before each message. */
struct MsgQueueHeader {
	SceSize size;
	SceUInt32 reserved[3];
};

/* The server's answer to a SyncMultiCallRequest, followed by the outCount output sizes and outputs. */
struct SyncMultiCallResponse : Header {
	int error;
	int result;
	SceUInt32 outCount;
	SceUInt32 reserved[3];
};

}
}
}

#endif
