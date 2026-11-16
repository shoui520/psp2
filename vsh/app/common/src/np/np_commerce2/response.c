/*
 * SceNpCommerce2: the response of a request. It starts with a 0x64-byte header followed by a
 * word and the records of its operation; the Init...Result exports and the accessors read them.
 */

#include "np_commerce2_internal.h"

/* the header: *version gets its first word, *dataLength the length of the records after it
 * (the header's length word minus four), *status its fourth word; returns where the records
 * start */
__attribute__((noinline)) int npCommerce2ParseResponseHeader(const char *data, SceSize size, SceUInt32 *version,
				   SceUInt32 *dataLength, SceUInt32 *status)
{
	NpCommerce2ResponseHeader header;
	char tail[4];
	int ret;
	SceUInt32 statusWord;

	if (size >= 0x68) {
		sceClibMemcpy_safe(&header, data, sizeof(header));
		sceClibMemcpy_safe(tail, data + sizeof(header), 4);
		statusWord = header.status;
		if (header.length >= 4) {
			if (size >= header.length + 100) {
				ret = 104;
				*dataLength = header.length - 4;
				if (version != NULL)
					*version = header.version;
				if (status != NULL)
					*status = statusWord;
			} else {
				ret = SCE_NP_COMMERCE2_ERROR_INVALID_RESULT_DATA;
			}
		} else {
			ret = SCE_NP_COMMERCE2_ERROR_INVALID_RESULT_DATA;
		}
	} else {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_RESULT_DATA;
	}
	return ret;
}

/* tells whether the response carries the record of tag 0x78 */
int npCommerce2ResponseHasRecord78(const char *data, SceSize size, int *has)
{
	SceUInt32 value = 0;
	SceUInt32 version;
	SceUInt32 dataLength = 0;
	int ret;
	int result;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		result = 1;
		if (npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x78, NULL, &value) < 0)
			result = 0;
		*has = result;
		ret = 0;
	}
	return ret;
}

/* the result of a response whose records are an index (tag 0x78) */
int npCommerce2InitResult0x78(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 end;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		sceClibMemset(result, 0, 0x60);
		sceClibMemset(result, 0, sizeof(result->commonData));
		end = ret + dataLength;
		result->commonData.version = version;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = dataLength;
		result->commonData.buf_size = (const SceUChar8 *)end;
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x78, NULL, &result->value[0]);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}

/* the same, for the records of tag 0x16 */
int npCommerce2InitResult0x16(NpCommerce2Item *result, const char *data, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLength = 0;
	SceUInt32 end;
	int ret;

	ret = npCommerce2ParseResponseHeader(data, size, &version, &dataLength, NULL);
	if (ret >= 0) {
		sceClibMemset(result, 0, 0x60);
		sceClibMemset(result, 0, sizeof(result->commonData));
		end = ret + dataLength;
		result->commonData.version = version;
		result->commonData.buf_head = (const SceUChar8 *)data;
		result->commonData.data_size = dataLength;
		result->commonData.buf_size = (const SceUChar8 *)end;
		result->commonData.data = (const SceUChar8 *)data + ret;
		ret = npCommerce2FindNestedRecordWhole(data + ret, dataLength, 0x16, NULL, &result->value[0]);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}
