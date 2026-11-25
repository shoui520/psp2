/*
 * tel_sms_pdu_parser.cpp: TelSmsPduParser converting GSM/WCDMA or CDMA
 * incoming SMS PDUs into TelSmsMessage
 * (3.65-CEX: 0x813F3058..0x813F3AC3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/encoded_string.h>
#include "../../tel_sms_pdu_transcoder.h"

namespace telephony {

namespace {

static int mapUdhNationalLang(int wmsLangId)
{
	if (wmsLangId == 2)
		return TEL_GSM7_LANG_SPANISH;
	if (wmsLangId == 3)
		return TEL_GSM7_LANG_PORTUGUESE;
	return TEL_GSM7_LANG_DEFAULT;
}

} /* namespace */

/* 0x813F3058 */
TelSmsPduParser::TelSmsPduParser()
	: m_wmsFormat(WMS_FORMAT_GW_PP)
{
}

/* 0x813F3A8C */
TelephonyResult TelSmsPduParser::Initialize(int mode)
{
	if (mode == TEL_SMS_TRANSCODER_GSM) {
		m_wmsFormat = WMS_FORMAT_GW_PP;
		return TelephonyResult(TEL_OK);
	}
	if (mode == TEL_SMS_TRANSCODER_CDMA) {
		m_wmsFormat = WMS_FORMAT_CDMA;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_ERR_FAILED);
}

/* 0x813F3068: Parse decoded CDMA Bearer Data Deliver into TelSmsMessage */
TelephonyResult TelSmsPduParser::ParseCdmaDeliver(const WmsClientTsData &tsData,
						  TelSmsMessage *outMessage) const
{
	if (tsData.format != WMS_FORMAT_CDMA)
		return TelephonyResult(TEL_ERR_FAILED);

	const WmsCdmaBearerData &cdma = tsData.u.cdma;
	if (cdma.messageId.type != 1 || cdma.messageId.udhPresent)
		return TelephonyResult(TEL_ERR_FAILED);

	if (cdma.userData.encoding != WMS_CDMA_ENC_UNICODE || cdma.userData.dataLength > 140U)
		return TelephonyResult(TEL_ERR_FAILED);

	paf::common::EncodedString bodyEncoded;
	ScePafMisc_E13019F7(&bodyEncoded, (const char *)cdma.userData.bytes,
			    paf::common::EncodedString::Encoding_Utf16);
	const wchar_t *wideText = ScePafMisc_53948F03(&bodyEncoded);
	outMessage->SetBody(paf::wstring(wideText));
	ScePafMisc_B45A7521(&bodyEncoded);
	return TelephonyResult(TEL_OK);
}

/* 0x813F3752: Validate decoded GSM TPDU is SMS-DELIVER */
TelephonyResult TelSmsPduParser::ValidateGsmDeliver(const WmsClientTsData &tsData) const
{
	if (tsData.format != WMS_FORMAT_GW_PP)
		return TelephonyResult(TEL_ERR_FAILED);
	if (tsData.u.gwPp.tpduType != WMS_TPDU_DELIVER)
		return TelephonyResult(TEL_ERR_FAILED);
	return TelephonyResult(TEL_OK);
}

/* 0x813F3584: Extract GSM SMS-DELIVER originating address into TelSmsMessage peer */
TelephonyResult TelSmsPduParser::ParseGsmAddress(const WmsClientTsData &tsData,
						 TelSmsMessage *outMessage) const
{
	const WmsAddress &origAddr = tsData.u.gwPp.u.deliver.address;
	if (origAddr.digitMode != WMS_DIGIT_MODE_4_BIT &&
	    origAddr.digitMode != WMS_DIGIT_MODE_8_BIT)
		return TelephonyResult(TEL_ERR_FAILED);

	if (origAddr.numberType < 0 || origAddr.numberType > 7)
		return TelephonyResult(TEL_ERR_FAILED);

	if (origAddr.numberType == WMS_NUMBER_ALPHANUMERIC) {
		if (origAddr.digitCount > 32U)
			return TelephonyResult(TEL_ERR_FAILED);

		wchar_t wideLabel[33];
		sce_paf_memset(wideLabel, 0, sizeof(wideLabel));
		TelSmsMessageHelper::DecodeGsm7ToUtf16(origAddr.digits, origAddr.digitCount,
						       TEL_GSM7_LANG_DEFAULT,
						       TEL_GSM7_LANG_DEFAULT, wideLabel);

		paf::common::EncodedString labelEncoded;
		ScePafMisc_DCBA2BD4(&labelEncoded, wideLabel,
				    paf::common::EncodedString::Encoding_Utf16);
		const char *utf8Label = ScePafMisc_A7C6AF5C(&labelEncoded);
		TelPhoneNumber phone(utf8Label);
		TelSmsAddress smsAddr(phone, "");
		TelSmsPeerInfo peer(smsAddr, 0, false, 0, 4);
		outMessage->SetSinglePeer(peer);
		ScePafMisc_B45A7521(&labelEncoded);
		return TelephonyResult(TEL_OK);
	}

	if (origAddr.digitCount == 0 || origAddr.digitCount > 32U)
		return TelephonyResult(TEL_ERR_FAILED);

	char asciiDigits[33];
	sce_paf_memset(asciiDigits, 0, sizeof(asciiDigits));
	if (!wmsTsBcdToAscii(origAddr.digits, origAddr.digitCount, asciiDigits))
		return TelephonyResult(TEL_ERR_FAILED);

	TelPhoneNumber phone(asciiDigits);
	TelSmsAddress smsAddr(phone, "");
	TelSmsPeerInfo peer(smsAddr, 0, false, 0, 4);
	outMessage->SetSinglePeer(peer);
	return TelephonyResult(TEL_OK);
}

