/*
 * shell_svc_telephony.cpp: SceShellSvc Telephony IPMI channel 0x00050000
 * (86 sync methods 0x050000..0x050055, per-session General/SMS event queues
 * with notify flags 22/23, disconnect cleanup, and SceTelephony client helpers)
 * (3.65-CEX: 0x81269C00..0x8126E4EB).
 */
#include <stddef.h>
#include <stdint.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <scetypes.h>
#include <plugin.h>
#include <thread/thread.h>
#include <ipmi/session.h>
#include <ipmi/client.h>
#include "../../telephony_plugin/src/general/tel_general_internal.h"

extern int shellSvcNotifyEvent(IPMI::Session *session, int eventIndex);

extern "C" {
IPMI::Client *SceShellSvc_B31E7F1C(void);
}

using namespace telephony;

namespace {

static const SceSize kMaxSessionEventQueue = 32;
static const SceSize kMaxSubscribedSessions = 4;
static const int kGeneralEventNotifyBit = 22;
static const int kSmsEventNotifyBit = 23;
static const int kErrTooManyClients = -2146424815;	/* 0x80102011 */
static const int kErrInvalidPointer = -2146424823;	/* 0x80102009 */
static const int kErrUnsupportedMethod = -2147351103;	/* 0x800205C1 */
static const char *s_fallbackUpdateListUrl =
	"http://upd.bf1.jp/update3/psp2-updatelist-err.xml";

/* Wire payload for SMS update-status request (0x050017) */
struct TelIpcSmsUpdateStatusReq {
	SceUInt64 messageId;
	int status;
};

/* Wire payload for network registration info (0x05002D, 0x050043) */
struct TelIpcNetRegistrationInfo {
	int mcc;
	int mnc;
	int networkType;
	int roamingState;
};

/* Wire payload for CheckImsiForOperatorCode (0x05004F) */
struct TelIpcCheckImsiReq {
	int operatorCode;
	int matched;
};

static paf::thread::Mutex s_generalEventMutex("SceTelIpcGenEv", 0);
static paf::map<IPMI::Session *, paf::vector<TelGeneralLegacyEventRecord> > s_generalEventQueues;

static paf::thread::Mutex s_smsEventMutex("SceTelIpcSmsEv", 0);
static paf::map<IPMI::Session *, paf::vector<TelSmsLegacyEventRecord> > s_smsEventQueues;

static paf::thread::Mutex s_clientMutex("SceTelephonyClientMutex", 0);

static const TelephonyPluginFullInterface *telGetPluginInterface()
{
	paf::Plugin *plugin = paf::Plugin::Find("telephony_plugin");
	if (plugin == NULL)
		return NULL;
	return static_cast<const TelephonyPluginFullInterface *>(plugin->GetInterface(1));
}

static TelGeneralLogicServiceInterface *telGetGeneralService()
{
	const TelephonyPluginFullInterface *iface = telGetPluginInterface();
	if (iface == NULL || iface->getGeneralService == NULL)
		return NULL;
	return reinterpret_cast<TelGeneralLogicServiceInterface *>(iface->getGeneralService());
}

static TelSmsLogicApiInterface *telGetSmsApi()
{
	const TelephonyPluginFullInterface *iface = telGetPluginInterface();
	if (iface == NULL || iface->getSmsApi == NULL)
		return NULL;
	return iface->getSmsApi();
}

/* 0x81269C00: fan out general telephony legacy events to all subscribed IPMI sessions */
static void telOnGeneralLegacyEvent(const TelGeneralLegacyEventRecord *record, void *userData)
{
	(void)userData;
	if (record == NULL)
		return;

	s_generalEventMutex.Lock();
	for (paf::map<IPMI::Session *, paf::vector<TelGeneralLegacyEventRecord> >::iterator it =
		     s_generalEventQueues.begin();
	     it != s_generalEventQueues.end(); ++it) {
		if (it->second.size() < kMaxSessionEventQueue)
			it->second.push_back(*record);
		shellSvcNotifyEvent(it->first, kGeneralEventNotifyBit);
	}
	s_generalEventMutex.Unlock();
}

/* 0x81269CF2: fan out SMS legacy events to all subscribed IPMI sessions */
static void telOnSmsLegacyEvent(const TelSmsLegacyEventRecord *record, void *userData)
{
	(void)userData;
	if (record == NULL)
		return;

	s_smsEventMutex.Lock();
	for (paf::map<IPMI::Session *, paf::vector<TelSmsLegacyEventRecord> >::iterator it =
		     s_smsEventQueues.begin();
	     it != s_smsEventQueues.end(); ++it) {
		if (it->second.size() < kMaxSessionEventQueue)
			it->second.push_back(*record);
		shellSvcNotifyEvent(it->first, kSmsEventNotifyBit);
	}
	s_smsEventMutex.Unlock();
}

/* 0x81269DF0: unregister session from both general & SMS event fan-out maps */
static int telUnregisterSession(IPMI::Session *session)
{
	s_generalEventMutex.Lock();
	paf::map<IPMI::Session *, paf::vector<TelGeneralLegacyEventRecord> >::iterator genIt =
		s_generalEventQueues.find(session);
	if (genIt != s_generalEventQueues.end())
		s_generalEventQueues.erase(genIt);
	if (s_generalEventQueues.size() == 0) {
		TelGeneralLogicServiceInterface *svc = telGetGeneralService();
		if (svc != NULL)
			svc->SetLegacyCallback(NULL, NULL);
	}
	s_generalEventMutex.Unlock();

	s_smsEventMutex.Lock();
	paf::map<IPMI::Session *, paf::vector<TelSmsLegacyEventRecord> >::iterator smsIt =
		s_smsEventQueues.find(session);
	if (smsIt != s_smsEventQueues.end())
		s_smsEventQueues.erase(smsIt);
	if (s_smsEventQueues.size() == 0) {
		TelSmsLogicApiInterface *sms = telGetSmsApi();
		if (sms != NULL)
			sms->SetLegacyCallback(NULL, NULL);
	}
	s_smsEventMutex.Unlock();
	return 0;
}

/* 0x81269F66: register session (max 4 concurrent subscribers) */
static int telRegisterSession(IPMI::Session *session)
{
	if (telGetPluginInterface() == NULL)
		return TEL_ERR_NOT_FOUND;

	int err = 0;

	s_generalEventMutex.Lock();
	if (s_generalEventQueues.size() == 0) {
		TelGeneralLogicServiceInterface *svc = telGetGeneralService();
		if (svc == NULL) {
			s_generalEventMutex.Unlock();
			telUnregisterSession(session);
			return TEL_ERR_NOT_FOUND;
		}
		err = svc->SetLegacyCallback(telOnGeneralLegacyEvent, NULL).GetCode();
		if (err != 0) {
			s_generalEventMutex.Unlock();
			telUnregisterSession(session);
			return err;
		}
	} else {
		if (s_generalEventQueues.find(session) != s_generalEventQueues.end()) {
			s_generalEventMutex.Unlock();
			return 0;
		}
		if (s_generalEventQueues.size() >= kMaxSubscribedSessions) {
			s_generalEventMutex.Unlock();
			telUnregisterSession(session);
			return kErrTooManyClients;
		}
	}
	s_generalEventQueues[session].reserve(kMaxSessionEventQueue);
	s_generalEventMutex.Unlock();

	s_smsEventMutex.Lock();
	if (s_smsEventQueues.size() == 0) {
		TelSmsLogicApiInterface *sms = telGetSmsApi();
		if (sms == NULL) {
			s_smsEventMutex.Unlock();
			telUnregisterSession(session);
			return TEL_ERR_NOT_FOUND;
		}
		err = sms->SetLegacyCallback(telOnSmsLegacyEvent, NULL);
		if (err != 0) {
			s_smsEventMutex.Unlock();
			telUnregisterSession(session);
			return err;
		}
	} else {
		if (s_smsEventQueues.find(session) != s_smsEventQueues.end()) {
			s_smsEventMutex.Unlock();
			return 0;
		}
		if (s_smsEventQueues.size() >= kMaxSubscribedSessions) {
			s_smsEventMutex.Unlock();
			telUnregisterSession(session);
			return kErrTooManyClients;
		}
	}
	s_smsEventQueues[session].reserve(kMaxSessionEventQueue);
	s_smsEventMutex.Unlock();
	return 0;
}

/* 0x8126A206: pop next general legacy event for session */
static int telPopGeneralEvent(IPMI::Session *session, void *outRecord, int *outRemaining)
{
	int err = TEL_ERR_NOT_FOUND;
	if (outRemaining != NULL)
		*outRemaining = 0;

	s_generalEventMutex.Lock();
	paf::map<IPMI::Session *, paf::vector<TelGeneralLegacyEventRecord> >::iterator it =
		s_generalEventQueues.find(session);
	if (it != s_generalEventQueues.end() && !it->second.empty()) {
		if (outRecord != NULL) {
			sce_paf_memcpy(outRecord, &it->second[0],
				       sizeof(TelGeneralLegacyEventRecord));
		}
		it->second.erase(it->second.begin());
		if (outRemaining != NULL)
			*outRemaining = (int)it->second.size();
		err = 0;
	}
	s_generalEventMutex.Unlock();
	return err;
}

/* 0x8126A2EC: pop next SMS legacy event for session */
static int telPopSmsEvent(IPMI::Session *session, void *outRecord, int *outRemaining)
{
	int err = TEL_ERR_NOT_FOUND;
	if (outRemaining != NULL)
		*outRemaining = 0;

	s_smsEventMutex.Lock();
	paf::map<IPMI::Session *, paf::vector<TelSmsLegacyEventRecord> >::iterator it =
		s_smsEventQueues.find(session);
	if (it != s_smsEventQueues.end() && !it->second.empty()) {
		if (outRecord != NULL) {
			sce_paf_memcpy(outRecord, &it->second[0],
				       sizeof(TelSmsLegacyEventRecord));
		}
		it->second.erase(it->second.begin());
		if (outRemaining != NULL)
			*outRemaining = (int)it->second.size();
		err = 0;
	}
	s_smsEventMutex.Unlock();
	return err;
}

static void telCopyStringField(void *dst, SceSize dstSize, const paf::string &src)
{
	if (dst == NULL || dstSize == 0)
		return;
	sce_paf_memset(dst, 0, dstSize);
	sce_paf_strncpy(static_cast<char *>(dst), src.c_str(), dstSize - 1);
}

static void telPackNetRegistration(const TelNetworkInfo &netInfo, void *outBuf)
{
	if (outBuf == NULL)
		return;
	TelIpcNetRegistrationInfo packed;
	sce_paf_memset(&packed, 0, sizeof(packed));
	TelephonyPlmn plmn;
	netInfo.GetPlmn(&plmn);
	packed.mcc = plmn.GetMcc();
	packed.mnc = plmn.GetMnc();
	netInfo.GetMappedNetworkType(&packed.networkType);
	netInfo.GetMappedRoamingState(&packed.roamingState);
	sce_paf_memcpy(outBuf, &packed, sizeof(packed));
}

/* ------------------------------------------------------------------
 * Per-method Telephony IPMI handlers (0x8126A1FE, 0x8126A3DA..0x8126D1E0)
 * ------------------------------------------------------------------ */

/* 0x8126A1FE */
static int telIpcUnregisterSession(IPMI::Session *session)
{
	return telUnregisterSession(session);
}

/* 0x8126A3DA */
static int telIpcGetGeneralLogicState(int *outState)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outState == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetGeneralLogicState(outState).GetCode();
}

