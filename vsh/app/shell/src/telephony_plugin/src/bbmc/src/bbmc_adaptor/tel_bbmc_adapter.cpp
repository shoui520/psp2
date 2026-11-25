/*
 * tel_bbmc_adapter.cpp: real hardware BBMC adapter, typed event payloads,
 * network/device diagnostic formatters, and debug output routing for
 * telephony_plugin (3.65-CEX: 0x813CB470..0x813CFBA9).
 */
#include <std/string.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

static bool g_bbmcDebugOutEnabled = false;
static const int kBbmcSubscriberThreadPriority = 0x1000010E;
static const SceSize kBbmcSubscriberStackSize = 0x4000;

/* TelBbmcAdapterInterface base class */
TelBbmcAdapterInterface::TelBbmcAdapterInterface()
	: TelObservable("SceTelBbmcAdapterInterface")
{
}

TelBbmcAdapterInterface::~TelBbmcAdapterInterface()
{
}

/* Typed BBMC event subclasses */
TelBbmcNetworkEvent::TelBbmcNetworkEvent()
	: TelEventInfo(TEL_BBMC_EVENT_NETWORK_INFO, -1, -1),
	  m_networkInfo()
{
	SetCategory(11);
}

TelBbmcNetworkEvent::TelBbmcNetworkEvent(const TelBbmcNetworkEvent &src)
	: TelEventInfo(src),
	  m_networkInfo(src.m_networkInfo)
{
}

TelBbmcNetworkEvent::~TelBbmcNetworkEvent()
{
}

paf::common::SharedPtr<TelObject> TelBbmcNetworkEvent::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelBbmcNetworkEvent(*this));
}

TelBbmcCallEndEvent::TelBbmcCallEndEvent()
	: TelEventInfo(TEL_BBMC_EVENT_CALL_END_REASON, -1, -1),
	  m_reason()
{
	SetCategory(23);
}

TelBbmcCallEndEvent::TelBbmcCallEndEvent(const TelBbmcCallEndEvent &src)
	: TelEventInfo(src),
	  m_reason(src.m_reason)
{
}

TelBbmcCallEndEvent::~TelBbmcCallEndEvent()
{
}

paf::common::SharedPtr<TelObject> TelBbmcCallEndEvent::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelBbmcCallEndEvent(*this));
}

TelBbmcBearerEvent::TelBbmcBearerEvent()
	: TelEventInfo(TEL_BBMC_EVENT_BEARER_TECH, -1, -1),
	  m_bearer()
{
	SetCategory(22);
}

TelBbmcBearerEvent::TelBbmcBearerEvent(const TelBbmcBearerEvent &src)
	: TelEventInfo(src),
	  m_bearer(src.m_bearer)
{
}

TelBbmcBearerEvent::~TelBbmcBearerEvent()
{
}

paf::common::SharedPtr<TelObject> TelBbmcBearerEvent::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelBbmcBearerEvent(*this));
}

/* TelBbmcAdapter */
TelBbmcAdapter::TelBbmcAdapter()
	: TelBbmcAdapterInterface(),
	  m_sharedData(NULL),
	  m_generalHandler(NULL),
	  m_simHandler(NULL),
	  m_smsHandler(NULL),
	  m_packetHandler(NULL),
	  m_subscribers()
{
}

TelBbmcAdapter::~TelBbmcAdapter()
{
}

TelephonyResult TelBbmcAdapter::GetPacketBearerInfo(TelPacketBearerInfo *outBearer)
{
	TelephonyResult res(TEL_OK);

	if (!m_packetHandler->IsPacketCallActive()) {
		outBearer->SetBearerType(1);
		return res;
	}

	outBearer->SetBearerType(0);
	SceBbmcBearerTech bearer;
	sce_paf_memset(&bearer, 0, sizeof(bearer));
	m_packetHandler->GetCurrentBearerTech(&bearer);

	if (bearer.networkType == 1) {
		if ((bearer.techMask & 4U) != 0)
			outBearer->SetBearerType(4);
		else if ((bearer.techMask & 2U) != 0)
			outBearer->SetBearerType(3);
		else if ((bearer.techMask & 1U) != 0)
			outBearer->SetBearerType(2);
	} else if (bearer.networkType == 2) {
		if ((bearer.techMask & 8U) != 0)
			outBearer->SetBearerType(8);
		else if ((bearer.techMask & 4U) != 0)
			outBearer->SetBearerType(7);
		else if ((bearer.techMask & 1U) != 0)
			outBearer->SetBearerType(5);
		else if ((bearer.techMask & 0x10U) != 0)
			outBearer->SetBearerType(9);
		else if ((bearer.techMask & 2U) != 0)
			outBearer->SetBearerType(6);
	}

	return res;
}

