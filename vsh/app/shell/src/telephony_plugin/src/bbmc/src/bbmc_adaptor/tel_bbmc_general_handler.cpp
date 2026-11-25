/*
 * tel_bbmc_general_handler.cpp: BBMC general/radio/power/network/scan/setup
 * feature handler for telephony_plugin (3.65-CEX: 0x813CFC1A..0x813D2E0F).
 */
#include <std/string.h>
#include <kernel/threadmgr.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

static const int kSignalLevelTable[11] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

enum TelGeneralTaskBit {
	GEN_TASK_POLL_DRIVER = 0x0001,
	GEN_TASK_POLL_NETWORK = 0x0002,
	GEN_TASK_TURN_ON = 0x0004,
	GEN_TASK_TURN_OFF = 0x0008,
	GEN_TASK_LPM_UPDATE = 0x0010,
	GEN_TASK_LPM_OVERRIDE = 0x0020,
	GEN_TASK_RESERVE_OPMODE = 0x0040,
	GEN_TASK_SETUP_GET = 0x0080,
	GEN_TASK_SETUP_SET = 0x0100,
	GEN_TASK_SCAN_START = 0x0200,
	GEN_TASK_SCAN_ABORT = 0x0400,
	GEN_TASK_REJECT_CAUSE = 0x2000,
	GEN_TASK_CHECK_TEMP = 0x4000,
	GEN_TASK_SCAN_DONE = 0x8000,
	GEN_TASK_POWER_SYNC = 0x10000
};

enum TelGeneralLogTag {
	GEN_LOG_DRV = 0,
	GEN_LOG_ROPM = 1,
	GEN_LOG_RSSI = 2,
	GEN_LOG_ON = 3,
	GEN_LOG_OFF = 4,
	GEN_LOG_LPMON = 5,
	GEN_LOG_LPMOFF = 6,
	GEN_LOG_GETV = 7,
	GEN_LOG_SETV = 8,
	GEN_LOG_SCAN = 9,
	GEN_LOG_ABORT = 10,
	GEN_LOG_DONE = 11,
	GEN_LOG_REJ = 12,
	GEN_LOG_FAIL = 13
};

static int mapSignalLevel(SceInt32 rawLevel)
{
	if (rawLevel >= 0 && rawLevel <= 10)
		return kSignalLevelTable[rawLevel];
	return 0;
}

static int mapRoamingIndicator(SceUInt32 rawIndicator)
{
	if (rawIndicator == 0)
		return 2;
	if (rawIndicator == 1)
		return 1;
	if (rawIndicator == 2)
		return 3;
	return 0;
}

void TelBbmcGeneralHandler::ResetState()
{
	TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
	sharedStatus->SetPowerStatus(TEL_BBMC_POWER_OFF);

	m_sharedData->SetControlKeyAttributes(0, 0);
	m_sharedData->SetSimStateAttributes(0, false);
	m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, false);
	m_sharedData->ResetPinAttributes();
	m_sharedData->ResetDeviceInfoAttributes();
	m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 2);
	m_sharedData->SetInt(TEL_ATTR_BBMC_VOLTAGE_STATE, 1);
	m_sharedData->SetInt(TEL_ATTR_BBMC_LPM_OVERRIDE, 0);

	sharedStatus->SetFlag(TEL_BBMC_FLAG_SUSPENDING, false);

	m_lpmRequested = true;
	m_operationMode = 1;
	m_validCacheMask = 0;

	sce_paf_memset(&m_driverInfo, 0, sizeof(m_driverInfo));
	m_driverInfo.currentState = -1;
	m_driverInfo.targetState = -1;

	sce_paf_memset(&m_curNetworkInfo, 0, sizeof(m_curNetworkInfo));
	sce_paf_memset(&m_rejectCause, 0, sizeof(m_rejectCause));

	m_scanState = 0;
	m_scanResultEvent = 0;
	m_scanDoneRetries = 0;
	m_scanActive = false;
	m_scanResultValid = false;
	sce_paf_memset(&m_scanInfo, 0, sizeof(m_scanInfo));
	m_scanInProgress = false;
	m_scanAbortCount = 0;
}

TelBbmcGeneralHandler::TelBbmcGeneralHandler(TelBbmcSharedData *sharedData)
	: TelBbmcHandler(sharedData),
	  TelBbmcLogClient(),
	  m_stateMutex("SceTelBbmcGeneralHandler", 0),
	  m_operationMode(1),
	  m_lpmRequested(true),
	  m_activeSetupCount(0),
	  m_powerCommand(0),
	  m_powerTransition(0),
	  m_powerError(0),
	  m_fatalNotified(false),
	  m_forceStop(false),
	  m_turnOffPending(false),
	  m_getSetupKind(0),
	  m_getSetupBuffer(NULL),
	  m_getSetupSize(0),
	  m_setSetupKind(0),
	  m_setSetupBuffer(NULL),
	  m_setSetupSize(0),
	  m_validCacheMask(0),
	  m_awaitingOnlineReady(true),
	  m_devInfoRequested(false),
	  m_scanState(0),
	  m_scanResultEvent(0),
	  m_scanDoneRetries(0),
	  m_scanActive(false),
	  m_scanResultValid(false),
	  m_scanInProgress(false),
	  m_scanAbortCount(0),
	  m_debugFlags(0)
{
	sce_paf_memset(&m_scanInfo, 0, sizeof(m_scanInfo));
	sce_paf_memset(&m_regParam, 0, sizeof(m_regParam));
	m_regParam.action = 2;
	m_regParam.radioInterface = 5;
	sce_paf_memset(&m_failureInfo, 0, sizeof(m_failureInfo));

	ResetState();
	Register(TEL_BBMC_LOG_CH_GENERAL);
}

TelBbmcGeneralHandler::~TelBbmcGeneralHandler()
{
	Unregister();
}

bool TelBbmcGeneralHandler::CanProcessTimerRetry()
{
	TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
	return sharedStatus == NULL || !sharedStatus->GetFlag(TEL_BBMC_FLAG_SUSPENDING);
}

SceUInt32 TelBbmcGeneralHandler::GetFeatureId()
{
	return TEL_BBMC_FEATURE_GENERAL;
}

SceUInt32 TelBbmcGeneralHandler::GetEventMask()
{
	return 0x0FFFFFFFU;
}

SceUInt32 TelBbmcGeneralHandler::GetSubscriberEvfBit()
{
	return 1U << GetFeatureId();
}

bool TelBbmcGeneralHandler::ShouldRetryResult(const TelephonyResult &res)
{
	return TelBbmcHandler::ShouldRetryResult(res);
}