/* 0x8126A45A */
static int telIpcSmsGetServiceState(int *outState)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL || outState == NULL)
		return TEL_ERR_NOT_FOUND;
	return sms->GetServiceState(outState).GetCode();
}

/* 0x8126A49A */
static int telIpcSetFlightMode(int flightMode)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetFlightMode(flightMode).GetCode();
}

/* 0x8126A516 */
static int telIpcGetFlightMode(int *outEnabled)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outEnabled == NULL)
		return TEL_ERR_NOT_FOUND;
	bool enabled = false;
	int result = svc->GetFlightMode(&enabled).GetCode();
	*outEnabled = enabled ? 1 : 0;
	return result;
}

/* 0x8126A59C */
static int telIpcSetEnableWwan(int enableWwan)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetEnableWwan(enableWwan != 0).GetCode();
}

/* 0x8126A618 */
static int telIpcGetEnableWwan(int *outEnabled)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outEnabled == NULL)
		return TEL_ERR_NOT_FOUND;
	bool enabled = false;
	int result = svc->GetEnableWwan(&enabled).GetCode();
	*outEnabled = enabled ? 1 : 0;
	return result;
}

/* 0x8126A696 */
static int telIpcGetPhoneNumber(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	TelPhoneNumber number;
	int result = svc->GetPhoneNumber(&number).GetCode();
	telCopyStringField(outBuf, 33, paf::string(number.GetDigits()));
	return result;
}

