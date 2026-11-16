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

/* a chain of one text: the empty text by default, the text of the tag when it is not empty */
#define READ_TEXT(field, tag, from, size, found) do { \
	item->field = NP_COMMERCE2_EMPTY_TEXT(item); \
	ret = npCommerce2FindRecordWhole((from), (size), (tag), 0, &(found)); \
	if (ret >= 0 && (found) != 0) \
		item->field = (from) + ret; \
} while (0)

/* the row of the table of tag 0x2b at an index, read for any of five data kinds: values, dates and texts
 * at fixed places, then records after them that depend on the kind */
int npCommerce2ReadTableRowBig0x2b(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2BigRow *item)
{
	const char *data = (const char *)result->commonData.data;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 found;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	const char *inner;
	SceUInt32 kind;
	int offset;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindTableRow(data, result->commonData.data_size, 0x2B, 0x95, index, &length,
				      result->commonData.ov);
	if (ret < 0)
		return ret;
	{
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x100);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
	}
	item->recordId = record;
	sceClibMemcpy_safe(&net, record + 49, 4);
	item->valueA = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 53, 4);
	item->kind = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 57, 4);
	item->valueC = sceNetNtohl(net);
	sceClibMemcpy_safe(&net64, record + 61, 8);
	item->rawA = sceNetNtohll(net64);
	sceClibMemcpy_safe(&net64, record + 69, 8);
	ms = sceNetNtohll(net64);
	if (ms != 0) {
		net64 = 1000 * ms;
		item->dateA = net64 + 62135596800000000LL;
	} else {
		net64 = 0;
		sceClibMemset(&item->dateA, 0, 8);
	}
	sceClibMemcpy_safe(&net64, record + 77, 8);
	ms = sceNetNtohll(net64);
	if (ms != 0) {
		net64 = 1000 * ms;
		item->dateB = net64 + 62135596800000000LL;
	} else {
		net64 = 0;
		sceClibMemset(&item->dateB, 0, 8);
	}
	sceClibMemcpy_safe(&net, record + 85, 4);
	item->flagA = sceNetNtohl(net) != 0;
	sceClibMemcpy_safe(&net64, record + 89, 8);
	ms = sceNetNtohll(net64);
	if (ms != 0) {
		net64 = 1000 * ms;
		item->dateC = net64 + 62135596800000000LL;
	} else {
		net64 = 0;
		sceClibMemset(&item->dateC, 0, 8);
	}
	sceClibMemcpy_safe(&net64, record + 97, 8);
	item->rawC = sceNetNtohll(net64);
	sceClibMemcpy_safe(&net, record + 105, 4);
	item->valueG = sceNetNtohl(net);
	sceClibMemcpy_safe(&net64, record + 109, 8);
	ms = sceNetNtohll(net64);
	if (ms != 0) {
		net64 = 1000 * ms;
		item->dateD = net64 + 62135596800000000LL;
	} else {
		net64 = 0;
		sceClibMemset(&item->dateD, 0, 8);
	}
	sceClibMemcpy_safe(&net, record + 117, 4);
	item->valueD = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 121, 4);
	item->flagB = sceNetNtohl(net) != 0;
	rest = record + 125;
	item->commonData.data2 = (const SceUChar8 *)rest;
	READ_TEXT(textA, 9, rest, length - 125, found);
	if (ret < 0)
		return ret;
	READ_TEXT(textB, 0xA, rest, length - 125, found);
	if (ret < 0)
		return ret;
	READ_TEXT(textC, 4, rest, length - 125, found);
	if (ret < 0)
		return ret;
	READ_TEXT(textD, 5, rest, length - 125, found);
	if (ret < 0)
		return ret;
	READ_TEXT(textE, 0x10, rest, length - 125, found);
	if (ret < 0)
		return ret;
	READ_TEXT(textF, 0x14, rest, length - 125, found);
	if (ret < 0)
		return ret;
	kind = item->kind;
	offset = 125;
	if (kind > 2) {
		if (kind != 4)
			goto tail;
	} else if (kind == 0) {
		goto tail;
	} else if (kind <= 1) {
		ret = npCommerce2FindRecordWhole(rest, length - 125, 0x98, 0x14, NULL);
		if (ret < 0)
			return ret;
		inner = rest + ret;
		sceClibMemcpy_safe(&net, inner, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, inner + 4, 4);
		item->valueF = sceNetNtohl(net);
		sceClibMemcpy_safe(&net64, inner + 8, 8);
		item->rawB = sceNetNtohll(net64);
		goto tail;
	}
	{
		SceUInt16 tag = kind == 4 ? 141 : 86;

		ret = npCommerce2FindRecordWhole(rest, length - 125, tag, 0x14, NULL);
		if (ret < 0)
			return ret;
		inner = rest + ret;
		sceClibMemcpy_safe(&net, inner, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, inner + 4, 4);
		item->valueF = sceNetNtohl(net);
		rest = inner + 20;
		offset = rest - record;
		READ_TEXT(textG, 0x59, rest, length - offset, found);
		if (ret < 0)
			return ret;
		READ_TEXT(textH, 0x55, rest, length - offset, found);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
		READ_TEXT(textI, 7, rest, length - offset, found);
		if (ret < 0)
			return ret;
		READ_TEXT(textJ, 0x44, rest, length - offset, found);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
		ret = npCommerce2FindRecordWhole(rest, length - offset, 0x4B, 8, NULL);
		item->mark = ret < 0 ? 1 : 3;
		ret = npCommerce2FindNestedRecordWhole(rest, length - offset, 0x9D, NULL, &item->countOfA);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
	}
tail:
	READ_TEXT(textK, 0x38, rest, length - offset, found);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - offset, 0x80, NULL, &item->countOfB);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}
