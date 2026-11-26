/*
 * np_push_ctl.cpp: SceNpPush main controller singleton, 8-state machine,
 * IMPS sign-in/out listener integration, and reconnect/presence orchestration.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_ctl.cpp
 * 3.65-CEX: 0x813AB344..0x813ACEE3, 0x813AD0A0..0x813AD14B
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static Ctl *s_ctlInstance = NULL;

struct CtlReconnectTarget {
	const void *const *vtable;
	Ctl *owner;
};

static void CtlReconnectTargetInvoke(CtlReconnectTarget *target)
{
	if (target != NULL && target->owner != NULL)
		target->owner->OnReconnectTimeout();
}

static const void *const kCtlReconnectTargetVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(CtlReconnectTargetInvoke)
};

static int CtlThreadThunk(NpPushThreadBase *threadBase)
{
	(void)threadBase;
	Ctl *ctl = Ctl::GetInstance();
	if (ctl != NULL)
		ctl->ThreadMain();
	return 0;
}

static const void *const kCtlThreadVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(SceNpCommonPs4_502309F4),
	reinterpret_cast<const void *>(CtlThreadThunk)
};

static void OnImpsSessionStateCallback(const NpImpsSessionIdentity *session,
				       int sessionState, int errorCode)
{
	(void)errorCode;
	Ctl *ctl = Ctl::GetInstance();
	if (ctl == NULL)
		return;

	if (sessionState == 6) {
		NpPushOnlineId onlineId;
		if (session != NULL)
			SceNpCommonPs4_D0787194(&onlineId, &session->npId);
		else
			SceNpCommonPs4_B0BAB9BE(&onlineId);
		ctl->PostSignedIn(&onlineId);
		SceNpCommonPs4_1543D7EE(&onlineId);
	} else {
		ctl->PostSignedOut();
	}
}

/* 0x813AB344: InitInstance */
int Ctl::InitInstance(void)
{
	NP_PUSH_ASSERT(s_ctlInstance == NULL, "InitInstance",
		       "core\\np_push_ctl.cpp", 82, "s_ctlInstance == NULL");

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(Ctl), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	Ctl *instance = new (rawMem) Ctl();
	int ret = instance->Init();
	if (ret < 0) {
		instance->~Ctl();
		SceNpCommonPs4_6B428829(GetAllocator(), instance);
		return ret;
	}

	s_ctlInstance = instance;
	return 0;
}

/* 0x813AB410: TermInstance */
void Ctl::TermInstance(void)
{
	if (s_ctlInstance == NULL)
		return;

	s_ctlInstance->Finalize();
	s_ctlInstance->~Ctl();
	SceNpCommonPs4_6B428829(GetAllocator(), s_ctlInstance);
	s_ctlInstance = NULL;
}

/* 0x813AB480: GetInstance */
Ctl *Ctl::GetInstance(void)
{
	return s_ctlInstance;
}

/* 0x813AB498 */
Ctl::Ctl()
	: m_state(CTL_STATE_INIT),
	  m_handler(NULL),
	  m_context(NULL),
	  m_ctlThreadRunning(false),
	  m_impsCallbackId(0),
	  m_isSignedIn(false),
	  m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	SceNpCommonPs4_04CBEB81(&m_ctlThread);
	SceNpCommonPs4_1CC3A645(&m_reconnectJob, GetCalloutContext());
	SceNpCommonPs4_B0BAB9BE(&m_signedInOnlineId);
}