TelephonyResult TelBbmcAdapter::GetCallEndReasonInfo(TelCallEndReasonInfo *outReason)
{
	SceBbmcCallEndReason reason;
	sce_paf_memset(&reason, 0, sizeof(reason));
	TelephonyResult res = m_packetHandler->GetLastCallEndReason(&reason);
	if (res.IsSuccess())
		outReason->SetSingleReason(reason.type, reason.reason);
	return res;
}

TelephonyResult TelBbmcAdapter::GetNetworkInfo(TelNetworkInfo *outInfo)
{
	return m_generalHandler->GetNetworkInfo(outInfo);
}

TelephonyResult TelBbmcAdapter::HandleHandlerEvent(int eventId, int eventArg)
{
	TelephonyResult res(TEL_OK);

	if (eventId == TEL_BBMC_EVENT_NETWORK_INFO) {
		TelNetworkInfo info;
		m_generalHandler->GetNetworkInfo(&info);
		TelBbmcNetworkEvent netEvent;
		netEvent.SetNetworkInfo(info);
		NotifyObservers(netEvent);
		return res;
	}

	if (eventId == TEL_BBMC_EVENT_BEARER_TECH) {
		TelPacketBearerInfo bearer;
		GetPacketBearerInfo(&bearer);
		TelBbmcBearerEvent bearerEvent;
		bearerEvent.SetBearerInfo(bearer);
		NotifyObservers(bearerEvent);
		return res;
	}

	if (eventId == TEL_BBMC_EVENT_CALL_END_REASON) {
		TelCallEndReasonInfo reason;
		GetCallEndReasonInfo(&reason);
		TelBbmcCallEndEvent callEndEvent;
		callEndEvent.SetCallEndReason(reason);
		NotifyObservers(callEndEvent);
		return res;
	}

	if (eventId == TEL_BBMC_EVENT_NET_REJECT) {
		SceBbmcRejectCause cause;
		sce_paf_memset(&cause, 0, sizeof(cause));
		m_generalHandler->GetLastRejectCause(&cause);
		TelEventInfo rejectEvent(TEL_BBMC_EVENT_NET_REJECT,
					 static_cast<int>(cause.domain),
					 static_cast<int>(cause.cause));
		NotifyObservers(rejectEvent);
		return res;
	}

	if (eventId == TEL_BBMC_EVENT_SIM_INSERTED || eventId == TEL_BBMC_EVENT_SIM_REMOVED)
		CheckSimCondition();

	TelEventInfo event(eventId, eventArg, -1);
	NotifyObservers(event);
	return res;
}

TelephonyResult TelBbmcAdapter::Initialize()
{
	TelephonyResult res = TelBbmcSharedData::Initialize();
	if (!res.IsSuccess())
		return res;

	m_sharedData = TelBbmcSharedData::GetInstance();
	m_generalHandler = new TelBbmcGeneralHandler(m_sharedData);
	m_simHandler = new TelBbmcSimHandler(m_sharedData);
	m_smsHandler = new TelBbmcSmsHandler(m_sharedData);
	m_packetHandler = new TelBbmcPacketHandler(m_sharedData);

	if (m_generalHandler == NULL || m_simHandler == NULL ||
	    m_smsHandler == NULL || m_packetHandler == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		Terminate();
		return res;
	}

	TelBbmcSubscriberThread *subscriber =
		new TelBbmcSubscriberThread("SceTelBbmcGenSubscriber",
					    kBbmcSubscriberThreadPriority,
					    kBbmcSubscriberStackSize);
	subscriber->AddHandler(m_generalHandler);
	subscriber->AddHandler(m_simHandler);
	subscriber->AddHandler(m_smsHandler);
	subscriber->AddHandler(m_packetHandler);
	m_subscribers.push_back(subscriber);

	for (SceSize index = 0; index < m_subscribers.size(); ++index) {
		res = m_subscribers[index]->StartSubscriber(this);
		if (!res.IsSuccess())
			break;
	}

	return res;
}

TelephonyResult TelBbmcAdapter::Terminate()
{
	for (SceSize index = 0; index < m_subscribers.size(); ++index) {
		if (m_subscribers[index] != NULL) {
			m_subscribers[index]->StopThread();
			delete m_subscribers[index];
			m_subscribers[index] = NULL;
		}
	}

	if (m_packetHandler != NULL) {
		delete m_packetHandler;
		m_packetHandler = NULL;
	}
	if (m_smsHandler != NULL) {
		delete m_smsHandler;
		m_smsHandler = NULL;
	}
	if (m_simHandler != NULL) {
		delete m_simHandler;
		m_simHandler = NULL;
	}
	if (m_generalHandler != NULL) {
		delete m_generalHandler;
		m_generalHandler = NULL;
	}

	TelephonyResult res = TelBbmcSharedData::Terminate();
	m_sharedData = NULL;
	return res;
}