const char *TelBbmcGeneralHandler::GetLogTagName(int tagId) const
{
	switch (tagId) {
	case GEN_LOG_DRV:
		return "Drv";
	case GEN_LOG_ROPM:
		return "ROpM";
	case GEN_LOG_RSSI:
		return "RSSI";
	case GEN_LOG_ON:
		return "ON";
	case GEN_LOG_OFF:
		return "OFF";
	case GEN_LOG_LPMON:
		return "LPMON";
	case GEN_LOG_LPMOFF:
		return "LPMOFF";
	case GEN_LOG_GETV:
		return "GetV";
	case GEN_LOG_SETV:
		return "SetV";
	case GEN_LOG_SCAN:
		return "Scan";
	case GEN_LOG_ABORT:
		return "Abort";
	case GEN_LOG_DONE:
		return "Done";
	case GEN_LOG_REJ:
		return "Rej";
	case GEN_LOG_FAIL:
		return "Fail";
	default:
		return "";
	}
}

void TelBbmcGeneralHandler::OnBroadcastFlags(SceUInt32 flags)
{
	(void)flags;
}

TelephonyResult TelBbmcGeneralHandler::TurnOn()
{
	TelephonyResult res(TEL_OK);

	m_stateMutex.Lock();
	m_powerCommand = 2;
	m_stateMutex.Unlock();

	for (;;) {
		if (PostTask(GEN_TASK_POWER_SYNC) < 0) {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
			return res;
		}

		int waitLoops = 0;
		while (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2) {
			m_stateMutex.Lock();
			if (m_powerError != 0) {
				res.SetCode(m_powerError);
				m_stateMutex.Unlock();
				break;
			}
			if (m_forceStop) {
				res.SetCode(TEL_ERR_MODEM_OFF);
				m_stateMutex.Unlock();
				break;
			}
			if (m_powerCommand == 5) {
				res.SetCode(TEL_ERR_MODEM_FATAL);
				m_stateMutex.Unlock();
				break;
			}
			if (m_powerCommand != 2) {
				res.SetCode(TEL_ERR_MODEM_OFF);
				m_stateMutex.Unlock();
				break;
			}
			m_stateMutex.Unlock();

			if (waitLoops >= 1500) {
				res.SetCode(TEL_ERR_TIMEOUT);
				break;
			}
			sceKernelDelayThread(20000);
			++waitLoops;
		}

		if (res.GetCode() != TEL_ERR_MODEM_FATAL ||
		    m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT) != 0 ||
		    static_cast<SceUInt32>(m_sharedData->GetInt(TEL_ATTR_BBMC_FAILURE_COUNT)) >= 3U ||
		    m_fatalNotified) {
			break;
		}

		res.SetCode(TEL_OK);
		m_stateMutex.Lock();
		m_powerCommand = 2;
		m_stateMutex.Unlock();
	}

	if (!res.IsSuccess() &&
	    m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT) == 0 &&
	    !m_fatalNotified) {
		m_fatalNotified = true;
		if (m_sharedData->GetInt(TEL_ATTR_BBMC_FAILURE_COUNT) != 0 &&
		    res.GetCode() != TEL_ERR_MODEM_FATAL) {
			PushNotify(TEL_BBMC_EVENT_MODEM_FAILURE,
				   static_cast<int>(m_failureInfo.errorCode));
			PostTask(GEN_TASK_POLL_DRIVER);
		}
	}

	return res;
}

