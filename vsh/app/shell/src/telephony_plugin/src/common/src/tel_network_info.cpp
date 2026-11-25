/*
 * tel_network_info.cpp: TelNetScanInfo, TelNetworkCodeUtil, TelNetworkInfo,
 * TelPacketBearerInfo, TelCallEndReasonInfo, and TelRoamingInfo
 * (3.65-CEX: 0x813DFDF8..0x813E0C77, 0x813E22E0..0x813E24B3, 0x813E36B0..0x813E3713).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string.h>
#include <common/char_encoding.h>
#include "../tel_common.h"

namespace telephony {

namespace {

/* 0x81529F64: network type mapping table (7 entries) */
static const int kNetworkTypeMap[7] = { 0, 5, 6, 6, 6, 6, 5 };

/* 0x81529F80: roaming state mapping table (5 entries) */
static const int kRoamingStateMap[5] = { 0, 1, 2, 2, 2 };

/* 0x8152A1C8: packet bearer capability mapping table (10 entries) */
static const int kPacketBearerMap[10] = { 0, 0, 0, 0, 0, 6, 3, 4, 4, 2 };

static paf::thread::Mutex g_operatorNameLookupMutex("SceTelCmnNetCodeUtilOpName", 0);

} /* namespace */

/* TelRoamingInfo (0x813E36B0..0x813E3713) */
TelRoamingInfo::TelRoamingInfo()
	: m_state(0)
{
}

TelRoamingInfo::~TelRoamingInfo()
{
}

/* 0x813E36E8 */
TelephonyResult TelRoamingInfo::SetState(int state)
{
	if (state >= 5)
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	m_state = state;
	return TelephonyResult(TEL_OK);
}

/* TelPacketBearerInfo (0x813E22E0..0x813E235F) */
TelPacketBearerInfo::TelPacketBearerInfo()
	: m_bearerType(0)
{
}

TelPacketBearerInfo::~TelPacketBearerInfo()
{
}

/* 0x813E2318 */
TelephonyResult TelPacketBearerInfo::SetBearerType(int bearerType)
{
	if (bearerType >= 10)
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	m_bearerType = bearerType;
	return TelephonyResult(TEL_OK);
}

/* 0x813E2342 */
TelephonyResult TelPacketBearerInfo::GetMappedBearerType(int *outMappedType) const
{
	*outMappedType = kPacketBearerMap[m_bearerType];
	return TelephonyResult(TEL_OK);
}

/* TelCallEndReasonInfo (0x813E2360..0x813E24B3) */
TelCallEndReasonInfo::TelCallEndReasonInfo()
	: m_callEndType(0)
{
}

TelCallEndReasonInfo::TelCallEndReasonInfo(int callEndType, const paf::vector<int> &reasonCodes)
	: m_callEndType(callEndType), m_reasonCodes(reasonCodes)
{
}

TelCallEndReasonInfo::~TelCallEndReasonInfo()
{
}

/* 0x813E23EE */
TelephonyResult TelCallEndReasonInfo::SetSingleReason(int callEndType, int reasonCode)
{
	m_callEndType = callEndType;
	m_reasonCodes.clear();
	m_reasonCodes.push_back(reasonCode);
	return TelephonyResult(TEL_OK);
}

/* 0x813E2454 */
TelephonyResult TelCallEndReasonInfo::GetFirstReason(int *outCallEndType,
						     int *outReasonCode) const
{
	*outCallEndType = m_callEndType;
	*outReasonCode = (m_reasonCodes.size() != 0) ? m_reasonCodes[0] : 0;
	return TelephonyResult(TEL_OK);
}

/* 0x813E2474 */
bool TelCallEndReasonInfo::Matches(int callEndType, int reasonCode) const
{
	if (m_callEndType != callEndType)
		return false;
	for (SceSize idx = 0; idx < m_reasonCodes.size(); ++idx) {
		if (m_reasonCodes[idx] == reasonCode)
			return true;
	}
	return false;
}

/* 0x813E24A2 */
bool TelCallEndReasonInfo::IsValid() const
{
	return m_callEndType > 0 && m_reasonCodes.size() != 0;
}

/* TelNetScanInfo (0x813DFDF8..0x813E023F) */
TelNetScanInfo::TelNetScanInfo()
	: m_operatorName(L""), m_status(0), m_accessTech(0),
	  m_signalQuality(0), m_radioCapability(0)
{
}

TelNetScanInfo::TelNetScanInfo(const TelNetScanInfo &src)
	: m_operatorName(L""), m_status(0), m_accessTech(0),
	  m_signalQuality(0), m_radioCapability(0)
{
	*this = src;
}

