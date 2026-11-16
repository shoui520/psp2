/*
 * SceNpCommerce2: the listed entries and detail records of a response.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

/* the entry of tag 0x16 at an index: its id and name (the first bytes of the record), a 16-bit kind,
 * six values, how many records of tag 0x15 follow, and the description and image texts */
int npCommerce2ReadEntry0x16(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2Entry *entry)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 foundImage;
	SceUInt16 netKind;
	SceUInt32 netValue;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x16, 0x2A, index, &length);
	if (ret >= 0) {
		sceClibMemset(entry, 0, 0x88);
		sceClibMemset(entry, 0, 0x5C);
		entry->commonData.version = result->commonData.version;
		entry->commonData.buf_head = result->commonData.buf_head;
		record = data + ret;
		entry->commonData.data = (const SceUChar8 *)record;
		entry->commonData.data_size = length;
		entry->entryId = record;
		entry->name = record + 4;
		sceClibMemcpy_safe(&netKind, record + 8, 2);
		entry->kind = sceNetNtohs(netKind);
		sceClibMemcpy_safe(&netValue, record + 10, 4);
		entry->attrA = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 14, 4);
		entry->attrB = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 18, 4);
		entry->attrC = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 22, 4);
		entry->attrD = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 26, 4);
		entry->attrE = sceNetNtohl(netValue);
		rest = record + 30;
		entry->commonData.data2 = (const SceUChar8 *)rest;
		ret = npCommerce2FindNestedRecordWhole(rest, length - (rest - (const char *)entry->commonData.data),
						       0x15, NULL, &entry->countOfChildren);
		if (ret >= 0) {
			entry->description = NP_COMMERCE2_EMPTY_TEXT(entry);
			ret = npCommerce2FindRecordWhole(rest, length - (rest - (const char *)entry->commonData.data),
							 0x7D, 0, &found);
			if (ret >= 0 && found != 0)
				entry->description = rest + ret;
			if (ret >= 0) {
				entry->imageUrl = NP_COMMERCE2_EMPTY_TEXT(entry);
				ret = npCommerce2FindRecordWhole(rest, length -
								 (rest - (const char *)entry->commonData.data),
								 0x7E, 0, &foundImage);
				if (ret >= 0 && foundImage != 0)
					entry->imageUrl = rest + ret;
			}
		}
	}
	return ret;
}

/* the extra record of a detail: whether it is there, and its three values */
int npCommerce2ReadExtra0x4a(const SceNpCommerce2CommonData *detail, SceUInt32 *hasExtra,
			     SceUInt32 *extraA, SceUInt32 *extraB, SceUInt32 *extraC)
{
	const char *data = (const char *)detail->data2;
	SceUInt16 length;
	SceUInt32 netValue;
	const char *record;
	int has;
	int ret;

	ret = npCommerce2FindRecordWhole(data, detail->data_size - (data - (const char *)detail->data), 0x4A,
					 0xC, &length);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND) {
		has = 0;
	} else {
		if (ret < 0)
			return ret;
		record = data + ret;
		sceClibMemcpy_safe(&netValue, record, 4);
		*extraA = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 4, 4);
		*extraB = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, record + 8, 4);
		*extraC = sceNetNtohl(netValue);
		has = 1;
	}
	*hasExtra = has;
	return 0;
}

/* the detail record of tag 0x4b: two values, three texts and the extra record */
int npCommerce2ReadDetail0x4b(SceUInt32 version, const SceUChar8 *bufHead, const char *data,
			      SceSize size, const char *records, NpCommerce2Detail *detail)
{
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt32 netValue;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindRecordWhole(records, size - (records - data), 0x4B, 8, &length);
	if (ret >= 0) {
		sceClibMemset(detail, 0, 0x80);
		sceClibMemset(detail, 0, 0x5C);
		detail->commonData.version = version;
		detail->commonData.buf_head = bufHead;
		detail->commonData.data = (const SceUChar8 *)records + ret;
		detail->commonData.data_size = length;
		sceClibMemcpy_safe(&netValue, records + ret, 4);
		detail->valueA = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, records + ret + 4, 4);
		detail->valueB = sceNetNtohl(netValue);
		rest = records + ret + 8;
		detail->commonData.data2 = (const SceUChar8 *)rest;
		detail->textA = NP_COMMERCE2_EMPTY_TEXT(detail);
		ret = npCommerce2FindRecordWhole(rest, length - 8, 0x5A, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			detail->textA = rest + ret;
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
			detail->textB = NP_COMMERCE2_EMPTY_TEXT(detail);
			ret = npCommerce2FindRecordWhole(rest, length - 8, 0x46, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				detail->textB = rest + ret;
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
				detail->textC = NP_COMMERCE2_EMPTY_TEXT(detail);
				ret = npCommerce2FindRecordWhole(rest, length - 8, 0x47, 0, &foundC);
				if (ret >= 0 && foundC != 0)
					detail->textC = rest + ret;
				if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
					ret = npCommerce2ReadExtra0x4a(&detail->commonData, &detail->hasExtra,
								       &detail->extraA, &detail->extraB,
								       &detail->extraC);
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}
	return ret;
}

/* the detail record of tag 0x91: two values, a text and the extra record */
int npCommerce2ReadDetail0x91(SceUInt32 version, const SceUChar8 *bufHead, const char *data,
			      SceSize size, const char *records, NpCommerce2Detail0x91 *detail)
{
	SceUInt16 length;
	SceUInt16 found;
	SceUInt32 netValue;
	const char *rest;
	int ret;

	ret = npCommerce2FindRecordWhole(records, size - (records - data), 0x91, 8, &length);
	if (ret >= 0) {
		SceSize dataSize;

		sceClibMemset(detail, 0, 0x78);
		dataSize = length;
		sceClibMemset(detail, 0, 0x5C);
		detail->commonData.version = version;
		detail->commonData.buf_head = bufHead;
		detail->commonData.data = (const SceUChar8 *)records + ret;
		detail->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&netValue, records + ret, 4);
		detail->valueA = sceNetNtohl(netValue);
		sceClibMemcpy_safe(&netValue, records + ret + 4, 4);
		detail->valueB = sceNetNtohl(netValue);
		rest = records + ret + 8;
		detail->commonData.data2 = (const SceUChar8 *)rest;
		detail->text = NP_COMMERCE2_EMPTY_TEXT(detail);
		ret = npCommerce2FindRecordWhole(rest, length - 8, 0x92, 0, &found);
		if (ret >= 0 && found != 0)
			detail->text = rest + ret;
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
			ret = npCommerce2ReadExtra0x4a(&detail->commonData, &detail->hasExtra, &detail->extraA,
						       &detail->extraB, &detail->extraC);
			if (ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* the detail records of a result, read where the result's records are */
int npCommerce2ReadDetailOf0x4b(const NpCommerce2Item *result, NpCommerce2Detail *detail)
{
	return npCommerce2ReadDetail0x4b(result->commonData.version, result->commonData.buf_head,
					 (const char *)result->commonData.data, result->commonData.data_size,
					 (const char *)result->commonData.data2, detail);
}

int npCommerce2ReadDetailOf0x91(const NpCommerce2Item *result, NpCommerce2Detail0x91 *detail)
{
	return npCommerce2ReadDetail0x91(result->commonData.version, result->commonData.buf_head,
					 (const char *)result->commonData.data, result->commonData.data_size,
					 (const char *)result->commonData.data2, detail);
}

/* a result with no record of its own: the response header and where the records start */
int npCommerce2InitPlainResult(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x7C);
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

/* the record of tag 0x29, found where the result's records start */
int npCommerce2ReadItem0x29(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 netValue;
	SceUInt32 version;
	const SceUChar8 *head;
	SceSize dataSize;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x29, 4, &length);
	if (ret >= 0) {
		sceClibMemset(item, 0, 0x60);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		item->commonData.data = (const SceUChar8 *)data + ret;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&netValue, data + ret, 4);
		item->value[0] = sceNetNtohl(netValue);
		item->commonData.data2 = (const SceUChar8 *)data + ret + 4;
	}
	return ret;
}

/* a result whose first records are the values of tags 0x2d and 0x15 */
int npCommerce2InitResult0x2d(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt32 value;
	SceUInt32 net;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x64);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		result->commonData.ov = status;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x2D, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&value, records + ret, 4);
			value = sceNetNtohl(value);
			sceClibMemcpy_safe(&result->value[0], &value, 4);
		}
		if (ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net, records + ret, 4);
				result->value[1] = sceNetNtohl(net);
			}
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* a result whose first records are the values of tags 0x2d and 0x15 */
int npCommerce2InitResult0x2d_8101150E(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt32 value;
	SceUInt32 net;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x64);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		result->commonData.ov = status;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x2D, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&value, records + ret, 4);
			value = sceNetNtohl(value);
			sceClibMemcpy_safe(&result->value[0], &value, 4);
		}
		if (ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net, records + ret, 4);
				result->value[1] = sceNetNtohl(net);
			}
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}


/* a result whose first records are the values of tags 0x2d and 0x15 */
int npCommerce2InitResult0x2d_8101160C(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt32 value;
	SceUInt32 net;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x64);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		result->commonData.ov = status;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x2D, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&value, records + ret, 4);
			value = sceNetNtohl(value);
			sceClibMemcpy_safe(&result->value[0], &value, 4);
		}
		if (ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net, records + ret, 4);
				result->value[1] = sceNetNtohl(net);
			}
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}


/* the same without the status, and with the first record optional */
int npCommerce2InitResult0x2dNoStatus_8101170A(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 value;
	SceUInt32 net;
	const char *records;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x64);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		records = data + ret;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)records;
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x2D, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&value, records + ret, 4);
			value = sceNetNtohl(value);
			sceClibMemcpy_safe(&result->value[0], &value, 4);
		}
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net, records + ret, 4);
				result->value[1] = sceNetNtohl(net);
			}
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* the record of tag 0x2c with a value and a date */
int npCommerce2ReadDateItem0x2c(const NpCommerce2Item *result, NpCommerce2DateItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	SceUInt64 netDate;
	SceUInt64 ms;
	const char *record;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x2C, 0xC, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x68);
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
		item->value = sceNetNtohl(net);
		sceClibMemcpy_safe(&netDate, record + 4, 8);
		ms = sceNetNtohll(netDate);
		if (ms != 0) {
			netDate = 1000 * ms;
			item->date = netDate + 62135596800000000LL;
		} else {
			netDate = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		item->commonData.data2 = (const SceUChar8 *)record + 12;
	}
	return ret;
}

/* the record of tag 0x2f: three values and how many records of tag 0x2e follow */
int npCommerce2ReadCountItem0x2f(const NpCommerce2Item *result, NpCommerce2CountItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	const char *record;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x2F, 0x10, &length);
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
		sceClibMemcpy_safe(&net, record + 8, 4);
		item->valueC = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)record + 12;
		ret = npCommerce2FindNestedRecord(record + 12, length - 12, 0x2E, NULL, &item->countOfA,
						  result->commonData.ov);
	}
	return ret;
}
