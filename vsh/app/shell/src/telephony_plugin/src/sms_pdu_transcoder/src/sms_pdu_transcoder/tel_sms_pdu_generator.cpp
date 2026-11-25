/*
 * tel_sms_pdu_generator.cpp: TelSmsPduGenerator converting TelSmsMessage to
 * GSM/WCDMA (3GPP TS 23.040) or CDMA (3GPP2 C.S0015) PDUs
 * (3.65-CEX: 0x813F2AA0..0x813F3057).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <common/encoded_string.h>
#include "../../tel_sms_pdu_transcoder.h"

namespace telephony {

/* 0x813F2AA0 */
TelSmsPduGenerator::TelSmsPduGenerator()
	: m_wmsFormat(WMS_FORMAT_GW_PP), m_cdmaMessageId(0)
{
}

/* 0x813F3000 */
TelephonyResult TelSmsPduGenerator::Initialize(int mode)
{
	if (mode == TEL_SMS_TRANSCODER_GSM) {
		m_wmsFormat = WMS_FORMAT_GW_PP;
		return TelephonyResult(TEL_OK);
	}
	if (mode == TEL_SMS_TRANSCODER_CDMA) {
		m_wmsFormat = WMS_FORMAT_CDMA;
		m_cdmaMessageId = (SceUInt16)(sce_paf_rand() % 65535);
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_ERR_FAILED);
}

/* 0x813F2AB0: Build CDMA Bearer Data SMS-SUBMIT */
TelephonyResult TelSmsPduGenerator::BuildCdmaSubmit(const TelSmsMessage &message,
						    WmsClientTsData *outTsData)
{
	outTsData->format = m_wmsFormat;
	WmsCdmaBearerData &cdma = outTsData->u.cdma;
	cdma.mask = 0x2001U; /* MessageId | Language */
	cdma.messageId.type = 2; /* Submit */
	cdma.messageId.idNumber = m_cdmaMessageId;
	cdma.messageId.udhPresent = false;
	cdma.language = 4; /* Japanese / default */

	if (m_cdmaMessageId == 0xFFFFU)
		m_cdmaMessageId = 0;
	else
		++m_cdmaMessageId;

	paf::common::EncodedString bodyEncoded;
	ScePafMisc_7709D1B2(&bodyEncoded, message.GetBody(),
			    paf::common::EncodedString::Encoding_Utf16);
	SceSize charCount = ScePafMisc_183DD5C3(&bodyEncoded);
	SceSize byteLen = charCount * 2U;

	TelephonyResult res(TEL_OK);
	if (byteLen > WMS_MAX_CDMA_USER_DATA) {
		res = TelephonyResult(TEL_ERR_FAILED);
	} else {
		cdma.userData.dataLength = (SceUInt8)byteLen;
		sce_paf_memset(cdma.userData.bytes, 0, WMS_MAX_CDMA_USER_DATA);
		sce_paf_memcpy(cdma.userData.bytes, ScePafMisc_53948F03(&bodyEncoded), byteLen);
		cdma.userData.encoding = WMS_CDMA_ENC_UNICODE;
		cdma.userData.numCharacters = (SceUInt8)charCount;
		cdma.userData.paddingBits = 0;
		cdma.mask |= 0x0002U;
	}
	ScePafMisc_B45A7521(&bodyEncoded);
	return res;
}

/* 0x813F2D56: Populate GSM SMS-SUBMIT destination address from message's first peer */
TelephonyResult TelSmsPduGenerator::BuildGsmAddress(const TelSmsMessage &message,
						    WmsClientTsData *outTsData)
{
	const TelPhoneNumber &peerNum = message.GetFirstPeer().GetAddress().GetNumber();

	paf::common::EncodedString addrEncoded;
	ScePafMisc_C11CB5E4(&addrEncoded, peerNum.GetDigits(),
			    paf::common::EncodedString::Encoding_Utf8);
	const char *digits = ScePafMisc_A7C6AF5C(&addrEncoded);
	SceSize digitLen = sce_paf_strlen(digits);

	TelephonyResult res(TEL_OK);
	if (!wmsTsIsValidAsciiDigits(digits, (int)digitLen) ||
	    digitLen > WMS_MAX_ADDRESS_DIGITS) {
		res = TelephonyResult(TEL_ERR_FAILED);
	} else {
		WmsAddress &dstAddr = outTsData->u.gwPp.u.submit.address;
		dstAddr.digitMode = WMS_DIGIT_MODE_4_BIT;
		dstAddr.numberType = WMS_NUMBER_UNKNOWN;
		dstAddr.numberPlan = 1;
		dstAddr.digitCount = (SceUInt8)digitLen;
		sce_paf_memset(dstAddr.digits, 0, WMS_MAX_ADDRESS_DIGITS);
		sce_paf_memcpy(dstAddr.digits, digits, digitLen);
	}
	ScePafMisc_B45A7521(&addrEncoded);
	return res;
}

