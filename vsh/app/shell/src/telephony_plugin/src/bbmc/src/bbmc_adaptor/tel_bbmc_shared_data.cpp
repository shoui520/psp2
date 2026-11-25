/*
 * tel_bbmc_shared_data.cpp: TelBbmcStateHistory and TelBbmcSharedData
 * (3.65-CEX: 0x813D982C..0x813DAC1D).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

namespace {

static TelBbmcSharedData *g_sharedData = NULL;

} /* namespace */

/* TelBbmcStateHistory (0x813D982C..0x813D998D) */
TelBbmcStateHistory::TelBbmcStateHistory()
	: m_maxEntries(16)
{
}

TelBbmcStateHistory::~TelBbmcStateHistory()
{
}

void TelBbmcStateHistory::PushState(SceUInt8 state)
{
	while (m_states.size() >= m_maxEntries)
		m_states.pop_front();
	m_states.push_back(state);
}

/* 0x813D98AA */
paf::string TelBbmcStateHistory::FormatHex() const
{
	paf::string result;
	paf::list<SceUInt8> &mutableStates = const_cast<paf::list<SceUInt8> &>(m_states);
	for (paf::list<SceUInt8>::iterator it = mutableStates.begin();
	     it != mutableStates.end(); ++it) {
		result.append(telFormatString("%x", (unsigned int)(*it & 0x0F)));
	}
	return result;
}

/* TelBbmcSharedData (0x813D998E..0x813DAC1D) */
TelBbmcSharedData::TelBbmcSharedData()
	: m_mutex("SceTelBbmcSharedData", 0),
	  m_evfBits(0),
	  m_sysInfoEvf(-1)
{
	m_sysInfoEvf = sceKernelCreateEventFlag("SceTelBbmcSysInfoEvf", 0x1080, 0, NULL);

	SetInt(TEL_ATTR_BBMC_DRIVER_STATE, (SceUInt32)-1);
	SetInt(TEL_ATTR_BBMC_PIN_STATUS, 2);
	SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 0);
	SetInt(TEL_ATTR_BBMC_SIM_STATE, 0);
	SetInt(TEL_ATTR_BBMC_CK_STATE, 0);
	SetInt(TEL_ATTR_BBMC_CK_BLOCK_TYPE, 0);
	SetInt(TEL_ATTR_BBMC_TEMP_STATE, 2);
	SetInt(TEL_ATTR_BBMC_VOLTAGE_STATE, 1);
	SetInt(TEL_ATTR_BBMC_LPM_OVERRIDE, 0);
}

/* 0x813D9EBC */
TelBbmcSharedData::~TelBbmcSharedData()
{
	if (m_sysInfoEvf >= 0) {
		sceKernelDeleteEventFlag(m_sysInfoEvf);
		m_sysInfoEvf = -1;
	}
}

/* 0x813D9D64 */
TelephonyResult TelBbmcSharedData::Initialize()
{
	if (g_sharedData != NULL)
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	g_sharedData = new TelBbmcSharedData();
	if (g_sharedData == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	return TelBbmcLogManager::Initialize();
}

/* 0x813D9DF4 */
TelephonyResult TelBbmcSharedData::Terminate()
{
	TelBbmcLogManager::Terminate();
	if (g_sharedData != NULL) {
		delete g_sharedData;
		g_sharedData = NULL;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813D9E50 */
TelBbmcSharedData *TelBbmcSharedData::GetInstance()
{
	return g_sharedData;
}

/* 0x813D9E5C */
void TelBbmcSharedData::SetStaticInt(int attrId, SceUInt32 value)
{
	if (g_sharedData != NULL)
		g_sharedData->SetInt(attrId, value);
}

/* 0x813D998E */
void TelBbmcSharedData::UpdateSysInfoEventFlag(int attrId, SceUInt32 value)
{
	if (value == (SceUInt32)-1)
		value = 0;

	SceUInt32 width = 0;
	SceUInt32 shift = 0;
	SceUInt32 encoded = value;

	switch (attrId) {
	case TEL_ATTR_BBMC_DRIVER_STATE:
		width = 3;
		shift = 4;
		break;
	case TEL_ATTR_BBMC_TARGET_STATE:
		width = 3;
		shift = 7;
		break;
	case TEL_ATTR_BBMC_SIM_STATE:
		width = 1;
		shift = 10;
		encoded = (value == 1) ? 1U : 0U;
		break;
	case TEL_ATTR_BBMC_CK_STATE:
		width = 2;
		shift = 14;
		if (value == 0 || value == 4)
			encoded = 0;
		else if (value == 1)
			encoded = 1;
		else
			encoded = 3;
		break;
	case TEL_ATTR_BBMC_CK_BLOCK_TYPE:
		width = 1;
		shift = 16;
		encoded = (value == 2) ? 1U : 0U;
		break;
	case TEL_ATTR_BBMC_TEMP_STATE:
		width = 1;
		shift = 17;
		encoded = (value == 1) ? 1U : 0U;
		break;
	case TEL_ATTR_BBMC_VOLTAGE_STATE:
		width = 1;
		shift = 18;
		encoded = (value == 0) ? 1U : 0U;
		break;
	case TEL_ATTR_BBMC_SIM_DISABLE_COUNT:
		width = 3;
		shift = 11;
		break;
	case TEL_ATTR_BBMC_BOOT_COUNT:
		width = 2;
		shift = 0;
		break;
	case TEL_ATTR_BBMC_FAILURE_COUNT:
		width = 2;
		shift = 2;
		break;
	case TEL_ATTR_BBMC_SYSINFO_EXTRA:
		width = 7;
		shift = 19;
		break;
	case TEL_ATTR_BBMC_SYSINFO_GROUP_A:
		width = 2;
		shift = 26;
		break;
	case TEL_ATTR_BBMC_SYSINFO_GROUP_B:
		width = 2;
		shift = 28;
		break;
	case TEL_ATTR_BBMC_SYSINFO_GROUP_C:
		width = 2;
		shift = 30;
		break;
	default:
		return;
	}

	SceUInt32 maxVal = (1U << width) - 1U;
	if (encoded > maxVal)
		encoded = maxVal;
	SceUInt32 fieldMask = maxVal << shift;

	m_mutex.Lock();
	SceUInt32 nextBits = (m_evfBits & ~fieldMask) | ((encoded & maxVal) << shift);
	if (nextBits != m_evfBits) {
		m_evfBits = nextBits;
		if (m_sysInfoEvf >= 0) {
			sceKernelClearEventFlag(m_sysInfoEvf, fieldMask);
			sceKernelSetEventFlag(m_sysInfoEvf, m_evfBits);
		}
	}
	m_mutex.Unlock();
}

/* 0x813D9AF2 */
void TelBbmcSharedData::SetInt(int attrId, SceUInt32 value)
{
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->SetInt(attrId, (int)value);
	UpdateSysInfoEventFlag(attrId, value);
}

/* 0x813D9FC8 */
int TelBbmcSharedData::GetInt(int attrId)
{
	int value = 0;
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->GetInt(attrId, &value);
	return value;
}

/* 0x813DAB0E */
void TelBbmcSharedData::AddInt(int attrId, int delta)
{
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->AddInt(attrId, delta);
	int updated = GetInt(attrId);
	UpdateSysInfoEventFlag(attrId, (SceUInt32)updated);
}

/* 0x813DA374 */
void TelBbmcSharedData::SetBool(int attrId, bool value)
{
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->SetBool(attrId, value);
	UpdateSysInfoEventFlag(attrId, value ? 1U : 0U);
}

/* 0x813D9F80 */
bool TelBbmcSharedData::GetBool(int attrId)
{
	bool value = false;
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->GetBool(attrId, &value);
	return value;
}

/* 0x813DA64C */
void TelBbmcSharedData::SetString(int attrId, const paf::string &value)
{
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->SetString(attrId, value);
}

/* 0x813DAB80 */
void TelBbmcSharedData::SetStringIfEmpty(int attrId, const paf::string &value)
{
	paf::string existing = GetString(attrId);
	if (existing.empty())
		SetString(attrId, value);
}

/* 0x813DAA68 */
paf::string TelBbmcSharedData::GetString(int attrId)
{
	paf::string value;
	TelAttributeStore *store = telGetBbmcAttributeStore();
	if (store != NULL)
		store->GetString(attrId, &value);
	return value;
}

/* 0x813DAABE */
paf::string TelBbmcSharedData::GetHomeDescription()
{
	return GetString(TEL_ATTR_BBMC_HOME_DESC);
}

/* 0x813DA9EE */
void TelBbmcSharedData::GetHomePlmn(SceUInt16 *outMcc, SceUInt16 *outMnc)
{
	*outMcc = (SceUInt16)GetInt(TEL_ATTR_BBMC_HOME_MCC);
	*outMnc = (SceUInt16)GetInt(TEL_ATTR_BBMC_HOME_MNC);
}

/* 0x813DA00C */
TelephonyResult TelBbmcSharedData::CheckSimCondition()
{
	TelephonyResult res(TEL_OK);
	int disableCount = GetInt(TEL_ATTR_BBMC_SIM_DISABLE_COUNT);
	if (disableCount != 0) {
		if (GetInt(TEL_ATTR_BBMC_SIM_STATE) == 1) {
			res.SetCode(TEL_ERR_SIM_REMOVED);
		} else if (GetBool(TEL_ATTR_BBMC_SIM_BOOT_MISSING) && disableCount == 1) {
			res.SetCode(TEL_ERR_SIM_INIT_FAILED);
		} else {
			res.SetCode(TEL_ERR_NO_SIM);
		}
	} else {
		if (GetInt(TEL_ATTR_BBMC_SIM_ENABLE_COUNT) != 0) {
			if (GetBool(TEL_ATTR_BBMC_SIM_INVALID))
				res.SetCode(TEL_ERR_SIM_INVALID);
		} else {
			res.SetCode(TEL_ERR_NOT_AVAILABLE);
		}
	}
	return res;
}

/* 0x813DA176 */
bool TelBbmcSharedData::IsRadioReady()
{
	if (!CheckSimCondition().IsSuccess())
		return false;
	if (GetInt(TEL_ATTR_BBMC_CK_EVENT_COUNT) != 0)
		return false;
	if (GetInt(TEL_ATTR_BBMC_DRIVER_STATE) < 6)
		return false;
	if (!GetBool(TEL_ATTR_BBMC_DEVINFO_VALID))
		return false;
	if (GetInt(TEL_ATTR_BBMC_TEMP_STATE) != 0)
		return false;
	if (GetInt(TEL_ATTR_BBMC_VOLTAGE_STATE) == 0)
		return false;
	if (GetInt(TEL_ATTR_BBMC_LPM_OVERRIDE) == 1)
		return false;
	return true;
}

/* 0x813DA2E4 */
void TelBbmcSharedData::RecordDriverState(SceUInt8 state)
{
	m_mutex.Lock();
	m_stateHistory.PushState(state);
	m_mutex.Unlock();
}

/* 0x813DA352 */
paf::string TelBbmcSharedData::FormatStateHistory()
{
	m_mutex.Lock();
	paf::string text = m_stateHistory.FormatHex();
	m_mutex.Unlock();
	return text;
}

/* 0x813DA3D2 */
void TelBbmcSharedData::ResetPinAttributes()
{
	SetBool(TEL_ATTR_BBMC_PIN_VALID, false);
	SetInt(TEL_ATTR_BBMC_PIN_STATUS, 2);
	SetInt(TEL_ATTR_BBMC_PIN_ENABLED_STATE, 0);
	SetInt(TEL_ATTR_BBMC_PIN_RETRIES, 0);
}

/* 0x813DA4A6 */
void TelBbmcSharedData::SetControlKeyAttributes(SceUInt32 ckState, SceUInt32 blockType)
{
	SetInt(TEL_ATTR_BBMC_CK_STATE, ckState);
	SetInt(TEL_ATTR_BBMC_CK_BLOCK_TYPE, blockType);
}

/* 0x813DA530 */
bool TelBbmcSharedData::HasControlKeyStatus()
{
	int ckState = GetInt(TEL_ATTR_BBMC_CK_STATE);
	if (ckState >= 1 && ckState <= 3)
		return true;
	return GetBool(TEL_ATTR_BBMC_CK_PERMANENT_BLOCKED);
}

/* 0x813DA5B8 */
void TelBbmcSharedData::SetSimStateAttributes(SceUInt32 simState, bool simInvalid)
{
	SetInt(TEL_ATTR_BBMC_SIM_STATE, simState);
	SetBool(TEL_ATTR_BBMC_SIM_INVALID, simInvalid);
}

/* 0x813DA694 */
void TelBbmcSharedData::ResetDeviceInfoAttributes()
{
	paf::string emptyStr("");
	SetBool(TEL_ATTR_BBMC_DEVINFO_VALID, false);
	SetString(TEL_ATTR_BBMC_IMSI, emptyStr);
	SetString(TEL_ATTR_BBMC_MSISDN, emptyStr);
	SetString(TEL_ATTR_BBMC_ICCID, emptyStr);
	SetString(TEL_ATTR_BBMC_IMEI, emptyStr);
	SetString(TEL_ATTR_BBMC_ESN, emptyStr);
	SetString(TEL_ATTR_BBMC_MEID, emptyStr);
	SetInt(TEL_ATTR_BBMC_PROTOCOL, 0);
	SetInt(TEL_ATTR_BBMC_PRL_VERSION, 0);
	SetInt(TEL_ATTR_BBMC_HOME_MCC, 0);
	SetInt(TEL_ATTR_BBMC_HOME_MNC, 0);
	SetString(TEL_ATTR_BBMC_HOME_DESC, emptyStr);
}

} /* namespace telephony */