/* 0x813F318C: Parse GSM SMS-DELIVER body, UDH concatenation, SCTS timestamp, and message class */
TelephonyResult TelSmsPduParser::ParseGsmDeliver(const WmsClientTsData &tsData,
						 TelSmsMessage *outMessage) const
{
	const WmsGwDeliver &deliver = tsData.u.gwPp.u.deliver;
	if (deliver.dcs.isCompressed)
		return TelephonyResult(TEL_ERR_FAILED);

	TelephonyResult res(TEL_OK);
	if (deliver.dcs.alphabet == WMS_ALPHABET_UCS2) {
		if (deliver.userData.dataLength > 140U)
			return TelephonyResult(TEL_ERR_FAILED);

		wchar_t wideBuf[71];
		sce_paf_memset(wideBuf, 0, sizeof(wideBuf));
		for (SceUInt16 idx = 0; idx + 1U < deliver.userData.dataLength;
		     idx = (SceUInt16)(idx + 2U)) {
			wideBuf[idx / 2U] =
				(wchar_t)(((SceUInt16)deliver.userData.bytes[idx] << 8) |
					  deliver.userData.bytes[idx + 1U]);
		}
		outMessage->SetBody(paf::wstring(wideBuf));
	} else if (deliver.dcs.alphabet == WMS_ALPHABET_DEFAULT_7BIT) {
		int lockingLang = TEL_GSM7_LANG_DEFAULT;
		int singleLang = TEL_GSM7_LANG_DEFAULT;
		if (deliver.userDataHeaderPresent) {
			for (SceUInt8 idx = 0; idx < deliver.userData.headerCount; ++idx) {
				const WmsUdhHeader &hdr = deliver.userData.headers[idx];
				if (hdr.headerId == WMS_UDH_NAT_LANG_LOCKING_SHIFT)
					lockingLang =
						mapUdhNationalLang(hdr.u.natLangShiftTable);
				else if (hdr.headerId == WMS_UDH_NAT_LANG_SINGLE_SHIFT)
					singleLang = mapUdhNationalLang(hdr.u.natLangShiftTable);
			}
		}

		if (deliver.userData.dataLength > 160U)
			return TelephonyResult(TEL_ERR_FAILED);

		wchar_t wideBuf[161];
		sce_paf_memset(wideBuf, 0, sizeof(wideBuf));
		TelSmsMessageHelper::DecodeGsm7ToUtf16(deliver.userData.bytes,
						       deliver.userData.dataLength, lockingLang,
						       singleLang, wideBuf);
		outMessage->SetBody(paf::wstring(wideBuf));
	} else {
		res = TelephonyResult(TEL_ERR_FAILED);
	}

	/* Populate concatenation metadata via TelSmsMessageBuilder */
	TelSmsMessageBuilder builder(outMessage);
	builder.SetConcatenated(false);
	if (deliver.userDataHeaderPresent) {
		for (SceUInt8 idx = 0; idx < deliver.userData.headerCount; ++idx) {
			const WmsUdhHeader &hdr = deliver.userData.headers[idx];
			if (hdr.headerId == WMS_UDH_CONCAT_8) {
				builder.SetConcatenated(true);
				builder.SetReferenceNumber(hdr.u.concat8.referenceNumber);
				builder.SetProtocolId(hdr.u.concat8.sequenceNumber);
				builder.MarkComplete();
			} else if (hdr.headerId == WMS_UDH_CONCAT_16) {
				builder.SetConcatenated(true);
				builder.SetReferenceNumber(hdr.u.concat16.referenceNumber);
				builder.SetProtocolId(hdr.u.concat16.sequenceNumber);
				builder.MarkComplete();
			}
		}
	}

	/* Convert BCD SCTS timestamp into TelDateTime and apply quarter-hour TZ offset */
	const WmsTimestamp &ts = deliver.timestamp;
	SceUInt16 year = (SceUInt16)((ts.year & 0x0FU) + 10U * (ts.year >> 4) + 2000U);
	SceUInt16 month = (SceUInt16)((ts.month & 0x0FU) + 10U * (ts.month >> 4));
	SceUInt16 day = (SceUInt16)((ts.day & 0x0FU) + 10U * (ts.day >> 4));
	SceUInt16 hour = (SceUInt16)((ts.hour & 0x0FU) + 10U * (ts.hour >> 4));
	SceUInt16 minute = (SceUInt16)((ts.minute & 0x0FU) + 10U * (ts.minute >> 4));
	SceUInt16 second = (SceUInt16)((ts.second & 0x0FU) + 10U * (ts.second >> 4));

	TelDateTime deliverTime(year, month, day, hour, minute, second, 0);
	deliverTime.ApplyQuarterHourTimezone(ts.timezone);
	outMessage->SetDeliverTime(deliverTime);

	/* Store message class on the peer entry */
	TelSmsPeerInfo peerCopy = outMessage->GetFirstPeer();
	int msgClass = deliver.dcs.messageClass;
	if (msgClass < 0 || msgClass > 3)
		msgClass = 4;
	peerCopy.SetDeliveryResult(msgClass);
	outMessage->SetSinglePeer(peerCopy);

	return res;
}