/* 0x813F2BBA: Populate GSM SMS-SUBMIT DCS and user data (UCS-2 BE or GSM-7) */
TelephonyResult TelSmsPduGenerator::BuildGsmUserData(const TelSmsMessage &message,
						     WmsClientTsData *outTsData)
{
	WmsGwSubmit &submit = outTsData->u.gwPp.u.submit;
	submit.userDataHeaderPresent = false;
	submit.userData.headerCount = 0;
	submit.dcs.messageClass = 4;
	submit.dcs.isCompressed = false;
	submit.dcs.alphabet = WMS_ALPHABET_UCS2;
	submit.dcs.msgWaitingKind = WMS_MW_NONE;
	submit.dcs.msgWaitingActive = false;
	submit.dcs.msgWaitingType = 0;
	submit.dcs.rawDcs = 0;
	sce_paf_memset(submit.userData.bytes, 0, WMS_MAX_GW_USER_DATA);

	paf::common::EncodedString bodyEncoded;
	ScePafMisc_7709D1B2(&bodyEncoded, message.GetBody(),
			    paf::common::EncodedString::Encoding_Utf16);

	TelephonyResult res(TEL_OK);
	if (message.NeedsUcs2Encoding()) {
		SceSize charCount = ScePafMisc_183DD5C3(&bodyEncoded);
		if (charCount * 2U > WMS_MAX_GW_USER_DATA) {
			res = TelephonyResult(TEL_ERR_FAILED);
		} else {
			submit.userData.dataLength = (SceUInt16)(charCount * 2U);
			const wchar_t *wideChars = ScePafMisc_53948F03(&bodyEncoded);
			for (SceSize idx = 0; idx < charCount; ++idx) {
				SceUInt16 ch = (SceUInt16)wideChars[idx];
				submit.userData.bytes[idx * 2U] = (SceUInt8)(ch >> 8);
				submit.userData.bytes[idx * 2U + 1U] = (SceUInt8)(ch & 0xFFU);
			}
		}
	} else {
		submit.dcs.alphabet = WMS_ALPHABET_DEFAULT_7BIT;
		SceUInt16 septetLen =
			TelSmsMessageHelper::GetGsm7EncodedLength(message.GetBody(),
								  TEL_GSM7_LANG_DEFAULT);
		if (septetLen > WMS_MAX_GW_USER_DATA) {
			res = TelephonyResult(TEL_ERR_FAILED);
		} else {
			submit.userData.dataLength = septetLen;
			TelSmsMessageHelper::EncodeUtf16ToGsm7(message.GetBody(),
							       TEL_GSM7_LANG_DEFAULT,
							       submit.userData.bytes);
		}
	}

	ScePafMisc_B45A7521(&bodyEncoded);
	return res;
}

/* 0x813F2E8A */
TelephonyResult TelSmsPduGenerator::Generate(const TelSmsMessage &message,
					     TelSmsProtocolDataUnit *outPdu)
{
	WmsClientTsData tsData;
	sce_paf_memset(&tsData, 0, sizeof(tsData));
	TelephonyResult res(TEL_OK);

	if (m_wmsFormat == WMS_FORMAT_GW_PP) {
		tsData.format = WMS_FORMAT_GW_PP;
		tsData.u.gwPp.tpduType = WMS_TPDU_SUBMIT;
		res = BuildGsmAddress(message, &tsData);
		tsData.u.gwPp.u.submit.messageReference = 0;
		tsData.u.gwPp.u.submit.protocolId = 0;
		tsData.u.gwPp.u.submit.rejectDuplicates = false;
		tsData.u.gwPp.u.submit.replyPath = false;
		tsData.u.gwPp.u.submit.statusReportRequest = false;
		tsData.u.gwPp.u.submit.validity.format = WMS_VALIDITY_NONE;
		res = BuildGsmUserData(message, &tsData);
	} else if (m_wmsFormat == WMS_FORMAT_CDMA) {
		res = BuildCdmaSubmit(message, &tsData);
	} else {
		res = TelephonyResult(TEL_ERR_FAILED);
	}

	WmsRawTsData rawData;
	sce_paf_memset(&rawData, 0, sizeof(rawData));
	if (wmsTsEncode(&tsData, &rawData) != WMS_OK || rawData.length + 1U > WMS_MAX_PDU_BYTES)
		return TelephonyResult(TEL_ERR_FAILED);

	SceUInt8 pduBuf[WMS_MAX_PDU_BYTES + 1];
	sce_paf_memset(pduBuf, 0, sizeof(pduBuf));
	pduBuf[0] = 0; /* Empty SMSC length prefix */
	sce_paf_memcpy(&pduBuf[1], rawData.bytes, rawData.length);
	outPdu->SetBytes(pduBuf, rawData.length + 1U);
	return res;
}

} /* namespace telephony */
