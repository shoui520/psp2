/*
 * tel_operator_profile.cpp: TelOperatorNetworkSetting, TelOperatorProfile, and
 * TelOperatorProfileFactory (3.65-CEX: 0x813E0C78..0x813E22DF).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <common/string_util.h>
#include "../tel_common.h"

namespace telephony {

/* TelOperatorNetworkSetting (0x813E0C78..0x813E0D4F) */
TelOperatorNetworkSetting::TelOperatorNetworkSetting()
	: m_networkType(0), m_billingPlan(0)
{
}

TelOperatorNetworkSetting::~TelOperatorNetworkSetting()
{
}

/* 0x813E0CDE */
void TelOperatorNetworkSetting::SetApnCredentials(const paf::string &apn,
						  const paf::string &userId,
						  const paf::string &password)
{
	m_apn.SetCredentials(apn, userId, password);
}

/* 0x813E0D1C */
void TelOperatorNetworkSetting::GetApnCredentials(TelephonyOperator *outOp) const
{
	outOp->SetCredentials(m_apn.GetName(), m_apn.GetUserId(), m_apn.GetPassword());
}

/* TelOperatorProfile (0x813E0D50..0x813E10E3) */
TelOperatorProfile::TelOperatorProfile()
	: m_operatorName(""), m_registrationUrl(""), m_iconPath(""),
	  m_needSimEnable(true), m_version("")
{
}

TelOperatorProfile::~TelOperatorProfile()
{
}

/* 0x81454564 */
paf::common::SharedPtr<TelObject> TelOperatorProfile::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelOperatorProfile(*this));
}

/* 0x813E0EC2 */
void TelOperatorProfile::SetPlmn(const TelephonyPlmn &plmn)
{
	m_plmn = plmn;
}

