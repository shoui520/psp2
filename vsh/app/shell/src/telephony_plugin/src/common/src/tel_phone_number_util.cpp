/*
 * tel_phone_number_util.cpp: TelPhoneNumber, TelPinCode, TelPukCode,
 * TelControlKey, TelUnblockPinInfo, and TelCarrierLockKey
 * (3.65-CEX: 0x813E24B4..0x813E258F, 0x813E5D3C..0x813E61DB).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/ctype.h>
#include <std/string.h>
#include "../tel_common.h"

namespace telephony {

/* TelPhoneNumber (0x813E24B4..0x813E258F) */
TelPhoneNumber::TelPhoneNumber()
{
	sce_paf_memset(m_digits, 0, sizeof(m_digits));
}

TelPhoneNumber::TelPhoneNumber(const char *digits)
{
	sce_paf_memset(m_digits, 0, sizeof(m_digits));
	sce_paf_strncpy(m_digits, digits, 32);
}

paf::common::SharedPtr<TelObject> TelPhoneNumber::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelPhoneNumber(m_digits));
}

void TelPhoneNumber::SetDigits(const char *digits)
{
	sce_paf_memset(m_digits, 0, sizeof(m_digits));
	sce_paf_strncpy(m_digits, digits, 32);
}

/* 0x813E2520 */
bool TelPhoneNumber::IsValid() const
{
	return ValidateDigits(m_digits);
}

/* 0x813E252A */
bool TelPhoneNumber::IsEmpty() const
{
	return sce_paf_strncmp(m_digits, "", 32) == 0;
}

/* 0x813E2548 */
bool TelPhoneNumber::ValidateDigits(const char *digits)
{
	if (digits == NULL || *digits == '\0')
		return false;
	if (*digits == '+')
		++digits;
	if (*digits == '\0')
		return false;
	while (*digits != '\0') {
		char ch = *digits;
		bool isNumeric = (ch >= '0' && ch <= '9');
		bool isDtmf = (ch >= 'a' && ch <= 'c') || ch == '#' || ch == '*';
		if (!isNumeric && !isDtmf)
			return false;
		++digits;
	}
	return true;
}

static bool isAsciiDigit(char ch)
{
	return ch >= '0' && ch <= '9';
}

/* 0x813E5E02: PIN code is 4..8 ASCII digits */
bool TelPinCode::IsValid() const
{
	SceSize len = m_code.length();
	if (len < 4U || len > 8U)
		return false;
	for (SceSize idx = 0; idx < len; ++idx) {
		if (!isAsciiDigit(m_code.c_str()[idx]))
			return false;
	}
	return true;
}

/* 0x813E5F06: PUK code is exactly 8 ASCII digits */
bool TelPukCode::IsValid() const
{
	if (m_code.length() != 8U)
		return false;
	for (SceSize idx = 0; idx < 8U; ++idx) {
		if (!isAsciiDigit(m_code.c_str()[idx]))
			return false;
	}
	return true;
}

/* 0x813E6002: SIM control key is 6..16 ASCII digits */
bool TelControlKey::IsValid() const
{
	SceSize len = m_key.length();
	if (len < 6U || len > 16U)
		return false;
	for (SceSize idx = 0; idx < len; ++idx) {
		if (!isAsciiDigit(m_key.c_str()[idx]))
			return false;
	}
	return true;
}

/* 0x813E60EC */
bool TelUnblockPinInfo::IsValid() const
{
	return m_puk.IsValid() && m_newPin.IsValid();
}

/* 0x813E6114..0x813E61DA (shares 6..16 digit check at 0x813E6002) */
bool TelCarrierLockKey::IsValid() const
{
	SceSize len = m_key.length();
	if (len < 6U || len > 16U)
		return false;
	for (SceSize idx = 0; idx < len; ++idx) {
		if (!isAsciiDigit(m_key.c_str()[idx]))
			return false;
	}
	return true;
}

} /* namespace telephony */
