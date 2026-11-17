/*
 * SceNpCommonPs4: Base64 (RFC 4648: the standard alphabet and the URL-safe one) and the library's
 * pseudo-random number generator.
 */

#include "np_common_ps4_internal.h"

extern "C" {

#define BASE64_INVALID	-1
#define BASE64_PAD	'='

const char g_base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";	/* 0x810275d0 */

/* the value of each character, -1 for one that is not in the alphabet */
const char g_base64DecodeTable[256] = {	/* 0x81027618 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
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

SceUInt32 g_randomState = 1;	/* 0x8102B018 */

int SceNpCommonPs4_EC456A83(const unsigned char *src, unsigned char *dst, SceSize length)
{
	unsigned char *start;
	SceUInt32 bits;
	SceUInt32 high;
	int pad;

	start = dst;
	bits = 0;
	while (length >= 3) {
		high = (src[1] << 8) | (src[0] << 16);
		bits = high | src[2];
		dst[0] = g_base64Alphabet[(bits >> 18) & 0x3F];
		length -= 3;
		dst[1] = g_base64Alphabet[(bits >> 12) & 0x3F];
		src += 3;
		dst[2] = g_base64Alphabet[(bits >> 6) & 0x3F];
		dst[3] = g_base64Alphabet[bits & 0x3F];
		dst += 4;
	}
	if (length != 0)
		bits = src[0] << 16;
	if (length > 1)
		bits |= src[1] << 8;
	if (length != 0) {
		dst[0] = g_base64Alphabet[(bits >> 18) & 0x3F];
		pad = BASE64_PAD;
		dst[1] = g_base64Alphabet[(bits >> 12) & 0x3F];
		if (length > 1)
			pad = g_base64Alphabet[(bits >> 6) & 0x3F];
		dst[2] = pad;
		dst[3] = BASE64_PAD;
		dst += 4;
	}
	return dst - start;
}

int SceNpCommonPs4_D082556B(const char *src, SceSize srcLength, unsigned char *dst, SceSize dstSize)
{
	int count;
	int a;
	int b;
	int c;
	int d;
	int bits;

	count = 0;
	if ((srcLength & 3) != 0)
		return NP_COMMON_PS4_ERROR_80558003;
	while (srcLength > 4) {
		a = g_base64DecodeTable[src[0]];
		if (a == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits = a << 18;
		b = g_base64DecodeTable[src[1]];
		if (b == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= b << 12;
		c = g_base64DecodeTable[src[2]];
		if (c == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= c << 6;
		d = g_base64DecodeTable[src[3]];
		if (d == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= d;
		if (dstSize < 3)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[2] = bits;
		dst[0] = bits >> 16;
		srcLength -= 4;
		dst[1] = bits >> 8;
		src += 4;
		dst += 3;
		dstSize -= 3;
		count += 3;
	}
	if (srcLength != 4)
		return NP_COMMON_PS4_ERROR_80558003;
	a = g_base64DecodeTable[src[0]];
	if (a == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	b = g_base64DecodeTable[src[1]];
	if (b == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits = (a << 18) | (b << 12);
	if (src[2] == BASE64_PAD) {
		if (src[3] != BASE64_PAD)
			return NP_COMMON_PS4_ERROR_80558004;
		if (dstSize == 0)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[0] = bits >> 16;
		return count + 1;
	}
	c = g_base64DecodeTable[src[2]];
	if (c == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits |= c << 6;
	if (src[3] == BASE64_PAD) {
		if (dstSize < 2)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[0] = bits >> 16;
		dst[1] = bits >> 8;
		return count + 2;
	}
	d = g_base64DecodeTable[src[3]];
	if (d == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits |= d;
	if (dstSize < 3)
		return NP_COMMON_PS4_ERROR_80558003;
	dst[2] = bits;
	dst[0] = bits >> 16;
	dst[1] = bits >> 8;
	return count + 3;
}

int SceNpCommonPs4_1EC7612A(const char *src, SceSize srcLength)
{
	int count;

	count = 0;
	if ((srcLength & 3) != 0)
		return NP_COMMON_PS4_ERROR_80558003;
	while (srcLength > 4) {
		if (g_base64DecodeTable[src[0]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (g_base64DecodeTable[src[1]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (g_base64DecodeTable[src[2]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (g_base64DecodeTable[src[3]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		srcLength -= 4;
		src += 4;
		count += 3;
	}
	if (srcLength != 4)
		return NP_COMMON_PS4_ERROR_80558003;
	if (g_base64DecodeTable[src[0]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (g_base64DecodeTable[src[1]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (src[2] == BASE64_PAD) {
		if (src[3] != BASE64_PAD)
			return NP_COMMON_PS4_ERROR_80558004;
		return count + 1;
	}
	if (g_base64DecodeTable[src[2]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (src[3] == BASE64_PAD)
		return count + 2;
	if (g_base64DecodeTable[src[3]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	return count + 3;
}

SceUInt32 SceNpCommonPs4_00F74689(void)
{
	g_randomState = g_randomState * 1103515245 + 12345;
	return (g_randomState >> 16) & 0x7FFF;
}

SceUInt32 SceNpCommonPs4_61DAB7E5(SceUInt32 seed)
{
	g_randomState = seed;
	return seed;
}

/* the URL-safe alphabet's decoder: the table's '+' and '/' invalid, '-' and '_' the values 62 and 63; the
   last group may come short, completed with '=' */
int SceNpCommonPs4_4A7DB2EF(const char *src, SceSize srcLength, unsigned char *dst, SceSize dstSize)
{
	char chunk[4];
	char table[256];
	int count;
	int a;
	int b;
	int c;
	int d;
	int bits;
	int i;

	for (i = 0; i < 256; i++)
		table[i] = g_base64DecodeTable[i];
	table[47] = BASE64_INVALID;
	table[43] = BASE64_INVALID;
	table[45] = 62;
	table[95] = 63;
	count = 0;
	while (srcLength > 4) {
		a = table[(int)src[0]];
		if (a == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits = a << 18;
		b = table[(int)src[1]];
		if (b == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= b << 12;
		c = table[(int)src[2]];
		if (c == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= c << 6;
		d = table[(int)src[3]];
		if (d == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		bits |= d;
		if (dstSize < 3)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[2] = bits;
		dst[0] = bits >> 16;
		srcLength -= 4;
		dst[1] = bits >> 8;
		src += 4;
		dst += 3;
		dstSize -= 3;
		count += 3;
	}
	for (i = 0; i < 4; i++) {
		if (i >= srcLength)
			chunk[i] = BASE64_PAD;
		else
			chunk[i] = src[i];
	}
	a = table[chunk[0]];
	if (a == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	b = table[chunk[1]];
	if (b == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits = (a << 18) | (b << 12);
	if (chunk[2] == BASE64_PAD) {
		if (chunk[3] != BASE64_PAD)
			return NP_COMMON_PS4_ERROR_80558004;
		if (dstSize == 0)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[0] = bits >> 16;
		return count + 1;
	}
	c = table[chunk[2]];
	if (c == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits |= c << 6;
	if (chunk[3] == BASE64_PAD) {
		if (dstSize < 2)
			return NP_COMMON_PS4_ERROR_80558003;
		dst[0] = bits >> 16;
		dst[1] = bits >> 8;
		return count + 2;
	}
	d = table[chunk[3]];
	if (d == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	bits |= d;
	if (dstSize < 3)
		return NP_COMMON_PS4_ERROR_80558003;
	dst[2] = bits;
	dst[0] = bits >> 16;
	dst[1] = bits >> 8;
	return count + 3;
}

/* the same decoder's length of what it would decode */
int SceNpCommonPs4_2E65CBA6(const char *src, SceSize srcLength)
{
	char chunk[4];
	char table[256];
	int count;
	int i;

	for (i = 0; i < 256; i++)
		table[i] = g_base64DecodeTable[i];
	table[47] = BASE64_INVALID;
	table[43] = BASE64_INVALID;
	table[45] = 62;
	table[95] = 63;
	count = 0;
	while (srcLength > 4) {
		if (table[(int)src[0]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (table[(int)src[1]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (table[(int)src[2]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		if (table[(int)src[3]] == BASE64_INVALID)
			return NP_COMMON_PS4_ERROR_80558004;
		srcLength -= 4;
		src += 4;
		count += 3;
	}
	for (i = 0; i < 4; i++) {
		if (i >= srcLength)
			chunk[i] = BASE64_PAD;
		else
			chunk[i] = src[i];
	}
	if (table[chunk[0]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (table[chunk[1]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (chunk[2] == BASE64_PAD) {
		if (chunk[3] == BASE64_PAD)
			return count + 1;
		return NP_COMMON_PS4_ERROR_80558004;
	}
	if (table[chunk[2]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	if (chunk[3] == BASE64_PAD)
		return count + 2;
	if (table[chunk[3]] == BASE64_INVALID)
		return NP_COMMON_PS4_ERROR_80558004;
	return count + 3;
}

}
