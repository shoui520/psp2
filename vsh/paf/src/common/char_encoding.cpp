/*
 * UTF-8 <-> UTF-16 through libces. The flags let a conversion keep the prefix
 * before a truncated (0x10), invalid (8), out-of-range (4) or illegal (2) code.
 */
#include <common/char_encoding.h>
#include <ces/libces_ucs.h>
#include <ces/error.h>

namespace paf {
namespace common {

static inline bool ConvertError(int result, int flags)
{
	return result != 0 &&
	       (!(flags & 0x10) || result != SCE_CES_ERROR_SRC_BUFFER_END) &&
	       (!(flags & 8) || result != SCE_CES_ERROR_INVALID_ENCODE) &&
	       (!(flags & 4) || result != SCE_CES_ERROR_OUT_OF_CODE_RANGE) &&
	       (!(flags & 2) || result != SCE_CES_ERROR_ILLEGAL_CODE);
}

int Utf8ToUtf16(const char *src, wstring *dst, int flags)
{
	SceCesUcsContext context;
	SceUInt32 src_len, dst_len;
	int result = -1;

	if (dst == NULL)
		return -1;
	if (src != NULL) {
		sceCesUcsContextInit(&context);
		if (ConvertError(sceCesUtf8StrGetUtf16Len(&context, (const SceUInt8 *)src, 0,
							  &src_len, &dst_len), flags))
			dst_len = 0;
		dst->resize(dst_len);
		if (dst->size() == dst_len) {
			if (dst_len != 0)
				sceCesUtf8StrToUtf16Str(&context, (const SceUInt8 *)src, 0, &src_len,
							(SceUInt16 *)dst->ptr, dst_len + 1, &dst_len);
			return dst_len;
		}
		result = -2;
	}
	dst->clear();
	return result;
}

int Utf8ToUtf16(const string &src, wstring *dst, int flags)
{
	return Utf8ToUtf16(src.c_str(), dst, flags);
}

int Utf16ToUtf8(const wchar_t *src, string *dst, int flags)
{
	SceCesUcsContext context;
	SceUInt32 src_len, dst_len;
	int result = -1;

	if (dst == NULL)
		return -1;
	if (src != NULL) {
		sceCesUcsContextInit(&context);
		if (ConvertError(sceCesUtf16StrGetUtf8Len(&context, (const SceUInt16 *)src, 0,
							  &src_len, &dst_len), flags))
			dst_len = 0;
		dst->resize(dst_len);
		if (dst->size() == dst_len) {
			if (dst_len != 0)
				sceCesUtf16StrToUtf8Str(&context, (const SceUInt16 *)src, 0, &src_len,
							(SceUInt8 *)dst->ptr, dst_len + 1, &dst_len);
			return dst_len;
		}
		result = -2;
	}
	dst->clear();
	return result;
}

int Utf16ToUtf8(const wstring &src, string *dst, int flags)
{
	return Utf16ToUtf8(src.c_str(), dst, flags);
}

int Utf8ToUtf16(const char *src, wstring *dst)
{
	return Utf8ToUtf16(src, dst, 0);
}

int Utf8ToUtf16(const string &src, wstring *dst)
{
	return Utf8ToUtf16(src.c_str(), dst, 0);
}

int Utf16ToUtf8(const wchar_t *src, string *dst)
{
	return Utf16ToUtf8(src, dst, 0);
}

int Utf16ToUtf8(const wstring &src, string *dst)
{
	return Utf16ToUtf8(src.c_str(), dst, 0);
}

}
}
