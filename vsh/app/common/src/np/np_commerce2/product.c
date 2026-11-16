/*
 * SceNpCommerce2: the product records of a response.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_EMPTY_TEXT(item)	((item) != NULL ? (const char *)(item)->commonData.buf_head + 4 : NULL)

#define NP_COMMERCE2_UNIX_EPOCH_TICKS	62135596800000000LL

/* the record of tag 0x28 */
int npCommerce2ReadItem0x28(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt32 netValue;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), 0x28, 4, &length);
	if (ret >= 0) {
		sceClibMemset(item, 0, 0x60);
		sceClibMemset(item, 0, 0x5C);
		item->commonData.version = result->commonData.version;
		item->commonData.buf_head = result->commonData.buf_head;
		item->commonData.data = (const SceUChar8 *)data + ret;
		item->commonData.data_size = length;
		sceClibMemcpy_safe(&netValue, data + ret, 4);
		item->value[0] = sceNetNtohl(netValue);
		item->commonData.data2 = (const SceUChar8 *)data + ret + 4;
	}
	return ret;
}

/* the record of tag 0x11 at an index */
int npCommerce2ReadRecord0x11(const NpCommerce2Item *result, SceUInt32 index, const char **record)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindIndexedRecord(data, result->commonData.data_size -
					   (data - (const char *)result->commonData.data), 0x11, 0, index,
					   &length);
	if (ret >= 0)
		*record = data + ret;
	return ret;
}

/* the first fields of a product record: the id, how many SKUs, and the release date, which
 * the store gives in milliseconds since 1970 */
__attribute__((noinline))
int npCommerce2ReadProductId(const char *buf, const char **productId, SceUInt32 *skuCount,
			     SceUInt64 *releaseDate)
{
	SceUInt32 netCount;
	SceUInt64 netDate;
	SceUInt64 ms;
	const char *p;

	*productId = buf;
	sceClibMemcpy_safe(&netCount, buf + 49, 4);
	*skuCount = sceNetNtohl(netCount);
	p = buf + 53;
	sceClibMemcpy_safe(&netDate, p, 8);
	ms = sceNetNtohll(netDate);
	if (ms != 0) {
		*releaseDate = 1000 * ms + NP_COMMERCE2_UNIX_EPOCH_TICKS;
	} else {
		netDate = 0;
		sceClibMemset(releaseDate, 0, 8);
	}
	return p - buf + 8;
}

/* the fixed part of a product record: the id and date, then five values; the annotation takes its
 * bits from the last two */
__attribute__((noinline))
int npCommerce2ReadProductValues(const SceUChar8 *header, const char *buf, const char **productId,
				 SceUInt32 *skuCount, SceUInt64 *releaseDate, SceUInt32 *status,
				 SceUInt32 *kind, SceUInt32 *downloadable, SceUInt32 *price,
				 SceUInt32 *annotation)
{
	SceUChar8 headerCopy[0x64];
	SceUInt32 net;
	SceUInt32 flags;
	SceUInt32 value;
	const char *p;
	int ret;

	sceClibMemcpy_safe(headerCopy, header, 0x64);
	ret = npCommerce2ReadProductId(buf, productId, skuCount, releaseDate);
	if (ret >= 0) {
		p = buf + ret;
		sceClibMemcpy_safe(&net, p, 4);
		*status = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, p + 4, 4);
		*kind = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, p + 8, 4);
		flags = sceNetNtohl(net);
		*price = flags;
		*downloadable = (flags & 2) != 0 || (flags & 8) != 0;
		sceClibMemcpy_safe(&net, p + 12, 4);
		value = sceNetNtohl(net);
		sceClibMemcpy_safe(&net, p + 16, 4);
		flags = sceNetNtohl(net);
		if (value & 0x80000000)
			*annotation |= 1;
		if (value & 0x40000000)
			*annotation |= 2;
		if (value & 0x20000000)
			*annotation |= 4;
		if (flags != 0)
			*annotation |= 8;
		ret = p - buf + 20;
	}
	return ret;
}

/* the name, description, image and provider texts of a product record; those it lacks point
 * at the empty string the response starts with */
__attribute__((noinline))
int npCommerce2ReadProductTexts(NpCommerce2ProductItem *item, const char *buf, SceUInt16 length,
				const char **name, const char **shortDescription,
				const char **imageUrl, const char **spName)
{
	SceUInt16 found;
	int ret;

	*name = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(buf, length, 7, 0, &found);
	if (ret >= 0 && found != 0)
		*name = buf + ret;
	if (ret >= 0) {
		*shortDescription = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(buf, length, 0xC, 0, &found);
		if (ret >= 0 && found != 0)
			*shortDescription = buf + ret;
		if (ret >= 0) {
			*imageUrl = NP_COMMERCE2_EMPTY_TEXT(item);
			ret = npCommerce2FindRecordWhole(buf, length, 0x10, 0, &found);
			if (ret >= 0 && found != 0)
				*imageUrl = buf + ret;
			if (ret >= 0) {
				*spName = NP_COMMERCE2_EMPTY_TEXT(item);
				ret = npCommerce2FindRecordWhole(buf, length, 4, 0, &found);
				if (ret >= 0 && found != 0)
					*spName = buf + ret;
			}
		}
	}
	return ret;
}

/* a product record: the fixed part, the texts, and what follows them (rewards, contents' ratings) */
int npCommerce2ReadProductItem(const NpCommerce2Item *result, NpCommerce2ProductItem *item, int kind)
{
	const char *data = (const char *)result->commonData.data2;
	SceUInt16 length;
	SceUInt16 found;
	SceUInt16 foundRatings;
	SceUInt16 tag;
	const char *rest;
	SceUInt32 left;
	int ret;
	int head;

	tag = kind == 3 ? 139 : 38;
	sceClibMemset(item, 0, 0xA0);
	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size -
					 (data - (const char *)result->commonData.data), tag, 0x61, &length);
	if (ret < 0)
		return ret;
	sceClibMemset(item, 0, 0x5C);
	item->commonData.version = result->commonData.version;
	item->commonData.buf_head = result->commonData.buf_head;
	item->commonData.data = (const SceUChar8 *)data + ret;
	item->commonData.data_size = length;
	ret = npCommerce2ReadProductValues(result->commonData.buf_head, (const char *)item->commonData.data,
					   &item->productId, &item->skuCount, &item->releaseDate,
					   &item->status, &item->kind, &item->downloadable,
					   &item->price, &item->annotation);
	if (ret < 0)
		return ret;
	head = ret;
	rest = (const char *)item->commonData.data + ret;
	item->commonData.data2 = (const SceUChar8 *)rest;
	ret = npCommerce2ReadProductTexts(item, rest, (SceUInt16)(length - head), &item->name,
					  &item->shortDescription, &item->imageUrl, &item->spName);
	if (ret < 0)
		return ret;
	left = length - head;
	if (kind == 3) {
		ret = npCommerce2FindNestedRecordWhole(rest, left, 0x11, NULL, (SceUInt32 *)&item->contentRatings);
		if (ret != NP_COMMERCE2_ERROR_RECORD_NOT_FOUND && ret < 0)
			return ret;
	} else {
		item->contentRatings = NP_COMMERCE2_EMPTY_TEXT(item);
		ret = npCommerce2FindRecordWhole(rest, left, 0x11, 0, &foundRatings);
		if (ret >= 0 && foundRatings != 0)
			item->contentRatings = rest + ret;
		if (ret < 0)
			return ret;
	}
	ret = npCommerce2FindNestedRecordWhole(rest, length - head, 0x18, NULL, &item->countOfRewards);
	if (ret < 0)
		return ret;
	item->rewards = NP_COMMERCE2_EMPTY_TEXT(item);
	ret = npCommerce2FindRecordWhole(rest, length - head, 0x44, 0, &found);
	if (ret >= 0 && found != 0)
		item->rewards = rest + ret;
	if (ret == NP_COMMERCE2_ERROR_RECORD_NOT_FOUND || ret >= 0)
		ret = 0;
	return ret;
}
