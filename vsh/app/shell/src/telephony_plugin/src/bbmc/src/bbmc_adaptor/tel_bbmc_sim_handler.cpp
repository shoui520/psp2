/*
 * tel_bbmc_sim_handler.cpp: BBMC SIM/PIN/PUK/CK/file-refresh feature handler
 * for telephony_plugin (3.65-CEX: 0x813D3E40..0x813D5ADB).
 */
#include <std/string.h>
#include <kernel/threadmgr.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

enum TelSimTaskBit {
	SIM_TASK_GET_PIN_STATUS = 0x001,
	SIM_TASK_VERIFY_PIN = 0x002,
	SIM_TASK_UNBLOCK_PIN = 0x004,
	SIM_TASK_GET_CK_STATUS = 0x008,
	SIM_TASK_DEPERSONALIZE_CK = 0x010,
	SIM_TASK_UNBLOCK_CK = 0x020,
	SIM_TASK_SETUP_SET = 0x100,
	SIM_TASK_FILE_REFRESH = 0x200
};

enum TelSimLogTag {
	SIM_LOG_PINST = 0,
	SIM_LOG_GETCK = 1,
	SIM_LOG_VERIFY = 2,
	SIM_LOG_UNBLOCK = 3,
	SIM_LOG_DCK = 4,
	SIM_LOG_UCK = 5,
	SIM_LOG_SETV = 6,
	SIM_LOG_REFRESH = 7,
	SIM_LOG_RES = 8,
	SIM_LOG_PLMN = 9
};

TelBbmcSimHandler::TelBbmcSimHandler(TelBbmcSharedData *sharedData)
	: TelBbmcHandler(sharedData),
	  TelBbmcLogClient(),
	  m_stateMutex("SceTelBbmcSimHandler", 0),
	  m_modemFatal(false),
	  m_pinString(),
	  m_setupKind(2),
	  m_setupBuffer(NULL),
	  m_setupSize(0)
{
	sce_paf_memset(&m_unblockParam, 0, sizeof(m_unblockParam));
	sce_paf_memset(&m_ckStatus, 0, sizeof(m_ckStatus));
	sce_paf_memset(&m_ckParam, 0, sizeof(m_ckParam));
	sce_paf_memset(&m_refreshDetail, 0, sizeof(m_refreshDetail));

	m_sharedData->SetInt(TEL_ATTR_BBMC_LAST_SIM_EVENT, 0);
	Register(TEL_BBMC_LOG_CH_SIM);
}

TelBbmcSimHandler::~TelBbmcSimHandler()
{
	Unregister();
}

bool TelBbmcSimHandler::CanProcessTimerRetry()
{
	TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
	if (sharedStatus != NULL && sharedStatus->GetFlag(TEL_BBMC_FLAG_SUSPENDING))
		return false;

	if (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) >= 2)
		return m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_READY);

	return false;
}

SceUInt32 TelBbmcSimHandler::GetFeatureId()
{
	return TEL_BBMC_FEATURE_SIM;
}

SceUInt32 TelBbmcSimHandler::GetEventMask()
{
	return 0x0FFFFFFFU;
}

SceUInt32 TelBbmcSimHandler::GetSubscriberEvfBit()
{
	return 1U << GetFeatureId();
}

bool TelBbmcSimHandler::ShouldRetryResult(const TelephonyResult &res)
{
	return TelBbmcHandler::ShouldRetryResult(res);
}

const char *TelBbmcSimHandler::GetLogTagName(int tagId) const
{
	switch (tagId) {
	case SIM_LOG_PINST:
		return "PinST";
	case SIM_LOG_GETCK:
		return "GetCK";
	case SIM_LOG_VERIFY:
		return "Verify";
	case SIM_LOG_UNBLOCK:
		return "Unblock";
	case SIM_LOG_DCK:
		return "DCK";
	case SIM_LOG_UCK:
		return "UCK";
	case SIM_LOG_SETV:
		return "SetV";
	case SIM_LOG_REFRESH:
		return "Refresh";
	case SIM_LOG_RES:
		return "Res";
	case SIM_LOG_PLMN:
		return "PLMN";
	default:
		return "";
	}
}

