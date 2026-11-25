/*
 * tel_bbmc_adapter_loopback_core.cpp: simulated modem / SIM / PIN / CK state
 * machine for TelBbmcAdapterLoopback (3.65-CEX: 0x813D8478..0x813D965A).
 */
#include <std/string.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

enum TelLoopbackModemState {
	LOOPBACK_STATE_OFF = 0,
	LOOPBACK_STATE_BOOTING = 1,
	LOOPBACK_STATE_SIM_CHECK = 2,
	LOOPBACK_STATE_CK_CHECK_SIM = 3,
	LOOPBACK_STATE_CK_CHECK_NOSIM = 4,
	LOOPBACK_STATE_READY_INIT = 5,
	LOOPBACK_STATE_LPM = 6,
	LOOPBACK_STATE_ONLINE = 7
};

enum TelLoopbackSequenceAction {
	LOOPBACK_ACTION_MODEM_FAILURE = 1,
	LOOPBACK_ACTION_OVERHEAT = 2,
	LOOPBACK_ACTION_TEMP_RECOVER = 3,
	LOOPBACK_ACTION_SIM_REMOVE = 4,
	LOOPBACK_ACTION_SIM_INSERT = 5,
	LOOPBACK_ACTION_VOLTAGE_LOW = 6,
	LOOPBACK_ACTION_VOLTAGE_NORMAL = 7
};

TelBbmcAdapterLoopbackCore::TelBbmcAdapterLoopbackCore()
	: m_notifyQueue("SceTelLoopbackNotifyQ", 32),
	  m_mutex("SceTelBbmcHandler", 0),
	  m_failBoot(false),
	  m_overheated(false),
	  m_overheatTicks(100),
	  m_currentState(LOOPBACK_STATE_OFF),
	  m_targetState(LOOPBACK_STATE_OFF),
	  m_pinEnabledState(0),
	  m_lpmDesired(0),
	  m_devInfoReady(false),
	  m_awaitingOnlineReady(true),
	  m_devInfoRequested(false)
{
	m_config.simInserted = true;
	m_config.pinStatus = 0;
	m_config.pinCode = "0000";
	m_config.pukCode = "11111111";
	m_config.pinRetries = 3;
	m_config.simLockState = 1;
	m_config.simBlockType = 0;
	m_config.initialCheckEnabled = 0;
	m_config.unlockCode = "00000000";
	m_config.unblockCode = "11111111";
	m_config.ckRetries = 10;

	m_notifyQueue.Activate();
}

TelBbmcAdapterLoopbackCore::~TelBbmcAdapterLoopbackCore()
{
	m_notifyQueue.Deactivate();
}

void TelBbmcAdapterLoopbackCore::ApplyConfig(const TelBbmcLoopbackConfig &config)
{
	m_config.simInserted = config.simInserted;
	m_config.pinStatus = config.pinStatus;
	m_config.pinCode = config.pinCode;
	m_config.pukCode = config.pukCode;
	m_config.pinRetries = config.pinRetries;
	m_config.simLockState = config.simLockState;
	m_config.simBlockType = config.simBlockType;
	m_config.initialCheckEnabled = config.initialCheckEnabled;
	m_config.unlockCode = config.unlockCode;
	m_config.unblockCode = config.unblockCode;
	m_config.ckRetries = config.ckRetries;
}

TelephonyResult TelBbmcAdapterLoopbackCore::PushEvent(int eventId, int eventArg)
{
	TelBbmcNotifyItem item;
	item.eventId = eventId;
	item.eventArg = eventArg;
	return m_notifyQueue.Push(item);
}