TelNetScanInfo &TelNetScanInfo::operator=(const TelNetScanInfo &src)
{
	if (this != &src) {
		m_plmn.SetMccMnc((SceUInt16)src.m_plmn.GetMcc(), (SceUInt16)src.m_plmn.GetMnc());
		m_operatorName = src.m_operatorName;
		m_status = src.m_status;
		m_accessTech = src.m_accessTech;
		m_roamingInfo.SetState(src.m_roamingInfo.GetState());
		m_signalQuality = src.m_signalQuality;
		m_radioCapability = src.m_radioCapability;
	}
	return *this;
}

TelNetScanInfo::~TelNetScanInfo()
{
}

TelephonyResult TelNetScanInfo::SetPlmnCodes(SceUInt16 mcc, SceUInt16 mnc)
{
	m_plmn.SetMccMnc(mcc, mnc);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::GetPlmn(TelephonyPlmn *outPlmn) const
{
	outPlmn->SetMccMnc((SceUInt16)m_plmn.GetMcc(), (SceUInt16)m_plmn.GetMnc());
	paf::string utf8Name;
	paf::common::Utf16ToUtf8(m_operatorName, &utf8Name);
	outPlmn->SetName(utf8Name);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetOperatorNameUtf8(const paf::string &nameUtf8)
{
	paf::common::Utf8ToUtf16(nameUtf8, &m_operatorName, 0);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetOperatorName(const paf::wstring &name)
{
	m_operatorName = name;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetStatus(int status)
{
	m_status = status;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetAccessTech(int accessTech)
{
	m_accessTech = accessTech;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetRoamingState(int roamingState)
{
	m_roamingInfo.SetState(roamingState);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetSignalQuality(int quality)
{
	m_signalQuality = quality;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelNetScanInfo::SetRadioCapability(int capability)
{
	m_radioCapability = capability;
	return TelephonyResult(TEL_OK);
}

/* 0x813E017C */
void TelNetScanInfo::PackToRaw(TelNetScanRawRecord *outRaw) const
{
	outRaw->mcc = m_plmn.GetMcc();
	outRaw->mnc = m_plmn.GetMnc();
	outRaw->status = m_status;
	outRaw->accessTech = m_accessTech;
	outRaw->roamingState = m_roamingInfo.GetState();
	outRaw->signalQuality = m_signalQuality;
	outRaw->radioCapability = m_radioCapability;
	sce_paf_wcsncpy(outRaw->operatorName, m_operatorName.c_str(), 32);
}

/* 0x813E01BC */
void TelNetScanInfo::UnpackFromRaw(const TelNetScanRawRecord *raw)
{
	m_plmn.SetMcc((SceUInt16)raw->mcc, 3);
	m_plmn.SetMnc((SceUInt16)raw->mnc, 3);
	m_status = raw->status;
	m_accessTech = raw->accessTech;
	m_roamingInfo.SetState(raw->roamingState);
	m_signalQuality = raw->signalQuality;
	m_radioCapability = raw->radioCapability;
	m_operatorName = raw->operatorName;
}

/* TelNetworkCodeUtil (0x813E0240..0x813E09DB) */
/* 0x813E0240 */
paf::string TelNetworkCodeUtil::FormatPlmnFallbackName(const TelephonyPlmn &plmn)
{
	paf::string name = plmn.GetName();
	if (name.length() == 0) {
		TelephonyPlmn mccCopy = plmn;
		TelephonyPlmn mncCopy = plmn;
		name = telFormatString("%s-%s", mccCopy.GetMccString(), mncCopy.GetMncString());
	}
	return name;
}

/* 0x813E049C */
TelephonyResult TelNetworkCodeUtil::LookupOperatorNames(const TelephonyPlmn &plmn,
							paf::wstring *outFullName,
							paf::wstring *outShortName)
{
	g_operatorNameLookupMutex.Lock();
	*outFullName = L"";
	*outShortName = L"";

	if (plmn.GetMcc() == 0 && plmn.GetMnc() == 0) {
		g_operatorNameLookupMutex.Unlock();
		return TelephonyResult(TEL_OK);
	}

	TelIniFileAccessor accessor;
	accessor.Initialize();
	TelephonyResult res = accessor.Open(
		paf::string("vs0:vsh/shell/telephony/tel_operator_name_list.ini"), false);
	if (res.GetCode() != TEL_OK) {
		paf::string fallback = FormatPlmnFallbackName(plmn);
		paf::common::Utf8ToUtf16(fallback, outFullName, 0);
		*outShortName = *outFullName;
		accessor.Close();
		accessor.Terminate();
		g_operatorNameLookupMutex.Unlock();
		return TelephonyResult(TEL_OK);
	}

	paf::string fullKey = telFormatString("%03d%03dPpciN", plmn.GetMcc(), plmn.GetMnc());
	paf::string foundName;
	res = accessor.GetString(fullKey, &foundName);
	if (res.GetCode() != TEL_OK) {
		paf::string fallback = FormatPlmnFallbackName(plmn);
		paf::common::Utf8ToUtf16(fallback, outFullName, 0);
		*outShortName = *outFullName;
		accessor.Close();
		accessor.Terminate();
		g_operatorNameLookupMutex.Unlock();
		return TelephonyResult(TEL_OK);
	}

	paf::common::Utf8ToUtf16(foundName, outFullName, 0);

	paf::string abbrevKey = telFormatString("%03d%03dAbrev", plmn.GetMcc(), plmn.GetMnc());
	res = accessor.GetString(abbrevKey, &foundName);
	if (res.GetCode() != TEL_OK) {
		paf::string fallback = FormatPlmnFallbackName(plmn);
		paf::common::Utf8ToUtf16(fallback, outShortName, 0);
		res = TelephonyResult(TEL_OK);
	} else {
		paf::common::Utf8ToUtf16(foundName, outShortName, 0);
	}

	accessor.Close();
	accessor.Terminate();
	g_operatorNameLookupMutex.Unlock();
	return res;
}

/* TelNetworkInfo (0x813E09DC..0x813E0C77) */
TelNetworkInfo::TelNetworkInfo()
	: m_operatorName("")
{
	Reset();
}

TelNetworkInfo::~TelNetworkInfo()
{
}

/* 0x813E09DC */
TelephonyResult TelNetworkInfo::Reset()
{
	m_networkType = 0;
	m_registrationState = 0;
	m_mcc = 0;
	m_mnc = 0;
	m_signalRssi = 0;
	m_signalLevel = -1;
	m_roamingState = 0;
	m_operatorName = "";
	return TelephonyResult(TEL_OK);
}

/* 0x813E0AFA */
TelephonyResult TelNetworkInfo::SetNetworkType(int networkType)
{
	if (networkType >= 7)
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	m_networkType = networkType;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0B24 */
TelephonyResult TelNetworkInfo::GetMappedNetworkType(int *outType) const
{
	*outType = kNetworkTypeMap[m_networkType];
	return TelephonyResult(TEL_OK);
}

/* 0x813E0B40 */
TelephonyResult TelNetworkInfo::SetRegistrationState(int regState)
{
	if (regState >= 5)
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	m_registrationState = regState;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0B6A */
TelephonyResult TelNetworkInfo::SetPlmnCodes(SceUInt16 mcc, SceUInt16 mnc)
{
	m_mcc = mcc;
	m_mnc = mnc;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0B7C */
TelephonyResult TelNetworkInfo::GetPlmn(TelephonyPlmn *outPlmn) const
{
	outPlmn->SetMccMnc(m_mcc, m_mnc);
	outPlmn->SetName(m_operatorName);
	return TelephonyResult(TEL_OK);
}

/* 0x813E0BE6 */
TelephonyResult TelNetworkInfo::SetSignalRssi(int rssi)
{
	m_signalRssi = rssi;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0BF6 */
TelephonyResult TelNetworkInfo::GetSignalRssi(int *outRssi) const
{
	*outRssi = m_signalRssi;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0C06 */
TelephonyResult TelNetworkInfo::SetSignalLevel(int level)
{
	m_signalLevel = level;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0C16 */
TelephonyResult TelNetworkInfo::GetSignalLevel(int *outLevel) const
{
	*outLevel = m_signalLevel;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0C26 */
TelephonyResult TelNetworkInfo::SetRoamingState(int roamingState)
{
	if (roamingState >= 5)
		return TelephonyResult(TEL_ERR_INVALID_ARG);
	m_roamingState = roamingState;
	return TelephonyResult(TEL_OK);
}

/* 0x813E0C50 */
TelephonyResult TelNetworkInfo::GetMappedRoamingState(int *outState) const
{
	*outState = kRoamingStateMap[m_roamingState];
	return TelephonyResult(TEL_OK);
}

/* 0x813E0C6C */
void TelNetworkInfo::SetOperatorName(const paf::string &name)
{
	m_operatorName = name;
}

} /* namespace telephony */