TelephonyResult TelBbmcSimHandler::VerifyPin(const TelPinCode &pin)
{
	TelephonyResult res(TEL_OK);
	if (!pin.IsValid()) {
		res.SetCode(TEL_ERR_INVALID_PIN);
		return res;
	}

	ResetRetryCount(-1);

	for (;;) {
		res = TelBbmcSharedStatus::GetInstance()->CheckReady();
		if (!res.IsSuccess())
			return res;

		res = m_sharedData->CheckSimCondition();
		if (!res.IsSuccess())
			return res;

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		BeginSyncWait(0x051EU);
		m_pinString = pin.GetCode();

		if (PostTask(SIM_TASK_VERIFY_PIN) < 0) {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		} else {
			SceUInt32 lastEvent = 0;
			int status = 0;
			WaitSync(&lastEvent, &status, 30000000);
			if ((lastEvent & 0x0800U) != 0) {
				res.SetCode(TEL_ERR_INVALID_PIN);
				ResetRetryCount(0);
			} else if ((lastEvent & 0x0002U) != 0) {
				res.SetCode(TEL_ERR_NO_SIM);
				ResetRetryCount(0);
			} else if (status < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			}
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();

		for (int waitLoops = 0;
		     (m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE) == 3 ||
		      m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2 ||
		      !m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_READY)) &&
		     waitLoops < 1500;
		     ++waitLoops) {
			if (m_modemFatal) {
				res.SetCode(TEL_ERR_MODEM_FATAL);
				ResetRetryCount(0);
				break;
			}
			sceKernelDelayThread(20000);
		}

		if (!ConsumeRetryAndDelay(res))
			return res;
	}
}

TelephonyResult TelBbmcSimHandler::UnblockPin(const TelPukCode &puk, const TelPinCode &newPin)
{
	TelephonyResult res(TEL_OK);
	if (!puk.IsValid()) {
		res.SetCode(TEL_ERR_INVALID_PUK);
		return res;
	}
	if (!newPin.IsValid()) {
		res.SetCode(TEL_ERR_INVALID_PIN);
		return res;
	}

	ResetRetryCount(-1);

	for (;;) {
		res = TelBbmcSharedStatus::GetInstance()->CheckReady();
		if (!res.IsSuccess())
			return res;

		res = m_sharedData->CheckSimCondition();
		if (!res.IsSuccess())
			return res;

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		BeginSyncWait(0x03A6U);

		sce_paf_memset(&m_unblockParam, 0, sizeof(m_unblockParam));
		const paf::string &pukStr = puk.GetCode();
		m_unblockParam.pukLength = static_cast<SceUInt8>(pukStr.size());
		sce_paf_memcpy(m_unblockParam.puk, pukStr.c_str(), m_unblockParam.pukLength);

		const paf::string &pinStr = newPin.GetCode();
		m_unblockParam.pinLength = static_cast<SceUInt8>(pinStr.size());
		sce_paf_memcpy(m_unblockParam.pin, pinStr.c_str(), m_unblockParam.pinLength);

		if (PostTask(SIM_TASK_UNBLOCK_PIN) < 0) {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		} else {
			SceUInt32 lastEvent = 0;
			int status = 0;
			WaitSync(&lastEvent, &status, 30000000);
			if ((lastEvent & 0x0800U) != 0) {
				res.SetCode(TEL_ERR_INVALID_PUK);
				ResetRetryCount(0);
			} else if ((lastEvent & 0x0002U) != 0) {
				res.SetCode(TEL_ERR_NO_SIM);
				ResetRetryCount(0);
			} else if (status < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			}
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();

		for (int waitLoops = 0;
		     (m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE) == 3 ||
		      m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2 ||
		      !m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_READY)) &&
		     waitLoops < 1500;
		     ++waitLoops) {
			if (m_modemFatal) {
				res.SetCode(TEL_ERR_MODEM_FATAL);
				ResetRetryCount(0);
				break;
			}
			sceKernelDelayThread(20000);
		}

		if (!ConsumeRetryAndDelay(res))
			return res;
	}
}

