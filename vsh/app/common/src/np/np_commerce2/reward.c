/*
 * SceNpCommerce2: reward and entitlement records of a response, read at an index.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

#define NP_COMMERCE2_EMPTY_TEXT_OF(common)	((common) != NULL ? (const char *)(common)->buf_head + 4 : NULL)

#define NP_COMMERCE2_UNIX_EPOCH_TICKS	62135596800000000LL

/* the reward record of tag 0x80 at an index: strings and values at fixed places in the record, a
 * date, and a name and a description after it */
int npCommerce2ReadRewardItem0x80(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2RewardItem *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUChar8 headerCopy[0x64];
	SceUInt16 length;
	SceUInt16 foundName;
	SceUInt16 foundDescription;
	SceUInt32 net;
	SceUInt64 netDate;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	int ret;

	sceClibMemcpy_safe(headerCopy, result->commonData.buf_head, 0x64);
	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x80, 0xF4, index,
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
		item->rewardId = record;
		sceClibMemcpy_safe(&net, record + 49, 4);
		item->valueA = sceNetNtohl(net);
		item->textA = record + 53;
		item->textB = record + 110;
		sceClibMemcpy_safe(&net, record + 159, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 163, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 167, 4);
		item->valueD = sceNetNtohl(net);
		item->textC = record + 171;
		sceClibMemcpy_safe(&net, record + 228, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&netDate, record + 232, 8);
		ms = sceNetNtohll(netDate);
		if (ms != 0) {
			netDate = 1000 * ms;
			item->date = netDate + NP_COMMERCE2_UNIX_EPOCH_TICKS;
		} else {
			netDate = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 240, 4);
		item->valueF = sceNetNtohl(net);
		rest = record + 248;
		item->commonData.data2 = (const SceUChar8 *)rest;
		item->name = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 248, 8, 0, &foundName);
		if (ret >= 0 && foundName != 0)
			item->name = rest + ret;
		if (ret >= 0) {
			item->description = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 248, 0x84, 0, &foundDescription);
			if (ret >= 0 && foundDescription != 0)
				item->description = rest + ret;
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
				ret = 0;
		}
	}
	return ret;
}

/* the record of tag 0x96 at an index: where it starts and the text 57 bytes into it */
int npCommerce2ReadTextItem0x96(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextItem2 *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	const char *record;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x96, 0x6A, index,
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
		item->textB = record + 57;
	}
	return ret;
}

/* the record of tag 0x9c at an index: an id, three values and two 64-bit values */
int npCommerce2ReadValueItem0x9c(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2ValueItem *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt32 net;
	SceUInt64 net64;
	const char *record;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x9C, 0x4D, index,
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
		item->valueId = record;
		sceClibMemcpy_safe(&net, record + 49, 4);
		item->valueA = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 53, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 57, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net64, record + 61, 8);
		item->valueD = sceNetNtohll(net64);
		sceClibMemcpy_safe(&net64, record + 69, 8);
		item->valueE = sceNetNtohll(net64);
		item->commonData.data2 = (const SceUChar8 *)record + 77;
	}
	return ret;
}

/* the grant record of tag 0x95 at an index: strings and values at fixed places, a date, then a
 * record of tag 0x83 holding more values, a text (0x97) and the count of records of tag 0x96 */
