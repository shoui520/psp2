#include <libkernel/libkernel.h>
#include "server.h"
#include "server_impl.h"
#include "ipmi_error.h"

void *operator new(size_t size, void *where);

namespace IPMI {

/* What a message's header and the call's other fields add to the largest payload. */
#define SERVER_MESSAGE_OVERHEAD	256

static inline SceSize max_size(SceSize a, SceSize b)
{
	return a < b ? b : a;
}

SceSize Server::getWorkBufferSize(const Config *pConfig)
{
	return max_size(pConfig->maxRequestSize + SERVER_MESSAGE_OVERHEAD,
			pConfig->maxMessageSize + SERVER_MESSAGE_OVERHEAD);
}

/* A service needs a name; without one only the missing event handler is worse. */
static inline int check_service_name(const Server::Config *pConfig)
{
	if (pConfig->pEventHandler == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (sceClibStrnlen(pConfig->serviceName, sizeof(pConfig->serviceName)) == 0)
		return IPMI_ERROR_INVALID_INDEX;
	return 0;
}

int Server::create(Server **ppServer, const Config *pConfig, void *pUserData, void *pMemory)
{
	impl::ServerImpl *pServer;
	int openLimit;
	int ret;

	if (ppServer == NULL || pConfig == NULL || pMemory == NULL ||
	    pConfig->pEventHandler == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (((SceUIntPtr)pMemory & 3) != 0)
		return IPMI_ERROR_MISALIGNED;
	if (sceClibStrnlen(pConfig->serviceName, sizeof(pConfig->serviceName)) == 0)
		return check_service_name(pConfig);
	pServer = new (pMemory) impl::ServerImpl;
	openLimit = -1;
	if (SceLibKernel_9F793F84() >= 0x1800000)
		openLimit = pConfig->openLimit;
	ret = pServer->initialize(pConfig->serviceName, pConfig->pEventHandler, pUserData,
				  pConfig->maxRequestSize, pConfig->maxMessageSize,
				  getWorkBufferSize(pConfig), pConfig->budget, openLimit);
	if (ret != 0)
		return ret;
	*ppServer = pServer;
	return 0;
}

Server::~Server()
{
}

Server::EventHandler::~EventHandler()
{
}

/* The dispatch handlers a server does not override are not implemented: they stop. */
void Server::EventHandler::onSyncMethodDispatch(Session *pSession, SceUInt32 methodId,
						 void *pIn, SceSize inSize, SceUInt32 outCount,
						 void *pOut, SceSize outSize)
{
	__builtin_trap();
}

void Server::EventHandler::onSyncMethodDispatch(Session *pSession, SceUInt32 methodId,
						 const DataInfo *pInList, SceUInt32 inCount,
						 BufferInfo *pOutList, SceUInt32 outCount)
{
	__builtin_trap();
}

void Server::EventHandler::onAsyncMethodDispatch(Session *pSession, SceUInt32 requestId,
						  SceUInt32 methodId, void *pIn, SceSize inSize,
						  void *pOut, SceSize outSize)
{
	__builtin_trap();
}

void Server::EventHandler::onAsyncMethodDispatch(Session *pSession, SceUInt32 requestId,
						  SceUInt32 methodId, const DataInfo *pInList,
						  SceUInt32 inCount)
{
	__builtin_trap();
}

/*
 * 0.990 warns here that the session was killed without a handler; 3.6x keeps only the
 * call that read the warning's argument.
 */
void Server::EventHandler::onSessionKilled(Session *pSession)
{
	pSession->getServer();
}

}

/* An empty function the original ends this file with; nothing refers to it. The name is ours. */
void serverEnd()
{
}
