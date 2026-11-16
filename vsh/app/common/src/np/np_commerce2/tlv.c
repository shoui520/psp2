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

int npCommerce2FindRecordWhole(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
			       SceUInt16 *length)
{
	return npCommerce2FindRecord(data, size, tag, minLength, length, 0);
}