TelephonyResult TelBbmcAdapter::TurnOn(bool lpmEnabled)
{
	TelephonyResult res = m_generalHandler->ReserveOperationMode(1);
	if (res.IsSuccess()) {
		res = m_generalHandler->SetLowPowerMode(lpmEnabled, false);
		if (res.IsSuccess())
			res = m_generalHandler->TurnOn();
	}
	return res;
}

TelephonyResult TelBbmcAdapter::TurnOnDefault()
{
	return TurnOn(false);
}

TelephonyResult TelBbmcAdapter::TurnOff(bool forceStop)
{
	return m_generalHandler->TurnOff(forceStop);
}

TelephonyResult TelBbmcAdapter::VerifyPin(const TelPinCode &pin)
{
	return m_simHandler->VerifyPin(pin);
}

TelephonyResult TelBbmcAdapter::UnblockPin(const TelPukCode &puk, const TelPinCode &newPin)
{
	return m_simHandler->UnblockPin(puk, newPin);
}

TelephonyResult TelBbmcAdapter::GetPinStatus(int *outStatus)
{
	return m_simHandler->GetPinStatus(outStatus);
}

TelephonyResult TelBbmcAdapter::GetPhoneNumber(TelPhoneNumber *outNumber)
{
	paf::string msisdn = m_sharedData->GetString(TEL_ATTR_BBMC_MSISDN);
	if (msisdn.empty())
		m_generalHandler->FetchDeviceInformation();
	return m_generalHandler->GetPhoneNumber(outNumber);
}

TelephonyResult TelBbmcAdapter::SetLowPowerModeSync(bool enableLpm)
{
	return m_generalHandler->SetLowPowerMode(enableLpm, true);
}

TelephonyResult TelBbmcAdapter::SetLowPowerModeAsync(bool enableLpm)
{
	return m_generalHandler->SetLowPowerMode(enableLpm, false);
}

TelephonyResult TelBbmcAdapter::ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin)
{
	return m_simHandler->ChangePin(oldPin, newPin);
}

TelephonyResult TelBbmcAdapter::SetPinEnabled(const bool *enable, const TelPinCode &pin)
{
	return m_simHandler->SetPinEnabled(enable, pin);
}

TelephonyResult TelBbmcAdapter::GetPinEnabled(bool *outEnabled)
{
	return m_simHandler->GetPinEnabled(outEnabled);
}

TelephonyResult TelBbmcAdapter::SendSms(const TelSmsProtocolDataUnit &pdu)
{
	return m_smsHandler->SendSms(pdu);
}

TelephonyResult TelBbmcAdapter::ReadSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outIndex)
{
	return m_smsHandler->ReadNextSms(outPdu, outIndex);
}

TelephonyResult TelBbmcAdapter::DeleteSms(const SceUInt32 *memoryIndex)
{
	return m_smsHandler->DeleteSms(memoryIndex);
}

TelephonyResult TelBbmcAdapter::GetSignalLevel(TelAntennaLevelThreshold *outLevel)
{
	return m_generalHandler->GetSignalLevel(outLevel);
}

TelephonyResult TelBbmcAdapter::GetPinRetryCount(SceUInt16 *outRetries)
{
	int retries = 0;
	TelephonyResult res = m_simHandler->GetPinRetryCount(&retries);
	*outRetries = static_cast<SceUInt16>(retries);
	return res;
}

TelephonyResult TelBbmcAdapter::GetRoamingInfo(TelRoamingInfo *outRoaming)
{
	return m_generalHandler->GetRoamingInfo(outRoaming);
}

TelephonyResult TelBbmcAdapter::GetServingPlmn(TelephonyPlmn *outPlmn)
{
	return m_generalHandler->GetServingPlmn(outPlmn);
}

