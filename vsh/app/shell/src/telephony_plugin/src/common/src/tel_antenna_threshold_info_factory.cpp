/*
 * tel_antenna_threshold_info_factory.cpp: antenna signal strength threshold tables
 * loaded from vs0:vsh/shell/telephony/tel_antenna_info.ini
 * (3.65-CEX: 0x813DBD48..0x813DC28F).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <common/string_util.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813DBD48 */
TelAntennaLevelThreshold::TelAntennaLevelThreshold()
	: m_threshold(0)
{
}

/* 0x813DBD5C */
TelAntennaLevelThreshold::~TelAntennaLevelThreshold()
{
}

/* 0x813DBD80 */
TelephonyResult TelAntennaLevelThreshold::SetThreshold(int threshold)
{
	m_threshold = threshold;
	return TelephonyResult(TEL_OK);
}

/* 0x813DBD90 */
TelAntennaThresholdInfo::TelAntennaThresholdInfo()
{
}

/* 0x813DBDA8 */
TelAntennaThresholdInfo::~TelAntennaThresholdInfo()
{
}

/* 0x813DBDFA */
TelephonyResult TelAntennaThresholdInfo::SetThresholds(const paf::vector<int> &thresholds)
{
	m_thresholds = thresholds;
	return TelephonyResult(TEL_OK);
}

/* 0x813DBE4C */
TelephonyResult TelAntennaThresholdInfo::GetThresholds(paf::vector<int> *outThresholds) const
{
	*outThresholds = m_thresholds;
	return TelephonyResult(TEL_OK);
}

/* 0x813DBEA0 */
TelAntennaThresholdInfoFactory::~TelAntennaThresholdInfoFactory()
{
}

/* 0x813DBEC4 */
TelephonyResult TelAntennaThresholdInfoFactory::Create(int radioType,
						       TelAntennaThresholdInfo *outInfo)
{
	TelAntennaThresholdInfoUtil util;
	return util.LoadFromIni(radioType, outInfo);
}

/* 0x813DBF20 */
TelAntennaThresholdInfoUtil::~TelAntennaThresholdInfoUtil()
{
}

/* 0x813DBF44 */
TelephonyResult TelAntennaThresholdInfoUtil::LoadFromIni(int radioType,
							 TelAntennaThresholdInfo *outInfo)
{
	TelIniFileAccessor accessor;
	accessor.Initialize();

	paf::string iniPath("vs0:vsh/shell/telephony/tel_antenna_info.ini");
	TelephonyResult res = accessor.Open(iniPath, false);
	if (res.GetCode() == TEL_OK) {
		paf::vector<int> thresholds;
		while (res.GetCode() == TEL_OK) {
			paf::string key("ANTENNA_LEVEL_MAP");
			paf::string line;
			res = accessor.GetNextString(key, &line);
			if (res.GetCode() != TEL_OK)
				break;

			paf::list<paf::string> tokens;
			paf::common::string_util::tokenize(tokens, line, 0, ',');
			if (tokens.size() == 0)
				continue;

			paf::list<paf::string>::iterator it = tokens.begin();
			int entryRadioType = sce_paf_atoi(it->c_str());
			++it;
			if (entryRadioType == radioType) {
				while (it != tokens.end()) {
					int val = sce_paf_abs(sce_paf_atoi(it->c_str()));
					thresholds.push_back(val);
					++it;
				}
				outInfo->SetThresholds(thresholds);
				break;
			}
		}
	}

	accessor.Close();
	accessor.Terminate();
	return res;
}

} /* namespace telephony */
