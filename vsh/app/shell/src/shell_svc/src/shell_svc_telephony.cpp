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
#include "telephony_plugin/src/general/tel_general_internal.h"

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

} /* anonymous namespace */

/* ------------------------------------------------------------------
 * SceShellSvc Channel 0x00050000 Dispatchers
 * ------------------------------------------------------------------ */

/* 0x8126D372: sync dispatcher for methods 0x050000..0x050055 */
int shellSvcDisp_05_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	TelGeneralLogicServiceInterface *svc = telGetGeneralService();
	TelSmsLogicApiInterface *sms = telGetSmsApi();

	int result = TEL_ERR_NOT_FOUND;
	SceSize outBytes = 0;

	switch (methodId) {
	case 0x050000: /* 327680: Register session */
		result = telRegisterSession(session);
		break;

	case 0x050001: /* 327681: Unregister session */
		result = telUnregisterSession(session);
		break;

	case 0x050002: /* 327682: Pop general event */
		result = telPopGeneralEvent(session,
					    (outCount > 0) ? outList[0].pBuffer : NULL,
					    (outCount > 1) ? static_cast<int *>(outList[1].pBuffer) : NULL);
		outBytes = sizeof(TelGeneralLegacyEventRecord);
		break;

	case 0x050003: /* 327683: Pop SMS event */
		result = telPopSmsEvent(session,
					(outCount > 0) ? outList[0].pBuffer : NULL,
					(outCount > 1) ? static_cast<int *>(outList[1].pBuffer) : NULL);
		outBytes = sizeof(TelSmsLegacyEventRecord);
		break;

	case 0x050004: /* 327684: GetGeneralLogicState */
		if (svc != NULL && outCount > 0)
			result = svc->GetGeneralLogicState(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050005: /* 327685: SMS GetServiceState */
		if (sms != NULL && outCount > 0)
			result = sms->GetServiceState(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050006: /* 327686: SetFlightMode */
		if (svc != NULL && inCount > 1)
			result = svc->SetFlightMode(*static_cast<const int *>(inList[0].pData)).GetCode();
		break;

	case 0x050007: /* 327687: GetFlightMode */
		if (svc != NULL && outCount > 0) {
			bool enabled = false;
			result = svc->GetFlightMode(&enabled).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = enabled ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x050008: /* 327688: SetEnableWwan */
		if (svc != NULL && inCount > 1)
			result = svc->SetEnableWwan(*static_cast<const int *>(inList[0].pData) != 0).GetCode();
		break;

	case 0x050009: /* 327689: GetEnableWwan */
		if (svc != NULL && outCount > 0) {
			bool enabled = false;
			result = svc->GetEnableWwan(&enabled).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = enabled ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x05000A: /* 327690: GetPhoneNumber */
		if (svc != NULL && outCount > 0) {
			TelPhoneNumber number;
			result = svc->GetPhoneNumber(&number).GetCode();
			telCopyStringField(outList[0].pBuffer, 33, paf::string(number.GetDigits()));
		}
		outBytes = 33;
		break;

	case 0x05000B: /* 327691: GetImei */
		if (svc != NULL && outCount > 0) {
			paf::string imei;
			result = svc->GetImei(&imei).GetCode();
			telCopyStringField(outList[0].pBuffer, 33, imei);
		}
		outBytes = 33;
		break;

	case 0x05000C: /* 327692: GetIccid */
		if (svc != NULL && outCount > 0) {
			paf::string iccid;
			result = svc->GetIccid(&iccid).GetCode();
			telCopyStringField(outList[0].pBuffer, 21, iccid);
		}
		outBytes = 21;
		break;

	case 0x05000D: /* 327693: VerifyPin */
		if (svc != NULL && inCount > 1) {
			TelPinCode pin(paf::string(static_cast<const char *>(inList[1].pData)));
			result = svc->VerifyPin(pin).GetCode();
		}
		break;

	case 0x05000E: /* 327694: UnblockPin */
		if (svc != NULL && inCount > 2) {
			TelUnblockPinInfo unblock(
				paf::string(static_cast<const char *>(inList[1].pData)),
				paf::string(static_cast<const char *>(inList[2].pData)));
			result = svc->UnblockPin(unblock).GetCode();
		}
		break;

	case 0x05000F: /* 327695: SetPinProtection */
		if (svc != NULL && inCount > 2) {
			bool enable = (*static_cast<const int *>(inList[0].pData) != 0);
			TelPinCode pin(paf::string(static_cast<const char *>(inList[2].pData)));
			result = svc->SetPinProtection(enable, pin).GetCode();
		}
		break;

	case 0x050010: /* 327696: GetPinProtection */
		if (svc != NULL && outCount > 0) {
			bool enabled = false;
			result = svc->GetPinProtection(&enabled).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = enabled ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x050011: /* 327697: GetPinStatus */
		if (svc != NULL && outCount > 0)
			result = svc->GetPinStatus(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050012: /* 327698: GetPinRemain */
		if (svc != NULL && outCount > 0)
			result = svc->GetPinRemain(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050013: /* 327699: ChangePinCode */
		if (svc != NULL && inCount > 2) {
			TelPinCode oldPin(paf::string(static_cast<const char *>(inList[1].pData)));
			TelPinCode newPin(paf::string(static_cast<const char *>(inList[2].pData)));
			result = svc->ChangePinCode(oldPin, newPin).GetCode();
		}
		break;

	case 0x050014: /* 327700: SMS GetDbPath */
		if (sms != NULL && outCount > 0) {
			paf::string dbPath;
			result = sms->GetDbPath(&dbPath).GetCode();
			telCopyStringField(outList[0].pBuffer, 128, dbPath);
		}
		outBytes = 128;
		break;

	case 0x050015: /* 327701: SMS SendSms */
		if (sms != NULL && inCount > 0) {
			TelSmsMessageId msgId(*static_cast<const SceUInt64 *>(inList[0].pData));
			result = sms->SendSms(msgId);
		}
		break;

	case 0x050016: /* 327702: SMS ResendSms */
		if (sms != NULL && inCount > 0) {
			TelSmsMessageId msgId(*static_cast<const SceUInt64 *>(inList[0].pData));
			result = sms->ResendSms(msgId);
		}
		break;

	case 0x050017: /* 327703: SMS UpdateSmsStatus */
		if (sms != NULL && inCount > 0) {
			const TelIpcSmsUpdateStatusReq *req =
				static_cast<const TelIpcSmsUpdateStatusReq *>(inList[0].pData);
			TelSmsMessageId msgId(req->messageId);
			result = sms->UpdateSmsStatus(msgId, req->status);
		}
		break;

	case 0x050018: /* 327704: SMS DeleteSmsList */
		if (sms != NULL && inCount > 0) {
			SceSize count = inList[0].dataSize / sizeof(SceUInt64);
			const SceUInt64 *rawIds = static_cast<const SceUInt64 *>(inList[0].pData);
			paf::vector<TelSmsMessageId> ids;
			for (SceSize idx = 0; idx < count; ++idx)
				ids.push_back(TelSmsMessageId(rawIds[idx]));
			result = sms->DeleteSmsList(ids);
		}
		break;

	case 0x050019: /* 327705: SMS DeleteAllSms */
		if (sms != NULL)
			result = sms->DeleteAllSms();
		break;

	case 0x05001A: /* 327706: SetApnInfo (save = false) */
		if (svc != NULL && inCount > 1) {
			TelephonyOperator apnOp;
			apnOp.SetFromRawRecord(static_cast<const char *>(inList[1].pData));
			result = svc->SetApnInfo(apnOp, false).GetCode();
		}
		break;

	case 0x05001B: /* 327707: SetApnInfo (save = true) */
		if (svc != NULL && inCount > 1) {
			TelephonyOperator apnOp;
			apnOp.SetFromRawRecord(static_cast<const char *>(inList[1].pData));
			result = svc->SetApnInfo(apnOp, true).GetCode();
		}
		break;

	case 0x05001C: /* 327708: SetApnInfoWithBillingPlan */
		if (svc != NULL && inCount > 1) {
			TelOperatorApnInfo apnInfo;
			apnInfo.UnpackFromRaw(
				static_cast<const TelOperatorApnRawRecord *>(inList[1].pData));
			result = svc->SetApnInfoWithBillingPlan(apnInfo, true).GetCode();
		}
		break;

	case 0x05001D: /* 327709: GetUnsupportedProfile */
		if (svc != NULL)
			result = svc->GetUnsupportedProfile().GetCode();
		break;

	case 0x05001E: /* 327710: RestoreApnInfo */
		if (svc != NULL)
			result = svc->RestoreApnInfo().GetCode();
		break;

	case 0x05001F: /* 327711: GetMccMncOfImsi */
		if (svc != NULL && outCount > 0) {
			TelephonyPlmn plmn;
			result = svc->GetMccMncOfImsi(&plmn).GetCode();
			int *outPair = static_cast<int *>(outList[0].pBuffer);
			outPair[0] = plmn.GetMcc();
			outPair[1] = plmn.GetMnc();
			outBytes = 8;
		}
		break;

	case 0x050020: /* 327712: GetProductTargetOperator */
		if (svc != NULL && outCount > 0)
			result = svc->GetProductTargetOperator(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050021: /* 327713: GetOperatorProfileCount */
		if (svc != NULL && outCount > 0)
			result = svc->GetOperatorProfileCount(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050022: /* 327714: GetOperatorProfile */
		if (svc != NULL && inCount > 0 && outCount > 0) {
			int index = *static_cast<const int *>(inList[0].pData);
			TelOperatorProfile profile;
			result = svc->GetOperatorProfile(index, &profile).GetCode();
			profile.PackToRaw(static_cast<TelOperatorProfileRawRecord *>(outList[0].pBuffer));
		}
		outBytes = sizeof(TelOperatorProfileRawRecord);
		break;

	case 0x050023: /* 327715: GetApnInfoListCount */
		if (svc != NULL && outCount > 0)
			result = svc->GetApnInfoListCount(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050024: /* 327716: GetApnInfoListItem */
		if (svc != NULL && inCount > 0 && outCount > 0) {
			int index = *static_cast<const int *>(inList[0].pData);
			TelOperatorApnInfo apnItem;
			result = svc->GetApnInfoListItem(index, &apnItem).GetCode();
			apnItem.PackToRaw(static_cast<TelOperatorApnRawRecord *>(outList[0].pBuffer));
		}
		outBytes = sizeof(TelOperatorApnRawRecord);
		break;

	case 0x050025: /* 327717: GetCurrentApnInfo */
		if (svc != NULL && outCount > 0) {
			TelephonyOperator apnOp;
			result = svc->GetCurrentApnInfo(&apnOp).GetCode();
			TelOperatorApnInfo apnInfo(apnOp);
			apnInfo.PackToRaw(static_cast<TelOperatorApnRawRecord *>(outList[0].pBuffer));
		}
		outBytes = sizeof(TelOperatorApnRawRecord);
		break;

	case 0x050026: /* 327718: RemoveCurrentApnInfo */
		if (svc != NULL)
			result = svc->RemoveCurrentApnInfo().GetCode();
		break;

	case 0x050027: /* 327719: StartNetworkScan */
		if (svc != NULL)
			result = svc->StartNetworkScan().GetCode();
		break;

	case 0x050028: /* 327720: AbortNetworkScan */
		if (svc != NULL)
			result = svc->AbortNetworkScan().GetCode();
		break;

	case 0x050029: /* 327721: GetNetworkScanInfoCount */
		if (svc != NULL && outCount > 0)
			result = svc->GetNetworkScanInfoCount(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05002A: /* 327722: GetNetworkScanInfo */
		if (svc != NULL && inCount > 0 && outCount > 0) {
			int index = *static_cast<const int *>(inList[0].pData);
			TelNetScanInfo scanItem;
			result = svc->GetNetworkScanInfo(index, &scanItem).GetCode();
			scanItem.PackToRaw(static_cast<TelNetScanRawRecord *>(outList[0].pBuffer));
		}
		outBytes = sizeof(TelNetScanRawRecord);
		break;

	case 0x05002B: /* 327723: SetNetRegisterAuto */
		if (svc != NULL)
			result = svc->SetNetRegisterAuto().GetCode();
		break;

	case 0x05002C: /* 327724: SetNetRegisterInfo */
		if (svc != NULL && inCount > 1) {
			TelNetScanInfo scanTarget;
			scanTarget.UnpackFromRaw(
				static_cast<const TelNetScanRawRecord *>(inList[1].pData));
			result = svc->SetNetRegisterInfo(scanTarget).GetCode();
		}
		break;

	case 0x05002D: /* 327725: GetNetRegistrationInfo */
		if (svc != NULL && outCount > 0) {
			TelNetworkInfo netInfo;
			result = svc->GetNetRegistrationInfo(&netInfo).GetCode();
			telPackNetRegistration(netInfo, outList[0].pBuffer);
		}
		outBytes = sizeof(TelIpcNetRegistrationInfo);
		break;

	case 0x05002E: /* 327726: SetRegistrationAppNetMode */
		if (svc != NULL && inCount > 0)
			result = svc->SetRegistrationAppNetMode(*static_cast<const int *>(inList[0].pData)).GetCode();
		break;

	case 0x05002F: /* 327727: SetDataRoamingPermission */
		if (svc != NULL && inCount > 0)
			result = svc->SetDataRoamingPermission(*static_cast<const int *>(inList[0].pData) != 0).GetCode();
		break;

	case 0x050030: /* 327728: GetDataRoamingPermission */
		if (svc != NULL && outCount > 0) {
			bool allowed = false;
			result = svc->GetDataRoamingPermission(&allowed).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = allowed ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x050031: /* 327729: SetDataRoamingDefinition */
		if (svc != NULL && inCount > 0)
			result = svc->SetDataRoamingDefinition(*static_cast<const int *>(inList[0].pData)).GetCode();
		break;

	case 0x050032: /* 327730: CheckSimCondition */
		if (svc != NULL && outCount > 0) {
			int cond = svc->CheckSimCondition().GetCode();
			*static_cast<int *>(outList[0].pBuffer) = cond;
			result = 0;
		}
		outBytes = 4;
		break;

	case 0x050033: /* 327731: GetControlKeyState */
		if (svc != NULL && outCount > 0)
			result = svc->GetControlKeyState(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050034: /* 327732: GetControlKeyBlockType */
		if (svc != NULL && outCount > 0)
			result = svc->GetControlKeyBlockType(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050035: /* 327733: GetNetworkStatusReport */
		if (svc != NULL) {
			paf::string report;
			result = svc->GetNetworkStatusReport(&report).GetCode();
		}
		break;

	case 0x050036: /* 327734: GetDeviceInfoReport */
		if (svc != NULL) {
			paf::string report;
			result = svc->GetDeviceInfoReport(&report).GetCode();
		}
		break;

	case 0x050037: /* 327735: GetIsComExist */
		if (svc != NULL && outCount > 0)
			result = svc->GetIsComExist(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050038: /* 327736: GetIsApnInit */
		if (svc != NULL && outCount > 0)
			result = svc->GetIsApnInit(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050039: /* 327737: IsOpenSimModel */
		if (svc != NULL && outCount > 0) {
			bool openModel = false;
			result = svc->IsOpenSimModel(&openModel).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = openModel ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x05003A: /* 327738: GetNetworkSelectionMode */
		if (svc != NULL && outCount > 0)
			result = svc->GetNetworkSelectionMode(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05003B: /* 327739: GetConditionError */
		if (svc != NULL && outCount > 0) {
			TelephonyResult condErr(TEL_OK);
			result = svc->GetConditionError(&condErr).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = condErr.GetCode();
		}
		outBytes = 4;
		break;

	case 0x05003C: /* 327740: DeactivateSimLock */
		if (svc != NULL && inCount > 1) {
			TelCarrierLockKey key(paf::string(static_cast<const char *>(inList[1].pData)));
			result = svc->DeactivateSimLock(key).GetCode();
		}
		break;

	case 0x05003D: /* 327741: UnblockSimLock */
		if (svc != NULL && inCount > 1) {
			TelControlKey key(paf::string(static_cast<const char *>(inList[1].pData)));
			result = svc->UnblockSimLock(key).GetCode();
		}
		break;

	case 0x05003E: /* 327742: GetSimLockDeactivateRemain */
		if (svc != NULL && outCount > 0)
			result = svc->GetSimLockDeactivateRemain(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05003F: /* 327743: GetSimLockUnblockRemain */
		if (svc != NULL && outCount > 0)
			result = svc->GetSimLockUnblockRemain(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050040: /* 327744: GetCurrentSignalLevel */
		if (svc != NULL && outCount > 0)
			result = svc->GetCurrentSignalLevel(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050041: /* 327745: SetBillingPlan */
		if (svc != NULL && inCount > 0) {
			int plan = *static_cast<const int *>(inList[0].pData);
			result = svc->SetBillingPlan(&plan).GetCode();
		}
		break;

	case 0x050042: /* 327746: GetBillingPlan */
		if (svc != NULL && outCount > 0)
			result = svc->GetBillingPlan(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050043: /* 327747: GetSavedNetworkSelection */
		if (svc != NULL && outCount > 0) {
			TelNetworkInfo netInfo;
			result = svc->GetSavedNetworkSelection(&netInfo).GetCode();
			telPackNetRegistration(netInfo, outList[0].pBuffer);
		}
		outBytes = sizeof(TelIpcNetRegistrationInfo);
		break;

	case 0x050044: /* 327748: SetDebugLogEnabled */
		if (svc != NULL && inCount > 0)
			result = svc->SetDebugLogEnabled(*static_cast<const int *>(inList[0].pData) != 0).GetCode();
		break;

	case 0x050045: /* 327749: ReadDebugLogLine */
		if (svc != NULL && outCount > 0) {
			paf::string line;
			result = svc->ReadDebugLogLine(&line).GetCode();
			telCopyStringField(outList[0].pBuffer, 257, line);
		}
		outBytes = 257;
		break;

	case 0x050046: /* 327750: SetDebugDiagFlag */
		if (svc != NULL && inCount > 0) {
			int diagVal = *static_cast<const int *>(inList[0].pData);
			result = svc->SetDebugDiagFlag(&diagVal).GetCode();
		}
		break;

	case 0x050047: /* 327751: GetDebugDiagFlag */
		if (svc != NULL && outCount > 0)
			result = svc->GetDebugDiagFlag(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x050048: /* 327752: SetDebugForceFlag */
		if (svc != NULL && inCount > 0) {
			int forceVal = *static_cast<const int *>(inList[0].pData);
			result = svc->SetDebugForceFlag(&forceVal).GetCode();
		}
		break;

	case 0x050049: /* 327753: GetDebugForceFlag */
		if (svc != NULL && outCount > 0)
			result = svc->GetDebugForceFlag(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05004A: /* 327754: IsSimActivated */
		if (svc != NULL && outCount > 0)
			result = svc->IsSimActivated(static_cast<int *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05004B: /* 327755: GetLatestArchiveVersion */
		if (svc != NULL && outCount > 0)
			result = svc->GetLatestArchiveVersion(static_cast<SceUInt32 *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05004C: /* 327756: GetArchiveVersion */
		if (svc != NULL && outCount > 0)
			result = svc->GetArchiveVersion(static_cast<SceUInt32 *>(outList[0].pBuffer)).GetCode();
		outBytes = 4;
		break;

	case 0x05004D: /* 327757: CheckComVersion */
		if (svc != NULL && outCount > 0) {
			bool matches = false;
			result = svc->CheckComVersion(&matches).GetCode();
			*static_cast<int *>(outList[0].pBuffer) = matches ? 1 : 0;
		}
		outBytes = 4;
		break;

	case 0x05004E: /* 327758: ReleasePdpContext */
		if (svc != NULL)
			result = svc->ReleasePdpContext().GetCode();
		break;

	case 0x05004F: /* 327759: CheckImsiForOperatorCode */
		if (svc != NULL && inCount > 1) {
			const TelIpcCheckImsiReq *req =
				static_cast<const TelIpcCheckImsiReq *>(inList[1].pData);
			int opCode = req->operatorCode;
			bool matched = false;
			result = svc->CheckImsiForOperatorCode(&opCode, &matched).GetCode();
		}
		break;

	case 0x050050: /* 327760: StartApnListDownload */
		if (svc != NULL)
			result = svc->StartApnListDownload().GetCode();
		break;

	case 0x050051: /* 327761: CheckContentDownloadAllowed */
		if (svc != NULL && inCount > 0) {
			int serviceType = *static_cast<const int *>(inList[0].pData);
			result = svc->CheckContentDownloadAllowed(serviceType).GetCode();
		}
		break;

	case 0x050052: /* 327762: GetDownloadSizeLimit */
		if (svc != NULL && inCount > 0 && outCount > 0) {
			unsigned int serviceType = *static_cast<const unsigned int *>(inList[0].pData);
			result = svc->GetDownloadSizeLimit(
				serviceType, static_cast<SceInt64 *>(outList[0].pBuffer)).GetCode();
		}
		outBytes = 8;
		break;

	case 0x050053: /* 327763: CheckRoamingDownloadAllowed */
		if (svc != NULL && inCount > 0) {
			unsigned int serviceType = *static_cast<const unsigned int *>(inList[0].pData);
			result = svc->CheckRoamingDownloadAllowed(serviceType).GetCode();
		}
		break;

	case 0x050054: /* 327764: GetModemFirmwareVersion */
		if (svc != NULL && outCount > 0) {
			paf::string fwVer;
			result = svc->GetModemFirmwareVersion(&fwVer).GetCode();
			telCopyStringField(outList[0].pBuffer, 33, fwVer);
		}
		outBytes = 33;
		break;

	case 0x050055: /* 327765: GetUpdateListUrl */
		if (svc != NULL && outCount > 0) {
			paf::string url;
			result = svc->GetUpdateListUrl(&url).GetCode();
			telCopyStringField(outList[0].pBuffer, 128, url);
		}
		outBytes = 128;
		break;

	default:
		return kErrUnsupportedMethod;
	}

	if (outCount > 0 && outBytes != 0)
		outList[0].outSize = outBytes;
	return session->respondToSyncMethodRequest(result, outList, outCount);
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
