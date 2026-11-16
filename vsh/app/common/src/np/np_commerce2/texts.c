/*
 * SceNpCommerce2: results made of texts and values (texts.c).
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

/* a result whose records are texts: 0x4f, 0x50, 0x51, 0x52, 0x36, 0x54 */
int npCommerce2InitTextsResult0x4f(NpCommerce2Texts6Result *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt16 found;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x74);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		result->textA = NP_COMMERCE2_EMPTY_TEXT(result);
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x4F, 0, &found);
		if (ret >= 0 && found != 0)
			result->textA = records + ret;
		if (ret >= 0) {
			result->textB = NP_COMMERCE2_EMPTY_TEXT(result);
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x50, 0, &found);
			if (ret >= 0 && found != 0)
				result->textB = records + ret;
			if (ret >= 0) {
				result->textC = NP_COMMERCE2_EMPTY_TEXT(result);
				ret = npCommerce2FindRecordWhole(records, dataLength, 0x51, 0, &found);
				if (ret >= 0 && found != 0)
					result->textC = records + ret;
				if (ret >= 0) {
					result->textD = NP_COMMERCE2_EMPTY_TEXT(result);
					ret = npCommerce2FindRecordWhole(records, dataLength, 0x52, 0, &found);
					if (ret >= 0 && found != 0)
						result->textD = records + ret;
					if (ret >= 0) {
						result->textE = NP_COMMERCE2_EMPTY_TEXT(result);
						ret = npCommerce2FindRecordWhole(records, dataLength, 0x36, 0, &found);
						if (ret >= 0 && found != 0)
							result->textE = records + ret;
						if (ret >= 0) {
							result->textF = NP_COMMERCE2_EMPTY_TEXT(result);
							ret = npCommerce2FindRecordWhole(records, dataLength, 0x54, 0, &found);
							if (ret >= 0 && found != 0)
								result->textF = records + ret;
							if (ret >= 0)
								ret = 0;
						}
					}
				}
			}
		}
	}
	return ret;
}

/* a result whose records are texts: 0x4f, 0x52, 0x36 */
int npCommerce2InitTextsResult0x4f3(NpCommerce2Texts3Result *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt16 found;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x68);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		result->textA = NP_COMMERCE2_EMPTY_TEXT(result);
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x4F, 0, &found);
		if (ret >= 0 && found != 0)
			result->textA = records + ret;
		if (ret >= 0) {
			result->textB = NP_COMMERCE2_EMPTY_TEXT(result);
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x52, 0, &found);
			if (ret >= 0 && found != 0)
				result->textB = records + ret;
			if (ret >= 0) {
				result->textC = NP_COMMERCE2_EMPTY_TEXT(result);
				ret = npCommerce2FindRecordWhole(records, dataLength, 0x36, 0, &found);
				if (ret >= 0 && found != 0)
					result->textC = records + ret;
				if (ret >= 0)
					ret = 0;
			}
		}
	}
	return ret;
}

/* a result with only the header fields (the copy in another of Sony's source files) */
int npCommerce2InitBareResult_81012B60(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x5C);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.data_size = savedLength;
		ret = 0;
	}
	return ret;
}

/* a result with the values of tags 0x15 and 0x2d and the raw 64-bit value of tag 3, all required */
int npCommerce2InitResult0x15x2d3(NpCommerce2ValuesResult *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 net;
	SceUInt64 net64;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x70);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&net, records + ret, 4);
			result->valueA = sceNetNtohl(net);
		}
		if (ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x2D, 4, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net, records + ret, 4);
				net = sceNetNtohl(net);
				sceClibMemcpy_safe(&result->valueB, &net, 4);
			}
			if (ret >= 0) {
				ret = npCommerce2FindRecordWhole(records, dataLength, 3, 8, NULL);
				if (ret >= 0) {
					sceClibMemcpy_safe(&net64, records + ret, 8);
					result->valueC = sceNetNtohll(net64);
				}
				if (ret >= 0)
					ret = 0;
			}
		}
	}
	return ret;
}

/* a result with the one value of tag 0x31 */
int npCommerce2InitResult0x31(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 net;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x60);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x31, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&net, records + ret, 4);
			result->value[0] = sceNetNtohl(net);
		}
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}

/* a result with only the header fields (the copy in a third of Sony's source files) */
int npCommerce2InitBareResult_81012CF0(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x5C);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.data_size = savedLength;
		ret = 0;
	}
	return ret;
}

/* the record of tag 0x34: two values, the text at 8 and the text of tag 0x13 */
int npCommerce2ReadValueText0x34(const NpCommerce2Item *result, NpCommerce2ValueTextItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt32 net;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x34, 0x1D, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x6C);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net, record, 4);
		item->valueA = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 4, 4);
		item->valueB = sceNetNtohl(net);
		item->textA = record + 8;
		rest = record + 25;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 25, 0x13, 0, &found);
		if (ret >= 0 && found != 0)
			item->textB = rest + ret;
	}
	return ret;
}

/* the record of tag 0x32: two values */
int npCommerce2ReadItem0x32(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	const char *record;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x32, 8, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x64);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net, record, 4);
		item->value[0] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 4, 4);
		item->value[1] = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)record + 8;
	}
	return ret;
}

/* the record of tag 0x33 and the count of records of tag 0x7a in it */
int npCommerce2ReadCount0x33(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x33, 4, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x60);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		item->commonData.data = (const SceUChar8 *)data + ret;
		item->commonData.data_size = dataSize;
		item->commonData.data2 = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, length, 0x7A, NULL, &item->value[0]);
	}
	return ret;
}
