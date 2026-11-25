/*
 * LibWMS.cpp: Qualcomm AMSS 3GPP TS 23.040 (GSM/WCDMA) and 3GPP2 C.S0015 (CDMA)
 * SMS TPDU / Bearer Data encoder, decoder, and bit-packing utilities
 * (3.65-CEX: 0x813EDED2..0x813F2A9F, 0x813F3AC4..0x813F3DBB).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/ctype.h>
#include <std/string.h>
#include "../../../tel_sms_pdu_transcoder.h"

namespace telephony {

namespace {

static bool g_wmsInitialized = false;
static volatile SceSize g_lastDecodedLength = 0;

static void ensureWmsInitialized(void)
{
	if (!g_wmsInitialized)
		g_wmsInitialized = true;
}

static bool isValidBcdByte(SceUInt8 bcd)
{
	return (bcd & 0x0FU) <= 9U && (bcd >> 4) <= 9U;
}

static SceUInt8 swapNibbles(SceUInt8 byteVal)
{
	return (SceUInt8)((byteVal >> 4) | ((byteVal & 0x0FU) << 4));
}

static SceUInt8 decimalToBcd(int value)
{
	return (SceUInt8)(((value / 10) << 4) | (value % 10));
}

/* 0x813EE286: Encode 3GPP TS 23.038 Data Coding Scheme octet */
static int wmsTsEncodeDcs(const WmsDcs *dcs, SceUInt8 *outByte)
{
	ensureWmsInitialized();
	if (dcs->msgWaitingKind == WMS_MW_NONE) {
		SceUInt8 val = 0;
		if (dcs->isCompressed)
			val |= 0x20U;
		if (dcs->messageClass != 4)
			val |= 0x10U;
		val |= (SceUInt8)((dcs->alphabet & 0x03) << 2);
		val |= (SceUInt8)(dcs->messageClass & 0x03);
		*outByte = val;
	} else if (dcs->msgWaitingKind == WMS_MW_DATA_CODING) {
		SceUInt8 val = 0xF0U;
		if (dcs->alphabet == WMS_ALPHABET_8BIT)
			val = 0xF4U;
		val |= (SceUInt8)(dcs->messageClass & 0x03);
		*outByte = val;
	} else {
		SceUInt8 val = 0xE0U;
		if (dcs->msgWaitingKind == WMS_MW_DISCARD)
			val = 0xC0U;
		else if (dcs->msgWaitingKind == WMS_MW_STORE &&
			 dcs->alphabet == WMS_ALPHABET_DEFAULT_7BIT)
			val = 0xD0U;
		if (dcs->msgWaitingActive)
			val |= 0x08U;
		val |= (SceUInt8)(dcs->msgWaitingType & 0x03);
		*outByte = val;
	}
	return 1;
}

/* 0x813EF620: Decode 3GPP TS 23.038 Data Coding Scheme octet */
static int wmsTsDecodeDcs(const SceUInt8 *inByte, WmsDcs *outDcs)
{
	if (inByte == NULL || outDcs == NULL)
		return 0;

	ensureWmsInitialized();
	outDcs->messageClass = 4;
	outDcs->alphabet = WMS_ALPHABET_DEFAULT_7BIT;
	outDcs->msgWaitingKind = WMS_MW_NONE;
	outDcs->isCompressed = false;
	outDcs->msgWaitingType = 0;
	outDcs->msgWaitingActive = false;

	SceUInt8 raw = *inByte;
	SceUInt8 group = (SceUInt8)(raw >> 6);
	if (group == 0U) {
		outDcs->isCompressed = (raw & 0x20U) != 0;
		if ((raw & 0x10U) != 0)
			outDcs->messageClass = raw & 0x03;
		else
			outDcs->messageClass = 4;
		outDcs->alphabet = (raw >> 2) & 0x03;
		if (outDcs->alphabet > 2)
			outDcs->alphabet = WMS_ALPHABET_DEFAULT_7BIT;
	} else if (group == 3U) {
		if ((raw & 0x30U) == 0x30U) {
			outDcs->alphabet = (raw & 0x04U) != 0 ? WMS_ALPHABET_8BIT
							      : WMS_ALPHABET_DEFAULT_7BIT;
			outDcs->messageClass = raw & 0x03;
			outDcs->msgWaitingKind = WMS_MW_DATA_CODING;
		} else {
			if ((raw & 0x30U) == 0x00U) {
				outDcs->alphabet = WMS_ALPHABET_DEFAULT_7BIT;
				outDcs->msgWaitingKind = WMS_MW_DISCARD;
			} else if ((raw & 0x30U) == 0x10U) {
				outDcs->alphabet = WMS_ALPHABET_DEFAULT_7BIT;
				outDcs->msgWaitingKind = WMS_MW_STORE;
			} else {
				outDcs->alphabet = WMS_ALPHABET_UCS2;
				outDcs->msgWaitingKind = WMS_MW_STORE;
			}
			outDcs->msgWaitingActive = (raw & 0x08U) != 0;
			outDcs->msgWaitingType = raw & 0x03;
		}
	}
	outDcs->rawDcs = raw;
	return 1;
}

/* 0x813EE31E: Encode 7-octet 3GPP TS 23.040 SCTS timestamp */
static int wmsTsEncodeTimestamp(const WmsTimestamp *ts, SceUInt8 *outBytes)
{
	if (!isValidBcdByte(ts->year))
		return 0;
	outBytes[0] = swapNibbles(ts->year);

	if (!isValidBcdByte(ts->month))
		return 0;
	int monthVal = (ts->month & 0x0FU) + 10 * (ts->month >> 4);
	if (monthVal <= 0 || monthVal > 12)
		return 0;
	outBytes[1] = swapNibbles(ts->month);

	if (!isValidBcdByte(ts->day))
		return 0;
	int dayVal = (ts->day & 0x0FU) + 10 * (ts->day >> 4);
	if (dayVal <= 0 || dayVal > 31)
		return 0;
	outBytes[2] = swapNibbles(ts->day);

	if (!isValidBcdByte(ts->hour))
		return 0;
	if (((ts->hour & 0x0FU) + 10 * (ts->hour >> 4)) > 23)
		return 0;
	outBytes[3] = swapNibbles(ts->hour);

	if (!isValidBcdByte(ts->minute))
		return 0;
	if (((ts->minute & 0x0FU) + 10 * (ts->minute >> 4)) > 59)
		return 0;
	outBytes[4] = swapNibbles(ts->minute);

	if (!isValidBcdByte(ts->second))
		return 0;
	if (((ts->second & 0x0FU) + 10 * (ts->second >> 4)) > 59)
		return 0;
	outBytes[5] = swapNibbles(ts->second);

	if (ts->timezone > 48 || ts->timezone < -48)
		return 0;
	if (ts->timezone < 0) {
		int absTz = -ts->timezone;
		outBytes[6] = (SceUInt8)((absTz / 10) | ((absTz % 10) << 4) | 0x08);
	} else {
		outBytes[6] = (SceUInt8)((ts->timezone / 10) | ((ts->timezone % 10) << 4));
	}
	return 7;
}

/* 0x813EF70A: Decode 7-octet 3GPP TS 23.040 SCTS timestamp */
static int wmsTsDecodeTimestamp(const SceUInt8 *bytes, WmsTimestamp *outTs)
{
	if (bytes == NULL || outTs == NULL)
		return 0;

	outTs->year = isValidBcdByte(bytes[0]) ? swapNibbles(bytes[0]) : 0;

	SceUInt8 mon = 1;
	if (isValidBcdByte(bytes[1])) {
		int val = (bytes[1] >> 4) + 10 * (bytes[1] & 0x0FU);
		if (val >= 1 && val <= 12)
			mon = swapNibbles(bytes[1]);
	}
	outTs->month = mon;

	SceUInt8 day = 1;
	if (isValidBcdByte(bytes[2])) {
		int val = (bytes[2] >> 4) + 10 * (bytes[2] & 0x0FU);
		if (val >= 1 && val <= 31)
			day = swapNibbles(bytes[2]);
	}
	outTs->day = day;

	SceUInt8 hour = 0;
	if (isValidBcdByte(bytes[3]) && ((bytes[3] >> 4) + 10 * (bytes[3] & 0x0FU)) <= 23)
		hour = swapNibbles(bytes[3]);
	outTs->hour = hour;

	SceUInt8 min = 0;
	if (isValidBcdByte(bytes[4]) && ((bytes[4] >> 4) + 10 * (bytes[4] & 0x0FU)) <= 59)
		min = swapNibbles(bytes[4]);
	outTs->minute = min;

	SceUInt8 sec = 0;
	if (isValidBcdByte(bytes[5]) && ((bytes[5] >> 4) + 10 * (bytes[5] & 0x0FU)) <= 59)
		sec = swapNibbles(bytes[5]);
	outTs->second = sec;

	SceInt8 tz = 0;
	if ((bytes[6] & 0x08U) != 0)
		tz = (SceInt8)(-((bytes[6] >> 4) + 10 * (bytes[6] & 0x07)));
	else
		tz = (SceInt8)((bytes[6] >> 4) + 10 * (bytes[6] & 0x0F));
	if (tz > 48 || tz < -48)
		tz = 0;
	outTs->timezone = tz;
	return 7;
}

/* 0x813EED04: Encode relative validity period into 1 octet (3GPP TS 23.040 9.2.3.12.1) */
static SceUInt8 wmsTsEncodeValidityRelative(const WmsTimestamp *rel)
{
	if (rel == NULL)
		return 0;

	ensureWmsInitialized();
	SceUInt32 days = 0;
	if (isValidBcdByte(rel->year))
		days += 365U * ((rel->year & 0x0FU) + 10U * (rel->year >> 4));
	if (isValidBcdByte(rel->month))
		days += 30U * ((rel->month & 0x0FU) + 10U * (rel->month >> 4));
	if (isValidBcdByte(rel->day))
		days += (rel->day & 0x0FU) + 10U * (rel->day >> 4);

	if (days > 30U)
		return (SceUInt8)(((days + 6U) / 7U) + 192U);
	if (days > 0U)
		return (SceUInt8)(days + 166U);

	SceUInt32 minutes = 0;
	if (isValidBcdByte(rel->day))
		minutes += 1440U * ((rel->day & 0x0FU) + 10U * (rel->day >> 4));
	if (isValidBcdByte(rel->hour))
		minutes += 60U * ((rel->hour & 0x0FU) + 10U * (rel->hour >> 4));
	if (isValidBcdByte(rel->minute))
		minutes += (rel->minute & 0x0FU) + 10U * (rel->minute >> 4);

	if (minutes <= 720U)
		return (SceUInt8)(((minutes + 4U) / 5U) - 1U);
	return (SceUInt8)(((minutes - 749U) / 30U) + 143U);
}

/* 0x813F030C: Decode 1-octet relative validity period */
static void wmsTsDecodeValidityRelative(SceUInt8 octet, WmsTimestamp *outRel)
{
	if (outRel == NULL)
		return;

	ensureWmsInitialized();
	sce_paf_memset(outRel, 0, sizeof(*outRel));
	if (octet <= 143U) {
		SceUInt32 mins = 5U * octet + 5U;
		outRel->hour = decimalToBcd((int)(mins / 60U));
		outRel->minute = decimalToBcd((int)(mins % 60U));
	} else if (octet <= 166U) {
		SceUInt32 mins = 30U * octet - 4290U;
		outRel->hour = decimalToBcd((int)(mins / 60U + 12U));
		outRel->minute = decimalToBcd((int)(mins % 60U));
	} else if (octet <= 196U) {
		SceUInt32 days = octet - 166U;
		outRel->month = decimalToBcd((int)(days / 30U));
		outRel->day = decimalToBcd((int)(days % 30U));
	} else {
		SceUInt32 days = 7U * octet - 1344U;
		outRel->year = decimalToBcd((int)(days / 365U));
		SceUInt32 remDays = days % 365U;
		outRel->month = decimalToBcd((int)(remDays / 30U));
		outRel->day = decimalToBcd((int)(remDays % 30U));
	}
}

/* 0x813EEE46 */
static int wmsTsEncodeValidity(const WmsValidity *validity, SceUInt8 *outBytes)
{
	if (validity->format == WMS_VALIDITY_RELATIVE) {
		*outBytes = wmsTsEncodeValidityRelative(&validity->time);
		return 1;
	}
	if (validity->format == WMS_VALIDITY_ABSOLUTE)
		return wmsTsEncodeTimestamp(&validity->time, outBytes);
	return 0;
}

/* 0x813F056C */
static int wmsTsDecodeValidity(const SceUInt8 *bytes, WmsValidity *outValidity)
{
	if (bytes == NULL || outValidity == NULL)
		return 0;
	if (outValidity->format == WMS_VALIDITY_NONE) {
		sce_paf_memset(outValidity, 0, sizeof(*outValidity));
		return 0;
	}
	if (outValidity->format == WMS_VALIDITY_RELATIVE) {
		wmsTsDecodeValidityRelative(*bytes, &outValidity->time);
		return 1;
	}
	if (outValidity->format == WMS_VALIDITY_ABSOLUTE)
		return wmsTsDecodeTimestamp(bytes, &outValidity->time);
	return 0;
}

/* 0x813EEA4E */
static int wmsTsGetHeaderLength(const WmsUdhHeader *hdr)
{
	if (hdr == NULL)
		return 0;

	switch (hdr->headerId) {
	case WMS_UDH_CONCAT_8:
		return 5;
	case WMS_UDH_SPECIAL_SM:
	case WMS_UDH_PORT_8:
	case WMS_UDH_PRE_DEF_SOUND:
	case WMS_UDH_PRE_DEF_ANIM:
		return 4;
	case WMS_UDH_PORT_16:
	case WMS_UDH_CONCAT_16:
		return 6;
	case WMS_UDH_TEXT_FORMATTING:
		return hdr->u.textFormat.hasColor ? 6 : 5;
	case WMS_UDH_USER_DEF_SOUND:
		return hdr->u.userDefSound.length + 3;
	case WMS_UDH_LARGE_ANIM:
	case WMS_UDH_LARGE_PICTURE:
		return 131;
	case WMS_UDH_SMALL_ANIM:
	case WMS_UDH_SMALL_PICTURE:
		return 35;
	case WMS_UDH_VAR_PICTURE:
		return ((hdr->u.varPic.width * hdr->u.varPic.height) >> 3) + 5;
	case WMS_UDH_USER_PROMPT:
	case WMS_UDH_RFC822:
	case WMS_UDH_NAT_LANG_SINGLE_SHIFT:
	case WMS_UDH_NAT_LANG_LOCKING_SHIFT:
		return 3;
	case WMS_UDH_EXT_OBJECT:
		return hdr->u.extObject.hasHeader ? (hdr->u.extObject.dataLength + 9)
						  : (hdr->u.extObject.dataLength + 2);
	default:
		return hdr->u.other.length + 2;
	}
}