/* 0x813AB5D0 */
Ctl::~Ctl()
{
	SceNpCommonPs4_1543D7EE(&m_signedInOnlineId);
	SceNpCommonPs4_941B0290(&m_reconnectJob);
	SceNpCommonPs4_502309F4(&m_ctlThread);
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813AB6C0: Init */
int Ctl::Init(void)
{
	NP_PUSH_ASSERT(!m_initialized, "Init", "core\\np_push_ctl.cpp", 162, "!m_initialized");

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushCtl", 1);
	if (ret < 0)
		return ret;

	ret = m_cmdQueue.Init();
	if (ret < 0) {
		SceNpCommonPs4_63EED478(&m_mutex);
		return ret;
	}

	ret = m_userRequestList.Init();
	if (ret < 0) {
		m_cmdQueue.Finalize();
		SceNpCommonPs4_63EED478(&m_mutex);
		return ret;
	}

	m_state = CTL_STATE_IDLE;
	m_initialized = true;
	return 0;
}

/* 0x813AB820: Finalize */
void Ctl::Finalize(void)
{
	NP_PUSH_ASSERT(m_initialized, "Finalize", "core\\np_push_ctl.cpp", 204, "m_initialized");

	Stop();
	CancelReconnect();
	DestroyContext();

	if (m_impsCallbackId != 0) {
		npImpsCoreUnregisterCallbacks(m_impsCallbackId);
		m_impsCallbackId = 0;
	}

	m_userRequestList.SetFinishedAll(SCE_NP_PUSH_ERROR_ABORTED);
	DrainFinishedUserRequests();
	m_userRequestList.Finalize();
	m_cmdQueue.Finalize();
	SceNpCommonPs4_63EED478(&m_mutex);
	m_state = CTL_STATE_INIT;
	m_initialized = false;
}

/* 0x813AB960: RegisterHandler */
int Ctl::RegisterHandler(Handler *handler)
{
	NP_PUSH_ASSERT(m_initialized, "RegisterHandler",
		       "core\\np_push_ctl.cpp", 242, "m_initialized");

	ScopedLock lock(&m_mutex);
	m_handler = handler;
	return 0;
}

/* 0x813AB9E0: Start */
int Ctl::Start(void)
{
	NP_PUSH_ASSERT(m_initialized, "Start", "core\\np_push_ctl.cpp", 262, "m_initialized");

	if (m_ctlThreadRunning)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	NpImpsCoreCallbacks impsCallbacks;
	sceClibMemset(&impsCallbacks, 0, sizeof(impsCallbacks));
	impsCallbacks.onSessionState = OnImpsSessionStateCallback;
	int cbId = 0;
	if (npImpsCoreRegisterCallbacks(&impsCallbacks, &cbId) >= 0)
		m_impsCallbackId = cbId;

	m_ctlThread.vtable = &kCtlThreadVtable[2];
	NpPushThreadParam threadParam;
	threadParam.name = "SceNpPushCtl";
	threadParam.stackSize = 16 * 1024;
	threadParam.priority = 160;
	threadParam.cpuAffinity = 0;

	int ret = SceNpCommonPs4_C8F1844F(&m_ctlThread, &threadParam);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_8CBDFDB7(&m_ctlThread);
	if (ret < 0)
		return ret;

	m_ctlThreadRunning = true;

	CtlCmd startCmd;
	sceClibMemset(&startCmd, 0, sizeof(startCmd));
	startCmd.type = CTL_CMD_START;
	return m_cmdQueue.Push(&startCmd);
}

/* 0x813ABBC0: Stop */
int Ctl::Stop(void)
{
	if (!m_ctlThreadRunning)
		return 0;

	CtlCmd stopCmd;
	sceClibMemset(&stopCmd, 0, sizeof(stopCmd));
	stopCmd.type = CTL_CMD_STOP;
	m_cmdQueue.Push(&stopCmd);
	m_cmdQueue.Abort();

	int exitStatus = 0;
	SceNpCommonPs4_2C0585AD(&m_ctlThread, &exitStatus);
	m_ctlThreadRunning = false;
	return 0;
}

/* 0x813ABCB0: PostSignedIn */
int Ctl::PostSignedIn(const NpPushOnlineId *onlineId)
{
	if (!m_initialized || onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	{
		ScopedLock lock(&m_mutex);
		SceNpCommonPs4_AEC0DAC5(&m_signedInOnlineId, onlineId);
		m_isSignedIn = true;
	}

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SIGNED_IN;
	return m_cmdQueue.Push(&cmd);
}

/* 0x813ABD50: PostSignedOut */
int Ctl::PostSignedOut(void)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	{
		ScopedLock lock(&m_mutex);
		m_isSignedIn = false;
	}

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SIGNED_OUT;
	return m_cmdQueue.Push(&cmd);
}

/* 0x813ABDD0: PostSetPresenceOnline */
int Ctl::PostSetPresenceOnline(const NpPushOnlineId *onlineId,
			       UserRequestCallback callback, void *userContext)
{
	if (!m_initialized || onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	UserRequest *req = UserRequest::CreateSetPresenceOnline(onlineId, callback, userContext);
	if (req == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	RefPtr<UserRequest> reqRef;
	reqRef.Adopt(req);
	int ret = m_userRequestList.Register(reqRef);
	if (ret < 0)
		return ret;

	SceNpCommonPs4_64BAC888(req);
	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_USER_REQUEST;
	cmd.payload = req;
	ret = m_cmdQueue.Push(&cmd);
	if (ret < 0)
		SceNpCommonPs4_93C97239(req);
	return ret;
}

/* 0x813ABEC0: PostSetPresenceStatus */
int Ctl::PostSetPresenceStatus(const NpPushOnlineId *onlineId, int status)
{
	if (!m_initialized || onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushOnlineId *copyId = static_cast<NpPushOnlineId *>(
		SceNpCommonPs4_0C922423(sizeof(NpPushOnlineId), GetAllocator()));
	if (copyId == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	SceNpCommonPs4_AEC0DAC5(copyId, onlineId);

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SET_PRESENCE_STATUS;
	cmd.statusCode = status;
	cmd.payload = copyId;
	int ret = m_cmdQueue.Push(&cmd);
	if (ret < 0) {
		SceNpCommonPs4_1543D7EE(copyId);
		SceNpCommonPs4_6B428829(GetAllocator(), copyId);
	}
	return ret;
}

/* 0x813ABFC0: Session::Handler callbacks */
void Ctl::OnSessionConnected(Session *session, const ConnectionResponseInfo *respInfo)
{
	(void)session;
	ConnectionResponseInfo *respCopy = NULL;
	if (respInfo != NULL) {
		respCopy = static_cast<ConnectionResponseInfo *>(
			SceNpCommonPs4_AA980113(GetAllocator(), sizeof(ConnectionResponseInfo)));
		if (respCopy != NULL)
			*respCopy = *respInfo;
	}

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SESSION_CONNECTED;
	cmd.payload = respCopy;
	if (m_cmdQueue.Push(&cmd) < 0 && respCopy != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), respCopy);
}

void Ctl::OnSessionConnectFailed(Session *session, int errorCode, SceUInt32 retryIntervalSec)
{
	(void)session;
	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SESSION_CONNECT_FAILED;
	cmd.statusCode = errorCode;
	cmd.retryIntervalSec = retryIntervalSec;
	m_cmdQueue.Push(&cmd);
}

void Ctl::OnSessionDisconnected(Session *session, int errorCode)
{
	(void)session;
	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SESSION_DISCONNECTED;
	cmd.statusCode = errorCode;
	m_cmdQueue.Push(&cmd);
}

void Ctl::OnSessionReceivedPacket(Session *session, const RefPtr<Packet> &packet)
{
	(void)session;
	if (packet.IsNull())
		return;
	Packet *rawPacket = packet.Get();
	SceNpCommonPs4_64BAC888(rawPacket);

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_SESSION_RECV_PACKET;
	cmd.payload = rawPacket;
	if (m_cmdQueue.Push(&cmd) < 0)
		SceNpCommonPs4_93C97239(rawPacket);
}

/* UserContext::Handler callbacks */
void Ctl::OnUserContextRequestFinished(UserContext *userContext, Request *request)
{
	if (request != NULL)
		SceNpCommonPs4_64BAC888(request);

	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_REQUEST_FINISHED;
	cmd.target = userContext;
	cmd.payload = request;
	if (m_cmdQueue.Push(&cmd) < 0 && request != NULL)
		SceNpCommonPs4_93C97239(request);
}

void Ctl::OnUserContextRetryTimer(UserContext *userContext)
{
	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_USER_RETRY_TIMER;
	cmd.target = userContext;
	m_cmdQueue.Push(&cmd);
}

void Ctl::OnReconnectTimeout(void)
{
	CtlCmd cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.type = CTL_CMD_RECONNECT_TIMER;
	m_cmdQueue.Push(&cmd);
}

/* 0x813AC240: ConnectSession */
int Ctl::ConnectSession(bool isReconnect)
{
	NP_PUSH_ASSERT(m_context != NULL, "ConnectSession",
		       "core\\np_push_ctl.cpp", 492, "m_context != NULL");

	int ret = m_context->ConnectSession(isReconnect);
	if (ret < 0)
		return ret;

	m_state = CTL_STATE_CONNECTING;
	return 0;
}

/* 0x813AC2D0: DestroyContext */
void Ctl::DestroyContext(void)
{
	NP_PUSH_ASSERT(m_initialized, "DestroyContext",
		       "core\\np_push_ctl.cpp", 514, "m_initialized");

	if (m_context != NULL) {
		m_context->Finalize();
		m_context->~Context();
		SceNpCommonPs4_6B428829(GetAllocator(), m_context);
		m_context = NULL;
	}
}

void Ctl::ScheduleReconnect(SceUInt32 delaySec)
{
	CancelReconnect();
	SceUInt32 effectiveDelay = (delaySec > 0) ? delaySec : NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC;
	CtlReconnectTarget *target = static_cast<CtlReconnectTarget *>(
		SceNpCommonPs4_AA980113(GetAllocator(), sizeof(CtlReconnectTarget)));
	if (target == NULL)
		return;

	target->vtable = kCtlReconnectTargetVtable;
	target->owner = this;
	SceNpCommonPs4_ED3D9A81(&m_reconnectJob, effectiveDelay * 1000U, target);
}

void Ctl::CancelReconnect(void)
{
	SceNpCommonPs4_7A30E24E(&m_reconnectJob);
}

void Ctl::DrainFinishedUserRequests(void)
{
	RefPtr<UserRequest> finishedReq;
	while (m_userRequestList.PopFinished(&finishedReq))
		finishedReq->InvokeCallback();
}

/* 0x813AC450: ProcessUserRequestStartPresenceOnline */
int Ctl::ProcessUserRequestStartPresenceOnline(const RefPtr<UserRequest> &userRequest)
{
	NP_PUSH_ASSERT(!userRequest.IsNull(), "ProcessUserRequestStartPresenceOnline",
		       "core\\np_push_ctl.cpp", 568, "userRequest != NULL");

	if (m_context == NULL) {
		m_userRequestList.SetFinished(userRequest, SCE_NP_PUSH_ERROR_INVALID_STATE);
		DrainFinishedUserRequests();
		return SCE_NP_PUSH_ERROR_INVALID_STATE;
	}

	UserContext *user = m_context->GetOrCreateUserContext(&userRequest->GetOnlineId(), this);
	if (user == NULL) {
		m_userRequestList.SetFinished(userRequest, SCE_NP_PUSH_ERROR_OUT_OF_MEMORY);
		DrainFinishedUserRequests();
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	m_userRequestList.SetRunning(userRequest);
	int ret = m_context->StartPresenceOnlineRequest(userRequest);
	if (ret < 0) {
		m_userRequestList.SetFinished(userRequest, ret);
		DrainFinishedUserRequests();
	}
	return ret;
}

/* 0x813AC580: StateIdle_ProcessUserRequestSetPresenceOnline */
int Ctl::StateIdle_ProcessUserRequestSetPresenceOnline(const RefPtr<UserRequest> &userRequest)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_IDLE,
		       "StateIdle_ProcessUserRequestSetPresenceOnline",
		       "core\\np_push_ctl.cpp", 602, "m_state == CTL_STATE_IDLE");

	m_userRequestList.SetFinished(userRequest, SCE_NP_PUSH_ERROR_NOT_CONNECTED);
	DrainFinishedUserRequests();
	return SCE_NP_PUSH_ERROR_NOT_CONNECTED;
}

/* 0x813AC600: StateRunning_ProcessUserRequestSetPresenceOnline */
int Ctl::StateRunning_ProcessUserRequestSetPresenceOnline(const RefPtr<UserRequest> &userRequest)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_RUNNING,
		       "StateRunning_ProcessUserRequestSetPresenceOnline",
		       "core\\np_push_ctl.cpp", 624, "m_state == CTL_STATE_RUNNING");

	if (m_context != NULL) {
		UserContext *user = m_context->GetOrCreateUserContext(
			&userRequest->GetOnlineId(), this);
		if (user != NULL)
			user->SetDesiredStatus(PRESENCE_STATUS_ACTIVE);
		if (m_isSignedIn) {
			m_context->SetSessionOnlineId(&m_signedInOnlineId);
			ConnectSession(false);
		}
	}
	return 0;
}

/* 0x813AC690: StateConnecting_ProcessUserRequestSetPresenceOnline */
int Ctl::StateConnecting_ProcessUserRequestSetPresenceOnline(
	const RefPtr<UserRequest> &userRequest)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_CONNECTING,
		       "StateConnecting_ProcessUserRequestSetPresenceOnline",
		       "core\\np_push_ctl.cpp", 652, "m_state == CTL_STATE_CONNECTING");

	if (m_context != NULL) {
		UserContext *user = m_context->GetOrCreateUserContext(
			&userRequest->GetOnlineId(), this);
		if (user != NULL)
			user->SetDesiredStatus(PRESENCE_STATUS_ACTIVE);
	}
	return 0;
}