/* 0x813F3796: Strip SMSC header and extract raw GSM TPDU */
TelephonyResult TelSmsPduParser::ExtractGsmTpdu(const TelSmsProtocolDataUnit &pdu,
						WmsRawTsData *outRawData) const
{
	SceUInt8 rawBytes[WMS_MAX_PDU_BYTES + 1];
	sce_paf_memset(rawBytes, 0, sizeof(rawBytes));
	SceSize pduLen = pdu.GetLength();
	pdu.GetBytes(rawBytes, pduLen);

	WmsAddress scAddr;
	sce_paf_memset(&scAddr, 0, sizeof(scAddr));
	int scLen = wmsTsDecodeScAddress(rawBytes, &scAddr);
	if (scLen == 0 || (SceSize)scLen >= pduLen)
		return TelephonyResult(TEL_ERR_FAILED);

	SceSize tpduLen = pduLen - (SceSize)scLen;
	sce_paf_memcpy(outRawData->bytes, &rawBytes[scLen], tpduLen);
	outRawData->format = m_wmsFormat;
	outRawData->length = tpduLen;

	if ((outRawData->bytes[0] & 0x03U) != 0U)
		return TelephonyResult(TEL_ERR_FAILED);

	outRawData->tpduType = WMS_TPDU_DELIVER;
	return TelephonyResult(TEL_OK);
}

/* 0x813F3876 */
TelephonyResult TelSmsPduParser::Parse(const TelSmsProtocolDataUnit &pdu,
				       TelSmsMessage *outMessage)
{
	WmsRawTsData rawData;
	sce_paf_memset(&rawData, 0, sizeof(rawData));
	TelephonyResult res(TEL_OK);

	if (m_wmsFormat == WMS_FORMAT_GW_PP) {
		res = ExtractGsmTpdu(pdu, &rawData);
	} else if (m_wmsFormat == WMS_FORMAT_CDMA) {
		SceSize pduLen = pdu.GetLength();
		pdu.GetBytes(rawData.bytes, pduLen);
		rawData.format = m_wmsFormat;
		rawData.length = pduLen;
	} else {
		res = TelephonyResult(TEL_ERR_FAILED);
	}

	if (!res.IsSuccess())
		return res;

	WmsClientTsData decoded;
	sce_paf_memset(&decoded, 0, sizeof(decoded));
	if (wmsTsDecode(&rawData, &decoded) != WMS_OK)
		return TelephonyResult(TEL_ERR_FAILED);

	if (m_wmsFormat == WMS_FORMAT_CDMA)
		return ParseCdmaDeliver(decoded, outMessage);

	if (m_wmsFormat == WMS_FORMAT_GW_PP) {
		res = ValidateGsmDeliver(decoded);
		if (res.IsSuccess())
			res = ParseGsmAddress(decoded, outMessage);
		if (res.IsSuccess())
			res = ParseGsmDeliver(decoded, outMessage);
		return res;
	}
	return TelephonyResult(TEL_ERR_FAILED);
}

} /* namespace telephony */
