/*
 * tel_mcc_mnc.cpp: TelephonyResult, TelephonyPlmn, TelephonyOperator, and MCC -> ISO country
 * code table (3.65-CEX: 0x8124E070..0x8124E57F, 0x813DD154..0x813DD158).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../tel_common.h"

namespace {

struct MccCountryEntry {
	SceUInt16 mcc;
	char countryCode[4];
};

/* 0x81514F68: 72-entry MCC to ISO 3166-1 alpha-2 country code table */
static const MccCountryEntry kMccCountryTable[] = {
	{ 722, "ar" }, { 505, "au" }, { 232, "at" }, { 426, "bh" },
	{ 206, "be" }, { 724, "br" }, { 284, "bg" }, { 302, "ca" },
	{ 730, "cl" }, { 732, "co" }, { 219, "hr" }, { 280, "cy" },
	{ 230, "cz" }, { 238, "dk" }, { 244, "fi" }, { 208, "fr" },
	{ 262, "de" }, { 202, "gr" }, { 454, "hk" }, { 216, "hu" },
	{ 274, "is" }, { 404, "in" }, { 405, "in" }, { 406, "in" },
	{ 510, "id" }, { 272, "ie" }, { 425, "il" }, { 222, "it" },
	{ 440, "jp" }, { 441, "jp" }, { 450, "kr" }, { 419, "kw" },
	{ 415, "lb" }, { 270, "lu" }, { 502, "my" }, { 278, "mt" },
	{ 334, "mx" }, { 204, "nl" }, { 530, "nz" }, { 242, "no" },
	{ 422, "om" }, { 716, "pe" }, { 260, "pl" }, { 268, "pt" },
	{ 427, "qa" }, { 226, "ro" }, { 250, "ru" }, { 420, "sa" },
	{ 525, "sg" }, { 231, "sk" }, { 293, "si" }, { 655, "za" },
	{ 214, "es" }, { 240, "se" }, { 228, "ch" }, { 466, "tw" },
	{ 520, "th" }, { 286, "tr" }, { 255, "ua" }, { 424, "ae" },
	{ 430, "ae" }, { 431, "ae" }, { 234, "gb" }, { 235, "gb" },
	{ 310, "us" }, { 311, "us" }, { 312, "us" }, { 313, "us" },
	{ 314, "us" }, { 315, "us" }, { 316, "us" }, {   0, ""   }
};

} /* namespace */

/* TelephonyResult (0x8124E288) */
TelephonyResult::~TelephonyResult()
{
}

int TelephonyResult::GetCode() const
{
	return m_code;
}

/* TelephonyOperator (0x8124E070..0x8124E287) */
TelephonyOperator::TelephonyOperator()
	: m_name(""), m_userId(""), m_password("")
{
}

TelephonyOperator::~TelephonyOperator()
{
}

void *TelephonyOperator::CloneObject() const
{
	return NULL;
}

/* 0x8124E26E */
bool TelephonyOperator::IsValid() const
{
	return m_name.length() <= 101U && m_userId.length() <= 127U &&
	       m_password.length() <= 127U;
}

/* 0x8124E11A */
TelephonyResult TelephonyOperator::SetCredentials(const paf::string &name,
						  const paf::string &userId,
						  const paf::string &password)
{
	m_name = name;
	m_userId = userId;
	m_password = password;
	return TelephonyResult(telephony::TEL_OK);
}

/* 0x8124E14A */
TelephonyResult TelephonyOperator::GetCredentials(paf::string *outName,
						  paf::string *outUserId,
						  paf::string *outPassword) const
{
	*outName = m_name;
	*outUserId = m_userId;
	*outPassword = m_password;
	return TelephonyResult(telephony::TEL_OK);
}

/* 0x8124E17A */
void TelephonyOperator::SetFromRawRecord(const char *rawRecord)
{
	const telephony::TelOperatorApnRawRecord *raw =
		(const telephony::TelOperatorApnRawRecord *)rawRecord;
	m_name = raw->apnName;
	m_userId = raw->userId;
	m_password = raw->password;
}

/* TelephonyPlmn (0x8124E28C..0x8124E57F, 0x813DD154..0x813DD158) */
TelephonyPlmn::TelephonyPlmn()
	: m_mcc(0), m_mccDigits(3), m_mnc(0), m_mncDigits(3), m_name("")
{
	sce_paf_memset(m_mccStr, 0, sizeof(m_mccStr));
	sce_paf_memset(m_mncStr, 0, sizeof(m_mncStr));
}

