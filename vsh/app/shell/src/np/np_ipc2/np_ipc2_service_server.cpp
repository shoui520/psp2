/*
 * np_ipc2_service_server.cpp: NpIpc2 ServiceServer, ServiceSession, ServiceBinding,
 * and ServiceEntry implementation handling service open/close, blocking requests,
 * event queues/flags, and method ID routing.
 * Sony path: vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/np_ipc2_service_server.cpp
 * 3.65-CEX: 0x813B6C04..0x813B8287
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

enum ServiceControlMethod {
	IPC2_CTRL_OPEN_SERVICE		= 0,
	IPC2_CTRL_CLOSE_SERVICE		= 1,
	IPC2_CTRL_CREATE_REQUEST	= 2,
	IPC2_CTRL_START_REQUEST		= 3,
	IPC2_CTRL_DESTROY_REQUEST	= 4,
	IPC2_CTRL_ABORT_REQUEST		= 5,
	IPC2_CTRL_DEQUEUE_EVENT		= 6
};

/* ============================================================================
 * ServiceEntry (0x813B7004, 0x813B806C..0x813B81DC)
 * ============================================================================ */

ServiceEntry::ServiceEntry() : m_server(NULL)
{
	sceClibMemset(&m_desc, 0, sizeof(m_desc));
}

ServiceEntry::~ServiceEntry()
{
	m_server = NULL;
}

int ServiceEntry::Init(ServiceServer *server, const ServiceDescriptor *desc)
{
	sceClibMemcpy_safe(&m_desc, desc, sizeof(m_desc));
	m_server = server;
	NP_IPC2_ASSERT(IsInit(), "Init", "np_ipc2_service_server.cpp", 59, "IsInit()");
	return 0;
}

SceInt32 ServiceEntry::GetServiceType(void) const
{
	NP_IPC2_ASSERT(IsInit(), "GetServiceType", "np_ipc2_service_server.cpp", 74, "IsInit()");
	return m_desc.serviceType;
}

bool ServiceEntry::IsServiceIdRange(SceUInt32 methodId) const
{
	NP_IPC2_ASSERT(IsInit(), "IsServiceIdRange", "np_ipc2_service_server.cpp", 82, "IsInit()");
	return methodId >= m_desc.minMethodId && methodId <= m_desc.maxMethodId;
}

const char *ServiceEntry::GetServiceName(void) const
{
	NP_IPC2_ASSERT(IsInit(), "GetServiceName", "np_ipc2_service_server.cpp", 96, "IsInit()");
	return m_desc.serviceName;
}

bool ServiceEntry::AutoDeleteRequest(void) const
{
	NP_IPC2_ASSERT(IsInit(), "AutoDeleteRequest", "np_ipc2_service_server.cpp", 104,
		       "IsInit()");
	return (m_desc.cleanupFlags & 2U) != 0;
}

bool ServiceEntry::AutoAbortRequest(void) const
{
	NP_IPC2_ASSERT(IsInit(), "AutoAbortRequest", "np_ipc2_service_server.cpp", 112, "IsInit()");
	return (m_desc.cleanupFlags & 1U) != 0;
}

void ServiceEntry::OnClientConnect(IPMI::Session *session)
{
	if (m_desc.onClientConnect != NULL)
		m_desc.onClientConnect(session);
}

void ServiceEntry::OnClientDisconnect(IPMI::Session *session)
{
	if (m_desc.onClientDisconnect != NULL)
		m_desc.onClientDisconnect(session);
}

void ServiceEntry::OnServiceOpen(IPMI::Session *session)
{
	if (m_desc.onServiceOpen != NULL)
		m_desc.onServiceOpen(session);
}

void ServiceEntry::OnServiceClose(IPMI::Session *session)
{
	if (m_desc.onServiceClose != NULL)
		m_desc.onServiceClose(session);
}

int ServiceEntry::OnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	if (m_desc.onSyncMethod != NULL)
		return m_desc.onSyncMethod(session, methodId, inList, inCount, outList, outCount);
	return SCE_NP_IPC2_ERROR_NOT_SUPPORTED;
}