/* 0x813AC700: StateEstablished_ProcessUserRequestSetPresenceOnline */
int Ctl::StateEstablished_ProcessUserRequestSetPresenceOnline(
	const RefPtr<UserRequest> &userRequest)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_ESTABLISHED,
		       "StateEstablished_ProcessUserRequestSetPresenceOnline",
		       "core\\np_push_ctl.cpp", 674, "m_state == CTL_STATE_ESTABLISHED");

	return ProcessUserRequestStartPresenceOnline(userRequest);
}

/* 0x813AC744: StateIdle */
int Ctl::StateIdle(const CtlCmd *cmd)
{
	switch (cmd->type) {
	case CTL_CMD_START: {
		void *rawCtx = SceNpCommonPs4_0C922423(sizeof(Context), GetAllocator());
		if (rawCtx == NULL) {
			m_state = CTL_STATE_ERROR;
			return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
		}
		m_context = new (rawCtx) Context();
		int ret = m_context->Init(this, this);
		if (ret < 0) {
			DestroyContext();
			m_state = CTL_STATE_ERROR;
			return ret;
		}
		m_state = CTL_STATE_RUNNING;
		if (m_isSignedIn) {
			m_context->SetSessionOnlineId(&m_signedInOnlineId);
			UserContext *user = m_context->GetOrCreateUserContext(
				&m_signedInOnlineId, this);
			if (user != NULL)
				user->SetDesiredStatus(PRESENCE_STATUS_ACTIVE);
			ConnectSession(false);
		}
		return 0;
	}

	case CTL_CMD_USER_REQUEST: {
		if (cmd->payload != NULL) {
			RefPtr<UserRequest> req;
			req.Adopt(static_cast<UserRequest *>(cmd->payload));
			return StateIdle_ProcessUserRequestSetPresenceOnline(req);
		}
		return 0;
	}

	default:
		return 0;
	}
}