TelephonyResult TelBbmcGeneralHandler::AbortNetworkScan()
{
	TelephonyResult res(TEL_OK);
	m_stateMutex.Lock();
	if (m_scanInProgress) {
		++m_scanAbortCount;
		if (PostTask(GEN_TASK_SCAN_ABORT) < 0) {
			--m_scanAbortCount;
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}
	} else {
		res.SetCode(TEL_ERR_MODEM_OFF);
	}
	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::TurnOff(bool forceStop)
{
	TelephonyResult res(TEL_OK);

	AbortNetworkScan();

	m_stateMutex.Lock();
	m_powerCommand = 1;
	if (!m_forceStop)
		m_forceStop = forceStop;
	m_turnOffPending = true;
	m_stateMutex.Unlock();

	if (PostTask(GEN_TASK_POWER_SYNC) < 0) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	int waitLoops = 0;
	while (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) > 0) {
		if (waitLoops >= 1500) {
			res.SetCode(TEL_ERR_TIMEOUT);
			return res;
		}
		sceKernelDelayThread(20000);
		++waitLoops;
	}

	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetLowPowerMode(bool enableLpm, bool waitDone)
{
	TelephonyResult res(TEL_OK);

	m_stateMutex.Lock();
	m_lpmRequested = enableLpm;
	m_stateMutex.Unlock();

	if (PostTask(GEN_TASK_LPM_UPDATE) < 0)
		res.SetCode(TEL_ERR_BBMC_DRIVER);

	if (enableLpm && waitDone) {
		for (int waitLoops = 0; ; ++waitLoops) {
			if (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) <= 6)
				break;

			m_stateMutex.Lock();
			bool changed = (m_lpmRequested != enableLpm);
			m_stateMutex.Unlock();
			if (changed)
				break;

			if (waitLoops >= 500) {
				res.SetCode(TEL_ERR_TIMEOUT);
				break;
			}
			sceKernelDelayThread(20000);
		}
	}

	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetSignalLevel(TelAntennaLevelThreshold *outLevel)
{
	m_stateMutex.Lock();

	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess()) {
		outLevel->SetThreshold(0);
	} else {
		int mapped = ((m_validCacheMask & 4U) != 0)
				     ? mapSignalLevel(m_curNetworkInfo.signalLevel)
				     : 0;
		outLevel->SetThreshold(mapped);
	}

	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetNetworkInfo(TelNetworkInfo *outInfo)
{
	m_stateMutex.Lock();

	outInfo->Reset();
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess() || (m_validCacheMask & 4U) == 0) {
		m_stateMutex.Unlock();
		return res;
	}

	if ((m_curNetworkInfo.signal.validMask & 1U) != 0 &&
	    m_curNetworkInfo.signal.strengthCount != 0) {
		int netType = 0;
		int radioTech = 0;
		switch (m_curNetworkInfo.signal.strengths[0].radioInterface) {
		case 1:
			netType = 3;
			break;
		case 2:
			netType = 5;
			break;
		case 3:
			netType = 6;
			break;
		case 4:
			netType = 1;
			radioTech = 1;
			break;
		case 5:
			netType = 2;
			radioTech = 2;
			break;
		case 8:
			netType = 0;
			radioTech = 3;
			break;
		default:
			break;
		}
		outInfo->SetNetworkType(netType);
		outInfo->SetSignalRssi(radioTech);
	}

	int sigLevel = ((m_validCacheMask & 4U) != 0)
			       ? mapSignalLevel(m_curNetworkInfo.signalLevel)
			       : 0;
	outInfo->SetSignalLevel(sigLevel);

	int regState = 0;
	if ((m_validCacheMask & 4U) != 0) {
		if (m_curNetworkInfo.serving.registrationState <= 4U)
			regState = static_cast<int>(m_curNetworkInfo.serving.registrationState);
	}
	outInfo->SetRegistrationState(regState);

	if ((m_curNetworkInfo.serving.validFlags & 1U) != 0) {
		int roaming = mapRoamingIndicator(m_curNetworkInfo.serving.roamingIndicator);
		if (roaming != 0)
			outInfo->SetRoamingState(roaming);
	}

	if ((m_curNetworkInfo.serving.validFlags & 4U) != 0) {
		outInfo->SetPlmnCodes(m_curNetworkInfo.serving.mcc, m_curNetworkInfo.serving.mnc);
		if (m_curNetworkInfo.serving.descriptionLength != 0) {
			m_curNetworkInfo.serving.description[sizeof(m_curNetworkInfo.serving.description) - 1] = '\0';
			paf::string desc(m_curNetworkInfo.serving.description);
			outInfo->SetOperatorName(desc);
		}
	}

	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetRoamingInfo(TelRoamingInfo *outRoaming)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess()) {
		outRoaming->SetState(0);
		return res;
	}

	int state = 0;
	if ((m_validCacheMask & 4U) != 0 && (m_curNetworkInfo.serving.validFlags & 1U) != 0)
		state = mapRoamingIndicator(m_curNetworkInfo.serving.roamingIndicator);
	outRoaming->SetState(state);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetServingPlmn(TelephonyPlmn *outPlmn)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	m_stateMutex.Lock();
	if ((m_validCacheMask & 4U) != 0 && (m_curNetworkInfo.serving.validFlags & 4U) != 0) {
		outPlmn->SetMccMnc(m_curNetworkInfo.serving.mcc, m_curNetworkInfo.serving.mnc);
		if (m_curNetworkInfo.serving.descriptionLength != 0) {
			m_curNetworkInfo.serving.description[sizeof(m_curNetworkInfo.serving.description) - 1] = '\0';
			paf::string desc(m_curNetworkInfo.serving.description);
			outPlmn->SetName(desc);
		}
	} else {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	}
	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetPhoneNumber(TelPhoneNumber *outNumber)
{
	outNumber->SetDigits("");
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	paf::string msisdn = m_sharedData->GetString(TEL_ATTR_BBMC_MSISDN);
	if (!msisdn.empty())
		outNumber->SetDigits(msisdn.c_str());
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::ReserveOperationMode(SceUInt32 opMode)
{
	TelephonyResult res(TEL_OK);
	m_sync.Lock();
	m_operationMode = opMode;
	m_sync.IncrementActiveCount();
	ResetRetryCount(-1);

	do {
		res.SetCode(TEL_OK);
		BeginSyncWait(0x3001U);

		if (PostTask(GEN_TASK_RESERVE_OPMODE) < 0) {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		} else {
			SceUInt32 lastEvent = 0;
			int status = 0;
			SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
			if (status < 0)
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			else if ((matched & 0x2000U) != 0)
				res.SetCode(TEL_ERR_BBMC_REJECTED);
			else if ((matched & 0x0001U) != 0)
				res.SetCode(TEL_ERR_MODEM_FATAL);
		}
	} while (ConsumeRetryAndDelay(res));

	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetupGetValue(SceUInt32 kind, void *outBuf, SceSize size)
{
	TelephonyResult res(TEL_OK);
	int pendingErr = 0;
	bool readyForDone = false;

	ResetRetryCount(-1);

	for (;;) {
		m_sync.Lock();
		m_sync.IncrementActiveCount();
		res.SetCode(TEL_OK);

		if (!readyForDone) {
			bool bypassReady = false;
			if (m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT) != 0 &&
			    kind == 0) {
				if (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2) {
					res.SetCode(TEL_ERR_NOT_AVAILABLE);
					m_sync.DecrementActiveCount();
					m_sync.Unlock();
					return res;
				}
				bypassReady = true;
			}

			if (!bypassReady) {
				res = TelBbmcSharedStatus::GetInstance()->CheckReady();
				if (!res.IsSuccess()) {
					m_sync.DecrementActiveCount();
					m_sync.Unlock();
					return res;
				}
				res = m_sharedData->CheckSimCondition();
				if (!res.IsSuccess()) {
					m_sync.DecrementActiveCount();
					m_sync.Unlock();
					return res;
				}
			}

			m_getSetupKind = kind;
			m_getSetupBuffer = outBuf;
			m_getSetupSize = size;
			BeginSyncWait(0xC1U);

			m_stateMutex.Lock();
			++m_activeSetupCount;
			m_stateMutex.Unlock();

			if (PostTask(GEN_TASK_SETUP_GET) >= 0) {
				pendingErr = 0;
				SceUInt32 lastEvent = 0;
				int status = 0;
				SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);

				m_stateMutex.Lock();
				if (--m_activeSetupCount == 0)
					PostTask(GEN_TASK_LPM_UPDATE);
				m_stateMutex.Unlock();

				readyForDone = true;
				if ((matched & 0x80U) != 0) {
					if (status >= 0) {
						pendingErr = TEL_ERR_BBMC_REJECTED;
					} else {
						pendingErr = TEL_ERR_BBMC_DRIVER;
						readyForDone = false;
					}
				} else if ((matched & 0x01U) != 0) {
					pendingErr = TEL_ERR_MODEM_FATAL;
				} else if (status < 0) {
					pendingErr = TEL_ERR_BBMC_DRIVER;
				} else if (m_syncTimedOut) {
					pendingErr = TEL_ERR_TIMEOUT;
				}
				res.SetCode(pendingErr);
			} else {
				m_stateMutex.Lock();
				if (--m_activeSetupCount == 0)
					PostTask(GEN_TASK_LPM_UPDATE);
				m_stateMutex.Unlock();
				pendingErr = TEL_ERR_BBMC_DRIVER;
			}
		}

		if (readyForDone) {
			if (sceBbmcSetupGetValueDone(m_subscriberId, m_getSetupKind,
						     m_getSetupBuffer, m_getSetupSize) >= 0) {
				if (pendingErr < 0) {
					res.SetCode(pendingErr);
					ResetRetryCount(0);
				}
			} else {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			}
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();

		if (!ConsumeRetryAndDelay(res))
			return res;
	}
}

TelephonyResult TelBbmcGeneralHandler::FetchDeviceInformation()
{
	TelephonyResult res(TEL_OK);

	if (m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT) != 0) {
		if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID))
			return res;
	} else if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID)) {
		paf::string msisdn = m_sharedData->GetString(TEL_ATTR_BBMC_MSISDN);
		if (!msisdn.empty())
			return res;
	}

	SceBbmcDeviceInformation *devInfo = new SceBbmcDeviceInformation;
	if (devInfo == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		return res;
	}

	sce_paf_memset(devInfo, 0, sizeof(*devInfo));
	res = SetupGetValue(0, devInfo, sizeof(*devInfo));
	if (res.IsSuccess()) {
		m_sharedData->SetStringIfEmpty(TEL_ATTR_BBMC_REVISION, paf::string(devInfo->revision));
		m_sharedData->SetString(TEL_ATTR_BBMC_ICCID, paf::string(devInfo->iccid));
		m_sharedData->SetString(TEL_ATTR_BBMC_IMEI, paf::string(devInfo->imei));
		m_sharedData->SetStringIfEmpty(TEL_ATTR_BBMC_MANUFACTURER, paf::string(devInfo->manufacturer));
		m_sharedData->SetStringIfEmpty(TEL_ATTR_BBMC_MODEL, paf::string(devInfo->model));
		m_sharedData->SetString(TEL_ATTR_BBMC_ESN, paf::string(devInfo->esn));
		m_sharedData->SetString(TEL_ATTR_BBMC_MEID, paf::string(devInfo->meid));
		m_sharedData->SetInt(TEL_ATTR_BBMC_PROTOCOL, devInfo->messageProtocol);
		m_sharedData->SetInt(TEL_ATTR_BBMC_PRL_VERSION, devInfo->prlVersion);
		m_sharedData->SetInt(TEL_ATTR_BBMC_SW_VERSION, devInfo->imeiSoftwareVersion);
		m_sharedData->SetInt(TEL_ATTR_BBMC_HOME_MCC, devInfo->homeNetwork.mcc);
		m_sharedData->SetInt(TEL_ATTR_BBMC_HOME_MNC, devInfo->homeNetwork.mnc);

		devInfo->homeNetwork.description[sizeof(devInfo->homeNetwork.description) - 1] = '\0';
		m_sharedData->SetString(TEL_ATTR_BBMC_HOME_DESC,
					paf::string(devInfo->homeNetwork.description));
		m_sharedData->SetString(TEL_ATTR_BBMC_IMSI, paf::string(devInfo->imsi));
		m_sharedData->SetString(TEL_ATTR_BBMC_MSISDN, paf::string(devInfo->msisdn));
		m_sharedData->SetBool(TEL_ATTR_BBMC_DEVINFO_VALID, true);

		m_stateMutex.Lock();
		if (m_awaitingOnlineReady)
			PostTask(GEN_TASK_POLL_DRIVER);
		m_stateMutex.Unlock();
	}

	delete devInfo;
	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetupSetValue(SceUInt32 kind, const void *inBuf, SceSize size)
{
	TelephonyResult res(TEL_OK);
	ResetRetryCount(-1);

	do {
		res.SetCode(TEL_OK);
		if (kind != 6) {
			res = TelBbmcSharedStatus::GetInstance()->CheckReady();
			if (!res.IsSuccess())
				break;
			res = m_sharedData->CheckSimCondition();
			if (!res.IsSuccess())
				break;
		}

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		m_setSetupKind = kind;
		m_setSetupBuffer = inBuf;
		m_setSetupSize = size;
		BeginSyncWait(0x301U);

		m_stateMutex.Lock();
		++m_activeSetupCount;
		m_stateMutex.Unlock();

		if (PostTask(GEN_TASK_SETUP_SET) >= 0) {
			SceUInt32 lastEvent = 0;
			int status = 0;
			SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);

			m_stateMutex.Lock();
			if (--m_activeSetupCount == 0)
				PostTask(GEN_TASK_LPM_UPDATE);
			m_stateMutex.Unlock();

			if ((matched & 0x200U) != 0) {
				res.SetCode(TEL_ERR_BBMC_REJECTED);
			} else if ((matched & 0x001U) != 0) {
				res.SetCode(TEL_ERR_MODEM_FATAL);
			} else if (status < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			} else if (m_syncTimedOut) {
				res.SetCode(TEL_ERR_TIMEOUT);
			}
		} else {
			m_stateMutex.Lock();
			if (--m_activeSetupCount == 0)
				PostTask(GEN_TASK_LPM_UPDATE);
			m_stateMutex.Unlock();
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetProfileSettings(const SceBbmcProfileSettings *profile)
{
	TelephonyResult res(TEL_OK);
	if (profile->validFields != 0)
		res = SetupSetValue(5, profile, sizeof(*profile));
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetProfileSettings(SceBbmcProfileSettings *outProfile)
{
	TelephonyResult res(TEL_OK);
	SceBbmcProfileSettings *temp = new SceBbmcProfileSettings;
	if (temp == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		return res;
	}

	res = SetupGetValue(5, temp, sizeof(*temp));
	if (res.IsSuccess() && outProfile != NULL)
		sce_paf_memcpy(outProfile, temp, sizeof(*temp));

	delete temp;
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetImsi(TelImeiInfo *outImsi)
{
	outImsi->SetImei(paf::string(""));
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	paf::string imsi = m_sharedData->GetString(TEL_ATTR_BBMC_IMSI);
	if (!imsi.empty())
		outImsi->SetImei(imsi);
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetImei(paf::string *outImei)
{
	*outImei = "";
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID))
		*outImei = m_sharedData->GetString(TEL_ATTR_BBMC_IMEI);
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetIccid(paf::string *outIccid)
{
	*outIccid = "";
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID))
		*outIccid = m_sharedData->GetString(TEL_ATTR_BBMC_ICCID);
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetHomePlmn(TelephonyPlmn *outPlmn)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID)) {
		SceUInt16 mcc = 0;
		SceUInt16 mnc = 0;
		m_sharedData->GetHomePlmn(&mcc, &mnc);
		outPlmn->SetMccMnc(mcc, mnc);
		paf::string desc = m_sharedData->GetHomeDescription();
		outPlmn->SetName(desc);
	} else {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	}
	return res;
}

