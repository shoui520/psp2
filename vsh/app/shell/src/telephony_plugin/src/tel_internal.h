/*
 * tel_internal.h: internal classes and interfaces shared across
 * telephony_plugin top-level, attribute stores, logic factory, SMS logic,
 * and general logic layers.
 */
#ifndef TELEPHONY_INTERNAL_H
#define TELEPHONY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <thread/thread.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../include/common/statemap.h"
#include "common/tel_common.h"
#include "dbmanager/tel_sms_dbmanager.h"
#include "sms_pdu_transcoder/tel_sms_pdu_transcoder.h"
#include "bbmc/tel_bbmc_adapter.h"

namespace telephony {

/* Attribute IDs mapped across INI, Registry, IdStorage, and Memory stores */
enum TelAttributeId {
	/* INI file attributes (0x10000) */
	TEL_ATTR_LOOPBACK_AUTO_RECV = 0x10001,
	TEL_ATTR_LOOPBACK_PIN_STATUS = 0x10002,
	TEL_ATTR_LOOPBACK_SIM_LOCK_STATE = 0x10003,
	TEL_ATTR_LOOPBACK_SIM_BLOCK_TYPE = 0x10004,
	TEL_ATTR_LOOPBACK_INTERRUPT_TIME = 0x10005,
	TEL_ATTR_LOOPBACK_INTERRUPT_EVENT = 0x10006,
	TEL_ATTR_ANTENNA_LEVEL_MAP = 0x10007,

	/* Registry attributes (0x20000) */
	TEL_ATTR_USE_DEBUG_SETTINGS = 0x20001,
	TEL_ATTR_IS_COM_EXIST = 0x20002,
	TEL_ATTR_IMSI = 0x20003,
	TEL_ATTR_APN_NAME = 0x20004,
	TEL_ATTR_APN_USER_ID = 0x20005,
	TEL_ATTR_APN_PASSWORD = 0x20006,
	TEL_ATTR_IS_APN_INIT = 0x20007,
	TEL_ATTR_MCC = 0x20008,
	TEL_ATTR_MNC = 0x20009,
	TEL_ATTR_NETWORK_SELECTION = 0x2000A,
	TEL_ATTR_ENABLE_DATA_ROAMING = 0x2000B,
	TEL_ATTR_COM_OPERATION_MODE = 0x2000C,
	TEL_ATTR_RAT = 0x2000D,
	TEL_ATTR_MAX_TEMP = 0x2000E,
	TEL_ATTR_RECOVER_TEMP = 0x2000F,
	TEL_ATTR_USB_SETTING = 0x20010,
	TEL_ATTR_DBG_LOG_TEL = 0x20011,
	TEL_ATTR_DBG_LOG_BBMC = 0x20012,
	TEL_ATTR_DBG_LOG_NET = 0x20013,
	TEL_ATTR_SIM_UNIQUE_ID = 0x20014,
	TEL_ATTR_BILLING_PLAN = 0x20015,
	TEL_ATTR_FORBIDDEN_PHONE = 0x20016,

	/* Dual Registry/INI attributes (0x30000) */
	TEL_ATTR_FLIGHT_MODE = 0x30001,
	TEL_ATTR_ENABLE_WWAN = 0x30002,

	/* IdStorage attributes (0x40000 / 0x60000) */
	TEL_ATTR_IDSTORAGE_MODEM_ID = 0x40001,
	TEL_ATTR_ANTENNA_DISP_TYPE = 0x60001,
	TEL_ATTR_PRODUCT_TARGET_OPERATOR = 0x60002,

	/* Temporary in-memory attributes (0x80001..0x8002B) */
	TEL_ATTR_MEMORY_FIRST = 0x80001,
	TEL_ATTR_MEMORY_COUNT = 43
};

/* SMS mediator and general logic event IDs */
enum TelSmsLogicEventId {
	TEL_SMS_EVENT_INIT_DONE = 257,
	TEL_SMS_EVENT_STOP = 258,
	TEL_SMS_EVENT_SEND_REQUEST = 259,
	TEL_SMS_EVENT_RESEND_REQUEST = 260,
	TEL_SMS_EVENT_RECEIVE_REQUEST = 261,
	TEL_SMS_EVENT_UPDATE_STATUS = 262,
	TEL_SMS_EVENT_DELETE_ALL = 263,
	TEL_SMS_EVENT_DELETE_LIST = 264,

	TEL_SMS_EVENT_STATUS_UPDATED = 1281,
	TEL_SMS_EVENT_STATUS_UPDATE_FAILED = 1282,
	TEL_SMS_EVENT_ALL_DELETED = 1283,
	TEL_SMS_EVENT_DELETE_ALL_FAILED = 1284,
	TEL_SMS_EVENT_SEND_RESULT = 1285,
	TEL_SMS_EVENT_RESEND_RESULT = 1286,
	TEL_SMS_EVENT_RECEIVE_RESULT = 1287,
	TEL_SMS_EVENT_NEW_MESSAGE_STORED = 1288,
	TEL_SMS_EVENT_MESSAGE_DELETED = 1289,
	TEL_SMS_EVENT_DELETE_LIST_FAILED = 1290,
	TEL_SMS_EVENT_ACTIVATION_SMS = 1291
};

enum TelGeneralGuiEventId {
	TEL_GUI_EVENT_ERROR_DIALOG = 770,
	TEL_GUI_EVENT_OVERHEAT_DIALOG = 774,
	TEL_GUI_EVENT_SIM_STATE_RESET = 778,
	TEL_GUI_EVENT_MM_REJECT_CAUSE = 781,
	TEL_GUI_EVENT_COM_FW_UPDATE = 827,
	TEL_GUI_EVENT_SIM_REMOVED = 1028,
	TEL_GUI_EVENT_SIM_ERROR = 1029,
	TEL_GUI_EVENT_SIM_NOT_READY = 1076,
	TEL_GUI_EVENT_DIALOG_ACCEPTED = 34601,
	TEL_GUI_EVENT_DIALOG_CANCELLED = 34602
};

enum TelGeneralDialogType {
	TEL_DIALOG_SIM_ERROR = 0,
	TEL_DIALOG_SIM_REMOVED = 1,
	TEL_DIALOG_SIM_PIN_REQUIRED = 2,
	TEL_DIALOG_OVERHEAT = 3,
	TEL_DIALOG_MM_REJECT_NOT_PROVISIONED = 4,
	TEL_DIALOG_MM_REJECT_SIM_NOT_ALLOWED = 5,
	TEL_DIALOG_MM_REJECT_PHONE_NOT_ALLOWED = 6,
	TEL_DIALOG_COUNT = 7
};

/* Legacy flat event record forwarded to C-style SMS listener callbacks */
struct TelSmsLegacyEventRecord {
	int category;
	int eventId;
	int eventArg;
	int eventResult;
	SceUInt64 messageId;
};

typedef void (*TelSmsEventCallback)(const TelEventInfo &event, void *userData);
typedef void (*TelSmsLegacyCallback)(const TelSmsLegacyEventRecord *record, void *userData);

/* Event payload carrying a list of SMS message IDs (event 264) */
class TelSmsMessageIdListEvent : public TelEventInfo {
public:
	TelSmsMessageIdListEvent(int eventId, const paf::vector<TelSmsMessageId> &ids)
		: TelEventInfo(eventId, -1, -1), m_messageIds(ids)
	{
		SetCategory(4);
	}
	virtual ~TelSmsMessageIdListEvent() {}

	const paf::vector<TelSmsMessageId> &GetMessageIds() const { return m_messageIds; }

private:
	paf::vector<TelSmsMessageId> m_messageIds;
};

/* ------------------------------------------------------------------
 * Attribute stores & factory (0x8124EC9C..0x8124EE23, 0x8124F5D8..0x812506FB)
 * ------------------------------------------------------------------ */

class TelSmsManagerIdStorageAttribute : public TelAttributeStore {
public:
	TelSmsManagerIdStorageAttribute();
	virtual ~TelSmsManagerIdStorageAttribute();

	virtual int GetMask() const { return 0x40000; }
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr);
	virtual TelephonyResult GetInt(int attrId, int *outValue);
	virtual TelephonyResult GetFloat(int attrId, float *outValue);
	virtual TelephonyResult GetString(int attrId, paf::string *outValue);

private:
	TelephonyResult ReadAndCacheAttribute(TelAttributeValue *outAttr);

	paf::thread::Mutex m_mutex;
	paf::map<int, TelAttributeValue> m_cache;
};

class TelSmsManagerIniFileAttribute : public TelIniFileAttributeBase {
public:
	TelSmsManagerIniFileAttribute();
	virtual ~TelSmsManagerIniFileAttribute();

	virtual int GetMask() const { return 0x10000; }
	virtual const TelAttributeEntry *FindEntry(int attrId) const;
};

class TelSmsManagerRegistoryAttribute : public TelRegistoryAttributeBase {
public:
	TelSmsManagerRegistoryAttribute();
	virtual ~TelSmsManagerRegistoryAttribute();

	virtual int GetMask() const { return 0x20000; }
	virtual const TelAttributeEntry *FindEntry(int attrId) const;
	virtual TelephonyResult SetAttribute(const TelAttributeValue &attr);
	virtual TelephonyResult GetAttribute(TelAttributeValue *outAttr);
	virtual TelephonyResult SetInt(int attrId, int value);
	virtual TelephonyResult GetInt(int attrId, int *outValue);
	virtual TelephonyResult AddInt(int attrId, int delta);
	virtual TelephonyResult SetFloat(int attrId, float value);
	virtual TelephonyResult GetFloat(int attrId, float *outValue);
	virtual TelephonyResult SetString(int attrId, const paf::string &value);
	virtual TelephonyResult GetString(int attrId, paf::string *outValue);

private:
	TelephonyResult ReadWithOverride(TelAttributeValue *outAttr);

	paf::thread::Mutex m_cacheMutex;
	paf::map<int, TelAttributeValue> m_cache;
};

class TelSmsManagerMemoryAttribute : public TelTemporaryAttributeBase {
public:
	TelSmsManagerMemoryAttribute();
	virtual ~TelSmsManagerMemoryAttribute();

	virtual TelephonyResult Initialize();
	virtual int GetMask() const { return 0x80000; }
};

class TelSmsManagerAttributeFactory : public TelAttributeFactory {
public:
	TelSmsManagerAttributeFactory() {}
	virtual ~TelSmsManagerAttributeFactory() {}

	virtual TelephonyResult CreateStores(paf::vector<TelAttributeStore *> *stores);
	virtual TelephonyResult DestroyStores(paf::vector<TelAttributeStore *> *stores);
};

/* ------------------------------------------------------------------
 * SMS receiver, sender, mediator, state machine, and GUI
 * ------------------------------------------------------------------ */

class TelSmsReceiver : public TelObservable {
public:
	TelSmsReceiver(TelBbmcAdapterInterface *bbmcAdapter,
		       TelSmsDbManager *dbManager,
		       TelSmsPduParser *pduParser);
	virtual ~TelSmsReceiver() {}

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult ReceiveNextSms(bool *outQueueEmpty,
					       bool *outMessageCompleted,
					       bool *outRetryNeeded);

private:
	TelephonyResult ReassembleConcatenatedMessage(const TelSmsMessageId &messageId,
						      const TelSmsAddress &sender,
						      int *referenceNumber,
						      unsigned int *totalSegments,
						      int *receivedCount,
						      bool *outCompleted,
						      bool *outRetryNeeded);
	TelephonyResult CheckPendingConcatenatedMessages(bool *outCompleted,
							 bool *outRetryNeeded);

	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelSmsDbManager *m_dbManager;
	TelSmsPduParser *m_pduParser;
	int m_maxStoredMessages;
};

class TelSmsSender : public TelObservable {
public:
	TelSmsSender(TelBbmcAdapterInterface *bbmcAdapter,
		     TelSmsDbManager *dbManager,
		     TelSmsPduGenerator *pduGenerator);
	virtual ~TelSmsSender() {}

	virtual TelephonyResult Initialize() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Terminate() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult SendMessage(const TelSmsMessageId &messageId);

private:
	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelSmsDbManager *m_dbManager;
	TelSmsPduGenerator *m_pduGenerator;
};

/* Observer registration interface exposed by telephony_plugin slot 0 */
class TelSmsLogicObserverInterface {
public:
	virtual ~TelSmsLogicObserverInterface() {}
	virtual void AddSmsObserver(TelObserver *observer) = 0;
	virtual void RemoveSmsObserver(TelObserver *observer) = 0;
};

/* Public SMS API interface exposed by telephony_plugin slot 1 */
class TelSmsLogicApiInterface {
public:
	virtual ~TelSmsLogicApiInterface() {}
	virtual TelephonyResult GetServiceState(int *outState) = 0;
	virtual int SetEventCallback(TelSmsEventCallback callback, void *userData) = 0;
	virtual int SetLegacyCallback(TelSmsLegacyCallback callback, void *userData) = 0;
	virtual TelephonyResult GetDbPath(paf::string *outPath) = 0;
	virtual int SendSms(const TelSmsMessageId &messageId) = 0;
	virtual int ResendSms(const TelSmsMessageId &messageId) = 0;
	virtual int UpdateSmsStatus(const TelSmsMessageId &messageId, int status) = 0;
	virtual int DeleteSmsList(const paf::vector<TelSmsMessageId> &messageIds) = 0;
	virtual int DeleteAllSms() = 0;
	virtual int IsValidMessageBody(const paf::wstring &body) = 0;
};

class TelSmsLogicMediator;
class TelSmsLogicMediatorContext;

/* Action executor driven by TelSmsLogicMediatorSm */
class TelSmsLogicAction {
public:
	TelSmsLogicAction(TelSmsLogicMediator *mediator,
			  TelSmsSender *sender,
			  TelSmsReceiver *receiver);
	virtual ~TelSmsLogicAction();

	virtual void SetCurrentMessageId(const TelSmsMessageId &messageId);
	virtual const TelSmsMessageId &GetCurrentMessageId() const { return m_currentMessageId; }
	virtual void ExecuteSend(const TelSmsMessageId &messageId);
	virtual void OnSendSucceeded(const TelSmsMessageId &messageId) { (void)messageId; }
	virtual void OnSendFailed(const TelSmsMessageId &messageId) { (void)messageId; }
	virtual void ExecuteResend(const TelSmsMessageId &messageId);
	virtual void OnResendSucceeded(const TelSmsMessageId &messageId) { (void)messageId; }
	virtual void OnResendFailed(const TelSmsMessageId &messageId) { (void)messageId; }
	virtual void ExecuteReceive();
	virtual void OnReceiveSucceeded() {}
	virtual void OnReceiveFailed() {}

private:
	TelSmsLogicMediator *m_mediator;
	TelSmsSender *m_sender;
	TelSmsReceiver *m_receiver;
	TelSmsMessageId m_currentMessageId;
};

/* SMC state base class for TelSmsLogicMediatorSm */
class TelSmsLogicMediatorState : public statemap::State {
public:
	TelSmsLogicMediatorState(const char *name, int stateId)
		: statemap::State(name, stateId)
	{
	}
	virtual ~TelSmsLogicMediatorState() {}

	virtual void Entry(TelSmsLogicMediatorContext &context) { (void)context; }
	virtual void Exit(TelSmsLogicMediatorContext &context) { (void)context; }
	virtual void InitializeFinished(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsFailed(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsRequest(TelSmsLogicMediatorContext &context);
	virtual void ReceiveSmsSucceeded(TelSmsLogicMediatorContext &context);
	virtual void ResendSmsFailed(TelSmsLogicMediatorContext &context,
				     const TelSmsMessageId &messageId);
	virtual void ResendSmsRequest(TelSmsLogicMediatorContext &context,
				      const TelSmsMessageId &messageId);
	virtual void ResendSmsSucceeded(TelSmsLogicMediatorContext &context,
					const TelSmsMessageId &messageId);
	virtual void SendSmsFailed(TelSmsLogicMediatorContext &context,
				   const TelSmsMessageId &messageId);
	virtual void SendSmsRequest(TelSmsLogicMediatorContext &context,
				    const TelSmsMessageId &messageId);
	virtual void SendSmsSucceeded(TelSmsLogicMediatorContext &context,
				      const TelSmsMessageId &messageId);
	virtual void Default(TelSmsLogicMediatorContext &context) { (void)context; }
};

class TelSmsLogicMediatorContext : public statemap::FSMContext {
public:
	explicit TelSmsLogicMediatorContext(TelSmsLogicAction &owner);
	virtual ~TelSmsLogicMediatorContext() {}

	virtual void enterStartState();

	TelSmsLogicAction &getOwner() { return m_owner; }
	TelSmsLogicMediatorState &getState()
	{
		return *static_cast<TelSmsLogicMediatorState *>(m_state);
	}

	void InitializeFinished();
	void ReceiveSmsRequest();
	void ReceiveSmsSucceeded();
	void ReceiveSmsFailed();
	void SendSmsRequest(const TelSmsMessageId &messageId);
	void SendSmsSucceeded(const TelSmsMessageId &messageId);
	void SendSmsFailed(const TelSmsMessageId &messageId);
	void ResendSmsRequest(const TelSmsMessageId &messageId);
	void ResendSmsSucceeded(const TelSmsMessageId &messageId);
	void ResendSmsFailed(const TelSmsMessageId &messageId);

private:
	TelSmsLogicAction &m_owner;
};

/* Base mediator contract shared by full and mini SMS mediators */
class TelSmsLogicMediatorBase : public TelObservable,
				public TelObserver,
				public TelSmsLogicObserverInterface,
				public TelSmsLogicApiInterface {
public:
	explicit TelSmsLogicMediatorBase(const char *mutexName)
		: TelObservable(mutexName)
	{
	}
	virtual ~TelSmsLogicMediatorBase() {}

	virtual TelephonyResult Initialize() = 0;
	virtual TelephonyResult Terminate() = 0;

	virtual void AddSmsObserver(TelObserver *observer) { AddObserver(observer); }
	virtual void RemoveSmsObserver(TelObserver *observer) { RemoveObserver(observer); }
};

/* Full SMS mediator (0x812517EC..0x81252C9F) */
class TelSmsLogicMediator : public TelSmsLogicMediatorBase, public TelMessageThread {
public:
	TelSmsLogicMediator(TelSmsSender *sender, TelSmsReceiver *receiver,
			    TelBbmcAdapterInterface *bbmcAdapter,
			    TelSmsDbManager *dbManager);
	virtual ~TelSmsLogicMediator();

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult PostNormalEvent(const TelEventInfo &event);
	virtual TelephonyResult PostUrgentEvent(const TelEventInfo &event);

	virtual TelephonyResult GetServiceState(int *outState);
	virtual int SetEventCallback(TelSmsEventCallback callback, void *userData);
	virtual int SetLegacyCallback(TelSmsLegacyCallback callback, void *userData);
	virtual TelephonyResult GetDbPath(paf::string *outPath);
	virtual int SendSms(const TelSmsMessageId &messageId);
	virtual int ResendSms(const TelSmsMessageId &messageId);
	virtual int UpdateSmsStatus(const TelSmsMessageId &messageId, int status);
	virtual int DeleteSmsList(const paf::vector<TelSmsMessageId> &messageIds);
	virtual int DeleteAllSms();
	virtual int IsValidMessageBody(const paf::wstring &body);

	virtual void OnNotify(const TelEventInfo &event);
	virtual void OnHandleEvent(const TelEventInfo &event);

	void DispatchToListeners(const TelEventInfo &event);

private:
	void HandleDbStatusUpdated(const TelEventInfo &event);
	void HandleDeleteAll();
	void HandleDeleteList(const TelEventInfo &event);
	void HandleResendResult(const TelEventInfo &event);
	void HandleUpdateStatus(const TelEventInfo &event);
	void HandleSendResult(const TelEventInfo &event);

	TelSmsEventCallback m_eventCallback;
	void *m_callbackUserData;
	paf::thread::Mutex m_callbackMutex;
	paf::thread::Mutex m_receiveCountMutex;
	TelSmsLegacyCallback m_legacyCallback;
	void *m_legacyUserData;
	TelSmsSender *m_sender;
	TelSmsReceiver *m_receiver;
	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelSmsDbManager *m_dbManager;
	TelSmsLogicAction *m_action;
	TelSmsLogicMediatorContext *m_stateMachine;
	SceUInt32 m_pendingReceiveCount;
};

/* Minimal stub SMS mediator for Wi-Fi-only / no-WWAN units (0x81252F74..0x812530AF) */
class TelSmsLogicMediatorMini : public TelSmsLogicMediatorBase {
public:
	TelSmsLogicMediatorMini();
	virtual ~TelSmsLogicMediatorMini();

	virtual TelephonyResult Initialize() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult Terminate() { return TelephonyResult(TEL_OK); }
	virtual TelephonyResult GetServiceState(int *outState)
	{
		*outState = 0;
		return TelephonyResult(TEL_OK);
	}
	virtual int SetEventCallback(TelSmsEventCallback callback, void *userData)
	{
		(void)callback;
		(void)userData;
		return 0;
	}
	virtual int SetLegacyCallback(TelSmsLegacyCallback callback, void *userData)
	{
		(void)callback;
		(void)userData;
		return 0;
	}
	virtual TelephonyResult GetDbPath(paf::string *outPath)
	{
		(void)outPath;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	virtual int SendSms(const TelSmsMessageId &messageId)
	{
		(void)messageId;
		return 0;
	}
	virtual int ResendSms(const TelSmsMessageId &messageId)
	{
		(void)messageId;
		return 0;
	}
	virtual int UpdateSmsStatus(const TelSmsMessageId &messageId, int status)
	{
		(void)messageId;
		(void)status;
		return 0;
	}
	virtual int DeleteSmsList(const paf::vector<TelSmsMessageId> &messageIds)
	{
		(void)messageIds;
		return 0;
	}
	virtual int DeleteAllSms() { return 0; }
	virtual int IsValidMessageBody(const paf::wstring &body)
	{
		(void)body;
		return 0;
	}
	virtual void OnNotify(const TelEventInfo &event);
};

/* SMS GUI observer (LSDB notification + Class-0 Flash SMS dialog, 0x81251198..0x812517EB) */
class TelSmsLogicGui : public TelObserver {
public:
	TelSmsLogicGui(TelSmsLogicMediatorBase *smsMediator, TelSmsDbManager *dbManager);
	virtual ~TelSmsLogicGui();

	TelephonyResult Attach();
	TelephonyResult Detach();
	virtual void OnNotify(const TelEventInfo &event);

private:
	TelephonyResult PostSmsNotification(const paf::wstring &messageText);
	void OpenFlashSmsDialog(const TelEventInfo &event);
	void HandleMessageStatusUpdated(const TelEventInfo &event);

	TelSmsLogicMediatorBase *m_smsMediator;
	TelSmsDbManager *m_dbManager;
};

/* ------------------------------------------------------------------
 * General logic interfaces, GUI dialog manager, and NetCtl factory
 * ------------------------------------------------------------------ */

/* Flat event record forwarded to legacy C-style general event callbacks */
struct TelGeneralLegacyEventRecord {
	int category;
	int eventId;
	int eventArg;
	int eventResult;
};

typedef void (*TelGeneralEventCallback)(const TelEventInfo &event, void *userData);
typedef void (*TelGeneralLegacyCallback)(const TelGeneralLegacyEventRecord *record, void *userData);

/* Condition error codes reported by GetConditionError (0x81516014) */
enum TelGeneralConditionError {
	TEL_COND_ERR_COM_FAILURE = -2146424655,		/* 0x801028B1 */
	TEL_COND_ERR_SIM_DISABLED = -2146424654,	/* 0x801028B2 */
	TEL_COND_ERR_SIM_BLOCKED = -2146424653,		/* 0x801028B3 */
	TEL_COND_ERR_HIGH_TEMP = -2146424652,		/* 0x801028B4 */
	TEL_COND_ERR_SIM_UNUSABLE = -2146424651,	/* 0x801028B5 */
	TEL_COND_ERR_COM_BOOT_TEMP = -2146424650,	/* 0x801028B6 */
	TEL_COND_ERR_LOW_BATTERY = -2146424649,		/* 0x801028B7 */
	TEL_COND_ERR_COM_BOOT_OTHER = -2146424648,	/* 0x801028B8 */
	TEL_COND_ERR_ATTRIBUTE = -2146424646,		/* 0x801028BA */
	TEL_COND_ERR_BBMC = -2146424645			/* 0x801028BB */
};

/* NetCtl / product operator spec error codes */
enum TelNetCtlErrorCode {
	TEL_NETCTL_ERR_DENIED = -2136014844,		/* 0x80AF0004 */
	TEL_NETCTL_ERR_INVALID_SERVICE = -2136014705	/* 0x80AF008F */
};

/* Product target operator identifiers (TEL_ATTR_PRODUCT_TARGET_OPERATOR = 0x60002) */
enum TelProductTargetOperator {
	TEL_OPERATOR_DEFAULT = 0,
	TEL_OPERATOR_ATT = 1,
	TEL_OPERATOR_DOCOMO = 2,
	TEL_OPERATOR_VODAFONE = 4,
	TEL_OPERATOR_CANADA = 5,
	TEL_OPERATOR_REGION_ALT = 6
};

class TelGeneralLogicNetCtl : public TelObservable {
public:
	TelGeneralLogicNetCtl() : TelObservable("SceTelGenProductOpSpecIf") {}
	virtual ~TelGeneralLogicNetCtl() {}

	virtual TelephonyResult SetBbmcAdapter(TelBbmcAdapterInterface *bbmcAdapter) = 0;
	virtual TelephonyResult Initialize() = 0;
	virtual TelephonyResult SetSimActivationState(const int *activationState) = 0;
	virtual TelephonyResult GetSimActivationState(int *outActivationState) = 0;
	virtual bool IsSimCardMatchOperator(const TelephonyPlmn &plmn) = 0;
	virtual TelephonyResult IsApnMatchOperator(const paf::vector<TelephonyPlmn> &plmns,
						   bool *outMatched) = 0;
	virtual TelephonyResult IsOperatorRestrictedApn(const paf::vector<TelephonyPlmn> &plmns,
							bool *outRestricted) = 0;
	virtual TelephonyResult UpdateRoamingOverride() = 0;
	virtual TelephonyResult IsDomesticRoamingAllowed(bool *outAllowed) = 0;
	virtual TelephonyResult IsApnEditAllowed(bool *outAllowed) = 0;
	virtual TelephonyResult IsManualOperatorSelectionAllowed(bool *outAllowed) = 0;
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId) = 0;
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType) = 0;
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes) = 0;
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType) = 0;
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl) = 0;
};

class TelGeneralLogicNetCtlFactory {
public:
	static TelephonyResult Create(TelBbmcAdapterInterface *bbmcAdapter,
				      TelGeneralLogicNetCtl **outNetCtl);
	static TelephonyResult Destroy(TelGeneralLogicNetCtl *netCtl);
};

/* Shared product operator utilities (0x81260038..0x81260677) */
TelephonyResult telGetProductTargetOperator(int *outOperator);
TelephonyResult telLoadSupportedOperatorPlmns(paf::vector<TelephonyPlmn> *outPlmns);
bool telIsPlmnSupportedByOperator(int operatorId, const TelephonyPlmn &plmn);
bool telIsApnInOperatorPlmnList(const paf::string &apnName,
				const paf::vector<TelephonyPlmn> &plmns);

class TelGeneralLogicMediatorBase : public TelObservable {
public:
	explicit TelGeneralLogicMediatorBase(const char *mutexName)
		: TelObservable(mutexName)
	{
	}
	virtual ~TelGeneralLogicMediatorBase() {}

	virtual TelephonyResult Initialize() = 0;
	virtual TelephonyResult Terminate() = 0;
	virtual TelephonyResult StartLogic() = 0;
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId) = 0;
	virtual TelephonyModem *GetModemInterface() = 0;
	virtual TelephonyService *GetServiceInterface() = 0;
};

/* Full & mini general logic mediators (implemented in src/general/) */
TelGeneralLogicMediatorBase *telCreateGeneralLogicMediator(TelBbmcAdapterInterface *bbmcAdapter,
							   TelGeneralLogicNetCtl *netCtl);
TelGeneralLogicMediatorBase *telCreateGeneralLogicMediatorMini();

/* Dialog manager embedded in TelGeneralLogicGui (0x81254AC0..0x81255773) */
class TelGeneralLogicDialogManager : public TelObservable {
public:
	TelGeneralLogicDialogManager();
	virtual ~TelGeneralLogicDialogManager();

	TelephonyResult ShowDialog(int dialogType);
	void OnDialogButtonPressed(int dialogId, int buttonId);

	static TelGeneralLogicDialogManager *GetInstance();

private:
	paf::map<int, int> m_activeDialogs;
};

/* General logic GUI observer (0x8124EE24..0x8124F5D7) */
class TelGeneralLogicGui : public TelObserver {
public:
	explicit TelGeneralLogicGui(TelGeneralLogicMediatorBase *generalMediator);
	virtual ~TelGeneralLogicGui();

	TelephonyResult Attach();
	TelephonyResult Detach();
	virtual void OnNotify(const TelEventInfo &event);

private:
	void PostComUpdateNotification();
	void HandleMmRejectCause(const TelEventInfo &event);
	void HandleDialogAccepted(const TelEventInfo &event);
	void ShowErrorDialog(const TelEventInfo &event);

	TelGeneralLogicMediatorBase *m_generalMediator;
	bool m_simNotReadyShown;
	TelGeneralLogicDialogManager m_dialogManager;
};

/* ------------------------------------------------------------------
 * Telephony logic factory (0x812506FC..0x81251197)
 * ------------------------------------------------------------------ */

enum TelFactoryMode {
	TEL_FACTORY_MODE_MINI = 0,
	TEL_FACTORY_MODE_BBMC = 1,
	TEL_FACTORY_MODE_LOOPBACK = 2
};

class TelLogicFactory {
public:
	TelLogicFactory();
	virtual ~TelLogicFactory();

	TelephonyResult CreateAndStart(int mode,
				       TelSmsLogicObserverInterface **outSmsObserver,
				       TelSmsLogicApiInterface **outSmsApi,
				       TelephonyModem **outGeneralModem,
				       TelephonyService **outGeneralService,
				       int useLegacyDbPath);
	TelephonyResult StopAndDestroy();

private:
	TelephonyResult CreateMini(int mode,
				   TelSmsLogicObserverInterface **outSmsObserver,
				   TelSmsLogicApiInterface **outSmsApi,
				   TelephonyModem **outGeneralModem,
				   TelephonyService **outGeneralService);
	TelephonyResult StartAll();
	TelephonyResult StopAll();

	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelSmsDbManager *m_dbManager;
	TelSmsLogicMediator *m_smsMediator;
	TelSmsPduParser *m_pduParser;
	TelSmsPduGenerator *m_pduGenerator;
	TelSmsSender *m_smsSender;
	TelSmsReceiver *m_smsReceiver;
	TelGeneralLogicMediatorBase *m_generalMediator;
	TelGeneralLogicNetCtl *m_netCtl;
	TelGeneralLogicGui *m_generalGui;
	TelGeneralLogicMediatorBase *m_generalMediatorMini;
	TelSmsLogicMediatorMini *m_smsMediatorMini;
	TelSmsLogicGui *m_smsGui;
};

/* Full telephony_plugin interface table returned by GetInterface(1) (0x81545648) */
struct TelephonyPluginFullInterface {
	TelSmsLogicObserverInterface *(*getSmsObserver)(void);
	TelSmsLogicApiInterface *(*getSmsApi)(void);
	TelephonyModem *(*getGeneralModem)(void);
	TelephonyService *(*getGeneralService)(void);
};

} /* namespace telephony */

#endif /* TELEPHONY_INTERNAL_H */