/* 0x813AC8B0: StateRunning */
int Ctl::StateRunning(const CtlCmd *cmd)
{
	switch (cmd->type) {
	case CTL_CMD_SIGNED_IN:
	case CTL_CMD_RECONNECT_TIMER: {
		if (m_context != NULL && m_isSignedIn) {
			m_context->SetSessionOnlineId(&m_signedInOnlineId);
			UserContext *user = m_context->GetOrCreateUserContext(
				&m_signedInOnlineId, this);
			if (user != NULL)
				user->SetDesiredStatus(PRESENCE_STATUS_ACTIVE);
			return ConnectSession(cmd->type == CTL_CMD_RECONNECT_TIMER);
		}
		return 0;
	}

	case CTL_CMD_SIGNED_OUT:
		CancelReconnect();
		if (m_context != NULL)
			m_context->ClearAllUserContexts();
		m_userRequestList.SetFinishedAll(SCE_NP_PUSH_ERROR_SIGNED_OUT);
		DrainFinishedUserRequests();
		return 0;

	case CTL_CMD_USER_REQUEST: {
		if (cmd->payload != NULL) {
			RefPtr<UserRequest> req;
			req.Adopt(static_cast<UserRequest *>(cmd->payload));
			return StateRunning_ProcessUserRequestSetPresenceOnline(req);
		}
		return 0;
	}

	default:
		return 0;
	}
}

/* 0x813ACA1C: StateConnecting */
int Ctl::StateConnecting(const CtlCmd *cmd)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_CONNECTING, "StateConnecting",
		       "core\\np_push_ctl.cpp", 772, "m_state == CTL_STATE_CONNECTING");

	switch (cmd->type) {
	case CTL_CMD_SESSION_CONNECTED: {
		if (cmd->payload != NULL) {
			ConnectionResponseInfo *respInfo =
				static_cast<ConnectionResponseInfo *>(cmd->payload);
			if (m_context != NULL)
				m_context->UpdateIntervals(respInfo);
			SceNpCommonPs4_6B428829(GetAllocator(), respInfo);
		}
		m_state = CTL_STATE_ESTABLISHED;
		if (m_context != NULL) {
			m_context->UpdateInitialSessionStatus();
			m_context->UpdateUserContexts();
		}
		RefPtr<UserRequest> pendingReq;
		while (m_userRequestList.PopRegistered(&pendingReq))
			ProcessUserRequestStartPresenceOnline(pendingReq);
		return 0;
	}

	case CTL_CMD_SESSION_CONNECT_FAILED:
		m_state = CTL_STATE_RUNNING;
		m_userRequestList.SetFinishedAll(cmd->statusCode);
		DrainFinishedUserRequests();
		if (m_isSignedIn)
			ScheduleReconnect(cmd->retryIntervalSec);
		return 0;

	case CTL_CMD_SIGNED_OUT:
		CancelReconnect();
		m_state = CTL_STATE_DISCONNECTING;
		if (m_context != NULL && m_context->GetSession() != NULL)
			m_context->GetSession()->Disconnect();
		return 0;

	case CTL_CMD_USER_REQUEST: {
		if (cmd->payload != NULL) {
			RefPtr<UserRequest> req;
			req.Adopt(static_cast<UserRequest *>(cmd->payload));
			return StateConnecting_ProcessUserRequestSetPresenceOnline(req);
		}
		return 0;
	}

	default:
		return 0;
	}
}

