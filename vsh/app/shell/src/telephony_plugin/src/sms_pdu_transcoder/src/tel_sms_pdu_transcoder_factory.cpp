/*
 * tel_sms_pdu_transcoder_factory.cpp: factories for TelSmsPduGenerator and
 * TelSmsPduParser, plus loopback Submit->Deliver PDU converter
 * (3.65-CEX: 0x813ED8B8..0x813EDD60).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/encoded_string.h>
#include <datetime/datetime.h>
#include <memory/heap_allocator.h>
#include "../tel_sms_pdu_transcoder.h"

namespace telephony {

namespace {

/* 0x813EDD60: Fill WmsTimestamp with current UTC time (BCD) and fixed TZ +36 (+9h) */
static void fillLoopbackUtcTimestamp(WmsTimestamp *outTs)
{
	paf::datetime::DateTime utc = paf::datetime::DateTime::UTC();
	SceUInt8 yearTwoDigit = (SceUInt8)(utc.m_year % 100U);
	outTs->year = (SceUInt8)(((yearTwoDigit / 10U) << 4) | (yearTwoDigit % 10U));
	outTs->month = (SceUInt8)(((utc.m_month / 10U) << 4) | (utc.m_month % 10U));
	outTs->day = (SceUInt8)(((utc.m_day / 10U) << 4) | (utc.m_day % 10U));
	outTs->hour = (SceUInt8)(((utc.m_hour / 10U) << 4) | (utc.m_hour % 10U));
	outTs->minute = (SceUInt8)(((utc.m_minute / 10U) << 4) | (utc.m_minute % 10U));
	outTs->second = (SceUInt8)(((utc.m_second / 10U) << 4) | (utc.m_second % 10U));
	outTs->timezone = 36;
}

} /* namespace */

/* 0x813ED8B8 */
TelephonyResult TelSmsPduGeneratorFactory::Create(int factoryType,
						  TelSmsPduGenerator **outGenerator)
{
	if (factoryType == TEL_SMS_FACTORY_NONE)
		return TelephonyResult(TEL_OK);

	int mode = 0;
	if (factoryType == TEL_SMS_FACTORY_GSM)
		mode = TEL_SMS_TRANSCODER_GSM;
	else if (factoryType == TEL_SMS_FACTORY_CDMA)
		mode = TEL_SMS_TRANSCODER_CDMA;
	else
		return TelephonyResult(TEL_ERR_FAILED);

	TelSmsPduGenerator *generator = new (telGetAllocator()) TelSmsPduGenerator();
	*outGenerator = generator;
	if (generator != NULL)
		generator->Initialize(mode);
	return TelephonyResult(TEL_OK);
}

/* 0x813ED964 */
TelephonyResult TelSmsPduParserFactory::Create(int factoryType, TelSmsPduParser **outParser)
{
	if (factoryType == TEL_SMS_FACTORY_NONE)
		return TelephonyResult(TEL_OK);

	int mode = 0;
	if (factoryType == TEL_SMS_FACTORY_GSM)
		mode = TEL_SMS_TRANSCODER_GSM;
	else if (factoryType == TEL_SMS_FACTORY_CDMA)
		mode = TEL_SMS_TRANSCODER_CDMA;
	else
		return TelephonyResult(TEL_ERR_FAILED);

	TelSmsPduParser *parser = new (telGetAllocator()) TelSmsPduParser();
	*outParser = parser;
	if (parser != NULL)
		parser->Initialize(mode);
	return TelephonyResult(TEL_OK);
}

/* 0x813EDA10: Convert an outgoing GSM Submit PDU into an incoming loopback Deliver PDU */
TelephonyResult telConvertSubmitPduToLoopbackDeliver(const TelPhoneNumber &senderNumber,
						     TelSmsProtocolDataUnit *inOutPdu,
						     TelPhoneNumber *outDestinationNumber)
{
	SceUInt8 rawBytes[WMS_MAX_PDU_BYTES + 1];
	sce_paf_memset(rawBytes, 0, sizeof(rawBytes));
	SceSize pduLen = inOutPdu->GetLength();
	inOutPdu->GetBytes(rawBytes, pduLen);
	if (pduLen <= 1U)
		return TelephonyResult(TEL_ERR_FAILED);

	WmsRawTsData submitRaw;
	sce_paf_memset(&submitRaw, 0, sizeof(submitRaw));
	submitRaw.format = WMS_FORMAT_GW_PP;
	submitRaw.tpduType = WMS_TPDU_SUBMIT;
	submitRaw.length = pduLen - 1U;
	sce_paf_memcpy(submitRaw.bytes, &rawBytes[1], submitRaw.length);

	WmsClientTsData decodedSubmit;
	sce_paf_memset(&decodedSubmit, 0, sizeof(decodedSubmit));
	if (wmsTsDecode(&submitRaw, &decodedSubmit) != WMS_OK)
		return TelephonyResult(TEL_ERR_FAILED);

	/* Extract original Submit destination digits */
	const WmsGwSubmit &submit = decodedSubmit.u.gwPp.u.submit;
	char destDigits[33];
	sce_paf_memset(destDigits, 0, sizeof(destDigits));
	wmsTsBcdToAscii(submit.address.digits, submit.address.digitCount, destDigits);
	outDestinationNumber->SetDigits(destDigits);

	/* Build Deliver TPDU using senderNumber and copied DCS/UD/UDH */
	WmsClientTsData deliverTs;
	sce_paf_memset(&deliverTs, 0, sizeof(deliverTs));
	deliverTs.format = WMS_FORMAT_GW_PP;
	deliverTs.u.gwPp.tpduType = WMS_TPDU_DELIVER;
	WmsGwDeliver &deliver = deliverTs.u.gwPp.u.deliver;

	deliver.moreMessages = false;
	deliver.replyPath = submit.replyPath;
	deliver.userDataHeaderPresent = submit.userDataHeaderPresent;
	deliver.statusReportIndication = submit.replyPath;

	paf::common::EncodedString senderEncoded;
	ScePafMisc_C11CB5E4(&senderEncoded, senderNumber.GetDigits(),
			    paf::common::EncodedString::Encoding_Utf8);
	const char *senderAscii = ScePafMisc_A7C6AF5C(&senderEncoded);
	SceSize senderLen = sce_paf_strlen(senderAscii);
	if (senderLen > WMS_MAX_ADDRESS_DIGITS)
		senderLen = WMS_MAX_ADDRESS_DIGITS;

	deliver.address.digitMode = WMS_DIGIT_MODE_4_BIT;
	deliver.address.numberType = WMS_NUMBER_UNKNOWN;
	deliver.address.numberPlan = 1;
	deliver.address.digitCount = (SceUInt8)senderLen;
	sce_paf_memcpy(deliver.address.digits, senderAscii, senderLen);

	deliver.protocolId = submit.protocolId;
	deliver.dcs = submit.dcs;
	fillLoopbackUtcTimestamp(&deliver.timestamp);

	deliver.userData.dataLength = submit.userData.dataLength;
	sce_paf_memcpy(deliver.userData.bytes, submit.userData.bytes, deliver.userData.dataLength);

	SceUInt8 outHdrCount = 0;
	for (SceUInt8 idx = 0; idx < submit.userData.headerCount; ++idx) {
		const WmsUdhHeader &srcHdr = submit.userData.headers[idx];
		if (srcHdr.headerId == WMS_UDH_CONCAT_8 ||
		    srcHdr.headerId == WMS_UDH_CONCAT_16 ||
		    srcHdr.headerId == WMS_UDH_NAT_LANG_SINGLE_SHIFT ||
		    srcHdr.headerId == WMS_UDH_NAT_LANG_LOCKING_SHIFT) {
			deliver.userData.headers[outHdrCount++] = srcHdr;
		}
	}
	deliver.userData.headerCount = outHdrCount;

	WmsRawTsData deliverRaw;
	sce_paf_memset(&deliverRaw, 0, sizeof(deliverRaw));
	TelephonyResult res(TEL_OK);
	if (wmsTsEncode(&deliverTs, &deliverRaw) != WMS_OK) {
		res = TelephonyResult(TEL_ERR_FAILED);
	} else {
		SceUInt8 fullPdu[WMS_MAX_PDU_BYTES + 1];
		sce_paf_memset(fullPdu, 0, sizeof(fullPdu));

		WmsAddress scAddr;
		sce_paf_memset(&scAddr, 0, sizeof(scAddr));
		scAddr.digitMode = WMS_DIGIT_MODE_4_BIT;
		scAddr.numberType = WMS_NUMBER_UNKNOWN;
		scAddr.numberPlan = 1;
		scAddr.digitCount = 6;
		sce_paf_memcpy(scAddr.digits, "999999", 6);

		int scLen = wmsTsEncodeScAddress(&scAddr, fullPdu);
		sce_paf_memcpy(&fullPdu[scLen], deliverRaw.bytes, deliverRaw.length);
		inOutPdu->SetBytes(fullPdu, (SceSize)scLen + deliverRaw.length);
	}

	ScePafMisc_B45A7521(&senderEncoded);
	return res;
}

} /* namespace telephony */