TelephonyResult TelBbmcGeneralHandler::StartNetworkScan()
{
	TelephonyResult res(TEL_OK);
	m_sync.Lock();
	m_sync.IncrementActiveCount();

	res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (res.IsSuccess()) {
		res = m_sharedData->CheckSimCondition();
		if (res.IsSuccess()) {
			m_stateMutex.Lock();
			m_scanInProgress = true;
			m_scanResultValid = false;
			sce_paf_memset(&m_scanInfo, 0, sizeof(m_scanInfo));
			m_scanActive = true;
			m_stateMutex.Unlock();

			if (PostTask(GEN_TASK_SCAN_START) < 0)
				res.SetCode(TEL_ERR_BBMC_DRIVER);
		}
	}

	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetNetworkScanInfo(SceBbmcNetworkScanInfo *outScan)
{
	TelephonyResult res(TEL_OK);
	m_stateMutex.Lock();
	if (m_scanResultValid) {
		if (outScan != NULL)
			sce_paf_memcpy(outScan, &m_scanInfo, sizeof(m_scanInfo));
	} else {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	}
	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetNetworkRegistration(const SceBbmcNetworkRegistration *reg)
{
	if (reg != NULL) {
		m_stateMutex.Lock();
		sce_paf_memcpy(&m_regParam, reg, sizeof(m_regParam));
		m_stateMutex.Unlock();
	}
	return SetupSetValue(6, NULL, 0);
}

void TelBbmcGeneralHandler::PollNetworkInfoLocked()
{
	m_stateMutex.Lock();
	if ((m_validCacheMask & 1U) == 0) {
		m_stateMutex.Unlock();
		return;
	}

	sce_paf_memcpy(&m_prevNetworkInfo, &m_curNetworkInfo, sizeof(m_curNetworkInfo));
	sceBbmcGetNetworkInfo(&m_curNetworkInfo);
	m_validCacheMask |= 4U;
	m_curNetworkInfo.serving.description[sizeof(m_curNetworkInfo.serving.description) - 1] = '\0';

	if (m_driverInfo.currentState != 7) {
		m_curNetworkInfo.signalLevel = -1;
		m_curNetworkInfo.signal.validMask &= ~1U;
		sce_paf_memset(&m_curNetworkInfo.signal.strengthCount, 0,
			       sizeof(m_curNetworkInfo.signal.strengthCount) +
			       sizeof(m_curNetworkInfo.signal.strengths) + 2);
		m_curNetworkInfo.serving.validFlags &= static_cast<SceUInt8>(~0x05U);
	}

	if ((m_curNetworkInfo.signal.validMask & 1U) != 0 &&
	    m_curNetworkInfo.signal.strengthCount != 0) {
		for (int index = 0; index < m_curNetworkInfo.signal.strengthCount; ++index)
			LogU16(GEN_LOG_RSSI,
			       static_cast<SceUInt8>(m_curNetworkInfo.signal.strengths[index].strength));
	}

	int changedCount = (m_curNetworkInfo.signalLevel != m_prevNetworkInfo.signalLevel) ? 1 : 0;

	if ((((m_curNetworkInfo.signal.validMask ^ m_prevNetworkInfo.signal.validMask) & 1U) != 0) ||
	    m_curNetworkInfo.signal.strengthCount != m_prevNetworkInfo.signal.strengthCount ||
	    m_curNetworkInfo.signal.strengths[0].radioInterface !=
		    m_prevNetworkInfo.signal.strengths[0].radioInterface) {
		++changedCount;
	}

	if ((((m_curNetworkInfo.serving.validFlags ^ m_prevNetworkInfo.serving.validFlags) & 1U) != 0) ||
	    m_curNetworkInfo.serving.roamingIndicator != m_prevNetworkInfo.serving.roamingIndicator) {
		++changedCount;
	}

	if ((((m_curNetworkInfo.serving.validFlags ^ m_prevNetworkInfo.serving.validFlags) & 4U) != 0) ||
	    m_curNetworkInfo.serving.mcc != m_prevNetworkInfo.serving.mcc ||
	    m_curNetworkInfo.serving.mnc != m_prevNetworkInfo.serving.mnc ||
	    m_curNetworkInfo.serving.descriptionLength != m_prevNetworkInfo.serving.descriptionLength ||
	    sce_paf_strncmp(m_curNetworkInfo.serving.description,
			    m_prevNetworkInfo.serving.description, 67) != 0) {
		++changedCount;
	}

	if (m_curNetworkInfo.serving.registrationState != m_prevNetworkInfo.serving.registrationState)
		++changedCount;

	if (changedCount != 0)
		PushNotify(TEL_BBMC_EVENT_NETWORK_INFO, 0);

	m_stateMutex.Unlock();
}

TelephonyResult TelBbmcGeneralHandler::RefreshNetworkInfo()
{
	TelephonyResult res(TEL_OK);
	PollNetworkInfoLocked();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::SetRssiThresholds(const SceBbmcRssiThresholdList *thresholds)
{
	return SetupSetValue(7, thresholds, sizeof(*thresholds));
}

TelephonyResult TelBbmcGeneralHandler::GetRawNetworkInfo(SceBbmcNetworkInfo *outRaw)
{
	TelephonyResult res(TEL_OK);
	if ((m_validCacheMask & 4U) == 0) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
		return res;
	}

	m_stateMutex.Lock();
	sce_paf_memcpy(outRaw, &m_curNetworkInfo, sizeof(m_curNetworkInfo));
	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetLastRejectCause(SceBbmcRejectCause *outCause)
{
	TelephonyResult res(TEL_OK);
	m_stateMutex.Lock();
	sce_paf_memcpy(outCause, &m_rejectCause, sizeof(m_rejectCause));
	m_stateMutex.Unlock();
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetLatestArchiveVersion(SceUInt32 *outVersion)
{
	TelephonyResult res(TEL_OK);
	if (m_subscriberId < 0) {
		res.SetCode(SCE_BBMC_ERROR_NOT_SUBSCRIBED);
		return res;
	}
	res.SetCode(sceBbmcDLUpdaterGetLatestArchiveVersion(m_subscriberId, outVersion));
	return res;
}

TelephonyResult TelBbmcGeneralHandler::GetArchiveVersion(SceUInt32 *outVersion)
{
	TelephonyResult res(TEL_OK);
	if (m_subscriberId < 0) {
		res.SetCode(SCE_BBMC_ERROR_NOT_SUBSCRIBED);
		return res;
	}
	res.SetCode(sceBbmcDLUpdaterGetArchiveVersion(m_subscriberId, outVersion));
	return res;
}

TelephonyResult TelBbmcGeneralHandler::CheckComVersion(bool *outMatches)
{
	TelephonyResult res(TEL_OK);
	if (m_subscriberId < 0) {
		res.SetCode(SCE_BBMC_ERROR_NOT_SUBSCRIBED);
		return res;
	}

	SceUInt32 rawMatches = 0;
	int ret = sceBbmcDLUpdaterCheckComVersion(m_subscriberId, &rawMatches);
	if (ret == 0)
		*outMatches = (rawMatches != 0);
	res.SetCode(ret);
	return res;
}

TelephonyResult TelBbmcGeneralHandler::FetchComFirmVersion()
{
	TelephonyResult res(TEL_OK);
	if (m_subscriberId < 0) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
		return res;
	}

	paf::string existing = m_sharedData->GetString(TEL_ATTR_BBMC_REVISION);
	if (existing.empty()) {
		char revBuf[104];
		sce_paf_memset(revBuf, 0, sizeof(revBuf));
		if (sceBbmcGetComFirmVersion(m_subscriberId, revBuf, sizeof(revBuf)) != 0)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		else
			m_sharedData->SetStringIfEmpty(TEL_ATTR_BBMC_REVISION, paf::string(revBuf));
	}
	return res;
}

void TelBbmcGeneralHandler::SetDebugFlags(int flags)
{
	m_debugFlags = flags;
}

int TelBbmcGeneralHandler::CheckTemperatureStatus()
{
	return sceBbmcIsAbleToRunUnderCurrentTemp(m_subscriberId);
}

TelephonyResult TelBbmcGeneralHandler::SyncBarrier()
{
	TelephonyResult res(TEL_OK);
	m_stateMutex.Lock();
	m_stateMutex.Unlock();
	return res;
}

void TelBbmcGeneralHandler::ProcessTasks(SceUInt32 taskBits)
{
	bool isPollTimeout = false;
	if ((taskBits & 0x20000000U) != 0) {
		taskBits &= ~0x20000000U;
		isPollTimeout = true;
	}

	m_stateMutex.Lock();
	int prevState = m_driverInfo.currentState;
	sceBbmcGetDriverInfo(m_subscriberId, &m_driverInfo);
	m_validCacheMask |= 1U;
	m_sharedData->SetInt(TEL_ATTR_BBMC_DRIVER_STATE,
			     static_cast<SceUInt32>(m_driverInfo.currentState));
	m_sharedData->SetInt(TEL_ATTR_BBMC_TARGET_STATE,
			     static_cast<SceUInt32>(m_driverInfo.targetState));

	if (prevState != m_driverInfo.currentState) {
		LogTimestamp();
		LogU16(GEN_LOG_DRV, static_cast<SceUInt16>(m_driverInfo.currentState));
		if (isPollTimeout && taskBits == 0)
			PushNotify(TEL_BBMC_EVENT_DRIVER_ERROR, TEL_ERR_APN_VERIFY);

		m_sharedData->RecordDriverState(static_cast<SceUInt8>(m_driverInfo.currentState));

		switch (m_driverInfo.currentState) {
		case 2:
			FetchComFirmVersion();
			break;
		case 4:
			if (!m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_BOOTED) &&
			    static_cast<SceUInt32>(m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT)) <= 1U) {
				m_sharedData->SetBool(TEL_ATTR_BBMC_SIM_BOOT_MISSING, true);
			}
			break;
		case 7:
			taskBits |= GEN_TASK_POLL_NETWORK;
			if (m_sharedData->IsRadioReady())
				PushNotify(TEL_BBMC_EVENT_ENTER_ONLINE, 0);
			break;
		default:
			break;
		}

		if (prevState == 7 && m_driverInfo.currentState < 7)
			PushNotify(TEL_BBMC_EVENT_LEAVE_ONLINE, 0);

		int cur = m_driverInfo.currentState;
		int target = m_driverInfo.targetState;
		if (cur == 0 && target == 0)
			m_powerTransition = 1;
		else if (cur > target && target == 0)
			m_powerTransition = 4;
		else if (cur < target && cur < 2)
			m_powerTransition = 3;
		else
			m_powerTransition = 2;

		if (cur < 2)
			m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, false);
		else
			TelBbmcSharedStatus::GetInstance()->SetPowerStatus(TEL_BBMC_POWER_READY);

		if (cur >= 3) {
			m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
			m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, true);
		}
		if (cur < 5) {
			m_devInfoRequested = false;
			m_awaitingOnlineReady = true;
		}
	}

	if (m_awaitingOnlineReady && m_driverInfo.currentState >= 6) {
		if (m_sharedData->GetBool(TEL_ATTR_BBMC_DEVINFO_VALID) &&
		    m_sharedData->HasControlKeyStatus()) {
			m_awaitingOnlineReady = false;
			m_devInfoRequested = false;
			if (m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT) == 0)
				PushNotify(TEL_BBMC_EVENT_MODEM_ONLINE_READY, 0);
		} else if (!m_devInfoRequested &&
			   m_sharedData->GetInt(TEL_ATTR_BBMC_TEMP_STATE) == 0 &&
			   m_sharedData->GetInt(TEL_ATTR_BBMC_VOLTAGE_STATE) != 0) {
			if (m_sharedData->CheckSimCondition().IsSuccess()) {
				m_devInfoRequested = true;
				PushNotify(TEL_BBMC_EVENT_DEVINFO_NEEDED, 0);
			}
		}
	}
	m_stateMutex.Unlock();

	if ((taskBits & GEN_TASK_LPM_UPDATE) != 0) {
		taskBits |= GEN_TASK_LPM_OVERRIDE;
		SetTaskRetryBudget(GEN_TASK_LPM_OVERRIDE, -1);
	}

	m_stateMutex.Lock();
	if ((m_driverInfo.currentState == 7 || m_driverInfo.currentState == 6) &&
	    m_activeSetupCount == 0) {
		if (m_driverInfo.currentState == 7) {
			if (m_lpmRequested && m_driverInfo.secondBootReply == 4) {
				int ret = sceBbmcSetLPMMode(m_subscriberId, 1);
				LogResult(GEN_LOG_LPMON, ret);
				if (ret < 0 && ret != SCE_BBMC_ERROR_BUSY)
					PushNotify(TEL_BBMC_EVENT_DRIVER_ERROR, ret);
			}
		} else if (!m_lpmRequested && m_sharedData->IsRadioReady() &&
			   m_driverInfo.secondBootReply == 0) {
			int ret = sceBbmcSetLPMMode(m_subscriberId, 0);
			LogResult(GEN_LOG_LPMOFF, ret);
			if (ret < 0 && ret != SCE_BBMC_ERROR_BUSY)
				PushNotify(TEL_BBMC_EVENT_DRIVER_ERROR, ret);
		}
	}

	if (m_forceStop && m_powerCommand != 1) {
		m_powerCommand = 1;
		if (m_powerTransition == 2 || m_turnOffPending)
			taskBits |= GEN_TASK_TURN_OFF;
	} else if (m_powerCommand == 1) {
		if (m_powerTransition == 2 || m_turnOffPending)
			taskBits |= GEN_TASK_TURN_OFF;
	} else if (m_powerCommand == 2 && m_powerTransition == 1) {
		if (m_powerError != 0 || m_forceStop)
			m_powerCommand = 0;
		else
			taskBits |= GEN_TASK_TURN_ON;
	}
	m_stateMutex.Unlock();

	if ((taskBits & GEN_TASK_LPM_OVERRIDE) != 0 &&
	    m_sharedData->GetInt(TEL_ATTR_BBMC_LPM_OVERRIDE) == 1) {
		m_stateMutex.Lock();
		int ret;
		if (m_lpmRequested) {
			ret = sceBbmcSetLPMMode(m_subscriberId, 1);
			LogResult(GEN_LOG_LPMON, ret);
		} else {
			ret = sceBbmcSetLPMMode(m_subscriberId, 0);
			LogResult(GEN_LOG_LPMOFF, ret);
		}
		if (ret < 0 && ret != SCE_BBMC_ERROR_BUSY)
			ScheduleTaskRetry(GEN_TASK_LPM_OVERRIDE);
		m_stateMutex.Unlock();
	}

	if ((taskBits & GEN_TASK_POLL_NETWORK) != 0)
		PollNetworkInfoLocked();

	if ((taskBits & GEN_TASK_TURN_ON) != 0) {
		LogTimestamp();
		int ret = sceBbmcTurnOn(m_subscriberId);
		LogResult(GEN_LOG_ON, ret);
		if (ret >= 0) {
			m_stateMutex.Lock();
			m_powerTransition = 3;
			TelBbmcSharedStatus::GetInstance()->SetPowerStatus(TEL_BBMC_POWER_STARTING);
			m_stateMutex.Unlock();
		}
	}

	if ((taskBits & GEN_TASK_TURN_OFF) != 0) {
		int ret = 0;
		if (m_powerTransition != 1) {
			LogTimestamp();
			ret = sceBbmcTurnOff(m_subscriberId);
			LogResult(GEN_LOG_OFF, ret);
			if (ret >= 0)
				m_powerTransition = 4;
		}
		m_stateMutex.Lock();
		m_turnOffPending = false;
		if (ret >= 0) {
			m_validCacheMask &= 1U;
			m_lpmRequested = true;
			m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 0);
		}
		m_stateMutex.Unlock();
	}

	if ((taskBits & GEN_TASK_RESERVE_OPMODE) != 0) {
		int ret = sceBbmcReserveOperationMode(m_subscriberId, m_operationMode);
		LogResult(GEN_LOG_ROPM, ret);
		if (ret < 0)
			SignalSyncError(0x2000U, ret);
	}

	if ((taskBits & GEN_TASK_SETUP_GET) != 0) {
		int ret = sceBbmcSetupGetValueStart(m_subscriberId, m_getSetupKind);
		LogResult(GEN_LOG_GETV, ret);
		LogU16(GEN_LOG_GETV, static_cast<SceUInt16>(m_getSetupKind));
		if (ret < 0)
			SignalSyncError(0x0080U, ret);
	}

	if ((taskBits & GEN_TASK_SETUP_SET) != 0) {
		if (m_setSetupKind == 6) {
			if (m_driverInfo.currentState != 7) {
				SignalSyncError(0x0100U, 0);
			} else {
				int ret = sceBbmcSetupSetValue(m_subscriberId, 6,
							       &m_regParam, sizeof(m_regParam));
				LogResult(GEN_LOG_SETV, ret);
				LogU16(GEN_LOG_SETV, 6);
				if (ret < 0)
					SignalSyncError(0x0200U, ret);
			}
		} else {
			int ret = sceBbmcSetupSetValue(m_subscriberId, m_setSetupKind,
						       m_setSetupBuffer, m_setSetupSize);
			LogResult(GEN_LOG_SETV, ret);
			LogU16(GEN_LOG_SETV, static_cast<SceUInt16>(m_setSetupKind));
			if (ret < 0)
				SignalSyncError(0x0200U, ret);
		}
	}

	if ((taskBits & GEN_TASK_SCAN_START) != 0) {
		m_stateMutex.Lock();
		if (m_scanAbortCount <= 0) {
			sceBbmcNetworkScanDone(m_subscriberId, &m_scanInfo);
			int ret = sceBbmcNetworkScanStart(m_subscriberId);
			if (ret >= 0)
				m_scanState = 1;
			LogResult(GEN_LOG_SCAN, ret);
			if (ret < 0) {
				m_scanState = 0;
				m_scanDoneRetries = 0;
				m_scanAbortCount = 0;
				m_scanInProgress = false;
				PushNotify(TEL_BBMC_EVENT_NET_SCAN_FAILED, ret);
			}
		} else {
			taskBits &= ~GEN_TASK_SCAN_ABORT;
			if (m_scanActive) {
				m_scanState = 0;
				m_scanDoneRetries = 0;
				m_scanAbortCount = 0;
				m_scanInProgress = false;
				PushNotify(TEL_BBMC_EVENT_NET_SCAN_FAILED, TEL_ERR_SCAN_ABORTED);
			}
		}
		m_stateMutex.Unlock();
	}

	if ((taskBits & GEN_TASK_SCAN_ABORT) != 0) {
		m_stateMutex.Lock();
		LogTimestamp();
		int ret = 0;
		if (m_scanState == 1)
			ret = sceBbmcNetworkScanAbort(m_subscriberId);
		LogResult(GEN_LOG_ABORT, ret);
		m_stateMutex.Unlock();
	}

	if ((taskBits & GEN_TASK_SCAN_DONE) != 0) {
		m_stateMutex.Lock();
		LogTimestamp();
		int ret = 0;
		if (m_scanState == 2) {
			ret = sceBbmcNetworkScanDone(m_subscriberId, &m_scanInfo);
			if (ret >= 0) {
				if (m_scanResultEvent == 0x8000U) {
					m_scanResultValid = true;
				} else {
					m_scanResultValid = false;
					sce_paf_memset(&m_scanInfo, 0, sizeof(m_scanInfo));
				}
				m_scanState = 0;
			}
		}
		LogResult(GEN_LOG_DONE, ret);

		if (ret != 0) {
			if (m_scanDoneRetries <= 5) {
				++m_scanDoneRetries;
				m_retryTasks |= GEN_TASK_SCAN_DONE;
			} else {
				m_scanState = 0;
				m_scanDoneRetries = 0;
				m_scanAbortCount = 0;
				m_scanInProgress = false;
				PushNotify(TEL_BBMC_EVENT_NET_SCAN_FAILED, ret);
			}
		} else {
			m_scanState = 0;
			m_scanDoneRetries = 0;
			m_scanAbortCount = 0;
			m_scanInProgress = false;
			PushNotify(m_scanResultValid ? TEL_BBMC_EVENT_NET_SCAN_DONE
						     : TEL_BBMC_EVENT_NET_SCAN_FAILED,
				   0);
		}
		m_stateMutex.Unlock();
	}

	if ((taskBits & GEN_TASK_CHECK_TEMP) != 0) {
		int tempRet = sceBbmcIsAbleToRunUnderCurrentTemp(m_subscriberId);
		if (tempRet > 0) {
			m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 0);
			PushNotify(TEL_BBMC_EVENT_TEMP_NORMAL, 0);
		} else {
			m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 1);
			PushNotify(TEL_BBMC_EVENT_TEMP_HIGH, 0);
		}
	}

	if ((taskBits & GEN_TASK_REJECT_CAUSE) != 0) {
		m_stateMutex.Lock();
		LogTimestamp();
		int ret = sceBbmcNetGetRejectCause(m_subscriberId, &m_rejectCause);
		LogResult(GEN_LOG_REJ, ret);
		if (ret >= 0) {
			LogU16(GEN_LOG_REJ, static_cast<SceUInt16>(m_rejectCause.domain));
			LogU16(GEN_LOG_REJ, static_cast<SceUInt16>(m_rejectCause.cause));
		}
		m_stateMutex.Unlock();
		if (ret >= 0)
			PushNotify(TEL_BBMC_EVENT_NET_REJECT, 0);
	}
}

