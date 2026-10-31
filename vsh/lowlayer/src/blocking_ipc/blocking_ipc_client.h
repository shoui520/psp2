#ifndef BLOCKING_IPC_CLIENT_H
#define BLOCKING_IPC_CLIENT_H

#include <stddef.h>
#include <kernel.h>
#include "../ipmi/client.h"

/*
 * BlockingIpc: an IPMI client whose requests a server finishes later. The client creates a
 * request (a sync call the server answers with its ID), then brackets the blocking call with
 * beginBlockingIpc and endBlockingIpc; at most blockingIpcCount run at once, and the others
 * wait in line on their request's semaphore. Sony's file: blocking_ipc/blocking_ipc_client.cpp;
 * its log prefix names the library SceBlockingIpc. The class and member names are ours.
 */

/* Our names for SceBlockingIpc's error codes (facility 0x0010, 0x801017xx). */
#define BLOCKING_IPC_ERROR_INVALID_STATE	-2146429184	/* 0x80101700 */
#define BLOCKING_IPC_ERROR_ALREADY_CONNECTED	-2146429183	/* 0x80101701 */
#define BLOCKING_IPC_ERROR_NOT_CONNECTED	-2146429182	/* 0x80101702 */
#define BLOCKING_IPC_ERROR_NO_CLIENT		-2146429181	/* 0x80101703 */
#define BLOCKING_IPC_ERROR_TOO_MANY_SESSIONS	-2146429179	/* 0x80101705 */
#define BLOCKING_IPC_ERROR_NOT_READY		-2146429175	/* 0x80101709 */
#define BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND	-2146429152	/* 0x80101720 */
#define BLOCKING_IPC_ERROR_REQUEST_BUSY		-2146429151	/* 0x80101721 */
#define BLOCKING_IPC_ERROR_REQUEST_ABORTED	-2146429150	/* 0x80101722 */
#define BLOCKING_IPC_ERROR_NO_FREE_REQUEST	-2146429149	/* 0x80101723 */
#define BLOCKING_IPC_ERROR_NOT_BEGUN		-2146429148	/* 0x80101724 */
#define BLOCKING_IPC_ERROR_TIMEOUT		-2146429088	/* 0x80101760 */

namespace BlockingIpc {

/* The sync methods a BlockingIpc server implements. */
enum MethodId {
	METHOD_CREATE_REQUEST = 0,
	METHOD_DESTROY_REQUEST = 1,
	METHOD_ABORT_REQUEST = 2
};

class Client {
public:
	struct Config {
		char name[32];
		SceUInt32 requestCount;
		SceSize maxRequestSize;
		SceSize maxResponseSize;
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 blockingIpcCount;
		SceSize maxBlockingInSize;
		SceSize maxBlockingOutSize;
		SceUInt32 asyncCallCount;
		SceUInt32 msgQueueCount;
		struct {
			SceSize msgSize;
			SceUInt32 msgCount;
		} msgQueue[32];
		SceUInt32 slotCount;
		int budget;
	};

	/* Where the server signals the end of a blocking call: an event flag and its bits. */
	struct Handle {
		SceUInt32 eventFlagIndex;
		SceUInt32 bitPattern;
	};

	static int create(Client **ppClient, const Config *pConfig, void *pMemory);
	static SceSize getBufferSize(const Config *pConfig);
	static SceSize getSessionBufferSize(const Config *pConfig);

	int initialize(const Config *pConfig);
	int destroy();
	int connect();
	int disconnect();
	int createRequest(int *pRequestId, const void *pParam, SceSize paramSize);
	int destroyRequest(int requestId);
	int abortRequest(int requestId);
	int beginBlockingIpc(int requestId, Handle *pHandle);
	int endBlockingIpc(int requestId);
	int waitBlockingIpc(const Handle *pHandle, SceUInt32 *pTimeout);
	IPMI::Client *getClient();

private:
	enum SlotState {
		SLOT_FREE = 0,
		SLOT_CREATED = 1,
		SLOT_WAITING = 2,
		SLOT_ACTIVE = 3,
		SLOT_ABORTED = 4
	};

	/* One request: the server's ID for it and the semaphore it waits in line on. */
	struct Slot {
		int requestId;
		int state;
		SceUID sema;
		Slot *pNext;
	};

	SceKernelLwMutexWork m_mutex;
	IPMI::Client *m_pClient;
	int m_connected;
	Slot *m_pSlots;
	Slot *m_pWaitQueue;
	SceUInt32 m_maxActive;
	SceUInt32 m_activeCount;
	SceUInt32 m_eventFlagBase;
	SceUInt32 m_slotCount;
};

}

#endif