TelephonyResult TelBbmcAdapter::SetApnCredentials(const TelephonyOperator &credentials)
{
	TelephonyResult res(TEL_OK);
	if (!credentials.IsValid()) {
		res.SetCode(TEL_ERR_IO_FAILED);
		return res;
	}

	SceBbmcProfileSettings profile;
	sce_paf_memset(&profile, 0, sizeof(profile));

	paf::string apnName;
	paf::string userId;
	paf::string password;
	credentials.GetCredentials(&apnName, &userId, &password);

	sce_paf_memcpy(profile.apn, apnName.c_str(), apnName.size());
	profile.apnLength = apnName.size();
	sce_paf_memcpy(profile.username, userId.c_str(), userId.size());
	profile.usernameLength = userId.size();
	sce_paf_memcpy(profile.password, password.c_str(), password.size());
	profile.passwordLength = password.size();

	SceUInt32 useAuth = ((userId.size() | password.size()) != 0) ? 1U : 0U;
	profile.papAuthentication = useAuth;
	profile.chapAuthentication = useAuth;
	profile.validFields = 0x0E04U;

	res = m_generalHandler->SetProfileSettings(&profile);
	if (!res.IsSuccess())
		return res;

	sce_paf_memset(&profile, 0, sizeof(profile));
	res = m_generalHandler->GetProfileSettings(&profile);
	if (!res.IsSuccess())
		return res;

	if (profile.apnLength != apnName.size() ||
	    sce_paf_strncmp(profile.apn, apnName.c_str(), apnName.size()) != 0 ||
	    profile.usernameLength != userId.size() ||
	    sce_paf_strncmp(profile.username, userId.c_str(), userId.size()) != 0 ||
	    profile.passwordLength != password.size() ||
	    sce_paf_strncmp(profile.password, password.c_str(), password.size()) != 0) {
		res.SetCode(TEL_ERR_APN_VERIFY);
	}

	return res;
}

TelephonyResult TelBbmcAdapter::GetImsi(TelImeiInfo *outImsi)
{
	return m_generalHandler->GetImsi(outImsi);
}

TelephonyResult TelBbmcAdapter::StartNetworkScan()
{
	return m_generalHandler->StartNetworkScan();
}

TelephonyResult TelBbmcAdapter::GetNetworkScanResult(TelNetScanInfoList *outList)
{
	TelephonyResult res(TEL_OK);
	SceBbmcNetworkScanInfo *scanInfo =
		static_cast<SceBbmcNetworkScanInfo *>(telMalloc(sizeof(SceBbmcNetworkScanInfo)));
	if (scanInfo == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		return res;
	}

	res = m_generalHandler->GetNetworkScanInfo(scanInfo);
	if (res.IsSuccess() && scanInfo->count <= 10U) {
		TelNetScanInfo entry;
		for (SceUInt16 index = 0; index < scanInfo->count; ++index) {
			const SceBbmcScannedNetwork &net = scanInfo->networks[index];
			entry.SetPlmnCodes(net.mcc, net.mnc);
			paf::string desc(net.description);
			entry.SetOperatorNameUtf8(desc);

			entry.SetStatus((net.forbiddenStatus == 1 || net.forbiddenStatus == 2)
						? static_cast<int>(net.forbiddenStatus)
						: 0);
			entry.SetAccessTech((net.preferredStatus == 1 || net.preferredStatus == 2)
						    ? static_cast<int>(net.preferredStatus)
						    : 0);
			entry.SetRoamingState((net.availabilityStatus == 1 || net.availabilityStatus == 2)
						      ? static_cast<int>(net.availabilityStatus)
						      : 0);
			entry.SetSignalQuality((net.currentStatus == 1 || net.currentStatus == 2)
						       ? static_cast<int>(net.currentStatus)
						       : 0);

			int radioCap = 0;
			if (net.radioInterface == 4)
				radioCap = 1;
			else if (net.radioInterface == 5)
				radioCap = 2;
			else if (net.radioInterface == 8)
				radioCap = 3;
			entry.SetRadioCapability(radioCap);

			if (outList != NULL)
				outList->GetEntries().push_back(entry);
		}
	}

	telFree(scanInfo);
	return res;
}

TelephonyResult TelBbmcAdapter::AbortNetworkScan()
{
	return m_generalHandler->AbortNetworkScan();
}

TelephonyResult TelBbmcAdapter::RegisterNetworkManual(const TelephonyPlmn &plmn,
						      int radioCapability)
{
	TelephonyResult res(TEL_OK);
	SceBbmcNetworkRegistration reg;
	sce_paf_memset(&reg, 0, sizeof(reg));
	reg.action = 2;
	reg.mcc = static_cast<SceUInt16>(plmn.GetMcc());
	reg.mnc = static_cast<SceUInt16>(plmn.GetMnc());

	if (radioCapability == 1) {
		reg.radioInterface = 4;
	} else if (radioCapability == 2) {
		reg.radioInterface = 5;
	} else if (radioCapability == 3) {
		reg.radioInterface = 8;
	} else {
		res.SetCode(TEL_ERR_INVALID_ARG);
		return res;
	}

	res = m_generalHandler->SetNetworkRegistration(&reg);
	m_generalHandler->RefreshNetworkInfo();
	return res;
}

TelephonyResult TelBbmcAdapter::RegisterNetworkAuto()
{
	SceBbmcNetworkRegistration reg;
	sce_paf_memset(&reg, 0, sizeof(reg));
	reg.action = 1;

	TelephonyResult res = m_generalHandler->SetNetworkRegistration(&reg);
	m_generalHandler->RefreshNetworkInfo();
	return res;
}

