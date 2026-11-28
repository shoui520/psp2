/*
 * np_push_request.cpp: Outgoing push protocol request transaction with OAuth access token
 * attachment, 60-second timeout callout, and response return-code evaluation.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_request.cpp
 * 3.65-CEX: 0x813B57AA..0x813B6203
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static void RequestRefDestroy(NpPushRefCounted *base)
{
	Request *req = static_cast<Request *>(base);
	req->~Request();
	SceNpCommonPs4_6B428829(GetAllocator(), req);
}

static const void *const kRequestVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(RequestRefDestroy),
	reinterpret_cast<const void *>(RequestRefDestroy)
};

struct RequestTimeoutTarget {
	const void *const *vtable;
	Request *owner;
};

static void RequestTimeoutInvoke(RequestTimeoutTarget *target)
{
	if (target != NULL && target->owner != NULL)
		target->owner->OnTimeout();
}

static const void *const kRequestTimeoutTargetVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(RequestTimeoutInvoke)
};

static int MapServerReturnCode(SceInt32 returnCode, bool hasErrorSource)
{
	if (returnCode == NP_PUSH_RETURN_CODE_OK ||
	    returnCode == NP_PUSH_RETURN_CODE_ALREADY_LOGGED_IN ||
	    returnCode == NP_PUSH_RETURN_CODE_ALREADY_LOGGED_OUT) {
		return 0;
	}

	if (hasErrorSource)
		return SCE_NP_PUSH_ERROR_SERVER_RESPONSE;
	return SCE_NP_PUSH_ERROR_SERVER_RESPONSE;
}

/* 0x813B57AA */
Request *Request::Create(RequestType requestType, const NpPushOnlineId *onlineId,
			 const RefPtr<Packet> &sendPacket, Handler *handler,
			 void *userContext)
{
	void *rawMem = SceNpCommonPs4_0C922423(sizeof(Request), GetAllocator());
	if (rawMem == NULL)
		return NULL;
	return new (rawMem) Request(requestType, onlineId, sendPacket, handler, userContext);
}

/* 0x813B5830 */
void Request::Destroy(Request *request)
{
	if (request != NULL)
		SceNpCommonPs4_93C97239(request);
}

/* 0x813B585A */
Request::Request(RequestType requestType, const NpPushOnlineId *onlineId,
		 const RefPtr<Packet> &sendPacket, Handler *handler, void *userContext)
	: m_type(requestType),
	  m_state(REQUEST_STATE_IDLE),
	  m_requestId(0),
	  m_needsAuthToken(true),
	  m_sendPacket(sendPacket),
	  m_result(0),
	  m_retryIntervalSec(0),
	  m_handler(handler),
	  m_userContext(userContext)
{
	SceNpCommonPs4_108738F2(this);
	vtable = &kRequestVtable[2];
	SceNpCommonPs4_4266D870(&m_mutex);
	SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushRequest", 1);
	SceNpCommonPs4_1CC3A645(&m_timeoutJob, GetCalloutContext());
	if (onlineId != NULL)
		SceNpCommonPs4_AEC0DAC5(&m_onlineId, onlineId);
	else
		SceNpCommonPs4_B0BAB9BE(&m_onlineId);
	sceClibMemset(m_tranId, 0, sizeof(m_tranId));
}

/* 0x813B58E8 */
Request::~Request()
{
	NP_PUSH_ASSERT(m_state != REQUEST_STATE_RUNNING, "~Request",
		       "core\\np_push_request.cpp", 95, "m_state != REQUEST_STATE_RUNNING");

	SceNpCommonPs4_7A30E24E(&m_timeoutJob);
	SceNpCommonPs4_941B0290(&m_timeoutJob);
	SceNpCommonPs4_1543D7EE(&m_onlineId);
	SceNpCommonPs4_63EED478(&m_mutex);
	SceNpCommonPs4_294F0B1A(&m_mutex);
	SceNpCommonPs4_BA23AD76(this);
}

/* 0x813B5A20: Start */
int Request::Start(Session *session, NpPushHandle *cancelHandle)
{
	NP_PUSH_ASSERT(session != NULL, "Start", "core\\np_push_request.cpp", 128,
		       "session != NULL");

	ScopedLock lock(&m_mutex);
	if (m_state != REQUEST_STATE_IDLE)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;
	if (m_sendPacket.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = Packet::GenerateTranId(m_tranId, sizeof(m_tranId));
	if (ret < 0)
		return ret;

	ret = m_sendPacket->SetTranId(m_tranId);
	if (ret < 0)
		return ret;

	NpPushUserToken userToken;
	SceNpCommonPs4_988DFD87(&userToken);
	ret = SceNpCommonPs4_191AEA9E(cancelHandle, &m_onlineId, &userToken);
	if (ret < 0) {
		SceNpCommonPs4_78CC9FCD(&userToken);
		return ret;
	}

	ret = m_sendPacket->SetAuthAccessToken(userToken.accessToken);
	SceNpCommonPs4_78CC9FCD(&userToken);
	if (ret < 0)
		return ret;

	m_state = REQUEST_STATE_RUNNING;

	RequestTimeoutTarget *timeoutTarget = static_cast<RequestTimeoutTarget *>(
		SceNpCommonPs4_AA980113(GetAllocator(), sizeof(RequestTimeoutTarget)));
	if (timeoutTarget != NULL) {
		timeoutTarget->vtable = kRequestTimeoutTargetVtable;
		timeoutTarget->owner = this;
		SceNpCommonPs4_ED3D9A81(&m_timeoutJob, NP_PUSH_REQUEST_TIMEOUT_USEC / 1000U,
					timeoutTarget);
	}

	RefPtr<Request> selfRef(this);
	ret = session->SendRequest(selfRef);
	if (ret < 0) {
		SceNpCommonPs4_7A30E24E(&m_timeoutJob);
		if (timeoutTarget != NULL)
			SceNpCommonPs4_6B428829(GetAllocator(), timeoutTarget);
		m_state = REQUEST_STATE_IDLE;
		return ret;
	}

	return 0;
}

/* 0x813B5C80: Cancel */
void Request::Cancel(void)
{
	SetResultError(SCE_NP_PUSH_ERROR_ABORTED, 0);
}

/* 0x813B5B32 */
void Request::AbortAndUnlink(void)
{
	ScopedLock lock(&m_mutex);
	SceNpCommonPs4_7A30E24E(&m_timeoutJob);
	m_handler = NULL;
	m_sendPacket.Reset();
	m_state = REQUEST_STATE_FINISHED;
}

/* 0x813B5FE2 */
void Request::UnlinkIfFinished(RefPtr<Request> *outSelf)
{
	if (outSelf == NULL)
		return;
	ScopedLock lock(&m_mutex);
	if (m_state == REQUEST_STATE_FINISHED)
		*outSelf = RefPtr<Request>(this);
	else
		outSelf->Reset();
}

/* 0x813B6032 */
void Request::InvokeHandler(void)
{
	if (m_handler != NULL)
		m_handler->OnRequestFinished(this);
}

/* 0x813B60A4 */
int Request::PrepareSendPacket(NpPushHandle *cancelHandle, RefPtr<Packet> *outPacket)
{
	RefPtr<Packet> packet = m_sendPacket;
	if (m_needsAuthToken && !packet.IsNull()) {
		NpPushUserToken userToken;
		SceNpCommonPs4_988DFD87(&userToken);
		int ret = SceNpCommonPs4_191AEA9E(cancelHandle, &m_onlineId, &userToken);
		if (ret >= 0)
			ret = packet->SetAuthAccessToken(userToken.accessToken);
		SceNpCommonPs4_78CC9FCD(&userToken);
		if (ret < 0)
			return ret;
	}

	*outPacket = packet;
	m_sendPacket.Reset();
	return 0;
}

/* 0x813B5CA8: OnTimeout */
void Request::OnTimeout(void)
{
	SetResultError(SCE_NP_PUSH_ERROR_TIMEOUT, 0);
}

/* 0x813B5D10: SetResultReceivedReqTransResponse */
int Request::SetResultReceivedReqTransResponse(const RefPtr<Packet> &respPacket)
{
	NP_PUSH_ASSERT(!respPacket.IsNull(), "SetResultReceivedReqTransResponse",
		       "core\\np_push_request.cpp", 242, "respPacket != NULL");

	ScopedLock lock(&m_mutex);
	if (m_state != REQUEST_STATE_RUNNING)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	SceNpCommonPs4_7A30E24E(&m_timeoutJob);

	SceInt32 returnCode = 0;
	int ret = respPacket->GetReturnCode(&returnCode);
	if (ret < 0)
		return SetResultErrorImpl(SCE_NP_PUSH_ERROR_INVALID_PACKET, 0);

	SceUInt32 retryInterval = 0;
	if (respPacket->HasRetryInterval())
		respPacket->GetRetryInterval(&retryInterval);

	bool hasErrorSource = respPacket->HasErrorSource();
	int mappedResult = MapServerReturnCode(returnCode, hasErrorSource);

	m_recvPacket = respPacket;
	m_result = mappedResult;
	m_retryIntervalSec = retryInterval;
	m_state = REQUEST_STATE_FINISHED;
	NotifyFinished();
	return 0;
}

/* 0x813B5E90: SetResultReceivedQueryResponse */
int Request::SetResultReceivedQueryResponse(const RefPtr<Packet> &respPacket)
{
	NP_PUSH_ASSERT(!respPacket.IsNull(), "SetResultReceivedQueryResponse",
		       "core\\np_push_request.cpp", 296, "respPacket != NULL");

	ScopedLock lock(&m_mutex);
	if (m_state != REQUEST_STATE_RUNNING)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	SceNpCommonPs4_7A30E24E(&m_timeoutJob);

	m_recvPacket = respPacket;
	m_result = 0;
	m_retryIntervalSec = 0;
	m_state = REQUEST_STATE_FINISHED;
	NotifyFinished();
	return 0;
}

/* 0x813B5FC0: SetResultErrorImpl */
int Request::SetResultErrorImpl(int errorCode, SceUInt32 retryIntervalSec)
{
	NP_PUSH_ASSERT(errorCode < 0, "SetResultErrorImpl",
		       "core\\np_push_request.cpp", 342, "errorCode < 0");

	if (m_state != REQUEST_STATE_RUNNING)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	SceNpCommonPs4_7A30E24E(&m_timeoutJob);
	m_result = errorCode;
	m_retryIntervalSec = retryIntervalSec;
	m_state = REQUEST_STATE_FINISHED;
	NotifyFinished();
	return 0;
}

/* 0x813B6068: SetResultError */
int Request::SetResultError(int errorCode, SceUInt32 retryIntervalSec)
{
	NP_PUSH_ASSERT(errorCode < 0, "SetResultError",
		       "core\\np_push_request.cpp", 368, "errorCode < 0");

	ScopedLock lock(&m_mutex);
	return SetResultErrorImpl(errorCode, retryIntervalSec);
}

void Request::NotifyFinished(void)
{
	if (m_handler != NULL)
		m_handler->OnRequestFinished(this);
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