/* 0x813EEB48 */
static int wmsTsGetTotalHeadersLength(SceUInt8 headerCount, const WmsUdhHeader *headers)
{
	if (headers == NULL || headerCount == 0)
		return 0;

	int total = 1;
	for (SceUInt8 idx = 0; idx < headerCount && idx < WMS_MAX_UDH_HEADERS; ++idx)
		total += wmsTsGetHeaderLength(&headers[idx]);
	return total;
}

/* 0x813EE47E */
static int wmsTsEncodeTextFormattingHeader(const WmsUdhTextFormatting *fmt, SceUInt8 *out)
{
	out[0] = WMS_UDH_TEXT_FORMATTING;
	out[1] = fmt->hasColor ? 4 : 3;
	out[2] = fmt->startPosition;
	out[3] = fmt->length;
	out[4] = (SceUInt8)((fmt->alignment & 0x03) | ((fmt->fontSize & 0x03) << 2) |
			    (fmt->bold ? 0x10 : 0) | (fmt->italic ? 0x20 : 0) |
			    (fmt->underlined ? 0x40 : 0) | (fmt->strikethrough ? 0x80 : 0));
	if (fmt->hasColor) {
		out[5] = (SceUInt8)((fmt->foregroundColor & 0x0F) |
				    ((fmt->backgroundColor & 0x0F) << 4));
		return 6;
	}
	return 5;
}

/* 0x813EE4EA */
static int wmsTsEncodeVarPictureHeader(const WmsUdhVarPicture *pic, SceUInt8 *out)
{
	int byteCount = (pic->width * pic->height) >> 3;
	out[0] = WMS_UDH_VAR_PICTURE;
	out[1] = (SceUInt8)(byteCount + 3);
	out[2] = pic->position;
	out[3] = (SceUInt8)(pic->width >> 3);
	out[4] = pic->height;
	for (int idx = 0; idx < byteCount; ++idx)
		out[5 + idx] = pic->bitmap[idx];
	return 5 + byteCount;
}

/* 0x813EE57E */
static int wmsTsEncodeExtObjectHeader(const WmsUdhExtObject *extObj, SceUInt8 *out)
{
	int ieLen = extObj->dataLength + (extObj->hasHeader ? 7 : 0);
	if (ieLen > 131)
		return 0;
	out[0] = WMS_UDH_EXT_OBJECT;
	out[1] = (SceUInt8)ieLen;
	int pos = 2;
	if (extObj->hasHeader) {
		out[2] = extObj->referenceNumber;
		out[3] = (SceUInt8)(extObj->totalLength >> 8);
		out[4] = (SceUInt8)(extObj->totalLength & 0xFF);
		out[5] = extObj->controlByte;
		out[6] = (SceUInt8)extObj->objectType;
		out[7] = (SceUInt8)(extObj->position >> 8);
		out[8] = (SceUInt8)(extObj->position & 0xFF);
		pos = 9;
	}
	sce_paf_memcpy(&out[pos], extObj->payload, extObj->dataLength);
	return pos + extObj->dataLength;
}

/* 0x813EE5FE: Encode UDH list into outBuf (outBuf[0] = UDHL); returns UDHL */
static int wmsTsEncodeUserDataHeaders(SceUInt8 headerCount, const WmsUdhHeader *headers,
				      SceUInt8 *outBuf)
{
	if (headerCount == 0)
		return 0;

	int pos = 1;
	for (int idx = 0; idx < headerCount && idx < WMS_MAX_UDH_HEADERS; ++idx) {
		const WmsUdhHeader *hdr = &headers[idx];
		SceUInt8 *dst = &outBuf[pos];
		switch (hdr->headerId) {
		case WMS_UDH_CONCAT_8:
			if (hdr->u.concat8.totalSegments != 0 &&
			    hdr->u.concat8.sequenceNumber != 0 &&
			    hdr->u.concat8.sequenceNumber <= hdr->u.concat8.totalSegments) {
				dst[0] = WMS_UDH_CONCAT_8;
				dst[1] = 3;
				dst[2] = hdr->u.concat8.referenceNumber;
				dst[3] = hdr->u.concat8.totalSegments;
				dst[4] = hdr->u.concat8.sequenceNumber;
				pos += 5;
			}
			break;
		case WMS_UDH_SPECIAL_SM:
			dst[0] = WMS_UDH_SPECIAL_SM;
			dst[1] = 2;
			dst[2] = (SceUInt8)((hdr->u.specialSm.msgWaitingStore == 2 ? 0x80 : 0) |
					    (hdr->u.specialSm.msgWaitingType & 0x7F));
			dst[3] = hdr->u.specialSm.messageCount;
			pos += 4;
			break;
		case WMS_UDH_PORT_8:
			dst[0] = WMS_UDH_PORT_8;
			dst[1] = 2;
			dst[2] = hdr->u.port8.destPort;
			dst[3] = hdr->u.port8.origPort;
			pos += 4;
			break;
		case WMS_UDH_PORT_16:
			dst[0] = WMS_UDH_PORT_16;
			dst[1] = 4;
			dst[2] = (SceUInt8)(hdr->u.port16.destPort >> 8);
			dst[3] = (SceUInt8)(hdr->u.port16.destPort & 0xFF);
			dst[4] = (SceUInt8)(hdr->u.port16.origPort >> 8);
			dst[5] = (SceUInt8)(hdr->u.port16.origPort & 0xFF);
			pos += 6;
			break;
		case WMS_UDH_CONCAT_16:
			if (hdr->u.concat16.totalSegments != 0 &&
			    hdr->u.concat16.sequenceNumber != 0 &&
			    hdr->u.concat16.sequenceNumber <= hdr->u.concat16.totalSegments) {
				dst[0] = WMS_UDH_CONCAT_16;
				dst[1] = 4;
				dst[2] = (SceUInt8)(hdr->u.concat16.referenceNumber >> 8);
				dst[3] = (SceUInt8)(hdr->u.concat16.referenceNumber & 0xFF);
				dst[4] = hdr->u.concat16.totalSegments;
				dst[5] = hdr->u.concat16.sequenceNumber;
				pos += 6;
			}
			break;
		case WMS_UDH_TEXT_FORMATTING:
			pos += wmsTsEncodeTextFormattingHeader(&hdr->u.textFormat, dst);
			break;
		case WMS_UDH_PRE_DEF_SOUND:
			dst[0] = WMS_UDH_PRE_DEF_SOUND;
			dst[1] = 2;
			dst[2] = hdr->u.preDefSound.position;
			dst[3] = hdr->u.preDefSound.mediaIndex;
			pos += 4;
			break;
		case WMS_UDH_USER_DEF_SOUND:
			dst[0] = WMS_UDH_USER_DEF_SOUND;
			dst[1] = (SceUInt8)(hdr->u.userDefSound.length + 1);
			dst[2] = hdr->u.userDefSound.position;
			sce_paf_memcpy(&dst[3], hdr->u.userDefSound.melody,
				       hdr->u.userDefSound.length);
			pos += 3 + hdr->u.userDefSound.length;
			break;
		case WMS_UDH_PRE_DEF_ANIM:
			dst[0] = WMS_UDH_PRE_DEF_ANIM;
			dst[1] = 2;
			dst[2] = hdr->u.preDefAnim.position;
			dst[3] = hdr->u.preDefAnim.mediaIndex;
			pos += 4;
			break;
		case WMS_UDH_LARGE_ANIM:
			dst[0] = WMS_UDH_LARGE_ANIM;
			dst[1] = 129;
			dst[2] = hdr->u.largeAnim.position;
			sce_paf_memcpy(&dst[3], hdr->u.largeAnim.frames, 128);
			pos += 131;
			break;
		case WMS_UDH_SMALL_ANIM:
			dst[0] = WMS_UDH_SMALL_ANIM;
			dst[1] = 33;
			dst[2] = hdr->u.smallAnim.position;
			sce_paf_memcpy(&dst[3], hdr->u.smallAnim.frames, 32);
			pos += 35;
			break;
		case WMS_UDH_LARGE_PICTURE:
			dst[0] = WMS_UDH_LARGE_PICTURE;
			dst[1] = 129;
			dst[2] = hdr->u.largePic.position;
			sce_paf_memcpy(&dst[3], hdr->u.largePic.bitmap, 128);
			pos += 131;
			break;
		case WMS_UDH_SMALL_PICTURE:
			dst[0] = WMS_UDH_SMALL_PICTURE;
			dst[1] = 33;
			dst[2] = hdr->u.smallPic.position;
			sce_paf_memcpy(&dst[3], hdr->u.smallPic.bitmap, 32);
			pos += 35;
			break;
		case WMS_UDH_VAR_PICTURE:
			pos += wmsTsEncodeVarPictureHeader(&hdr->u.varPic, dst);
			break;
		case WMS_UDH_USER_PROMPT:
			dst[0] = WMS_UDH_USER_PROMPT;
			dst[1] = 1;
			dst[2] = hdr->u.userPromptCount;
			pos += 3;
			break;
		case WMS_UDH_EXT_OBJECT:
			pos += wmsTsEncodeExtObjectHeader(&hdr->u.extObject, dst);
			break;
		case WMS_UDH_RFC822:
			dst[0] = WMS_UDH_RFC822;
			dst[1] = 1;
			dst[2] = hdr->u.rfc822HeaderLen;
			pos += 3;
			break;
		case WMS_UDH_NAT_LANG_SINGLE_SHIFT:
		case WMS_UDH_NAT_LANG_LOCKING_SHIFT:
			dst[0] = (SceUInt8)hdr->headerId;
			dst[1] = 1;
			dst[2] = (SceUInt8)hdr->u.natLangShiftTable;
			pos += 3;
			break;
		default:
			dst[0] = (SceUInt8)hdr->u.other.iei;
			dst[1] = hdr->u.other.length;
			if (hdr->u.other.length != 0)
				sce_paf_memcpy(&dst[2], hdr->u.other.payload, hdr->u.other.length);
			pos += 2 + hdr->u.other.length;
			break;
		}
	}
	outBuf[0] = (SceUInt8)(pos - 1);
	return (SceUInt8)(pos - 1);
}

/* 0x813EF860: Decode UDH list from inBuf (inBuf[0] = UDHL); returns UDHL */
static int wmsTsDecodeUserDataHeaders(SceUInt8 maxLen, const SceUInt8 *inBuf,
				      SceUInt8 *outHeaderCount, WmsUdhHeader *outHeaders)
{
	if (inBuf == NULL || maxLen == 0 || inBuf[0] == 0 || outHeaderCount == NULL ||
	    outHeaders == NULL)
		return 0;

	SceUInt8 udhl = inBuf[0];
	SceUInt8 count = 0;
	int pos = 1;

	while (pos < (int)udhl + 1 && count < WMS_MAX_UDH_HEADERS) {
		SceUInt8 iei = inBuf[pos];
		int lenPos = pos + 1;
		SceUInt8 ieLen = inBuf[lenPos];
		const SceUInt8 *ieData = &inBuf[lenPos];
		WmsUdhHeader *hdr = &outHeaders[count];
		int consumed = 0;

		switch (iei) {
		case WMS_UDH_CONCAT_8:
			if (ieLen >= 3U && ieData[2] != 0 && ieData[3] != 0 &&
			    ieData[3] <= ieData[2]) {
				hdr->headerId = WMS_UDH_CONCAT_8;
				hdr->u.concat8.referenceNumber = ieData[1];
				hdr->u.concat8.totalSegments = ieData[2];
				hdr->u.concat8.sequenceNumber = ieData[3];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_SPECIAL_SM:
			if (ieLen >= 2U) {
				hdr->headerId = WMS_UDH_SPECIAL_SM;
				hdr->u.specialSm.msgWaitingStore = (ieData[1] & 0x80U) ? 2 : 1;
				hdr->u.specialSm.msgWaitingType = ieData[1] & 0x7F;
				hdr->u.specialSm.messageCount = ieData[2];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_PORT_8:
			if (ieLen >= 2U) {
				hdr->headerId = WMS_UDH_PORT_8;
				hdr->u.port8.destPort = ieData[1];
				hdr->u.port8.origPort = ieData[2];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_PORT_16:
			if (ieLen >= 4U) {
				hdr->headerId = WMS_UDH_PORT_16;
				hdr->u.port16.destPort =
					(SceUInt16)((ieData[1] << 8) | ieData[2]);
				hdr->u.port16.origPort =
					(SceUInt16)((ieData[3] << 8) | ieData[4]);
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_CONCAT_16:
			if (ieLen >= 4U && ieData[3] != 0 && ieData[4] != 0 &&
			    ieData[4] <= ieData[3]) {
				hdr->headerId = WMS_UDH_CONCAT_16;
				hdr->u.concat16.referenceNumber =
					(SceUInt16)((ieData[1] << 8) | ieData[2]);
				hdr->u.concat16.totalSegments = ieData[3];
				hdr->u.concat16.sequenceNumber = ieData[4];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_TEXT_FORMATTING:
			if (ieLen >= 3U) {
				hdr->headerId = WMS_UDH_TEXT_FORMATTING;
				hdr->u.textFormat.startPosition = ieData[1];
				hdr->u.textFormat.length = ieData[2];
				hdr->u.textFormat.alignment = ieData[3] & 0x03;
				hdr->u.textFormat.fontSize = (ieData[3] >> 2) & 0x03;
				hdr->u.textFormat.bold = (ieData[3] & 0x10U) != 0;
				hdr->u.textFormat.italic = (ieData[3] & 0x20U) != 0;
				hdr->u.textFormat.underlined = (ieData[3] & 0x40U) != 0;
				hdr->u.textFormat.strikethrough = (ieData[3] & 0x80U) != 0;
				hdr->u.textFormat.hasColor = (ieLen >= 4U);
				if (hdr->u.textFormat.hasColor) {
					hdr->u.textFormat.foregroundColor = ieData[4] & 0x0F;
					hdr->u.textFormat.backgroundColor = (ieData[4] >> 4) & 0x0F;
				}
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_PRE_DEF_SOUND:
			if (ieLen >= 2U) {
				hdr->headerId = WMS_UDH_PRE_DEF_SOUND;
				hdr->u.preDefSound.position = ieData[1];
				hdr->u.preDefSound.mediaIndex = ieData[2];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_USER_DEF_SOUND:
			if (ieLen >= 1U && (SceUInt8)(ieLen - 1U) <= 128U) {
				hdr->headerId = WMS_UDH_USER_DEF_SOUND;
				hdr->u.userDefSound.length = (SceUInt8)(ieLen - 1U);
				hdr->u.userDefSound.position = ieData[1];
				sce_paf_memset(hdr->u.userDefSound.melody, 0xFF, 128);
				sce_paf_memcpy(hdr->u.userDefSound.melody, &ieData[2],
					       hdr->u.userDefSound.length);
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_PRE_DEF_ANIM:
			if (ieLen == 2U) {
				hdr->headerId = WMS_UDH_PRE_DEF_ANIM;
				hdr->u.preDefAnim.position = ieData[1];
				hdr->u.preDefAnim.mediaIndex = ieData[2];
				consumed = 3;
			}
			break;
		case WMS_UDH_LARGE_ANIM:
			if (ieLen == 129U) {
				hdr->headerId = WMS_UDH_LARGE_ANIM;
				hdr->u.largeAnim.position = ieData[1];
				sce_paf_memcpy(hdr->u.largeAnim.frames, &ieData[2], 128);
				consumed = 130;
			}
			break;
		case WMS_UDH_SMALL_ANIM:
			if (ieLen == 33U) {
				hdr->headerId = WMS_UDH_SMALL_ANIM;
				hdr->u.smallAnim.position = ieData[1];
				sce_paf_memcpy(hdr->u.smallAnim.frames, &ieData[2], 32);
				consumed = 34;
			}
			break;
		case WMS_UDH_LARGE_PICTURE:
			if (ieLen == 129U) {
				hdr->headerId = WMS_UDH_LARGE_PICTURE;
				hdr->u.largePic.position = ieData[1];
				sce_paf_memcpy(hdr->u.largePic.bitmap, &ieData[2], 128);
				consumed = 130;
			}
			break;
		case WMS_UDH_SMALL_PICTURE:
			if (ieLen == 33U) {
				hdr->headerId = WMS_UDH_SMALL_PICTURE;
				hdr->u.smallPic.position = ieData[1];
				sce_paf_memcpy(hdr->u.smallPic.bitmap, &ieData[2], 32);
				consumed = 34;
			}
			break;
		case WMS_UDH_VAR_PICTURE:
			if (ieLen <= 137U && (SceUInt32)(ieLen - 3U) ==
						     (SceUInt32)ieData[2] * (SceUInt32)ieData[3]) {
				hdr->headerId = WMS_UDH_VAR_PICTURE;
				hdr->u.varPic.position = ieData[1];
				hdr->u.varPic.width = (SceUInt8)(ieData[2] * 8U);
				hdr->u.varPic.height = ieData[3];
				SceUInt8 bytesToCopy = (SceUInt8)(ieLen - 3U);
				if (bytesToCopy > 134U)
					bytesToCopy = 134U;
				sce_paf_memcpy(hdr->u.varPic.bitmap, &ieData[4], bytesToCopy);
				consumed = 4 + bytesToCopy;
			}
			break;
		case WMS_UDH_USER_PROMPT:
			if (ieLen >= 1U) {
				hdr->headerId = WMS_UDH_USER_PROMPT;
				hdr->u.userPromptCount = ieData[1];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_EXT_OBJECT: {
			bool isFirstSegment = true;
			int scan = 1;
			int scanCount = 0;
			while (scan < (int)udhl && scanCount < WMS_MAX_UDH_HEADERS) {
				if (inBuf[scan] == WMS_UDH_CONCAT_16) {
					if (inBuf[scan + 5] != 1)
						isFirstSegment = false;
					break;
				}
				scan += inBuf[scan + 1] + 2;
				++scanCount;
			}
			if (ieLen >= 1U) {
				hdr->headerId = WMS_UDH_EXT_OBJECT;
				hdr->u.extObject.hasHeader = isFirstSegment;
				int payloadOff = 1;
				bool valid = true;
				if (isFirstSegment) {
					if (ieLen < 7U) {
						valid = false;
					} else {
						hdr->u.extObject.referenceNumber = ieData[1];
						hdr->u.extObject.totalLength =
							(SceUInt16)((ieData[2] << 8) | ieData[3]);
						hdr->u.extObject.controlByte = ieData[4];
						hdr->u.extObject.objectType = ieData[5];
						hdr->u.extObject.position =
							(SceUInt16)((ieData[6] << 8) | ieData[7]);
						payloadOff = 8;
					}
				}
				if (valid) {
					SceUInt8 payLen = (SceUInt8)(ieLen - payloadOff + 1);
					hdr->u.extObject.dataLength = payLen;
					sce_paf_memcpy(hdr->u.extObject.payload,
						       &ieData[payloadOff], payLen);
					consumed = payloadOff + payLen;
				}
			}
			break;
		}
		case WMS_UDH_RFC822:
			if (ieLen >= 1U) {
				hdr->headerId = WMS_UDH_RFC822;
				hdr->u.rfc822HeaderLen = ieData[1];
				consumed = ieLen + 1;
			}
			break;
		case WMS_UDH_NAT_LANG_SINGLE_SHIFT:
		case WMS_UDH_NAT_LANG_LOCKING_SHIFT:
			if (ieLen == 1U && ieData[1] >= 1U && ieData[1] <= 3U) {
				hdr->headerId = iei;
				hdr->u.natLangShiftTable = ieData[1];
				consumed = 2;
			}
			break;
		default:
			if (ieLen <= 226U) {
				hdr->headerId = iei;
				hdr->u.other.iei = iei;
				hdr->u.other.length = ieLen;
				if (ieLen != 0)
					sce_paf_memcpy(hdr->u.other.payload, &ieData[1], ieLen);
				consumed = ieLen + 1;
			}
			break;
		}

		if (consumed == 0 || lenPos + consumed > 255) {
			*outHeaderCount = 0;
			return 0;
		}
		pos = lenPos + consumed;
		++count;
	}

	if (count >= WMS_MAX_UDH_HEADERS)
		pos = udhl + 1;
	if (pos != (int)udhl + 1)
		return 0;
	if (count != 0)
		*outHeaderCount = count;
	return udhl;
}

/* 0x813EEE76 */
static SceUInt32 wmsTsGetUserDataEncodedSize(const WmsDcs *dcs, const WmsGwUserData *ud)
{
	if (dcs == NULL || ud == NULL)
		return 0;

	SceUInt32 udhBytes = (SceUInt32)wmsTsGetTotalHeadersLength(ud->headerCount, ud->headers);
	SceUInt32 dataBytes = ud->dataLength;
	if (dcs->alphabet == WMS_ALPHABET_DEFAULT_7BIT)
		dataBytes = (7U * dataBytes + 7U) >> 3;
	return udhBytes + dataBytes;
}

/* 0x813EEB78 */
static int wmsTsEncodeUserData(const WmsDcs *dcs, const WmsGwUserData *ud, SceUInt8 *outBuf)
{
	outBuf[0] = (SceUInt8)ud->dataLength;
	if (dcs->alphabet == WMS_ALPHABET_DEFAULT_7BIT) {
		int udPos = 1;
		SceUInt16 fillBits = 0;
		if (ud->headerCount != 0) {
			int totalHdr = wmsTsGetTotalHeadersLength(ud->headerCount, ud->headers);
			if (totalHdr <= 140) {
				int udhl = wmsTsEncodeUserDataHeaders(ud->headerCount, ud->headers,
								      &outBuf[1]);
				int hdrBits = 8 * (udhl + 1);
				fillBits = (SceUInt16)(hdrBits % 7);
				if (fillBits != 0)
					fillBits = (SceUInt16)(7 - fillBits);
				outBuf[0] = (SceUInt8)((7 * ud->dataLength + hdrBits + fillBits) / 7);
				outBuf[1] = (SceUInt8)udhl;
				udPos = udhl + 2;
			}
		}
		int packed = wmsTsPackGsm7(ud->bytes, ud->dataLength, fillBits,
					   (SceUInt16)(255 - udPos), &outBuf[udPos]);
		return (SceUInt8)(udPos + packed);
	}

	int udPos = 1;
	if (ud->headerCount != 0) {
		int totalHdr = wmsTsGetTotalHeadersLength(ud->headerCount, ud->headers);
		if (totalHdr <= 140) {
			int udhl = wmsTsEncodeUserDataHeaders(ud->headerCount, ud->headers,
							      &outBuf[1]);
			outBuf[0] = (SceUInt8)(ud->dataLength + udhl + 1);
			udPos = udhl + 2;
		}
	}
	sce_paf_memcpy(&outBuf[udPos], ud->bytes, ud->dataLength);
	return (SceUInt8)(udPos + ud->dataLength);
}

/* 0x813F01F6 */
static int wmsTsDecodeUserData(const WmsDcs *dcs, SceUInt8 udl, const SceUInt8 *inBytes,
			       bool hasUdh, WmsGwUserData *outUd)
{
	if (dcs == NULL || inBytes == NULL || outUd == NULL)
		return 0;

	sce_paf_memset(outUd, 0, sizeof(*outUd));
	if (udl == 0)
		return 0;

	if (dcs->alphabet == WMS_ALPHABET_DEFAULT_7BIT) {
		if (udl > 160U)
			return 0;
		int udhl = 0;
		if (hasUdh)
			udhl = wmsTsDecodeUserDataHeaders(inBytes[0], inBytes, &outUd->headerCount,
							  outUd->headers);
		if (udhl > udl)
			return 0;

		int offset = 0;
		SceInt16 shiftBits = 0;
		SceUInt8 septets = udl;
		if (udhl > 0) {
			int remBits = 7 * udl - 8 * udhl - 8;
			shiftBits = (SceInt16)(remBits % 7);
			if (shiftBits != 0)
				shiftBits = (SceInt16)(8 - shiftBits);
			septets = (SceUInt8)(remBits / 7);
			offset = udhl + 1;
		}
		int consumed =
			wmsTsUnpackGsm7(&inBytes[offset], septets, 255, shiftBits, outUd->bytes);
		outUd->dataLength = septets;
		return (SceUInt8)(offset + consumed);
	}

	if (udl > 140U)
		return 0;
	int offset = 0;
	SceSize copyLen = udl;
	if (hasUdh) {
		int udhl = wmsTsDecodeUserDataHeaders(inBytes[0], inBytes, &outUd->headerCount,
						      outUd->headers);
		if (udhl > udl)
			return 0;
		offset = udhl + 1;
		copyLen = (SceSize)(udl - udhl - 1);
	}
	sce_paf_memcpy(outUd->bytes, &inBytes[offset], copyLen);
	outUd->dataLength = (SceUInt16)copyLen;
	return (SceUInt8)(offset + copyLen);
}

/* 0x813EEEDE: Encode GSM SMS-DELIVER */
static int wmsTsEncodeDeliver(const WmsGwDeliver *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	SceUInt8 firstOctet = 0;
	if (!msg->moreMessages)
		firstOctet |= 0x04U;
	if (msg->statusReportIndication)
		firstOctet |= 0x20U;
	if (msg->userDataHeaderPresent)
		firstOctet |= 0x40U;
	if (msg->replyPath)
		firstOctet |= 0x80U;
	outRaw->bytes[0] = firstOctet;

	int addrLen = wmsTsEncodeAddress(&msg->address, &outRaw->bytes[1]);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;

	int pos = addrLen + 1;
	outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int tsLen = wmsTsEncodeTimestamp(&msg->timestamp, &outRaw->bytes[pos]);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += tsLen;

	int status = WMS_OK;
	if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
		pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
	else
		status = WMS_ERR_INVALID_USER_DATA_SIZE;

	outRaw->tpduType = WMS_TPDU_DELIVER;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EEFB2 */
static int wmsTsEncodeDeliverReportAck(const WmsGwDeliverReportAck *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	outRaw->bytes[0] = msg->userDataHeaderPresent ? 0x40U : 0x00U;
	outRaw->bytes[1] = (SceUInt8)msg->mask;
	int pos = 2;
	if ((msg->mask & 1) != 0)
		outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	if ((msg->mask & 2) != 0)
		pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if ((msg->mask & 4) != 0) {
		if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
			pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
		else
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
	}
	outRaw->tpduType = WMS_TPDU_DELIVER_REPORT_ACK;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EF058 */
static int wmsTsEncodeDeliverReportError(const WmsGwDeliverReportError *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	outRaw->bytes[0] = msg->userDataHeaderPresent ? 0x40U : 0x00U;
	outRaw->bytes[1] = (SceUInt8)msg->tpFailureCause;
	outRaw->bytes[2] = (SceUInt8)msg->mask;
	int pos = 3;
	if ((msg->mask & 1) != 0)
		outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	if ((msg->mask & 2) != 0)
		pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if ((msg->mask & 4) != 0) {
		if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
			pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
		else
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
	}
	outRaw->tpduType = WMS_TPDU_DELIVER_REPORT_ERROR;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EF108 */
static int wmsTsEncodeStatusReport(const WmsGwStatusReport *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	SceUInt8 firstOctet = 0x02U;
	if (!msg->moreMessages)
		firstOctet |= 0x04U;
	if (msg->statusReportQualifier)
		firstOctet |= 0x20U;
	if (msg->userDataHeaderPresent)
		firstOctet |= 0x40U;
	outRaw->bytes[0] = firstOctet;
	outRaw->bytes[1] = (SceUInt8)msg->messageReference;

	int addrLen = wmsTsEncodeAddress(&msg->address, &outRaw->bytes[2]);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = addrLen + 2;

	int tsLen = wmsTsEncodeTimestamp(&msg->timestamp, &outRaw->bytes[pos]);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += tsLen;

	int disLen = wmsTsEncodeTimestamp(&msg->dischargeTime, &outRaw->bytes[pos]);
	if (disLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += disLen;

	outRaw->bytes[pos++] = (SceUInt8)msg->tpStatus;
	outRaw->bytes[pos++] = (SceUInt8)msg->mask;
	if ((msg->mask & 1) != 0)
		outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	if ((msg->mask & 2) != 0)
		pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if ((msg->mask & 4) != 0) {
		if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
			pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
		else
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
	}
	outRaw->tpduType = WMS_TPDU_STATUS_REPORT;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EF232 */
static int wmsTsEncodeCommand(const WmsGwCommand *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	SceUInt8 firstOctet = 0x02U;
	if (msg->statusReportRequest)
		firstOctet |= 0x20U;
	if (msg->userDataHeaderPresent)
		firstOctet |= 0x40U;
	outRaw->bytes[0] = firstOctet;
	outRaw->bytes[1] = (SceUInt8)msg->messageReference;
	outRaw->bytes[2] = (SceUInt8)msg->protocolId;
	outRaw->bytes[3] = (SceUInt8)msg->commandType;
	outRaw->bytes[4] = (SceUInt8)msg->messageNumber;

	int addrLen = wmsTsEncodeAddress(&msg->address, &outRaw->bytes[5]);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = addrLen + 5;

	SceUInt8 cmdLen = msg->commandDataLen <= 157U ? msg->commandDataLen : 157U;
	outRaw->bytes[pos++] = cmdLen;
	int avail = 255 - pos;
	int copyLen = cmdLen < avail ? cmdLen : avail;
	sce_paf_memcpy(&outRaw->bytes[pos], msg->commandData, (SceSize)copyLen);

	outRaw->tpduType = WMS_TPDU_COMMAND;
	outRaw->length = (SceSize)(pos + msg->commandDataLen);
	return WMS_OK;
}

/* 0x813EF2FA: Encode GSM SMS-SUBMIT */
static int wmsTsEncodeSubmit(const WmsGwSubmit *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	if ((SceUInt32)msg->validity.format > 3U)
		return WMS_ERR_INVALID_VALIDITY_FORMAT;

	SceUInt8 firstOctet = 0x01U;
	if (msg->rejectDuplicates)
		firstOctet |= 0x04U;
	firstOctet |= (SceUInt8)((msg->validity.format & 0x03) << 3);
	if (msg->statusReportRequest)
		firstOctet |= 0x20U;
	if (msg->userDataHeaderPresent)
		firstOctet |= 0x40U;
	if (msg->replyPath)
		firstOctet |= 0x80U;
	outRaw->bytes[0] = firstOctet;
	outRaw->bytes[1] = (SceUInt8)msg->messageReference;

	int addrLen = wmsTsEncodeAddress(&msg->address, &outRaw->bytes[2]);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = addrLen + 2;

	outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);
	pos += wmsTsEncodeValidity(&msg->validity, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
		pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
	else
		status = WMS_ERR_INVALID_USER_DATA_SIZE;

	outRaw->tpduType = WMS_TPDU_SUBMIT;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EF3EC */
static int wmsTsEncodeSubmitReportAck(const WmsGwSubmitReportAck *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	outRaw->bytes[0] = (SceUInt8)(0x01U | (msg->userDataHeaderPresent ? 0x40U : 0x00U));
	outRaw->bytes[1] = (SceUInt8)msg->mask;
	int tsLen = wmsTsEncodeTimestamp(&msg->timestamp, &outRaw->bytes[2]);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	int pos = tsLen + 2;

	if ((msg->mask & 1) != 0)
		outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	if ((msg->mask & 2) != 0)
		pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if ((msg->mask & 4) != 0) {
		if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
			pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
		else
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
	}
	outRaw->tpduType = WMS_TPDU_SUBMIT_REPORT_ACK;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813EF4B2 */
static int wmsTsEncodeSubmitReportError(const WmsGwSubmitReportError *msg, WmsRawTsData *outRaw)
{
	if (msg == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	sce_paf_memset(outRaw->bytes, 0, WMS_MAX_PDU_BYTES);
	outRaw->bytes[0] = (SceUInt8)(0x01U | (msg->userDataHeaderPresent ? 0x40U : 0x00U));
	outRaw->bytes[1] = (SceUInt8)msg->tpFailureCause;
	outRaw->bytes[2] = (SceUInt8)msg->mask;
	int tsLen = wmsTsEncodeTimestamp(&msg->timestamp, &outRaw->bytes[3]);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	int pos = tsLen + 3;

	if ((msg->mask & 1) != 0)
		outRaw->bytes[pos++] = (SceUInt8)msg->protocolId;
	if ((msg->mask & 2) != 0)
		pos += wmsTsEncodeDcs(&msg->dcs, &outRaw->bytes[pos]);

	int status = WMS_OK;
	if ((msg->mask & 4) != 0) {
		if (wmsTsGetUserDataEncodedSize(&msg->dcs, &msg->userData) <= 140U)
			pos += wmsTsEncodeUserData(&msg->dcs, &msg->userData, &outRaw->bytes[pos]);
		else
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
	}
	outRaw->tpduType = WMS_TPDU_SUBMIT_REPORT_ERROR;
	outRaw->length = (SceSize)pos;
	return status;
}

/* 0x813F05B2: Decode GSM SMS-DELIVER */
static int wmsTsDecodeDeliver(const WmsRawTsData *raw, WmsGwDeliver *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	SceUInt8 firstOctet = raw->bytes[0];
	if ((firstOctet & 0x03U) != 0U)
		return WMS_ERR_INVALID_TPDU_TYPE;

	outMsg->moreMessages = (firstOctet & 0x04U) == 0;
	outMsg->statusReportIndication = (firstOctet & 0x20U) != 0;
	outMsg->userDataHeaderPresent = (firstOctet & 0x40U) != 0;
	outMsg->replyPath = (firstOctet & 0x80U) != 0;

	int addrLen = wmsTsDecodeAddress(&raw->bytes[1], &outMsg->address);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;

	int pos = addrLen + 1;
	outMsg->protocolId = raw->bytes[pos++];
	pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	if (outMsg->dcs.msgWaitingType != 0 && outMsg->protocolId == 0x5F) {
		outMsg->dcs.msgWaitingType = 0;
		outMsg->dcs.msgWaitingKind = WMS_MW_STORE;
		outMsg->dcs.msgWaitingActive = true;
	}

	int tsLen = wmsTsDecodeTimestamp(&raw->bytes[pos], &outMsg->timestamp);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += tsLen;

	SceUInt8 udl = raw->bytes[pos++];
	int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
					outMsg->userDataHeaderPresent, &outMsg->userData);
	g_lastDecodedLength = (SceSize)(pos + udLen);
	return udLen > 140 ? WMS_ERR_INVALID_USER_DATA_SIZE : WMS_OK;
}

/* 0x813F06A0 */
static int wmsTsDecodeDeliverReportAck(const WmsRawTsData *raw, WmsGwDeliverReportAck *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	outMsg->userDataHeaderPresent = (raw->bytes[0] & 0x40U) != 0;
	outMsg->mask = raw->bytes[1];
	int pos = 2;
	if ((outMsg->mask & 1) != 0)
		outMsg->protocolId = raw->bytes[pos++];
	if ((outMsg->mask & 2) != 0)
		pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	int status = WMS_OK;
	if ((outMsg->mask & 4) != 0) {
		SceUInt8 udl = raw->bytes[pos++];
		int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
						outMsg->userDataHeaderPresent, &outMsg->userData);
		if (udLen > 140)
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
		pos += udLen;
	}
	g_lastDecodedLength = (SceSize)pos;
	return status;
}

/* 0x813F0724 */
static int wmsTsDecodeDeliverReportError(const WmsRawTsData *raw, WmsGwDeliverReportError *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	outMsg->userDataHeaderPresent = (raw->bytes[0] & 0x40U) != 0;
	outMsg->tpFailureCause = raw->bytes[1];
	outMsg->mask = raw->bytes[2];
	int pos = 3;
	if ((outMsg->mask & 1) != 0)
		outMsg->protocolId = raw->bytes[pos++];
	if ((outMsg->mask & 2) != 0)
		pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	int status = WMS_OK;
	if ((outMsg->mask & 4) != 0) {
		SceUInt8 udl = raw->bytes[pos++];
		int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
						outMsg->userDataHeaderPresent, &outMsg->userData);
		if (udLen > 140)
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
		pos += udLen;
	}
	g_lastDecodedLength = (SceSize)pos;
	return status;
}

/* 0x813F07B0 */
static int wmsTsDecodeStatusReport(const WmsRawTsData *raw, WmsGwStatusReport *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	SceUInt8 firstOctet = raw->bytes[0];
	outMsg->moreMessages = (firstOctet & 0x04U) == 0;
	outMsg->statusReportQualifier = (firstOctet & 0x20U) != 0;
	outMsg->userDataHeaderPresent = (firstOctet & 0x40U) != 0;
	outMsg->messageReference = raw->bytes[1];

	int addrLen = wmsTsDecodeAddress(&raw->bytes[2], &outMsg->address);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = addrLen + 2;

	int tsLen = wmsTsDecodeTimestamp(&raw->bytes[pos], &outMsg->timestamp);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += tsLen;

	int disLen = wmsTsDecodeTimestamp(&raw->bytes[pos], &outMsg->dischargeTime);
	if (disLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += disLen;

	outMsg->tpStatus = raw->bytes[pos++];
	outMsg->mask = raw->bytes[pos];
	outMsg->protocolId = 0;
	outMsg->userData.dataLength = 0;

	int status = WMS_OK;
	if (outMsg->mask == 0xFF || outMsg->mask == 0) {
		outMsg->mask = 0;
	} else {
		++pos;
		if ((outMsg->mask & 1) != 0)
			outMsg->protocolId = raw->bytes[pos++];
		if ((outMsg->mask & 2) != 0)
			pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);
		if ((outMsg->mask & 4) != 0) {
			SceUInt8 udl = raw->bytes[pos++];
			int udLen =
				wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
						    outMsg->userDataHeaderPresent,
						    &outMsg->userData);
			if (udLen > 140)
				status = WMS_ERR_INVALID_USER_DATA_SIZE;
			pos += udLen;
		}
	}
	g_lastDecodedLength = (SceSize)pos;
	return status;
}

/* 0x813F08CC */
static int wmsTsDecodeCommand(const WmsRawTsData *raw, WmsGwCommand *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	outMsg->statusReportRequest = (raw->bytes[0] & 0x20U) != 0;
	outMsg->userDataHeaderPresent = (raw->bytes[0] & 0x40U) != 0;
	outMsg->messageReference = raw->bytes[1];
	outMsg->protocolId = raw->bytes[2];
	outMsg->commandType = raw->bytes[3];
	outMsg->messageNumber = raw->bytes[4];

	int addrLen = wmsTsDecodeAddress(&raw->bytes[5], &outMsg->address);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;

	SceUInt8 cmdLen = raw->bytes[addrLen + 5];
	if (cmdLen > 157U)
		cmdLen = 157U;
	outMsg->commandDataLen = cmdLen;
	sce_paf_memcpy(outMsg->commandData, &raw->bytes[addrLen + 6], cmdLen);
	g_lastDecodedLength = (SceSize)(addrLen + 6);
	return WMS_OK;
}

/* 0x813F094C: Decode GSM SMS-SUBMIT */
static int wmsTsDecodeSubmit(const WmsRawTsData *raw, WmsGwSubmit *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	SceUInt8 firstOctet = raw->bytes[0];
	outMsg->rejectDuplicates = (firstOctet & 0x04U) != 0;
	outMsg->validity.format = (firstOctet >> 3) & 0x03;
	outMsg->statusReportRequest = (firstOctet & 0x20U) != 0;
	outMsg->userDataHeaderPresent = (firstOctet & 0x40U) != 0;
	outMsg->replyPath = (firstOctet & 0x80U) != 0;
	outMsg->messageReference = raw->bytes[1];

	int addrLen = wmsTsDecodeAddress(&raw->bytes[2], &outMsg->address);
	if (addrLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = addrLen + 2;

	outMsg->protocolId = raw->bytes[pos++];
	pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	int valLen = wmsTsDecodeValidity(&raw->bytes[pos], &outMsg->validity);
	if (outMsg->validity.format != WMS_VALIDITY_NONE && valLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	pos += valLen;

	SceUInt8 udl = raw->bytes[pos++];
	int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
					outMsg->userDataHeaderPresent, &outMsg->userData);
	g_lastDecodedLength = (SceSize)(pos + udLen);
	return udLen > 140 ? WMS_ERR_INVALID_USER_DATA_SIZE : WMS_OK;
}

/* 0x813F0A24 */
static int wmsTsDecodeSubmitReportAck(const WmsRawTsData *raw, WmsGwSubmitReportAck *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	outMsg->userDataHeaderPresent = (raw->bytes[0] & 0x40U) != 0;
	outMsg->mask = raw->bytes[1];
	int tsLen = wmsTsDecodeTimestamp(&raw->bytes[2], &outMsg->timestamp);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_SIZE;
	int pos = tsLen + 2;

	if ((outMsg->mask & 1) != 0)
		outMsg->protocolId = raw->bytes[pos++];
	if ((outMsg->mask & 2) != 0)
		pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	int status = WMS_OK;
	if ((outMsg->mask & 4) != 0) {
		SceUInt8 udl = raw->bytes[pos++];
		int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
						outMsg->userDataHeaderPresent, &outMsg->userData);
		if (udLen > 140)
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
		pos += udLen;
	}
	g_lastDecodedLength = (SceSize)pos;
	return status;
}

/* 0x813F0AD4 */
static int wmsTsDecodeSubmitReportError(const WmsRawTsData *raw, WmsGwSubmitReportError *outMsg)
{
	if (raw == NULL || outMsg == NULL)
		return WMS_ERR_NULL_PTR;

	outMsg->userDataHeaderPresent = (raw->bytes[0] & 0x40U) != 0;
	outMsg->tpFailureCause = raw->bytes[1];
	outMsg->mask = raw->bytes[2];
	int tsLen = wmsTsDecodeTimestamp(&raw->bytes[3], &outMsg->timestamp);
	if (tsLen == 0)
		return WMS_ERR_INVALID_PARM_VALUE;
	int pos = tsLen + 3;

	if ((outMsg->mask & 1) != 0)
		outMsg->protocolId = raw->bytes[pos++];
	if ((outMsg->mask & 2) != 0)
		pos += wmsTsDecodeDcs(&raw->bytes[pos], &outMsg->dcs);

	int status = WMS_OK;
	if ((outMsg->mask & 4) != 0) {
		SceUInt8 udl = raw->bytes[pos++];
		int udLen = wmsTsDecodeUserData(&outMsg->dcs, udl, &raw->bytes[pos],
						outMsg->userDataHeaderPresent, &outMsg->userData);
		if (udLen > 140)
			status = WMS_ERR_INVALID_USER_DATA_SIZE;
		pos += udLen;
	}
	g_lastDecodedLength = (SceSize)pos;
	return status;
}

/* 0x813F0C98: Encode CDMA IS-91 extended protocol subpayload */
static int wmsTsEncodeCdmaIs91(const WmsCdmaBearerData *src, WmsCdmaBearerData *dst)
{
	if (src == NULL || dst == NULL)
		return WMS_ERR_NULL_PTR;

	switch (src->userData.is91epType) {
	case WMS_IS91EP_VOICE_MAIL: {
		SceUInt8 tens = (SceUInt8)(src->numMessages / 10);
		SceUInt8 ones = (SceUInt8)(src->numMessages % 10);
		if (tens > 9U)
			return WMS_ERR_INVALID_FORMAT;
		dst->userData.bytes[0] = tens;
		dst->userData.bytes[1] = ones;
		int offset = 2;
		if ((src->mask & 0x100U) != 0) {
			dst->userData.bytes[2] = src->priority != 0 ? 1 : 0;
			offset = 3;
		}
		for (int idx = 0; idx < src->userData.numCharacters; ++idx)
			dst->userData.bytes[offset + idx] =
				(SceUInt8)(src->userData.bytes[idx] - 32);
		dst->userData.dataLength = (SceUInt8)(src->userData.numCharacters + offset);
		dst->userData.numCharacters = dst->userData.dataLength;
		return WMS_OK;
	}
	case WMS_IS91EP_SHORT_MESSAGE_FULL:
	case WMS_IS91EP_SHORT_MESSAGE:
		for (int idx = 0; idx < src->userData.numCharacters; ++idx)
			dst->userData.bytes[idx] = (SceUInt8)(src->userData.bytes[idx] - 32);
		dst->userData.dataLength = src->userData.numCharacters;
		dst->userData.numCharacters = src->userData.numCharacters;
		return WMS_OK;
	case WMS_IS91EP_CLI_ORDER:
		for (int idx = 0; idx < src->callback.digitCount; ++idx)
			dst->userData.bytes[idx] = src->callback.digits[idx];
		dst->userData.dataLength = src->callback.digitCount;
		dst->userData.numCharacters = src->callback.digitCount;
		return WMS_OK;
	default:
		return WMS_ERR_INVALID_FORMAT;
	}
}

/* 0x813F1B2E: Decode CDMA IS-91 extended protocol subpayload */
static int wmsTsDecodeCdmaIs91(int is91Type, SceUInt8 length, const SceUInt8 *bytes,
			       WmsCdmaBearerData *outData)
{
	if (outData == NULL || bytes == NULL)
		return WMS_ERR_NULL_PTR;
	if (length > 48U)
		return WMS_ERR_INVALID_PARM_SIZE;

	outData->alertMode = 0;
	outData->mask = 0x1000U;

	if (is91Type == WMS_IS91EP_CLI_ORDER) {
		outData->callback.digitMode = WMS_DIGIT_MODE_4_BIT;
		outData->mask = 0x5000U;
		outData->callback.digitCount = length;
		for (int idx = 0; idx < length && idx < 48; ++idx)
			outData->callback.digits[idx] = bytes[idx];
		return WMS_OK;
	}
	if (is91Type == WMS_IS91EP_SHORT_MESSAGE_FULL || is91Type == WMS_IS91EP_SHORT_MESSAGE) {
		outData->mask = 0x1002U;
		outData->userData.encoding = WMS_CDMA_ENC_IS91EP;
		outData->userData.is91epType = WMS_IS91EP_SHORT_MESSAGE;
		outData->userData.numCharacters = length;
		outData->userData.dataLength = length;
		for (int idx = 0; idx < length; ++idx) {
			if (idx >= WMS_MAX_CDMA_USER_DATA)
				return WMS_ERR_INVALID_USER_DATA_SIZE;
			outData->userData.bytes[idx] = (SceUInt8)(bytes[idx] + 32);
		}
		return WMS_OK;
	}
	if (is91Type == WMS_IS91EP_VOICE_MAIL) {
		outData->mask = 0x1800U;
		outData->numMessages = (SceUInt8)(10 * (bytes[0] & 0x0F) + (bytes[1] & 0x0F));
		int offset = 2;
		if (length > 2U && (bytes[2] == 0U || bytes[2] == 1U)) {
			outData->priority = (bytes[2] == 1U) ? 2 : 0;
			outData->mask = 0x1900U;
			offset = 3;
		}
		if (length > offset) {
			outData->mask |= 0x0002U;
			outData->userData.encoding = WMS_CDMA_ENC_IS91EP;
			outData->userData.is91epType = WMS_IS91EP_VOICE_MAIL;
			outData->userData.numCharacters = (SceUInt8)(length - offset);
			outData->userData.dataLength = (SceUInt8)(length - offset);
			for (int idx = offset; idx < length; ++idx) {
				if (idx >= WMS_MAX_CDMA_USER_DATA)
					return WMS_ERR_INVALID_USER_DATA_SIZE;
				outData->userData.bytes[idx - offset] = (SceUInt8)(bytes[idx] + 32);
			}
		}
		return WMS_OK;
	}
	return WMS_ERR_INVALID_FORMAT;
}

/* 0x813F0DE0: Encode 3GPP2 C.S0015 CDMA Bearer Data */
static int wmsTsEncodeCdmaBearerData(const WmsCdmaBearerData *cdma, WmsRawTsData *outRaw)
{
	if (cdma == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;

	SceUInt8 *buf = outRaw->bytes;
	SceUInt16 pos = 0;
	int status = WMS_OK;

	/* Subparameter 0: Message Identifier */
	if ((cdma->mask & 0x0001U) != 0) {
		buf[0] = 0;
		buf[1] = 3;
		wmsBitPackByte((SceUInt8)cdma->messageId.type, buf, 16, 4);
		wmsBitPackWord((SceUInt16)cdma->messageId.idNumber, buf, 20, 16);
		wmsBitPackByte(cdma->messageId.udhPresent ? 1 : 0, buf, 36, 1);
		wmsBitPackByte(0, buf, 37, 3);
		pos = 5;
	}

	/* Subparameter 1: User Data */
	if ((cdma->mask & 0x0002U) != 0 ||
	    (cdma->userData.encoding == WMS_CDMA_ENC_IS91EP &&
	     cdma->userData.is91epType == WMS_IS91EP_CLI_ORDER)) {
		buf[pos] = 1;
		SceUInt16 lenPos = (SceUInt16)(pos + 1);
		SceUInt16 bodyStart = (SceUInt16)(pos + 2);
		SceUInt16 bitPos = (SceUInt16)(bodyStart * 8);

		wmsBitPackByte((SceUInt8)cdma->userData.encoding, buf, bitPos, 5);
		bitPos = (SceUInt16)(bitPos + 5);

		int charBits = 8;
		WmsCdmaBearerData is91Copy;
		if (cdma->userData.encoding == WMS_CDMA_ENC_IS91EP) {
			if (cdma->userData.numCharacters != cdma->userData.dataLength)
				return WMS_ERR_INVALID_USER_DATA_SIZE;
			wmsBitPackByte((SceUInt8)cdma->userData.is91epType, buf, bitPos, 8);
			bitPos = (SceUInt16)(bitPos + 8);
			charBits = (cdma->userData.is91epType == WMS_IS91EP_CLI_ORDER) ? 4 : 6;
			status = wmsTsEncodeCdmaIs91(cdma, &is91Copy);
			if (status != WMS_OK)
				return status;
		}

		SceUInt8 tempPacked[WMS_MAX_PDU_BYTES];
		sce_paf_memset(tempPacked, 0, sizeof(tempPacked));
		int outCount = 0;
		SceUInt8 extraPadBits = 0;

		if (cdma->messageId.udhPresent) {
			int udhl = 0;
			int hdrLen = 1;
			if (wmsTsGetTotalHeadersLength(cdma->userData.headerCount,
						       cdma->userData.headers) > 229) {
				status = WMS_ERR_INVALID_USER_DATA_SIZE;
			} else {
				udhl = wmsTsEncodeUserDataHeaders(cdma->userData.headerCount,
								  cdma->userData.headers,
								  tempPacked);
				hdrLen = udhl + 1;
			}
			int enc = cdma->userData.encoding;
			SceUInt16 fillBits = 0;
			SceUInt8 numFields = 0;
			if (enc == WMS_CDMA_ENC_GSM7 || enc == WMS_CDMA_ENC_ASCII ||
			    enc == WMS_CDMA_ENC_IA5) {
				int hdrBits = 8 * (udhl + 1);
				fillBits = (SceUInt16)(hdrBits % 7);
				if (fillBits != 0)
					fillBits = (SceUInt16)(7 - fillBits);
				numFields = (SceUInt8)((7 * cdma->userData.numCharacters +
							hdrBits + fillBits) /
						       7);
			} else if (enc == WMS_CDMA_ENC_UNICODE) {
				fillBits = (hdrLen % 2 != 0) ? 8 : 0;
				numFields = (SceUInt8)((8 * udhl + 16 * cdma->userData.numCharacters +
							fillBits + 8) >>
						       4);
			} else {
				numFields = (SceUInt8)(udhl + cdma->userData.numCharacters + 1);
			}
			wmsBitPackByte(numFields, buf, bitPos, 8);
			bitPos = (SceUInt16)(bitPos + 8);

			if (enc == WMS_CDMA_ENC_GSM7) {
				outCount = hdrLen + wmsTsPackGsm7(cdma->userData.bytes,
								  cdma->userData.numCharacters,
								  fillBits,
								  (SceUInt16)(255 - hdrLen -
									      (bitPos >> 3)),
								  &tempPacked[hdrLen]);
				extraPadBits = 3;
			} else if (enc == WMS_CDMA_ENC_ASCII || enc == WMS_CDMA_ENC_IA5) {
				SceUInt16 pBit = (SceUInt16)(hdrLen * 8 + fillBits);
				for (int idx = 0; idx < cdma->userData.dataLength; ++idx) {
					wmsBitPackByte(cdma->userData.bytes[idx], tempPacked, pBit,
						       (SceUInt16)charBits);
					pBit = (SceUInt16)(pBit + charBits);
				}
				outCount = hdrLen + cdma->userData.dataLength;
			} else {
				int padOctets = (enc == WMS_CDMA_ENC_UNICODE) ? (fillBits >> 3) : 0;
				sce_paf_memcpy(&tempPacked[hdrLen + padOctets],
					       cdma->userData.bytes, cdma->userData.dataLength);
				outCount = hdrLen + padOctets + cdma->userData.dataLength;
			}
		} else {
			SceUInt8 numFields =
				(cdma->userData.encoding == WMS_CDMA_ENC_IS91EP)
					? is91Copy.userData.numCharacters
					: cdma->userData.numCharacters;
			wmsBitPackByte(numFields, buf, bitPos, 8);
			bitPos = (SceUInt16)(bitPos + 8);
			if (cdma->userData.encoding == WMS_CDMA_ENC_IS91EP) {
				outCount = is91Copy.userData.dataLength;
			} else if (cdma->userData.encoding == WMS_CDMA_ENC_GSM7) {
				outCount = wmsTsPackGsm7(cdma->userData.bytes,
							 cdma->userData.numCharacters, 0,
							 (SceUInt16)(255 - (bitPos >> 3)),
							 tempPacked);
			} else {
				outCount = cdma->userData.dataLength;
			}
		}

		for (int idx = 0; idx < outCount; ++idx) {
			if (idx >= WMS_MAX_CDMA_USER_DATA)
				return WMS_ERR_INVALID_USER_DATA_SIZE;
			SceUInt8 byteVal = 0;
			if (!cdma->messageId.udhPresent &&
			    cdma->userData.encoding == WMS_CDMA_ENC_IS91EP)
				byteVal = is91Copy.userData.bytes[idx];
			else if (!cdma->messageId.udhPresent &&
				 cdma->userData.encoding != WMS_CDMA_ENC_GSM7)
				byteVal = cdma->userData.bytes[idx];
			else
				byteVal = tempPacked[idx];
			wmsBitPackByte(byteVal, buf, bitPos, (SceUInt16)charBits);
			bitPos = (SceUInt16)(bitPos + charBits);
		}
		if (status != WMS_OK)
			return status;

		if (cdma->messageId.udhPresent)
			bitPos = (SceUInt16)(bitPos + extraPadBits);
		else
			bitPos = (SceUInt16)(bitPos - cdma->userData.paddingBits);

		SceUInt8 rem = (SceUInt8)(bitPos & 7U);
		if (rem != 0) {
			wmsBitPackByte(0, buf, bitPos, (SceUInt16)(8 - rem));
			bitPos = (SceUInt16)(bitPos + (8 - rem));
		}
		SceUInt8 subLen = (SceUInt8)((bitPos >> 3) - bodyStart);
		buf[lenPos] = subLen;
		pos = (SceUInt16)(bodyStart + subLen);
	}

	/* Subparameter 2: User Response Code */
	if ((cdma->mask & 0x0004U) != 0) {
		buf[pos] = 2;
		buf[pos + 1] = 1;
		buf[pos + 2] = cdma->userResponseCode;
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 3: Message Center Time Stamp */
	if ((cdma->mask & 0x0008U) != 0) {
		buf[pos] = 3;
		buf[pos + 1] = 6;
		buf[pos + 2] = cdma->mcTime.year;
		buf[pos + 3] = cdma->mcTime.month;
		buf[pos + 4] = cdma->mcTime.day;
		buf[pos + 5] = cdma->mcTime.hour;
		buf[pos + 6] = cdma->mcTime.minute;
		buf[pos + 7] = cdma->mcTime.second;
		pos = (SceUInt16)(pos + 8);
	}

	/* Subparameter 4: Validity Period Absolute */
	if ((cdma->mask & 0x0010U) != 0) {
		buf[pos] = 4;
		buf[pos + 1] = 6;
		buf[pos + 2] = cdma->validityAbsolute.year;
		buf[pos + 3] = cdma->validityAbsolute.month;
		buf[pos + 4] = cdma->validityAbsolute.day;
		buf[pos + 5] = cdma->validityAbsolute.hour;
		buf[pos + 6] = cdma->validityAbsolute.minute;
		buf[pos + 7] = cdma->validityAbsolute.second;
		pos = (SceUInt16)(pos + 8);
	}

	/* Subparameter 5: Validity Period Relative */
	if ((cdma->mask & 0x0020U) != 0) {
		buf[pos] = 5;
		buf[pos + 1] = 1;
		buf[pos + 2] = wmsTsEncodeValidityRelative(&cdma->validityRelative);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 6: Deferred Delivery Time Absolute */
	if ((cdma->mask & 0x0040U) != 0) {
		buf[pos] = 6;
		buf[pos + 1] = 6;
		buf[pos + 2] = cdma->deferredAbsolute.year;
		buf[pos + 3] = cdma->deferredAbsolute.month;
		buf[pos + 4] = cdma->deferredAbsolute.day;
		buf[pos + 5] = cdma->deferredAbsolute.hour;
		buf[pos + 6] = cdma->deferredAbsolute.minute;
		buf[pos + 7] = cdma->deferredAbsolute.second;
		pos = (SceUInt16)(pos + 8);
	}

	/* Subparameter 7: Deferred Delivery Time Relative */
	if ((cdma->mask & 0x0080U) != 0) {
		buf[pos] = 7;
		buf[pos + 1] = 1;
		buf[pos + 2] = wmsTsEncodeValidityRelative(&cdma->deferredRelative);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 8: Priority Indicator */
	if ((cdma->mask & 0x0100U) != 0 &&
	    (cdma->userData.encoding != WMS_CDMA_ENC_IS91EP ||
	     cdma->userData.is91epType != WMS_IS91EP_VOICE_MAIL)) {
		buf[pos] = 8;
		buf[pos + 1] = 1;
		wmsBitPackByte((SceUInt8)cdma->priority, buf, (SceUInt16)((pos + 2) * 8), 2);
		wmsBitPackByte(0, buf, (SceUInt16)(pos * 8 + 18), 6);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 9: Privacy Indicator */
	if ((cdma->mask & 0x0200U) != 0) {
		buf[pos] = 9;
		buf[pos + 1] = 1;
		wmsBitPackByte((SceUInt8)cdma->privacy, buf, (SceUInt16)((pos + 2) * 8), 2);
		wmsBitPackByte(0, buf, (SceUInt16)(pos * 8 + 18), 6);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 10: Reply Option */
	if ((cdma->mask & 0x0400U) != 0) {
		buf[pos] = 10;
		buf[pos + 1] = 1;
		buf[pos + 2] = 0;
		SceUInt16 bitOff = (SceUInt16)((pos + 2) * 8);
		wmsBitPackByte(cdma->replyOption.userAckRequested ? 1 : 0, buf, bitOff, 1);
		wmsBitPackByte(cdma->replyOption.deliveryAckRequested ? 1 : 0, buf,
			       (SceUInt16)(bitOff + 1), 1);
		wmsBitPackByte(cdma->replyOption.readAckRequested ? 1 : 0, buf,
			       (SceUInt16)(bitOff + 2), 1);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 11: Number of Messages */
	if ((cdma->mask & 0x0800U) != 0 &&
	    (cdma->userData.encoding != WMS_CDMA_ENC_IS91EP ||
	     cdma->userData.is91epType != WMS_IS91EP_VOICE_MAIL)) {
		buf[pos] = 11;
		buf[pos + 1] = 1;
		wmsBitPackByte((SceUInt8)(cdma->numMessages / 10), &buf[pos + 2], 0, 4);
		wmsBitPackByte((SceUInt8)(cdma->numMessages % 10), &buf[pos + 2], 4, 4);
		if (cdma->numMessages / 10 > 9)
			return WMS_ERR_INVALID_PARM_VALUE;
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 12: Alert on Message Delivery */
	if ((cdma->mask & 0x1000U) != 0) {
		buf[pos] = 12;
		if (cdma->alertMode != 0) {
			buf[pos + 1] = 1;
			buf[pos + 2] = 0;
			wmsBitPackByte((SceUInt8)cdma->alertMode, buf, (SceUInt16)((pos + 2) * 8),
				       2);
			pos = (SceUInt16)(pos + 3);
		} else {
			buf[pos + 1] = 0;
			pos = (SceUInt16)(pos + 2);
		}
	}

	/* Subparameter 13: Language Indicator */
	if ((cdma->mask & 0x2000U) != 0) {
		buf[pos] = 13;
		buf[pos + 1] = 1;
		buf[pos + 2] = (SceUInt8)cdma->language;
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 14: Call-Back Number */
	if ((cdma->mask & 0x4000U) != 0 &&
	    (cdma->userData.encoding != WMS_CDMA_ENC_IS91EP ||
	     cdma->userData.is91epType != WMS_IS91EP_CLI_ORDER)) {
		buf[pos] = 14;
		SceUInt16 lenPos = (SceUInt16)(pos + 1);
		SceUInt16 startByte = (SceUInt16)(pos + 2);
		SceUInt16 bitOff = (SceUInt16)(startByte * 8);
		wmsBitPackByte((SceUInt8)cdma->callback.digitMode, buf, bitOff, 1);
		bitOff = (SceUInt16)(bitOff + 1);
		SceUInt16 digitBits = 4;
		if (cdma->callback.digitMode == WMS_DIGIT_MODE_8_BIT) {
			wmsBitPackByte((SceUInt8)cdma->callback.numberType, buf, bitOff, 3);
			wmsBitPackByte((SceUInt8)cdma->callback.numberPlan, buf,
				       (SceUInt16)(bitOff + 3), 4);
			bitOff = (SceUInt16)(bitOff + 7);
			digitBits = 8;
		}
		wmsBitPackByte(cdma->callback.digitCount, buf, bitOff, 8);
		bitOff = (SceUInt16)(bitOff + 8);
		for (int idx = 0; idx < cdma->callback.digitCount; ++idx) {
			wmsBitPackByte(cdma->callback.digits[idx], buf, bitOff, digitBits);
			bitOff = (SceUInt16)(bitOff + digitBits);
		}
		if ((bitOff & 7U) != 0) {
			SceUInt16 pad = (SceUInt16)(8 - (bitOff & 7U));
			wmsBitPackByte(0, buf, bitOff, pad);
			bitOff = (SceUInt16)(bitOff + pad);
		}
		SceUInt8 subLen = (SceUInt8)((bitOff >> 3) - startByte);
		buf[lenPos] = subLen;
		pos = (SceUInt16)(startByte + subLen);
	}

	/* Subparameter 15: Message Display Mode */
	if ((cdma->mask & 0x8000U) != 0) {
		buf[pos] = 15;
		buf[pos + 1] = 1;
		buf[pos + 2] = 0;
		wmsBitPackByte((SceUInt8)cdma->displayMode, buf, (SceUInt16)((pos + 2) * 8), 2);
		pos = (SceUInt16)(pos + 3);
	}

	/* Subparameter 17: Message Deposit Index */
	if ((cdma->mask & 0x40000U) != 0) {
		buf[pos] = 17;
		buf[pos + 1] = 2;
		wmsBitPackWord((SceUInt16)cdma->depositIndex, buf, (SceUInt16)((pos + 2) * 8), 16);
		pos = (SceUInt16)(pos + 4);
	}

	/* Subparameter 20: Message Status */
	if ((cdma->mask & 0x80000U) != 0) {
		buf[pos] = 20;
		buf[pos + 1] = 1;
		wmsBitPackByte((SceUInt8)cdma->deliveryStatus.errorClass, buf,
			       (SceUInt16)((pos + 2) * 8), 2);
		wmsBitPackByte((SceUInt8)cdma->deliveryStatus.statusCode, buf,
			       (SceUInt16)((pos + 2) * 8 + 2), 6);
		pos = (SceUInt16)(pos + 3);
	}

	/* Optional trailing opaque subparameters */
	if ((cdma->mask & 0x40000000U) != 0) {
		if (cdma->otherData == NULL)
			return WMS_ERR_NULL_PTR;
		sce_paf_memcpy(&buf[pos], cdma->otherData, cdma->otherDataMaxLen);
		pos = (SceUInt16)(pos + cdma->otherDataMaxLen);
	}

	outRaw->length = pos;
	if (pos > WMS_MAX_PDU_BYTES)
		return WMS_ERR_INVALID_PARM_SIZE;
	return WMS_OK;
}

/* 0x813F1B14 */
static int wmsTsEncodeCdma(const WmsCdmaBearerData *cdma, WmsRawTsData *outRaw)
{
	if (cdma == NULL || outRaw == NULL)
		return WMS_ERR_NULL_PTR;
	if (cdma->mask == 0) {
		outRaw->length = 0;
		return WMS_OK;
	}
	return wmsTsEncodeCdmaBearerData(cdma, outRaw);
}

/* 0x813F1C80: Decode 3GPP2 C.S0015 CDMA Bearer Data */
static int wmsTsDecodeCdmaBearerData(const WmsRawTsData *raw, bool skipUdh, bool copyOther,
				     WmsCdmaBearerData *outCdma)
{
	if (raw == NULL || outCdma == NULL)
		return WMS_ERR_NULL_PTR;

	outCdma->mask = 0;
	if (raw->length == 0)
		return WMS_OK;

	SceSize totalLen = raw->length < WMS_MAX_PDU_BYTES ? raw->length : WMS_MAX_PDU_BYTES;
	if (totalLen < 2U)
		return WMS_ERR_INVALID_PARM_SIZE;

	const SceUInt8 *buf = raw->bytes;
	SceSize pos = 0;
	int status = WMS_OK;
	SceUInt8 otherBytes = 0;

	while (pos < totalLen && status == WMS_OK) {
		if (pos + 2U > totalLen) {
			status = WMS_ERR_INVALID_PARM_SIZE;
			break;
		}
		SceUInt8 subId = buf[pos];
		SceUInt8 subLen = buf[pos + 1];
		SceSize bodyPos = pos + 2U;
		SceSize nextPos = bodyPos + subLen;
		if (nextPos > totalLen) {
			status = WMS_ERR_INVALID_PARM_SIZE;
			break;
		}

		SceUInt16 bitOff = (SceUInt16)(bodyPos * 8U);
		switch (subId) {
		case 0: /* Message Identifier */
			if ((outCdma->mask & 0x0001U) == 0) {
				outCdma->mask |= 0x0001U;
				if (subLen < 3U)
					return WMS_ERR_INVALID_PARM_SIZE;
				outCdma->messageId.type = wmsBitUnpackByte(buf, bitOff, 4);
				outCdma->messageId.idNumber =
					wmsBitUnpackWord(buf, (SceUInt16)(bitOff + 4), 16);
				outCdma->messageId.udhPresent =
					wmsBitUnpackByte(buf, (SceUInt16)(bitOff + 20), 1) != 0;
			}
			break;
		case 1: /* User Data */
			if ((outCdma->mask & 0x0002U) == 0) {
				outCdma->mask |= 0x0002U;
				outCdma->userData.encoding = wmsBitUnpackByte(buf, bitOff, 5);
				SceUInt16 curBit = (SceUInt16)(bitOff + 5);
				SceUInt16 charBits = 8;
				if (outCdma->userData.encoding == WMS_CDMA_ENC_IS91EP) {
					outCdma->userData.is91epType =
						wmsBitUnpackByte(buf, curBit, 8);
					curBit = (SceUInt16)(curBit + 8);
					charBits = (outCdma->userData.is91epType ==
						    WMS_IS91EP_CLI_ORDER)
							   ? 4
							   : 6;
				}
				outCdma->userData.numCharacters =
					wmsBitUnpackByte(buf, curBit, 8);
				curBit = (SceUInt16)(curBit + 8);
				SceUInt16 endBit = (SceUInt16)(nextPos * 8U);
				SceUInt8 byteIdx = 0;
				while (curBit < endBit) {
					if (byteIdx >= WMS_MAX_CDMA_USER_DATA) {
						status = WMS_ERR_INVALID_USER_DATA_SIZE;
						break;
					}
					outCdma->userData.bytes[byteIdx++] =
						wmsBitUnpackByte(buf, curBit, charBits);
					curBit = (SceUInt16)(curBit + charBits);
				}
				if (status == WMS_OK) {
					SceUInt8 chars = outCdma->userData.numCharacters;
					if (outCdma->userData.encoding == WMS_CDMA_ENC_IS91EP) {
						outCdma->userData.dataLength = chars;
						if (byteIdx < chars)
							status = WMS_ERR_INVALID_USER_DATA_SIZE;
					} else if (outCdma->userData.encoding == WMS_CDMA_ENC_ASCII ||
						   outCdma->userData.encoding == WMS_CDMA_ENC_IA5) {
						int totalBits = 7 * chars;
						outCdma->userData.dataLength =
							(SceUInt8)((totalBits + 7) >> 3);
						outCdma->userData.paddingBits =
							(SceUInt8)(((totalBits + 7) & ~7) -
								   totalBits);
					} else if (outCdma->userData.encoding ==
						   WMS_CDMA_ENC_UNICODE) {
						outCdma->userData.paddingBits = 0;
						outCdma->userData.dataLength = (SceUInt8)(2 * chars);
					} else {
						outCdma->userData.dataLength =
							byteIdx > 0 ? (SceUInt8)(byteIdx - 1) : 0;
						outCdma->userData.paddingBits = 0;
					}
				}
			}
			break;
		case 2: /* User Response Code */
			if ((outCdma->mask & 0x0004U) == 0) {
				outCdma->mask |= 0x0004U;
				if (subLen == 0)
					status = WMS_ERR_INVALID_PARM_SIZE;
				else
					outCdma->userResponseCode = buf[bodyPos];
			}
			break;
		case 3: /* Message Center Time Stamp */
			if ((outCdma->mask & 0x0008U) == 0 && subLen >= 6U) {
				outCdma->mask |= 0x0008U;
				outCdma->mcTime.year = buf[bodyPos];
				outCdma->mcTime.month = buf[bodyPos + 1];
				outCdma->mcTime.day = buf[bodyPos + 2];
				outCdma->mcTime.hour = buf[bodyPos + 3];
				outCdma->mcTime.minute = buf[bodyPos + 4];
				outCdma->mcTime.second = buf[bodyPos + 5];
			}
			break;
		case 4: /* Validity Period Absolute */
			if ((outCdma->mask & 0x0010U) == 0 && subLen >= 6U) {
				outCdma->mask |= 0x0010U;
				outCdma->validityAbsolute.year = buf[bodyPos];
				outCdma->validityAbsolute.month = buf[bodyPos + 1];
				outCdma->validityAbsolute.day = buf[bodyPos + 2];
				outCdma->validityAbsolute.hour = buf[bodyPos + 3];
				outCdma->validityAbsolute.minute = buf[bodyPos + 4];
				outCdma->validityAbsolute.second = buf[bodyPos + 5];
			}
			break;
		case 5: /* Validity Period Relative */
			if ((outCdma->mask & 0x0020U) == 0 && subLen >= 1U) {
				outCdma->mask |= 0x0020U;
				wmsTsDecodeValidityRelative(buf[bodyPos],
							    &outCdma->validityRelative);
			}
			break;
		case 6: /* Deferred Delivery Time Absolute */
			if ((outCdma->mask & 0x0040U) == 0 && subLen >= 6U) {
				outCdma->mask |= 0x0040U;
				outCdma->deferredAbsolute.year = buf[bodyPos];
				outCdma->deferredAbsolute.month = buf[bodyPos + 1];
				outCdma->deferredAbsolute.day = buf[bodyPos + 2];
				outCdma->deferredAbsolute.hour = buf[bodyPos + 3];
				outCdma->deferredAbsolute.minute = buf[bodyPos + 4];
				outCdma->deferredAbsolute.second = buf[bodyPos + 5];
			}
			break;
		case 7: /* Deferred Delivery Time Relative */
			if ((outCdma->mask & 0x0080U) == 0 && subLen >= 1U) {
				outCdma->mask |= 0x0080U;
				wmsTsDecodeValidityRelative(buf[bodyPos],
							    &outCdma->deferredRelative);
			}
			break;
		case 8: /* Priority Indicator */
			if ((outCdma->mask & 0x0100U) == 0) {
				if (subLen == 0)
					return WMS_ERR_INVALID_PARM_SIZE;
				outCdma->mask |= 0x0100U;
				outCdma->priority = wmsBitUnpackByte(buf, bitOff, 2);
			}
			break;
		case 9: /* Privacy Indicator */
			if ((outCdma->mask & 0x0200U) == 0) {
				outCdma->mask |= 0x0200U;
				outCdma->privacy = wmsBitUnpackByte(buf, bitOff, 2);
			}
			break;
		case 10: /* Reply Option */
			if ((outCdma->mask & 0x0400U) == 0) {
				outCdma->mask |= 0x0400U;
				outCdma->replyOption.userAckRequested =
					wmsBitUnpackByte(buf, bitOff, 1) != 0;
				outCdma->replyOption.deliveryAckRequested =
					wmsBitUnpackByte(buf, (SceUInt16)(bitOff + 1), 1) != 0;
				outCdma->replyOption.readAckRequested =
					wmsBitUnpackByte(buf, (SceUInt16)(bitOff + 2), 1) != 0;
			}
			break;
		case 11: /* Number of Messages */
			if ((outCdma->mask & 0x0800U) == 0) {
				outCdma->mask |= 0x0800U;
				if (subLen == 0) {
					status = WMS_ERR_INVALID_PARM_SIZE;
				} else {
					SceUInt8 hi = wmsBitUnpackByte(buf, bitOff, 4);
					SceUInt8 lo =
						wmsBitUnpackByte(buf, (SceUInt16)(bitOff + 4), 4);
					if (hi >= 10U || lo >= 10U)
						status = WMS_ERR_INVALID_PARM_VALUE;
					else
						outCdma->numMessages = (SceUInt8)(10U * hi + lo);
				}
			}
			break;
		case 12: /* Alert */
			if ((outCdma->mask & 0x1000U) == 0) {
				outCdma->mask |= 0x1000U;
				outCdma->alertMode =
					(subLen != 0) ? wmsBitUnpackByte(buf, bitOff, 2) : 0;
			}
			break;
		case 13: /* Language Indicator */
			if ((outCdma->mask & 0x2000U) == 0) {
				outCdma->mask |= 0x2000U;
				if (subLen == 0)
					status = WMS_ERR_INVALID_PARM_SIZE;
				else
					outCdma->language = buf[bodyPos];
			}
			break;
		case 14: /* Call-Back Number */
			if ((outCdma->mask & 0x4000U) == 0) {
				outCdma->mask |= 0x4000U;
				int mode = wmsBitUnpackByte(buf, bitOff, 1);
				outCdma->callback.digitMode = mode;
				SceUInt16 curBit = (SceUInt16)(bitOff + 1);
				SceUInt16 digitBits = 4;
				if (mode == WMS_DIGIT_MODE_8_BIT) {
					outCdma->callback.numberType =
						wmsBitUnpackByte(buf, curBit, 3);
					outCdma->callback.numberPlan =
						wmsBitUnpackByte(buf, (SceUInt16)(curBit + 3), 4);
					curBit = (SceUInt16)(curBit + 7);
					digitBits = 8;
				} else {
					outCdma->callback.numberType = 0;
					outCdma->callback.numberPlan = 0;
				}
				SceUInt8 digits = wmsBitUnpackByte(buf, curBit, 8);
				curBit = (SceUInt16)(curBit + 8);
				if (digits > WMS_MAX_ADDRESS_DIGITS)
					digits = WMS_MAX_ADDRESS_DIGITS;
				outCdma->callback.digitCount = digits;
				for (SceUInt8 idx = 0; idx < digits; ++idx) {
					outCdma->callback.digits[idx] =
						wmsBitUnpackByte(buf, curBit, digitBits);
					curBit = (SceUInt16)(curBit + digitBits);
				}
			}
			break;
		case 15: /* Display Mode */
			if ((outCdma->mask & 0x8000U) == 0) {
				outCdma->mask |= 0x8000U;
				if (subLen == 0) {
					status = WMS_ERR_INVALID_PARM_SIZE;
				} else {
					outCdma->displayMode = wmsBitUnpackByte(buf, bitOff, 2);
				}
			}
			break;
		case 17: /* Deposit Index */
			if ((outCdma->mask & 0x40000U) == 0) {
				outCdma->mask |= 0x40000U;
				outCdma->depositIndex = wmsBitUnpackWord(buf, bitOff, 16);
			}
			break;
		case 20: /* Message Status */
			if ((outCdma->mask & 0x80000U) == 0) {
				outCdma->mask |= 0x80000U;
				outCdma->deliveryStatus.errorClass =
					wmsBitUnpackByte(buf, bitOff, 2);
				outCdma->deliveryStatus.statusCode =
					wmsBitUnpackByte(buf, (SceUInt16)(bitOff + 2), 6);
			}
			break;
		default:
			outCdma->mask |= 0x40000000U;
			if (copyOther && outCdma->otherData != NULL &&
			    outCdma->otherDataMaxLen >= otherBytes + subLen + 2) {
				outCdma->otherData[otherBytes] = subId;
				outCdma->otherData[otherBytes + 1] = subLen;
				sce_paf_memcpy(&outCdma->otherData[otherBytes + 2], &buf[bodyPos],
					       subLen);
			}
			otherBytes = (SceUInt8)(otherBytes + subLen + 2);
			break;
		}
		pos = nextPos;
	}

	outCdma->otherDataLen = otherBytes;

	if (status == WMS_OK && (outCdma->mask & 0x0002U) != 0 &&
	    outCdma->userData.encoding == WMS_CDMA_ENC_IS91EP) {
		WmsCdmaBearerData saved = *outCdma;
		status = wmsTsDecodeCdmaIs91(saved.userData.is91epType,
					     saved.userData.numCharacters, saved.userData.bytes,
					     outCdma);
		if (status != WMS_OK)
			return WMS_ERR_INVALID_FORMAT;
		if ((saved.mask & 0x0001U) != 0) {
			outCdma->mask |= 0x0001U;
			outCdma->messageId = saved.messageId;
		}
	}

	outCdma->userData.headerCount = 0;
	if (!skipUdh && outCdma->messageId.udhPresent) {
		WmsCdmaUserData savedUd = outCdma->userData;
		int udhl = wmsTsDecodeUserDataHeaders(savedUd.numCharacters, savedUd.bytes,
						      &outCdma->userData.headerCount,
						      outCdma->userData.headers);
		if (udhl + 1 > savedUd.dataLength)
			return WMS_ERR_INVALID_PARM_SIZE;

		if (savedUd.encoding == WMS_CDMA_ENC_GSM7) {
			int shift = 0;
			SceUInt8 septets = savedUd.dataLength;
			if (udhl > 0) {
				int remBits = 7 * savedUd.numCharacters - 8 * udhl - 8;
				shift = remBits % 7;
				if (shift != 0)
					shift = 8 - shift;
				septets = (SceUInt8)(remBits / 7);
			}
			outCdma->userData.dataLength = septets;
			outCdma->userData.numCharacters = septets;
			wmsTsUnpackGsm7(&savedUd.bytes[udhl + 1], septets, WMS_MAX_CDMA_USER_DATA,
					(SceInt16)shift, outCdma->userData.bytes);
		} else if (savedUd.encoding == WMS_CDMA_ENC_ASCII ||
			   savedUd.encoding == WMS_CDMA_ENC_IA5) {
			int remBits = 7 * savedUd.numCharacters - 8 * udhl - 8;
			int pad = (udhl > 0) ? (remBits % 7) : 0;
			if (pad == 7)
				pad = 0;
			SceUInt8 chars = (SceUInt8)(remBits / 7);
			outCdma->userData.paddingBits = (SceUInt8)pad;
			outCdma->userData.dataLength = chars;
			outCdma->userData.numCharacters = chars;
			SceUInt16 startBit = (SceUInt16)(8 * (udhl + 1) + pad);
			for (SceUInt8 idx = 0; idx < chars; ++idx) {
				outCdma->userData.bytes[idx] = wmsBitUnpackByte(
					savedUd.bytes, (SceUInt16)(startBit + idx * 8), 8);
			}
		} else if (savedUd.encoding == WMS_CDMA_ENC_UNICODE) {
			int padBits = ((udhl + 1) % 2 != 0) ? 8 : 0;
			int skipBytes = udhl + 1 + (padBits >> 3);
			SceUInt8 remain = (SceUInt8)(savedUd.dataLength - skipBytes);
			outCdma->userData.dataLength = remain;
			outCdma->userData.paddingBits = (SceUInt8)padBits;
			outCdma->userData.numCharacters = (SceUInt8)(remain >> 1);
			sce_paf_memcpy(outCdma->userData.bytes, &savedUd.bytes[skipBytes], remain);
		} else {
			SceUInt8 remain = (SceUInt8)(savedUd.dataLength - udhl - 1);
			if (remain > 228U)
				remain = 228U;
			outCdma->userData.dataLength = remain;
			outCdma->userData.numCharacters =
				(SceUInt8)(savedUd.numCharacters - udhl - 1);
			sce_paf_memcpy(outCdma->userData.bytes, &savedUd.bytes[udhl + 1], remain);
		}
	} else if (!skipUdh && outCdma->userData.encoding == WMS_CDMA_ENC_GSM7) {
		WmsCdmaUserData savedUd = outCdma->userData;
		outCdma->userData.dataLength = savedUd.numCharacters;
		wmsTsUnpackGsm7(savedUd.bytes, savedUd.numCharacters, 255, 0,
				outCdma->userData.bytes);
	}
	return status;
}

/* 0x813F2A24 */
static int wmsTsDecodeCdma(const WmsRawTsData *raw, WmsCdmaBearerData *outCdma)
{
	if (raw == NULL || outCdma == NULL)
		return WMS_ERR_NULL_PTR;

	outCdma->mask = 0;
	if (raw->length == 0)
		return WMS_OK;

	switch (raw->format) {
	case WMS_FORMAT_CDMA:
	case WMS_FORMAT_ANALOG_AWISMS:
		return wmsTsDecodeCdmaBearerData(raw, false, false, outCdma);
	case WMS_FORMAT_ANALOG_CLI:
	case WMS_FORMAT_ANALOG_VOICE_MAIL:
	case WMS_FORMAT_ANALOG_SMS: {
		int is91Type = WMS_IS91EP_SHORT_MESSAGE;
		if (raw->format == WMS_FORMAT_ANALOG_CLI)
			is91Type = WMS_IS91EP_CLI_ORDER;
		else if (raw->format == WMS_FORMAT_ANALOG_VOICE_MAIL)
			is91Type = WMS_IS91EP_VOICE_MAIL;
		SceUInt8 len = raw->length < 255U ? (SceUInt8)raw->length : 255U;
		return wmsTsDecodeCdmaIs91(is91Type, len, raw->bytes, outCdma);
	}
	case WMS_FORMAT_MWI:
		outCdma->mask = 0x0800U;
		outCdma->numMessages = raw->bytes[0];
		return WMS_OK;
	default:
		return WMS_ERR_UNSUPPORTED;
	}
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Public LibWMS functions
 * -------------------------------------------------------------------------- */

/* 0x813EDED2 */
int wmsTsBcdToAscii(const SceUInt8 *bcdDigits, int count, char *outAscii)
{
	static const char kBcdMap[15] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'a', 'b', 'c'
	};
	for (int idx = 0; idx < count; ++idx) {
		SceUInt8 digit = bcdDigits[idx];
		if (digit >= 15U)
			return 0;
		outAscii[idx] = kBcdMap[digit];
	}
	return 1;
}

/* 0x813EDF76 */
int wmsTsIsValidAsciiDigits(const char *digits, int count)
{
	for (int idx = 0; idx < count; ++idx) {
		int ch = (unsigned char)digits[idx];
		if ((sce_paf_look_ctype_table(ch) & 4) == 0 && ch != '#' && ch != '*' &&
		    ch != 'a' && ch != 'b' && ch != 'c')
			return 0;
	}
	return 1;
}

/* 0x813EDFCA */
int wmsTsPackGsm7(const SceUInt8 *septets, SceUInt16 septetCount, SceUInt16 shiftBits,
		  SceUInt16 maxOctets, SceUInt8 *outOctets)
{
	if (septets == NULL || outOctets == NULL)
		return 0;

	SceUInt16 srcIdx = 0;
	SceUInt16 dstIdx = 0;
	SceUInt16 shift = (SceUInt16)(shiftBits % 7U);

	if (shift != 0) {
		outOctets[0] |= (SceUInt8)(septets[0] << shift);
		shift = (SceUInt16)(8U - shift);
		dstIdx = 1;
		if (shift == 7U) {
			shift = 0;
			srcIdx = 1;
		}
	}

	while (dstIdx < maxOctets && srcIdx < septetCount) {
		SceUInt8 lo = (SceUInt8)(septets[srcIdx] >> shift);
		outOctets[dstIdx] = lo;
		SceUInt16 nextSrc = (SceUInt16)(srcIdx + 1U);
		if (nextSrc < septetCount) {
			SceUInt8 hi = (SceUInt8)(septets[nextSrc] << (7U - shift));
			outOctets[dstIdx] = (SceUInt8)(lo | hi);
			++shift;
			if (shift == 7U) {
				shift = 0;
				++nextSrc;
			}
		}
		++dstIdx;
		srcIdx = nextSrc;
	}
	return dstIdx;
}

/* 0x813EE13E */
int wmsTsUnpackGsm7(const SceUInt8 *octets, SceUInt8 septetCount, SceUInt8 maxSeptets,
		    SceInt16 shiftBits, SceUInt8 *outSeptets)
{
	if (octets == NULL || outSeptets == NULL)
		return 0;

	SceUInt16 dstIdx = 0;
	SceUInt16 srcIdx = (shiftBits != 0) ? 1 : 0;
	SceInt16 shift = shiftBits;

	if (shift == 7) {
		shift = 0;
		outSeptets[0] = (SceUInt8)(octets[0] >> 1);
		dstIdx = 1;
	}

	while (dstIdx < maxSeptets && dstIdx < septetCount) {
		SceUInt8 val = (SceUInt8)((octets[srcIdx] << shift) & 0x7FU);
		if (srcIdx != 0)
			val |= (SceUInt8)(octets[srcIdx - 1] >> (8 - shift));
		outSeptets[dstIdx++] = val;
		++shift;
		if (shift == 7) {
			shift = 0;
			if (dstIdx >= maxSeptets)
				return (SceUInt8)srcIdx;
			outSeptets[dstIdx++] = (SceUInt8)(octets[srcIdx] >> 1);
		}
		++srcIdx;
	}
	return (SceUInt8)srcIdx;
}

/* 0x813EE078 */
int wmsTsEncodeAddress(const WmsAddress *address, SceUInt8 *outBytes)
{
	if (address->digitCount > 20U)
		return 0;

	if (address->numberType == WMS_NUMBER_ALPHANUMERIC)
		outBytes[0] = (SceUInt8)((7U * address->digitCount + 3U) >> 2);
	else
		outBytes[0] = address->digitCount;

	outBytes[1] = (SceUInt8)(0x80U | ((address->numberType & 0x07) << 4) |
				 (address->numberPlan & 0x0F));

	if (address->numberType == WMS_NUMBER_ALPHANUMERIC)
		return (SceUInt8)(wmsTsPackGsm7(address->digits, address->digitCount, 0, 20,
						&outBytes[2]) +
				  2);

	int pos = 2;
	for (SceUInt8 idx = 0; idx < address->digitCount; idx = (SceUInt8)(idx + 2)) {
		SceUInt8 lo = (SceUInt8)(address->digits[idx] & 0x0FU);
		SceUInt8 hi = (idx + 1U < address->digitCount)
				      ? (SceUInt8)((address->digits[idx + 1] & 0x0FU) << 4)
				      : 0xF0U;
		outBytes[pos++] = (SceUInt8)(lo | hi);
	}
	return pos;
}

/* 0x813EE124 */
int wmsTsEncodeScAddress(const WmsAddress *address, SceUInt8 *outBytes)
{
	int total = wmsTsEncodeAddress(address, outBytes);
	SceUInt8 octets = (SceUInt8)(total - 1);
	outBytes[0] = octets;
	if (octets < 2U || octets > 11U)
		return 0;
	return total;
}

/* 0x813EE1DA */
int wmsTsDecodeAddress(const SceUInt8 *bytes, WmsAddress *outAddress)
{
	SceUInt8 addrLen = bytes[0];
	if (addrLen > 20U)
		return 0;

	outAddress->digitCount = addrLen;
	outAddress->digitMode = WMS_DIGIT_MODE_4_BIT;
	outAddress->numberType = (bytes[1] >> 4) & 0x07;
	outAddress->numberPlan = bytes[1] & 0x0F;

	if (outAddress->numberType == WMS_NUMBER_ALPHANUMERIC) {
		outAddress->digitMode = WMS_DIGIT_MODE_8_BIT;
		SceUInt8 charCount = (SceUInt8)((4U * addrLen) / 7U);
		outAddress->digitCount = charCount;
		wmsTsUnpackGsm7(&bytes[2], charCount, 20, 0, outAddress->digits);
		return (SceUInt8)(((addrLen + 1U) >> 1) + 2U);
	}

	int pos = 2;
	for (SceUInt8 idx = 0; idx < addrLen; idx = (SceUInt8)(idx + 2)) {
		outAddress->digits[idx] = (SceUInt8)(bytes[pos] & 0x0FU);
		outAddress->digits[idx + 1] = (SceUInt8)(bytes[pos] >> 4);
		++pos;
	}
	return pos;
}

/* 0x813F0C2A */
int wmsTsDecodeScAddress(const SceUInt8 *bytes, WmsAddress *outAddress)
{
	if (bytes == NULL || outAddress == NULL)
		return 0;
	SceUInt8 octets = bytes[0];
	if (octets < 2U || octets > 11U)
		return 0;

	SceUInt8 tmp[12];
	sce_paf_memcpy(tmp, bytes, (SceSize)(octets + 1U));
	SceUInt8 digits = (SceUInt8)(2U * octets - 2U);
	if ((bytes[octets] & 0xF0U) == 0xF0U)
		digits = (SceUInt8)(digits - 1U);
	tmp[0] = digits;
	return wmsTsDecodeAddress(tmp, outAddress);
}

/* 0x813EF580 */
int wmsTsEncode(const WmsClientTsData *clientData, WmsRawTsData *outRawData)
{
	if (clientData == NULL || outRawData == NULL)
		return WMS_ERR_NULL_PTR;

	ensureWmsInitialized();
	int status = WMS_ERR_UNSUPPORTED;

	switch (clientData->format) {
	case WMS_FORMAT_CDMA:
	case WMS_FORMAT_ANALOG_CLI:
	case WMS_FORMAT_ANALOG_VOICE_MAIL:
	case WMS_FORMAT_ANALOG_SMS:
	case WMS_FORMAT_ANALOG_AWISMS:
	case WMS_FORMAT_MWI:
		status = wmsTsEncodeCdma(&clientData->u.cdma, outRawData);
		break;
	case WMS_FORMAT_GW_PP:
		outRawData->tpduType = clientData->u.gwPp.tpduType;
		switch (clientData->u.gwPp.tpduType) {
		case WMS_TPDU_DELIVER:
			status = wmsTsEncodeDeliver(&clientData->u.gwPp.u.deliver, outRawData);
			break;
		case WMS_TPDU_DELIVER_REPORT_ACK:
			status = wmsTsEncodeDeliverReportAck(
				&clientData->u.gwPp.u.deliverReportAck, outRawData);
			break;
		case WMS_TPDU_DELIVER_REPORT_ERROR:
			status = wmsTsEncodeDeliverReportError(
				&clientData->u.gwPp.u.deliverReportError, outRawData);
			break;
		case WMS_TPDU_SUBMIT:
			status = wmsTsEncodeSubmit(&clientData->u.gwPp.u.submit, outRawData);
			break;
		case WMS_TPDU_SUBMIT_REPORT_ACK:
			status = wmsTsEncodeSubmitReportAck(
				&clientData->u.gwPp.u.submitReportAck, outRawData);
			break;
		case WMS_TPDU_SUBMIT_REPORT_ERROR:
			status = wmsTsEncodeSubmitReportError(
				&clientData->u.gwPp.u.submitReportError, outRawData);
			break;
		case WMS_TPDU_STATUS_REPORT:
			status = wmsTsEncodeStatusReport(&clientData->u.gwPp.u.statusReport,
							 outRawData);
			break;
		case WMS_TPDU_COMMAND:
			status = wmsTsEncodeCommand(&clientData->u.gwPp.u.command, outRawData);
			break;
		default:
			status = WMS_ERR_INVALID_TPDU_TYPE;
			break;
		}
		break;
	default:
		status = WMS_ERR_UNSUPPORTED;
		break;
	}
	outRawData->format = clientData->format;
	return status;
}

/* 0x813F0B8A */
int wmsTsDecode(const WmsRawTsData *rawData, WmsClientTsData *outClientData)
{
	if (rawData == NULL || outClientData == NULL)
		return WMS_ERR_NULL_PTR;

	ensureWmsInitialized();
	int status = WMS_ERR_UNSUPPORTED;

	switch (rawData->format) {
	case WMS_FORMAT_CDMA:
	case WMS_FORMAT_ANALOG_CLI:
	case WMS_FORMAT_ANALOG_VOICE_MAIL:
	case WMS_FORMAT_ANALOG_SMS:
	case WMS_FORMAT_ANALOG_AWISMS:
	case WMS_FORMAT_MWI:
		status = wmsTsDecodeCdma(rawData, &outClientData->u.cdma);
		break;
	case WMS_FORMAT_GW_PP:
		outClientData->u.gwPp.tpduType = rawData->tpduType;
		switch (rawData->tpduType) {
		case WMS_TPDU_DELIVER:
			status = wmsTsDecodeDeliver(rawData, &outClientData->u.gwPp.u.deliver);
			break;
		case WMS_TPDU_DELIVER_REPORT_ACK:
			status = wmsTsDecodeDeliverReportAck(
				rawData, &outClientData->u.gwPp.u.deliverReportAck);
			break;
		case WMS_TPDU_DELIVER_REPORT_ERROR:
			status = wmsTsDecodeDeliverReportError(
				rawData, &outClientData->u.gwPp.u.deliverReportError);
			break;
		case WMS_TPDU_SUBMIT:
			status = wmsTsDecodeSubmit(rawData, &outClientData->u.gwPp.u.submit);
			break;
		case WMS_TPDU_SUBMIT_REPORT_ACK:
			status = wmsTsDecodeSubmitReportAck(
				rawData, &outClientData->u.gwPp.u.submitReportAck);
			break;
		case WMS_TPDU_SUBMIT_REPORT_ERROR:
			status = wmsTsDecodeSubmitReportError(
				rawData, &outClientData->u.gwPp.u.submitReportError);
			break;
		case WMS_TPDU_STATUS_REPORT:
			status = wmsTsDecodeStatusReport(rawData,
							 &outClientData->u.gwPp.u.statusReport);
			break;
		case WMS_TPDU_COMMAND:
			status = wmsTsDecodeCommand(rawData, &outClientData->u.gwPp.u.command);
			break;
		default:
			status = WMS_ERR_INVALID_TPDU_TYPE;
			break;
		}
		break;
	default:
		status = WMS_ERR_UNSUPPORTED;
		break;
	}
	outClientData->format = rawData->format;
	return status;
}

/* 0x813F3AC4 */
void wmsBitPackByte(SceUInt8 value, SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength)
{
	int bitInByte = bitOffset & 7;
	int endByte = (bitOffset + bitLength - 1) / 8;
	SceUInt8 mask = (SceUInt8)(0xFFU >> bitInByte);

	if ((SceUInt16)(8 - bitInByte) < bitLength) {
		int spillBits = bitLength - (8 - bitInByte);
		buffer[endByte - 1] =
			(SceUInt8)((buffer[endByte - 1] & ~mask) | ((value >> spillBits) & mask));
		SceUInt8 hiMask = (SceUInt8)(0xFFU << (8 - spillBits));
		buffer[endByte] =
			(SceUInt8)((buffer[endByte] & ~hiMask) | ((value << (8 - spillBits)) & hiMask));
	} else {
		SceUInt8 innerMask = (SceUInt8)((0xFFU << (8 - (bitInByte + bitLength))) & mask);
		int shift = 8 - bitInByte - bitLength;
		buffer[endByte] =
			(SceUInt8)((buffer[endByte] & ~innerMask) | ((value << shift) & innerMask));
	}
}

/* 0x813F3B72 */
void wmsBitPackWord(SceUInt16 value, SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength)
{
	int endBit = bitOffset + bitLength - 1;
	int byteIdx = endBit / 8;
	int bitInEnd = endBit % 8;
	int startBitInEnd = (bitLength <= (SceUInt16)bitInEnd) ? (bitInEnd - bitLength + 1) : 0;

	SceUInt8 mask = (SceUInt8)((0xFFU >> startBitInEnd) & (0xFFU << (7 - bitInEnd)));
	buffer[byteIdx] =
		(SceUInt8)((buffer[byteIdx] & ~mask) | ((value << (7 - bitInEnd)) & mask));

	int written = bitInEnd - startBitInEnd + 1;
	SceUInt16 remVal = (SceUInt16)(value >> written);
	if ((int)bitLength > written) {
		int remBits = bitLength - written;
		int curByte = byteIdx - 1;
		while (remBits >= 8) {
			buffer[curByte--] = (SceUInt8)(remVal & 0xFFU);
			remVal >>= 8;
			remBits -= 8;
		}
		if (remBits > 0) {
			SceUInt8 loMask = (SceUInt8)((1U << remBits) - 1U);
			buffer[curByte] =
				(SceUInt8)((buffer[curByte] & ~loMask) | (remVal & loMask));
		}
	}
}

/* 0x813F3C20 */
SceUInt8 wmsBitUnpackByte(const SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength)
{
	int byteIdx = bitOffset >> 3;
	int bitInByte = bitOffset & 7;
	SceUInt8 firstByte = buffer[byteIdx];
	int totalBits = bitInByte + bitLength;

	if (totalBits >= 8) {
		SceUInt8 hi = firstByte;
		if (bitInByte != 0)
			hi = (SceUInt8)(firstByte & (0xFFU >> bitInByte));
		int spill = totalBits - 8;
		if (spill != 0)
			return (SceUInt8)((hi << spill) | (buffer[byteIdx + 1] >> (8 - spill)));
		return hi;
	}
	int rightShift = 8 - totalBits;
	SceUInt8 masked = (SceUInt8)(firstByte & ((0xFFU << (8 - (bitLength & 7))) >> bitInByte));
	return (SceUInt8)(masked >> rightShift);
}

/* 0x813F3CDA */
SceUInt16 wmsBitUnpackWord(const SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength)
{
	int byteIdx = bitOffset >> 3;
	int bitInByte = bitOffset & 7;
	SceUInt8 firstByte = buffer[byteIdx];
	int totalBits = bitInByte + bitLength;

	if (totalBits >= 8) {
		SceUInt16 acc = firstByte;
		if (bitInByte != 0)
			acc = (SceUInt16)(firstByte & (0xFFU >> bitInByte));
		int remBits = totalBits - 8;
		int curByte = byteIdx + 1;
		while (remBits >= 8) {
			acc = (SceUInt16)((acc << 8) | buffer[curByte++]);
			remBits -= 8;
		}
		if (remBits > 0)
			acc = (SceUInt16)((acc << remBits) | (buffer[curByte] >> (8 - remBits)));
		return acc;
	}
	int rightShift = 8 - totalBits;
	SceUInt8 masked = (SceUInt8)(firstByte & ((0xFFU << (8 - (bitLength & 7))) >> bitInByte));
	return (SceUInt16)(masked >> rightShift);
}

} /* namespace telephony */
