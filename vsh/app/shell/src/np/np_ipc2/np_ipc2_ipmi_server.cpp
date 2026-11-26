/*
 * np_ipc2_ipmi_server.cpp: Base IPMI::Server wrapper, client session array, and
 * dedicated "%sIpcServer" dispatch thread for NpIpc2.
 * Sony path: vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/np_ipc2_ipmi_server.cpp
 * 3.65-CEX: 0x813B8288..0x813B899B
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

struct DispatchThreadObj {
	NpIpc2ThreadBase base;
	IpmiServer *owner;
};

static int DispatchThreadEntry(DispatchThreadObj *threadObj)
{
	if (threadObj != NULL && threadObj->owner != NULL)
		return threadObj->owner->DispatchLoop();
	return 0;
}

static void DispatchThreadDestroy(DispatchThreadObj *threadObj)
{
	SceNpCommonPs4_502309F4(&threadObj->base);
	SceNpCommonPs4_BB5282C8(threadObj);
}

static const void *const kDispatchThreadVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(DispatchThreadDestroy),
	reinterpret_cast<const void *>(DispatchThreadEntry)
};

/* ============================================================================
 * IpmiSession (0x813B8288..0x813B8435)
 * ============================================================================ */

IpmiSession::IpmiSession()
	: m_ipmiSession(NULL), m_ipmiServer(NULL), m_sessionWorkBuf(NULL)
{
}

IpmiSession::~IpmiSession()
{
	Finalize();
}