/* 0x8126A742 */
static int telIpcGetImei(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string imei;
	int result = svc->GetImei(&imei).GetCode();
	telCopyStringField(outBuf, 33, imei);
	return result;
}

/* 0x8126A7F2 */
static int telIpcGetIccid(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string iccid;
	int result = svc->GetIccid(&iccid).GetCode();
	telCopyStringField(outBuf, 21, iccid);
	return result;
}

/* 0x8126A8A2 */
static int telIpcVerifyPin(const char *pinText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string pinStr(pinText);
	TelPinCode pin(pinStr);
	return svc->VerifyPin(pin).GetCode();
}

/* 0x8126A968 */
static int telIpcUnblockPin(const char *pukText, const char *newPinText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string pukStr(pukText);
	paf::string newPinStr(newPinText);
	TelUnblockPinInfo unblock(pukStr, newPinStr);
	return svc->UnblockPin(unblock).GetCode();
}

/* 0x8126AA8A */
static int telIpcSetPinProtection(int enableProtection, const char *pinText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string pinStr(pinText);
	TelPinCode pin(pinStr);
	return svc->SetPinProtection(enableProtection != 0, pin).GetCode();
}

/* 0x8126AB66 */
static int telIpcGetPinProtection(int *outEnabled)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outEnabled == NULL)
		return TEL_ERR_NOT_FOUND;
	bool enabled = false;
	int result = svc->GetPinProtection(&enabled).GetCode();
	*outEnabled = enabled ? 1 : 0;
	return result;
}

/* 0x8126ABE4 */
static int telIpcGetPinStatus(int *outStatus)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outStatus == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetPinStatus(outStatus).GetCode();
}

/* 0x8126AC60 */
static int telIpcGetPinRemain(int *outRemain)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRemain == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetPinRemain(outRemain).GetCode();
}

/* 0x8126ACDE */
static int telIpcChangePinCode(const char *oldPinText, const char *newPinText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string oldPinStr(oldPinText);
	paf::string newPinStr(newPinText);
	TelPinCode oldPin(oldPinStr);
	TelPinCode newPin(newPinStr);
	return svc->ChangePinCode(oldPin, newPin).GetCode();
}

/* 0x8126AE00 */
static int telIpcSmsGetDbPath(void *outBuf)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string dbPath;
	int result = sms->GetDbPath(&dbPath).GetCode();
	telCopyStringField(outBuf, 128, dbPath);
	return result;
}

/* 0x8126AEAE */
static int telIpcSmsSendSms(SceUInt64 rawMessageId)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL)
		return TEL_ERR_NOT_FOUND;
	TelSmsMessageId msgId(rawMessageId);
	return sms->SendSms(msgId);
}

/* 0x8126AF32 */
static int telIpcSmsResendSms(SceUInt64 rawMessageId)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL)
		return TEL_ERR_NOT_FOUND;
	TelSmsMessageId msgId(rawMessageId);
	return sms->ResendSms(msgId);
}

/* 0x8126AFB6 */
static int telIpcSmsUpdateSmsStatus(const TelIpcSmsUpdateStatusReq *req)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL || req == NULL)
		return TEL_ERR_NOT_FOUND;
	TelSmsMessageId msgId(req->messageId);
	return sms->UpdateSmsStatus(msgId, req->status);
}

/* 0x8126B040 */
static int telIpcSmsDeleteSmsList(const SceUInt64 *rawIds, SceSize count)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::vector<TelSmsMessageId> ids;
	for (SceSize idx = 0; idx < count; ++idx)
		ids.push_back(TelSmsMessageId(rawIds[idx]));
	return sms->DeleteSmsList(ids);
}

/* 0x8126B144 */
static int telIpcSmsDeleteAllSms(void)
{
	TelSmsLogicApiInterface *sms = telGetSmsApi();
	if (sms == NULL)
		return TEL_ERR_NOT_FOUND;
	return sms->DeleteAllSms();
}

/* 0x8126B184 */
static int telIpcSetApnInfoTemp(const char *rawRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	TelephonyOperator apnOp;
	apnOp.SetFromRawRecord(rawRecord);
	return svc->SetApnInfo(apnOp, false).GetCode();
}

/* 0x8126B21C */
static int telIpcSetApnInfoSave(const char *rawRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	TelephonyOperator apnOp;
	apnOp.SetFromRawRecord(rawRecord);
	return svc->SetApnInfo(apnOp, true).GetCode();
}

/* 0x8126B2B4 */
static int telIpcSetApnInfoWithBillingPlan(const TelOperatorApnRawRecord *rawRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	TelOperatorApnInfo apnInfo;
	apnInfo.UnpackFromRaw(rawRecord);
	return svc->SetApnInfoWithBillingPlan(apnInfo, true).GetCode();
}

/* 0x8126B34C */
static int telIpcGetUnsupportedProfile(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetUnsupportedProfile().GetCode();
}

/* 0x8126B3C0 */
static int telIpcRestoreApnInfo(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->RestoreApnInfo().GetCode();
}

/* 0x8126B434 */
static int telIpcGetMccMncOfImsi(int *outPair)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outPair == NULL)
		return TEL_ERR_NOT_FOUND;
	TelephonyPlmn plmn;
	int result = svc->GetMccMncOfImsi(&plmn).GetCode();
	outPair[0] = plmn.GetMcc();
	outPair[1] = plmn.GetMnc();
	return result;
}

/* 0x8126B4F4 */
static int telIpcGetProductTargetOperator(int *outOperator)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outOperator == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetProductTargetOperator(outOperator).GetCode();
}

/* 0x8126B574 */
static int telIpcGetOperatorProfileCount(int *outCount)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outCount == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetOperatorProfileCount(outCount).GetCode();
}

/* 0x8126B604 */
static int telIpcGetOperatorProfile(int index, TelOperatorProfileRawRecord *outRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRecord == NULL)
		return TEL_ERR_NOT_FOUND;
	TelOperatorProfile profile;
	int result = svc->GetOperatorProfile(index, &profile).GetCode();
	profile.PackToRaw(outRecord);
	return result;
}

/* 0x8126B6A6 */
static int telIpcGetApnInfoListCount(int *outCount)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outCount == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetApnInfoListCount(outCount).GetCode();
}

/* 0x8126B72A */
static int telIpcGetApnInfoListItem(int index, TelOperatorApnRawRecord *outRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRecord == NULL)
		return TEL_ERR_NOT_FOUND;
	TelOperatorApnInfo apnItem;
	int result = svc->GetApnInfoListItem(index, &apnItem).GetCode();
	apnItem.PackToRaw(outRecord);
	return result;
}

/* 0x8126B7CC */
static int telIpcGetCurrentApnInfo(TelOperatorApnRawRecord *outRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRecord == NULL)
		return TEL_ERR_NOT_FOUND;
	TelephonyOperator apnOp;
	int result = svc->GetCurrentApnInfo(&apnOp).GetCode();
	TelOperatorApnInfo apnInfo(apnOp);
	apnInfo.PackToRaw(outRecord);
	return result;
}

/* 0x8126B868 */
static int telIpcRemoveCurrentApnInfo(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->RemoveCurrentApnInfo().GetCode();
}

/* 0x8126B8DE */
static int telIpcStartNetworkScan(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->StartNetworkScan().GetCode();
}

/* 0x8126B954 */
static int telIpcAbortNetworkScan(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->AbortNetworkScan().GetCode();
}

/* 0x8126B9CA */
static int telIpcGetNetworkScanInfoCount(int *outCount)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outCount == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetNetworkScanInfoCount(outCount).GetCode();
}

/* 0x8126BA50 */
static int telIpcGetNetworkScanInfo(int index, TelNetScanRawRecord *outRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRecord == NULL)
		return TEL_ERR_NOT_FOUND;
	TelNetScanInfo scanItem;
	int result = svc->GetNetworkScanInfo(index, &scanItem).GetCode();
	scanItem.PackToRaw(outRecord);
	return result;
}

/* 0x8126BAF2 */
static int telIpcSetNetRegisterAuto(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetNetRegisterAuto().GetCode();
}

/* 0x8126BB68 */
static int telIpcSetNetRegisterInfo(const TelNetScanRawRecord *rawRecord)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	TelNetScanInfo scanTarget;
	scanTarget.UnpackFromRaw(rawRecord);
	return svc->SetNetRegisterInfo(scanTarget).GetCode();
}

/* 0x8126BBF8 */
static int telIpcGetNetRegistrationInfo(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	TelNetworkInfo netInfo;
	int result = svc->GetNetRegistrationInfo(&netInfo).GetCode();
	telPackNetRegistration(netInfo, outBuf);
	return result;
}

/* 0x8126BCDC */
static int telIpcGetSavedNetworkSelection(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	TelNetworkInfo netInfo;
	int result = svc->GetSavedNetworkSelection(&netInfo).GetCode();
	telPackNetRegistration(netInfo, outBuf);
	return result;
}

/* 0x8126BDC0 */
static int telIpcSetRegistrationAppNetMode(int netMode)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetRegistrationAppNetMode(netMode).GetCode();
}

/* 0x8126BE3A */
static int telIpcSetDataRoamingPermission(int allowRoaming)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetDataRoamingPermission(allowRoaming != 0).GetCode();
}

/* 0x8126BEB8 */
static int telIpcGetDataRoamingPermission(int *outAllowed)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outAllowed == NULL)
		return TEL_ERR_NOT_FOUND;
	bool allowed = false;
	int result = svc->GetDataRoamingPermission(&allowed).GetCode();
	*outAllowed = allowed ? 1 : 0;
	return result;
}

/* 0x8126BF40 */
static int telIpcSetDataRoamingDefinition(int definition)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetDataRoamingDefinition(definition).GetCode();
}

/* 0x8126BFBE */
static int telIpcCheckSimCondition(int *outCondition)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outCondition == NULL)
		return TEL_ERR_NOT_FOUND;
	*outCondition = svc->CheckSimCondition().GetCode();
	return 0;
}

/* 0x8126C036 */
static int telIpcGetControlKeyState(int *outState)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outState == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetControlKeyState(outState).GetCode();
}

/* 0x8126C0B0 */
static int telIpcGetControlKeyBlockType(int *outBlockType)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBlockType == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetControlKeyBlockType(outBlockType).GetCode();
}

