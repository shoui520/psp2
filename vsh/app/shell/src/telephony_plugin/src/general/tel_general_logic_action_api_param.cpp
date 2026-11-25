/*
 * tel_general_logic_action_api_param.cpp: input/output parameter and per-API
 * result store embedded in TelGeneralLogicAction
 * (3.65-CEX: 0x8126904C..0x8126933B).
 */
#include "tel_general_internal.h"

namespace telephony {

/* 0x8126904C */
TelGeneralLogicActionApiParam::TelGeneralLogicActionApiParam()
	: stateResult(0),
	  flightModeInput(0),
	  flightModeOutput(false),
	  iccid(""),
	  pinProtectionInput(false),
	  pinProtectionOutput(false),
	  pinStatusOutput(0),
	  pinRemainOutput(0),
	  saveApnToRegistry(false),
	  operatorProfileCount(0),
	  operatorProfileIndex(0),
	  apnListCount(0),
	  apnListIndex(0),
	  networkScanCount(0),
	  networkScanIndex(0),
	  dataRoamingPermission(false),
	  dataRoamingDefinition(0),
	  openSimModel(false),
	  conditionError(TEL_OK),
	  simLockDeactivateRemain(0),
	  simLockUnblockRemain(0),
	  internalProtectionEnable(false),
	  internalApnSaveFlag(false),
	  internalBillingSaveFlag(false),
	  checkOperatorCode(0),
	  checkOperatorMatched(false),
	  m_currentApiId(TEL_API_NONE)
{
	for (int idx = 0; idx < TEL_API_COUNT; ++idx)
		m_apiResults[idx] = TelephonyResult(TEL_OK);
}

/* 0x81269186 */
TelGeneralLogicActionApiParam::~TelGeneralLogicActionApiParam()
{
}

/* 0x8126930E */
TelephonyResult TelGeneralLogicActionApiParam::SetCurrentApiId(int apiId)
{
	m_currentApiId = apiId;
	return TelephonyResult(TEL_OK);
}

/* 0x8126931E */
int TelGeneralLogicActionApiParam::GetCurrentApiId() const
{
	return m_currentApiId;
}

/* 0x81269324 */
void TelGeneralLogicActionApiParam::SetApiResult(int apiId, const TelephonyResult &result)
{
	if (apiId >= 0 && apiId < TEL_API_COUNT)
		m_apiResults[apiId] = result;
}

/* 0x81269330 */
TelephonyResult TelGeneralLogicActionApiParam::GetApiResult(int apiId) const
{
	if (apiId >= 0 && apiId < TEL_API_COUNT)
		return m_apiResults[apiId];
	return TelephonyResult(TEL_ERR_INVALID_ARG);
}

} /* namespace telephony */
