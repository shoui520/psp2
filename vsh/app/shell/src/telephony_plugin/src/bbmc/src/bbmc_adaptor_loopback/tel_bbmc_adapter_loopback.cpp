/*
 * tel_bbmc_adapter_loopback.cpp: BBMC loopback adapter for testing telephony
 * without cellular hardware (3.65-CEX: 0x813D6B20..0x813D8477).
 */
#include <std/string.h>
#include <std/stdlib.h>
#include <ini_file_processor/ini_file_processor.h>
#include "../../tel_bbmc_adapter.h"
#include "../../../sms_pdu_transcoder/tel_sms_pdu_transcoder.h"

namespace telephony {

static const char *g_loopbackPhoneNumber = "09012345678";
static const paf::string g_loopbackImsi("001011234567890");
static const TelPinCode g_loopbackDefaultPuk(paf::string("11111111"));

struct TelLoopbackSeqStep {
	int delayTicks;
	int actionCode;
	int actionValue;
};

static const int kMaxLoopbackSeqSteps = 64;
static TelLoopbackSeqStep g_loopbackSeqTable[kMaxLoopbackSeqSteps];

static const int kLoopbackMessageThreadPriority = 0x1000010E;
static const SceSize kLoopbackThreadStackSize = 0x4000;

TelBbmcAdapterLoopback::TelBbmcAdapterLoopback()
	: TelBbmcAdapterInterface(),
	  TelMessageThread("SceTelBbmcAdapterLoopback", kLoopbackMessageThreadPriority,
			   kLoopbackThreadStackSize),
	  m_core(),
	  m_smsInbox(),
	  m_periodicThread("SceTelBbmcLoopbackPeriodic", kLoopbackThreadStackSize),
	  m_signalLevel(0),
	  m_networkType(0),
	  m_roamingState(0),
	  m_bearerType(0),
	  m_simInserted(true),
	  m_mcc(1),
	  m_mnc(1),
	  m_pduGenerator(NULL),
	  m_autoRecvSms(false),
	  m_unusedCounter(0),
	  m_unusedFlag(0),
	  m_seqIndex(-1),
	  m_seqCount(0),
	  m_seqCountdown(0),
	  m_seqScale(0),
	  m_registrationState(1)
{
}

TelBbmcAdapterLoopback::~TelBbmcAdapterLoopback()
{
}

TelephonyResult TelBbmcAdapterLoopback::Initialize()
{
	TelephonyResult res = TelBbmcSharedData::Initialize();
	if (!res.IsSuccess())
		return res;

	TelBbmcLoopbackConfig config;
	config.simInserted = true;
	config.pinStatus = 0;
	config.pinCode = "0000";
	config.pukCode = "11111111";
	config.pinRetries = 3;
	config.simLockState = 1;
	config.simBlockType = 0;
	config.initialCheckEnabled = 0;
	config.unlockCode = "00000000";
	config.unblockCode = "11111111";
	config.ckRetries = 10;

	IniFileProcessor iniProc;
	IniConfig initParam;
	iniProc.initialize(&initParam);

	char keyBuf[256];
	char valBuf[256];
	sce_paf_memset(keyBuf, 0, sizeof(keyBuf));
	sce_paf_memset(valBuf, 0, sizeof(valBuf));

	int seqCount = 0;
	if (iniProc.open("vs0:vsh/shell/telephony/tel_config.ini", "r", 0) == 0) {
		while (iniProc.next(keyBuf, valBuf, sizeof(keyBuf)) == 0) {
			m_core.ApplyConfig(config);

			if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_AUTO_RECV", 24) == 0) {
				bool enableAuto = false;
				if (res.IsSuccess())
					enableAuto = (sce_paf_atoi(valBuf) == 1);
				m_autoRecvSms = enableAuto;
				if (enableAuto) {
					res = TelSmsPduGeneratorFactory::Create(
						TEL_SMS_FACTORY_GSM, &m_pduGenerator);
				}
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_PIN_STATUS", 25) == 0) {
				config.pinStatus = sce_paf_atoi(valBuf);
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_SIM_LOCK_STATE", 29) == 0) {
				config.simLockState = sce_paf_atoi(valBuf);
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_SIM_BLOCK_TYPE", 29) == 0) {
				config.simBlockType = sce_paf_atoi(valBuf);
			} else if (sce_paf_strncmp(keyBuf, "INITIAL_CHECK_ENABLED", 22) == 0) {
				config.initialCheckEnabled = sce_paf_atoi(valBuf);
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_PIN_CODE", 23) == 0) {
				config.pinCode = valBuf;
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_PUK_CODE", 23) == 0) {
				config.pukCode = valBuf;
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_PIN_REMAIN", 25) == 0) {
				config.pinRetries = sce_paf_atoi(valBuf);
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_UNLOCK_CODE", 26) == 0) {
				config.unlockCode = valBuf;
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_UNBLOCK_CODE", 27) == 0) {
				config.unblockCode = valBuf;
			} else if (sce_paf_strncmp(keyBuf, "BBMC_LOOPBACK_SEQ", 18) == 0) {
				if (seqCount < kMaxLoopbackSeqSteps) {
					char delayPart[256];
					char actionPart[256];
					sce_paf_memset(delayPart, 0, sizeof(delayPart));
					sce_paf_memset(actionPart, 0, sizeof(actionPart));

					SceSize commaIdx = 0;
					for (; commaIdx < sizeof(delayPart); ++commaIdx) {
						char ch = valBuf[commaIdx];
						if (ch == ',')
							break;
						delayPart[commaIdx] = ch;
					}

					SceSize actionOffset = 0;
					if (commaIdx == sizeof(delayPart)) {
						g_loopbackSeqTable[seqCount].delayTicks = 0;
					} else {
						int parsedDelay = sce_paf_atoi(delayPart);
						if (parsedDelay < 0) {
							g_loopbackSeqTable[seqCount].delayTicks = 0;
						} else {
							g_loopbackSeqTable[seqCount].delayTicks = parsedDelay;
							actionOffset = commaIdx;
						}

						SceSize actionLen = 0;
						const char *cursor = valBuf + actionOffset;
						while (actionLen < sizeof(actionPart)) {
							char ch = *++cursor;
							if (ch == '\0')
								break;
							actionPart[actionLen++] = ch;
						}

						g_loopbackSeqTable[seqCount].actionCode =
							(actionLen == sizeof(actionPart))
								? 0
								: sce_paf_atoi(actionPart);
					}
					++seqCount;
				}
			}
		}
	}

	iniProc.close();
	iniProc.terminate();

	m_seqIndex = 0;
	m_seqScale = 1;
	m_seqCount = seqCount;
	m_seqCountdown = g_loopbackSeqTable[0].delayTicks;

	StartThread(NULL);
	m_periodicThread.StartPeriodic(this, &m_core, m_autoRecvSms);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::Terminate()
{
	m_periodicThread.StopThread();
	StopThread();

	if (m_autoRecvSms && m_pduGenerator != NULL) {
		delete m_pduGenerator;
		m_pduGenerator = NULL;
	}

	return TelBbmcSharedData::Terminate();
}

TelephonyResult TelBbmcAdapterLoopback::TurnOnDefault()
{
	return TurnOn(true);
}

TelephonyResult TelBbmcAdapterLoopback::TurnOn(bool lpmEnabled)
{
	m_core.SetLowPowerMode(lpmEnabled);
	return m_core.TurnOn();
}

TelephonyResult TelBbmcAdapterLoopback::TurnOff(bool forceStop)
{
	(void)forceStop;
	return m_core.TurnOff();
}

TelephonyResult TelBbmcAdapterLoopback::VerifyPin(const TelPinCode &pin)
{
	return m_core.VerifyPin(pin, false);
}

TelephonyResult TelBbmcAdapterLoopback::UnblockPin(const TelPukCode &puk, const TelPinCode &newPin)
{
	return m_core.UnblockPin(puk, newPin);
}

TelephonyResult TelBbmcAdapterLoopback::GetPinStatus(int *outStatus)
{
	return m_core.GetPinStatus(outStatus);
}

TelephonyResult TelBbmcAdapterLoopback::GetPhoneNumber(TelPhoneNumber *outNumber)
{
	TelephonyResult res(TEL_OK);
	outNumber->SetDigits(g_loopbackPhoneNumber);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::SetLowPowerModeSync(bool enableLpm)
{
	return m_core.SetLowPowerMode(enableLpm);
}

TelephonyResult TelBbmcAdapterLoopback::SetLowPowerModeAsync(bool enableLpm)
{
	return m_core.SetLowPowerMode(enableLpm);
}

TelephonyResult TelBbmcAdapterLoopback::ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin)
{
	return m_core.ChangePin(oldPin, newPin);
}

TelephonyResult TelBbmcAdapterLoopback::SetPinEnabled(const bool *enable, const TelPinCode &pin)
{
	return m_core.SetPinEnabled(enable, pin);
}

TelephonyResult TelBbmcAdapterLoopback::GetPinEnabled(bool *outEnabled)
{
	return m_core.GetPinEnabled(outEnabled);
}

void TelBbmcAdapterLoopback::ForwardEvent(const TelEventInfo &event)
{
	int eventId = event.GetEventId();
	if (eventId == TEL_BBMC_EVENT_CALL_END_REASON || eventId == TEL_BBMC_EVENT_BEARER_TECH)
		return;

	if (eventId == TEL_BBMC_EVENT_LEAVE_ONLINE) {
		TelNetworkInfo clearedInfo;
		clearedInfo.SetNetworkType(0);
		clearedInfo.SetRegistrationState(0);
		clearedInfo.SetPlmnCodes(0, 0);
		clearedInfo.SetSignalRssi(0);
		clearedInfo.SetSignalLevel(0);
		clearedInfo.SetRoamingState(0);

		TelBbmcNetworkEvent netEvent;
		netEvent.SetNetworkInfo(clearedInfo);
		ForwardEvent(netEvent);
		NotifyObservers(event);
		return;
	}

	if (eventId == TEL_BBMC_EVENT_SIM_INSERTED || eventId == TEL_BBMC_EVENT_SIM_REMOVED)
		CheckSimCondition();

	NotifyObservers(event);
}

TelephonyResult TelBbmcAdapterLoopback::SendSms(const TelSmsProtocolDataUnit &pdu)
{
	TelephonyResult res(TEL_OK);
	SceUInt8 rawBytes[256];
	pdu.GetBytes(rawBytes, sizeof(rawBytes));
	m_smsInbox.push_back(pdu);

	TelEventInfo smsEvent(TEL_BBMC_EVENT_SMS_RECEIVED, -1, -1);
	ForwardEvent(smsEvent);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::ReadSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outIndex)
{
	TelephonyResult res(TEL_OK);
	if (m_smsInbox.size() == 0) {
		res.SetCode(TEL_ERR_NOT_FOUND);
		return res;
	}

	*outPdu = m_smsInbox.front();
	m_smsInbox.pop_front();

	TelPhoneNumber scAddress("00000000000");
	TelPhoneNumber origAddress;
	TelSmsProtocolDataUnit submitCopy(*outPdu);

	telConvertSubmitPduToLoopbackDeliver(scAddress, &submitCopy, &origAddress);
	telConvertSubmitPduToLoopbackDeliver(origAddress, outPdu, &scAddress);

	*outIndex = 0;
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::DeleteSms(const SceUInt32 *memoryIndex)
{
	(void)memoryIndex;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::GetPinRetryCount(SceUInt16 *outRetries)
{
	return m_core.GetPinRetryCount(outRetries);
}

TelephonyResult TelBbmcAdapterLoopback::GetSignalLevel(TelAntennaLevelThreshold *outLevel)
{
	TelephonyResult res(TEL_OK);
	outLevel->SetThreshold(m_signalLevel);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetNetworkInfo(TelNetworkInfo *outInfo)
{
	TelephonyResult res(TEL_OK);
	outInfo->SetNetworkType(m_networkType);
	outInfo->SetRegistrationState(m_registrationState);
	outInfo->SetPlmnCodes(m_mcc, m_mnc);

	int validity = (m_signalLevel != 0) ? 2 : 0;
	outInfo->SetSignalRssi(validity);
	outInfo->SetSignalLevel(m_signalLevel);
	outInfo->SetRoamingState(m_roamingState);
	outInfo->SetOperatorName(paf::string(""));
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetRoamingInfo(TelRoamingInfo *outRoaming)
{
	TelephonyResult res(TEL_OK);
	outRoaming->SetState(m_roamingState);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetServingPlmn(TelephonyPlmn *outPlmn)
{
	TelephonyResult res(TEL_OK);
	outPlmn->SetMccMnc(m_mcc, m_mnc);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::SetApnCredentials(const TelephonyOperator &credentials)
{
	(void)credentials;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::GetImsi(TelImeiInfo *outImsi)
{
	TelephonyResult res(TEL_OK);
	outImsi->SetImei(g_loopbackImsi);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::StartNetworkScan()
{
	TelephonyResult res(TEL_OK);
	m_core.PushEvent(TEL_BBMC_EVENT_NET_SCAN_DONE, 0);
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetNetworkScanResult(TelNetScanInfoList *outList)
{
	(void)outList;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::AbortNetworkScan()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::RegisterNetworkManual(const TelephonyPlmn &plmn,
							      int radioCapability)
{
	(void)plmn;
	(void)radioCapability;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::RegisterNetworkAuto()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::SetRssiThresholds(const paf::vector<int> &thresholds)
{
	(void)thresholds;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::GetImei(paf::string *outImei)
{
	TelephonyResult res(TEL_OK);
	*outImei = "012345678901234";
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetIccid(paf::string *outIccid)
{
	TelephonyResult res(TEL_OK);
	*outIccid = "01234567890123456789";
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetHomePlmn(TelephonyPlmn *outPlmn)
{
	(void)outPlmn;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::CheckSimCondition()
{
	return m_core.CheckSimCondition();
}

TelephonyResult TelBbmcAdapterLoopback::GetControlKeyState(int *outState)
{
	return m_core.GetControlKeyState(outState);
}

TelephonyResult TelBbmcAdapterLoopback::GetControlKeyBlockType(int *outBlockType)
{
	return m_core.GetControlKeyBlockType(outBlockType);
}

TelephonyResult TelBbmcAdapterLoopback::GetControlKeyVerifyRetries(int *outRetries)
{
	return m_core.GetControlKeyRetries(outRetries);
}

TelephonyResult TelBbmcAdapterLoopback::GetControlKeyUnblockRetries(int *outRetries)
{
	return m_core.GetControlKeyRetries(outRetries);
}

TelephonyResult TelBbmcAdapterLoopback::DepersonalizeControlKey(const TelControlKey &key)
{
	return m_core.DepersonalizeControlKey(key);
}

TelephonyResult TelBbmcAdapterLoopback::UnblockControlKey(const TelControlKey &key)
{
	return m_core.UnblockControlKey(key);
}

TelephonyResult TelBbmcAdapterLoopback::FetchDeviceInformation()
{
	TelephonyResult res(TEL_OK);
	m_core.MarkDeviceInfoReady();
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::FetchDefaultProfileSettings()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::SetRoamingControlBit(bool enable)
{
	(void)enable;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::SetPacketControlBit(bool enable)
{
	(void)enable;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::ClearSuspendControlBit()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::SetSuspendControlBit()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::GetPacketBearerInfo(TelPacketBearerInfo *outBearer)
{
	(void)outBearer;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::GetCallEndReasonInfo(TelCallEndReasonInfo *outReason)
{
	(void)outReason;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::ReleasePdpContext()
{
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::GetLatestArchiveVersion(SceUInt32 *outVersion)
{
	(void)outVersion;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::GetArchiveVersion(SceUInt32 *outVersion)
{
	(void)outVersion;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::CheckComVersion(bool *outMatches)
{
	(void)outMatches;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::SyncGeneralHandler()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcAdapterLoopback::GetPlmnModeBit(int *outModeBit)
{
	(void)outModeBit;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::GetNetworkStatusReport(paf::string *outReport)
{
	TelephonyResult res(TEL_OK);
	*outReport = "";

	if (m_core.GetCurrentState() == 7) {
		outReport->append(paf::string("mcc: ") + telFormatString("%03d\n", 1));
		outReport->append("mnc: ");
		outReport->append(telFormatString("%02d\n", 1));
	}

	int curState = m_core.GetCurrentState();
	outReport->append(telFormatString("ST: %d\n", curState));
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::GetDeviceInfoReport(paf::string *outReport)
{
	TelephonyResult res(TEL_OK);
	*outReport = "software version: N/A\n";
	return res;
}

TelephonyResult TelBbmcAdapterLoopback::StartUsbExtIf()
{
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::StopUsbExtIf()
{
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::SetShellDebugMedia(int media)
{
	(void)media;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::SetModemDebugMedia(int media)
{
	(void)media;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::SetDriverDebugMedia(int media)
{
	(void)media;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

TelephonyResult TelBbmcAdapterLoopback::GetFirmwareRevision(paf::string *outRevision)
{
	(void)outRevision;
	return TelephonyResult(TEL_ERR_NOT_IMPLEMENTED);
}

void TelBbmcAdapterLoopback::GenerateAutoRecvSms()
{
	TelSmsProtocolDataUnit pdu;
	TelSmsMessage message;

	paf::wstring body(L"auto recieved message.");
	message.SetBody(body);

	TelPhoneNumber senderNumber("08012345678");
	TelSmsAddress senderAddr(senderNumber, "");
	TelSmsPeerInfo peer(senderAddr, 0, false, 0, 4);
	message.SetSinglePeer(peer);

	if (m_pduGenerator != NULL)
		m_pduGenerator->Generate(message, &pdu);

	m_smsInbox.push_back(pdu);

	TelEventInfo smsEvent(TEL_BBMC_EVENT_SMS_RECEIVED, -1, -1);
	ForwardEvent(smsEvent);
}

void TelBbmcAdapterLoopback::ProcessPeriodicTick()
{
	if (m_seqIndex >= 0 && m_seqCount > 0) {
		if (m_seqCountdown > 0)
			--m_seqCountdown;

		if (m_seqCountdown == 0) {
			do {
				m_core.InjectSequenceEvent(
					g_loopbackSeqTable[m_seqIndex].actionCode,
					g_loopbackSeqTable[m_seqIndex].actionValue);
				int nextIndex = m_seqIndex + 1;
				if (nextIndex < m_seqCount) {
					m_seqIndex = nextIndex;
					int nextDelay = g_loopbackSeqTable[nextIndex].delayTicks;
					if (m_seqScale != 0)
						nextDelay *= m_seqScale;
					m_seqCountdown = nextDelay;
				} else {
					m_seqIndex = -1;
					m_seqCountdown = -1;
				}
			} while (m_seqCountdown == 0);
		}
	}

	if (m_core.GetCurrentState() == 7) {
		TelNetworkInfo info;
		info.SetSignalRssi(2);
		m_signalLevel = 6;
		info.SetSignalLevel(m_signalLevel);
		m_networkType = 2;
		info.SetNetworkType(m_networkType);
		m_roamingState = 0;
		info.SetRoamingState(0);
		info.SetPlmnCodes(1, 1);
		m_registrationState = 1;
		info.SetRegistrationState(m_registrationState);

		TelBbmcNetworkEvent netEvent;
		netEvent.SetNetworkInfo(info);
		ForwardEvent(netEvent);
	}
}

void TelBbmcAdapterLoopback::OnHandleEvent(const TelEventInfo &event)
{
	int eventId = event.GetEventId();
	if (eventId == TEL_BBMC_EVENT_LOOPBACK_AUTO_SMS) {
		GenerateAutoRecvSms();
		return;
	}
	if (eventId == TEL_BBMC_EVENT_LOOPBACK_PERIODIC) {
		ProcessPeriodicTick();
		return;
	}
	if (eventId != TEL_EVENT_THREAD_START && eventId != TEL_EVENT_THREAD_STOP)
		ForwardEvent(event);
}

} /* namespace telephony */
