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

/* the record of tag 0x58: two values; found in a buffer the caller passes, the header fields come from
 * the result */
int npCommerce2ReadItem0x58(const SceNpCommerce2CommonData *result, const char *buf, SceSize size,
			    NpCommerce2Item *item)
{
	SceUInt16 length;
	SceUInt32 net;
	const char *record;
	int ret;

	ret = npCommerce2FindRecordWhole(buf, size, 0x58, 8, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x64);
		version = result->version;
		head = result->buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = buf + ret;
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

/* a result with a value of tag 0x15 and a raw 64-bit value of tag 3, both optional */
int npCommerce2InitResult0x15(NpCommerce2DateItem *result, const char *data, SceSize size)
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
		ret = npCommerce2FindRecordWhole(records, dataLength, 0x15, 4, NULL);
		if (ret >= 0) {
			sceClibMemcpy_safe(&net, records + ret, 4);
			result->value = sceNetNtohl(net);
		}
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
			ret = npCommerce2FindRecordWhole(records, dataLength, 3, 8, NULL);
			if (ret >= 0) {
				sceClibMemcpy_safe(&net64, records + ret, 8);
				result->date = sceNetNtohll(net64);
			}
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* a result with only the header fields */
int npCommerce2InitBareResult(NpCommerce2Item *result, const char *data, SceSize size)
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

/* the record of tag 0x30, found where the result's records start: an id, a text, then a record of tag
 * 0x63 with four values (optional) and three more texts */
int npCommerce2ReadRecord0x30(const NpCommerce2Item *result, NpCommerce2Record0x30 *record)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt16 foundD;
	SceUInt32 net;
	const char *start;
	const char *rest;
	const char *inner;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x30, 0x19, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(record, 0, 0x80);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(record, 0, 0x5C);
		record->commonData.version = version;
		record->commonData.buf_head = head;
		start = data + ret;
		record->commonData.data = (const SceUChar8 *)start;
		record->commonData.data_size = dataSize;
		record->recordId = start;
		rest = start + 21;
		record->commonData.data2 = (const SceUChar8 *)rest;
		record->textA = NP_COMMERCE2_EMPTY_TEXT(record);
		ret = npCommerce2FindRecordWhole(rest, length - 21, 0xF, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			record->textA = rest + ret;
		if (ret >= 0) {
			ret = npCommerce2FindRecordWhole(rest, length - 21, 0x63, 0x14, NULL);
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND) {
				ret = 0;
			} else if (ret >= 0) {
				inner = rest + ret;
				sceClibMemcpy_safe(&net, inner, 4);
				record->valueA = sceNetNtohl(net);
				sceClibMemcpy_safe(&net, inner + 4, 4);
				record->valueB = sceNetNtohl(net);
				sceClibMemcpy_safe(&net, inner + 8, 4);
				record->valueC = sceNetNtohl(net);
				sceClibMemcpy_safe(&net, inner + 12, 4);
				record->valueD = sceNetNtohl(net);
				inner += 16;
				record->textB = NP_COMMERCE2_EMPTY_TEXT(record);
				ret = npCommerce2FindRecordWhole(inner, length - (inner - start), 0x62, 0, &foundB);
				if (ret >= 0 && foundB != 0)
					record->textB = inner + ret;
				if (ret >= 0) {
					record->textC = NP_COMMERCE2_EMPTY_TEXT(record);
					ret = npCommerce2FindRecordWhole(inner, length - (inner - start), 0xB7, 0, &foundC);
					if (ret >= 0 && foundC != 0)
						record->textC = inner + ret;
					if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
						record->textD = NP_COMMERCE2_EMPTY_TEXT(record);
						ret = npCommerce2FindRecordWhole(inner, length - (inner - start), 0xB8, 0,
										 &foundD);
						if (ret >= 0 && foundD != 0)
							record->textD = inner + ret;
						if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
							ret = 0;
					}
				}
			}
		}
	}
	return ret;
}

/* the record of tag 0x37 at an index, found where the result's records start: its text and the count of
 * records of tag 0x3b */