int TelBbmcGeneralHandler::HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					       SceUInt32 *outTaskBits)
{
	(void)notifyCount;

	int powerFailReason = TEL_ERR_POWER_ON_FAILED;
	SceUInt32 handledBits = 0;

	LogEventMask(eventBits);

	if ((eventBits & 0x00000001U) != 0) {
		*outTaskBits |= GEN_TASK_POLL_DRIVER;
		handledBits |= 0x00000001U;
		m_sharedData->AddInt(TEL_ATTR_BBMC_FAILURE_COUNT, 1);
		TelBbmcSharedStatus::GetInstance()->SetPowerStatus(TEL_BBMC_POWER_FAILURE);

		m_stateMutex.Lock();
		m_powerCommand = 5;
		if (sceBbmcGetFailureInfo(&m_failureInfo) == 0) {
			LogTimestamp();
			LogU16(GEN_LOG_FAIL, static_cast<SceUInt16>(m_failureInfo.category));
			LogU16(GEN_LOG_FAIL, static_cast<SceUInt16>(m_failureInfo.stage));
			LogU32(GEN_LOG_FAIL, m_failureInfo.detailCode);
			LogU32(GEN_LOG_FAIL, m_failureInfo.errorCode);
		}

		if (m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT) != 0 ||
		    static_cast<SceUInt32>(m_sharedData->GetInt(TEL_ATTR_BBMC_FAILURE_COUNT)) >= 3U ||
		    m_fatalNotified) {
			PushNotify(TEL_BBMC_EVENT_MODEM_FAILURE,
				   static_cast<int>(m_failureInfo.errorCode));
		}
		m_broadcastFlags |= 1U;
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00004000U) != 0) {
		handledBits |= 0x00004000U;
		m_stateMutex.Lock();
		ResetState();
		m_stateMutex.Unlock();
		PushNotify(TEL_BBMC_EVENT_NETWORK_INFO, 0);
		PushNotify(TEL_BBMC_EVENT_MODEM_RESET, 0);
	}

	if ((eventBits & 0x00000002U) != 0) {
		handledBits |= 0x00000002U;
		*outTaskBits |= GEN_TASK_POLL_DRIVER;
	}

	if ((eventBits & 0x00000004U) != 0) {
		handledBits |= 0x00000004U;
		*outTaskBits |= GEN_TASK_POLL_NETWORK;
	}

	if ((eventBits & 0x00000400U) != 0) {
		powerFailReason = TEL_ERR_LOW_BATTERY;
		handledBits |= 0x00000400U;
	}
	if ((eventBits & 0x00000800U) != 0) {
		powerFailReason = TEL_ERR_HIGH_TEMP;
		handledBits |= 0x00000800U;
	}

	if ((eventBits & 0x00000008U) != 0) {
		m_stateMutex.Lock();
		m_powerError = 0;
		handledBits |= 0x00000008U;
		m_sharedData->AddInt(TEL_ATTR_BBMC_BOOT_COUNT, 1);
		*outTaskBits |= GEN_TASK_CHECK_TEMP;
		PushNotify(TEL_BBMC_EVENT_MODEM_POWER_ON, 0);
		m_broadcastFlags |= 2U;
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00000010U) != 0) {
		m_stateMutex.Lock();
		m_powerError = powerFailReason;
		m_powerCommand = 1;
		handledBits |= 0x00000010U;
		PushNotify(TEL_BBMC_EVENT_MODEM_POWER_ON_FAIL, powerFailReason);
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00000020U) != 0) {
		m_stateMutex.Lock();
		m_powerTransition = 1;
		ResetState();
		m_broadcastFlags |= 4U;
		handledBits |= 0x00000020U;
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00000040U) != 0)
		handledBits |= 0x00000040U;
	if ((eventBits & 0x00000080U) != 0)
		handledBits |= 0x00000080U;
	if ((eventBits & 0x00000100U) != 0)
		handledBits |= 0x00000100U;
	if ((eventBits & 0x00000200U) != 0)
		handledBits |= 0x00000200U;
	if ((eventBits & 0x00001000U) != 0)
		handledBits |= 0x00001000U;
	if ((eventBits & 0x00002000U) != 0)
		handledBits |= 0x00002000U;

	if ((eventBits & 0x00008000U) != 0) {
		m_stateMutex.Lock();
		m_scanState = 2;
		m_scanResultEvent = 0x8000U;
		handledBits |= 0x00008000U;
		if (m_scanActive)
			*outTaskBits |= GEN_TASK_SCAN_DONE;
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00010000U) != 0) {
		m_stateMutex.Lock();
		m_scanState = 2;
		m_scanResultEvent = 0x10000U;
		handledBits |= 0x00010000U;
		if (m_scanActive)
			*outTaskBits |= GEN_TASK_SCAN_DONE;
		m_stateMutex.Unlock();
	}

	if ((eventBits & 0x00020000U) != 0)
		handledBits |= 0x00020000U;
	if ((eventBits & 0x00040000U) != 0)
		handledBits |= 0x00040000U;

	if ((eventBits & 0x00080000U) != 0) {
		handledBits |= 0x00080000U;
		TelBbmcSharedStatus::GetInstance()->SetFlag(TEL_BBMC_FLAG_SUSPENDING, true);
		m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 2);
	}

	if ((eventBits & 0x00100000U) != 0) {
		handledBits |= 0x00100000U;
		TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
		sharedStatus->SetFlag(TEL_BBMC_FLAG_SUSPENDING, false);
		*outTaskBits |= GEN_TASK_CHECK_TEMP;
		if (sharedStatus->LatchSmsDuringSuspend()) {
			sharedStatus->SetFlag(TEL_BBMC_FLAG_SMS_PENDING_SUSPEND, false);
			PushNotify(TEL_BBMC_EVENT_SMS_RECEIVED, 0);
		}
		if (sharedStatus->GetFlag(TEL_BBMC_FLAG_PACKET_ACTIVE)) {
			sharedStatus->SetFlag(TEL_BBMC_FLAG_PACKET_ACTIVE, false);
			PushNotify(TEL_BBMC_EVENT_BEARER_TECH, 0);
		}
	}

	bool hasTempHigh = (eventBits & 0x00200000U) != 0;
	bool hasTempNormal = (eventBits & 0x00400000U) != 0;
	if (hasTempHigh && hasTempNormal) {
		handledBits |= 0x00600000U;
		m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 2);
		*outTaskBits |= GEN_TASK_CHECK_TEMP;
	} else {
		if (hasTempHigh) {
			handledBits |= 0x00200000U;
			m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 1);
			PushNotify(TEL_BBMC_EVENT_TEMP_HIGH, 0);
		}
		if (hasTempNormal) {
			handledBits |= 0x00400000U;
			m_sharedData->SetInt(TEL_ATTR_BBMC_TEMP_STATE, 0);
			PushNotify(TEL_BBMC_EVENT_TEMP_NORMAL, 0);
		}
	}

	if ((eventBits & 0x00800000U) != 0) {
		handledBits |= 0x00800000U;
		m_sharedData->SetInt(TEL_ATTR_BBMC_VOLTAGE_STATE, 0);
		PushNotify(TEL_BBMC_EVENT_VOLTAGE_LOW, 0);
	}

	if ((eventBits & 0x01000000U) != 0) {
		handledBits |= 0x01000000U;
		m_sharedData->SetInt(TEL_ATTR_BBMC_VOLTAGE_STATE, 1);
		PushNotify(TEL_BBMC_EVENT_VOLTAGE_NORMAL, 0);
	}

	if ((eventBits & 0x02000000U) != 0) {
		handledBits |= 0x02000000U;
		*outTaskBits |= GEN_TASK_REJECT_CAUSE;
	}

	if ((eventBits & 0x04000000U) != 0) {
		handledBits |= 0x04000000U;
		m_sharedData->SetInt(TEL_ATTR_BBMC_LPM_OVERRIDE, 1);
		*outTaskBits |= (GEN_TASK_POLL_DRIVER | GEN_TASK_LPM_OVERRIDE);
		SetTaskRetryBudget(GEN_TASK_LPM_OVERRIDE, -1);
	}

	if ((eventBits & 0x08000000U) != 0) {
		handledBits |= 0x08000000U;
		m_sharedData->SetInt(TEL_ATTR_BBMC_LPM_OVERRIDE, 0);
		*outTaskBits |= GEN_TASK_POLL_DRIVER;
	}

	if (eventBits != handledBits)
		handledBits = eventBits;

	ClearAndSignalEvent(handledBits);
	return 0;
}

} /* namespace telephony */