TelephonyResult TelBbmcSimHandler::GetPinStatus(int *outStatus)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	if (!m_sharedData->GetBool(TEL_ATTR_BBMC_PIN_VALID)) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
		return res;
	}

	int rawStatus = m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_STATUS);
	if (rawStatus >= 0 && rawStatus <= 4)
		*outStatus = rawStatus;
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	return res;
}

TelephonyResult TelBbmcSimHandler::GetPinRetryCount(int *outRetries)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	if (m_sharedData->GetBool(TEL_ATTR_BBMC_PIN_VALID))
		*outRetries = static_cast<SceUInt16>(m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_RETRIES));
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	return res;
}

TelephonyResult TelBbmcSimHandler::SetupSetPinValue(SceUInt32 kind, const void *buffer,
						    SceSize size)
{
	TelephonyResult res(TEL_OK);
	ResetRetryCount(-1);

	do {
		res = TelBbmcSharedStatus::GetInstance()->CheckReady();
		if (!res.IsSuccess())
			break;

		res = m_sharedData->CheckSimCondition();
		if (!res.IsSuccess())
			break;

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		m_setupKind = kind;
		m_setupBuffer = buffer;
		m_setupSize = size;
		BeginSyncWait(0x06000000U);

		if (PostTask(SIM_TASK_SETUP_SET) < 0) {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		} else {
			SceUInt32 lastEvent = 0;
			int status = 0;
			SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
			if ((matched & 0x04000000U) != 0) {
				if ((lastEvent & 0x0800U) != 0) {
					res.SetCode(TEL_ERR_INVALID_PIN);
					ResetRetryCount(0);
				} else {
					res.SetCode(TEL_ERR_BBMC_DRIVER);
				}
			} else if (status < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			} else if (m_syncTimedOut) {
				res.SetCode(TEL_ERR_TIMEOUT);
			}
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

TelephonyResult TelBbmcSimHandler::ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin)
{
	TelephonyResult res(TEL_OK);
	if (!oldPin.IsValid() || !newPin.IsValid()) {
		res.SetCode(TEL_ERR_INVALID_PIN);
		return res;
	}

	SceBbmcPinChange *changeParam = new SceBbmcPinChange;
	if (changeParam == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		return res;
	}

	sce_paf_memset(changeParam, 0, sizeof(*changeParam));
	changeParam->pinId = 1;

	const paf::string &oldStr = oldPin.GetCode();
	changeParam->oldPinLength = static_cast<SceUInt8>((oldStr.size() < 12U) ? oldStr.size() : 12U);
	sce_paf_memcpy(changeParam->oldPin, oldStr.c_str(), changeParam->oldPinLength);

	const paf::string &newStr = newPin.GetCode();
	changeParam->newPinLength = static_cast<SceUInt8>((newStr.size() < 12U) ? newStr.size() : 12U);
	sce_paf_memcpy(changeParam->newPin, newStr.c_str(), changeParam->newPinLength);

	res = SetupSetPinValue(3, changeParam, sizeof(*changeParam));
	delete changeParam;

	for (int waitLoops = 0;
	     (m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE) == 3 ||
	      m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2 ||
	      !m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_READY)) &&
	     waitLoops < 1500 && !m_modemFatal;
	     ++waitLoops) {
		sceKernelDelayThread(20000);
	}

	return res;
}

TelephonyResult TelBbmcSimHandler::SetPinEnabled(const bool *enable, const TelPinCode &pin)
{
	TelephonyResult res(TEL_OK);
	if (!pin.IsValid()) {
		res.SetCode(TEL_ERR_INVALID_PIN);
		return res;
	}

	SceBbmcPinProtection *protParam = new SceBbmcPinProtection;
	if (protParam == NULL) {
		res.SetCode(TEL_ERR_NO_MEMORY);
		return res;
	}

	sce_paf_memset(protParam, 0, sizeof(*protParam));
	protParam->pinId = 1;
	protParam->protectionEnabled = (*enable) ? 1 : 0;

	const paf::string &pinStr = pin.GetCode();
	protParam->pinLength = static_cast<SceUInt8>((pinStr.size() < 12U) ? pinStr.size() : 12U);
	sce_paf_memcpy(protParam->pin, pinStr.c_str(), protParam->pinLength);

	res = SetupSetPinValue(2, protParam, sizeof(*protParam));
	delete protParam;

	for (int waitLoops = 0;
	     (m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE) == 3 ||
	      m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 2 ||
	      !m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_READY)) &&
	     waitLoops < 1500 && !m_modemFatal;
	     ++waitLoops) {
		sceKernelDelayThread(20000);
	}

	return res;
}

