#ifndef PAF_COMMON_CHAR_ENCODING_H
#define PAF_COMMON_CHAR_ENCODING_H

#include "../std/string.h"

#define Utf8ToUtf16 ScePafCommon_A9ED25D9
#define Utf8StringToUtf16 ScePafCommon_23A39B05
#define Utf16ToUtf8 ScePafCommon_5FAAAE1C
#define Utf16StringToUtf8 ScePafCommon_683BC8E8
#define Utf8ToUtf16Default ScePafCommon_950C3AA0
#define Utf8StringToUtf16Default ScePafCommon_3CFEB1EB
#define Utf16ToUtf8Default ScePafCommon_8E009780
#define Utf16StringToUtf8Default ScePafCommon_06C015C6

int Utf8ToUtf16(const char *src, PafWString *dst, int flags);
int Utf8StringToUtf16(const PafString *src, PafWString *dst, int flags);
int Utf16ToUtf8(const uint16_t *src, PafString *dst, int flags);
int Utf16StringToUtf8(const PafWString *src, PafString *dst, int flags);
int Utf8ToUtf16Default(const char *src, PafWString *dst);
int Utf8StringToUtf16Default(const PafString *src, PafWString *dst);
int Utf16ToUtf8Default(const uint16_t *src, PafString *dst);
int Utf16StringToUtf8Default(const PafWString *src, PafString *dst);

#endif