/* 0x813ACBD0: StateEstablished */
int Ctl::StateEstablished(const CtlCmd *cmd)
{
	NP_PUSH_ASSERT(m_state == CTL_STATE_ESTABLISHED, "StateEstablished",
		       "core\\np_push_ctl.cpp", 844, "m_state == CTL_STATE_ESTABLISHED");

	switch (cmd->type) {
	case CTL_CMD_SESSION_RECV_PACKET: {
		if (cmd->payload == NULL)
			return 0;
		RefPtr<Packet> packet;
		packet.Adopt(static_cast<Packet *>(cmd->payload));
		if (m_handler != NULL)
			m_handler->OnCtlReceivedPacket(packet);
		return 0;
	}

	case CTL_CMD_SESSION_DISCONNECTED:
		m_state = CTL_STATE_RUNNING;
		if (m_context != NULL)
			m_context->ResetUsersOnDisconnect();
		m_userRequestList.SetFinishedAll(cmd->statusCode != 0
							 ? cmd->statusCode
							 : SCE_NP_PUSH_ERROR_DISCONNECTED);
		DrainFinishedUserRequests();
		if (m_handler != NULL)
			m_handler->OnCtlDisconnected(cmd->statusCode);
		if (m_isSignedIn && cmd->statusCode != SCE_NP_PUSH_ERROR_KICKED_OUT &&
		    cmd->statusCode != SCE_NP_PUSH_ERROR_SIGNED_OUT) {
			ScheduleReconnect(NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC);
		}
		return 0;

	case CTL_CMD_SIGNED_OUT:
		CancelReconnect();
		m_state = CTL_STATE_DISCONNECTING;
		if (m_context != NULL && m_context->GetSession() != NULL)
			m_context->GetSession()->Disconnect();
		return 0;

	case CTL_CMD_USER_REQUEST: {
		if (cmd->payload != NULL) {
			RefPtr<UserRequest> req;
			req.Adopt(static_cast<UserRequest *>(cmd->payload));
			return StateEstablished_ProcessUserRequestSetPresenceOnline(req);
		}
		return 0;
	}

	case CTL_CMD_REQUEST_FINISHED: {
		if (cmd->payload != NULL) {
			RefPtr<Request> req;
			req.Adopt(static_cast<Request *>(cmd->payload));
			if (m_context != NULL)
				m_context->HandleRequestResult(
					static_cast<UserContext *>(cmd->target), req.Get());
		}
		return 0;
	}

	case CTL_CMD_USER_RETRY_TIMER:
		if (m_context != NULL)
			m_context->UpdateUserContexts();
		return 0;

	case CTL_CMD_SET_PRESENCE_STATUS: {
		if (cmd->payload != NULL) {
			NpPushOnlineId *onlineId = static_cast<NpPushOnlineId *>(cmd->payload);
			if (m_context != NULL) {
				UserContext *user = m_context->GetOrCreateUserContext(
					onlineId, this);
				if (user != NULL) {
					user->SetDesiredStatus(cmd->statusCode);
					m_context->UpdateUserContexts();
				}
			}
			SceNpCommonPs4_1543D7EE(onlineId);
			SceNpCommonPs4_6B428829(GetAllocator(), onlineId);
		}
		return 0;
	}

	default:
		return 0;
	}
}

