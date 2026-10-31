#ifndef IPMI_SERVER_H
#define IPMI_SERVER_H

#include <stddef.h>
#include <kernel.h>
#include "client.h"
#include "session.h"

namespace IPMI {

/*
 * The server end: one dispatcher serving many sessions. Sony's class: IPMI::Server, its
 * implementation IPMI::impl::ServerImpl (server_impl.cpp); the method names are 0.990's.
 */
class Server {
public:
	class EventHandler;

	/* What the server allocates; the event handler receives every call. */
	struct Config {
		SceSize maxRequestSize;
		SceSize maxMessageSize;
		EventHandler *pEventHandler;
		char serviceName[16];
		SceUInt32 reserved;
		int budget;
		int openLimit;

		Config()
		{
			maxRequestSize = 0xF00;
			maxMessageSize = 0xF00;
			pEventHandler = NULL;
			budget = DEFAULT_BUDGET;
			openLimit = -1;
			serviceName[0] = '\0';
		}
	};

	/* Sony's IPMI::Server::EventHandler: what a server implements. */
	class EventHandler {
	public:
		virtual ~EventHandler();
		virtual void onSyncMethodDispatch(Session *pSession, SceUInt32 methodId,
						  void *pIn, SceSize inSize, SceUInt32 outCount,
						  void *pOut, SceSize outSize);
		virtual void onSyncMethodDispatch(Session *pSession, SceUInt32 methodId,
						  const DataInfo *pInList, SceUInt32 inCount,
						  BufferInfo *pOutList, SceUInt32 outCount);
		virtual void onAsyncMethodDispatch(Session *pSession, SceUInt32 requestId,
						   SceUInt32 methodId, void *pIn, SceSize inSize,
						   void *pOut, SceSize outSize);
		virtual void onAsyncMethodDispatch(Session *pSession, SceUInt32 requestId,
						   SceUInt32 methodId, const DataInfo *pInList,
						   SceUInt32 inCount);
		/* A client connects with its Config and parameter; call createSession to accept it. */
		virtual int onNewClient(Server *pServer, const Session::Config *pConfig,
					const void *pParam, SceSize paramSize) = 0;
		virtual void onSessionClosedByClient(Session *pSession) = 0;
		virtual void onSessionKilled(Session *pSession);
	};

	static int create(Server **ppServer, const Config *pConfig, void *pUserData,
			  void *pMemory);
	static SceSize getWorkBufferSize(const Config *pConfig);

	virtual int destroy() = 0;
	virtual int tryDispatch(void *pBuffer, SceSize bufferSize) = 0;
	virtual int runDispatcher(void *pBuffer, SceSize bufferSize) = 0;
	virtual int shutdownDispatcher() = 0;
	virtual int createSession(Session **ppSession, void *pUserData, void *pMemory) = 0;
	virtual void *getUserData() = 0;
	virtual int notifyProcessExit(SceUID processId) = 0;

	virtual ~Server();
};

}

#endif
