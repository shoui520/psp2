#ifndef IPMI_WRAPPER_SERVER_H
#define IPMI_WRAPPER_SERVER_H

#include <stddef.h>
#include <kernel.h>
#include "ipmi_wrapper_client.h"
#include "../ipmi/server.h"

/*
 * The server end of IpmiWrapper: an IPMI server with its own dispatcher thread, which passes
 * every multi-data call to the owner's callbacks and sets a client's callback bits
 * (signalCallback). Sony's messages name it Server::on{Sync,Async}MethodDispatch() under
 * SceIpmiWrapper; the file name (ipmi_wrapper_server.cpp) and the member names are ours.
 */

namespace IpmiWrapper {

class Server : public IPMI::Server::EventHandler {
public:
	/* What the owner implements; NULL where it has nothing to do. */
	struct Callbacks {
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
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 asyncCallCount;
		SceSize maxAsyncCallInSize;
		SceSize maxAsyncCallOutSize;
		SceUInt32 callbackCount;
		SceUInt32 msgQueueCount;
		struct {
			SceSize msgSize;
			SceUInt32 msgCount;
		} msgQueue[32];
		Callbacks callbacks;
		int budget;
	};

	static SceSize getSessionBufferSize(const Config *pConfig);
	static int create(Server **ppServer, const Config *pConfig, void *pMemory);
	static int threadEntry(SceSize argSize, void *pArgBlock);
	static SceSize getBufferSize(const Config *pConfig);

	int initialize(const Config *pConfig);
	int finalize();
	int destroy();
	int start();
	int stop();
	int signalCallback(IPMI::Session *pSession, SceUInt32 index);
	int runDispatcher();

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

private:
	/* One session's place: in use, its memory and the session. */
	struct SessionEntry {
		int inUse;
		void *pMemory;
		IPMI::Session *pSession;
	};

	SceUInt32 m_maxSessions;
	SceUInt32 m_callbackCount;
	SceUInt32 m_msgQueueCount;
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