/* 0x813E0F44 */
TelephonyResult TelOperatorProfile::SetNetworkSetting(const TelOperatorNetworkSetting &setting)
{
	m_networkSetting = setting;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0F7E */
void TelOperatorProfile::SetCallEndReasons(const paf::vector<TelCallEndReasonInfo> &reasons)
{
	m_callEndReasons = reasons;
}

/* 0x813E1018 */
void TelOperatorProfile::PackToRaw(TelOperatorProfileRawRecord *outRaw) const
{
	outRaw->mcc = m_plmn.GetMcc();
	outRaw->mnc = m_plmn.GetMnc();
	outRaw->needSimEnable = m_needSimEnable ? 1 : 0;
	outRaw->billingPlan = m_networkSetting.GetBillingPlan();
	sce_paf_strncpy(outRaw->operatorName, m_operatorName.c_str(), 32);
	sce_paf_strncpy(outRaw->registrationUrl, m_registrationUrl.c_str(), 256);
	sce_paf_strncpy(outRaw->iconPath, m_iconPath.c_str(), 64);

	TelephonyOperator apn;
	m_networkSetting.GetApnCredentials(&apn);
	outRaw->networkType = m_networkSetting.GetNetworkType();
	sce_paf_strncpy(outRaw->apnName, apn.GetName().c_str(), 32);
	sce_paf_strncpy(outRaw->apnUserId, apn.GetUserId().c_str(), 32);
	sce_paf_strncpy(outRaw->apnPassword, apn.GetPassword().c_str(), 32);
}

/* TelOperatorProfileFactory (0x813E10E4..0x813E22DF) */
/* 0x813E10E4 */
TelOperatorProfileFactory::TelOperatorProfileFactory(int operatorId, const TelephonyPlmn &plmn,
						     SceUInt8 flags)
	: m_operatorId(operatorId), m_plmn(plmn),
	  m_checkPlmnMatch((flags & 0x0FU) == 0),
	  m_urlVariantIndex((int)(flags >> 4))
{
}

/* 0x813E1192 */
TelOperatorProfileFactory::~TelOperatorProfileFactory()
{
}

/* 0x813E11D4 */
void TelOperatorProfileFactory::ParseReasonList(const paf::string &csvText,
						paf::vector<int> *outReasons) const
{
	paf::list<paf::string> tokens;
	paf::common::string_util::tokenize(tokens, csvText, 0, ',');
	for (paf::list<paf::string>::iterator it = tokens.begin(); it != tokens.end(); ++it)
		outReasons->push_back(sce_paf_atoi(it->c_str()));
}

/* 0x813E12B0 */
bool TelOperatorProfileFactory::MatchesPlmnList(const paf::string &csvText,
						const TelephonyPlmn &plmn) const
{
	paf::list<paf::string> tokens;
	paf::common::string_util::tokenize(tokens, csvText, 0, ',');
	for (paf::list<paf::string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
		paf::string targetCode = plmn.GetPlmnString();
		if (*it == targetCode)
			return true;
	}
	return false;
}

/* 0x813E13DC */
TelephonyResult TelOperatorProfileFactory::LoadProfiles(
	paf::vector<TelOperatorProfile> *outProfiles)
{
	TelIniFileAccessor accessor;
	accessor.Initialize();
	TelephonyResult res = accessor.Open(
		paf::string("vs0:vsh/shell/telephony/tel_operator_profile.ini"), false);
	if (res.GetCode() != TEL_OK) {
		accessor.Close();
		accessor.Terminate();
		return res;
	}

	paf::string key = telFormatString("TEL_REG_OPCO%d_MCC_MNC", m_operatorId);
	paf::string value;
	res = accessor.GetString(key, &value);
	if (res.GetCode() != TEL_OK) {
		accessor.Close();
		accessor.Terminate();
		return TelephonyResult(TEL_ERR_PROFILE_NOT_FOUND);
	}

	bool plmnMatched = MatchesPlmnList(value, m_plmn);
	if (!m_checkPlmnMatch || plmnMatched) {
		TelOperatorProfile baseProfile;
		baseProfile.SetPlmn(m_plmn);

		key = telFormatString("TEL_REG_OPCO%d_NAME", m_operatorId);
		res = accessor.GetString(key, &value);
		baseProfile.SetOperatorName(value);

		key = telFormatString("TEL_REG_OPCO%d_ICON", m_operatorId);
		accessor.GetString(key, &value);
		baseProfile.SetIconPath(value);

		key = telFormatString("TEL_REG_OPCO%d_VER", m_operatorId);
		res = accessor.GetString(key, &value);
		if (res.GetCode() != TEL_OK)
			baseProfile.SetVersion(paf::string(""));
		else
			baseProfile.SetVersion(value);

		int needSimEnable = 0;
		key = telFormatString("TEL_REG_OPCO%d_NEED_SIM_ENABLE", m_operatorId);
		res = accessor.GetInt(key, &needSimEnable);
		baseProfile.SetNeedSimEnable(needSimEnable == 1);

		paf::vector<TelCallEndReasonInfo> callEndReasons;
		int callEndCount = 0;
		key = telFormatString("TEL_REG_OPCO%d_CALL_END_COUNT", m_operatorId);
		res = accessor.GetInt(key, &callEndCount);
		for (int idx = 1; idx <= callEndCount; ++idx) {
			int endType = 0;
			paf::string reasonCsv;
			key = telFormatString("TEL_REG_OPCO%d_CALL_END_TYPE_%d", m_operatorId, idx);
			TelephonyResult typeRes = accessor.GetInt(key, &endType);
			key = telFormatString("TEL_REG_OPCO%d_CALL_END_REASON_%d", m_operatorId, idx);
			TelephonyResult reasonRes = accessor.GetString(key, &reasonCsv);
			if (typeRes.GetCode() == TEL_OK && reasonRes.GetCode() == TEL_OK) {
				paf::vector<int> codes;
				ParseReasonList(reasonCsv, &codes);
				callEndReasons.push_back(TelCallEndReasonInfo(endType, codes));
			}
		}
		baseProfile.SetCallEndReasons(callEndReasons);

		int networkCount = 0;
		key = telFormatString("TEL_REG_OPCO%d_NETWORK_COUNT", m_operatorId);
		res = accessor.GetInt(key, &networkCount);
		if (networkCount == 0) {
			outProfiles->push_back(baseProfile);
		} else {
			for (int netIdx = 1; netIdx <= networkCount; ++netIdx) {
				TelOperatorProfile profileCopy = baseProfile;
				TelOperatorNetworkSetting netSetting;
				paf::string netTypeStr;
				paf::string apnName;
				paf::string userId;
				paf::string password;
				paf::string billingPlanStr;
				paf::string regUrl;

				key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_TYPE",
						      m_operatorId, netIdx);
				res = accessor.GetString(key, &netTypeStr);
				key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_APN",
						      m_operatorId, netIdx);
				res = accessor.GetString(key, &apnName);
				key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_ID",
						      m_operatorId, netIdx);
				res = accessor.GetString(key, &userId);
				key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_PW",
						      m_operatorId, netIdx);
				res = accessor.GetString(key, &password);
				key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_BILLING_PLAN",
						      m_operatorId, netIdx);
				accessor.GetString(key, &billingPlanStr);

				bool urlFound = false;
				if (m_urlVariantIndex != 0) {
					key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_URL_%d",
							      m_operatorId, netIdx,
							      m_urlVariantIndex);
					res = accessor.GetString(key, &regUrl);
					if (res.GetCode() == TEL_OK)
						urlFound = true;
					else
						m_urlVariantIndex = 0;
				}
				if (!urlFound) {
					key = telFormatString("TEL_REG_OPCO%d_NETWORK%d_URL_DEFAULT",
							      m_operatorId, netIdx);
					res = accessor.GetString(key, &regUrl);
				}

				if (netTypeStr == "WWAN") {
					netSetting.SetNetworkType(1);
				} else if (netTypeStr == "WIFI") {
					netSetting.SetNetworkType(2);
				} else if (netTypeStr == "AUTO") {
					netSetting.SetNetworkType(3);
				} else {
					res = TelephonyResult(TEL_ERR_PROFILE_NOT_FOUND);
					break;
				}

				netSetting.SetApnCredentials(apnName, userId, password);
				netSetting.SetBillingPlan(sce_paf_atoi(billingPlanStr.c_str()));
				profileCopy.SetNetworkSetting(netSetting);
				profileCopy.SetRegistrationUrl(regUrl);
				outProfiles->push_back(profileCopy);
			}
		}
	}

	accessor.Close();
	accessor.Terminate();
	return res;
}