TelephonyResult TelBbmcAdapter::SetRssiThresholds(const paf::vector<int> &thresholds)
{
	TelephonyResult res(TEL_OK);
	SceBbmcRssiThresholdList list;
	sce_paf_memset(&list, 0, sizeof(list));

	SceSize count = thresholds.size();
	if (count > 10U)
		res.SetCode(TEL_ERR_IO_FAILED);

	list.count = static_cast<SceUInt8>(count);
	for (SceSize index = 0; index < count && index < 11U; ++index) {
		int val = thresholds[index];
		if (val >= 110)
			val = 110;
		if (val < 48)
			val = 48;
		list.thresholds[index] = static_cast<SceInt8>(val);
	}

	return m_generalHandler->SetRssiThresholds(&list);
}

TelephonyResult TelBbmcAdapter::GetImei(paf::string *outImei)
{
	return m_generalHandler->GetImei(outImei);
}

TelephonyResult TelBbmcAdapter::GetIccid(paf::string *outIccid)
{
	return m_generalHandler->GetIccid(outIccid);
}

TelephonyResult TelBbmcAdapter::GetHomePlmn(TelephonyPlmn *outPlmn)
{
	return m_generalHandler->GetHomePlmn(outPlmn);
}

TelephonyResult TelBbmcAdapter::CheckSimCondition()
{
	return m_sharedData->CheckSimCondition();
}

TelephonyResult TelBbmcAdapter::GetControlKeyState(int *outState)
{
	TelephonyResult res(TEL_OK);
	if (m_sharedData->GetBool(TEL_ATTR_BBMC_CK_PERMANENT_BLOCKED)) {
		*outState = 3;
		return res;
	}

	int ckState = m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE);
	if (ckState == 1)
		*outState = 0;
	else if (ckState == 2)
		*outState = 1;
	else if (ckState == 3)
		*outState = 2;
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	return res;
}

TelephonyResult TelBbmcAdapter::GetControlKeyBlockType(int *outBlockType)
{
	TelephonyResult res(TEL_OK);
	if (m_sharedData->GetBool(TEL_ATTR_BBMC_CK_PERMANENT_BLOCKED)) {
		*outBlockType = 1;
		return res;
	}

	res = m_sharedData->CheckSimCondition();
	if (!res.IsSuccess())
		return res;

	int blockType = m_sharedData->GetInt(TEL_ATTR_BBMC_CK_BLOCK_TYPE);
	if (blockType == 1)
		*outBlockType = 0;
	else if (blockType == 2)
		*outBlockType = 1;
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	return res;
}

TelephonyResult TelBbmcAdapter::GetControlKeyVerifyRetries(int *outRetries)
{
	SceBbmcControlKeyStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	TelephonyResult res = m_simHandler->GetControlKeyStatus(&status);
	if (res.IsSuccess())
		*outRetries = status.verifyRetries;
	return res;
}

TelephonyResult TelBbmcAdapter::GetControlKeyUnblockRetries(int *outRetries)
{
	SceBbmcControlKeyStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	TelephonyResult res = m_simHandler->GetControlKeyStatus(&status);
	if (res.IsSuccess())
		*outRetries = status.unblockRetries;
	return res;
}

TelephonyResult TelBbmcAdapter::DepersonalizeControlKey(const TelControlKey &key)
{
	if (!key.IsValid())
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	return m_simHandler->DepersonalizeControlKey(key.GetKey());
}

TelephonyResult TelBbmcAdapter::UnblockControlKey(const TelControlKey &key)
{
	if (!key.IsValid())
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	return m_simHandler->UnblockControlKey(key.GetKey());
}

TelephonyResult TelBbmcAdapter::SetRoamingControlBit(bool enable)
{
	return m_packetHandler->SetRoamingControlBit(enable);
}

TelephonyResult TelBbmcAdapter::SetPacketControlBit(bool enable)
{
	return m_packetHandler->SetPacketControlBit(enable);
}

TelephonyResult TelBbmcAdapter::ClearSuspendControlBit()
{
	return m_packetHandler->SetSuspendControlBit(false);
}

TelephonyResult TelBbmcAdapter::SetSuspendControlBit()
{
	return m_packetHandler->SetSuspendControlBit(true);
}

TelephonyResult TelBbmcAdapter::ReleasePdpContext()
{
	return m_packetHandler->ReleasePdpContext();
}

TelephonyResult TelBbmcAdapter::SyncGeneralHandler()
{
	return m_generalHandler->SyncBarrier();
}

TelephonyResult TelBbmcAdapter::GetPlmnModeBit(int *outModeBit)
{
	return m_simHandler->GetPlmnModeBit(outModeBit);
}

TelephonyResult TelBbmcAdapter::FetchDeviceInformation()
{
	return m_generalHandler->FetchDeviceInformation();
}