void TelBbmcAdapterLoopbackCore::Step()
{
	m_mutex.Lock();

	int curState = m_currentState;
	if (curState != LOOPBACK_STATE_ONLINE && m_overheated && m_overheatTicks > 0) {
		--m_overheatTicks;
		if (m_overheatTicks == 0) {
			m_overheated = false;
			PushEvent(TEL_BBMC_EVENT_TEMP_NORMAL, 0);
			curState = m_currentState;
		}
	}

	int nextState = m_targetState;
	if (curState != nextState) {
		int prevState = curState;
		m_currentState = nextState;

		switch (nextState) {
		case LOOPBACK_STATE_OFF:
			PushEvent(TEL_BBMC_EVENT_MODEM_FAILURE, 0);
			m_pinEnabledState = 0;
			m_devInfoReady = false;
			break;
		case LOOPBACK_STATE_BOOTING:
			PushEvent(TEL_BBMC_EVENT_MODEM_POWER_ON, 0);
			m_targetState = LOOPBACK_STATE_SIM_CHECK;
			break;
		case LOOPBACK_STATE_SIM_CHECK:
			PushEvent(m_overheated ? TEL_BBMC_EVENT_TEMP_HIGH
					       : TEL_BBMC_EVENT_TEMP_NORMAL,
				  0);
			if (m_config.simInserted) {
				PushEvent(TEL_BBMC_EVENT_SIM_INSERTED, 0);
				m_pinEnabledState = (m_config.pinStatus == 0) ? 2 : 1;
				switch (m_config.pinStatus) {
				case 0:
				case 4:
					PushEvent(TEL_BBMC_EVENT_SIM_READY, 0);
					m_targetState = LOOPBACK_STATE_CK_CHECK_SIM;
					break;
				case 1:
					PushEvent(TEL_BBMC_EVENT_PUK_REQUIRED, 0);
					break;
				case 2:
					PushEvent(TEL_BBMC_EVENT_PIN_REQUIRED, 0);
					break;
				case 3:
					PushEvent(TEL_BBMC_EVENT_SIM_BLOCKED, 0);
					break;
				default:
					break;
				}
				if (prevState == LOOPBACK_STATE_BOOTING)
					PushEvent(TEL_BBMC_EVENT_PIN_STATUS_INIT, 0);
			} else {
				PushEvent(TEL_BBMC_EVENT_SIM_REMOVED, 0);
				m_targetState = LOOPBACK_STATE_CK_CHECK_NOSIM;
			}
			break;
		case LOOPBACK_STATE_CK_CHECK_SIM:
		case LOOPBACK_STATE_CK_CHECK_NOSIM:
			if (m_config.simLockState == 3) {
				PushEvent(TEL_BBMC_EVENT_SIM_PERMANENT_LOCKED, 0);
			} else if (m_config.simBlockType != 0) {
				PushEvent(TEL_BBMC_EVENT_SIM_LOCKED, 0);
			} else if (nextState == LOOPBACK_STATE_CK_CHECK_SIM) {
				m_targetState = LOOPBACK_STATE_READY_INIT;
			}
			PushEvent(TEL_BBMC_EVENT_CK_STATUS_UPDATED, 0);
			break;
		case LOOPBACK_STATE_READY_INIT:
			m_targetState = LOOPBACK_STATE_LPM;
			break;
		case LOOPBACK_STATE_ONLINE:
			PushEvent(TEL_BBMC_EVENT_ENTER_ONLINE, 0);
			break;
		default:
			break;
		}

		curState = m_currentState;
		if (prevState == LOOPBACK_STATE_ONLINE && curState < LOOPBACK_STATE_ONLINE) {
			PushEvent(TEL_BBMC_EVENT_LEAVE_ONLINE, 0);
			curState = m_currentState;
		}

		if (curState < LOOPBACK_STATE_READY_INIT) {
			m_awaitingOnlineReady = true;
			m_devInfoRequested = false;
		}
	}

	if (m_awaitingOnlineReady && curState >= LOOPBACK_STATE_READY_INIT) {
		if (m_devInfoReady) {
			m_devInfoRequested = false;
			m_awaitingOnlineReady = false;
			PushEvent(TEL_BBMC_EVENT_MODEM_ONLINE_READY, 0);
			curState = m_currentState;
		} else if (!m_devInfoRequested) {
			m_devInfoRequested = true;
			PushEvent(TEL_BBMC_EVENT_DEVINFO_NEEDED, 0);
			curState = m_currentState;
		}
	}

	if (curState == LOOPBACK_STATE_ONLINE) {
		if (m_lpmDesired != 2)
			m_targetState = LOOPBACK_STATE_LPM;
	} else if (curState == LOOPBACK_STATE_LPM && m_lpmDesired == 2 && m_devInfoReady) {
		m_targetState = LOOPBACK_STATE_ONLINE;
	}

	m_mutex.Unlock();
}