int IpmiSession::Init(IpmiServer *server, const IPMI::Session::Config *sessionConfig)
{
	NP_IPC2_ASSERT(!IsInit(), "Init", "np_ipc2_ipmi_server.cpp", 66, "!IsInit()");

	SceSize sessionBufSize = sessionConfig->bufferSize;
	m_sessionWorkBuf = Alloc(sessionBufSize);
	if (m_sessionWorkBuf == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	int ret = server->GetIpmiServer()->createSession(&m_ipmiSession, NULL, m_sessionWorkBuf);
	if (ret < 0) {
		Free(m_sessionWorkBuf);
		m_sessionWorkBuf = NULL;
		return ret;
	}

	m_ipmiServer = server;
	NP_IPC2_ASSERT(IsInit(), "Init", "np_ipc2_ipmi_server.cpp", 88, "IsInit()");
	return 0;
}

void IpmiSession::Finalize(void)
{
	if (m_ipmiSession != NULL) {
		int ret = m_ipmiSession->destroy();
		NP_IPC2_ASSERT(ret == 0, "dtor", "np_ipc2_ipmi_server.cpp", 49, "ret==0");
		m_ipmiSession = NULL;
	}
	if (m_sessionWorkBuf != NULL) {
		Free(m_sessionWorkBuf);
		m_sessionWorkBuf = NULL;
	}
}

/* ============================================================================
 * IpmiServer (0x813B8438..0x813B899B)
 * ============================================================================ */

IpmiServer::IpmiServer()
	: m_nextServer(NULL),
	  m_prevServer(NULL),
	  m_ipmiServer(NULL),
	  m_serverObjBuf(NULL),
	  m_workBufSize(0),
	  m_workBuf(NULL),
	  m_mutex(NULL),
	  m_dispatchThread(NULL),
	  m_clients(NULL),
	  m_clientCount(0),
	  m_maxClients(0)
{
	sceClibMemset(m_serverName, 0, sizeof(m_serverName));
}

IpmiServer::~IpmiServer()
{
	Destroy();
}

int IpmiServer::Init(const IpmiServerConfig *config)
{
	NP_IPC2_ASSERT(!IsInit(), "Init", "np_ipc2_ipmi_server.cpp", 207, "!IsInit()");

	IPMI::Server::Config ipmiCfg;
	sceClibStrlcpy(ipmiCfg.serviceName, config->serverName, sizeof(ipmiCfg.serviceName));
	ipmiCfg.pEventHandler = this;
	ipmiCfg.openLimit = static_cast<int>(config->maxClients);
	SceUInt32 syncSum = config->maxSyncRequestSize + config->maxSyncResponseSize;
	ipmiCfg.maxRequestSize = (syncSum > config->maxEventDataSize)
					 ? syncSum
					 : config->maxEventDataSize;
	ipmiCfg.maxMessageSize = ipmiCfg.maxRequestSize;

	m_serverObjBuf = Alloc(256);
	if (m_serverObjBuf == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	m_workBufSize = IPMI::Server::getWorkBufferSize(&ipmiCfg);
	m_workBuf = Alloc(m_workBufSize);
	if (m_workBuf == NULL) {
		Destroy();
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}

	m_clients = static_cast<IpmiSession **>(Alloc(sizeof(IpmiSession *) * config->maxClients));
	if (m_clients == NULL) {
		Destroy();
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(m_clients, 0, sizeof(IpmiSession *) * config->maxClients);

	void *rawMutex = SceNpCommonPs4_0C922423(sizeof(NpIpc2Mutex), GetAllocator());
	if (rawMutex == NULL) {
		Destroy();
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}
	m_mutex = SceNpCommonPs4_4266D870(static_cast<NpIpc2Mutex *>(rawMutex));
	int ret = SceNpCommonPs4_8AA7E995(m_mutex, "SceNpIpc2IpmiServer", 1);
	if (ret < 0) {
		Destroy();
		return ret;
	}

	m_clientCount = 0;
	m_maxClients = config->maxClients;
	ret = IPMI::Server::create(&m_ipmiServer, &ipmiCfg, NULL, m_serverObjBuf);
	if (ret < 0) {
		Destroy();
		return ret;
	}

	sceClibStrlcpy(m_serverName, config->serverName, sizeof(m_serverName));
	NP_IPC2_ASSERT(IsInit(), "Init", "np_ipc2_ipmi_server.cpp", 268, "IsInit()");
	return 0;
}

void IpmiServer::Destroy(void)
{
	if (m_dispatchThread != NULL)
		StopDispatch();

	if (m_ipmiServer != NULL) {
		m_ipmiServer->destroy();
		m_ipmiServer = NULL;
	}

	if (m_clients != NULL) {
		for (SceUInt32 index = 0; index < m_maxClients; ++index) {
			if (m_clients[index] != NULL) {
				m_clients[index]->~IpmiSession();
				SceNpCommonPs4_6B428829(GetAllocator(), m_clients[index]);
				m_clients[index] = NULL;
			}
		}
		Free(m_clients);
		m_clients = NULL;
	}

	if (m_workBuf != NULL) {
		Free(m_workBuf);
		m_workBuf = NULL;
		m_workBufSize = 0;
	}

	if (m_serverObjBuf != NULL) {
		Free(m_serverObjBuf);
		m_serverObjBuf = NULL;
	}

	if (m_mutex != NULL) {
		SceNpCommonPs4_63EED478(m_mutex);
		SceNpCommonPs4_294F0B1A(m_mutex);
		SceNpCommonPs4_6B428829(GetAllocator(), m_mutex);
		m_mutex = NULL;
	}
}

int IpmiServer::StartDispatch(void)
{
	NP_IPC2_ASSERT(IsInit(), "StartDispatch", "np_ipc2_ipmi_server.cpp", 316, "IsInit()");
	NP_IPC2_ASSERT(m_dispatchThread == NULL, "StartDispatch", "np_ipc2_ipmi_server.cpp",
		       317, "m_thread == 0");

	void *rawThread = SceNpCommonPs4_0C922423(sizeof(DispatchThreadObj), GetAllocator());
	if (rawThread == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	DispatchThreadObj *threadObj = static_cast<DispatchThreadObj *>(rawThread);
	SceNpCommonPs4_04CBEB81(&threadObj->base);
	threadObj->base.vtable = &kDispatchThreadVtable[2];
	threadObj->owner = this;
	m_dispatchThread = &threadObj->base;

	char threadName[32];
	sceClibSnprintf(threadName, sizeof(threadName), "%sIpcServer", m_serverName);
	int ret = SceNpCommonPs4_D699FE34(m_dispatchThread, threadName, 0x10000100, 4096, 0);
	if (ret >= 0)
		ret = SceNpCommonPs4_8CBDFDB7(m_dispatchThread);

	if (ret < 0) {
		SceNpCommonPs4_502309F4(m_dispatchThread);
		SceNpCommonPs4_6B428829(GetAllocator(), m_dispatchThread);
		m_dispatchThread = NULL;
		return ret;
	}

	return 0;
}

int IpmiServer::StopDispatch(void)
{
	NP_IPC2_ASSERT(IsInit(), "StopDispatch", "np_ipc2_ipmi_server.cpp", 367, "IsInit()");
	NP_IPC2_ASSERT(m_dispatchThread != NULL, "StopDispatch", "np_ipc2_ipmi_server.cpp",
		       368, "m_thread != 0");

	m_ipmiServer->shutdownDispatcher();
	SceNpCommonPs4_2C0585AD(m_dispatchThread, NULL);
	SceNpCommonPs4_502309F4(m_dispatchThread);
	SceNpCommonPs4_6B428829(GetAllocator(), m_dispatchThread);
	m_dispatchThread = NULL;
	return 0;
}

int IpmiServer::DispatchLoop(void)
{
	int ret = m_ipmiServer->runDispatcher(m_workBuf, m_workBufSize);
	return (ret >= 0) ? 0 : ret;
}

IpmiSession *IpmiServer::FindClient(IPMI::Session *session) const
{
	for (SceUInt32 index = 0; index < m_maxClients; ++index) {
		if (m_clients[index] != NULL && m_clients[index]->GetIpmiSession() == session)
			return m_clients[index];
	}
	return NULL;
}

int IpmiServer::AddClient(IpmiSession *client)
{
	for (SceUInt32 index = 0; index < m_maxClients; ++index) {
		if (m_clients[index] == NULL) {
			m_clients[index] = client;
			m_clientCount++;
			return 0;
		}
	}
	return SCE_NP_IPC2_ERROR_CLIENT_MAX;
}

IpmiSession *IpmiServer::RemoveClient(IPMI::Session *session)
{
	for (SceUInt32 index = 0; index < m_maxClients; ++index) {
		if (m_clients[index] != NULL && m_clients[index]->GetIpmiSession() == session) {
			IpmiSession *found = m_clients[index];
			m_clients[index] = NULL;
			if (m_clientCount > 0)
				m_clientCount--;
			return found;
		}
	}
	return NULL;
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */
