/*
 * SceNpCommerce2: reward and entitlement records of a response, read at an index.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

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