/* 0x8126C12A */
static int telIpcGetNetworkStatusReport(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string report;
	return svc->GetNetworkStatusReport(&report).GetCode();
}

/* 0x8126C1A6 */
static int telIpcGetDeviceInfoReport(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string report;
	return svc->GetDeviceInfoReport(&report).GetCode();
}

/* 0x8126C21C */
static int telIpcGetIsComExist(int *outExists)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outExists == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetIsComExist(outExists).GetCode();
}

/* 0x8126C296 */
static int telIpcGetIsApnInit(int *outInitialized)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outInitialized == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetIsApnInit(outInitialized).GetCode();
}

/* 0x8126C310 */
static int telIpcIsOpenSimModel(int *outOpenModel)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outOpenModel == NULL)
		return TEL_ERR_NOT_FOUND;
	bool openModel = false;
	int result = svc->IsOpenSimModel(&openModel).GetCode();
	*outOpenModel = openModel ? 1 : 0;
	return result;
}

/* 0x8126C38A */
static int telIpcGetNetworkSelectionMode(int *outMode)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outMode == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetNetworkSelectionMode(outMode).GetCode();
}

/* 0x8126C404 */
static int telIpcGetConditionError(int *outErrorCode)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outErrorCode == NULL)
		return TEL_ERR_NOT_FOUND;
	TelephonyResult condErr(TEL_OK);
	int result = svc->GetConditionError(&condErr).GetCode();
	*outErrorCode = condErr.GetCode();
	return result;
}

/* 0x8126C49C */
static int telIpcDeactivateSimLock(const char *keyText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string keyStr(keyText);
	TelCarrierLockKey key(keyStr);
	return svc->DeactivateSimLock(key).GetCode();
}

/* 0x8126C570 */
static int telIpcUnblockSimLock(const char *keyText)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string keyStr(keyText);
	TelControlKey key(keyStr);
	return svc->UnblockSimLock(key).GetCode();
}

/* 0x8126C644 */
static int telIpcGetSimLockDeactivateRemain(int *outRemain)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRemain == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetSimLockDeactivateRemain(outRemain).GetCode();
}

/* 0x8126C6BE */
static int telIpcGetSimLockUnblockRemain(int *outRemain)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outRemain == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetSimLockUnblockRemain(outRemain).GetCode();
}

/* 0x8126C738 */
static int telIpcGetCurrentSignalLevel(int *outLevel)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outLevel == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetCurrentSignalLevel(outLevel).GetCode();
}

/* 0x8126C7B6 */
static int telIpcSetBillingPlan(int billingPlan)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetBillingPlan(&billingPlan).GetCode();
}

/* 0x8126C832 */
static int telIpcGetBillingPlan(int *outPlan)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outPlan == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetBillingPlan(outPlan).GetCode();
}

/* 0x8126C8B0 */
static int telIpcSetDebugLogEnabled(int enabled)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetDebugLogEnabled(enabled != 0).GetCode();
}

/* 0x8126C934 */
static int telIpcReadDebugLogLine(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string line;
	int result = svc->ReadDebugLogLine(&line).GetCode();
	telCopyStringField(outBuf, 257, line);
	return result;
}

/* 0x8126C9EA */
static int telIpcSetDebugDiagFlag(int diagFlag)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetDebugDiagFlag(&diagFlag).GetCode();
}

/* 0x8126CA64 */
static int telIpcGetDebugDiagFlag(int *outDiagFlag)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outDiagFlag == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetDebugDiagFlag(outDiagFlag).GetCode();
}

/* 0x8126CADE */
static int telIpcSetDebugForceFlag(int forceFlag)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->SetDebugForceFlag(&forceFlag).GetCode();
}

/* 0x8126CB84 */
static int telIpcGetDebugForceFlag(int *outForceFlag)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outForceFlag == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetDebugForceFlag(outForceFlag).GetCode();
}

/* 0x8126CC30 */
static int telIpcIsSimActivated(int *outActivated)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outActivated == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->IsSimActivated(outActivated).GetCode();
}

/* 0x8126CCAA */
static int telIpcGetLatestArchiveVersion(SceUInt32 *outVersion)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outVersion == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetLatestArchiveVersion(outVersion).GetCode();
}

/* 0x8126CD24 */
static int telIpcGetArchiveVersion(SceUInt32 *outVersion)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outVersion == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetArchiveVersion(outVersion).GetCode();
}

/* 0x8126CD9E */
static int telIpcCheckComVersion(int *outMatches)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outMatches == NULL)
		return TEL_ERR_NOT_FOUND;
	bool matches = false;
	int result = svc->CheckComVersion(&matches).GetCode();
	*outMatches = matches ? 1 : 0;
	return result;
}

/* 0x8126CE34 */
static int telIpcReleasePdpContext(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->ReleasePdpContext().GetCode();
}

/* 0x8126CEB0 */
static int telIpcCheckImsiForOperatorCode(const TelIpcCheckImsiReq *req)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || req == NULL)
		return TEL_ERR_NOT_FOUND;
	int opCode = req->operatorCode;
	bool matched = false;
	return svc->CheckImsiForOperatorCode(&opCode, &matched).GetCode();
}

/* 0x8126CF28 */
static int telIpcStartApnListDownload(void)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->StartApnListDownload().GetCode();
}

/* 0x8126CFA4 */
static int telIpcCheckContentDownloadAllowed(int serviceType)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->CheckContentDownloadAllowed(serviceType).GetCode();
}

/* 0x8126D01E */
static int telIpcGetDownloadSizeLimit(unsigned int serviceType, SceInt64 *outLimitBytes)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outLimitBytes == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->GetDownloadSizeLimit(serviceType, outLimitBytes).GetCode();
}

/* 0x8126D0B4 */
static int telIpcCheckRoamingDownloadAllowed(unsigned int serviceType)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL)
		return TEL_ERR_NOT_FOUND;
	return svc->CheckRoamingDownloadAllowed(serviceType).GetCode();
}