int npCommerce2ReadGrantItem0x95(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2GrantItem *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 foundName;
	SceUInt32 net;
	SceUInt64 net64;
	SceUInt64 netDate;
	SceUInt64 ms;
	const char *record;
	const char *rest;
	const char *inner;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x95, 0xF8, index,
					   &length);
	if (ret >= 0) {
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0xB0);
		version = result->commonData.version;
		head = result->commonData.buf_head;
		dataSize = length;
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = version;
		item->commonData.buf_head = head;
		record = data + ret;
		item->commonData.data = (const SceUChar8 *)record;
		item->commonData.data_size = dataSize;
		item->grantId = record;
		sceClibMemcpy_safe(&net, record + 49, 4);
		item->valueA = sceNetNtohl(net);
		item->textA = record + 110;
		sceClibMemcpy_safe(&net, record + 159, 4);
		item->valueB = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 163, 4);
		item->valueC = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 167, 4);
		item->valueD = sceNetNtohl(net);
		item->textB = record + 171;
		sceClibMemcpy_safe(&net, record + 228, 4);
		item->valueE = sceNetNtohl(net);
		sceClibMemcpy_safe(&netDate, record + 232, 8);
		ms = sceNetNtohll(netDate);
		if (ms != 0) {
			netDate = 1000 * ms;
			item->date = netDate + NP_COMMERCE2_UNIX_EPOCH_TICKS;
		} else {
			netDate = 0;
			sceClibMemset(&item->date, 0, 8);
		}
		sceClibMemcpy_safe(&net, record + 240, 4);
		item->valueF = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, record + 244, 4);
		item->valueG = sceNetNtohl(net);
		rest = record + 248;
		item->commonData.data2 = (const SceUChar8 *)rest;
		ret = npCommerce2FindRecordWhole(rest, length - 248, 0x83, 0x14, NULL);
		if (ret >= 0) {
			inner = rest + ret;
			sceClibMemcpy_safe(&net, inner, 4);
			item->valueH = sceNetNtohl(net);
			sceClibMemcpy_safe(&net64, inner + 4, 8);
			item->valueI = sceNetNtohll(net64);
			sceClibMemcpy_safe(&net, inner + 12, 4);
			item->valueJ = sceNetNtohl(net);
			sceClibMemcpy_safe(&net, inner + 16, 4);
			item->valueK = sceNetNtohl(net);
			item->name = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(rest, length - 248, 0x97, 0, &foundName);
			if (ret >= 0 && foundName != 0)
				item->name = rest + ret;
			if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
				ret = npCommerce2FindNestedRecordWhole(rest, length - 248, 0x96, NULL, &item->countOfA);
				if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
					ret = 0;
			}
		}
	}
	return ret;
}

/* the offer record at an index: five values and a 64-bit value, then a text and counts */
int npCommerce2ReadOfferItem(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2OfferItem *item,
			     int kind)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 foundRef;
	SceUInt16 foundName;
	SceUInt16 tag;
	SceUInt32 net;
	SceUInt64 net64;
	const char *record;
	const char *rest;
	int ret;

	tag = kind == 3 ? 144 : 133;
	sceClibMemset(item, 0, 0x90);
	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), tag, 0x55, index,
					   &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	record = data + ret;
	item->commonData.data = (const SceUChar8 *)record;
	item->commonData.data_size = length;
	item->offerId = record;
	sceClibMemcpy_safe(&net, record + 49, 4);
	item->valueA = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 53, 4);
	item->valueB = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 57, 4);
	item->valueC = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 61, 4);
	item->valueD = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 65, 4);
	item->valueE = sceNetNtohl(net);
	sceClibMemcpy_safe(&net64, record + 69, 8);
	item->valueF = sceNetNtohll(net64);
	rest = record + 77;
	item->commonData.data2 = (const SceUChar8 *)rest;
	if (kind == 3) {
		ret = npCommerce2FindNestedRecordWhole(rest, length - 77, 0x11, NULL, (SceUInt32 *)&item->textOrCount);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
	} else {
		item->textOrCount = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, length - 77, 0x11, 0, &foundRef);
		if (ret >= 0 && foundRef != 0)
			item->textOrCount = rest + ret;
		if (ret < 0)
			return ret;
	}
	item->name = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - 77, 0x3C, 0, &foundName);
	if (ret >= 0 && foundName != 0)
		item->name = rest + ret;
	if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 77, 0x3D, NULL, &item->countOfA);
	if (ret < 0)
		return ret;
	ret = npCommerce2FindNestedRecordWhole(rest, length - 77, 0x3E, NULL, &item->countOfB);
	if (ret >= 0)
		ret = 0;
	return ret;
}

/* the record of tag 0x2e at an index: seven values and whether a record of tag 0x18 follows */
int npCommerce2ReadListItem0x2e(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2ListItem *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt32 net;
	const char *record;
	int has;
	int ret;

	ret = npCommerce2FindIndexedRecordPartial(data, result->commonData.data_size -
						  (data - (const char *)result->commonData.data), 0x2E, 0x1C,
						  index, &length, result->commonData.ov);
	if (ret < 0)
		return ret;
	{
		SceUInt32 version;
		const SceUChar8 *head;
		SceSize dataSize;

		sceClibMemset(item, 0, 0x7C);
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
	sceClibMemcpy_safe(&net, record, 4);
	item->valueA = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 4, 4);
	item->valueB = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 8, 4);
	item->valueC = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 12, 4);
	item->valueD = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 16, 4);
	item->valueE = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 20, 4);
	item->valueF = sceNetNtohl(net);
	sceClibMemcpy_safe(&net, record + 24, 4);
	item->valueG = sceNetNtohl(net);
	item->commonData.data2 = (const SceUChar8 *)record + 28;
	ret = npCommerce2FindRecordWhole(record + 28, item->commonData.data_size -
					 (record + 28 - (const char *)item->commonData.data), 0x18, 0x19, NULL);
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND) {
		has = 0;
	} else {
		if (ret < 0)
			return ret;
		has = 1;
	}
	item->hasExtra = has;
	return 0;
}

/* the record of tag 0x4e: the one record (data type 0) or the one at an index (data type 1) */
int npCommerce2ReadItem0x4e(const NpCommerce2Item *result, int indexed, SceUInt32 index, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt32 net;
	SceSize left;
	int ret;

	left = result->commonData.data_size - (data - (const char *)result->commonData.data);
	if (indexed == 1)
		ret = npCommerce2FindIndexedRecord(data, left, 0x4E, 0xB9, index, &length);
	else
		ret = npCommerce2FindRecordWhole(data, left, 0x4E, 0xB9, &length);
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

/* the texts of a SKU record: the list texts, three more, the count of records of tag 0x80 and the
 * description */
int npCommerce2ReadSkuTexts(const SceNpCommerce2CommonData *item, const char *buf, SceUInt16 length,
			    int kind, const char **textA, const char **textB, const char **textC,
			    const char **textD, const char **textE, const char **textF, SceUInt32 *countOfA,
			    const char **description)
{
	SceUInt16 foundA;
	SceUInt16 foundB;
	SceUInt16 foundC;
	SceUInt16 foundD;
	int ret;

	ret = npCommerce2ReadListTexts(item, buf, length, kind, textA, textB, textC);
	if (ret >= 0) {
		*textD = NP_COMMERCE2_EMPTY_TEXT_OF(item);
		ret = npCommerce2FindRecordWhole(buf, length, 7, 0, &foundA);
		if (ret >= 0 && foundA != 0)
			*textD = buf + ret;
		if (ret >= 0) {
			*textE = NP_COMMERCE2_EMPTY_TEXT_OF(item);
			ret = npCommerce2FindRecordWhole(buf, length, 0x10, 0, &foundB);
			if (ret >= 0 && foundB != 0)
				*textE = buf + ret;
			if (ret >= 0) {
				*textF = NP_COMMERCE2_EMPTY_TEXT_OF(item);
				ret = npCommerce2FindRecordWhole(buf, length, 4, 0, &foundC);
				if (ret >= 0 && foundC != 0)
					*textF = buf + ret;
				if (ret >= 0) {
					ret = npCommerce2FindNestedRecordWhole(buf, length, 0x80, NULL, countOfA);
					if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0) {
						*description = NP_COMMERCE2_EMPTY_TEXT_OF(item);
						ret = npCommerce2FindRecordWhole(buf, length, 0xA6, 0, &foundD);
						if (ret >= 0 && foundD != 0)
							*description = buf + ret;
						if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
							ret = 0;
					}
				}
			}
		}
	}
	return ret;
}
