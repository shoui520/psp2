/*
 * tel_sms_protocol_data_unit.cpp: TelSmsProtocolDataUnit raw SMS PDU container
 * (3.65-CEX: 0x813E5CBC..0x813E5D3B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "../../tel_common.h"

namespace telephony {

/* 0x813E5CBC */
TelSmsProtocolDataUnit::TelSmsProtocolDataUnit(const TelSmsProtocolDataUnit &src)
	: m_length(src.m_length)
{
	sce_paf_memcpy(m_bytes, src.m_bytes, src.m_length);
}

TelSmsProtocolDataUnit &TelSmsProtocolDataUnit::operator=(const TelSmsProtocolDataUnit &src)
{
	if (this != &src) {
		m_length = src.m_length;
		sce_paf_memcpy(m_bytes, src.m_bytes, src.m_length);
	}
	return *this;
}

/* 0x813E5CD0 */
TelephonyResult TelSmsProtocolDataUnit::SetBytes(const void *bytes, SceSize length)
{
	if (length > sizeof(m_bytes))
		return TelephonyResult(TEL_ERR_BUFFER_TOO_SMALL);
	m_length = length;
	sce_paf_memcpy(m_bytes, bytes, length);
	return TelephonyResult(TEL_OK);
}

/* 0x813E5D06 */
TelephonyResult TelSmsProtocolDataUnit::GetBytes(void *outBuf, SceSize maxLength) const
{
	if (m_length > maxLength)
		return TelephonyResult(TEL_ERR_BUFFER_TOO_SMALL);
	sce_paf_memcpy(outBuf, m_bytes, m_length);
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