TelephonyResult TelBbmcAdapter::FetchDefaultProfileSettings()
{
	return m_generalHandler->GetProfileSettings(NULL);
}

TelephonyResult TelBbmcAdapter::GetLatestArchiveVersion(SceUInt32 *outVersion)
{
	return m_generalHandler->GetLatestArchiveVersion(outVersion);
}

TelephonyResult TelBbmcAdapter::GetArchiveVersion(SceUInt32 *outVersion)
{
	return m_generalHandler->GetArchiveVersion(outVersion);
}

TelephonyResult TelBbmcAdapter::CheckComVersion(bool *outMatches)
{
	return m_generalHandler->CheckComVersion(outMatches);
}

TelephonyResult TelBbmcAdapter::GetFirmwareRevision(paf::string *outRevision)
{
	TelephonyResult res(TEL_OK);
	*outRevision = m_sharedData->GetString(TEL_ATTR_BBMC_REVISION);
	if (outRevision->empty()) {
		res = m_generalHandler->FetchComFirmVersion();
		*outRevision = m_sharedData->GetString(TEL_ATTR_BBMC_REVISION);
	}
	return res;
}

TelephonyResult TelBbmcAdapter::GetNetworkStatusReport(paf::string *outReport)
{
	TelephonyResult res(TEL_OK);
	*outReport = "";

	SceBbmcNetworkInfo *rawInfo = new SceBbmcNetworkInfo;
	if (rawInfo != NULL) {
		sce_paf_memset(rawInfo, 0, sizeof(*rawInfo));
		res = m_generalHandler->GetRawNetworkInfo(rawInfo);
		if (res.IsSuccess()) {
			if ((rawInfo->serving.validFlags & 4U) != 0) {
				outReport->append(paf::string("MCC: ") +
						  telFormatString("%03d\n", rawInfo->serving.mcc));
				outReport->append("MNC: ");
				if (rawInfo->serving.mnc >= 10U)
					outReport->append(telFormatString("%d\n", rawInfo->serving.mnc));
				else
					outReport->append(telFormatString("%02d\n", rawInfo->serving.mnc));

				outReport->append("Registration Network: ");
				if (rawInfo->serving.selectedNetwork == 1)
					outReport->append("3GPP2\n");
				else if (rawInfo->serving.selectedNetwork == 2)
					outReport->append("3GPP\n");
				else
					outReport->append("UNKNOWN\n");

				if (rawInfo->serving.descriptionLength != 0) {
					outReport->append(paf::string("Description: ") +
							  paf::string(rawInfo->serving.description) + "\n");
				}
			}

			outReport->append("CS: ");
			if (rawInfo->serving.csAttachState == 1)
				outReport->append("ATTACHED\n");
			else if (rawInfo->serving.csAttachState == 2)
				outReport->append("DETACHED\n");
			else
				outReport->append("N/A\n");

			outReport->append("PS: ");
			if (rawInfo->serving.psAttachState == 1)
				outReport->append("ATTACHED\n");
			else if (rawInfo->serving.psAttachState == 2)
				outReport->append("DETACHED\n");
			else
				outReport->append("N/A\n");

			outReport->append("Registration Status: ");
			if (rawInfo->serving.registrationState == 0)
				outReport->append("NOT REGISTERED\n");
			else if (rawInfo->serving.registrationState == 1)
				outReport->append("REGISTERED\n");
			else if (rawInfo->serving.registrationState == 2)
				outReport->append("SEARCHING\n");
			else if (rawInfo->serving.registrationState == 3)
				outReport->append("DENIED\n");
			else
				outReport->append("N/A\n");

			SceBbmcRejectCause cause;
			sce_paf_memset(&cause, 0, sizeof(cause));
			if (m_generalHandler->GetLastRejectCause(&cause).IsSuccess()) {
				outReport->append("Reject Cause: ");
				if (cause.domain == 1)
					outReport->append("CS ");
				else if (cause.domain == 2)
					outReport->append("PS ");
				else if (cause.domain == 3)
					outReport->append("CS+PS ");
				else
					outReport->append("N/A ");
			}
			outReport->append(telFormatString("%d\n", cause.cause));

			if ((rawInfo->serving.validFlags & 1U) != 0) {
				outReport->append("Roaming: ");
				if (rawInfo->serving.roamingIndicator == 0)
					outReport->append("ROAMING\n");
				else if (rawInfo->serving.roamingIndicator == 1)
					outReport->append("HOME\n");
				else if (rawInfo->serving.roamingIndicator == 2)
					outReport->append("FLASHING\n");
				else
					outReport->append("UNKNOWN\n");
			}

			if ((rawInfo->signal.validMask & 1U) != 0 &&
			    rawInfo->signal.rssiCount != 0) {
				outReport->append("Signal:\n");
				for (SceUInt16 index = 0; index < rawInfo->signal.rssiCount; ++index) {
					const SceBbmcSignalStrength &item = rawInfo->signal.rssi[index];
					outReport->append(paf::string(" RSSI: ") +
							  telFormatString("%d ",
									  static_cast<SceUInt8>(item.strength)));
					switch (item.radioInterface) {
					case 0:
						outReport->append("NO SERVICE");
						break;
					case 1:
						outReport->append("CDMA 1x");
						break;
					case 2:
						outReport->append("CDMA2000 1xEVDO");
						break;
					case 3:
						outReport->append("AMPS");
						break;
					case 4:
						outReport->append("GSM");
						break;
					case 5:
						outReport->append("UMTS");
						break;
					case 8:
						outReport->append("LTE");
						break;
					default:
						outReport->append("UNKNOWN");
						break;
					}
					outReport->append("\n");
				}
			}
		}
		delete rawInfo;
	}

	outReport->append("Packet Call: ");
	if (m_packetHandler->IsPacketCallActive()) {
		outReport->append("Started\n");
		SceBbmcBearerTech bearer;
		sce_paf_memset(&bearer, 0, sizeof(bearer));
		if (m_packetHandler->GetCurrentBearerTech(&bearer).IsSuccess()) {
			outReport->append("Packet Bearer Technology: ");
			if (bearer.networkType == 1) {
				outReport->append("CDMA ");
				if ((bearer.techMask & 1U) != 0)
					outReport->append("CDMA_1X ");
				if ((bearer.techMask & 2U) != 0)
					outReport->append("EV-DO_Rev.0 ");
				if ((bearer.techMask & 4U) != 0)
					outReport->append("EV-DO_Rev.A ");
				outReport->append("\n");
			} else if (bearer.networkType == 2) {
				outReport->append("UMTS ");
				if ((bearer.techMask & 1U) != 0)
					outReport->append("WCDMA ");
				if ((bearer.techMask & 2U) != 0)
					outReport->append("GPRS ");
				if ((bearer.techMask & 4U) != 0)
					outReport->append("HSDPA ");
				if ((bearer.techMask & 8U) != 0)
					outReport->append("HSUPA ");
				if ((bearer.techMask & 0x10U) != 0)
					outReport->append("EDGE ");
				outReport->append("\n");
			} else {
				outReport->append("UNKNOWN\n");
			}
		}
	} else {
		outReport->append("Ended\n");
		SceBbmcCallEndReason reason;
		sce_paf_memset(&reason, 0, sizeof(reason));
		if (m_packetHandler->GetLastCallEndReason(&reason).IsSuccess() && reason.type != 0) {
			outReport->append("Call End Reason: ");
			outReport->append(paf::string("type=") + telFormatString("%d ", reason.type));
			outReport->append(paf::string("reason=") + telFormatString("%d\n", reason.reason));
		}
	}

	res.SetCode(TEL_OK);
	return res;
}

