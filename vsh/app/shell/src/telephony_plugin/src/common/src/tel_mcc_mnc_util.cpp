/*
 * tel_mcc_mnc_util.cpp: MCC/MNC digit-length resolution against APN list INI files
 * (3.65-CEX: 0x813DF270..0x813DF9B7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813DF270 */
TelMccMncUtil::TelMccMncUtil()
	: m_initialized(false)
{
}

/* 0x813DF282 */
TelMccMncUtil::~TelMccMncUtil()
{
	if (m_initialized)
		m_initialized = false;
}

/* 0x813DF32C */
bool TelMccMncUtil::HasApnEntry(const paf::string &plmnDigits, SceSize prefixLen)
{
	paf::string mccPart(plmnDigits.c_str(), 3);
	SceUInt16 mcc = (SceUInt16)sce_paf_atoi(mccPart.c_str());

	TelApnIniFileAccessor accessor;
	accessor.OpenForMcc(mcc);

	paf::string prefix(plmnDigits.c_str(), prefixLen);
	paf::string key = prefix + "_1_NAME";
	paf::string apnName;
	TelephonyResult res = accessor.GetString(key, &apnName);
	return res.GetCode() == TEL_OK;
}

/* 0x813DF55E */
TelephonyResult TelMccMncUtil::ResolveFromImsi(TelephonyPlmn *outPlmn, const paf::string &imsi)
{
	if (!m_initialized)
		m_initialized = true;

	SceSize matchedLen = 5;
	if (!HasApnEntry(imsi, 5)) {
		matchedLen = 6;
		if (!HasApnEntry(imsi, 6))
			return TelephonyResult(TEL_ERR_FAILED);
	}

	paf::string mccStr = imsi.substr(0, 3);
	paf::string mncStr = imsi.substr(3, matchedLen - 3);
	outPlmn->SetMccMncStrings(mccStr, mncStr);
	return TelephonyResult(TEL_OK);
}

/* 0x813DF75E */
TelephonyResult TelMccMncUtil::ResolveFromNumeric(TelephonyPlmn *outPlmn, SceUInt16 mcc,
						  SceUInt16 mnc)
{
	TelApnIniFileAccessor accessor;
	TelephonyResult res = accessor.OpenForMcc(mcc);
	if (res.GetCode() != TEL_OK)
		return res;

	TelephonyPlmn twoDigitCandidate;
	twoDigitCandidate.SetMcc(mcc, 3);
	twoDigitCandidate.SetMnc(mnc, 2);
	if (HasApnEntry(twoDigitCandidate.GetPlmnString(), 5)) {
		*outPlmn = twoDigitCandidate;
		return TelephonyResult(TEL_OK);
	}

	TelephonyPlmn threeDigitCandidate;
	threeDigitCandidate.SetMcc(mcc, 3);
	threeDigitCandidate.SetMnc(mnc, 3);
	if (HasApnEntry(threeDigitCandidate.GetPlmnString(), 6)) {
		*outPlmn = threeDigitCandidate;
		return TelephonyResult(TEL_OK);
	}

	return TelephonyResult(TEL_ERR_FAILED);
}

} /* namespace telephony */