int ServiceEntry::OnAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
				const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	if (m_desc.onAsyncMethod != NULL)
		return m_desc.onAsyncMethod(session, requestId, methodId, inList, inCount);
	return SCE_NP_IPC2_ERROR_NOT_SUPPORTED;
}

int ServiceEntry::CreateRequest(IPMI::Session *session, const void *optionBuf,
				SceSize optionSize, int *outRequestId)
{
	if (m_desc.onCreateRequest != NULL)
		return m_desc.onCreateRequest(session, optionBuf, optionSize, outRequestId);
	return SCE_NP_IPC2_ERROR_NOT_SUPPORTED;
}

int ServiceEntry::DestroyRequest(IPMI::Session *session, int requestId)
{
	if (m_desc.onDestroyRequest != NULL)
		return m_desc.onDestroyRequest(session, requestId);
	return SCE_NP_IPC2_ERROR_NOT_SUPPORTED;
}

int ServiceEntry::AbortRequest(IPMI::Session *session, int requestId)
{
	if (m_desc.onAbortRequest != NULL)
		return m_desc.onAbortRequest(session, requestId);
	return SCE_NP_IPC2_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * ServiceBinding (0x813B6E4E, 0x813B81DE..0x813B825B)
 * ============================================================================ */

ServiceBinding::ServiceBinding()
	: m_entry(NULL), m_blSvr(NULL), m_eqSvr(NULL), m_efSvr(NULL)
{
}

ServiceBinding::~ServiceBinding()
{
	if (m_blSvr != NULL) {
		m_blSvr->~BlockingSession();
		SceNpCommonPs4_6B428829(GetAllocator(), m_blSvr);
		m_blSvr = NULL;
	}
	if (m_eqSvr != NULL) {
		m_eqSvr->~EventQueueSession();
		SceNpCommonPs4_6B428829(GetAllocator(), m_eqSvr);
		m_eqSvr = NULL;
	}
	if (m_efSvr != NULL) {
		m_efSvr->~EventFlagSession();
		SceNpCommonPs4_6B428829(GetAllocator(), m_efSvr);
		m_efSvr = NULL;
	}
}

int ServiceBinding::Init(ServiceEntry *entry, ServiceSession *session)
{
	NP_IPC2_ASSERT(!IsInit(), "Init", "np_ipc2_service_server.cpp", 329, "!IsInit()");

	const ServiceDescriptor &desc = entry->GetDescriptor();
	int ret = 0;

	if (desc.maxBlockingRequests != 0) {
		void *rawBl = SceNpCommonPs4_0C922423(sizeof(BlockingSession), GetAllocator());
		if (rawBl == NULL)
			return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		m_blSvr = new (rawBl) BlockingSession(session->GetBlockingServer(), entry);

		BlockingSessionConfig blCfg;
		blCfg.maxRequests = desc.maxBlockingRequests;
		blCfg.handler = entry;
		ret = m_blSvr->Init(&blCfg);
	}

	if (ret >= 0 && desc.eventQueueItemSize != 0 && desc.eventQueueItemCount != 0) {
		void *rawEq = SceNpCommonPs4_0C922423(sizeof(EventQueueSession), GetAllocator());
		if (rawEq == NULL) {
			ret = SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			m_eqSvr = new (rawEq) EventQueueSession(session->GetEventQueueServer());
			EventQueueSessionConfig eqCfg;
			sceClibMemset(&eqCfg, 0, sizeof(eqCfg));
			eqCfg.numQueues = 1;
			eqCfg.queues[0].itemCount = desc.eventQueueItemCount;
			eqCfg.queues[0].itemSize = desc.eventQueueItemSize;
			ret = m_eqSvr->Init(desc.serviceName, session, &eqCfg);
		}
	}

	if (ret >= 0 && desc.numEventFlags != 0) {
		void *rawEf = SceNpCommonPs4_0C922423(sizeof(EventFlagSession), GetAllocator());
		if (rawEf == NULL) {
			ret = SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			m_efSvr = new (rawEf) EventFlagSession(session->GetEventFlagServer());
			EventFlagSessionConfig efCfg;
			efCfg.numEventFlags = desc.numEventFlags;
			ret = m_efSvr->Init(desc.serviceName, session, &efCfg);
		}
	}

	if (ret < 0) {
		if (m_efSvr != NULL) {
			m_efSvr->~EventFlagSession();
			SceNpCommonPs4_6B428829(GetAllocator(), m_efSvr);
			m_efSvr = NULL;
		}
		if (m_eqSvr != NULL) {
			m_eqSvr->~EventQueueSession();
			SceNpCommonPs4_6B428829(GetAllocator(), m_eqSvr);
			m_eqSvr = NULL;
		}
		if (m_blSvr != NULL) {
			m_blSvr->~BlockingSession();
			SceNpCommonPs4_6B428829(GetAllocator(), m_blSvr);
			m_blSvr = NULL;
		}
		return ret;
	}

	m_entry = entry;
	NP_IPC2_ASSERT(IsInit(), "Init", "np_ipc2_service_server.cpp", 418, "IsInit()");
	return 0;
}

/* ============================================================================
 * ServiceSession (0x813B6C04..0x813B766B)
 * ============================================================================ */

ServiceSession::ServiceSession()
	: m_serviceServer(NULL),
	  m_blockingServer(NULL),
	  m_eventQueueServer(NULL),
	  m_eventFlagServer(NULL)
{
	sceClibMemset(m_bindings, 0, sizeof(m_bindings));
}

ServiceSession::~ServiceSession()
{
	for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
		if (m_bindings[index] != NULL) {
			m_bindings[index]->~ServiceBinding();
			SceNpCommonPs4_6B428829(GetAllocator(), m_bindings[index]);
			m_bindings[index] = NULL;
		}
	}
	if (m_blockingServer != NULL) {
		m_blockingServer->~BlockingServer();
		SceNpCommonPs4_6B428829(GetAllocator(), m_blockingServer);
		m_blockingServer = NULL;
	}
	if (m_eventQueueServer != NULL) {
		m_eventQueueServer->~EventQueueServer();
		SceNpCommonPs4_6B428829(GetAllocator(), m_eventQueueServer);
		m_eventQueueServer = NULL;
	}
	if (m_eventFlagServer != NULL) {
		m_eventFlagServer->~EventFlagServer();
		SceNpCommonPs4_6B428829(GetAllocator(), m_eventFlagServer);
		m_eventFlagServer = NULL;
	}
}

int ServiceSession::Init(ServiceServer *server, const IPMI::Session::Config *sessionConfig,
			 const BlockingServerConfig *blockingConfig)
{
	NP_IPC2_ASSERT(m_serviceServer == NULL, "Init", "np_ipc2_service_server.cpp", 738,
		       "!IsInit()");

	int ret = IpmiSession::Init(server, sessionConfig);
	if (ret < 0)
		return ret;

	if (blockingConfig != NULL && blockingConfig->maxOutstanding != 0) {
		void *rawBl = SceNpCommonPs4_0C922423(sizeof(BlockingServer), GetAllocator());
		if (rawBl == NULL) {
			ret = SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			m_blockingServer = new (rawBl) BlockingServer();
			ret = m_blockingServer->Init(server, this, blockingConfig);
		}
	}

	if (ret >= 0) {
		void *rawEq = SceNpCommonPs4_0C922423(sizeof(EventQueueServer), GetAllocator());
		if (rawEq == NULL) {
			ret = SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			m_eventQueueServer = new (rawEq) EventQueueServer();
			ret = m_eventQueueServer->Init(server);
		}
	}

	if (ret >= 0) {
		void *rawEf = SceNpCommonPs4_0C922423(sizeof(EventFlagServer), GetAllocator());
		if (rawEf == NULL) {
			ret = SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			m_eventFlagServer = new (rawEf) EventFlagServer();
			ret = m_eventFlagServer->Init(server);
		}
	}

	if (ret < 0) {
		if (m_eventFlagServer != NULL) {
			m_eventFlagServer->~EventFlagServer();
			SceNpCommonPs4_6B428829(GetAllocator(), m_eventFlagServer);
			m_eventFlagServer = NULL;
		}
		if (m_eventQueueServer != NULL) {
			m_eventQueueServer->~EventQueueServer();
			SceNpCommonPs4_6B428829(GetAllocator(), m_eventQueueServer);
			m_eventQueueServer = NULL;
		}
		if (m_blockingServer != NULL) {
			m_blockingServer->~BlockingServer();
			SceNpCommonPs4_6B428829(GetAllocator(), m_blockingServer);
			m_blockingServer = NULL;
		}
		if (IsInit())
			IpmiSession::Finalize();
		m_serviceServer = NULL;
		return ret;
	}

	m_serviceServer = server;
	NP_IPC2_ASSERT(m_serviceServer != NULL, "Init", "np_ipc2_service_server.cpp", 804,
		       "IsInit()");
	return 0;
}

ServiceBinding *ServiceSession::FindService(SceInt32 serviceType) const
{
	for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
		ServiceBinding *binding = m_bindings[index];
		if (binding == NULL)
			continue;
		NP_IPC2_ASSERT(binding->IsInit(), "GetServiceType",
			       "np_ipc2_service_server.cpp", 447, "IsInit()");
		if (binding->GetEntry()->GetServiceType() == serviceType)
			return binding;
	}
	return NULL;
}

ServiceBinding *ServiceSession::RemoveService(SceInt32 serviceType)
{
	for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
		ServiceBinding *binding = m_bindings[index];
		if (binding == NULL)
			continue;
		NP_IPC2_ASSERT(binding->IsInit(), "GetServiceType",
			       "np_ipc2_service_server.cpp", 447, "IsInit()");
		if (binding->GetEntry()->GetServiceType() == serviceType) {
			m_bindings[index] = NULL;
			return binding;
		}
	}
	return NULL;
}

ServiceBinding *ServiceSession::FindServiceByMethodId(SceUInt32 methodId) const
{
	for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
		ServiceBinding *binding = m_bindings[index];
		if (binding == NULL)
			continue;
		NP_IPC2_ASSERT(binding->IsInit(), "IsServiceIdRange",
			       "np_ipc2_service_server.cpp", 471, "IsInit()");
		if (binding->GetEntry()->IsServiceIdRange(methodId))
			return binding;
	}
	return NULL;
}

int ServiceSession::OpenService(SceInt32 serviceType, IPMI::Session *session)
{
	if (FindService(serviceType) != NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_ALREADY_OPEN;

	ServiceEntry *entry = m_serviceServer->FindRegisteredService(serviceType);
	if (entry == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_AVAILABLE;

	void *rawBinding = SceNpCommonPs4_0C922423(sizeof(ServiceBinding), GetAllocator());
	if (rawBinding == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	ServiceBinding *binding = new (rawBinding) ServiceBinding();
	int ret = binding->Init(entry, this);
	if (ret >= 0) {
		int freeIndex = -1;
		for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
			if (m_bindings[index] == NULL) {
				freeIndex = static_cast<int>(index);
				break;
			}
		}
		if (freeIndex >= 0)
			m_bindings[freeIndex] = binding;
		else
			ret = SCE_NP_IPC2_ERROR_SERVICE_MAX;
	}

	if (ret < 0) {
		binding->~ServiceBinding();
		SceNpCommonPs4_6B428829(GetAllocator(), binding);
		return ret;
	}

	NpIpc2Mutex *lock = entry->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);
	entry->OnServiceOpen(session);
	SceNpCommonPs4_6BA5A164(lock);
	return 0;
}

void ServiceSession::CloseServiceBinding(ServiceBinding *binding, IPMI::Session *session)
{
	ServiceEntry *entry = binding->GetEntry();
	NpIpc2Mutex *lock = entry->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);

	BlockingSession *blSvr = binding->GetBlockingSession();
	if (blSvr != NULL) {
		if (entry->AutoAbortRequest())
			blSvr->AutoAbortAll(session);
		if (entry->AutoDeleteRequest())
			blSvr->AutoDeleteAll(session);
	}

	entry->OnServiceClose(session);
	SceNpCommonPs4_6BA5A164(lock);
}

void ServiceSession::CloseAllServices(IPMI::Session *session)
{
	for (SceUInt32 index = 0; index < NP_IPC2_MAX_SERVICES; ++index) {
		ServiceBinding *binding = m_bindings[index];
		if (binding != NULL) {
			CloseServiceBinding(binding, session);
			binding->~ServiceBinding();
			SceNpCommonPs4_6B428829(GetAllocator(), binding);
			m_bindings[index] = NULL;
		}
	}
}

void ServiceSession::OnDisconnect(IPMI::Session *session)
{
	CloseAllServices(session);
}

int ServiceSession::CreateRequest(SceInt32 serviceType, const void *optionBuf,
				  SceSize optionSize, char flags, int *outRequestId)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	BlockingSession *blSvr = binding->GetBlockingSession();
	NP_IPC2_ASSERT(blSvr != NULL, "CreateRequest", "np_ipc2_service_server.cpp", 549,
		       "m_blSvr");

	int ret = blSvr->CreateRequest(GetIpmiSession(), outRequestId, optionBuf, optionSize,
				       flags);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::DestroyRequest(SceInt32 serviceType, IPMI::Session *session,
				   int requestId, char flags)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	NpIpc2Mutex *lock = binding->GetEntry()->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);
	BlockingSession *blSvr = binding->GetBlockingSession();
	NP_IPC2_ASSERT(blSvr != NULL, "DestroyRequest", "np_ipc2_service_server.cpp", 562,
		       "m_blSvr");
	int ret = blSvr->DestroyRequest(session, requestId, flags);
	SceNpCommonPs4_6BA5A164(lock);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::AbortRequest(SceInt32 serviceType, IPMI::Session *session,
				 int requestId, char flags)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	NpIpc2Mutex *lock = binding->GetEntry()->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);
	BlockingSession *blSvr = binding->GetBlockingSession();
	NP_IPC2_ASSERT(blSvr != NULL, "AbortRequest", "np_ipc2_service_server.cpp", 575,
		       "m_blSvr");
	int ret = blSvr->AbortRequest(session, requestId, flags);
	SceNpCommonPs4_6BA5A164(lock);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::StartRequest(SceInt32 serviceType, int requestId, int bitIndex)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	NpIpc2Mutex *lock = binding->GetEntry()->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);
	BlockingSession *blSvr = binding->GetBlockingSession();
	NP_IPC2_ASSERT(blSvr != NULL, "StartRequest", "np_ipc2_service_server.cpp", 588,
		       "m_blSvr");
	int ret = blSvr->StartRequest(GetIpmiSession(), requestId, bitIndex);
	SceNpCommonPs4_6BA5A164(lock);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::FinishRequest(ServiceBinding *binding, int requestId)
{
	NpIpc2Mutex *lock = binding->GetEntry()->GetServer()->GetLock();
	SceNpCommonPs4_A2A8493B(lock);
	BlockingSession *blSvr = binding->GetBlockingSession();
	NP_IPC2_ASSERT(blSvr != NULL, "FinishRequest", "np_ipc2_service_server.cpp", 598,
		       "m_blSvr");
	int ret = blSvr->FinishRequest(requestId);
	SceNpCommonPs4_6BA5A164(lock);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::DequeueEvent(SceInt32 serviceType, void *outBuf, SceSize size)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	EventQueueSession *eqSvr = binding->GetEventQueueSession();
	NP_IPC2_ASSERT(eqSvr != NULL, "DequeueEvent", "np_ipc2_service_server.cpp", 616,
		       "m_eqSvr");
	int ret = eqSvr->Dequeue(0, outBuf, size);
	return (ret >= 0) ? 0 : ret;
}

