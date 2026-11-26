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

struct SessionThreadAdapter {
	NpPushThreadBase base;
	Session *owner;
};

static int SessionThreadEntry(SessionThreadAdapter *adapter)
{
	if (adapter != NULL && adapter->owner != NULL)
		adapter->owner->ThreadMain();
	return 0;
}

static int ReceiveThreadEntry(SessionThreadAdapter *adapter)
{
	if (adapter != NULL && adapter->owner != NULL)
		adapter->owner->ReceiveMain();
	return 0;
}

static const void *const kSessionThreadVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SessionThreadEntry)
};

static const void *const kReceiveThreadVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(ReceiveThreadEntry)
};

static Session *s_activeSessionForThreads = NULL;

static int SessionThreadThunk(NpPushThreadBase *threadBase)
{
	(void)threadBase;
	if (s_activeSessionForThreads != NULL)
		s_activeSessionForThreads->ThreadMain();
	return 0;
}

static int ReceiveThreadThunk(NpPushThreadBase *threadBase)
{
	(void)threadBase;
	if (s_activeSessionForThreads != NULL)
		s_activeSessionForThreads->ReceiveMain();
	return 0;
}

static const void *const kSessionThreadDirectVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SessionThreadThunk)
};

static const void *const kReceiveThreadDirectVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(ReceiveThreadThunk)
};

/* 0x813AFDC2 */
Session::Session()
	: m_state(SESSION_STATE_INIT),
	  m_handler(NULL),
	  m_sessionThreadRunning(false),
	  m_receiveThreadRunning(false),
	  m_isReconnect(false),
	  m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	SceNpCommonPs4_04CBEB81(&m_sessionThread);
	SceNpCommonPs4_04CBEB81(&m_receiveThread);
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
	SceNpCommonPs4_502309F4(&m_receiveThread);
	SceNpCommonPs4_502309F4(&m_sessionThread);
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
	return 0;
}

/* 0x813B02E0: Start */
int Session::Start(void)
{
	NP_PUSH_ASSERT(m_initialized, "Start",
		       "core\\np_push_session.cpp", 204, "m_initialized");

	if (m_sessionThreadRunning)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	s_activeSessionForThreads = this;
	m_sessionThread.vtable = &kSessionThreadDirectVtable[2];

	NpPushThreadParam threadParam;
	threadParam.name = "SceNpPushSession";
	threadParam.stackSize = 16 * 1024;
	threadParam.priority = 160;
	threadParam.cpuAffinity = 0;

	int ret = SceNpCommonPs4_C8F1844F(&m_sessionThread, &threadParam);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_8CBDFDB7(&m_sessionThread);
	if (ret < 0)
		return ret;

	m_sessionThreadRunning = true;
	return 0;
}

/* 0x813B03E0: Stop */
int Session::Stop(void)
{
	if (!m_sessionThreadRunning)
		return 0;

	SceNpCommonPs4_6B8D9291(&m_cancelHandle);

	SessionCmd stopCmd;
	stopCmd.type = SESSION_CMD_STOP;
	stopCmd.errorCode = 0;
	stopCmd.payload = NULL;
	m_cmdQueue.Push(&stopCmd);
	m_cmdQueue.Abort();

	int exitStatus = 0;
	SceNpCommonPs4_2C0585AD(&m_sessionThread, &exitStatus);
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

	SessionCmd cmd;
	cmd.type = SESSION_CMD_CONNECT;
	cmd.errorCode = 0;
	cmd.payload = NULL;
	return m_cmdQueue.Push(&cmd);
}

/* 0x813B0550: Disconnect */
int Session::Disconnect(void)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	SceNpCommonPs4_6B8D9291(&m_cancelHandle);

	SessionCmd cmd;
	cmd.type = SESSION_CMD_DISCONNECT;
	cmd.errorCode = 0;
	cmd.payload = NULL;
	return m_cmdQueue.Push(&cmd);
}

/* 0x813B05E0: SendRequest */
int Session::SendRequest(const RefPtr<Request> &request)
{
	if (!m_initialized || request.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	Request *rawReq = request.Get();
	SceNpCommonPs4_64BAC888(rawReq);

	SessionCmd cmd;
	cmd.type = SESSION_CMD_SEND_REQUEST;
	cmd.errorCode = 0;
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

	s_activeSessionForThreads = this;
	m_receiveThread.vtable = &kReceiveThreadDirectVtable[2];

	NpPushThreadParam threadParam;
	threadParam.name = "SceNpPushReceive";
	threadParam.stackSize = 16 * 1024;
	threadParam.priority = 160;
	threadParam.cpuAffinity = 0;

	int ret = SceNpCommonPs4_C8F1844F(&m_receiveThread, &threadParam);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_8CBDFDB7(&m_receiveThread);
	if (ret < 0)
		return ret;

	m_receiveThreadRunning = true;
	return 0;
}

/* 0x813B0AD0: StopReceiveThread */
void Session::StopReceiveThread(void)
{
	if (!m_receiveThreadRunning)
		return;

	int exitStatus = 0;
	SceNpCommonPs4_2C0585AD(&m_receiveThread, &exitStatus);
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
			SessionCmd errCmd;
			errCmd.type = SESSION_CMD_RECV_ERROR;
			errCmd.errorCode = ret;
			errCmd.payload = NULL;
			m_cmdQueue.Push(&errCmd);
			break;
		}

		if (!packet.IsNull()) {
			Packet *rawPacket = packet.Get();
			SceNpCommonPs4_64BAC888(rawPacket);
			SessionCmd pktCmd;
			pktCmd.type = SESSION_CMD_RECV_PACKET;
			pktCmd.errorCode = 0;
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
		sceClibMemset(&cmd, 0, sizeof(cmd));
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

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