TelephonyPlmn::~TelephonyPlmn()
{
}

bool TelephonyPlmn::IsValid() const
{
	return true;
}

/* 0x8124E316 */
void TelephonyPlmn::SetMcc(SceUInt16 mcc, SceUInt16 digits)
{
	m_mcc = mcc;
	m_mccDigits = digits;
}

/* 0x8124E31C */
void TelephonyPlmn::SetMnc(SceUInt16 mnc, SceUInt16 digits)
{
	m_mnc = mnc;
	m_mncDigits = digits;
}

/* 0x8124E322 */
void TelephonyPlmn::SetMccMnc(SceUInt16 mcc, SceUInt16 mnc)
{
	m_mcc = mcc;
	m_mnc = mnc;
}

/* 0x8124E328 */
void TelephonyPlmn::SetMccMncStrings(const paf::string &mccStr, const paf::string &mncStr)
{
	m_mcc = (SceUInt16)sce_paf_atoi(mccStr.c_str());
	m_mccDigits = (SceUInt16)mccStr.length();
	m_mnc = (SceUInt16)sce_paf_atoi(mncStr.c_str());
	m_mncDigits = (SceUInt16)mncStr.length();
}

/* 0x8124E34C */
void TelephonyPlmn::SetPlmnString(const paf::string &plmnStr)
{
	paf::string mccPart = plmnStr.substr(0, 3);
	m_mcc = (SceUInt16)sce_paf_atoi(mccPart.c_str());
	m_mccDigits = (SceUInt16)mccPart.length();

	paf::string mncPart = plmnStr.substr(m_mccDigits, 3);
	m_mnc = (SceUInt16)sce_paf_atoi(mncPart.c_str());
	m_mncDigits = (SceUInt16)mncPart.length();
}

/* 0x8124E3CC */
void TelephonyPlmn::SetName(const paf::string &name)
{
	m_name = name;
}

/* 0x8124E3D6 */
const char *TelephonyPlmn::GetMccString()
{
	if (m_mccDigits == 2)
		sce_paf_snprintf(m_mccStr, 5, "%02d", (int)m_mcc);
	else
		sce_paf_snprintf(m_mccStr, 5, "%03d", (int)m_mcc);
	return m_mccStr;
}

/* 0x8124E420 */
const char *TelephonyPlmn::GetMncString()
{
	if (m_mncDigits == 2)
		sce_paf_snprintf(m_mncStr, 5, "%02d", (int)m_mnc);
	else
		sce_paf_snprintf(m_mncStr, 5, "%03d", (int)m_mnc);
	return m_mncStr;
}

/* 0x8124E46A */
paf::string TelephonyPlmn::GetPlmnString() const
{
	char mccBuf[6];
	char mncBuf[6];

	if (m_mccDigits == 2)
		sce_paf_snprintf(mccBuf, 5, "%02d", (int)m_mcc);
	else
		sce_paf_snprintf(mccBuf, 5, "%03d", (int)m_mcc);

	if (m_mncDigits == 2)
		sce_paf_snprintf(mncBuf, 5, "%02d", (int)m_mnc);
	else
		sce_paf_snprintf(mncBuf, 5, "%03d", (int)m_mnc);

	return telephony::telFormatString("%s%s", mccBuf, mncBuf);
}

/* 0x8124E51C */
paf::string TelephonyPlmn::GetName() const
{
	return m_name;
}

/* 0x8124E534 */
bool TelephonyPlmn::GetCountryCode(paf::string *outCountry) const
{
	const MccCountryEntry *entry = kMccCountryTable;
	while (entry->mcc != 0) {
		if (entry->mcc == m_mcc)
			break;
		++entry;
	}
	*outCountry = entry->countryCode;
	return outCountry->length() != 0;
}

/* 0x813DD154 */
int TelephonyPlmn::GetMcc() const
{
	return m_mcc;
}

/* 0x813DD158 */
int TelephonyPlmn::GetMnc() const
{
	return m_mnc;
}

/* External helper called by nsx_whats_new.cpp (0x8124E46A) */
paf::string telephonyPlmnFormatCode(const TelephonyPlmn *plmn)
{
	return plmn->GetPlmnString();
}
