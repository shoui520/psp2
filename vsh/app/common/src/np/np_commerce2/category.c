/*
 * SceNpCommerce2: the category and content records of a response, read into the SDK's
 * SceNpCommerce2CategoryInfo and the like.
 */

#include "np_commerce2_internal.h"

/* the ticks between year 1 and 1970 in microseconds */
#define NP_COMMERCE2_EMPTY_TEXT(info)	((info) != NULL ? (const char *)(info)->commonData.buf_head + 4 : NULL)

#define NP_COMMERCE2_UNIX_EPOCH_TICKS	62135596800000000LL

/* a category record: the fixed part, then the name, description, image URL and provider's name
 * records, each only if it holds something; those not present point at the empty string
 * the response starts with */
int npCommerce2ReadCategoryInfo(const char *data, SceSize length, const SceUChar8 *bufHead,
				SceUInt32 version, int dataType, SceNpCommerce2CategoryInfo *info)
{
	SceUInt64 ticks;
	const char *p;
	SceSize rest;
	SceUInt16 recordLength;
	int ret;

	sceClibMemset(info, 0, 0xC8);
	sceClibMemset(info, 0, 0x5C);
	info->commonData.version = version;
	info->commonData.buf_head = bufHead;
	info->commonData.data = (const SceUChar8 *)data;
	info->commonData.data_size = length;
	info->dataType = dataType;
	info->categoryId = data;
	sceClibMemcpy_safe(&ticks, data + 57, 8);
	ticks = sceNetNtohll(ticks);
	if (ticks != 0) {
		ticks = 1000 * ticks;
		info->releaseDate.tick = ticks + NP_COMMERCE2_UNIX_EPOCH_TICKS;
	} else {
		ticks = 0;
		sceClibMemset(&info->releaseDate, 0, 8);
	}
	p = data + 65;
	if (dataType == 1) {
		SceUInt32 net32;

		sceClibMemcpy_safe(&net32, data + 65, 4);
		info->countOfSubCategory = sceNetNtohl(net32);
		sceClibMemcpy_safe(&net32, data + 69, 4);
		info->countOfProduct = sceNetNtohl(net32);
		p = data + 73;
	}
	info->commonData.data2 = (const SceUChar8 *)p;
	rest = length - (p - data);
	info->categoryName = NP_COMMERCE2_EMPTY_TEXT(info);
	ret = npCommerce2FindRecordWhole(p, rest, 6, 0, &recordLength);
	if (ret >= 0) {
		if (recordLength != 0)
			info->categoryName = p + ret;
		info->categoryDescription = NP_COMMERCE2_EMPTY_TEXT(info);
		ret = npCommerce2FindRecordWhole(p, rest, 0xB, 0, &recordLength);
		if (ret >= 0) {
			if (recordLength != 0)
				info->categoryDescription = p + ret;
			info->imageUrl = NP_COMMERCE2_EMPTY_TEXT(info);
			ret = npCommerce2FindRecordWhole(p, rest, 0x10, 0, &recordLength);
			if (ret >= 0) {
				if (recordLength != 0)
					info->imageUrl = p + ret;
				info->spName = NP_COMMERCE2_EMPTY_TEXT(info);
				ret = npCommerce2FindRecordWhole(p, rest, 4, 0, &recordLength);
				if (ret >= 0 && recordLength != 0)
					info->spName = p + ret;
			}
		}
	}
	return 0;
}

/* the category record of a result of the category list */
int npCommerce2GetCategoryOfResult(const NpCommerce2Item *result, SceNpCommerce2CategoryInfo *info)
{
	const char *data = (const char *)result->commonData.data;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindRecordWhole(data, result->commonData.data_size, 0x1B, 0x55, &length);
	if (ret >= 0)
		ret = npCommerce2ReadCategoryInfo(data + ret, length, result->commonData.buf_head,
						  result->commonData.version, 1, info);
	return ret;
}

/* the category record of a content item */
int npCommerce2GetCategoryOfContent(const NpCommerce2Item *content, SceNpCommerce2CategoryInfo *info)
{
	const char *data = (const char *)content->commonData.data2;
	SceUInt16 length;
	int ret;

	ret = npCommerce2FindRecordWhole(data, content->commonData.data_size -
					 (data - (const char *)content->commonData.data), 0x1A, 0x4D, &length);
	if (ret >= 0)
		ret = npCommerce2ReadCategoryInfo(data + ret, length, content->commonData.buf_head,
						  content->commonData.version, 0, info);
	return ret;
}