TelephonyResult TelBbmcSimHandler::GetPinEnabled(bool *outEnabled)
{
	TelephonyResult res = TelBbmcSharedStatus::GetInstance()->CheckReady();
	if (!res.IsSuccess())
		return res;

	res = m_sharedData->CheckSimCondition();
	if (!res.IsSuccess())
		return res;

	int pinEnabledState = m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE);
	if (pinEnabledState == 1) {
		*outEnabled = true;
		return res;
	}
	if (pinEnabledState == 2) {
		*outEnabled = false;
		return res;
	}
	if (pinEnabledState == 3) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
		return res;
	}

	if (m_sharedData->GetInt(TEL_ATTR_BBMC_PIN_STATUS) == 3)
		res.SetCode(TEL_ERR_PIN_LOCKED);
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	return res;
}

TelephonyResult TelBbmcSimHandler::DepersonalizeControlKey(const paf::string &key)
{
	TelephonyResult res(TEL_OK);
	SceSize keyLen = key.size();
	if (keyLen > 20U) {
		res.SetCode(TEL_ERR_BUFFER_TOO_SMALL);
		return res;
	}

	m_stateMutex.Lock();
	SceUInt8 prevVerifyRetries = m_ckStatus.verifyRetries;
	m_stateMutex.Unlock();

	ResetRetryCount(-1);

	for (;;) {
		res = TelBbmcSharedStatus::GetInstance()->CheckReady();
		if (!res.IsSuccess())
			return res;

		if (m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE) == 0) {
			res.SetCode(TEL_ERR_MODEM_BUSY);
			return res;
		}

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		sce_paf_memset(&m_ckParam, 0, sizeof(m_ckParam));
		m_ckParam.featureId = 0;
		m_ckParam.controlKeyLength = static_cast<SceUInt8>(keyLen);
		sce_paf_strncpy(m_ckParam.controlKey, key.c_str(), sizeof(m_ckParam.controlKey));
		BeginSyncWait(0x01C00000U);

		if (PostTask(SIM_TASK_DEPERSONALIZE_CK) >= 0) {
			SceUInt32 lastEvent = 0;
			int status = 0;
			WaitSync(&lastEvent, &status, 30000000);
			if (status < 0)
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			while (m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE) == 4)
				sceKernelDelayThread(10000);
		} else {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();

		if (!ConsumeRetryAndDelay(res)) {
			m_stateMutex.Lock();
			if (prevVerifyRetries > m_ckStatus.verifyRetries)
				res.SetCode(TEL_ERR_INVALID_CK);
			m_stateMutex.Unlock();
			return res;
		}
	}
}

TelephonyResult TelBbmcSimHandler::UnblockControlKey(const paf::string &key)
{
	TelephonyResult res(TEL_OK);
	SceSize keyLen = key.size();
	if (keyLen > 20U) {
		res.SetCode(TEL_ERR_BUFFER_TOO_SMALL);
		return res;
	}

	m_stateMutex.Lock();
	SceUInt8 prevUnblockRetries = m_ckStatus.unblockRetries;
	m_stateMutex.Unlock();

	ResetRetryCount(-1);

	for (;;) {
		res = TelBbmcSharedStatus::GetInstance()->CheckReady();
		if (!res.IsSuccess())
			return res;

		if (m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE) == 0) {
			res.SetCode(TEL_ERR_MODEM_BUSY);
			return res;
		}

		m_sync.Lock();
		m_sync.IncrementActiveCount();
		sce_paf_memset(&m_ckParam, 0, sizeof(m_ckParam));
		m_ckParam.featureId = 0;
		m_ckParam.controlKeyLength = static_cast<SceUInt8>(keyLen);
		sce_paf_strncpy(m_ckParam.controlKey, key.c_str(), sizeof(m_ckParam.controlKey));
		BeginSyncWait(0x01C00000U);

		if (PostTask(SIM_TASK_UNBLOCK_CK) >= 0) {
			SceUInt32 lastEvent = 0;
			int status = 0;
			WaitSync(&lastEvent, &status, 30000000);
			if (status < 0)
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			while (m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE) == 4)
				sceKernelDelayThread(10000);
		} else {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();

		if (!ConsumeRetryAndDelay(res)) {
			m_stateMutex.Lock();
			if (prevUnblockRetries > m_ckStatus.unblockRetries)
				res.SetCode(TEL_ERR_INVALID_CK);
			m_stateMutex.Unlock();
			return res;
		}
	}
}

TelephonyResult TelBbmcSimHandler::GetControlKeyStatus(SceBbmcControlKeyStatus *outStatus)
{
	TelephonyResult res(TEL_OK);
	int ckState = m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE);
	if (ckState >= 1 && ckState <= 3) {
		m_stateMutex.Lock();
		sce_paf_memcpy(outStatus, &m_ckStatus, sizeof(m_ckStatus));
		m_stateMutex.Unlock();
	} else {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	}
	return res;
}

TelephonyResult TelBbmcSimHandler::QueryPlmnModeBitUnlocked(int *outModeBit)
{
	TelephonyResult res(TEL_OK);
	SceUInt32 rawBit = 0;
	int ret = sceBbmcSimGetPLMNModeBit(m_subscriberId, &rawBit);
	LogResult(SIM_LOG_PLMN, ret);
	if (ret < 0) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	*outModeBit = 2;
	if (rawBit == 0)
		*outModeBit = 0;
	else if (rawBit == 1)
		*outModeBit = 1;
	else
		res.SetCode(TEL_ERR_NOT_AVAILABLE);

	LogU16(SIM_LOG_PLMN, static_cast<SceUInt16>(rawBit));
	return res;
}

TelephonyResult TelBbmcSimHandler::GetPlmnModeBit(int *outModeBit)
{
	m_sync.Lock();
	m_sync.IncrementActiveCount();
	TelephonyResult res = QueryPlmnModeBitUnlocked(outModeBit);
	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

TelephonyResult TelBbmcSimHandler::RequestFileRefresh()
{
	TelephonyResult res(TEL_OK);
	if (PostTask(SIM_TASK_FILE_REFRESH) < 0)
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	return res;
}

void TelBbmcSimHandler::ProcessTasks(SceUInt32 taskBits)
{
	if ((taskBits & SIM_TASK_GET_CK_STATUS) != 0) {
		m_stateMutex.Lock();
		int ret = sceBbmcGetCKStatus(m_subscriberId, 0, &m_ckStatus);
		LogResult(SIM_LOG_GETCK, ret);
		if (ret >= 0) {
			SceUInt32 mappedBlock = 0;
			if (m_ckStatus.blockType == 0)
				mappedBlock = 1;
			else if (m_ckStatus.blockType == 1)
				mappedBlock = 2;

			if (m_ckStatus.state == 0)
				m_sharedData->SetControlKeyAttributes(1, mappedBlock);
			else if (m_ckStatus.state == 1)
				m_sharedData->SetControlKeyAttributes(2, mappedBlock);
			else if (m_ckStatus.state == 2)
				m_sharedData->SetControlKeyAttributes(3, mappedBlock);
			else
				m_sharedData->SetControlKeyAttributes(0, 0);
		} else {
			if (ScheduleTaskRetry(SIM_TASK_GET_CK_STATUS))
				m_sharedData->SetControlKeyAttributes(4, 3);
			else
				m_sharedData->SetControlKeyAttributes(0, 0);
		}
		m_stateMutex.Unlock();

		if (m_sharedData->GetInt(TEL_ATTR_BBMC_CK_STATE) != 0) {
			PushNotify(TEL_BBMC_EVENT_CK_STATUS_UPDATED, 0);
			m_stateMutex.Lock();
			m_stateMutex.Unlock();
		}
	}

	if ((taskBits & SIM_TASK_GET_PIN_STATUS) != 0) {
		SceUInt32 pinStatus = 0;
		SceUInt16 retries = 0;
		int ret = sceBbmcGetPinStatus(m_subscriberId, &pinStatus, &retries);
		LogResult(SIM_LOG_PINST, ret);
		if (ret < 0) {
			if (ScheduleTaskRetry(SIM_TASK_GET_PIN_STATUS))
				m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 3);
			else
				m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 0);
		} else {
			bool wasValid = m_sharedData->GetBool(TEL_ATTR_BBMC_PIN_VALID);
			m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_STATUS, pinStatus);
			m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_RETRIES, retries);
			m_sharedData->SetBool(TEL_ATTR_BBMC_PIN_VALID, true);

			SceUInt32 enabledState = 0;
			if (pinStatus == 0)
				enabledState = 2;
			else if (pinStatus == 1 || pinStatus == 2 || pinStatus == 4)
				enabledState = 1;

			if (!wasValid)
				PushNotify(TEL_BBMC_EVENT_PIN_STATUS_INIT, 0);

			m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, enabledState);
			LogU16(SIM_LOG_PINST, static_cast<SceUInt16>(pinStatus));
		}
	}

	if ((taskBits & SIM_TASK_VERIFY_PIN) != 0) {
		int ret = sceBbmcVerifyPin(m_subscriberId, m_pinString.c_str(), m_pinString.size());
		LogResult(SIM_LOG_VERIFY, ret);
		if (ret < 0)
			SignalSyncError(0x0400U, ret);
	}

	if ((taskBits & SIM_TASK_UNBLOCK_PIN) != 0) {
		int ret = sceBbmcUnblockPin(m_subscriberId, &m_unblockParam);
		LogResult(SIM_LOG_UNBLOCK, ret);
		if (ret < 0)
			SignalSyncError(0x0200U, ret);
	}

	if ((taskBits & SIM_TASK_DEPERSONALIZE_CK) != 0) {
		int ret = sceBbmcDepersonalizeCK(m_subscriberId, &m_ckParam);
		LogResult(SIM_LOG_DCK, ret);
		if (ret < 0)
			SignalSyncError(0x00400000U, ret);
	}

	if ((taskBits & SIM_TASK_UNBLOCK_CK) != 0) {
		int ret = sceBbmcUnblockCK(m_subscriberId, &m_ckParam);
		LogResult(SIM_LOG_UCK, ret);
		if (ret < 0)
			SignalSyncError(0x00400000U, ret);
	}

	if ((taskBits & SIM_TASK_SETUP_SET) != 0) {
		int ret = sceBbmcSetupSetValue(m_subscriberId, m_setupKind,
					       m_setupBuffer, m_setupSize);
		LogResult(SIM_LOG_SETV, ret);
		if (ret < 0)
			SignalSyncError(0x04000000U, ret);
	}

	if ((taskBits & SIM_TASK_FILE_REFRESH) != 0) {
		int ret = sceBbmcGetFileRefreshDetail(m_subscriberId, &m_refreshDetail);
		LogResult(SIM_LOG_RES, ret);
		if (ret < 0) {
			ScheduleTaskRetry(SIM_TASK_FILE_REFRESH);
			return;
		}

		bool needDevInfo = false;
		bool needPlmnBit = false;
		for (SceUInt32 index = 0; index < m_refreshDetail.fileCount && !needDevInfo; ++index) {
			switch (m_refreshDetail.files[index].fileKind) {
			case 2:
			case 0x0D:
			case 0x0E:
			case 0x0F:
			case 0x10:
				needDevInfo = true;
				break;
			case 0x11:
				needPlmnBit = true;
				break;
			default:
				break;
			}
		}

		SceUInt32 responseCode = 0;
		if (m_refreshDetail.stage == 0)
			responseCode = 1;
		else if (m_refreshDetail.stage == 1)
			responseCode = 2;
		else if (m_refreshDetail.stage == 3)
			responseCode = 3;

		if (responseCode != 0) {
			if (sceBbmcFileRefreshResponse(m_subscriberId, responseCode) < 0)
				ScheduleTaskRetry(SIM_TASK_FILE_REFRESH);
		}

		if (needDevInfo) {
			m_sharedData->SetString(TEL_ATTR_BBMC_MSISDN, paf::string(""));
			if (m_sharedData->GetInt(TEL_ATTR_BBMC_DRIVER_STATE) >= 5)
				PushNotify(TEL_BBMC_EVENT_DEVINFO_NEEDED, 0);
		}

		if (needPlmnBit) {
			int modeBit = 0;
			if (QueryPlmnModeBitUnlocked(&modeBit).IsSuccess())
				PushNotify(TEL_BBMC_EVENT_PLMN_MODE_BIT, modeBit);
		}
	}
}

void TelBbmcSimHandler::OnBroadcastFlags(SceUInt32 flags)
{
	if ((flags & 2U) != 0)
		m_modemFatal = false;

	if ((flags & 1U) != 0) {
		m_modemFatal = true;
		SceUInt32 waitMask = m_sync.GetWaitMask();
		if ((waitMask & 0x0200U) != 0)
			SignalSyncError(0x0200U, TEL_ERR_MODEM_FATAL);
		if ((waitMask & 0x0400U) != 0)
			SignalSyncError(0x0400U, TEL_ERR_MODEM_FATAL);
	}
}

int TelBbmcSimHandler::HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					   SceUInt32 *outTaskBits)
{
	(void)notifyCount;

	SceUInt32 handledBits = 0;
	LogEventMask(eventBits);

	bool hasPinError = (eventBits & 0x00000800U) != 0;
	bool hasSimRemoved = (eventBits & 0x00000002U) != 0;

	if ((eventBits & 0x08000003U) != 0) {
		SceUInt32 simState = static_cast<SceUInt32>(m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_STATE));
		bool simInvalid = false;

		if ((eventBits & 0x08000000U) != 0) {
			handledBits |= 0x08000000U;
			simInvalid = true;
		}

		if ((eventBits & 0x00000001U) != 0) {
			handledBits |= 0x00000001U;
			if (simState != 1 || simInvalid) {
				simState = 1;
				m_sharedData->ResetDeviceInfoAttributes();
				m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
				if (m_sharedData->GetInt(TEL_ATTR_BBMC_LAST_SIM_EVENT) != 1 || simInvalid)
					PushNotify(TEL_BBMC_EVENT_SIM_INSERTED, 0);
			}
			m_sharedData->AddInt(TEL_ATTR_BBMC_SIM_ENABLE_COUNT, 1);
			m_sharedData->SetInt(TEL_ATTR_BBMC_LAST_SIM_EVENT, 1);
		}

		if (hasSimRemoved) {
			handledBits |= 0x00000002U;
			if (simState != 2) {
				simState = 2;
				if (m_sharedData->GetInt(TEL_ATTR_BBMC_LAST_SIM_EVENT) != 2)
					PushNotify(TEL_BBMC_EVENT_SIM_REMOVED, 0);
			}
			simInvalid = false;
			m_sharedData->AddInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT, 1);
			m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 0);
			m_sharedData->ResetDeviceInfoAttributes();
			m_sharedData->ResetPinAttributes();
			m_sharedData->SetInt(TEL_ATTR_BBMC_LAST_SIM_EVENT, 2);
		}

		m_sharedData->SetSimStateAttributes(simState, simInvalid);
	}

	if ((eventBits & 0x00000004U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000004U;
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, true);
		PushNotify(TEL_BBMC_EVENT_PIN_REQUIRED, 0);
	}

	if ((eventBits & 0x00000008U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000008U;
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, true);
		PushNotify(TEL_BBMC_EVENT_PUK_REQUIRED,
			   hasPinError ? TEL_ERR_INVALID_PIN : 0);
	}

	if ((eventBits & 0x00000010U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000010U;
		PushNotify(TEL_BBMC_EVENT_PIN_VERIFIED, 0);
	}

	if ((eventBits & 0x00000020U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000020U;
		PushNotify(TEL_BBMC_EVENT_PIN_UNBLOCKED, 0);
	}

	if ((eventBits & 0x00000040U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000040U;
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, true);
		PushNotify(TEL_BBMC_EVENT_SIM_READY, 0);
	}

	if ((eventBits & 0x00000080U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000080U;
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_READY, true);
		if (m_sharedData->GetInt(TEL_ATTR_BBMC_SIM_STATE) == 0) {
			m_sharedData->SetSimStateAttributes(2, false);
			m_sharedData->AddInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT, 1);
			PushNotify(TEL_BBMC_EVENT_SIM_REMOVED, 0);
		}
		PushNotify(TEL_BBMC_EVENT_SIM_BLOCKED,
			   hasPinError ? TEL_ERR_INVALID_PUK : 0);
	}

	if ((eventBits & 0x00000100U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000100U;
	}
	if ((eventBits & 0x00000200U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000200U;
	}
	if ((eventBits & 0x00000400U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x00000400U;
	}
	if (hasPinError)
		handledBits |= 0x00000800U;
	if ((eventBits & 0x00001000U) != 0)
		handledBits |= 0x00001000U;
	if ((eventBits & 0x00002000U) != 0)
		handledBits |= 0x00002000U;
	if ((eventBits & 0x00004000U) != 0)
		handledBits |= 0x00004000U;
	if ((eventBits & 0x00008000U) != 0) {
		*outTaskBits |= SIM_TASK_FILE_REFRESH;
		handledBits |= 0x00008000U;
		SetTaskRetryBudget(SIM_TASK_FILE_REFRESH, -1);
	}
	if ((eventBits & 0x00010000U) != 0)
		handledBits |= 0x00010000U;
	if ((eventBits & 0x00020000U) != 0)
		handledBits |= 0x00020000U;
	if ((eventBits & 0x00040000U) != 0)
		handledBits |= 0x00040000U;
	if ((eventBits & 0x00080000U) != 0)
		handledBits |= 0x00080000U;
	if ((eventBits & 0x00100000U) != 0)
		handledBits |= 0x00100000U;
	if ((eventBits & 0x00200000U) != 0)
		handledBits |= 0x00200000U;
	if ((eventBits & 0x00400000U) != 0) {
		*outTaskBits |= SIM_TASK_GET_CK_STATUS;
		handledBits |= 0x00400000U;
	}
	if ((eventBits & 0x00800000U) != 0) {
		*outTaskBits |= SIM_TASK_GET_CK_STATUS;
		handledBits |= 0x00800000U;
		m_sharedData->AddInt(TEL_ATTR_BBMC_CK_EVENT_COUNT, 1);
		PushNotify(TEL_BBMC_EVENT_SIM_LOCKED, 0);
	}
	if ((eventBits & 0x01000000U) != 0) {
		*outTaskBits |= SIM_TASK_GET_CK_STATUS;
		handledBits |= 0x01000000U;
		m_sharedData->SetBool(TEL_ATTR_BBMC_CK_PERMANENT_BLOCKED, true);
		m_sharedData->AddInt(TEL_ATTR_BBMC_CK_EVENT_COUNT, 1);
		PushNotify(TEL_BBMC_EVENT_SIM_PERMANENT_LOCKED, 0);
		PushNotify(TEL_BBMC_EVENT_SIM_LOCKED, 0);
	}
	if ((eventBits & 0x02000000U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x02000000U;
	}
	if ((eventBits & 0x04000000U) != 0) {
		*outTaskBits |= SIM_TASK_GET_PIN_STATUS;
		handledBits |= 0x04000000U;
	}

	if ((*outTaskBits & SIM_TASK_GET_CK_STATUS) != 0) {
		m_sharedData->SetControlKeyAttributes(4, 3);
		SetTaskRetryBudget(SIM_TASK_GET_CK_STATUS, -1);
	}
	if ((*outTaskBits & SIM_TASK_GET_PIN_STATUS) != 0) {
		m_sharedData->SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 3);
		SetTaskRetryBudget(SIM_TASK_GET_PIN_STATUS, -1);
	}

	if (hasSimRemoved &&
	    !m_sharedData->GetBool(TEL_ATTR_BBMC_MODEM_BOOTED) &&
	    static_cast<SceUInt32>(m_sharedData->GetInt(TEL_ATTR_BBMC_BOOT_COUNT)) <= 1U) {
		m_sharedData->SetBool(TEL_ATTR_BBMC_MODEM_BOOTED, true);
		m_sharedData->SetBool(TEL_ATTR_BBMC_SIM_BOOT_MISSING, true);
	}

	if (eventBits != handledBits)
		handledBits = eventBits;

	ClearAndSignalEvent(handledBits);
	return 0;
}

} /* namespace telephony */