TelephonyResult TelBbmcAdapter::GetDeviceInfoReport(paf::string *outReport)
{
	TelephonyResult res(TEL_OK);
	*outReport = "";

	if (m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT) != 0 &&
	    !m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID)) {
		m_generalHandler->FetchDeviceInformation();
	}

	if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID)) {
		paf::string devSection;
		devSection.append(paf::string("Manufacture: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_MANUFACTURER) + "\n");
		devSection.append(paf::string("Model ID: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_MODEL) + "\n");
		devSection.append(paf::string("Revision ID: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_REVISION) + "\n");
		devSection.append(paf::string("Voice Number: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_MSISDN) + "\n");
		devSection.append(paf::string("ESN: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_ESN) + "\n");
		devSection.append(paf::string("IMEI: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_IMEI) + "\n");
		devSection.append(paf::string("MEID: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_MEID) + "\n");
		devSection.append(paf::string("ICC ID: ") +
				  m_sharedData->GetString(TEL_ATTR_BBMC_ICCID) + "\n");
		devSection.append(paf::string("Protocol: ") +
				  telFormatString("%d\n", m_sharedData->GetInt(TEL_ATTR_BBMC_PROTOCOL)));
		devSection.append(paf::string("Software Version: ") +
				  telFormatString("%d\n", m_sharedData->GetInt(TEL_ATTR_BBMC_SW_VERSION)));
		devSection.append(paf::string("Home Network: ") +
				  telFormatString("%d/%d\n",
						  m_sharedData->GetInt(TEL_ATTR_BBMC_HOME_MCC),
						  m_sharedData->GetInt(TEL_ATTR_BBMC_HOME_MNC)));
		outReport->append(devSection);
	}

	int ckState = m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE);
	if (ckState >= 1 && ckState <= 3) {
		outReport->append("SIM Lock: ");
		if (ckState == 1)
			outReport->append("DEACTIVATE\n");
		else if (ckState == 2)
			outReport->append("ACTIVATE\n");
		else if (ckState == 3)
			outReport->append("BLOCKED\n");
	}

	int blockType = m_sharedData->GetInt(TEL_ATTR_BBMC_CK_BLOCK_TYPE);
	if (blockType >= 1 && blockType <= 2) {
		outReport->append("Normal Operation: ");
		if (blockType == 1)
			outReport->append("PERMIT\n");
		else
			outReport->append("BLOCKED\n");
	}

	paf::string diagSection;
	paf::string stateHist = m_sharedData->FormatStateHistory();
	diagSection.append(telFormatString("ST: %s\n", stateHist.c_str()));
	diagSection.append(telFormatString("EV: %08x %08x %08x %08x\n",
					   m_generalHandler->GetClearedEvents(),
					   m_simHandler->GetClearedEvents(),
					   m_packetHandler->GetClearedEvents(),
					   m_smsHandler->GetClearedEvents()));
	diagSection.append(telFormatString("IATRUCT: %x\n", m_generalHandler->CheckTemperatureStatus()));
	diagSection.append(telFormatString("PCF: %x C%d E%d\n",
					   m_packetHandler->GetControlFlagValue(),
					   m_packetHandler->GetControlSetCount(),
					   m_packetHandler->GetControlEventCount()));
	diagSection.append(telFormatString("SC: E%d D%d\n",
					   m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_ENABLE_COUNT),
					   m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT)));
	diagSection.append(telFormatString("CBF: B%d F%d\n",
					   m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT),
					   m_sharedData->GetInt(TEL_ATTR_BBMC_FAILURE_COUNT)));
	outReport->append(diagSection);

	paf::string updaterSection;
	SceUInt32 archiveVer = 0;
	if (GetLatestArchiveVersion(&archiveVer).IsSuccess())
		updaterSection.append(telFormatString("LARVER: %x\n", archiveVer));
	if (GetArchiveVersion(&archiveVer).IsSuccess())
		updaterSection.append(telFormatString("ARVER: %x\n", archiveVer));
	bool comMatches = false;
	if (CheckComVersion(&comMatches).IsSuccess())
		updaterSection.append(telFormatString("CHKCOM: %x\n", comMatches ? 1 : 0));
	outReport->append(updaterSection);

	res.SetCode(TEL_OK);
	return res;
}

TelephonyResult TelBbmcAdapter::StartUsbExtIf()
{
	TelephonyResult res(TEL_OK);
	if (sceBbmcUsbExtIfStart(1) < 0)
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	return res;
}

TelephonyResult TelBbmcAdapter::StopUsbExtIf()
{
	TelephonyResult res(TEL_OK);
	if (sceBbmcUsbExtIfStop() < 0)
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	return res;
}

TelephonyResult TelBbmcAdapter::SetShellDebugMedia(int media)
{
	TelephonyResult res = SelectDebugOutMedia(2, media);
	if (res.IsSuccess())
		g_bbmcDebugOutEnabled = true;
	return res;
}

TelephonyResult TelBbmcAdapter::SetModemDebugMedia(int media)
{
	return SelectDebugOutMedia(0, media);
}

TelephonyResult TelBbmcAdapter::SetDriverDebugMedia(int media)
{
	return SelectDebugOutMedia(1, media);
}

int TelBbmcAdapter::WriteDebugString(const char *text)
{
	TelephonyResult res(TEL_OK);
	if (g_bbmcDebugOutEnabled) {
		SceSize len = sce_paf_strlen(text);
		int ret = sceBbmcDebugOutString(2, text, len + 1);
		if (ret < 0 && ret != SCE_BBMC_ERROR_BUSY)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
	}
	return res.GetCode();
}

TelephonyResult TelBbmcAdapter::RequestFileRefresh()
{
	return m_simHandler->RequestFileRefresh();
}

void TelBbmcAdapter::ClearVoiceNumber()
{
	m_sharedData->SetString(TEL_ATTR_BBMC_MSISDN, paf::string(""));
}

void TelBbmcAdapter::SetGeneralDebugFlags(int flags)
{
	m_generalHandler->SetDebugFlags(flags);
}

TelephonyResult TelBbmcAdapter::SelectDebugOutMedia(int channel, int media)
{
	TelephonyResult res(TEL_OK);
	if (media != 1 && media != 2) {
		res.SetCode(TEL_ERR_IO_FAILED);
		return res;
	}
	if (sceBbmcDebugSelectOutMedia(channel, media) < 0)
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	return res;
}

} /* namespace telephony */
