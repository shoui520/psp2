/*
 * SceNpCommon: base64 (RFC 4648 alphabet, '=' padding). The decoder takes whole groups of four
 * characters and checks the output space before each group.
 */

#include "np_common_internal.h"

/* each character's 6-bit value, -1 for a character outside the alphabet */
static const signed char s_base64DecodeTable[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/* encodes size bytes, padding the last group; returns the characters written */
SceSize sceNpCommonBase64Encode(const SceUInt8 *src, char *dst, SceSize size)
{
	static const char alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *start = dst;
	SceUInt32 bits = 0;

	for (; size >= 3; size -= 3) {
		bits = (src[0] << 16) | (src[1] << 8) | src[2];
		dst[0] = alphabet[(bits >> 18) & 0x3f];
		dst[1] = alphabet[(bits >> 12) & 0x3f];
		dst[2] = alphabet[(bits >> 6) & 0x3f];
		dst[3] = alphabet[bits & 0x3f];
		src += 3;
		dst += 4;
	}
	if (size != 0)
		bits = src[0] << 16;
	if (size > 1)
		bits |= src[1] << 8;
	if (size != 0) {
		dst[0] = alphabet[(bits >> 18) & 0x3f];
		dst[1] = alphabet[(bits >> 12) & 0x3f];
		dst[2] = size > 1 ? alphabet[(bits >> 6) & 0x3f] : '=';
		dst[3] = '=';
		dst += 4;
	}
	return dst - start;
}

/* decodes size characters (a multiple of four) into at most dstSize bytes; returns the bytes */
int SceNpCommon_FB1F70F3(const char *src, SceSize size, SceUInt8 *dst, SceSize dstSize)
{
	SceUInt32 bits;
	int value, count = 0;

	if ((size & 3) != 0)
		return NP_COMMON_ERROR_INVALID_ARGUMENT;
	for (; size > 4; size -= 4) {
		if ((value = s_base64DecodeTable[src[0]]) == -1)
			return NP_COMMON_ERROR_INVALID_DATA;
		bits = value << 18;
		if ((value = s_base64DecodeTable[src[1]]) == -1)
			return NP_COMMON_ERROR_INVALID_DATA;
		bits |= value << 12;
		if ((value = s_base64DecodeTable[src[2]]) == -1)
			return NP_COMMON_ERROR_INVALID_DATA;
		bits |= value << 6;
		if ((value = s_base64DecodeTable[src[3]]) == -1)
			return NP_COMMON_ERROR_INVALID_DATA;
		bits |= value;
		if (dstSize < 3)
			return NP_COMMON_ERROR_INVALID_ARGUMENT;
		dst[0] = (SceUInt8)(bits >> 16);
		dst[1] = (SceUInt8)(bits >> 8);
		dst[2] = (SceUInt8)bits;
		src += 4;
		dst += 3;
		dstSize -= 3;
		count += 3;
	}
	if (size != 4)
		return NP_COMMON_ERROR_INVALID_ARGUMENT;
	if ((value = s_base64DecodeTable[src[0]]) == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	bits = value << 18;
	if ((value = s_base64DecodeTable[src[1]]) == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	bits |= value << 12;
	if (src[2] == '=') {
		if (src[3] != '=')
			return NP_COMMON_ERROR_INVALID_DATA;
		if (dstSize == 0)
			return NP_COMMON_ERROR_INVALID_ARGUMENT;
		dst[0] = (SceUInt8)(bits >> 16);
		return count + 1;
	}
	if ((value = s_base64DecodeTable[src[2]]) == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	bits |= value << 6;
	if (src[3] == '=') {
		if (dstSize < 2)
			return NP_COMMON_ERROR_INVALID_ARGUMENT;
		dst[0] = (SceUInt8)(bits >> 16);
		dst[1] = (SceUInt8)(bits >> 8);
		return count + 2;
	}
	if ((value = s_base64DecodeTable[src[3]]) == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	bits |= value;
	if (dstSize < 3)
		return NP_COMMON_ERROR_INVALID_ARGUMENT;
	dst[0] = (SceUInt8)(bits >> 16);
	dst[1] = (SceUInt8)(bits >> 8);
	dst[2] = (SceUInt8)bits;
	return count + 3;
}

/* the bytes size characters decode to, checking them as the decoder does */
int SceNpCommon_A0C5C10E(const char *src, SceSize size)
{
	int count = 0;

	if ((size & 3) != 0)
		return NP_COMMON_ERROR_INVALID_ARGUMENT;
	for (; size > 4; size -= 4) {
		if (s_base64DecodeTable[src[0]] == -1 ||
		    s_base64DecodeTable[src[1]] == -1 ||
		    s_base64DecodeTable[src[2]] == -1 ||
		    s_base64DecodeTable[src[3]] == -1)
			return NP_COMMON_ERROR_INVALID_DATA;
		src += 4;
		count += 3;
	}
	if (size != 4)
		return NP_COMMON_ERROR_INVALID_ARGUMENT;
	if (s_base64DecodeTable[src[0]] == -1 ||
	    s_base64DecodeTable[src[1]] == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	if (src[2] == '=') {
		if (src[3] != '=')
			return NP_COMMON_ERROR_INVALID_DATA;
		return count + 1;
	}
	if (s_base64DecodeTable[src[2]] == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	if (src[3] == '=')
		return count + 2;
	if (s_base64DecodeTable[src[3]] == -1)
		return NP_COMMON_ERROR_INVALID_DATA;
	return count + 3;
}
