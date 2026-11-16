/*
 * SceNpCommerce2: the store's response records. A record is a 16-bit tag (the low twelve
 * bits) and a 16-bit length, both big-endian, then the value; records follow each other, and a
 * record's value may hold records again.
 */

#include "np_commerce2_internal.h"

/* finds the record of a tag; returns where its value starts, its length in *length */
__attribute__((noinline)) int npCommerce2FindRecord(const char *data, SceSize size, SceUInt16 tag,
						   SceUInt16 minLength, SceUInt16 *length, int partial)
{
	const char *p = data;
	SceUInt16 recordTag;
	SceUInt16 recordLength;
	int offset;
	int ret;

	if (size >= 4) {
		for (;;) {
			sceClibMemcpy_safe(&recordTag, p, 2);
			recordTag = sceNetNtohs(recordTag) & 0xFFF;
			sceClibMemcpy_safe(&recordLength, p + 2, 2);
			ret = sceNetNtohs(recordLength);
			p += 4;
			recordLength = ret;
			if (recordTag == tag)
				break;
			p += ret;
			if (size < (SceSize)(p - data) + 4)
				return NP_COMMERCE2_ERROR_RECORD_NOT_FOUND;
		}
		offset = p - data;
		if (size >= offset + minLength) {
			if (ret >= minLength) {
				if (size >= offset + ret || partial != 0) {
					if (length != NULL)
						*length = ret;
				} else {
					offset = NP_COMMERCE2_ERROR_RECORD_TRUNCATED;
				}
			} else {
				offset = NP_COMMERCE2_ERROR_RECORD_TOO_SHORT;
			}
		} else {
			offset = NP_COMMERCE2_ERROR_RECORD_CUT_OFF;
		}
	} else {
		offset = NP_COMMERCE2_ERROR_RECORD_NOT_FOUND;
	}
	return offset;
}

__attribute__((noinline)) int npCommerce2FindRecordWhole(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
			       SceUInt16 *length)
{
	return npCommerce2FindRecord(data, size, tag, minLength, length, 0);
}

/* finds, among the records of a tag-1 container, the record of a tag; *length: the value's length,
 * *count: the record's own length field */
__attribute__((noinline)) int npCommerce2FindNestedRecord(const char *data, SceSize size, SceUInt16 tag,
							 SceUInt16 *length, SceUInt32 *count, int partial)
{
	const char *p = data;
	const char *q;
	SceUInt16 recordTag;
	SceUInt16 recordLength;
	SceUInt16 containerLength;
	int offset = 0;
	int ret;
	int len;

	for (;;) {
		ret = npCommerce2FindRecord(p, size - offset, 1, 4, &containerLength, partial);
		if (ret < 0)
			break;
		q = p + ret;
		sceClibMemcpy_safe(&recordTag, q, 2);
		recordTag = sceNetNtohs(recordTag) & 0xFFF;
		sceClibMemcpy_safe(&recordLength, q + 2, 2);
		len = sceNetNtohs(recordLength);
		recordLength = len;
		if (recordTag == tag) {
			ret = q - data + 4;
			if (length != NULL)
				*length = containerLength - 4;
			if (count != NULL)
				*count = len;
			break;
		}
		p = q + containerLength;
		offset = p - data;
		if (size < (SceSize)offset) {
			ret = NP_COMMERCE2_ERROR_RECORD_NOT_FOUND;
			break;
		}
	}
	return ret;
}

__attribute__((noinline)) int npCommerce2FindNestedRecordWhole(const char *data, SceSize size, SceUInt16 tag, SceUInt16 *length,
				     SceUInt32 *count)
{
	return npCommerce2FindNestedRecord(data, size, tag, length, count, 0);
}

/* finds the record of a tag in a container, skipping index records of the same kind */
int npCommerce2FindIndexedRecord(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
				 SceUInt32 index, SceUInt16 *length)
{
	SceUInt16 recordLength;
	SceUInt16 containerLength;
	SceUInt32 count;
	const char *p;
	const char *q;
	SceUInt32 i;
	int ret;

	ret = npCommerce2FindNestedRecordWhole(data, size, tag, &containerLength, &count);
	if (ret >= 0) {
		p = data + ret;
		q = p;
		if (count > index) {
			i = 0;
			for (;;) {
				if (i != 0)
					p += recordLength;
				ret = npCommerce2FindRecordWhole(p, containerLength - (p - q), tag, minLength,
								 &recordLength);
				if (ret < 0)
					break;
				i++;
				p += ret;
				if (i > index) {
					ret = p - data;
					*length = recordLength;
					break;
				}
			}
		} else {
			ret = NP_COMMERCE2_ERROR_RECORD_INDEX;
		}
	}
	return ret;
}
