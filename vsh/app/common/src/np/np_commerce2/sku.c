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

/* a game SKU record as the SDK's SceNpCommerce2GameSkuInfo: the values, the texts, five counts for the
 * normal data type, the dates and the charge model */
int npCommerce2ReadGameSkuInfo(const char *data, SceSize length, const SceUChar8 *bufHead,
			       SceUInt32 version, int dataType, SceNpCommerce2GameSkuInfo *info)
{
	SceUChar8 headerCopy[0x64];
	NpCommerce2SkuOutputs out;
	SceUInt16 found;
	SceUInt16 foundText;
	const char *rest;
	SceSize left;
	int ret;

	sceClibMemcpy_safe(headerCopy, bufHead, 0x64);
	sceClibMemset(info, 0, 0xE0);
	sceClibMemset(info, 0, 0x5C);
	info->commonData.version = version;
	info->commonData.buf_head = bufHead;
	info->commonData.data = (const SceUChar8 *)data;
	info->commonData.data_size = length;
	info->dataType = dataType;
	sceClibMemset(&out, 0, 0x48);
	out.head = headerCopy;
	out.mask = 7;
	out.flags = &info->skuAttr;
	out.skuId = &info->skuId;
	out.skuType = &info->skuType;
	out.count = &info->countUntilExpiration;
	out.time = &info->timeUntilExpiration;
	out.purchasability = &info->purchasabilityFlag;
	out.annotation = &info->annotation;
	out.downloadable = (SceUInt32 *)&info->downloadable;
	out.price = &info->price;
	out.valueC = &info->salesType;
	out.valueD = &info->firstPlayExpiration;
	out.valueE = &info->chargeModel;
	out.dateA = (SceUInt64 *)&info->playableDate;
	out.dateB = (SceUInt64 *)&info->chargeDate;
	ret = npCommerce2ReadSkuValues(data, &out, 0, 0);
	if (ret >= 0) {
		rest = data + ret + 36;
		info->commonData.data2 = (const SceUChar8 *)rest;
		left = length - (ret + 36);
		if (dataType == 0) {
			info->skuName = NP_COMMERCE2_EMPTY_TEXT(info);
			ret = npCommerce2FindRecordWhole(rest, left, 8, 0, &found);
			if (ret >= 0 && found != 0)
				info->skuName = rest + ret;
			if (ret < 0)
				return ret;
		} else if (dataType == 1) {
			ret = npCommerce2ReadListTexts(&info->commonData, rest, left, info->skuType, &info->skuName,
						       &info->productId, &info->contentLinkUrl);
			if (ret < 0)
				return ret;
		}
		left = length - (rest - data);
		ret = npCommerce2FindNestedRecordWhole(rest, left, 0x80, NULL, &info->countOfRewardInfo);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
		ret = npCommerce2FindNestedRecordWhole(rest, left, 0x95, NULL, &info->countOfApplicableRewardInfo);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
		if (dataType == 1) {
			ret = npCommerce2FindNestedRecordWhole(rest, left, 0x9C, NULL,
							       &info->countOfGameSkuEntitlementInfo);
			if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
				return ret;
		}
		info->augmentedDescription = NP_COMMERCE2_EMPTY_TEXT(info);
		ret = npCommerce2FindRecordWhole(rest, left, 0xA6, 0, &foundText);
		if (ret >= 0 && foundText != 0)
			info->augmentedDescription = rest + ret;
		if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
			ret = 0;
	}
	return ret;
}

/* the SKU of a game product at an index (the data type 1) or the one SKU (the data type 0) */
int npCommerce2GetGameSkuInfoOf(const SceNpCommerce2GameProductInfo *productInfo, int dataType,
				SceUInt32 index, SceNpCommerce2GameSkuInfo *info)
{
	const char *data = (const char *)productInfo->commonData.data2;
	SceUInt16 length;
	int ret;
	int type;

	if (dataType == 1) {
		ret = npCommerce2FindIndexedRecord(data, productInfo->commonData.data_size -
						   (data - (const char *)productInfo->commonData.data), 0x1D, 0xA5,
						   index, &length);
		if (ret < 0)
			return ret;
		type = 1;
	} else {
		ret = npCommerce2FindRecordWhole(data, productInfo->commonData.data_size -
						 (data - (const char *)productInfo->commonData.data), 0x1C, 0xA5,
						 &length);
		if (ret < 0)
			return ret;
		type = 0;
	}
	return npCommerce2ReadGameSkuInfo(data + ret, length, productInfo->commonData.buf_head,
					  productInfo->commonData.version, type, info);
}

/* the record of tag 0x23 at an index */
int npCommerce2ReadRecord0x23(const NpCommerce2Item *result, SceUInt32 index, const char **record)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x23, 0, index,
					   &length);
	if (ret >= 0)
		*record = data + ret;
	return ret;
}

/* the record of tag 0x24 at an index: two texts */
int npCommerce2ReadTextItem0x24(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextItem2 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	const char *record;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x24, 0, index,
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
		item->commonData.data2 = (const SceUChar8 *)record;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(record, length, 0x42, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = record + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(record, length, 0x43, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = record + ret;
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* the record of tag 0x25 at an index: a value and three texts */
int npCommerce2ReadTextItem0x25(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextItem3 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt32 netValue;
	const char *record;
	const char *rest;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x25, 4, index,
					   &length);
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
		sceClibMemcpy_safe(&netValue, record, 4);
		item->value = sceNetNtohl(netValue);
		rest = record + 4;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->textA = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 4, 0x3F, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			item->textA = rest + ret;
		if (ret >= 0) {
			item->textB = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 4, 0x40, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				item->textB = rest + ret;
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
				item->textC = NP_COMMERCE2_EMPTY_TEXT(item);
				ret = npCommerce2FindRecordWhole(rest, length - 4, 0x41, 0, &foundC);
				if (ret >= 0 && foundC != 0)
					item->textC = rest + ret;
				if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
					ret = 0;
			}
		}
	}
	return ret;
}

/* a SKU detail record of tag 0x4c: the values through the table of outputs, the texts, and the count
 * of records of tag 0x9c */
int npCommerce2ReadSkuDetail0x4c(const NpCommerce2Item *result, NpCommerce2SkuDetail *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUChar8 headerCopy[0x64];
	NpCommerce2SkuOutputs out;
	SceUInt16 length;
	const char *start;
	const char *rest;
	int head;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), 0x4C, 0xB5, &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0xB8);
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	start = data + ret;
	item->commonData.data = (const SceUChar8 *)start;
	item->commonData.data_size = length;
	sceClibMemset(&out, 0, 0x48);
	out.head = headerCopy;
	out.mask = 6;
	out.flags = &item->flags;
	out.skuId = &item->skuId;
	out.skuType = &item->skuType;
	out.count = &item->count;
	out.time = &item->time;
	out.downloadable = &item->downloadable;
	out.valueC = &item->valueC;
	out.valueD = &item->valueD;
	out.valueE = &item->valueE;
	out.dateA = &item->dateA;
	out.dateB = &item->dateB;
	ret = npCommerce2ReadSkuValues(start, &out, 0, 0);
	if (ret < 0)
		return ret;
	head = ret + 36;
	rest = start + head;
	item->commonData.data2 = (const SceUChar8 *)rest;
	ret = npCommerce2ReadSkuTexts(&item->commonData, rest, (SceUInt16)(length - (head - 36)) - 36, item->skuType,
				      &item->textA, &item->textB, &item->textC, &item->textD, &item->textE,
				      &item->textF, &item->countOfA, &item->description);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x9C, NULL, &item->countOfB);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}

/* the SKU detail record with all its parts: the values, the texts, texts and counts of further tags and,
 * for kind 2, whether a record of tag 0x4b follows */
int npCommerce2ReadSkuDetailLong(const NpCommerce2Item *result, NpCommerce2SkuDetailLong *item, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	NpCommerce2SkuOutputs out;
	SceUInt16 length;
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 tag;
	SceSize left;
	const char *start;
	const char *rest;
	int head;
	int ret;

	tag = kind == 3 ? 138 : 77;
	if (kind == 3)
		sceClibMemset(item, 0, 0xF8);
	else
		sceClibMemset(item, 0, 0x100);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0xC1, &length);
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
	out.mask = 6;
	out.flags = &item->flags;
	out.skuId = &item->skuId;
	out.skuType = &item->skuType;
	out.count = &item->count;
	out.time = &item->time;
	out.downloadable = &item->downloadable;
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
	ret = npCommerce2ReadSkuTexts(&item->commonData, rest, length - head, item->skuType, &item->textA,
				      &item->textB, &item->textC, &item->textD, &item->textE, &item->textF,
				      &item->countOfC, &item->description);
	if (ret < 0)
		return ret;
	item->textG = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0x3C, 0, &foundA);
	if (ret >= 0 && foundA != 0)
		item->textG = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x3D, NULL, &item->countOfA);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x3E, NULL, &item->countOfB);
	if (ret < 0)
		return ret;
	item->textH = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0x44, 0, &foundB);
	if (ret >= 0 && foundB != 0)
		item->textH = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	left = length - head;
	if (kind == 2) {
		if (npCommerce2FindRecordWhole(rest, left, 0x4B, 8, NULL) >= 0)
			item->rewardKind = 3;
		else
			item->rewardKind = 1;
		left = length - head;
	}
	ret = npCommerce2FindNestedRecordWhole(rest, left, 0x85, NULL, &item->countOfD);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}

/* the record of tag 0x3d at an index and, when the record is long enough, the value after its
 * first three bytes */
int npCommerce2ReadRecord0x3d(const NpCommerce2Item *result, SceUInt32 index, const char **record,
			      SceUInt32 *value)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt32 net;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x3D, 3, index,
					   &length);
	if (ret >= 0) {
		*record = data + ret;
		if (length >= 7) {
			if (value != NULL) {
				sceClibMemcpy_safe(&net, data + ret + 3, 4);
				*value = sceNetNtohl(net);
			}
		} else if (value != NULL) {
			*value = 0;
		}
	}
	return ret;
}

/* the record of tag 0x3e at an index */
int npCommerce2ReadRecord0x3e(const NpCommerce2Item *result, SceUInt32 index, const char **record)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x3E, 3, index,
					   &length);
	if (ret >= 0)
		*record = data + ret;
	return ret;
}
