/*
 * SceNpCommerce2: the package record of a response (a record with dates, flags and many sub-records).
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

#define NP_COMMERCE2_UNIX_EPOCH_TICKS	62135596800000000LL

#define READ_VALUE(offset, field) do { \
	sceClibMemcpy_safe(&net, record + (offset), 4); \
	(field) = sceNetNtohl(net); \
} while (0)

#define READ_DATE(offset, field) do { \
	sceClibMemcpy_safe(&netDate, record + (offset), 8); \
	ms = sceNetNtohll(netDate); \
	if (ms != 0) { \
		netDate = 1000 * ms; \
		(field) = netDate + NP_COMMERCE2_UNIX_EPOCH_TICKS; \
	} else { \
		netDate = 0; \
		sceClibMemset(&(field), 0, 8); \
	} \
} while (0)

/* the package record (tag 143 or 127): values and dates at fixed places, then counts and texts of
 * sub-records */
int npCommerce2ReadPackageItem(const NpCommerce2Item *result, NpCommerce2PackageItem *item, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt16 foundD;
	SceUInt16 tag;
	SceUInt16 lastTag;
	SceUInt32 net;
	SceUInt64 netDate;
	SceUInt64 netValue;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	if (kind == 3) {
		tag = 143;
		lastTag = 144;
	} else {
		tag = 127;
		lastTag = 133;
	}
	sceClibMemset(item, 0, 0xD0);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0x64, &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	record = data + ret;
	item->commonData.data = (const SceUChar8 *)record;
	item->commonData.data_size = length;
	READ_VALUE(0, item->valueA);
	READ_VALUE(4, item->valueB);
	sceClibMemcpy_safe(&net, record + 8, 4);
	item->flag = sceNetNtohl(net) != 0;
	READ_VALUE(12, item->valueC);
	READ_VALUE(16, item->valueD);
	READ_VALUE(20, item->valueE);
	sceClibMemcpy_safe(&net, record + 24, 4);
	item->flags = sceNetNtohl(net);
	item->flag = (item->flags & 2) != 0 || (item->flags & 8) != 0;
	READ_VALUE(28, item->valueF);
	READ_VALUE(32, item->valueG);
	READ_DATE(36, item->dateA);
	READ_DATE(44, item->dateB);
	READ_DATE(52, item->dateC);
	READ_VALUE(60, item->valueH);
	sceClibMemcpy_safe(&netValue, record + 64, 8);
	item->valueI = sceNetNtohll(netValue);
	rest = record + 72;
	item->commonData.data2 = (const SceUChar8 *)rest;
	if (kind == 3) {
		ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x11, NULL, (SceUInt32 *)&item->textD);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
	} else {
		item->textD = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 72, 0x11, 0, &foundD);
		if (ret >= 0 && foundD != 0)
			item->textD = rest + ret;
		if (ret < 0)
			return ret;
	}
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x23, NULL, &item->countOfA);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x24, NULL, &item->countOfB);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x25, NULL, &item->countOfC);
	if (ret < 0)
		return ret;
	item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - 72, 0x44, 0, &foundA);
	if (ret >= 0 && foundA != 0)
		item->textA = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - 72, 0x45, 0, &foundB);
	if (ret >= 0 && foundB != 0)
		item->textB = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	item->textC = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - 72, 0x3C, 0, &foundC);
	if (ret >= 0 && foundC != 0)
		item->textC = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x3D, NULL, &item->countOfD);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, 0x3E, NULL, &item->countOfE);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 72, lastTag, NULL, &item->countOfF);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}
