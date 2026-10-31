/* paf::common character set conversions between UTF-8 and UTF-16 strings. */
#ifndef PAF_COMMON_CHAR_ENCODING_H
#define PAF_COMMON_CHAR_ENCODING_H

#include <std/string>

namespace paf {
namespace common {

int Utf8ToUtf16(const char *src, wstring *dst, int flags);
int Utf8ToUtf16(const string &src, wstring *dst, int flags);
int Utf16ToUtf8(const wchar_t *src, string *dst, int flags);
int Utf16ToUtf8(const wstring &src, string *dst, int flags);
int Utf8ToUtf16(const char *src, wstring *dst);
int Utf8ToUtf16(const string &src, wstring *dst);
int Utf16ToUtf8(const wchar_t *src, string *dst);
int Utf16ToUtf8(const wstring &src, string *dst);

}
}

#endif