/* 0x813E2084 */
TelephonyResult TelOperatorProfileFactory::LoadSupportedPlmns(
	paf::vector<TelephonyPlmn> *outPlmns)
{
	TelIniFileAccessor accessor;
	accessor.Initialize();
	TelephonyResult res = accessor.Open(
		paf::string("vs0:vsh/shell/telephony/tel_operator_profile.ini"), false);
	if (res.GetCode() != TEL_OK) {
		accessor.Close();
		accessor.Terminate();
		return res;
	}

	paf::string key = telFormatString("TEL_REG_OPCO%d_MCC_MNC", m_operatorId);
	paf::string csvText;
	res = accessor.GetString(key, &csvText);
	if (res.GetCode() != TEL_OK) {
		res = TelephonyResult(TEL_ERR_PROFILE_NOT_FOUND);
	} else {
		paf::list<paf::string> tokens;
		paf::common::string_util::tokenize(tokens, csvText, 0, ',');
		for (paf::list<paf::string>::iterator it = tokens.begin();
		     it != tokens.end(); ++it) {
			TelephonyPlmn plmn;
			plmn.SetPlmnString(*it);
			outPlmns->push_back(plmn);
		}
	}

	accessor.Close();
	accessor.Terminate();
	return res;
}

} /* namespace telephony */