int npCommerce2ReadCountedText0x37(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2CountedText *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 found;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x37, 0x49, index, &length);
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
		item->recordId = record;
		rest = record + 65;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->text = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 65, 0x36, 0, &found);
		if (ret >= 0 && found != 0)
			item->text = rest + ret;
		if (ret >= 0)
			ret = npCommerce2FindNestedRecordWhole(rest, length - 65, 0x3B, NULL, &item->count);
	}
	return ret;
}

/* the record of tag 0x3b at an index: its text */
int npCommerce2ReadTextItem0x3b(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextItem2 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 found;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x3B, 0x45, index,
					   &length);
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
		item->textA = record;
		rest = record + 65;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 65, 0x3A, 0, &found);
		if (ret >= 0 && found != 0)
			item->textB = rest + ret;
	}
	return ret;
}

/* the record of tag 0x39 at an index, found where the result's records start: its text, a value and
 * two more texts */
int npCommerce2ReadTextItem0x39(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextValueItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt32 net;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x39, 0x1D, index, &length);
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
		item->recordId = record;
		sceClibMemcpy_safe(&net, record + 17, 4);
		item->value = sceNetNtohl(net);
		rest = record + 21;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 21, 0xF, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 21, 0x10, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
		}
	}
	return ret;
}

/* a result whose first record is the table of tag 0x2b; the rows after the first bad one are dropped */
int npCommerce2InitTableResult0x2b(NpCommerce2SmallTableResult *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt16 length;
	SceUInt32 i;
	int ret;
	int r;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x88);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.ov = status;
		ret = npCommerce2FindTableRecord(data + ret, dataLength, 0x2B, NULL, &result->table, status);
		if (ret >= 0) {
			r = 0;
			if (result->table.rows != 0) {
				i = 0;
				for (;;) {
					r = npCommerce2FindTableRow((const char *)result->commonData.data,
								    result->commonData.data_size, 0x2B, 0x95, i,
								    &length, result->commonData.ov);
					if (r < 0)
						break;
					if (++i >= result->table.rows)
						goto done;
				}
				if (i != 0) {
					r = 0;
					result->table.rows = i;
				}
			}
done:
			ret = r;
			if (r >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* the detail record of a result: tag 86 or, for kind 3, tag 141, then the reader for that tag */
int npCommerce2ReadDetailRecord(const NpCommerce2Item *result, void *detail, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 tag;
	const char *record;
	int ret;

	tag = kind == 3 ? 141 : 86;
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0x14, &length);
	if (ret >= 0) {
		record = data + ret;
		if (kind == 3)
			ret = npCommerce2ReadDetail0x91(result->commonData.version, result->commonData.buf_head, record,
							length, record + 20, (NpCommerce2Detail0x91 *)detail);
		else
			ret = npCommerce2ReadDetail0x4b(result->commonData.version, result->commonData.buf_head, record,
							length, record + 20, (NpCommerce2Detail *)detail);
	}
	return ret;
}

/* a result whose first record is the table of tag 0x28; the rows after the first bad one are dropped */
int npCommerce2InitTableResult0x28(NpCommerce2SmallTableResult *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt16 length;
	SceUInt32 i;
	int ret;
	int r;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x88);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.ov = status;
		ret = npCommerce2FindTableRecord(data + ret, dataLength, 0x28, NULL, &result->table, status);
		if (ret >= 0) {
			r = 0;
			if (result->table.rows != 0) {
				i = 0;
				for (;;) {
					r = npCommerce2FindTableRow((const char *)result->commonData.data,
								    result->commonData.data_size, 0x28, 4, i,
								    &length, result->commonData.ov);
					if (r < 0)
						break;
					if (++i >= result->table.rows)
						goto done;
				}
				if (i != 0) {
					r = 0;
					result->table.rows = i;
				}
			}
done:
			ret = r;
			if (r >= 0)
				ret = 0;
		}
	}
	return ret;
}


/* the row of the table of tag 0x28 at an index */
int npCommerce2ReadTableRowItem0x28(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	int ret;

	ret = npCommerce2FindTableRow(data, result->commonData.data_size, 0x28, 4, index, &length,
				      result->commonData.ov);
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
		sceClibMemcpy_safe(&net, data + ret, 4);
		item->value[0] = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)data + ret + 4;
	}
	return ret;
}

/* a result whose first record is the container of tag 0x5b */
int npCommerce2InitResult0x5b(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x60);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = savedLength;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x5B, NULL, &result->value[0]);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}

/* the record of tag 0x5b at an index, found where the result's records start: two values and the
 * counts of records of tags 0x5c and 0x9b */
int npCommerce2ReadCountedItem0x5b(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2CountedPair *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	const char *record;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x5B, 0xF, index, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x70);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->recordId = record;
		sceClibMemcpy_safe(&net, record + 3, 4);
		item->valueA = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 7, 4);
		item->valueB = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)record + 11;
		ret = npCommerce2FindNestedRecordWhole(record + 11, length - 11, 0x5C, NULL, &item->countOfA);
		if (ret >= 0)
			ret = npCommerce2FindNestedRecordWhole(record + 11, length - 11, 0x9B, NULL, &item->countOfB);
	}
	return ret;
}

/* the record of tag 0x5c at an index */
int npCommerce2ReadRecord0x5c(const NpCommerce2Item *result, SceUInt32 index, const char **record)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x5C, 0, index,
					   &length);
	if (ret >= 0)
		*record = data + ret;
	return ret;
}

/* the record of tag 0x9b at an index: where it starts and the text 6 bytes into it */
int npCommerce2ReadTextItem0x9b(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextItem2 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	const char *record;
	int ret;
	int out;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x9B, 0x47, index,
					   &length);
	out = ret;
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		record = data + ret;
		dataSize = length;
		version = result->commonData.version;
		head = result->commonData.buf_head;
		out = 0;
		sceClibMemset(item, 0, 0x64);
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->textA = record;
		item->textB = record + 6;
		item->commonData.data2 = (const SceUChar8 *)record + 71;
	}
	return out;
}

/* a result whose first record, the container of tag 0x93, is optional */
int npCommerce2InitResult0x93(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x64);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = savedLength;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x93, NULL, &result->value[0]);
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
			ret = 0;
	}
	return ret;
}

/* the record of tag 0x5e, found where the result's records start: a raw 64-bit value, texts and values at
 * fixed places, and the text of tag 0x5d after them */
int npCommerce2ReadValuesItem0x5e(const NpCommerce2Item *result, NpCommerce2ValuesItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 netShort;
	SceUInt32 net;
	SceUInt64 net64;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x5E, 60, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x98);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net64, record, 8);
		item->valueA = sceNetNtohll(net64);
		item->textA = record + 8;
		item->textB = record + 25;
		item->textC = record + 28;
		sceClibMemcpy_safe(&net, record + 34, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 38, 4);
		item->flag = sceNetNtohl(net) != 0;
		sceClibMemcpy_safe(&net, record + 42, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net64, record + 46, 8);
		item->valueD = sceNetNtohll(net64);
		sceClibMemcpy_safe(&netShort, record + 54, 2);
		item->valueE = sceNetNtohs(netShort);
		rest = record + 56;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textD = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 56, 0x5D, 0, &found);
		if (ret >= 0 && found != 0)
			item->textD = rest + ret;
	}
	return ret;
}

/* the record of tag 0x9b, found where the result's records start: where it starts and the text 6 bytes
 * into it */
int npCommerce2ReadTextRecord0x9b(const NpCommerce2Item *result, NpCommerce2TextItem2 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	const char *record;
	int ret;
	int out;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), 0x9B, 0x47, &length);
	out = ret;
	if (ret >= 0) {
		SceSize dataSize;
		SceUInt32 version;
		const SceUChar8 *head;

		record = data + ret;
		dataSize = length;
		version = result->commonData.version;
		head = result->commonData.buf_head;
		out = 0;
		sceClibMemset(item, 0, 0x64);
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->textA = record;
		item->textB = record + 6;
		item->commonData.data2 = (const SceUChar8 *)record + 71;
	}
	return out;
}

/* the record of tag 0x93 at an index, found where the result's records start */
int npCommerce2ReadItem0x93At(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x93, 4, index, &length);
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
		sceClibMemcpy_safe(&net, data + ret, 4);
		item->value[0] = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)data + ret + 4;
	}
	return ret;
}

/* a result with only the header fields (the copy in a fourth of Sony's source files) */
int npCommerce2InitBareResult_81014250(NpCommerce2Item *result, const char *data, SceSize size)
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

/* the record of tag 0x5f, found where the result's records start: sixteen big-endian values, the fifth of
 * which is mapped from the fourth (8 gives 1, 4 gives 2, 1 gives 3, 0 gives 0) */
int npCommerce2ReadValueRecord0x5f(const NpCommerce2Item *result, NpCommerce2ValueRecord *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt32 net;
	SceUInt32 kind;
	const char *record;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x5F, 60, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x9C);
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
		item->values[0] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 4, 4);
		item->values[1] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 8, 4);
		item->values[2] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 12, 4);
		kind = sceNetNtohl(net);
		item->values[3] = kind;
		switch (kind) {
		case 8:
			item->values[4] = 1;
			break;
		case 4:
			item->values[4] = 2;
			break;
		case 1:
			item->values[4] = 3;
			break;
		case 0:
			item->values[4] = 0;
			break;
		default:
			break;
		}
		sceClibMemcpy_safe(&net, record + 16, 4);
		item->values[5] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 20, 4);
		item->values[6] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 24, 4);
		item->values[7] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 28, 4);
		item->values[8] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 32, 4);
		item->values[9] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 36, 4);
		item->values[10] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 40, 4);
		item->values[11] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 44, 4);
		item->values[12] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 48, 4);
		item->values[13] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 52, 4);
		item->values[14] = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 56, 4);
		item->values[15] = sceNetNtohl(net);
		item->commonData.data2 = (const SceUChar8 *)record + 60;
	}
	return ret;
}

/* a result whose first record is the container of tag 0x61 */
int npCommerce2InitResult0x61(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x60);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = savedLength;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x61, NULL, &result->value[0]);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}


/* the fields of a record of tag 0x61: a value and the text of tag 0x60 after it */
int npCommerce2ReadTextItem0x61Fields(const SceNpCommerce2CommonData *result, const char *base,
				      const char *record, SceUInt16 length, NpCommerce2ValueTextItem2 *item)
{
	SceUInt16 found;
	SceUInt32 net;
	SceUInt32 version;
	const SceUChar8 *head;
	int ret;

	sceClibMemset(item, 0, 0x64);
	version = result->version;
	head = result->buf_head;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = version;
	item->commonData.buf_head = head;
	item->commonData.data = (const SceUChar8 *)record;
	item->commonData.data_size = length;
	sceClibMemcpy_safe(&net, record, 4);
	item->value = sceNetNtohl(net);
	item->commonData.data2 = (const SceUChar8 *)record + 4;
	item->text = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(record + 4, length - (record + 4 - base), 0x60, 0, &found);
	if (ret >= 0 && found != 0)
		item->text = record + 4 + ret;
	return ret;
}

/* the record of tag 0x61 at an index, found where the result's records start */
int npCommerce2ReadTextItem0x61At(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2ValueTextItem2 *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x61, 8, index, &length);
	if (ret >= 0)
		ret = npCommerce2ReadTextItem0x61Fields(&result->commonData, data + ret, data + ret, length, item);
	return ret;
}

/* a result whose first record is the container of tag 0x6b */
int npCommerce2InitResult0x6b(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x60);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.version = savedVersion;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = savedLength;
		result->commonData.buf_size = (const SceUChar8 *)(ret + savedLength);
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x6b, NULL, &result->value[0]);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}


/* the record of tag 0x6b at an index, found where the result's records start: three 64-bit values (the
 * last a date), four values and a flag, then two texts */
int npCommerce2ReadDatedItem0x6b(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2DatedItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size, 0x6B, 0x34, index, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x98);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net64, record, 8);
		item->valueA = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 8, 8);
		item->valueB = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 16, 8);
		ms = sceNetNtohll(net64);
		if (ms != 0) {
			net64 = 1000 * ms;
			item->date = net64 + 62135596800000000LL;
		} else {
			net64 = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 24, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 28, 4);
		item->valueD = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 32, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 36, 4);
		item->valueF = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 40, 4);
		item->flag = sceNetNtohl(net) != 0;
		rest = record + 44;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 44, 0x6A, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 44, 0x5D, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
		}
	}
	return ret;
}

/* the record of tag 0x70, found where the result's records start: three 64-bit values (the
 * last a date), four values and a flag, then two texts */
int npCommerce2ReadDatedItem0x70(const NpCommerce2Item *result, NpCommerce2DatedItem *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x70, 0x38, &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x98);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net64, record, 8);
		item->valueA = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 8, 8);
		item->valueB = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 16, 8);
		ms = sceNetNtohll(net64);
		if (ms != 0) {
			net64 = 1000 * ms;
			item->date = net64 + 62135596800000000LL;
		} else {
			net64 = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 24, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 28, 4);
		item->valueD = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 32, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 36, 4);
		item->valueF = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 40, 4);
		item->flag = sceNetNtohl(net) != 0;
		rest = record + 44;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 44, 0x6A, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 44, 0x5D, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
			if (ret >= 0)
				ret = npCommerce2FindNestedRecordWhole(rest, length - 44, 0x6F, NULL, &item->countOfA);
		}
	}
	return ret;
}

/* the record of tag 0x6f at an index, found where the result's records start: a raw 64-bit value, a date,
 * two values, a text and the count of records of tag 0x6d */
int npCommerce2ReadDatedPair0x6f(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2DatedPair *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x6F, 0x1C, index,
					   &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x80);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		sceClibMemcpy_safe(&net64, record, 8);
		item->valueA = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 8, 8);
		ms = sceNetNtohll(net64);
		if (ms != 0) {
			net64 = 1000 * ms;
			item->date = net64 + 62135596800000000LL;
		} else {
			net64 = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 16, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 20, 4);
		item->valueC = sceNetNtohl(net);
		rest = record + 24;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->text = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 24, 0x6E, 0, &found);
		if (ret >= 0 && found != 0)
			item->text = rest + ret;
		if (ret >= 0) {
			ret = npCommerce2FindNestedRecordWhole(rest, length - 24, 0x6D, NULL, &item->countOfA);
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* a result whose first record is the table of tag 0x71; the rows after the first bad one are dropped */
int npCommerce2InitTableResult0x71(NpCommerce2SmallTableResult *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt16 length;
	SceUInt32 i;
	int ret;
	int r;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x88);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.ov = status;
		ret = npCommerce2FindTableRecord(data + ret, dataLength, 0x71, NULL, &result->table, status);
		if (ret >= 0) {
			r = 0;
			if (result->table.rows != 0) {
				i = 0;
				for (;;) {
					r = npCommerce2FindTableRow((const char *)result->commonData.data,
								    result->commonData.data_size, 0x71, 0x31, i,
								    &length, result->commonData.ov);
					if (r < 0)
						break;
					if (++i >= result->table.rows)
						goto done;
				}
				if (i != 0) {
					r = 0;
					result->table.rows = i;
				}
			}
done:
			ret = r;
			if (r >= 0)
				ret = 0;
		}
	}
	return ret;
}


/* the record of tag 0x6d at an index: raw 64-bit values, three negated values and texts */
int npCommerce2ReadNegatedItem0x6d(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2NegatedItem *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt16 foundD;
	SceUInt32 net;
	SceUInt64 net64;
	const char *record;
	const char *rest;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x6D, 0x61, index,
					   &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x98);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->recordId = record;
		sceClibMemcpy_safe(&net64, record + 57, 8);
		item->valueA = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 65, 8);
		item->valueB = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net, record + 73, 4);
		item->negatedA = -sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 77, 4);
		item->negatedB = -sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 81, 4);
		item->negatedC = -sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 85, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 89, 4);
		item->valueD = sceNetNtohl(net);
		rest = record + 93;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 93, 0x6C, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 93, 4, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
			if (ret >= 0) {
				item->textC = NP_COMMERCE2_EMPTY_TEXT(item);
				ret = npCommerce2FindRecordWhole(rest, length - 93, 7, 0, &foundC);
				if (ret >= 0 && foundC != 0)
					item->textC = rest + ret;
				if (ret >= 0) {
					item->textD = NP_COMMERCE2_EMPTY_TEXT(item);
					ret = npCommerce2FindRecordWhole(rest, length - 93, 8, 0, &foundD);
					if (ret >= 0 && foundD != 0)
						item->textD = rest + ret;
				}
			}
		}
	}
	return ret;
}

/* the record of tag 124 or, for kind 3, 142: three values and the text of tag 0x44 */
int npCommerce2ReadTextValuesItem(const NpCommerce2Item *result, NpCommerce2TextValuesItem *item, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 tag;
	SceUInt32 net;
	const char *record;
	const char *rest;
	int ret;

	tag = 124;
	if (kind == 3)
		tag = 142;
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0xC, &length);
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
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 4, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 8, 4);
		item->valueA = sceNetNtohl(net);
		rest = record + 12;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->text = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 12, 0x44, 0, &found);
		if (ret >= 0 && found != 0)
			item->text = rest + ret;
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
			ret = 0;
	}
	return ret;
}

/* a result whose first record is the table of tag 0x72; the rows after the first bad one are dropped */
int npCommerce2InitTableResult0x72(NpCommerce2SmallTableResult *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 status;
	SceUInt16 length;
	SceUInt32 i;
	int ret;
	int r;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, &status);
	if (ret >= 0) {
		SceUInt32 savedVersion;
		SceUInt32 savedLength;

		sceClibMemset(result, 0, 0x88);
		savedVersion = version;
		savedLength = dataLength;
		sceClibMemset(result, 0, 0x5C);
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.buf_size = (const SceUChar8 *)(ret + dataLength);
		result->commonData.version = savedVersion;
		result->commonData.data_size = savedLength;
		result->commonData.data = (const SceUChar8 *)data + ret;
		result->commonData.ov = status;
		ret = npCommerce2FindTableRecord(data + ret, dataLength, 0x72, NULL, &result->table, status);
		if (ret >= 0) {
			r = 0;
			if (result->table.rows != 0) {
				i = 0;
				for (;;) {
					r = npCommerce2FindTableRow((const char *)result->commonData.data,
								    result->commonData.data_size, 0x72, 0x8A, i,
								    &length, result->commonData.ov);
					if (r < 0)
						break;
					if (++i >= result->table.rows)
						goto done;
				}
				if (i != 0) {
					r = 0;
					result->table.rows = i;
				}
			}
done:
			ret = r;
			if (r >= 0)
				ret = 0;
		}
	}
	return ret;
}


/* the row of the table of tag 0x71 at an index: a date, two values and three texts */
int npCommerce2ReadTableRowDated0x71(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2DatedRow *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindTableRow(data, result->commonData.data_size, 0x71, 0x31, index, &length,
				      result->commonData.ov);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x80);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->recordId = record;
		sceClibMemcpy_safe(&net64, record + 33, 8);
		ms = sceNetNtohll(net64);
		if (ms != 0) {
			net64 = 1000 * ms;
			item->date = net64 + 62135596800000000LL;
		} else {
			net64 = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 41, 4);
		item->valueA = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 45, 4);
		item->valueB = sceNetNtohl(net);
		rest = record + 49;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 49, 4, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 49, 5, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
			if (ret >= 0) {
				item->textC = NP_COMMERCE2_EMPTY_TEXT(item);
				ret = npCommerce2FindRecordWhole(rest, length - 49, 0x10, 0, &foundC);
				if (ret >= 0 && foundC != 0)
					item->textC = rest + ret;
			}
		}
	}
	return ret;
}
