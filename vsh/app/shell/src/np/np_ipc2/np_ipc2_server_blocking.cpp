/*
 * np_ipc2_server_blocking.cpp: Blocking call tracking and completion signaling
 * for NpIpc2 service requests.
 * Sony path: vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/np_ipc2_server_blocking.cpp
 * 3.65-CEX: 0x813B899C..0x813B9093
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

/* ============================================================================
 * BlockingServer (0x813B899C..0x813B8B0B)
 * ============================================================================ */

BlockingServer::BlockingServer()
	: m_server(NULL), m_serviceSession(NULL), m_maxOutstanding(0)
{
	SceNpCommonPs4_4266D870(&m_mutex);
}

BlockingServer::~BlockingServer()
{
	if (m_server != NULL)
		SceNpCommonPs4_63EED478(&m_mutex);
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

int BlockingServer::Init(ServiceServer *server, ServiceSession *session,
			 const BlockingServerConfig *config)
{
	NP_IPC2_ASSERT(m_server == NULL, "Init", "np_ipc2_server_blocking.cpp", 675, "!IsInit()");
	NP_IPC2_ASSERT(config->maxOutstanding > 0, "Init", "np_ipc2_server_blocking.cpp", 676,
		       "config->maxOutstanding > 0");

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpIpc2ServerBlockingCall", 0);
	if (ret < 0)
		return ret;

	SceUInt32 numEventFlags = (config->maxOutstanding >> 6) + 1U;
	if (numEventFlags >= 100U) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf(
			"Assertion in %s(%s:%d): %s(%s)\n", "Init",
			NP_IPC2_SOURCE_PATH("np_ipc2_server_blocking.cpp"), 686,
			"HALT_ASSERT", "EVENTFLAG_NUM(config->maxOutstanding) < 100"));
	}

	m_serviceSession = session;
	m_maxOutstanding = config->maxOutstanding;
	m_server = server;
	return 0;
}

/* ============================================================================
 * BlockingSession (0x813B8B0C..0x813B9093)
 * ============================================================================ */

BlockingSession::BlockingSession(BlockingServer *server, ServiceEntry *serviceEntry)
	: m_blockingServer(server),
	  m_serviceEntry(serviceEntry),
	  m_maxRequests(0),
	  m_requestPool(NULL),
	  m_requests(NULL),
	  m_handler(NULL)
{
}

BlockingSession::~BlockingSession()
{
	if (m_requests != NULL) {
		Free(m_requests);
		m_requests = NULL;
	}
	if (m_requestPool != NULL) {
		Free(m_requestPool);
		m_requestPool = NULL;
	}
}

void BlockingSession::ChangeState(BlockingRequest *request, BlockingRequestState newState)
{
	if (request->state == BLOCKING_REQ_FINISHED &&
	    newState != BLOCKING_REQ_DORMANT &&
	    newState != BLOCKING_REQ_FINISHED) {
		ServiceServer *server = m_serviceEntry->GetServer();
		sceClibPrintf(
			"[NpIpc2Server:%s(%d:%s)] Request(%d):<%d>: assertion failed. should not change state from FINISHED to another state<%d>\n",
			server->GetServerName(),
			m_serviceEntry->GetServiceType(),
			m_serviceEntry->GetServiceName(),
			request->requestId,
			static_cast<int>(request->state),
			static_cast<int>(newState));
		NP_IPC2_ASSERT(newState == BLOCKING_REQ_FINISHED ||
				       newState == BLOCKING_REQ_DORMANT,
			       "ChangeState", "np_ipc2_server_blocking.cpp", 124,
			       "state == Request::STATE_FINISHED || state == Request::STATE_DORMANT");
	}

	if (request->state != newState) {
		request->state = newState;
		if (newState == BLOCKING_REQ_DORMANT) {
			request->requestId = -1;
			request->bitIndex = -1;
		}
	}
}

int BlockingSession::Init(const BlockingSessionConfig *config)
{
	NP_IPC2_ASSERT(m_requests == NULL, "Init", "np_ipc2_server_blocking.cpp", 163, "!IsInit()");

	m_requestPool = static_cast<BlockingRequest *>(
		Alloc(sizeof(BlockingRequest) * config->maxRequests));
	if (m_requestPool == NULL)
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;

	m_requests = static_cast<BlockingRequest **>(
		Alloc(sizeof(BlockingRequest *) * config->maxRequests));
	if (m_requests == NULL) {
		Free(m_requestPool);
		m_requestPool = NULL;
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}

	for (SceUInt32 index = 0; index < config->maxRequests; ++index) {
		BlockingRequest *req = &m_requestPool[index];
		req->state = BLOCKING_REQ_DORMANT;
		req->requestId = -1;
		req->bitIndex = -1;
		req->owner = this;
		m_requests[index] = req;
	}

	m_maxRequests = config->maxRequests;
	m_handler = config->handler;
	return 0;
}

