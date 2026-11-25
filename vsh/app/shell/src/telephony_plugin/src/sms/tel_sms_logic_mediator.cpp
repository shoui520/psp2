/*
 * tel_sms_logic_mediator.cpp: SMS logic mediator thread, public SMS API
 * implementation, action executor, and Wi-Fi-only mini stub
 * (3.65-CEX: 0x812517EC..0x812530AF).
 */
#include <std/new>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <processmgr/processmgr.h>
#include "../tel_internal.h"

namespace telephony {

static const int kSmsMediatorThreadPriority = 0x1000010E;
static const SceSize kSmsMediatorStackSize = 0x4000;

/* ------------------------------------------------------------------
 * TelSmsLogicMediator (0x812517EC..0x81252C9F)
 * ------------------------------------------------------------------ */

TelSmsLogicMediator::TelSmsLogicMediator(TelSmsSender *sender,
					 TelSmsReceiver *receiver,
					 TelBbmcAdapterInterface *bbmcAdapter,
					 TelSmsDbManager *dbManager)
	: TelSmsLogicMediatorBase("SceTelSmsLogicMediatorObsv"),
	  TelMessageThread("SceTelSmsLogicMediator",
			   kSmsMediatorThreadPriority,
			   kSmsMediatorStackSize),
	  m_eventCallback(NULL),
	  m_callbackUserData(NULL),
	  m_callbackMutex("SceTelSmsLogicMediatorCb", 0),
	  m_receiveCountMutex("SceTelSmsLogicMediatorRf", 0),
	  m_legacyCallback(NULL),
	  m_legacyUserData(NULL),
	  m_sender(sender),
	  m_receiver(receiver),
	  m_bbmcAdapter(bbmcAdapter),
	  m_dbManager(dbManager),
	  m_action(NULL),
	  m_stateMachine(NULL),
	  m_pendingReceiveCount(0)
{
}

TelSmsLogicMediator::~TelSmsLogicMediator()
{
}

/* 0x8125197E */
TelephonyResult TelSmsLogicMediator::Initialize()
{
	void *actionMem = telMalloc(sizeof(TelSmsLogicAction));
	m_action = (actionMem != NULL)
		? new (actionMem) TelSmsLogicAction(this, m_sender, m_receiver) : NULL;

	void *smMem = telMalloc(sizeof(TelSmsLogicMediatorContext));
	m_stateMachine = (smMem != NULL && m_action != NULL)
		? new (smMem) TelSmsLogicMediatorContext(*m_action) : NULL;

	m_sender->AddObserver(this);
	m_receiver->AddObserver(this);
	m_bbmcAdapter->AddObserver(this);
	m_dbManager->AddObserver(this);

	StartThread(NULL);
	return TelephonyResult(TEL_OK);
}

/* 0x81251A92 */
TelephonyResult TelSmsLogicMediator::Terminate()
{
	StopThread();

	m_dbManager->RemoveObserver(this);
	m_receiver->RemoveObserver(this);
	m_sender->RemoveObserver(this);
	m_bbmcAdapter->RemoveObserver(this);

	if (m_stateMachine != NULL) {
		m_stateMachine->~TelSmsLogicMediatorContext();
		telFree(m_stateMachine);
		m_stateMachine = NULL;
	}
	if (m_action != NULL) {
		m_action->~TelSmsLogicAction();
		telFree(m_action);
		m_action = NULL;
	}

	return TelephonyResult(TEL_OK);
}

/* 0x81251B26 */
TelephonyResult TelSmsLogicMediator::PostNormalEvent(const TelEventInfo &event)
{
	return PostMessage(event);
}

/* 0x81251B74 */
TelephonyResult TelSmsLogicMediator::PostUrgentEvent(const TelEventInfo &event)
{
	return PostUrgentMessage(event);
}

/* 0x81251BC2 */
TelephonyResult TelSmsLogicMediator::GetServiceState(int *outState)
{
	*outState = 0;
	return TelephonyResult(TEL_OK);
}

/* 0x81251BD0 */
int TelSmsLogicMediator::SetEventCallback(TelSmsEventCallback callback, void *userData)
{
	m_callbackMutex.Lock();
	m_eventCallback = callback;
	m_callbackUserData = userData;
	m_callbackMutex.Unlock();
	return 0;
}

/* 0x81251BF4 */
int TelSmsLogicMediator::SetLegacyCallback(TelSmsLegacyCallback callback, void *userData)
{
	m_callbackMutex.Lock();
	m_legacyCallback = callback;
	m_legacyUserData = userData;
	m_callbackMutex.Unlock();
	return 0;
}

/* 0x81251C1C */
TelephonyResult TelSmsLogicMediator::GetDbPath(paf::string *outPath)
{
	return m_dbManager->GetDbPath(outPath);
}

/* 0x81251C62 */
int TelSmsLogicMediator::SendSms(const TelSmsMessageId &messageId)
{
	TelSmsDbMessageIdEvent ev(TEL_SMS_EVENT_SEND_REQUEST, -1, -1, messageId);
	PostNormalEvent(ev);
	return 0;
}

/* 0x81251CF4 */
int TelSmsLogicMediator::ResendSms(const TelSmsMessageId &messageId)
{
	TelSmsDbMessageIdEvent ev(TEL_SMS_EVENT_RESEND_REQUEST, -1, -1, messageId);
	PostNormalEvent(ev);
	return 0;
}

/* 0x81251D86 */
int TelSmsLogicMediator::UpdateSmsStatus(const TelSmsMessageId &messageId, int status)
{
	TelSmsDbMessageIdEvent ev(TEL_SMS_EVENT_UPDATE_STATUS, status, -1, messageId);
	OnReceiveMessage(ev);
	return 0;
}

/* 0x81251E24 */
int TelSmsLogicMediator::DeleteSmsList(const paf::vector<TelSmsMessageId> &messageIds)
{
	TelSmsMessageIdListEvent ev(TEL_SMS_EVENT_DELETE_LIST, messageIds);
	OnReceiveMessage(ev);
	return 0;
}

/* 0x81251F44 */
int TelSmsLogicMediator::DeleteAllSms()
{
	TelSmsDbMessageIdEvent ev(TEL_SMS_EVENT_DELETE_ALL, -1, -1, TelSmsMessageId());
	OnReceiveMessage(ev);
	return 0;
}

/* 0x81251FCA */
int TelSmsLogicMediator::IsValidMessageBody(const paf::wstring &body)
{
	(void)body;
	return 0;
}

/* 0x81251FDC */
void TelSmsLogicMediator::DispatchToListeners(const TelEventInfo &event)
{
	m_callbackMutex.Lock();

	TelSmsLegacyEventRecord record;
	record.category = event.GetCategory();
	record.eventId = event.GetEventId();
	record.eventArg = event.GetEventArg();
	record.eventResult = event.GetEventResult();
	record.messageId = 0;

	if (record.category == 2) {
		const TelSmsDbMessageIdEvent &idEv =
			static_cast<const TelSmsDbMessageIdEvent &>(event);
		record.messageId = idEv.GetMessageId().GetValue();
	}

	if (m_eventCallback != NULL)
		m_eventCallback(event, m_callbackUserData);
	if (m_legacyCallback != NULL)
		m_legacyCallback(&record, m_callbackUserData);

	NotifyObservers(event);
	m_callbackMutex.Unlock();
}

/* 0x81252084 */
void TelSmsLogicMediator::HandleDbStatusUpdated(const TelEventInfo &event)
{
	const TelSmsDbMessageIdEvent &idEv =
		static_cast<const TelSmsDbMessageIdEvent &>(event);
	TelSmsMessageId msgId(idEv.GetMessageId());

	TelSmsDbMessageIdEvent statusEv(TEL_SMS_EVENT_STATUS_UPDATED, -1, -1, msgId);
	DispatchToListeners(statusEv);

	TelSmsMessage message;
	TelephonyResult res = m_dbManager->GetMessageById(msgId, &message);
	if (!res.IsSuccess())
		return;

	if (message.GetFirstPeer().GetStatus() == 7) {
		TelSmsMessageHelper helper;
		bool isActivation = false;
		helper.FormatCarrierMessage(message.GetBody(), &isActivation);
		if (isActivation) {
			TelSmsDbMessageIdEvent actEv(TEL_SMS_EVENT_ACTIVATION_SMS, -1, -1, msgId);
			DispatchToListeners(actEv);
		}
	}
}

/* 0x81252204 */
void TelSmsLogicMediator::OnNotify(const TelEventInfo &event)
{
	int eventId = event.GetEventId();

	switch (eventId) {
	case TEL_SMS_EVENT_INIT_DONE:
	case TEL_SMS_EVENT_STOP:
	case TEL_SMS_EVENT_SEND_REQUEST:
	case TEL_SMS_EVENT_RESEND_REQUEST:
	case TEL_SMS_EVENT_UPDATE_STATUS:
	case TEL_SMS_EVENT_DELETE_ALL:
	case TEL_SMS_EVENT_DELETE_LIST:
	case TEL_BBMC_EVENT_MODEM_ONLINE_READY:
	case TEL_SMS_EVENT_SEND_RESULT:
	case TEL_SMS_EVENT_RESEND_RESULT:
	case TEL_SMS_EVENT_RECEIVE_RESULT:
	case TEL_SMS_EVENT_NEW_MESSAGE_STORED:
		PostNormalEvent(event);
		break;

	case TEL_SMS_EVENT_RECEIVE_REQUEST:
		if (m_pendingReceiveCount < 2U) {
			m_receiveCountMutex.Lock();
			++m_pendingReceiveCount;
			PostNormalEvent(event);
			m_receiveCountMutex.Unlock();
		}
		break;

	case TEL_SMS_DB_EVENT_STATUS_UPDATED:
		HandleDbStatusUpdated(event);
		break;

	case TEL_SMS_DB_EVENT_MESSAGE_DELETED: {
		const TelSmsDbMessageIdEvent &idEv =
			static_cast<const TelSmsDbMessageIdEvent &>(event);
		TelSmsDbMessageIdEvent delEv(TEL_SMS_EVENT_MESSAGE_DELETED, -1, -1,
					     idEv.GetMessageId());
		DispatchToListeners(delEv);
		break;
	}

	case TEL_SMS_DB_EVENT_ALL_DELETED: {
		TelSmsDbMessageIdEvent delAllEv(TEL_SMS_EVENT_ALL_DELETED, -1, -1,
						TelSmsMessageId());
		DispatchToListeners(delAllEv);
		break;
	}

	case TEL_SMS_DB_EVENT_MESSAGE_RECEIVED:
		DispatchToListeners(event);
		break;

	default:
		break;
	}
}

/* 0x81252584 */
void TelSmsLogicMediator::HandleDeleteAll()
{
	if (m_dbManager != NULL) {
		TelephonyResult res = m_dbManager->DeleteAllMessages();
		if (!res.IsSuccess()) {
			TelEventInfo failEv(TEL_SMS_EVENT_DELETE_ALL_FAILED, res.GetCode(), -1);
			DispatchToListeners(failEv);
		}
	} else {
		TelEventInfo failEv(TEL_SMS_EVENT_DELETE_ALL_FAILED, TEL_ERR_NOT_INITIALIZED, -1);
		DispatchToListeners(failEv);
	}
}

/* 0x81252628 */
void TelSmsLogicMediator::HandleDeleteList(const TelEventInfo &event)
{
	const TelSmsMessageIdListEvent &listEv =
		static_cast<const TelSmsMessageIdListEvent &>(event);

	if (m_dbManager != NULL) {
		TelephonyResult res = m_dbManager->DeleteMessages(listEv.GetMessageIds());
		if (!res.IsSuccess()) {
			TelEventInfo failEv(TEL_SMS_EVENT_DELETE_LIST_FAILED, res.GetCode(), -1);
			DispatchToListeners(failEv);
		}
	} else {
		TelEventInfo failEv(TEL_SMS_EVENT_DELETE_LIST_FAILED, TEL_ERR_NOT_INITIALIZED, -1);
		DispatchToListeners(failEv);
	}
}

/* 0x8125272E */
void TelSmsLogicMediator::HandleResendResult(const TelEventInfo &event)
{
	TelSmsMessageId emptyId;
	if (event.GetEventArg() != 0)
		m_stateMachine->ResendSmsFailed(emptyId);
	else
		m_stateMachine->ResendSmsSucceeded(emptyId);
}

/* 0x812527A4 */
void TelSmsLogicMediator::HandleUpdateStatus(const TelEventInfo &event)
{
	const TelSmsDbMessageIdEvent &idEv =
		static_cast<const TelSmsDbMessageIdEvent &>(event);
	TelSmsMessageId msgId(idEv.GetMessageId());

	if (m_dbManager != NULL) {
		int targetStatus = event.GetEventArg();
		TelephonyResult res = m_dbManager->UpdateMessageStatus(msgId, &targetStatus);
		if (!res.IsSuccess()) {
			TelEventInfo failEv(TEL_SMS_EVENT_STATUS_UPDATE_FAILED,
					    res.GetCode(), -1);
			DispatchToListeners(failEv);
		}
	} else {
		TelEventInfo failEv(TEL_SMS_EVENT_STATUS_UPDATE_FAILED,
				    TEL_ERR_NOT_INITIALIZED, -1);
		DispatchToListeners(failEv);
	}
}

/* 0x81252874 */
void TelSmsLogicMediator::HandleSendResult(const TelEventInfo &event)
{
	TelSmsMessageId emptyId;
	if (event.GetEventArg() != 0)
		m_stateMachine->SendSmsFailed(emptyId);
	else
		m_stateMachine->SendSmsSucceeded(emptyId);
}

/* 0x812528EA */
void TelSmsLogicMediator::OnHandleEvent(const TelEventInfo &event)
{
	int eventId = event.GetEventId();

	switch (eventId) {
	case TEL_SMS_EVENT_INIT_DONE:
		m_stateMachine->setDebugFlag(true);
		m_stateMachine->InitializeFinished();
		break;

	case TEL_SMS_EVENT_STOP:
		break;

	case TEL_SMS_EVENT_SEND_REQUEST: {
		const TelSmsDbMessageIdEvent &idEv =
			static_cast<const TelSmsDbMessageIdEvent &>(event);
		m_stateMachine->SendSmsRequest(idEv.GetMessageId());
		break;
	}

	case TEL_SMS_EVENT_RESEND_REQUEST: {
		const TelSmsDbMessageIdEvent &idEv =
			static_cast<const TelSmsDbMessageIdEvent &>(event);
		m_stateMachine->ResendSmsRequest(idEv.GetMessageId());
		break;
	}

	case TEL_SMS_EVENT_RECEIVE_REQUEST:
		m_receiveCountMutex.Lock();
		if (m_pendingReceiveCount > 0U)
			--m_pendingReceiveCount;
		m_receiveCountMutex.Unlock();
		m_stateMachine->ReceiveSmsRequest();
		break;

	case TEL_SMS_EVENT_UPDATE_STATUS:
		HandleUpdateStatus(event);
		break;

	case TEL_SMS_EVENT_DELETE_ALL:
		HandleDeleteAll();
		break;

	case TEL_SMS_EVENT_DELETE_LIST:
		HandleDeleteList(event);
		break;

	case TEL_BBMC_EVENT_MODEM_ONLINE_READY: {
		TelSmsDbMessageIdEvent recvEv(TEL_SMS_EVENT_RECEIVE_REQUEST, -1, -1,
					      TelSmsMessageId());
		PostNormalEvent(recvEv);
		break;
	}

	case TEL_SMS_EVENT_SEND_RESULT:
		HandleSendResult(event);
		break;

	case TEL_SMS_EVENT_RESEND_RESULT:
		HandleResendResult(event);
		break;

	case TEL_SMS_EVENT_RECEIVE_RESULT:
		if (event.GetEventArg() != 0)
			m_stateMachine->ReceiveSmsFailed();
		else
			m_stateMachine->ReceiveSmsSucceeded();
		break;

	case TEL_SMS_EVENT_NEW_MESSAGE_STORED:
		DispatchToListeners(event);
		break;

	default:
		break;
	}
}

/* ------------------------------------------------------------------
 * TelSmsLogicAction (0x81252CA0..0x81252F73)
 * ------------------------------------------------------------------ */

TelSmsLogicAction::TelSmsLogicAction(TelSmsLogicMediator *mediator,
				     TelSmsSender *sender,
				     TelSmsReceiver *receiver)
	: m_mediator(mediator), m_sender(sender), m_receiver(receiver)
{
}

TelSmsLogicAction::~TelSmsLogicAction()
{
}

/* 0x81252D06 */
void TelSmsLogicAction::SetCurrentMessageId(const TelSmsMessageId &messageId)
{
	m_currentMessageId = messageId;
}

/* 0x81252D10 */
void TelSmsLogicAction::ExecuteSend(const TelSmsMessageId &messageId)
{
	TelephonyResult res = m_sender->SendMessage(messageId);
	TelSmsDbMessageIdEvent doneEv(TEL_SMS_EVENT_SEND_RESULT, res.GetCode(), -1, messageId);
	m_mediator->PostUrgentEvent(doneEv);
}

/* 0x81252DB8 */
void TelSmsLogicAction::ExecuteResend(const TelSmsMessageId &messageId)
{
	TelephonyResult res = m_sender->SendMessage(messageId);
	TelSmsDbMessageIdEvent doneEv(TEL_SMS_EVENT_RESEND_RESULT, res.GetCode(), -1, messageId);
	m_mediator->PostUrgentEvent(doneEv);
}

/* 0x81252E60 */
void TelSmsLogicAction::ExecuteReceive()
{
	bool queueEmpty = false;
	bool messageCompleted = false;
	bool retryNeeded = false;

	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	TelephonyResult res = m_receiver->ReceiveNextSms(&queueEmpty,
							 &messageCompleted,
							 &retryNeeded);
	sceKernelProcessPowerUnlockForUser(SCE_KERNEL_POWER_TICK_DEFAULT);

	if (messageCompleted) {
		TelEventInfo storedEv(TEL_SMS_EVENT_NEW_MESSAGE_STORED, -1, -1);
		m_mediator->NotifyObservers(storedEv);
	}

	if (queueEmpty || retryNeeded) {
		TelEventInfo doneEv(TEL_SMS_EVENT_RECEIVE_RESULT, res.GetCode(), -1);
		m_mediator->PostUrgentEvent(doneEv);
	} else {
		TelSmsDbMessageIdEvent nextEv(TEL_SMS_EVENT_RECEIVE_REQUEST, -1, -1,
					      TelSmsMessageId());
		m_mediator->PostNormalEvent(nextEv);
	}
}

/* ------------------------------------------------------------------
 * TelSmsLogicMediatorMini (0x81252F74..0x812530AF)
 * ------------------------------------------------------------------ */

TelSmsLogicMediatorMini::TelSmsLogicMediatorMini()
	: TelSmsLogicMediatorBase("SceTelSmsLogicMediatorMini")
{
}

TelSmsLogicMediatorMini::~TelSmsLogicMediatorMini()
{
}

/* 0x81253016 */
void TelSmsLogicMediatorMini::OnNotify(const TelEventInfo &event)
{
	(void)event.GetEventId();
}

} /* namespace telephony */