/* 0x8126D12E */
static int telIpcGetModemFirmwareVersion(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string fwVer;
	int result = svc->GetModemFirmwareVersion(&fwVer).GetCode();
	telCopyStringField(outBuf, 33, fwVer);
	return result;
}

/* 0x8126D1E0 */
static int telIpcGetUpdateListUrl(void *outBuf)
{
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	if (svc == NULL || outBuf == NULL)
		return TEL_ERR_NOT_FOUND;
	paf::string url;
	int result = svc->GetUpdateListUrl(&url).GetCode();
	telCopyStringField(outBuf, 128, url);
	return result;
}

/* 0x8126D372: sync dispatcher implementation for methods 0x050000..0x050055 */
static int telIpcDispatchSyncImpl(IPMI::Session *session, SceUInt32 methodId,
				  const IPMI::DataInfo *inList, SceUInt32 inCount,
				  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	int result = TEL_ERR_NOT_FOUND;
	SceSize outBytes = 0;

	switch (methodId) {
	case 0x050000:
		result = telRegisterSession(session);
		break;
	case 0x050001:
		result = telIpcUnregisterSession(session);
		break;
	case 0x050002:
		result = telPopGeneralEvent(session,
					    (outCount > 0) ? outList[0].pBuffer : NULL,
					    (outCount > 1) ? static_cast<int *>(outList[1].pBuffer) : NULL);
		if (outCount > 1)
			outList[1].outSize = sizeof(int);
		outBytes = sizeof(TelGeneralLegacyEventRecord);
		break;
	case 0x050003:
		result = telPopSmsEvent(session,
					(outCount > 0) ? outList[0].pBuffer : NULL,
					(outCount > 1) ? static_cast<int *>(outList[1].pBuffer) : NULL);
		if (outCount > 1)
			outList[1].outSize = sizeof(int);
		outBytes = sizeof(TelSmsLegacyEventRecord);
		break;
	case 0x050004:
		result = telIpcGetGeneralLogicState(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050005:
		result = telIpcSmsGetServiceState(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050006:
		if (inCount > 1)
			result = telIpcSetFlightMode(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050007:
		result = telIpcGetFlightMode(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050008:
		if (inCount > 1)
			result = telIpcSetEnableWwan(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050009:
		result = telIpcGetEnableWwan(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05000A:
		result = telIpcGetPhoneNumber((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 33;
		break;
	case 0x05000B:
		result = telIpcGetImei((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 33;
		break;
	case 0x05000C:
		result = telIpcGetIccid((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 21;
		break;
	case 0x05000D:
		if (inCount > 1)
			result = telIpcVerifyPin(static_cast<const char *>(inList[1].pData));
		break;
	case 0x05000E:
		if (inCount > 2)
			result = telIpcUnblockPin(static_cast<const char *>(inList[1].pData),
						  static_cast<const char *>(inList[2].pData));
		break;
	case 0x05000F:
		if (inCount > 2)
			result = telIpcSetPinProtection(*static_cast<const int *>(inList[0].pData),
							static_cast<const char *>(inList[2].pData));
		break;
	case 0x050010:
		result = telIpcGetPinProtection(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050011:
		result = telIpcGetPinStatus(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050012:
		result = telIpcGetPinRemain(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050013:
		if (inCount > 2)
			result = telIpcChangePinCode(static_cast<const char *>(inList[1].pData),
						     static_cast<const char *>(inList[2].pData));
		break;
	case 0x050014:
		result = telIpcSmsGetDbPath((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 128;
		break;
	case 0x050015:
		if (inCount > 0)
			result = telIpcSmsSendSms(*static_cast<const SceUInt64 *>(inList[0].pData));
		break;
	case 0x050016:
		if (inCount > 0)
			result = telIpcSmsResendSms(*static_cast<const SceUInt64 *>(inList[0].pData));
		break;
	case 0x050017:
		if (inCount > 0)
			result = telIpcSmsUpdateSmsStatus(
				static_cast<const TelIpcSmsUpdateStatusReq *>(inList[0].pData));
		break;
	case 0x050018:
		if (inCount > 0)
			result = telIpcSmsDeleteSmsList(
				static_cast<const SceUInt64 *>(inList[0].pData),
				inList[0].dataSize / sizeof(SceUInt64));
		break;
	case 0x050019:
		result = telIpcSmsDeleteAllSms();
		break;
	case 0x05001A:
		if (inCount > 1)
			result = telIpcSetApnInfoTemp(static_cast<const char *>(inList[1].pData));
		break;
	case 0x05001B:
		if (inCount > 1)
			result = telIpcSetApnInfoSave(static_cast<const char *>(inList[1].pData));
		break;
	case 0x05001C:
		if (inCount > 1)
			result = telIpcSetApnInfoWithBillingPlan(
				static_cast<const TelOperatorApnRawRecord *>(inList[1].pData));
		break;
	case 0x05001D:
		result = telIpcGetUnsupportedProfile();
		break;
	case 0x05001E:
		result = telIpcRestoreApnInfo();
		break;
	case 0x05001F:
		if (outCount > 0) {
			result = telIpcGetMccMncOfImsi(static_cast<int *>(outList[0].pBuffer));
			outBytes = 8;
		}
		break;
	case 0x050020:
		result = telIpcGetProductTargetOperator(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050021:
		result = telIpcGetOperatorProfileCount(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050022:
		if (inCount > 0 && outCount > 0)
			result = telIpcGetOperatorProfile(
				*static_cast<const int *>(inList[0].pData),
				static_cast<TelOperatorProfileRawRecord *>(outList[0].pBuffer));
		outBytes = sizeof(TelOperatorProfileRawRecord);
		break;
	case 0x050023:
		result = telIpcGetApnInfoListCount(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050024:
		if (inCount > 0 && outCount > 0)
			result = telIpcGetApnInfoListItem(
				*static_cast<const int *>(inList[0].pData),
				static_cast<TelOperatorApnRawRecord *>(outList[0].pBuffer));
		outBytes = sizeof(TelOperatorApnRawRecord);
		break;
	case 0x050025:
		result = telIpcGetCurrentApnInfo(
			(outCount > 0) ? static_cast<TelOperatorApnRawRecord *>(outList[0].pBuffer) : NULL);
		outBytes = sizeof(TelOperatorApnRawRecord);
		break;
	case 0x050026:
		result = telIpcRemoveCurrentApnInfo();
		break;
	case 0x050027:
		result = telIpcStartNetworkScan();
		break;
	case 0x050028:
		result = telIpcAbortNetworkScan();
		break;
	case 0x050029:
		result = telIpcGetNetworkScanInfoCount(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05002A:
		if (inCount > 0 && outCount > 0)
			result = telIpcGetNetworkScanInfo(
				*static_cast<const int *>(inList[0].pData),
				static_cast<TelNetScanRawRecord *>(outList[0].pBuffer));
		outBytes = sizeof(TelNetScanRawRecord);
		break;
	case 0x05002B:
		result = telIpcSetNetRegisterAuto();
		break;
	case 0x05002C:
		if (inCount > 1)
			result = telIpcSetNetRegisterInfo(
				static_cast<const TelNetScanRawRecord *>(inList[1].pData));
		break;
	case 0x05002D:
		result = telIpcGetNetRegistrationInfo((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = sizeof(TelIpcNetRegistrationInfo);
		break;
	case 0x05002E:
		if (inCount > 0)
			result = telIpcSetRegistrationAppNetMode(
				*static_cast<const int *>(inList[0].pData));
		break;
	case 0x05002F:
		if (inCount > 0)
			result = telIpcSetDataRoamingPermission(
				*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050030:
		result = telIpcGetDataRoamingPermission(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050031:
		if (inCount > 0)
			result = telIpcSetDataRoamingDefinition(
				*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050032:
		result = telIpcCheckSimCondition(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050033:
		result = telIpcGetControlKeyState(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050034:
		result = telIpcGetControlKeyBlockType(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050035:
		result = telIpcGetNetworkStatusReport();
		break;
	case 0x050036:
		result = telIpcGetDeviceInfoReport();
		break;
	case 0x050037:
		result = telIpcGetIsComExist(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050038:
		result = telIpcGetIsApnInit(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050039:
		result = telIpcIsOpenSimModel(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05003A:
		result = telIpcGetNetworkSelectionMode(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05003B:
		result = telIpcGetConditionError(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05003C:
		if (inCount > 1)
			result = telIpcDeactivateSimLock(static_cast<const char *>(inList[1].pData));
		break;
	case 0x05003D:
		if (inCount > 1)
			result = telIpcUnblockSimLock(static_cast<const char *>(inList[1].pData));
		break;
	case 0x05003E:
		result = telIpcGetSimLockDeactivateRemain(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05003F:
		result = telIpcGetSimLockUnblockRemain(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050040:
		result = telIpcGetCurrentSignalLevel(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050041:
		if (inCount > 0)
			result = telIpcSetBillingPlan(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050042:
		result = telIpcGetBillingPlan(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050043:
		result = telIpcGetSavedNetworkSelection((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = sizeof(TelIpcNetRegistrationInfo);
		break;
	case 0x050044:
		if (inCount > 0)
			result = telIpcSetDebugLogEnabled(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050045:
		result = telIpcReadDebugLogLine((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 257;
		break;
	case 0x050046:
		if (inCount > 0)
			result = telIpcSetDebugDiagFlag(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050047:
		result = telIpcGetDebugDiagFlag(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x050048:
		if (inCount > 0)
			result = telIpcSetDebugForceFlag(*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050049:
		result = telIpcGetDebugForceFlag(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05004A:
		result = telIpcIsSimActivated(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05004B:
		result = telIpcGetLatestArchiveVersion(
			(outCount > 0) ? static_cast<SceUInt32 *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05004C:
		result = telIpcGetArchiveVersion(
			(outCount > 0) ? static_cast<SceUInt32 *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05004D:
		result = telIpcCheckComVersion(
			(outCount > 0) ? static_cast<int *>(outList[0].pBuffer) : NULL);
		outBytes = 4;
		break;
	case 0x05004E:
		result = telIpcReleasePdpContext();
		break;
	case 0x05004F:
		if (inCount > 1)
			result = telIpcCheckImsiForOperatorCode(
				static_cast<const TelIpcCheckImsiReq *>(inList[1].pData));
		break;
	case 0x050050:
		result = telIpcStartApnListDownload();
		break;
	case 0x050051:
		if (inCount > 0)
			result = telIpcCheckContentDownloadAllowed(
				*static_cast<const int *>(inList[0].pData));
		break;
	case 0x050052:
		if (inCount > 0 && outCount > 0)
			result = telIpcGetDownloadSizeLimit(
				*static_cast<const unsigned int *>(inList[0].pData),
				static_cast<SceInt64 *>(outList[0].pBuffer));
		outBytes = 8;
		break;
	case 0x050053:
		if (inCount > 0)
			result = telIpcCheckRoamingDownloadAllowed(
				*static_cast<const unsigned int *>(inList[0].pData));
		break;
	case 0x050054:
		result = telIpcGetModemFirmwareVersion((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 33;
		break;
	case 0x050055:
		result = telIpcGetUpdateListUrl((outCount > 0) ? outList[0].pBuffer : NULL);
		outBytes = 128;
		break;
	default:
		return kErrUnsupportedMethod;
	}

	if (outCount > 0 && outBytes != 0)
		outList[0].outSize = outBytes;
	return session->respondToSyncMethodRequest(result, outList, outCount);
}

} /* anonymous namespace */

/* ------------------------------------------------------------------
 * SceShellSvc Channel 0x00050000 Dispatchers
 * ------------------------------------------------------------------ */

/* 0x8126D292: outer sync dispatcher forwarding to 0x8126D372 */
int shellSvcDisp_05_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return telIpcDispatchSyncImpl(session, methodId, inList, inCount, outList, outCount);
}

/* Async dispatcher (unsupported on Telephony channel) */
int shellSvcDisp_05_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)reqId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return kErrUnsupportedMethod;
}

/* Disconnect handler cleaning up per-session event queues */
void shellSvcDisp_05_Disconn(IPMI::Session *session)
{
	telUnregisterSession(session);
}

/* ------------------------------------------------------------------
 * In-process SceTelephony IPMI client helpers (0x8126E1AE..0x8126E4EC)
 * ------------------------------------------------------------------ */

static int telClientInvokeCheckDownload(IPMI::Client *client, const void *headerBuf,
					int serviceType)
{
	int retVal = 0;
	int svcTypeLocal = serviceType;
	IPMI::DataInfo inInfo[2];
	inInfo[0].pData = &svcTypeLocal;
	inInfo[0].dataSize = sizeof(svcTypeLocal);
	inInfo[1].pData = headerBuf;
	inInfo[1].dataSize = 8;

	int err = client->invokeSyncMethod(0x050051, inInfo, 2, &retVal, NULL, 0);
	return (err == 0) ? retVal : err;
}

static int telClientInvokeGetDownloadLimit(IPMI::Client *client, const void *headerBuf,
					   int serviceType, SceInt64 *outLimit)
{
	int retVal = 0;
	int svcTypeLocal = serviceType;
	IPMI::DataInfo inInfo[2];
	inInfo[0].pData = &svcTypeLocal;
	inInfo[0].dataSize = sizeof(svcTypeLocal);
	inInfo[1].pData = headerBuf;
	inInfo[1].dataSize = 8;

	IPMI::BufferInfo outInfo;
	outInfo.pBuffer = outLimit;
	outInfo.bufferSize = sizeof(SceInt64);
	outInfo.outSize = 0;

	int err = client->invokeSyncMethod(0x050052, inInfo, 2, &retVal, &outInfo, 1);
	return (err == 0) ? retVal : err;
}

static int telClientInvokeCheckRoamingDownload(IPMI::Client *client, const void *headerBuf,
					       int serviceType)
{
	int retVal = 0;
	int svcTypeLocal = serviceType;
	IPMI::DataInfo inInfo[2];
	inInfo[0].pData = &svcTypeLocal;
	inInfo[0].dataSize = sizeof(svcTypeLocal);
	inInfo[1].pData = headerBuf;
	inInfo[1].dataSize = 8;

	int err = client->invokeSyncMethod(0x050053, inInfo, 2, &retVal, NULL, 0);
	return (err == 0) ? retVal : err;
}

static int telClientInvokeGetUpdateUrl(IPMI::Client *client, const void *headerBuf,
				       void *outUrlBuf)
{
	int retVal = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = headerBuf;
	inInfo.dataSize = 8;

	IPMI::BufferInfo outInfo;
	outInfo.pBuffer = outUrlBuf;
	outInfo.bufferSize = 128;
	outInfo.outSize = 0;

	int err = client->invokeSyncMethod(0x050055, &inInfo, 1, &retVal, &outInfo, 1);
	return (err == 0) ? retVal : err;
}

/* 0x8126E1AE */
int telClientGetUpdateListUrl(char *outUrlBuf)
{
	if (outUrlBuf == NULL)
		return kErrInvalidPointer;

	SceUInt8 header[8];
	sce_paf_memset(header, 0, sizeof(header));
	IPMI::Client *client = SceShellSvc_B31E7F1C();
	int err = telClientInvokeGetUpdateUrl(client, header, outUrlBuf);
	if (err != 0) {
		sce_paf_memset(outUrlBuf, 0, 128);
		sce_paf_strncpy(outUrlBuf, s_fallbackUpdateListUrl, 127);
		return 0;
	}
	return err;
}

/* 0x8126E216 */
int telClientCheckContentDownloadAllowed(int serviceType)
{
	SceUInt8 header[8];
	sce_paf_memset(header, 0, sizeof(header));
	IPMI::Client *client = SceShellSvc_B31E7F1C();
	return telClientInvokeCheckDownload(client, header, serviceType);
}

/* 0x8126E252 */
int telClientGetDownloadSizeLimit(int serviceType, SceInt64 *outLimit)
{
	if (outLimit == NULL)
		return kErrInvalidPointer;

	SceUInt8 header[8];
	sce_paf_memset(header, 0, sizeof(header));
	SceInt64 limitVal = 0;
	IPMI::Client *client = SceShellSvc_B31E7F1C();
	int err = telClientInvokeGetDownloadLimit(client, header, serviceType, &limitVal);
	*outLimit = limitVal;
	return err;
}

/* 0x8126E2AE */
int telClientCheckRoamingDownloadAllowed(int serviceType)
{
	SceUInt8 header[8];
	sce_paf_memset(header, 0, sizeof(header));
	IPMI::Client *client = SceShellSvc_B31E7F1C();
	return telClientInvokeCheckRoamingDownload(client, header, serviceType);
}

int Bgdl_8126E1AE(char *outUrlBuf)
{
	return telClientGetUpdateListUrl(outUrlBuf);
}

int Bgdl_8126E216(int serviceType)
{
	return telClientCheckContentDownloadAllowed(serviceType);
}

int Bgdl_8126E252(int serviceType, SceUInt64 *outLimit)
{
	return telClientGetDownloadSizeLimit(serviceType, reinterpret_cast<SceInt64 *>(outLimit));
}

int Bgdl_8126E2AE(int serviceType)
{
	return telClientCheckRoamingDownloadAllowed(serviceType);
}
