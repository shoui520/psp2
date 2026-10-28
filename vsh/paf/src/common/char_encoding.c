/* Conversion flags allow the prefix preceding specific CES errors. */
#include "char_encoding.h"
#include <ces/libces_ucs.h>
#include <ces/error.h>

int Utf8ToUtf16(const char *src, PafWString *dst, int flags)
{
	SceCesUcsContext context;
	uint32_t srcLen, dstLen;
	int result;

	if (!dst)
		return -1;
	if (!src) {
		PafWStringAssign(dst, NULL, 0);
		return -1;
	}
	sceCesUcsContextInit(&context);
	result = sceCesUtf8StrGetUtf16Len(&context, (const uint8_t *)src, 0,
					&srcLen, &dstLen);
	if (result &&
	    (!(flags & 0x10) || result != SCE_CES_ERROR_SRC_BUFFER_END) &&
	    (!(flags & 8) || result != SCE_CES_ERROR_INVALID_ENCODE) &&
	    (!(flags & 4) || result != SCE_CES_ERROR_OUT_OF_CODE_RANGE) &&
	    (!(flags & 2) || result != SCE_CES_ERROR_ILLEGAL_CODE))
		dstLen = 0;
	if (dstLen < dst->count)
		PafWStringErase(dst, dstLen, (size_t)-1);
	else if (dstLen > dst->count)
		PafWStringAppend(dst, dstLen - dst->count, 0);
	if (dst->count != dstLen) {
		PafWStringAssign(dst, NULL, 0);
		return -2;
	}
	if (dstLen)
		sceCesUtf8StrToUtf16Str(&context, (const uint8_t *)src, 0,
				       &srcLen, dst->ptr, dstLen + 1, &dstLen);
	return dstLen;
}

int Utf8StringToUtf16(const PafString *src, PafWString *dst, int flags)
{
	return Utf8ToUtf16(src->ptr, dst, flags);
}

int Utf16ToUtf8(const uint16_t *src, PafString *dst, int flags)
{
	SceCesUcsContext context;
	uint32_t srcLen, dstLen;
	int result;

	if (!dst)
		return -1;
	if (!src) {
		PafStringAssign(dst, NULL, 0);
		return -1;
	}
	sceCesUcsContextInit(&context);
	result = sceCesUtf16StrGetUtf8Len(&context, src, 0, &srcLen, &dstLen);
	if (result &&
	    (!(flags & 0x10) || result != SCE_CES_ERROR_SRC_BUFFER_END) &&
	    (!(flags & 8) || result != SCE_CES_ERROR_INVALID_ENCODE) &&
	    (!(flags & 4) || result != SCE_CES_ERROR_OUT_OF_CODE_RANGE) &&
	    (!(flags & 2) || result != SCE_CES_ERROR_ILLEGAL_CODE))
		dstLen = 0;
	if (dstLen < dst->count)
		PafStringErase(dst, dstLen, (size_t)-1);
	else if (dstLen > dst->count)
		PafStringAppend(dst, dstLen - dst->count, 0);
	if (dst->count != dstLen) {
		PafStringAssign(dst, NULL, 0);
		return -2;
	}
	if (dstLen)
		sceCesUtf16StrToUtf8Str(&context, src, 0, &srcLen,
				       (uint8_t *)dst->ptr, dstLen + 1, &dstLen);
	return dstLen;
}

int Utf16StringToUtf8(const PafWString *src, PafString *dst, int flags)
{
	return Utf16ToUtf8(src->ptr, dst, flags);
}

int Utf8ToUtf16Default(const char *src, PafWString *dst)
{
	return Utf8ToUtf16(src, dst, 0);
}

int Utf8StringToUtf16Default(const PafString *src, PafWString *dst)
{
	return Utf8ToUtf16(src->ptr, dst, 0);
}

int Utf16ToUtf8Default(const uint16_t *src, PafString *dst)
{
	return Utf16ToUtf8(src, dst, 0);
}

int Utf16StringToUtf8Default(const PafWString *src, PafString *dst)
{
	return Utf16ToUtf8(src->ptr, dst, 0);
}