int BlockingSession::CreateRequest(IPMI::Session *session, int *outRequestId,
				   const void *optionBuf, SceSize optionSize, char flags)
{
	int createdId = -1;
	if ((flags & 1) != 0) {
		int ret = m_handler->CreateRequest(session, optionBuf, optionSize, &createdId);
		if (ret < 0)
			return ret;
		*outRequestId = createdId;
		return 0;
	}

	BlockingRequest *freeSlot = NULL;
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state == BLOCKING_REQ_DORMANT) {
			freeSlot = m_requests[index];
			break;
		}
	}
	if (freeSlot == NULL)
		return SCE_NP_IPC2_ERROR_REQUEST_MAX;

	int ret = m_handler->CreateRequest(session, optionBuf, optionSize, &createdId);
	if (ret < 0)
		return ret;
	if (createdId == -1)
		return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;

	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state != BLOCKING_REQ_DORMANT &&
		    m_requests[index]->requestId == createdId) {
			return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;
		}
	}

	freeSlot->requestId = createdId;
	ChangeState(freeSlot, BLOCKING_REQ_CREATED);
	*outRequestId = createdId;
	return 0;
}

int BlockingSession::DestroyRequest(IPMI::Session *session, int requestId, char flags)
{
	if ((flags & 1) != 0) {
		int ret = m_handler->DestroyRequest(session, requestId);
		return (ret >= 0) ? 0 : ret;
	}

	BlockingRequest *found = NULL;
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state != BLOCKING_REQ_DORMANT &&
		    m_requests[index]->requestId == requestId) {
			found = m_requests[index];
			break;
		}
	}
	if (found == NULL)
		return SCE_NP_IPC2_ERROR_REQUEST_NOT_FOUND;

	int ret = m_handler->DestroyRequest(session, requestId);
	if (ret >= 0) {
		ChangeState(found, BLOCKING_REQ_DORMANT);
		return 0;
	}
	return ret;
}

int BlockingSession::AbortRequest(IPMI::Session *session, int requestId, char flags)
{
	if ((flags & 1) != 0) {
		int ret = m_handler->AbortRequest(session, requestId);
		return (ret >= 0) ? 0 : ret;
	}

	BlockingRequest *found = NULL;
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state != BLOCKING_REQ_DORMANT &&
		    m_requests[index]->requestId == requestId) {
			found = m_requests[index];
			break;
		}
	}
	if (found == NULL)
		return SCE_NP_IPC2_ERROR_REQUEST_NOT_FOUND;

	if (found->state == BLOCKING_REQ_CREATED)
		return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;
	if (found->state == BLOCKING_REQ_ABORTING || found->state == BLOCKING_REQ_FINISHED)
		return 0;

	ChangeState(found, BLOCKING_REQ_ABORTING);
	int ret = m_handler->AbortRequest(session, found->requestId);
	return (ret >= 0) ? 0 : ret;
}

int BlockingSession::StartRequest(IPMI::Session *session, int requestId, int bitIndex)
{
	(void)session;
	BlockingRequest *found = NULL;
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state != BLOCKING_REQ_DORMANT &&
		    m_requests[index]->requestId == requestId) {
			found = m_requests[index];
			break;
		}
	}
	if (found == NULL)
		return SCE_NP_IPC2_ERROR_REQUEST_NOT_FOUND;
	if (found->state != BLOCKING_REQ_CREATED)
		return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;

	ChangeState(found, BLOCKING_REQ_STARTED);
	found->bitIndex = bitIndex;
	return 0;
}

int BlockingSession::FinishRequest(int requestId)
{
	BlockingRequest *found = NULL;
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		if (m_requests[index]->state != BLOCKING_REQ_DORMANT &&
		    m_requests[index]->requestId == requestId) {
			found = m_requests[index];
			break;
		}
	}
	if (found == NULL)
		return SCE_NP_IPC2_ERROR_REQUEST_NOT_FOUND;
	if (found->state != BLOCKING_REQ_STARTED && found->state != BLOCKING_REQ_ABORTING)
		return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;

	ChangeState(found, BLOCKING_REQ_FINISHED);
	IPMI::Session *session = m_blockingServer->GetServiceSession()->GetIpmiSession();
	int ret = session->setEventFlag(found->bitIndex / 32,
					1U << (found->bitIndex % 32));
	return (ret >= 0) ? 0 : ret;
}

void BlockingSession::AutoAbortAll(IPMI::Session *session)
{
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		BlockingRequest *req = m_requests[index];
		if (req->state == BLOCKING_REQ_STARTED) {
			ChangeState(req, BLOCKING_REQ_ABORTING);
			m_handler->AbortRequest(session, req->requestId);
		}
	}
}

void BlockingSession::AutoDeleteAll(IPMI::Session *session)
{
	for (SceUInt32 index = 0; index < m_maxRequests; ++index) {
		BlockingRequest *req = m_requests[index];
		if (req->state != BLOCKING_REQ_DORMANT) {
			if (m_handler->DestroyRequest(session, req->requestId) >= 0)
				ChangeState(req, BLOCKING_REQ_DORMANT);
		}
	}
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */
