/*
 * SceNpCommonPs4: JSON strings (npext/json/json_string.cpp): a text escaped for a JSON string, and a JSON
 * string's characters unescaped, one at a time or all.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_9AD6061C(char *dst, SceSize dstSize, const char *src, SceSize srcLength)
{
	SceSize i;
	SceSize length;
	SceSize written;
	int c;
	char buf[7];

	i = 0;
	if (dst == NULL || dstSize == 0 || src == NULL || srcLength == 0)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	if (dstSize < 7)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
	written = 0;
	while (i < srcLength) {
		if (written > dstSize - 7)
			return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
		c = (unsigned char)*src++;
		i++;
		if (c == 0)
			break;
		switch (c) {
		case '"':
			length = sceClibStrnlen("\\\"", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\\"", length);
			break;
		case '\\':
			length = sceClibStrnlen("\\\\", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\\\", length);
			break;
		case '/':
			length = sceClibStrnlen("\\/", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\/", length);
			break;
		case '\b':
			length = sceClibStrnlen("\\b", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\b", length);
			break;
		case '\t':
			length = sceClibStrnlen("\\t", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\t", length);
			break;
		case '\n':
			length = sceClibStrnlen("\\n", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\n", length);
			break;
		case '\f':
			length = sceClibStrnlen("\\f", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\f", length);
			break;
		case '\r':
			length = sceClibStrnlen("\\r", 0xFFFFFFFF);
			sceClibStrncat(dst, "\\r", length);
			break;
		default:
			sceClibMemset(buf, 0, 7);
			if (c < 32 || c == 127)
				sceClibSnprintf(buf, 7, "\\u%04x", c);
			else
				sceClibSnprintf(buf, 7, "%c", c);
			length = sceClibStrnlen(buf, 0xFFFFFFFF);
			sceClibStrncat(dst, buf, length);
			break;
		}
		written += length;
	}
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonParseHex4(const char *src, unsigned short *pCode)
{
	int code;
	int shift;
	int i;
	int c;
	unsigned char digit;

	*pCode = 0;
	code = 0;
	shift = 0;
	for (i = 0; i < 4; i++) {
		c = *src;
		if (c >= 'A' && c <= 'F') {
			digit = c - 55;
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 87;
		} else if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else {
			*pCode = 0;
			return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
		}
		code |= digit << (12 - shift);
		*pCode = code;
		src++;
		shift += 4;
	}
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonEncodeUtf8(unsigned int code, char *dst)
{
	if (code < 0x80) {
		dst[0] = code;
		dst[1] = 0;
		return 1;
	}
	if (code < 0x800) {
		dst[0] = (code >> 6) | 0xC0;
		dst[1] = (code & 0x3F) | 0x80;
		dst[2] = 0;
		return 2;
	}
	if (code < 0x10000) {
		dst[0] = (code >> 12) | 0xE0;
		dst[1] = ((code >> 6) & 0x3F) | 0x80;
		dst[2] = (code & 0x3F) | 0x80;
		dst[3] = 0;
		return 3;
	}
	if (code >= 0x200000)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
	dst[0] = (code >> 18) | 0xF0;
	dst[1] = ((code >> 12) & 0x3F) | 0x80;
	dst[2] = ((code >> 6) & 0x3F) | 0x80;
	dst[3] = (code & 0x3F) | 0x80;
	dst[4] = 0;
	return 4;
}

int SceNpCommonPs4_1A8B2250(char *dst, SceSize dstSize, const char *src, int srcLength, SceSize *pLength)
{
	unsigned short code;
	char encoded[8];
	const char *end;
	const char *p;
	SceSize count;
	SceSize one;
	SceSize k;
	unsigned char write;
	int result;
	int length;
	int c;
	int i;
	unsigned short high;

	code = 0;
	write = 1;
	one = 1;
	if (src == NULL || srcLength == 0)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (pLength == NULL)
			return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
		write = 0;
	}
	count = one;
	result = 0;
	end = src + srcLength;
	if (src < end) {
		while (count <= dstSize || !write) {
			c = *src;
			if (c == '\\') {
			for (k = one, p = src + 1; p != end;) {
				++k;
				++p;
				if (k < 2)
					continue;
				c = src[1];
				if (c == '"') {
					if (write)
						*dst++ = '"';
					src += 2;
					goto next;
				}
				if (c == '\\') {
					if (write)
						*dst++ = '\\';
					src += 2;
					goto next;
				}
				if (c == '/') {
					if (write)
						*dst++ = '/';
					src += 2;
					goto next;
				}
				if (c == 'b') {
					if (write)
						*dst++ = '\b';
					src += 2;
					goto next;
				}
				if (c == 'f') {
					if (write)
						*dst++ = '\f';
					src += 2;
					goto next;
				}
				if (c == 'n') {
					if (write)
						*dst++ = '\n';
					src += 2;
					goto next;
				}
				if (c == 'r') {
					if (write)
						*dst++ = '\r';
					src += 2;
					goto next;
				}
				if (c == 't') {
					if (write)
						*dst++ = '\t';
					src += 2;
					goto next;
				}
				if (c != 'u')
					break;
				for (k = 2, p = src + 2; p != end;) {
					++k;
					++p;
					if (k < 6)
						continue;
					result = npCommonPs4JsonParseHex4(src + 2, &code);
					if (result < 0)
						goto literal;
					high = code;
					if ((code & -0x400) == 0xD800) {
						for (k = 7, p = src + 7; p != end;) {
							++k;
							++p;
							if (k < 12)
								continue;
							if (src[6] != '\\' || src[7] != 'u')
								goto literal;
							result = npCommonPs4JsonParseHex4(src + 8, &code);
							if (result < 0 || (code & -0x400) != 0xDC00)
								goto literal;
							length = npCommonPs4JsonEncodeUtf8((code & 0x3FF | ((high & 0x3FF) << 10)) + 0x10000, encoded);
							if (length <= 0)
								goto literal;
							if (write) {
								for (i = length; i != 0; i--)
									*dst++ = encoded[length - i];
							}
							src += 12;
							count += length - 1;
							goto next;
						}
						goto literal;
					}
					length = npCommonPs4JsonEncodeUtf8(code, encoded);
					if (length <= 0)
						goto literal;
					if (write) {
						for (i = length; i != 0; i--)
							*dst++ = encoded[length - i];
					}
					src += 6;
					count += length - 1;
					goto next;
				}
				break;
			}
literal:
			if (write)
				*dst++ = *src;
			src++;
			} else {
				if (write)
					*dst++ = c;
				src++;
			}
next:
			count++;
			if (src >= end)
				goto done;
		}
		return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
	}
done:
	if (count <= dstSize) {
		if (pLength != NULL)
			*pLength = count;
		if (write)
			*dst = 0;
	} else {
		result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
	}
	return result;
}

int SceNpCommonPs4_12AA95E3(void *dst, SceSize dstSize, const char *src, SceSize srcLength, SceSize *pLength, int *pConsumed)
{
	unsigned short code;
	char encoded[8];
	char buf[8];
	int consumed;
	SceSize length;
	int write;
	int result;
	int c;
	int e;
	unsigned short high;

	consumed = 0;
	length = 0;
	write = 1;
	code = 0;
	if (src == NULL || srcLength == 0)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (pLength == NULL)
			return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
		write = 0;
	}
	c = *src;
	if ((c & 0x80) != 0) {
		if ((c & 0xE0) == 0xC0 && (unsigned char)c >= 0xC2) {
			if (srcLength < 2) {
				result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
				consumed = 2;
				goto out;
			}
			if ((src[1] & 0xC0) == 0x80) {
				length = 2;
				consumed = 2;
				goto convert;
			}
		}
		if ((c & 0xF0) == 0xE0) {
			if (srcLength < 3) {
				result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
				consumed = 3;
				goto out;
			}
			if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80) {
				consumed = 3;
				length = 3;
				goto convert;
			}
		}
		if ((c & 0xF8) == 0xF0) {
			if (srcLength < 4) {
				result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
				consumed = 4;
				goto out;
			}
			if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
				consumed = 4;
				length = 4;
			}
		} else {
			length = 1;
			consumed = 1;
		}
	} else {
		length = 1;
		if (c == '\\') {
			if (srcLength < 2) {
				result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
				consumed = 2;
				goto out;
			}
			e = src[1];
			if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') {
				consumed = 2;
			} else if (e == 'u') {
				if (npCommonPs4JsonParseHex4(src + 2, &code) < 0) {
					result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
					consumed = 6;
					goto out;
				}
				high = code;
				if ((code & -0x400) == 0xD800) {
					if (srcLength < 12) {
						result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
						consumed = 12;
						goto out;
					}
					if (npCommonPs4JsonParseHex4(src + 8, &code) < 0 || (code & -0x400) != 0xDC00) {
						result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
						consumed = 12;
						goto out;
					}
					length = npCommonPs4JsonEncodeUtf8((code & 0x3FF | ((high & 0x3FF) << 10)) + 0x10000, encoded);
					consumed = 12;
				} else {
					length = npCommonPs4JsonEncodeUtf8(code, encoded);
					consumed = 6;
				}
			} else {
				consumed = 1;
			}
		} else {
			consumed = 1;
		}
	}
convert:
	if (length > dstSize && write) {
		result = NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
		goto out;
	}
	result = SceNpCommonPs4_1A8B2250((char *)buf, 7, src, srcLength, NULL);
	if (result < 0)
		return result;
	if (write)
		sceClibMemcpy_safe(dst, buf, length);
out:
	if (pLength != NULL)
		*pLength = length;
	if (pConsumed != NULL)
		*pConsumed = consumed;
	return result;
}

}