void TelBbmcAdapterLoopbackCore::InjectSequenceEvent(int actionCode, int actionValue)
{
	m_mutex.Lock();

	switch (actionCode) {
	case LOOPBACK_ACTION_MODEM_FAILURE:
		m_targetState = LOOPBACK_STATE_OFF;
		break;
	case LOOPBACK_ACTION_OVERHEAT:
		m_overheatTicks = actionValue;
		if (!m_overheated) {
			m_overheated = true;
			PushEvent(TEL_BBMC_EVENT_TEMP_HIGH, 0);
		}
		break;
	case LOOPBACK_ACTION_TEMP_RECOVER:
		if (m_overheated)
			m_overheatTicks = 1;
		break;
	case LOOPBACK_ACTION_SIM_REMOVE:
		if (m_config.simInserted) {
			m_config.simInserted = false;
			m_targetState = LOOPBACK_STATE_SIM_CHECK;
		}
		break;
	case LOOPBACK_ACTION_SIM_INSERT:
		if (!m_config.simInserted) {
			m_config.simInserted = true;
			m_targetState = LOOPBACK_STATE_SIM_CHECK;
		}
		break;
	case LOOPBACK_ACTION_VOLTAGE_LOW:
		PushEvent(TEL_BBMC_EVENT_VOLTAGE_LOW, 0);
		break;
	case LOOPBACK_ACTION_VOLTAGE_NORMAL:
		PushEvent(TEL_BBMC_EVENT_VOLTAGE_NORMAL, 0);
		break;
	default:
		break;
	}

	m_mutex.Unlock();
}

TelephonyResult TelBbmcAdapterLoopbackCore::PopEvent(TelBbmcNotifyItem *outItem)
{
	TelephonyResult res(TEL_OK);
	if (!m_notifyQueue.Pop(outItem))
		res.SetCode(TEL_ERR_NOT_FOUND);
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::TurnOn()
{
	TelephonyResult res(TEL_OK);
	bool shouldBoot = !m_failBoot;
	m_mutex.Lock();
	m_targetState = shouldBoot ? LOOPBACK_STATE_BOOTING : LOOPBACK_STATE_OFF;
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::TurnOff()
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	m_failBoot = false;
	m_currentState = LOOPBACK_STATE_OFF;
	m_targetState = LOOPBACK_STATE_OFF;
	PushEvent(TEL_BBMC_EVENT_MODEM_POWER_OFF, 0);
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::SetLowPowerMode(bool enableLpm)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	if (enableLpm) {
		m_lpmDesired = 1;
		if (m_targetState == LOOPBACK_STATE_ONLINE)
			m_targetState = LOOPBACK_STATE_LPM;
	} else {
		m_lpmDesired = 2;
		if (m_targetState == LOOPBACK_STATE_LPM)
			m_targetState = LOOPBACK_STATE_ONLINE;
	}
	m_mutex.Unlock();
	return res;
}

int TelBbmcAdapterLoopbackCore::GetCurrentState()
{
	m_mutex.Lock();
	int state = m_currentState;
	m_mutex.Unlock();
	return state;
}

TelephonyResult TelBbmcAdapterLoopbackCore::VerifyPin(const TelPinCode &pin, bool silent)
{
	TelephonyResult res(TEL_OK);
	if (GetCurrentState() < LOOPBACK_STATE_SIM_CHECK) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	m_mutex.Lock();
	if (m_config.pinStatus == 1 || m_config.pinStatus == 3) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		m_mutex.Unlock();
		return res;
	}

	const char *inputPin = pin.GetCode().c_str();
	const char *expectedPin = m_config.pinCode.c_str();
	if (*expectedPin == *inputPin && sce_paf_strcmp(expectedPin, inputPin) == 0) {
		m_config.pinStatus = 4;
		m_config.pinRetries = 3;
		if (m_currentState == LOOPBACK_STATE_SIM_CHECK)
			m_targetState = LOOPBACK_STATE_CK_CHECK_SIM;
		if (!silent)
			PushEvent(TEL_BBMC_EVENT_PIN_VERIFY_OK, 0);
	} else {
		--m_config.pinRetries;
		if (m_config.pinRetries == 0) {
			m_config.pinStatus = 1;
			m_config.pinRetries = 10;
			m_targetState = LOOPBACK_STATE_SIM_CHECK;
		}
		PushEvent(TEL_BBMC_EVENT_PIN_VERIFY_FAIL, TEL_ERR_INVALID_PIN);
		res.SetCode(TEL_ERR_INVALID_PIN);
	}

	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::UnblockPin(const TelPukCode &puk,
						       const TelPinCode &newPin)
{
	TelephonyResult res(TEL_OK);
	if (GetCurrentState() < LOOPBACK_STATE_SIM_CHECK) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	m_mutex.Lock();
	if (m_config.pinStatus != 1) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		m_mutex.Unlock();
		return res;
	}

	const char *inputPuk = puk.GetCode().c_str();
	const char *expectedPuk = m_config.pukCode.c_str();
	if (*expectedPuk != *inputPuk || sce_paf_strcmp(expectedPuk, inputPuk) != 0) {
		--m_config.pinRetries;
		if (m_config.pinRetries == 0) {
			m_config.pinStatus = 3;
			PushEvent(TEL_BBMC_EVENT_SIM_BLOCKED, 0);
		}
		PushEvent(TEL_BBMC_EVENT_PUK_UNBLOCK_FAIL, TEL_ERR_INVALID_PUK);
		res.SetCode(TEL_ERR_INVALID_PUK);
	} else {
		m_config.pinStatus = 2;
		m_config.pinRetries = 3;
		m_config.pinCode = newPin.GetCode();
		PushEvent(TEL_BBMC_EVENT_PIN_REQUIRED, 0);
	}

	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetPinStatus(int *outStatus)
{
	TelephonyResult res(TEL_OK);
	if (GetCurrentState() < LOOPBACK_STATE_SIM_CHECK) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
		return res;
	}

	m_mutex.Lock();
	*outStatus = m_config.pinStatus;
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::ChangePin(const TelPinCode &oldPin,
						      const TelPinCode &newPin)
{
	TelephonyResult res = VerifyPin(oldPin, true);
	if (res.IsSuccess()) {
		m_mutex.Lock();
		m_config.pinCode = newPin.GetCode();
		m_config.pinRetries = 3;
		m_mutex.Unlock();
	}
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetPinEnabled(bool *outEnabled)
{
	int status = 0;
	TelephonyResult res = GetPinStatus(&status);
	if (!res.IsSuccess())
		return res;

	if (status == 3) {
		*outEnabled = true;
		res.SetCode(TEL_ERR_PIN_LOCKED);
		return res;
	}

	m_mutex.Lock();
	if (m_pinEnabledState == 1) {
		*outEnabled = true;
	} else if (m_pinEnabledState == 2) {
		*outEnabled = false;
	} else {
		*outEnabled = true;
		res.SetCode(TEL_ERR_PIN_LOCKED);
	}
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::SetPinEnabled(const bool *enable,
							  const TelPinCode &pin)
{
	TelephonyResult res = VerifyPin(pin, true);
	if (res.IsSuccess()) {
		bool currentlyEnabled = false;
		res = GetPinEnabled(&currentlyEnabled);
		if (res.IsSuccess() && currentlyEnabled != *enable) {
			m_mutex.Lock();
			if (*enable) {
				m_pinEnabledState = 1;
			} else {
				m_config.pinStatus = 0;
				m_pinEnabledState = 2;
			}
			m_mutex.Unlock();
		}
	}
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetPinRetryCount(SceUInt16 *outRetries)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	*outRetries = static_cast<SceUInt16>(m_config.pinRetries);
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::CheckSimCondition()
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	if (m_currentState <= LOOPBACK_STATE_OFF) {
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	} else if (!m_config.simInserted) {
		res.SetCode(TEL_ERR_NO_SIM);
	}
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetControlKeyState(int *outState)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	*outState = m_config.simLockState;
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetControlKeyBlockType(int *outBlockType)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	if (m_config.pinStatus != 4 && m_config.pinStatus != 0)
		res.SetCode(TEL_ERR_NOT_AVAILABLE);
	else
		*outBlockType = m_config.simBlockType;
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::GetControlKeyRetries(int *outRetries)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	*outRetries = m_config.ckRetries;
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::DepersonalizeControlKey(const TelControlKey &key)
{
	TelephonyResult res(TEL_OK);
	if (GetCurrentState() < LOOPBACK_STATE_CK_CHECK_SIM) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	m_mutex.Lock();
	if (m_config.simLockState == 0 || m_config.simLockState == 2 ||
	    m_config.simLockState == 3) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		m_mutex.Unlock();
		return res;
	}

	const char *inputKey = key.GetKey().c_str();
	const char *expectedKey = m_config.unlockCode.c_str();
	if (*expectedKey == *inputKey && sce_paf_strcmp(expectedKey, inputKey) == 0) {
		m_config.simLockState = 0;
		m_config.simBlockType = 0;
		m_config.ckRetries = 10;
		if (m_currentState == LOOPBACK_STATE_CK_CHECK_SIM)
			m_targetState = LOOPBACK_STATE_READY_INIT;
	} else {
		--m_config.ckRetries;
		if (m_config.ckRetries == 0) {
			m_config.simLockState = 2;
			m_config.ckRetries = 32;
			if (m_config.simBlockType == 0) {
				m_config.simBlockType = 1;
				PushEvent(TEL_BBMC_EVENT_SIM_LOCKED, 0);
			}
			m_targetState = m_config.simInserted ? LOOPBACK_STATE_CK_CHECK_SIM
							     : LOOPBACK_STATE_CK_CHECK_NOSIM;
		}
		res.SetCode(TEL_ERR_INVALID_CK);
	}

	PushEvent(TEL_BBMC_EVENT_CK_STATUS_UPDATED, 0);
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcAdapterLoopbackCore::UnblockControlKey(const TelControlKey &key)
{
	TelephonyResult res(TEL_OK);
	if (GetCurrentState() < LOOPBACK_STATE_CK_CHECK_SIM) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	m_mutex.Lock();
	if (m_config.simLockState == 0 || m_config.simLockState == 1 ||
	    m_config.simLockState == 3) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		m_mutex.Unlock();
		return res;
	}

	const char *inputKey = key.GetKey().c_str();
	const char *expectedKey = m_config.unblockCode.c_str();
	if (*expectedKey == *inputKey && sce_paf_strcmp(expectedKey, inputKey) == 0) {
		m_config.simLockState = 0;
		m_config.simBlockType = 0;
		m_config.ckRetries = 10;
		if (m_currentState == LOOPBACK_STATE_CK_CHECK_SIM)
			m_targetState = LOOPBACK_STATE_READY_INIT;
	} else {
		--m_config.ckRetries;
		if (m_config.ckRetries == 0) {
			m_config.simLockState = 3;
			m_targetState = m_config.simInserted ? LOOPBACK_STATE_CK_CHECK_SIM
							     : LOOPBACK_STATE_CK_CHECK_NOSIM;
			PushEvent(TEL_BBMC_EVENT_SIM_PERMANENT_LOCKED, 0);
		}
		res.SetCode(TEL_ERR_INVALID_CK);
	}

	PushEvent(TEL_BBMC_EVENT_CK_STATUS_UPDATED, 0);
	m_mutex.Unlock();
	return res;
}

void TelBbmcAdapterLoopbackCore::MarkDeviceInfoReady()
{
	m_mutex.Lock();
	m_devInfoReady = true;
	m_mutex.Unlock();
}

} /* namespace telephony */
