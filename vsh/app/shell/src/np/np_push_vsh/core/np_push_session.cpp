/*
 * np_push_session.cpp: Push session thread, receive thread, ASM + GetServerAddrTrans
 * connection sequence, active request tracking, and kickout/signout filtering.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_session.cpp
 * 3.65-CEX: 0x813AFDC2..0x813B17F3, 0x813B555C..0x813B57A9
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static const char kPushAsmServiceName[] = "pushNotification";
static const char kKickoutDataType[] = "np:service:pushnotification:kickout";
static const char kSignoutDataType[] = "np:service:pushnotification:signout";

/* 0x813AFDC2 */
Session::Session()
	: m_sessionId(0),
	  m_state(SESSION_STATE_INIT),
	  m_handler(NULL),
	  m_sessionThread(NULL),
	  m_receiveThread(NULL),
	  m_sessionThreadRunning(false),
	  m_receiveThreadRunning(false),
	  m_recvQueue(&m_mutex),
	  m_isReconnect(false),
	  m_retryIntervalSec(NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC),
	  m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	SceNpCommonPs4_2E2B2655(&m_cancelHandle);
	SceNpCommonPs4_B0BAB9BE(&m_connectOnlineId);
}

/* 0x813AFF20 */
Session::~Session()
{
	NP_PUSH_ASSERT(!m_initialized, "~Session",
		       "core\\np_push_session.cpp", 96, "!m_initialized");

	SceNpCommonPs4_1543D7EE(&m_connectOnlineId);
	SceNpCommonPs4_62214062(&m_cancelHandle);
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813B0050: Init */
int Session::Init(void)
{
	if (m_initialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushSession", 1);
	if (ret < 0)
		return ret;

	ret = m_cmdQueue.Init();
	if (ret < 0) {
		SceNpCommonPs4_63EED478(&m_mutex);
		return ret;
	}

	ret = SceNpCommonPs4_D86527FB(&m_cancelHandle);
	if (ret < 0) {
		m_cmdQueue.Finalize();
		SceNpCommonPs4_63EED478(&m_mutex);
		return ret;
	}

	m_handlers.SetAllocator(GetAllocator());
	m_activeRequests.SetAllocator(GetAllocator());
	m_state = SESSION_STATE_IDLE;
	m_initialized = true;
	return 0;
}

/* 0x813B0180: Finalize */
void Session::Finalize(void)
{
	NP_PUSH_ASSERT(m_initialized, "Finalize",
		       "core\\np_push_session.cpp", 156, "m_initialized");

	Stop();
	ErrorRequestAll(SCE_NP_PUSH_ERROR_ABORTED);
	SceNpCommonPs4_FC2480E5(&m_cancelHandle);
	m_cmdQueue.Finalize();
	SceNpCommonPs4_63EED478(&m_mutex);
	m_state = SESSION_STATE_INIT;
	m_initialized = false;
}

/* 0x813B0268: RegisterHandler */
int Session::RegisterHandler(Handler *handler)
{
	NP_PUSH_ASSERT(m_initialized, "RegisterHandler",
		       "core\\np_push_session.cpp", 184, "m_initialized");

	ScopedLock lock(&m_mutex);
	m_handler = handler;
	if (handler != NULL)
		m_handlers.PushBack(handler);
	return 0;
}

/* 0x813B02E0: Start */
int Session::Start(void)
{
	NP_PUSH_ASSERT(m_initialized, "Start",
		       "core\\np_push_session.cpp", 204, "m_initialized");

	if (m_sessionThreadRunning)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	void *rawThread = SceNpCommonPs4_0C922423(sizeof(SessionThread), GetAllocator());
	if (rawThread == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	m_sessionThread = new (rawThread) SessionThread(this);
	int ret = m_sessionThread->Start();
	if (ret >= 0)
		ret = SceNpCommonPs4_8CBDFDB7(m_sessionThread->AsThreadBase());
	if (ret < 0) {
		m_sessionThread->~SessionThread();
		SceNpCommonPs4_6B428829(GetAllocator(), m_sessionThread);
		m_sessionThread = NULL;
		return ret;
	}

	m_sessionThreadRunning = true;
	return 0;
}

/* 0x813B03E0: Stop */
int Session::Stop(void)
{
	if (!m_sessionThreadRunning)
		return 0;

	SceNpCommonPs4_6B8D9291(&m_cancelHandle);

	SessionCmd stopCmd(SESSION_CMD_STOP);
	m_cmdQueue.Push(&stopCmd);
	m_cmdQueue.Abort();

	StopSessionThread();
	m_sessionThreadRunning = false;
	return 0;
}

/* 0x813B04B0: Connect */
int Session::Connect(const NpPushOnlineId *onlineId, bool isReconnect)
{
	if (!m_initialized || onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	{
		ScopedLock lock(&m_mutex);
		SceNpCommonPs4_AEC0DAC5(&m_connectOnlineId, onlineId);
		m_isReconnect = isReconnect;
	}

	SessionCmd cmd(SESSION_CMD_CONNECT);
	return m_cmdQueue.Push(&cmd);
}

/* 0x813B0550: Disconnect */
int Session::Disconnect(void)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	SceNpCommonPs4_6B8D9291(&m_cancelHandle);

	SessionCmd cmd(SESSION_CMD_DISCONNECT);
	return m_cmdQueue.Push(&cmd);
}

/* 0x813B05E0: SendRequest */
int Session::SendRequest(const RefPtr<Request> &request)
{
	if (!m_initialized || request.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	Request *rawReq = request.Get();
	SceNpCommonPs4_64BAC888(rawReq);

	SessionCmd cmd(SESSION_CMD_SEND_REQUEST);
	cmd.payload = rawReq;
	int ret = m_cmdQueue.Push(&cmd);
	if (ret < 0)
		SceNpCommonPs4_93C97239(rawReq);
	return ret;
}

/* 0x813B06B0: OpenConnection */
int Session::OpenConnection(const NpPushOnlineId *onlineId, bool isReconnect,
			    ConnectionResponseInfo *outRespInfo,
			    SceUInt32 *outRetryIntervalSec)
{
	NP_PUSH_ASSERT(onlineId != NULL, "OpenConnection",
		       "core\\np_push_session.cpp", 342, "onlineId != NULL");

	NpPushStringHolder baseUrlHolder;
	SceNpCommonPs4_FDA863C0(&baseUrlHolder, GetAllocatorWithError());

	int ret = npAsmGetServiceBaseUrl(&m_cancelHandle, GetNpHttpClient(),
					 onlineId->text, kPushAsmServiceName, &baseUrlHolder);
	if (ret < 0) {
		SceNpCommonPs4_483168C0(&baseUrlHolder);
		return ret;
	}

	const char *baseUrl = SceNpCommonPs4_2CE76BE5(&baseUrlHolder);
	ServerAddr serverAddr;
	GetServerAddrTrans serverAddrTrans(GetAllocator(), onlineId);
	ret = serverAddrTrans.Execute(&m_cancelHandle, GetNpHttpClient(), baseUrl,
				      &serverAddr, outRetryIntervalSec);
	SceNpCommonPs4_483168C0(&baseUrlHolder);
	if (ret < 0)
		return ret;

	ret = m_connection.Connect(&m_cancelHandle, serverAddr.GetFqdn(), onlineId,
				   isReconnect, outRespInfo);
	if (ret < 0)
		return ret;

	return StartReceiveThread();
}

/* 0x813B0910: ShutdownConnection */
void Session::ShutdownConnection(void)
{
	NP_PUSH_ASSERT(m_initialized, "ShutdownConnection",
		       "core\\np_push_session.cpp", 418, "m_initialized");

	if (m_connection.IsConnected())
		m_connection.SendClose();

	m_connection.Disconnect();
	StopReceiveThread();
}

/* 0x813B0A10: StartReceiveThread */
int Session::StartReceiveThread(void)
{
	if (m_receiveThreadRunning)
		return 0;

	void *rawThread = SceNpCommonPs4_0C922423(sizeof(ReceiveThread), GetAllocator());
	if (rawThread == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	m_receiveThread = new (rawThread) ReceiveThread(this);
	int ret = m_receiveThread->Start();
	if (ret >= 0)
		ret = SceNpCommonPs4_8CBDFDB7(m_receiveThread->AsThreadBase());
	if (ret < 0) {
		m_receiveThread->~ReceiveThread();
		SceNpCommonPs4_6B428829(GetAllocator(), m_receiveThread);
		m_receiveThread = NULL;
		return ret;
	}

	m_receiveThreadRunning = true;
	return 0;
}

/* 0x813B0AD0: StopReceiveThread */
void Session::StopReceiveThread(void)
{
	if (!m_receiveThreadRunning)
		return;

	StopReceiveThreadAndConnection();
	m_receiveThreadRunning = false;
}

/* 0x813B0B20: ReceiveMain */
void Session::ReceiveMain(void)
{
	NP_PUSH_ASSERT(m_initialized, "ReceiveMain",
		       "core\\np_push_session.cpp", 478, "m_initialized");

	while (m_connection.IsConnected()) {
		RefPtr<Packet> packet;
		int ret = m_connection.ReceivePacket(&packet);
		if (ret < 0) {
			SessionCmd errCmd(SESSION_CMD_RECV_ERROR);
			errCmd.errorCode = ret;
			m_cmdQueue.Push(&errCmd);
			break;
		}

		if (!packet.IsNull()) {
			Packet *rawPacket = packet.Get();
			SceNpCommonPs4_64BAC888(rawPacket);
			SessionCmd pktCmd(SESSION_CMD_RECV_PACKET);
			pktCmd.payload = rawPacket;
			if (m_cmdQueue.Push(&pktCmd) < 0)
				SceNpCommonPs4_93C97239(rawPacket);
		}
	}
}

/* 0x813B0D40: DispatchReceivedPacket */
void Session::DispatchReceivedPacket(const RefPtr<Packet> &packet)
{
	if (packet.IsNull())
		return;

	/* Check if this packet is a response matching an in-flight Request by tranId */
	if (packet->HasTranId()) {
		char tranId[32];
		sceClibMemset(tranId, 0, sizeof(tranId));
		if (packet->GetTranId(tranId, sizeof(tranId)) >= 0) {
			for (ListNode<RefPtr<Request> > *iter = m_activeRequests.Begin();
			     iter != m_activeRequests.End(); iter = iter->next) {
				RefPtr<Request> req = iter->value;
				if (sceClibStrncmp(req->GetTranId(), tranId, sizeof(tranId)) == 0) {
					m_activeRequests.Erase(iter);
					if (req->GetType() == REQUEST_TYPE_QUERY)
						req->SetResultReceivedQueryResponse(packet);
					else
						req->SetResultReceivedReqTransResponse(packet);
					return;
				}
			}
		}
	}

	/* Check for server kickout / signout push control notifications */
	if (packet->HasDataType()) {
		if (packet->MatchDataTypeExact(kKickoutDataType)) {
			ShutdownConnection();
			ErrorRequestAll(SCE_NP_PUSH_ERROR_KICKED_OUT);
			m_state = SESSION_STATE_IDLE;
			if (m_handler != NULL)
				m_handler->OnSessionDisconnected(this, SCE_NP_PUSH_ERROR_KICKED_OUT);
			return;
		}
		if (packet->MatchDataTypeExact(kSignoutDataType)) {
			ShutdownConnection();
			ErrorRequestAll(SCE_NP_PUSH_ERROR_SIGNED_OUT);
			m_state = SESSION_STATE_IDLE;
			if (m_handler != NULL)
				m_handler->OnSessionDisconnected(this, SCE_NP_PUSH_ERROR_SIGNED_OUT);
			return;
		}
	}

	if (m_handler != NULL)
		m_handler->OnSessionReceivedPacket(this, packet);
}

/* 0x813B1080: ErrorRequestAll */
void Session::ErrorRequestAll(int errorCode)
{
	NP_PUSH_ASSERT(errorCode < 0, "ErrorRequestAll",
		       "core\\np_push_session.cpp", 604, "errorCode < 0");

	while (!m_activeRequests.Empty()) {
		RefPtr<Request> req = m_activeRequests.Begin()->value;
		m_activeRequests.Erase(m_activeRequests.Begin());
		req->SetResultError(errorCode, 0);
	}
}

/* 0x813B1200: ThreadMain */
void Session::ThreadMain(void)
{
	typedef int (Session::*StateHandler)(const SessionCmd *);
	static const StateHandler kStateTable[] = {
		&Session::StateInvalid,		/* SESSION_STATE_INIT */
		&Session::StateIdle,		/* SESSION_STATE_IDLE */
		&Session::StateConnecting,	/* SESSION_STATE_CONNECTING */
		&Session::StateEstablished,	/* SESSION_STATE_ESTABLISHED */
		&Session::StateDisconnecting,	/* SESSION_STATE_DISCONNECTING */
		&Session::StateInvalid,		/* SESSION_STATE_STOPPING */
		&Session::StateInvalid		/* SESSION_STATE_ERROR */
	};

	while (m_state != SESSION_STATE_STOPPING) {
		SessionCmd cmd;
		int ret = m_cmdQueue.Pop(&cmd, 0);
		if (ret < 0)
			break;

		if (cmd.type == SESSION_CMD_STOP) {
			ShutdownConnection();
			ErrorRequestAll(SCE_NP_PUSH_ERROR_ABORTED);
			m_state = SESSION_STATE_STOPPING;
			break;
		}

		int stateIndex = static_cast<int>(m_state);
		if (stateIndex < 0 || stateIndex >= 7)
			StateInvalid(&cmd);
		else
			(this->*kStateTable[stateIndex])(&cmd);
	}
}

/* 0x813B1540: StateIdle */
int Session::StateIdle(const SessionCmd *cmd)
{
	if (cmd->type == SESSION_CMD_CONNECT) {
		m_state = SESSION_STATE_CONNECTING;
		return StateConnecting(cmd);
	}
	if (cmd->type == SESSION_CMD_SEND_REQUEST && cmd->payload != NULL) {
		RefPtr<Request> req;
		req.Adopt(static_cast<Request *>(cmd->payload));
		req->SetResultError(SCE_NP_PUSH_ERROR_NOT_CONNECTED, 0);
		return 0;
	}
	if (cmd->type == SESSION_CMD_RECV_PACKET && cmd->payload != NULL) {
		RefPtr<Packet> pkt;
		pkt.Adopt(static_cast<Packet *>(cmd->payload));
	}
	return 0;
}

/* 0x813B15B0: StateConnecting */
int Session::StateConnecting(const SessionCmd *cmd)
{
	NP_PUSH_ASSERT(m_state == SESSION_STATE_CONNECTING, "StateConnecting",
		       "core\\np_push_session.cpp", 718, "m_state == SESSION_STATE_CONNECTING");

	if (cmd->type == SESSION_CMD_DISCONNECT) {
		ShutdownConnection();
		m_state = SESSION_STATE_IDLE;
		if (m_handler != NULL)
			m_handler->OnSessionDisconnected(this, SCE_NP_PUSH_ERROR_ABORTED);
		return 0;
	}

	if (cmd->type != SESSION_CMD_CONNECT)
		return 0;

	NpPushOnlineId connectId;
	bool isReconnect = false;
	{
		ScopedLock lock(&m_mutex);
		SceNpCommonPs4_AEC0DAC5(&connectId, &m_connectOnlineId);
		isReconnect = m_isReconnect;
	}

	ConnectionResponseInfo respInfo;
	sceClibMemset(&respInfo, 0, sizeof(respInfo));
	SceUInt32 retryIntervalSec = 0;
	int ret = OpenConnection(&connectId, isReconnect, &respInfo, &retryIntervalSec);
	SceNpCommonPs4_1543D7EE(&connectId);

	if (ret < 0) {
		m_state = SESSION_STATE_IDLE;
		if (m_handler != NULL)
			m_handler->OnSessionConnectFailed(this, ret, retryIntervalSec);
		return ret;
	}

	m_state = SESSION_STATE_ESTABLISHED;
	if (m_handler != NULL)
		m_handler->OnSessionConnected(this, &respInfo);
	return 0;
}

/* 0x813B160C: StateEstablished */
int Session::StateEstablished(const SessionCmd *cmd)
{
	switch (cmd->type) {
	case SESSION_CMD_DISCONNECT:
		m_state = SESSION_STATE_DISCONNECTING;
		ShutdownConnection();
		ErrorRequestAll(SCE_NP_PUSH_ERROR_DISCONNECTED);
		m_state = SESSION_STATE_IDLE;
		if (m_handler != NULL)
			m_handler->OnSessionDisconnected(this, 0);
		return 0;

	case SESSION_CMD_SEND_REQUEST: {
		if (cmd->payload == NULL)
			return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
		RefPtr<Request> req;
		req.Adopt(static_cast<Request *>(cmd->payload));
		int ret = m_connection.SendPacket(req->GetSendPacket().Get());
		if (ret < 0) {
			req->SetResultError(ret, 0);
			return ret;
		}
		if (!m_activeRequests.PushBack(req)) {
			req->SetResultError(SCE_NP_PUSH_ERROR_OUT_OF_MEMORY, 0);
			return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
		}
		return 0;
	}

	case SESSION_CMD_RECV_PACKET: {
		if (cmd->payload == NULL)
			return 0;
		RefPtr<Packet> packet;
		packet.Adopt(static_cast<Packet *>(cmd->payload));
		DispatchReceivedPacket(packet);
		return 0;
	}

	case SESSION_CMD_RECV_ERROR:
		ShutdownConnection();
		ErrorRequestAll(cmd->errorCode < 0 ? cmd->errorCode : SCE_NP_PUSH_ERROR_DISCONNECTED);
		m_state = SESSION_STATE_IDLE;
		if (m_handler != NULL)
			m_handler->OnSessionDisconnected(this, cmd->errorCode);
		return 0;

	default:
		return 0;
	}
}

/* 0x813B1748: StateDisconnecting */
int Session::StateDisconnecting(const SessionCmd *cmd)
{
	if (cmd->type == SESSION_CMD_SEND_REQUEST && cmd->payload != NULL) {
		RefPtr<Request> req;
		req.Adopt(static_cast<Request *>(cmd->payload));
		req->SetResultError(SCE_NP_PUSH_ERROR_DISCONNECTED, 0);
	} else if (cmd->type == SESSION_CMD_RECV_PACKET && cmd->payload != NULL) {
		RefPtr<Packet> pkt;
		pkt.Adopt(static_cast<Packet *>(cmd->payload));
	}
	return 0;
}

/* 0x813B17C0: StateInvalid */
int Session::StateInvalid(const SessionCmd *cmd)
{
	(void)cmd;
	return SCE_NP_PUSH_ERROR_INVALID_STATE;
}

/* 0x813AFE7A */
int Session::StopReceiveThreadAndConnection(void)
{
	if (m_receiveThread == NULL)
		return 0;

	SceNpCommonPs4_A6C7D48D(&m_cancelHandle.cancelable, SCE_NP_PUSH_ERROR_SERVER_RESPONSE, 0);
	SceNpCommonPs4_2C0585AD(m_receiveThread->AsThreadBase(), NULL);
	m_receiveThread->~ReceiveThread();
	SceNpCommonPs4_6B428829(GetAllocator(), m_receiveThread);
	m_receiveThread = NULL;
	m_recvQueue.Clear();
	return 0;
}

/* 0x813AFEDE */
int Session::StopSessionThread(void)
{
	if (m_sessionThread == NULL)
		return 0;

	SceNpCommonPs4_A6C7D48D(&m_cancelHandle.cancelable, SCE_NP_PUSH_ERROR_SERVER_RESPONSE, 0);
	m_cmdQueue.Abort();
	SceNpCommonPs4_2C0585AD(m_sessionThread->AsThreadBase(), NULL);
	m_sessionThread->~SessionThread();
	SceNpCommonPs4_6B428829(GetAllocator(), m_sessionThread);
	m_sessionThread = NULL;
	return 0;
}

/* 0x813B01C0 */
void Session::PopCompletedRequest(RefPtr<Request> *outRequest)
{
	ScopedLock lock(&m_mutex);
	outRequest->Reset();
	for (ListNode<RefPtr<Request> > *iter = m_activeRequests.Begin();
	     iter != m_activeRequests.End(); iter = iter->next) {
		RefPtr<Request> finished;
		iter->value->UnlinkIfFinished(&finished);
		if (!finished.IsNull()) {
			*outRequest = finished;
			m_activeRequests.Erase(iter);
			return;
		}
	}
}

/* 0x813B01EE */
void Session::NotifyCompletedRequests(void)
{
	while (true) {
		RefPtr<Request> completed;
		PopCompletedRequest(&completed);
		if (completed.IsNull())
			break;
		completed->InvokeHandler();
	}
}

/* 0x813B0252 */
void Session::PeekActiveRequest(RefPtr<Request> *outRequest)
{
	ScopedLock lock(&m_mutex);
	if (!m_activeRequests.Empty())
		*outRequest = m_activeRequests.Begin()->value;
	else
		outRequest->Reset();
}

/* 0x813B0372 */
void Session::FindRequestById(int requestId, RefPtr<Request> *outRequest)
{
	ScopedLock lock(&m_mutex);
	outRequest->Reset();
	for (ListNode<RefPtr<Request> > *iter = m_activeRequests.Begin();
	     iter != m_activeRequests.End(); iter = iter->next) {
		if (iter->value->GetRequestId() == requestId) {
			*outRequest = iter->value;
			return;
		}
	}
}

/* 0x813B03C8 */
void Session::CancelCommandRequest(const SessionCmd *cmd)
{
	if (cmd->type != SESSION_CMD_SEND_REQUEST)
		return;

	RefPtr<Request> req;
	FindRequestById(cmd->errorCode, &req);
	if (!req.IsNull())
		req->SetResultError(SCE_NP_PUSH_ERROR_NOT_FOUND, 0);
}

/* 0x813B0530 */
void Session::ChangeState(SessionState nextState, int errorCode)
{
	SessionState prevState;
	{
		ScopedLock lock(&m_mutex);
		prevState = m_state;
		m_state = nextState;
	}

	if (nextState == SESSION_STATE_STOPPING || nextState == SESSION_STATE_ERROR) {
		int err = errorCode;
		if (err == 0) {
			err = (m_state == SESSION_STATE_STOPPING)
				      ? SCE_NP_PUSH_ERROR_NOT_CONNECTED
				      : SCE_NP_PUSH_ERROR_INTERNAL;
		}
		ErrorRequestAll(err);
	}

	if (nextState == SESSION_STATE_STOPPING || nextState == SESSION_STATE_ERROR) {
		if (prevState != SESSION_STATE_CONNECTING &&
		    prevState != SESSION_STATE_ESTABLISHED &&
		    prevState != SESSION_STATE_DISCONNECTING) {
			return;
		}
		for (ListNode<Handler *> *iter = m_handlers.Begin();
		     iter != m_handlers.End(); iter = iter->next) {
			iter->value->OnSessionDisconnected(this, errorCode);
		}
	}
}

/* 0x813B0684 */
int Session::PostCommandChecked(const SessionCmd *cmd, const SceUInt32 *allowedStateMask)
{
	{
		ScopedLock lock(&m_mutex);
		if (m_state == SESSION_STATE_STOPPING || m_state == SESSION_STATE_ERROR)
			return SCE_NP_PUSH_ERROR_NOT_FOUND;
		if (allowedStateMask != NULL &&
		    ((1U << static_cast<SceUInt32>(m_state)) & *allowedStateMask) == 0) {
			return SCE_NP_PUSH_ERROR_NOT_FOUND;
		}
	}
	int ret = m_cmdQueue.Push(cmd);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B06FE */
int Session::PostConnectCommand(void)
{
	SessionCmd cmd(SESSION_CMD_CONNECT);
	SceUInt32 mask = (1U << SESSION_STATE_IDLE);
	int ret = PostCommandChecked(&cmd, &mask);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B0752 */
int Session::PostDisconnectCommand(void)
{
	SessionCmd cmd(SESSION_CMD_DISCONNECT);
	SceUInt32 mask = (1U << SESSION_STATE_IDLE) | (1U << SESSION_STATE_CONNECTING) |
			 (1U << SESSION_STATE_ESTABLISHED);
	int ret = PostCommandChecked(&cmd, &mask);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B07A6 */
int Session::SendLoginRequest(const NpPushOnlineId *onlineId, bool activeStatus,
			      bool includePresence, Request::Handler *handler, int *outRequestId)
{
	(void)includePresence;
	RefPtr<Packet> packet;
	int status = activeStatus ? PRESENCE_STATUS_ACTIVE : PRESENCE_STATUS_SLEEP;
	int ret = Packet::CreateLoginRequest(onlineId, status, &packet);
	if (ret < 0)
		return ret;

	Request *req = Request::Create(REQUEST_TYPE_TRANSACTION, onlineId, packet, handler, NULL);
	if (req == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	RefPtr<Request> reqRef;
	reqRef.Adopt(req);
	ret = reqRef->Start(this, &m_cancelHandle);
	if (ret < 0)
		return ret;

	SessionCmd cmd(SESSION_CMD_SEND_REQUEST);
	cmd.errorCode = reqRef->GetRequestId();
	SceUInt32 mask = (1U << SESSION_STATE_ESTABLISHED);
	ret = PostCommandChecked(&cmd, &mask);
	if (ret < 0) {
		reqRef->AbortAndUnlink();
		return ret;
	}
	if (outRequestId != NULL)
		*outRequestId = reqRef->GetRequestId();
	return 0;
}

/* 0x813B0906 */
int Session::SendLogoutRequest(const NpPushOnlineId *onlineId, Request::Handler *handler,
			       int *outRequestId)
{
	RefPtr<Packet> packet;
	int ret = Packet::CreateLogoutRequest(onlineId, &packet);
	if (ret < 0)
		return ret;

	Request *req = Request::Create(REQUEST_TYPE_TRANSACTION, onlineId, packet, handler, NULL);
	if (req == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	RefPtr<Request> reqRef;
	reqRef.Adopt(req);
	ret = reqRef->Start(this, &m_cancelHandle);
	if (ret < 0)
		return ret;

	SessionCmd cmd(SESSION_CMD_SEND_REQUEST);
	cmd.errorCode = reqRef->GetRequestId();
	SceUInt32 mask = (1U << SESSION_STATE_ESTABLISHED);
	ret = PostCommandChecked(&cmd, &mask);
	if (ret < 0) {
		reqRef->AbortAndUnlink();
		return ret;
	}
	if (outRequestId != NULL)
		*outRequestId = reqRef->GetRequestId();
	return 0;
}

/* 0x813B0A44 */
int Session::SendChangeStatusRequest(const NpPushOnlineId *onlineId, int newStatus,
				     const SceUInt64 *timestamp, Request::Handler *handler,
				     int *outRequestId)
{
	(void)timestamp;
	RefPtr<Packet> packet;
	int ret = Packet::CreateChangeStatusRequest(onlineId, newStatus, &packet);
	if (ret < 0)
		return ret;

	Request *req = Request::Create(REQUEST_TYPE_TRANSACTION, onlineId, packet, handler, NULL);
	if (req == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	RefPtr<Request> reqRef;
	reqRef.Adopt(req);
	ret = reqRef->Start(this, &m_cancelHandle);
	if (ret < 0)
		return ret;

	SessionCmd cmd(SESSION_CMD_SEND_REQUEST);
	cmd.errorCode = reqRef->GetRequestId();
	SceUInt32 mask = (1U << SESSION_STATE_ESTABLISHED);
	ret = PostCommandChecked(&cmd, &mask);
	if (ret < 0) {
		reqRef->AbortAndUnlink();
		return ret;
	}
	if (outRequestId != NULL)
		*outRequestId = reqRef->GetRequestId();
	return 0;
}

/* 0x813B0D40 */
void Session::OnConnectTimeout(void)
{
	ScopedLock lock(&m_mutex);
	if (m_state == SESSION_STATE_CONNECTING)
		SceNpCommonPs4_A6C7D48D(&m_cancelHandle.cancelable, SCE_NP_PUSH_ERROR_UNSUPPORTED, 0);
}

/* 0x813B0D6E */
void Session::FindRequestByTranId(const char *tranId, RefPtr<Request> *outRequest)
{
	ScopedLock lock(&m_mutex);
	outRequest->Reset();
	for (ListNode<RefPtr<Request> > *iter = m_activeRequests.Begin();
	     iter != m_activeRequests.End(); iter = iter->next) {
		if (TranIdEquals(iter->value->GetTranId(), tranId)) {
			*outRequest = iter->value;
			return;
		}
	}
}

/* 0x813B0DCC */
void Session::InspectResponseErrorSource(const RefPtr<Packet> &packet)
{
	SceInt32 returnCode = -1;
	char errSrcBuf[32];
	if (packet->HasErrorSource() &&
	    packet->GetErrorSource(errSrcBuf, sizeof(errSrcBuf)) == 0 &&
	    sceClibStrncmp(errSrcBuf, "client", 7) == 0) {
		packet->GetReturnCode(&returnCode);
	}
}

/* 0x813B0E1E */
void Session::HandleReqTransResponse(const RefPtr<Packet> &packet)
{
	char tranId[32];
	sceClibMemset(tranId, 0, sizeof(tranId));
	if (packet->GetTranIdFixed(tranId) >= 0) {
		RefPtr<Request> req;
		FindRequestByTranId(tranId, &req);
		if (!req.IsNull() && req->GetType() == REQUEST_TYPE_TRANSACTION)
			req->SetResultReceivedReqTransResponse(packet);
		InspectResponseErrorSource(packet);
	}
}

/* 0x813B0EBA */
void Session::HandleQueryResponse(const RefPtr<Packet> &packet)
{
	char tranId[32];
	sceClibMemset(tranId, 0, sizeof(tranId));
	if (packet->GetTranIdFixed(tranId) >= 0) {
		RefPtr<Request> req;
		FindRequestByTranId(tranId, &req);
		if (!req.IsNull() && req->GetType() == REQUEST_TYPE_QUERY)
			req->SetResultReceivedQueryResponse(packet);
	}
}

/* 0x813B11B0 */
void Session::NotifyNotificationPacket(const RefPtr<Packet> &packet)
{
	for (ListNode<Handler *> *iter = m_handlers.Begin();
	     iter != m_handlers.End(); iter = iter->next) {
		iter->value->OnSessionReceivedPacket(this, packet);
	}
}

/* 0x813B1220 */
void Session::NotifyServiceCommPacket(const RefPtr<Packet> &packet)
{
	for (ListNode<Handler *> *iter = m_handlers.Begin();
	     iter != m_handlers.End(); iter = iter->next) {
		iter->value->OnSessionReceivedPacket(this, packet);
	}
}

/* 0x813B1290 */
void Session::NotifyServiceQueryPacket(const RefPtr<Packet> &packet)
{
	for (ListNode<Handler *> *iter = m_handlers.Begin();
	     iter != m_handlers.End(); iter = iter->next) {
		iter->value->OnSessionReceivedPacket(this, packet);
	}
}

/* 0x813B1300 */
int Session::ProcessRequestCommand(int requestId)
{
	RefPtr<Request> req;
	FindRequestById(requestId, &req);
	if (req.IsNull())
		return 0;

	RefPtr<Packet> sendPacket;
	int ret = req->PrepareSendPacket(&m_cancelHandle, &sendPacket);
	if (ret >= 0)
		ret = m_connection.SendPacket(sendPacket.Get());

	if (ret < 0) {
		req->SetResultError(ret, 0);
		return ret;
	}
	return 0;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
