/*
 * np_ipc2_server_eventflag.cpp: Per-client event flag array ("%sEvf%02d%08x")
 * for NpIpc2 services.
 * Sony path: vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/np_ipc2_server_eventflag.cpp
 * 3.65-CEX: 0x813B9094..0x813B939F
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

EventFlagServer::EventFlagServer() : m_server(NULL) {}

EventFlagServer::~EventFlagServer()
{
	m_server = NULL;
}

int EventFlagServer::Init(ServiceServer *server)
{
	m_server = server;
	return 0;
}

EventFlagSession::EventFlagSession(EventFlagServer *server)
	: m_server(server), m_numEventFlags(0), m_efPool(NULL), m_efList(NULL)
{
}

EventFlagSession::~EventFlagSession()
{
	if (m_efList != NULL) {
		for (SceUInt32 index = 0; index < m_numEventFlags; ++index) {
			if (m_efList[index] != NULL) {
				SceNpCommonPs4_E7D29E99(m_efList[index]);
				SceNpCommonPs4_4C3E58CA(m_efList[index]);
			}
		}
		Free(m_efList);
		m_efList = NULL;
	}
	if (m_efPool != NULL) {
		Free(m_efPool);
		m_efPool = NULL;
	}
}

int EventFlagSession::Init(const char *serviceName, ServiceSession *session,
			   const EventFlagSessionConfig *config)
{
	NP_IPC2_ASSERT(m_efList == NULL, "Init", "np_ipc2_server_eventflag.cpp", 36, "!IsInit()");

	if (config->numEventFlags > NP_IPC2_MAX_EVENT_FLAGS)
		return SCE_NP_IPC2_ERROR_INVALID_CONFIG;

	m_efPool = static_cast<NpIpc2EventFlagObj *>(
		Alloc(sizeof(NpIpc2EventFlagObj) * config->numEventFlags));
	if (m_efPool == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	m_efList = static_cast<NpIpc2EventFlagObj **>(
		Alloc(sizeof(NpIpc2EventFlagObj *) * config->numEventFlags));
	if (m_efList == NULL) {
		Free(m_efPool);
		m_efPool = NULL;
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}

	for (SceUInt32 index = 0; index < config->numEventFlags; ++index) {
		NpIpc2EventFlagObj *ef = SceNpCommonPs4_945AC0FC(&m_efPool[index]);
		m_efList[index] = ef;
	}
	m_numEventFlags = config->numEventFlags;

	SceUID clientPid = session->GetIpmiSession()->getProcessId();
	int ret = 0;
	for (SceUInt32 index = 0; index < config->numEventFlags; ++index) {
		char efName[32];
		sceClibSnprintf(efName, sizeof(efName), "%sEvf%02d%08x", serviceName,
				static_cast<int>(index), static_cast<unsigned int>(clientPid));
		ret = SceNpCommonPs4_909FBD24(m_efList[index], efName);
		if (ret < 0)
			break;
	}

	if (ret < 0) {
		for (SceUInt32 index = 0; index < m_numEventFlags; ++index) {
			SceNpCommonPs4_E7D29E99(m_efList[index]);
			SceNpCommonPs4_4C3E58CA(m_efList[index]);
		}
		Free(m_efList);
		m_efList = NULL;
		Free(m_efPool);
		m_efPool = NULL;
		m_numEventFlags = 0;
		return ret;
	}

	return 0;
}

int EventFlagSession::SetEventFlag(SceUInt32 evfIndex, SceUInt64 bitPattern)
{
	NP_IPC2_ASSERT(m_efList != NULL, "SetEventFlag", "np_ipc2_server_eventflag.cpp", 116,
		       "IsInit()");
	NP_IPC2_ASSERT(evfIndex < m_numEventFlags, "SetEventFlag",
		       "np_ipc2_server_eventflag.cpp", 117, "evfIndex < m_numEventFlags");

	return SceNpCommonPs4_0F1ED1EB(m_efList[evfIndex], 0,
				       static_cast<SceUInt32>(bitPattern));
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */
