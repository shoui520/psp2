/*
 * SceNpCommerce2: the SKU records of a response.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

#define NP_COMMERCE2_UNIX_EPOCH_TICKS	62135596800000000LL

/* a big-endian value of the record, stored where the table points */
#define READ_VALUE(offset, pointer) do { \
	sceClibMemcpy_safe(&net, buf + (offset), 4); \
	*(pointer) = sceNetNtohl(net); \
} while (0)

/* a date of the record: milliseconds since 1970, stored as ticks since year 1 */
#define READ_DATE(offset, pointer) do { \
	sceClibMemcpy_safe(&netDate, buf + (offset), 8); \
	ms = sceNetNtohll(netDate); \
	if (ms != 0) { \
		netDate = 1000 * ms; \
		*(pointer) = netDate + NP_COMMERCE2_UNIX_EPOCH_TICKS; \
	} else { \
		netDate = 0; \
		sceClibMemset((pointer), 0, 8); \
	} \
} while (0)

/* the values of a SKU record after its id; each is stored only where the table has a pointer for it */
int npCommerce2ReadSkuValues(const char *buf, const NpCommerce2SkuOutputs *out, int detail, int unused)
{
	SceUInt32 net;
	SceUInt64 netDate;
	SceUInt64 ms;
	SceUInt32 bitFour = 0;

	*out->skuId = buf;
	READ_VALUE(57, out->skuType);
	READ_VALUE(61, out->count);
	READ_VALUE(65, out->time);
	if (out->purchasability != NULL)
		READ_VALUE(69, out->purchasability);
	if (out->annotation != NULL)
		READ_VALUE(73, out->annotation);
	sceClibMemcpy_safe(&net, buf + 77, 4);
	*out->downloadable = sceNetNtohl(net) != 0;
	if (out->mask & 4) {
		sceClibMemcpy_safe(&net, buf + 81, 4);
		bitFour = sceNetNtohl(net);
	}
	if (out->price != NULL)
		READ_VALUE(85, out->price);
	if (out->valueA != NULL)
		READ_VALUE(89, out->valueA);
	if (out->valueB != NULL)
		READ_VALUE(93, out->valueB);
	if (detail == 0 && out->valueC != NULL)
		READ_VALUE(97, out->valueC);
	if (detail == 0 && out->valueD != NULL)
		READ_VALUE(101, out->valueD);
	if (detail == 0) {
		if (out->valueE != NULL)
			READ_VALUE(105, out->valueE);
		if (out->dateA != NULL)
			READ_DATE(109, out->dateA);
	}
	if (detail == 0 && out->dateB != NULL)
		READ_DATE(117, out->dateB);
	if (detail == 0) {
		if (out->purchasability != NULL && *out->purchasability != 0)
			*out->flags |= 1;
		if (out->downloadable != NULL && *out->downloadable != 0)
			*out->flags |= 2;
		if (bitFour != 0)
			*out->flags |= 4;
	}
	return 125;
}

/* a whole SKU record: the values, three more dates, and two more values */
int npCommerce2ReadSkuRecord(const char *buf, const NpCommerce2SkuOutputs *out)
{
	SceUInt32 net;
	SceUInt64 netDate;
	SceUInt64 netValue;
	SceUInt64 ms;
	const char *start = buf;
	const char *p;
	int ret;

	ret = npCommerce2ReadSkuValues(buf, out, 0, 1);
	if (ret >= 0) {
		p = buf + ret;
		buf = p;
		READ_DATE(0, out->dateC);
		READ_DATE(8, out->dateD);
		READ_DATE(16, out->dateE);
		sceClibMemcpy_safe(&net, p + 24, 4);
		*out->valueF = sceNetNtohl(net);
		sceClibMemcpy_safe(&netValue, p + 28, 8);
		*out->valueG = sceNetNtohll(netValue);
		ret = p + 16 - start + 20;
	}
	return ret;
}

/* a SKU: the fixed part through the table of outputs, then the texts and counts that follow it */
int npCommerce2ReadSkuItem(const NpCommerce2Item *result, NpCommerce2SkuItem *item, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	NpCommerce2SkuOutputs out;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 foundText;
	SceUInt16 tag;
	const char *start;
	const char *rest;
	int ret;
	int head;

	tag = kind == 3 ? 135 : 30;
	sceClibMemset(item, 0, 0xE8);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0xA5, &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	start = data + ret;
	item->commonData.data = (const SceUChar8 *)start;
	item->commonData.data_size = length;
	sceClibMemset(&out, 0, 0x5C);
	sceClibMemset(&out, 0, 0x48);
	out.head = result->commonData.buf_head;
	out.mask = 7;
	out.flags = &item->flags;
	out.skuId = &item->skuId;
	out.skuType = &item->skuType;
	out.count = &item->count;
	out.time = &item->time;
	out.purchasability = &item->purchasability;
	out.annotation = &item->annotation;
	out.downloadable = &item->downloadable;
	out.price = &item->price;
	out.valueA = &item->valueA;
	out.valueB = &item->valueB;
	out.valueC = &item->valueC;
	out.valueD = &item->valueD;
	out.valueE = &item->valueE;
	out.dateA = &item->dateA;
	out.dateB = &item->dateB;
	out.dateC = &item->dateC;
	out.dateD = &item->dateD;
	out.dateE = &item->dateE;
	out.valueF = &item->valueF;
	out.valueG = &item->valueG;
	ret = npCommerce2ReadSkuRecord(start, &out);
	if (ret < 0)
		return ret;
	head = ret;
	rest = start + ret;
	item->commonData.data2 = (const SceUChar8 *)rest;
	item->text = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 8, 0, &found);
	if (ret >= 0 && found != 0)
		item->text = rest + ret;
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x80, NULL, &item->countOfRewards);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x95, NULL, &item->countOfEntitlements);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	item->description = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0xA6, 0, &foundText);
	if (ret >= 0 && foundText != 0)
		item->description = rest + ret;
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}

/* the SKU at an index of a list: the fixed part through the table of outputs, then the texts and counts */
int npCommerce2ReadSkuListItem(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2SkuListItem *item,
			       int kind)
{
	const char *data = (const char *)result->commonData.data2;
	NpCommerce2SkuOutputs out;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 foundText;
	SceUInt16 tag;
	const char *start;
	const char *rest;
	int ret;
	int head;

	tag = kind == 3 ? 137 : 31;
	sceClibMemset(item, 0, 0x100);
	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), tag, 0xA5, index,
					   &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	start = data + ret;
	item->commonData.data = (const SceUChar8 *)start;
	item->commonData.data_size = length;
	sceClibMemset(&out, 0, 0x5C);
	sceClibMemset(&out, 0, 0x48);
	out.head = result->commonData.buf_head;
	out.mask = 7;
	out.flags = &item->flags;
	out.skuId = &item->skuId;
	out.skuType = &item->skuType;
	out.count = &item->count;
	out.time = &item->time;
	out.purchasability = &item->purchasability;
	out.annotation = &item->annotation;
	out.downloadable = &item->downloadable;
	out.price = &item->price;
	out.valueA = &item->valueA;
	out.valueB = &item->valueB;
	out.valueC = &item->valueC;
	out.valueD = &item->valueD;
	out.valueE = &item->valueE;
	out.dateA = &item->dateA;
	out.dateB = &item->dateB;
	out.dateC = &item->dateC;
	out.dateD = &item->dateD;
	out.dateE = &item->dateE;
	out.valueF = &item->valueF;
	out.valueG = &item->valueG;
	ret = npCommerce2ReadSkuRecord(start, &out);
	if (ret < 0)
		return ret;
	head = ret;
	rest = start + ret;
	item->commonData.data2 = (const SceUChar8 *)rest;
	ret = npCommerce2ReadListTexts(&item->commonData, rest, length - head, item->skuType, &item->textA,
				       &item->textB, &item->textC);
	if (ret < 0)
		return ret;
	item->textD = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0x3C, 0, &found);
	if (ret >= 0 && found != 0)
		item->textD = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x3D, NULL, &item->countOfA);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x3E, NULL, &item->countOfB);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x80, NULL, &item->countOfC);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x85, NULL, &item->countOfD);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x95, NULL, &item->countOfE);
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	item->description = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0xA6, 0, &foundText);
	if (ret >= 0 && foundText != 0)
		item->description = rest + ret;
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}