int ServiceSession::SetEventFlag(SceInt32 serviceType, SceUInt32 evfIndex, SceUInt64 bitPattern)
{
	ServiceBinding *binding = FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	EventFlagSession *efSvr = binding->GetEventFlagSession();
	NP_IPC2_ASSERT(efSvr != NULL, "SetEventFlag", "np_ipc2_service_server.cpp", 649,
		       "m_efSvr");
	int ret = efSvr->SetEventFlag(evfIndex, bitPattern);
	return (ret >= 0) ? 0 : ret;
}

/* ============================================================================
 * ServiceServer (0x813B766C..0x813B8069)
 * ============================================================================ */

ServiceServer::ServiceServer()
	: m_serviceCount(0), m_maxBlockingRequests(0)
{
	sceClibMemset(m_services, 0, sizeof(m_services));
}

ServiceServer::~ServiceServer()
{
	ClearServices();
}

void ServiceServer::ClearServices(void)
{
	for (SceUInt32 index = 0; index < m_serviceCount; ++index) {
		if (m_services[index] != NULL) {
			m_services[index]->~ServiceEntry();
			SceNpCommonPs4_6B428829(GetAllocator(), m_services[index]);
			m_services[index] = NULL;
		}
	}
	m_serviceCount = 0;
}

int ServiceServer::Init(const IpmiServerConfig *config)
{
	NP_IPC2_ASSERT(!IsInit(), "Init", "np_ipc2_service_server.cpp", 1277, "!IsInit()");

	IpmiServerConfig adjusted;
	sceClibMemset(&adjusted, 0, sizeof(adjusted));
	sceClibStrlcpy(adjusted.serverName, config->serverName, sizeof(adjusted.serverName));
	adjusted.maxClients = config->maxClients + config->maxBlockingRequests;
	adjusted.maxSyncRequestSize = config->maxSyncRequestSize;
	adjusted.maxSyncResponseSize = config->maxSyncResponseSize;
	adjusted.maxAsyncRequestSize = config->maxAsyncRequestSize;
	adjusted.maxAsyncResponseSize = config->maxAsyncResponseSize;
	adjusted.maxEventDataSize = config->maxEventDataSize;
	adjusted.maxBlockingRequests = config->maxBlockingRequests;

	int ret = IpmiServer::Init(&adjusted);
	if (ret < 0)
		return ret;

	m_maxBlockingRequests = config->maxBlockingRequests;
	sceClibMemset(m_services, 0, sizeof(m_services));
	m_serviceCount = 0;
	NP_IPC2_ASSERT(IsInit(), "Init", "np_ipc2_service_server.cpp", 1300, "IsInit()");
	return 0;
}

void ServiceServer::Destroy(void)
{
	ClearServices();
	IpmiServer::Destroy();
	sceClibMemset(m_services, 0, sizeof(m_services));
	NP_IPC2_ASSERT(!IsInit(), "Destroy", "np_ipc2_service_server.cpp", 1311, "!IsInit()");
}

ServiceEntry *ServiceServer::FindRegisteredService(SceInt32 serviceType) const
{
	for (SceUInt32 index = 0; index < m_serviceCount; ++index) {
		if (m_services[index] != NULL &&
		    m_services[index]->GetServiceType() == serviceType) {
			return m_services[index];
		}
	}
	return NULL;
}

