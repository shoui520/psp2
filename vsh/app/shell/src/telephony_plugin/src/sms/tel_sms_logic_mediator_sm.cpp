/*
 * tel_sms_logic_mediator_sm.cpp: SMC-generated state machine for
 * TelSmsLogicMediator (Initial -> Idle <-> Receiving / Sending / Resending)
 * (3.65-CEX: 0x812530B0..0x812533B3).
 */
#include "../tel_internal.h"

namespace telephony {

/* Default transitions delegate to Default() (0x812530B0) */
void TelSmsLogicMediatorState::InitializeFinished(TelSmsLogicMediatorContext &context)
{
	Default(context);
}

void TelSmsLogicMediatorState::ReceiveSmsFailed(TelSmsLogicMediatorContext &context)
{
	Default(context);
}

void TelSmsLogicMediatorState::ReceiveSmsRequest(TelSmsLogicMediatorContext &context)
{
	Default(context);
}

void TelSmsLogicMediatorState::ReceiveSmsSucceeded(TelSmsLogicMediatorContext &context)
{
	Default(context);
}

void TelSmsLogicMediatorState::ResendSmsFailed(TelSmsLogicMediatorContext &context,
					       const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

void TelSmsLogicMediatorState::ResendSmsRequest(TelSmsLogicMediatorContext &context,
						const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

void TelSmsLogicMediatorState::ResendSmsSucceeded(TelSmsLogicMediatorContext &context,
						  const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

void TelSmsLogicMediatorState::SendSmsFailed(TelSmsLogicMediatorContext &context,
					     const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

void TelSmsLogicMediatorState::SendSmsRequest(TelSmsLogicMediatorContext &context,
					      const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

void TelSmsLogicMediatorState::SendSmsSucceeded(TelSmsLogicMediatorContext &context,
						const TelSmsMessageId &messageId)
{
	(void)messageId;
	Default(context);
}

/* Concrete state classes (0x815AC258..0x815AC288) */
class TelSmsMediatorStateIdle : public TelSmsLogicMediatorState {
public:
	TelSmsMediatorStateIdle(const char *name, int stateId)
		: TelSmsLogicMediatorState(name, stateId) {}

	virtual void ReceiveSmsRequest(TelSmsLogicMediatorContext &context);
	virtual void ResendSmsRequest(TelSmsLogicMediatorContext &context,
				      const TelSmsMessageId &messageId);
	virtual void SendSmsRequest(TelSmsLogicMediatorContext &context,
				    const TelSmsMessageId &messageId);
};

class TelSmsMediatorStateInitial : public TelSmsLogicMediatorState {
public:
	TelSmsMediatorStateInitial(const char *name, int stateId)
		: TelSmsLogicMediatorState(name, stateId) {}

	virtual void InitializeFinished(TelSmsLogicMediatorContext &context);
};

class TelSmsMediatorStateReceiving : public TelSmsLogicMediatorState {
public:
	TelSmsMediatorStateReceiving(const char *name, int stateId)
		: TelSmsLogicMediatorState(name, stateId) {}

	virtual void Entry(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsRequest(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsFailed(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsSucceeded(TelSmsLogicMediatorContext &context);
};

class TelSmsMediatorStateResending : public TelSmsLogicMediatorState {
public:
	TelSmsMediatorStateResending(const char *name, int stateId)
		: TelSmsLogicMediatorState(name, stateId) {}

	virtual void Entry(TelSmsLogicMediatorContext &context);
	virtual void ResendSmsFailed(TelSmsLogicMediatorContext &context,
				     const TelSmsMessageId &messageId);
	virtual void ResendSmsSucceeded(TelSmsLogicMediatorContext &context,
					const TelSmsMessageId &messageId);
};

class TelSmsMediatorStateSending : public TelSmsLogicMediatorState {
public:
	TelSmsMediatorStateSending(const char *name, int stateId)
		: TelSmsLogicMediatorState(name, stateId) {}

	virtual void Entry(TelSmsLogicMediatorContext &context);
	virtual void SendSmsFailed(TelSmsLogicMediatorContext &context,
				   const TelSmsMessageId &messageId);
	virtual void SendSmsSucceeded(TelSmsLogicMediatorContext &context,
				      const TelSmsMessageId &messageId);
};

/* Static state instances matching 0x812532E0 */
static TelSmsMediatorStateIdle      s_stateIdle("TelSmsLogicMediatorSm::Idle", 0);
static TelSmsMediatorStateInitial   s_stateInitial("TelSmsLogicMediatorSm::Initial", 1);
static TelSmsMediatorStateReceiving s_stateReceiving("TelSmsLogicMediatorSm::Receiving", 2);
static TelSmsMediatorStateResending s_stateResending("TelSmsLogicMediatorSm::Resending", 3);
static TelSmsMediatorStateSending   s_stateSending("TelSmsLogicMediatorSm::Sending", 4);

/* 0x812530BC */
void TelSmsMediatorStateIdle::ReceiveSmsRequest(TelSmsLogicMediatorContext &context)
{
	context.getState().Exit(context);
	context.setState(s_stateReceiving);
	context.getState().Entry(context);
}

/* 0x812530DC */
void TelSmsMediatorStateIdle::ResendSmsRequest(TelSmsLogicMediatorContext &context,
					       const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.SetCurrentMessageId(messageId);
	context.setState(s_stateResending);
	context.getState().Entry(context);
}

/* 0x81253112 */
void TelSmsMediatorStateIdle::SendSmsRequest(TelSmsLogicMediatorContext &context,
					     const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.SetCurrentMessageId(messageId);
	context.setState(s_stateSending);
	context.getState().Entry(context);
}

/* 0x81253148 */
void TelSmsMediatorStateInitial::InitializeFinished(TelSmsLogicMediatorContext &context)
{
	context.getState().Exit(context);
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x81253168 */
void TelSmsMediatorStateReceiving::Entry(TelSmsLogicMediatorContext &context)
{
	context.getOwner().ExecuteReceive();
}

void TelSmsMediatorStateReceiving::ReceiveSmsRequest(TelSmsLogicMediatorContext &context)
{
	context.getOwner().ExecuteReceive();
}

/* 0x81253174 */
void TelSmsMediatorStateReceiving::ReceiveSmsFailed(TelSmsLogicMediatorContext &context)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnReceiveFailed();
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x812531A6 */
void TelSmsMediatorStateReceiving::ReceiveSmsSucceeded(TelSmsLogicMediatorContext &context)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnReceiveSucceeded();
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x812531D8 */
void TelSmsMediatorStateResending::Entry(TelSmsLogicMediatorContext &context)
{
	TelSmsLogicAction &action = context.getOwner();
	action.ExecuteResend(action.GetCurrentMessageId());
}

/* 0x812531F0 */
void TelSmsMediatorStateResending::ResendSmsFailed(TelSmsLogicMediatorContext &context,
						   const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnResendFailed(messageId);
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x81253226 */
void TelSmsMediatorStateResending::ResendSmsSucceeded(TelSmsLogicMediatorContext &context,
						      const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnResendSucceeded(messageId);
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x8125325C */
void TelSmsMediatorStateSending::Entry(TelSmsLogicMediatorContext &context)
{
	TelSmsLogicAction &action = context.getOwner();
	action.ExecuteSend(action.GetCurrentMessageId());
}

/* 0x81253274 */
void TelSmsMediatorStateSending::SendSmsFailed(TelSmsLogicMediatorContext &context,
					       const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnSendFailed(messageId);
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* 0x812532AA */
void TelSmsMediatorStateSending::SendSmsSucceeded(TelSmsLogicMediatorContext &context,
						  const TelSmsMessageId &messageId)
{
	TelSmsLogicAction &action = context.getOwner();
	context.getState().Exit(context);
	context.clearState();
	action.OnSendSucceeded(messageId);
	context.setState(s_stateIdle);
	context.getState().Entry(context);
}

/* TelSmsLogicMediatorContext methods */
TelSmsLogicMediatorContext::TelSmsLogicMediatorContext(TelSmsLogicAction &owner)
	: statemap::FSMContext(s_stateInitial), m_owner(owner)
{
}

void TelSmsLogicMediatorContext::enterStartState()
{
	getState().Entry(*this);
}

void TelSmsLogicMediatorContext::InitializeFinished()
{
	setTransition("InitializeFinished");
	getState().InitializeFinished(*this);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ReceiveSmsRequest()
{
	setTransition("ReceiveSmsRequest");
	getState().ReceiveSmsRequest(*this);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ReceiveSmsSucceeded()
{
	setTransition("ReceiveSmsSucceeded");
	getState().ReceiveSmsSucceeded(*this);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ReceiveSmsFailed()
{
	setTransition("ReceiveSmsFailed");
	getState().ReceiveSmsFailed(*this);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::SendSmsRequest(const TelSmsMessageId &messageId)
{
	setTransition("SendSmsRequest");
	getState().SendSmsRequest(*this, messageId);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::SendSmsSucceeded(const TelSmsMessageId &messageId)
{
	setTransition("SendSmsSucceeded");
	getState().SendSmsSucceeded(*this, messageId);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::SendSmsFailed(const TelSmsMessageId &messageId)
{
	setTransition("SendSmsFailed");
	getState().SendSmsFailed(*this, messageId);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ResendSmsRequest(const TelSmsMessageId &messageId)
{
	setTransition("ResendSmsRequest");
	getState().ResendSmsRequest(*this, messageId);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ResendSmsSucceeded(const TelSmsMessageId &messageId)
{
	setTransition("ResendSmsSucceeded");
	getState().ResendSmsSucceeded(*this, messageId);
	setTransition(NULL);
}

void TelSmsLogicMediatorContext::ResendSmsFailed(const TelSmsMessageId &messageId)
{
	setTransition("ResendSmsFailed");
	getState().ResendSmsFailed(*this, messageId);
	setTransition(NULL);
}

} /* namespace telephony */
