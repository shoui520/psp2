/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * Data models for sign-in credentials, account/country/language selection,
 * date of birth validation, and password change/confirmation validation.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/char_encoding.h>
#include <datetime/datetime.h>
#include <versatileparam/idparam.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::datetime;

/* Global model instances shared across scenes and Commerce2 jobs */
SignInCredentialsModel *g_credentialsModel = NULL;
void *g_unusedModelSlot1 = NULL;
AccountInfoModel *g_accountInfoModel = NULL;
void *g_unusedModelSlot2 = NULL;
PasswordChangeModel *g_passwordChangeModel = NULL;
void *g_unusedModelSlot3 = NULL;
DobVerifyModel *g_dobVerifyModel = NULL;
void *g_unusedModelSlot4 = NULL;

static int32_t s_cachedCurrentYear = 0;
static wchar_t s_yearBuf[8];
static wchar_t s_monthBuf[4];
static wchar_t s_dayBuf[4];

static const int32_t s_daysInMonth[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const char *const s_langCodes[20] = {
	"ja", "en", "fr", "es", "de",
	"it", "nl", "pt", "ru", "ko",
	"ch", "zh", "fi", "sv", "da",
	"no", "pl", "pt", "en", "tr"
};

static const uint32_t s_langResIds[20] = {
	0xE7495CCEU, 0x9685804EU, 0x48285C8EU, 0x18355FAAU, 0x7DFCFB59U,
	0x3C2F44E7U, 0xC53644A6U, 0x67C4BD32U, 0x309246E6U, 0x0FFFE029U,
	0x745BD7F4U, 0x6234FCC8U, 0x6E79A394U, 0xD5C378DDU, 0xF5E0CC3DU,
	0x476FA812U, 0x21DBD393U, 0xE3743171U, 0xC36CCB2EU, 0xD3C831CBU
};

static const wchar_t s_emptyWideStr[2] = { 0, 0 };

/* ---- Language code mapping helper ---- */

int32_t SigninExtFindLanguageIndex(const char *langCode)
{
	for (int32_t i = 0; i < 20; ++i) {
		if (sce_paf_strcmp(s_langCodes[i], langCode) == 0)
			return i;
	}
	return 1;
}

int32_t SigninExtAdjustLanguageForCountry(int32_t langIndex, uint32_t countryCode)
{
	if (langIndex == 7 && countryCode == COUNTRY_CODE('b', 'r'))
		return 17;
	if (langIndex == 1 &&
	    countryCode != COUNTRY_CODE('u', 's') &&
	    countryCode != COUNTRY_CODE('c', 'a') &&
	    countryCode != COUNTRY_CODE('m', 'x') &&
	    countryCode != COUNTRY_CODE('b', 'r') &&
	    countryCode != COUNTRY_CODE('c', 'l') &&
	    countryCode != COUNTRY_CODE('p', 'e') &&
	    countryCode != COUNTRY_CODE('a', 'r') &&
	    countryCode != COUNTRY_CODE('c', 'o')) {
		return 18;
	}
	return langIndex;
}

void SigninExtModelsInit(void)
{
}

void SigninExtClearAllModels(void)
{
	if (g_unusedModelSlot3 != NULL) {
		delete static_cast<ModelBase *>(g_unusedModelSlot3);
		g_unusedModelSlot3 = NULL;
	}
	if (g_passwordChangeModel != NULL) {
		delete g_passwordChangeModel;
		g_passwordChangeModel = NULL;
	}
	if (g_unusedModelSlot4 != NULL) {
		delete static_cast<ModelBase *>(g_unusedModelSlot4);
		g_unusedModelSlot4 = NULL;
	}
	if (g_dobVerifyModel != NULL) {
		delete g_dobVerifyModel;
		g_dobVerifyModel = NULL;
	}
	if (g_unusedModelSlot1 != NULL) {
		delete static_cast<ModelBase *>(g_unusedModelSlot1);
		g_unusedModelSlot1 = NULL;
	}
	if (g_credentialsModel != NULL) {
		delete g_credentialsModel;
		g_credentialsModel = NULL;
	}
	if (g_unusedModelSlot2 != NULL) {
		delete static_cast<ModelBase *>(g_unusedModelSlot2);
		g_unusedModelSlot2 = NULL;
	}
	if (g_accountInfoModel != NULL) {
		delete g_accountInfoModel;
		g_accountInfoModel = NULL;
	}
	SigninExtCountryTableClear();
}

/* ---- AccountInfoModel ---- */

AccountInfoModel::AccountInfoModel()
	: m_country(), m_language(), m_eulaDocCount(0), m_eulaDocIndex(0)
{
}

AccountInfoModel::~AccountInfoModel()
{
}

int32_t AccountInfoModel::LoadFromCommerceAccount(const SceNpCommerce2AccountInfo *info)
{
	m_country.m_selectedIndex = m_country.FindByCode(info->countryCode);
	m_language.BindCountry(&m_country);

	const char *langStr = info->languageCode;
	if (m_country.m_selectedIndex < 0) {
		SigninExtCountryTableAddFallback(info->countryCode, langStr);
		m_country.m_selectedIndex = m_country.FindByCode(info->countryCode);
		m_language.BindCountry(&m_country);
		langStr = info->languageCode;
	}

	m_language.m_selectedIndex = m_language.FindByCode(langStr);
	int32_t langId = m_language.GetLanguageId(m_language.m_selectedIndex);
	SigninExtSetLanguageId(langId);
	return 0;
}

void AccountInfoModel::SetEulaDocCount(int32_t count)
{
	if (count > 0) {
		m_eulaDocCount = count;
		m_eulaDocIndex = 1;
	} else {
		m_eulaDocCount = 0;
		m_eulaDocIndex = 0;
	}
}

void AccountInfoModel::AdvanceEulaDoc(void)
{
	++m_eulaDocIndex;
	if (m_eulaDocIndex > m_eulaDocCount)
		m_eulaDocIndex = m_eulaDocCount;
}

/* ---- SpinFieldModel / YearField / MonthField / DayField ---- */

void SpinFieldModel::SetValue(int32_t value)
{
	if (value < m_min || value > m_max)
		value = -1;
	m_value = value;
}

YearField::YearField(int32_t minYear, int32_t maxYear, int32_t value)
{
	m_min = 0;
	m_max = 0;
	m_value = -1;

	if (minYear <= 0)
		minYear = 1900;
	if (maxYear <= 0) {
		if (s_cachedCurrentYear <= 0) {
			DateTime utc = DateTime::UTC();
			s_cachedCurrentYear = utc.m_year;
		}
		maxYear = s_cachedCurrentYear;
	}
	m_min = minYear;
	m_max = maxYear;
	SetValue(value);
}

const wchar_t *YearField::FormatValue(int32_t value)
{
	if (value >= m_min && value <= m_max) {
		sce_paf_swprintf(s_yearBuf, 8, L"%d", value);
		return s_yearBuf;
	}
	return SigninExtGetString(IDParam(RCO_STR_YEAR_PLACEHOLDER));
}

MonthField::MonthField()
{
	m_min = 1;
	m_max = 12;
	m_value = -1;
	SetValue(0);
}

const wchar_t *MonthField::FormatValue(int32_t value)
{
	if (value >= m_min && value <= m_max) {
		sce_paf_swprintf(s_monthBuf, 4, L"%d", value);
		return s_monthBuf;
	}
	return SigninExtGetString(IDParam(RCO_STR_MONTH_PLACEHOLDER));
}

DayField::DayField()
{
	m_min = 1;
	m_max = 31;
	m_value = -1;
	SetValue(0);
}

const wchar_t *DayField::FormatValue(int32_t value)
{
	if (value >= m_min && value <= m_max) {
		sce_paf_swprintf(s_dayBuf, 4, L"%d", value);
		return s_dayBuf;
	}
	return SigninExtGetString(IDParam(RCO_STR_DAY_PLACEHOLDER));
}

/* ---- BirthDateModel ---- */

BirthDateModel::BirthDateModel()
	: m_dateFormat(0),
	  m_year(0, 0, 0),
	  m_month(),
	  m_day(),
	  m_status(2),
	  m_errorCode(0)
{
}

int32_t BirthDateModel::Validate(void)
{
	int32_t year = m_year.GetValue();
	int32_t month = m_month.GetValue();
	int32_t maxDay = 31;

	if (month > 0 && month <= 12) {
		if (month == 2) {
			maxDay = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;
		} else {
			maxDay = s_daysInMonth[month];
		}
	}

	m_day.m_max = maxDay;
	if (m_day.m_value > maxDay)
		m_day.m_value = maxDay;

	m_status = (m_year.m_value >= 0 && m_month.m_value >= 0 && m_day.m_value >= 0) ? 1 : 2;
	m_errorCode = 0;
	return 0;
}

int32_t BirthDateModel::SetField(int32_t fieldIndex, int32_t value)
{
	switch (fieldIndex) {
	case 0:
		m_year.SetValue(value);
		break;
	case 1:
		m_month.SetValue(value);
		break;
	case 2:
		m_day.SetValue(value);
		break;
	default:
		break;
	}
	return Validate();
}

int32_t BirthDateModel::CopyFrom(const BirthDateModel &src)
{
	m_dateFormat = src.m_dateFormat;
	m_year.m_min = src.m_year.m_min;
	m_year.m_max = src.m_year.m_max;
	m_year.m_value = src.m_year.m_value;
	m_month.m_min = src.m_month.m_min;
	m_month.m_max = src.m_month.m_max;
	m_month.m_value = src.m_month.m_value;
	m_day.m_min = src.m_day.m_min;
	m_day.m_max = src.m_day.m_max;
	m_day.m_value = src.m_day.m_value;
	return Validate();
}

int32_t BirthDateModel::SetYear(int32_t year)
{
	m_year.SetValue(year);
	return Validate();
}

int32_t BirthDateModel::SetMonth(int32_t month)
{
	m_month.SetValue(month);
	return Validate();
}

int32_t BirthDateModel::SetDay(int32_t day)
{
	m_day.SetValue(day);
	return Validate();
}

wstring BirthDateModel::FormatDisplayString(void)
{
	const wchar_t *p1;
	const wchar_t *p2;
	const wchar_t *p3;

	if (m_dateFormat == 0) {
		p1 = m_year.FormatValue(m_year.m_value);
		p2 = m_month.FormatValue(m_month.m_value);
		p3 = m_day.FormatValue(m_day.m_value);
	} else if (m_dateFormat == 1) {
		p1 = m_month.FormatValue(m_month.m_value);
		p2 = m_day.FormatValue(m_day.m_value);
		p3 = m_year.FormatValue(m_year.m_value);
	} else {
		p1 = m_day.FormatValue(m_day.m_value);
		p2 = m_month.FormatValue(m_month.m_value);
		p3 = m_year.FormatValue(m_year.m_value);
	}

	wchar_t buf[16];
	sce_paf_swprintf(buf, 16, L"%s / %s / %s", p1, p2, p3);
	return wstring(buf);
}

/* ---- LanguageModel ---- */

LanguageModel::LanguageModel()
	: m_countryEntry(NULL)
{
	m_selectedIndex = -1;
}

void LanguageModel::BindCountry(const CountryModel *country)
{
	m_countryEntry = SigninExtCountryTableGet(country->m_selectedIndex);
	SetSelectedIndex(0);
}

int32_t LanguageModel::GetCount(void)
{
	if (m_countryEntry != NULL)
		return (int32_t)m_countryEntry->languages.size();
	return 0;
}

int32_t LanguageModel::GetLanguageId(int32_t index)
{
	if (index >= 0 && index < GetCount())
		return m_countryEntry->languages[index];
	return -1;
}

const wchar_t *LanguageModel::GetLabel(int32_t index)
{
	int32_t langId = GetLanguageId(index);
	if (langId == -1)
		return s_emptyWideStr;
	return SigninExtGetString(IDParam(s_langResIds[langId]));
}

const char *LanguageModel::GetLanguageCode(int32_t index)
{
	int32_t langId = GetLanguageId(index);
	if (langId == -1)
		return NULL;
	return s_langCodes[langId];
}

int32_t LanguageModel::FindByCode(const char *langCode)
{
	if (langCode == NULL || m_countryEntry == NULL)
		return -1;

	int32_t count = GetCount();
	for (int32_t i = 0; i < count; ++i) {
		int32_t langId = m_countryEntry->languages[i];
		if (langId >= 20)
			langId = 1;
		if (sce_paf_strcmp(s_langCodes[langId], langCode) == 0)
			return i;
	}
	return -1;
}

/* ---- PasswordField / ConfirmPasswordField ---- */

PasswordField::PasswordField()
	: TextInputField(30, 7), m_status(2), m_errorCode(0), m_ruleFlags(0)
{
}

void PasswordField::Validate(void)
{
	m_status = 2;
	m_errorCode = 0;
	m_ruleFlags = 0;

	if (m_length == 0)
		return;

	uint32_t policyFlags = 0;
	int32_t ret = SceNpCommerce2_34CDA223(40, m_buf, &policyFlags);
	uint8_t flags = (ret < 0) ? 0x80 : (uint8_t)policyFlags;

	if ((flags & 0x02) != 0)
		m_ruleFlags |= 0x01;
	if ((flags & 0x0C) != 0)
		m_ruleFlags |= 0x02;
	if ((flags & 0x10) != 0)
		m_ruleFlags |= 0x04;
	if ((flags & 0x20) != 0)
		m_ruleFlags |= 0x08;
	if ((flags & 0x80) != 0)
		m_ruleFlags |= 0x10;

	if (m_ruleFlags == 0x10) {
		m_errorCode = SIGNIN_EXT_ERROR_PW_INVALID_CHAR;
	} else if (m_ruleFlags != 0) {
		m_errorCode = SIGNIN_EXT_ERROR_PW_POLICY;
	} else {
		m_status = 1;
	}
}

void PasswordField::Clear(void)
{
	TextInputField::Clear();
	Validate();
}

void PasswordField::MarkConflict(uint32_t conflictBit)
{
	m_status = 2;
	m_errorCode = SIGNIN_EXT_ERROR_PW_POLICY;
	m_ruleFlags |= conflictBit;
	if (m_ruleFlags == 0x80)
		m_errorCode = SIGNIN_EXT_ERROR_PW_SAME_AS_OLD;
}

void PasswordField::SetServerResult(int32_t err)
{
	m_status = (err < 0) ? 1 : 0;
	if (err == SIGNIN_EXT_ERROR_COMMERCE_PW_WEAK ||
	    err == SIGNIN_EXT_ERROR_COMMERCE_BAD_REQUEST ||
	    err == SIGNIN_EXT_ERROR_PW_SAME_AS_OLD) {
		m_errorCode = err;
	} else {
		m_errorCode = 0;
	}
}

ConfirmPasswordField::ConfirmPasswordField()
	: TextInputField(30, 7), m_status(1), m_errorCode(0)
{
}

void ConfirmPasswordField::Clear(void)
{
	TextInputField::Clear();
	m_status = 1;
	m_errorCode = 0;
}

int32_t ConfirmPasswordField::CheckMatch(const PasswordField &pw)
{
	m_status = 1;
	m_errorCode = 0;
	if (m_length == 0 || pw.m_length == 0)
		return 0;

	if (Equals(pw)) {
		m_status = 0;
		return 0;
	}
	m_errorCode = SIGNIN_EXT_ERROR_PW_MISMATCH;
	return SIGNIN_EXT_ERROR_PW_MISMATCH;
}

/* ---- PasswordChangeModel ---- */

PasswordChangeModel::PasswordChangeModel()
	: m_signInId(63, 6),
	  m_currentPassword(30, 7),
	  m_onlineId(16, 4),
	  m_accountId(0),
	  m_countryCode(0),
	  m_newPassword(),
	  m_confirmPassword()
{
}

PasswordChangeModel::~PasswordChangeModel()
{
}

void PasswordChangeModel::Reset(bool clearCredentials)
{
	if (clearCredentials) {
		m_signInId.Clear();
		m_currentPassword.Clear();
	}
}

int32_t PasswordChangeModel::LoadFromCommerceAccount(const SceNpCommerce2AccountInfo *info)
{
	m_signInId.SetUtf8(info->signInId);
	m_onlineId.SetUtf8(info->onlineId);
	m_accountId = info->accountId;
	m_countryCode = 0;
	if (info->countryCode != NULL) {
		m_countryCode = ((uint32_t)(uint8_t)info->countryCode[0] | 0x20U) |
				(((uint32_t)(uint8_t)info->countryCode[1] | 0x20U) << 8);
	}

	int32_t langId = SigninExtFindLanguageIndex(info->languageCode);
	langId = SigninExtAdjustLanguageForCountry(langId, m_countryCode);
	SigninExtSetLanguageId(langId);
	return 0;
}

int32_t PasswordChangeModel::SetNewPassword(const wchar_t *pw)
{
	m_newPassword.SetWide(pw);
	m_newPassword.Validate();
	m_confirmPassword.CheckMatch(m_newPassword);
	if (m_newPassword.Equals(m_signInId))
		m_newPassword.MarkConflict(0x20);
	if (m_newPassword.Equals(m_onlineId))
		m_newPassword.MarkConflict(0x40);
	if (m_newPassword.Equals(m_currentPassword))
		m_newPassword.MarkConflict(0x80);
	return m_newPassword.m_errorCode;
}

int32_t PasswordChangeModel::SetConfirmPassword(const wchar_t *pw)
{
	m_confirmPassword.SetWide(pw);
	return m_confirmPassword.CheckMatch(m_newPassword);
}

void PasswordChangeModel::ClearNewPassword(void)
{
	m_newPassword.Clear();
	m_confirmPassword.CheckMatch(m_newPassword);
	if (m_newPassword.Equals(m_signInId))
		m_newPassword.MarkConflict(0x20);
	if (m_newPassword.Equals(m_onlineId))
		m_newPassword.MarkConflict(0x40);
	if (m_newPassword.Equals(m_currentPassword))
		m_newPassword.MarkConflict(0x80);
}

void PasswordChangeModel::ClearConfirmPassword(void)
{
	m_confirmPassword.Clear();
	m_confirmPassword.CheckMatch(m_newPassword);
}

/* ---- DobVerifyModel ---- */

DobVerifyModel::DobVerifyModel()
	: m_signInId(63, 6), m_birthDate()
{
}

DobVerifyModel::~DobVerifyModel()
{
}

void DobVerifyModel::Reset(bool clearSignInId)
{
	if (clearSignInId)
		m_signInId.Clear();
}

/* ---- ExternalStringField / SignInCredentialsModel ---- */

ExternalStringField::ExternalStringField(int32_t maxLen)
	: m_buf(NULL), m_maxLen(maxLen), m_length(0)
{
}

ExternalStringField::~ExternalStringField()
{
}

void ExternalStringField::AttachBuffer(wchar_t *buf)
{
	m_buf = buf;
	m_length = (int32_t)sce_paf_wcslen(buf);
}

void ExternalStringField::ToUtf8(string *out) const
{
	Utf16ToUtf8(m_buf, out);
}

void ExternalStringField::SetString(const wchar_t *src)
{
	if (src != NULL) {
		sce_paf_wcsncpy(m_buf, src, m_maxLen);
		m_length = (int32_t)sce_paf_wcslen(m_buf);
	} else {
		m_buf[0] = L'\0';
		m_length = 0;
	}
}

SignInCredentialsModel::SignInCredentialsModel()
	: m_id(63), m_password(30)
{
}

SignInCredentialsModel::~SignInCredentialsModel()
{
}