int ServiceServer::RegisterService(const ServiceDescriptor *desc)
{
	if (FindRegisteredService(desc->serviceType) != NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_ALREADY_REGISTERED;
	if (m_serviceCount >= NP_IPC2_MAX_SERVICES)
		return SCE_NP_IPC2_ERROR_SERVICE_MAX;

	void *rawEntry = SceNpCommonPs4_0C922423(sizeof(ServiceEntry), GetAllocator());
	if (rawEntry == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	ServiceEntry *entry = new (rawEntry) ServiceEntry();
	entry->Init(this, desc);
	m_services[m_serviceCount++] = entry;
	return 0;
}

int ServiceServer::UnregisterService(SceInt32 serviceType)
{
	for (SceUInt32 index = 0; index < m_serviceCount; ++index) {
		if (m_services[index]->GetServiceType() == serviceType) {
			ServiceEntry *entry = m_services[index];
			entry->~ServiceEntry();
			SceNpCommonPs4_6B428829(GetAllocator(), entry);
			if (m_serviceCount - index > 1) {
				sceClibMemmove(&m_services[index], &m_services[index + 1],
					       sizeof(ServiceEntry *) * (m_serviceCount - index - 1));
			}
			m_serviceCount--;
			m_services[m_serviceCount] = NULL;
			return 0;
		}
	}
	return SCE_NP_IPC2_ERROR_SERVICE_NOT_REGISTERED;
}

int ServiceServer::RegisterServices(const ServiceDescriptor *descTable)
{
	int registeredCount = 0;
	for (const ServiceDescriptor *cur = descTable; cur->serviceType != -1; ++cur) {
		int ret = RegisterService(cur);
		if (ret < 0) {
			for (int rev = 0; rev < registeredCount; ++rev)
				UnregisterService(descTable[rev].serviceType);
			return ret;
		}
		registeredCount++;
	}
	return 0;
}

int ServiceServer::FinishRequest(IPMI::Session *session, SceInt32 serviceType, int requestId)
{
	ServiceSession *client = static_cast<ServiceSession *>(FindClient(session));
	if (client == NULL)
		return SCE_NP_IPC2_ERROR_CLIENT_NOT_FOUND;

	ServiceBinding *binding = client->FindService(serviceType);
	if (binding == NULL)
		return SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN;

	return client->FinishRequest(binding, requestId);
}

int ServiceServer::SetEventFlag(IPMI::Session *session, SceInt32 serviceType,
				SceUInt32 evfIndex, SceUInt64 bitPattern)
{
	ServiceSession *client = static_cast<ServiceSession *>(FindClient(session));
	if (client == NULL)
		return SCE_NP_IPC2_ERROR_CLIENT_NOT_FOUND;

	return client->SetEventFlag(serviceType, evfIndex, bitPattern);
}

int ServiceServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
			       const void *pParam, SceSize paramSize)
{
	(void)pParam;
	(void)paramSize;

	NP_IPC2_ASSERT(m_ipmiServer == pServer, "onNewClient", "np_ipc2_service_server.cpp",
		       1442, "m_ipmiServer == server");

	if (m_clientCount >= m_maxClients)
		return SCE_NP_IPC2_ERROR_CLIENT_MAX;

	void *rawClient = SceNpCommonPs4_0C922423(sizeof(ServiceSession), GetAllocator());
	if (rawClient == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	ServiceSession *client = new (rawClient) ServiceSession();
	BlockingServerConfig blCfg;
	blCfg.maxOutstanding = m_maxBlockingRequests;

	int ret = client->Init(this, pConfig, &blCfg);
	if (ret >= 0)
		ret = AddClient(client);
	if (ret < 0) {
		client->~ServiceSession();
		SceNpCommonPs4_6B428829(GetAllocator(), client);
		return ret;
	}

	for (SceUInt32 index = 0; index < m_serviceCount; ++index)
		m_services[index]->OnClientConnect(client->GetIpmiSession());
	return 0;
}

void ServiceServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	IpmiSession *client = RemoveClient(pSession);
	if (client == NULL)
		return;

	client->OnDisconnect(pSession);
	for (SceUInt32 index = 0; index < m_serviceCount; ++index)
		m_services[index]->OnClientDisconnect(pSession);

	client->~IpmiSession();
	SceNpCommonPs4_6B428829(GetAllocator(), client);
}

void ServiceServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					 void *pIn, SceSize inSize, SceUInt32 outCount,
					 void *pOut, SceSize outSize)
{
	(void)outCount;
	(void)pOut;
	(void)outSize;

	ServiceSession *client = static_cast<ServiceSession *>(FindClient(pSession));
	NP_IPC2_ASSERT(client != NULL, "onSyncMethodDispatch", "np_ipc2_service_server.cpp",
		       1540, "context != 0");

	const void *emptyOut = NULL;
	const int *inWords = static_cast<const int *>(pIn);
	if (inWords == NULL) {
		pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
						     emptyOut, 0);
		return;
	}

	switch (methodId) {
	case IPC2_CTRL_OPEN_SERVICE: {
		if (inSize < 4) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		int ret = client->OpenService(inWords[0], pSession);
		pSession->respondToSyncMethodRequest(ret, emptyOut, 0);
		return;
	}
	case IPC2_CTRL_CLOSE_SERVICE: {
		if (inSize < 4) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		ServiceBinding *binding = client->RemoveService(inWords[0]);
		if (binding != NULL) {
			client->CloseServiceBinding(binding, pSession);
			binding->~ServiceBinding();
			SceNpCommonPs4_6B428829(GetAllocator(), binding);
			pSession->respondToSyncMethodRequest(0, emptyOut, 0);
		} else {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN,
							     emptyOut, 0);
		}
		return;
	}
	case IPC2_CTRL_CREATE_REQUEST: {
		if (inSize < 0x8C) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		int reqId = 0;
		int ret = client->CreateRequest(inWords[0], &inWords[2],
						static_cast<SceSize>(inWords[34]),
						static_cast<char>(inWords[1]), &reqId);
		pSession->respondToSyncMethodRequest((ret == 0) ? reqId : ret, emptyOut, 0);
		return;
	}
	case IPC2_CTRL_START_REQUEST: {
		if (inSize < 0x0C) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		int ret = client->StartRequest(inWords[0], inWords[1], inWords[2]);
		pSession->respondToSyncMethodRequest(ret, emptyOut, 0);
		return;
	}
	case IPC2_CTRL_DESTROY_REQUEST: {
		if (inSize < 0x0C) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		int ret = client->DestroyRequest(inWords[0], pSession, inWords[2],
						 static_cast<char>(inWords[1]));
		pSession->respondToSyncMethodRequest(ret, emptyOut, 0);
		return;
	}
	case IPC2_CTRL_ABORT_REQUEST: {
		if (inSize < 0x0C) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		int ret = client->AbortRequest(inWords[0], pSession, inWords[2],
					       static_cast<char>(inWords[1]));
		pSession->respondToSyncMethodRequest(ret, emptyOut, 0);
		return;
	}
	case IPC2_CTRL_DEQUEUE_EVENT: {
		if (inSize < 8) {
			pSession->respondToSyncMethodRequest(SCE_NP_IPC2_ERROR_INVALID_ARGUMENT,
							     emptyOut, 0);
			return;
		}
		unsigned char eventBuf[NP_IPC2_MAX_EVENT_SIZE];
		SceUInt32 reqSize = static_cast<SceUInt32>(inWords[1]);
		int ret = SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;
		if (reqSize < NP_IPC2_MAX_EVENT_SIZE)
			ret = client->DequeueEvent(inWords[0], eventBuf, reqSize);
		pSession->respondToSyncMethodRequest(ret, static_cast<const void *>(eventBuf),
						     reqSize);
		return;
	}
	default:
		return;
	}
}

void ServiceServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					 const IPMI::DataInfo *pInList, SceUInt32 inCount,
					 IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	ServiceSession *client = static_cast<ServiceSession *>(FindClient(pSession));
	if (client == NULL)
		return;

	ServiceBinding *binding = client->FindServiceByMethodId(methodId);
	if (binding == NULL)
		return;

	binding->GetEntry()->OnSyncMethod(client->GetIpmiSession(), methodId,
					  pInList, inCount, pOutList, outCount);
}

void ServiceServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					  SceUInt32 methodId, const IPMI::DataInfo *pInList,
					  SceUInt32 inCount)
{
	ServiceSession *client = static_cast<ServiceSession *>(FindClient(pSession));
	if (client == NULL)
		return;

	ServiceBinding *binding = client->FindServiceByMethodId(methodId);
	if (binding == NULL)
		return;

	binding->GetEntry()->OnAsyncMethod(client->GetIpmiSession(), requestId, methodId,
					   pInList, inCount);
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */
