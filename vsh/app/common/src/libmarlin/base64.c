/*
 * Base64, the encoder breaking its lines as asked. The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_BASE64_ALPHABET	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="
#define MLN_BASE64_PAD		64	/* '=' */
#define MLN_BASE64_SKIP		99	/* not of the alphabet: ignored */

#define MLN_BASE64_LF		0
#define MLN_BASE64_CRLF		1
#define MLN_BASE64_CR		2

/* each byte's value in the alphabet */
static const unsigned char s_base64Values[256] = {
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 62, 99, 99, 99, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 99, 99, 99, 64, 99, 99,
	99,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 99, 99, 99, 99, 99,
	99, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
};

int MlnImpl_81026814(int heap, const MlnBuffer *in, int lineLength, unsigned int lineEnd,
		     MlnBuffer **out)
{
	const unsigned char *src, *p;
	unsigned char *dst;
	MlnBuffer *encoded;
	unsigned int size, lineBytes, lineStride, breakSize, total, rest, i, o;
	int first, second, third;

	if (out == NULL || in == NULL || lineLength < 0 || (lineLength & 3) != 0 ||
	    lineEnd > MLN_BASE64_CR)
		return 3;
	size = in->size;
	if (size == 0) {
		encoded = MlnBufferCreate(heap, size, NULL);
		if (encoded == NULL)
			return 2;
		*out = encoded;
		return 0;
	}
	src = in->data;
	breakSize = lineEnd == MLN_BASE64_CRLF ? 2 : 1;
	lineBytes = (lineLength >> 2) * 3;
	lineStride = breakSize + lineLength;
	if (lineBytes == 0) {
		total = (size + 2) / 3 * 4;
	} else {
		total = size / lineBytes * lineStride;
		rest = size % lineBytes;
		if (rest != 0)
			total += breakSize + (rest + 2) / 3 * 4;
		total -= breakSize;
	}
	encoded = MlnBufferCreate(heap, total, NULL);
	if (encoded == NULL)
		return 2;
	dst = encoded->data;
	o = 0;
	i = 0;
	p = src;
	while (1) {
		if (lineBytes != 0 && o % lineStride == (unsigned int)lineLength) {
			if (lineEnd != MLN_BASE64_LF) {
				dst[o++] = '\r';
				if (lineEnd == MLN_BASE64_CR)
					goto line_done;
			}
			dst[o++] = '\n';
		}
line_done:
		first = src[i];
		dst[o] = MLN_BASE64_ALPHABET[first >> 2];
		first &= 3;
		if (i == size - 1) {
			dst[o + 1] = MLN_BASE64_ALPHABET[first << 4];
			dst[o + 2] = '=';
			dst[o + 3] = '=';
			break;
		}
		second = p[1];
		dst[o + 1] = MLN_BASE64_ALPHABET[first << 4 | second >> 4];
		second &= 0xf;
		if (i == size - 2) {
			dst[o + 2] = MLN_BASE64_ALPHABET[second << 2];
			dst[o + 3] = '=';
			break;
		}
		i += 3;
		third = p[2];
		dst[o + 2] = MLN_BASE64_ALPHABET[third >> 6 | second << 2];
		dst[o + 3] = MLN_BASE64_ALPHABET[third & 0x3f];
		o += 4;
		p += 3;
		if (size <= i)
			break;
	}
	*out = encoded;
	return 0;
}

int MlnImpl_81026A9C(int heap, const MlnBuffer *in, MlnBuffer **out)
{
	const unsigned char *p, *end;
	unsigned char *bytes;
	MlnBuffer *decoded;
	unsigned int size, bits, value, count, o;
	int ret;

	if (in == NULL || out == NULL)
		return 3;
	size = in->size;
	if (size == 0) {
		decoded = MlnBufferCreate(heap, size, NULL);
		if (decoded == NULL)
			return 2;
		*out = decoded;
		return 0;
	}
	p = in->data;
	bytes = MlnAllocate(heap, 3 * (size + 1) >> 2);
	if (bytes == NULL)
		return 2;
	bits = 0;
	end = p + size;
	count = 0;
	o = 0;
	do {
		value = s_base64Values[*p++];
		if (value > MLN_BASE64_PAD)
			continue;
		if (value == MLN_BASE64_PAD)
			break;
		bits = bits << 6 | value;
		if (++count == 4) {
			bytes[o] = bits >> 16;
			bytes[o + 2] = bits;
			bytes[o + 1] = bits >> 8;
			o += 3;
			bits = 0;
			count = 0;
		}
	} while (p != end);
	if (count == 2) {
		bytes[o++] = bits >> 4;
	} else if (count == 3) {
		bytes[o] = bits >> 10;
		bytes[o + 1] = bits >> 2;
		o += 2;
	}
	decoded = MlnBufferCreate(heap, o, bytes);
	if (decoded != NULL) {
		*out = decoded;
		ret = 0;
	} else {
		ret = 2;
	}
	MlnFree(heap, bytes);
	return ret;
}