/* 0x813ACD7C: StateDisconnecting */
int Ctl::StateDisconnecting(const CtlCmd *cmd)
{
	if (cmd->type == CTL_CMD_SESSION_DISCONNECTED) {
		m_state = CTL_STATE_RUNNING;
		if (m_context != NULL)
			m_context->ClearAllUserContexts();
		m_userRequestList.SetFinishedAll(SCE_NP_PUSH_ERROR_DISCONNECTED);
		DrainFinishedUserRequests();
		if (m_handler != NULL)
			m_handler->OnCtlDisconnected(cmd->statusCode);
	}
	return 0;
}

/* 0x813ACDEA: StateError */
int Ctl::StateError(const CtlCmd *cmd)
{
	if (cmd->type == CTL_CMD_USER_REQUEST && cmd->payload != NULL) {
		RefPtr<UserRequest> req;
		req.Adopt(static_cast<UserRequest *>(cmd->payload));
		m_userRequestList.SetFinished(req, SCE_NP_PUSH_ERROR_INTERNAL);
		DrainFinishedUserRequests();
	}
	return SCE_NP_PUSH_ERROR_INTERNAL;
}

/* 0x813ACE5C: StateInvalid */
int Ctl::StateInvalid(const CtlCmd *cmd)
{
	(void)cmd;
	return SCE_NP_PUSH_ERROR_INVALID_STATE;
}

/* 0x813AD0A0: CtlThread main loop */
void Ctl::ThreadMain(void)
{
	typedef int (Ctl::*StateHandler)(const CtlCmd *);
	static const StateHandler kCtlStateTable[] = {
		&Ctl::StateInvalid,		/* CTL_STATE_INIT */
		&Ctl::StateIdle,		/* CTL_STATE_IDLE */
		&Ctl::StateRunning,		/* CTL_STATE_RUNNING */
		&Ctl::StateConnecting,		/* CTL_STATE_CONNECTING */
		&Ctl::StateEstablished,		/* CTL_STATE_ESTABLISHED */
		&Ctl::StateDisconnecting,	/* CTL_STATE_DISCONNECTING */
		&Ctl::StateError,		/* CTL_STATE_ERROR */
		&Ctl::StateInvalid		/* CTL_STATE_STOPPING */
	};

	while (m_state != CTL_STATE_STOPPING) {
		CtlCmd cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		int ret = m_cmdQueue.Pop(&cmd, 0);
		if (ret < 0)
			break;

		if (cmd.type == CTL_CMD_STOP) {
			m_state = CTL_STATE_STOPPING;
			CancelReconnect();
			DestroyContext();
			break;
		}

		int stateIndex = static_cast<int>(m_state);
		if (stateIndex < 0 || stateIndex >= 8)
			StateInvalid(&cmd);
		else
			(this->*kCtlStateTable[stateIndex])(&cmd);
	}
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
