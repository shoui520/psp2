#ifndef BLOCKING_IPC_SERVER_H
#define BLOCKING_IPC_SERVER_H

#include <stddef.h>
#include <kernel.h>
#include "blocking_ipc_client.h"
#include "../ipmi/server.h"

/*
 * The server end of BlockingIpc: an IPMI server with its own dispatcher thread, which
 * answers the three request methods with the owner's callbacks and passes every other call
 * on. Sony's messages name it Server::on{Sync,Async}MethodDispatch() under SceBlockingIpc;
 * the file name (blocking_ipc_server.cpp) and the member names are ours.
 */

namespace BlockingIpc {

class Server : public IPMI::Server::EventHandler {
public:
	/* What the owner implements; NULL where it has nothing to do. */
	struct Callbacks {
		int (*createRequest)(IPMI::Session *pSession, const void *pParam, SceSize paramSize);
		int (*destroyRequest)(IPMI::Session *pSession, int requestId);
		int (*abortRequest)(IPMI::Session *pSession, int requestId);
		void (*onSessionCreated)(IPMI::Session *pSession);
		void (*onSessionClosedByClient)(IPMI::Session *pSession);
		int (*onSyncMethod)(IPMI::Session *pSession, SceUInt32 methodId,
				    const IPMI::DataInfo *pInList, SceUInt32 inCount,
				    IPMI::BufferInfo *pOutList, SceUInt32 outCount);
		int (*onAsyncMethod)(IPMI::Session *pSession, SceUInt32 requestId,
				     SceUInt32 methodId, const IPMI::DataInfo *pInList,
				     SceUInt32 inCount);
		void (*onSessionKilled)(IPMI::Session *pSession);

		Callbacks()
		{
			createRequest = NULL;
			destroyRequest = NULL;
			abortRequest = NULL;
			onSessionCreated = NULL;
			onSessionClosedByClient = NULL;
			onSyncMethod = NULL;
			onAsyncMethod = NULL;
			onSessionKilled = NULL;
		}
	};

	struct Config {
		char name[16];
		SceUInt32 reserved;
		int threadPriority;
		SceSize threadStackSize;
		SceUInt32 maxSessions;
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
		Callbacks callbacks;
		int budget;
	};

	static int create(Server **ppServer, const Config *pConfig, void *pMemory);
	static SceSize getBufferSize(const Config *pConfig);
	static int threadEntry(SceSize argSize, void *pArgBlock);

	int initialize(const Config *pConfig);
	int finalize();
	int destroy();
	int start();
	int stop();
	int runDispatcher();

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
					  SceSize inSize, SceUInt32 outCount, void *pOut,
					  SceSize outSize);
	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *pServer);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

private:
	/* One session's place: in use, how often reused, its memory and the session. */
	struct SessionEntry {
		SceUInt32 inUse : 8;
		SceUInt32 serial : 24;
		void *pMemory;
		IPMI::Session *pSession;
	};

	SceUInt32 m_maxSessions;
	SceUInt32 m_asyncCallCount;
	SceUInt32 m_msgQueueCount;
	SceUInt32 m_slotCount;
	IPMI::Server *m_pServer;
	SessionEntry *m_pSessions;
	SceUInt32 m_activeSessions;
	void *m_pWorkBuffer;
	SceSize m_workBufferSize;
	SceUID m_threadId;
	int m_running;
	Callbacks m_callbacks;
};

}

#endif
