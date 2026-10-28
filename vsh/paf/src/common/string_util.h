#ifndef PAF_COMMON_STRING_UTIL_H
#define PAF_COMMON_STRING_UTIL_H

#include "../std/string.h"

typedef struct CommonString {
	PafString m_string;
	PafWString m_wstring;
} CommonString;

typedef char CommonStringSizeCheck[sizeof(CommonString) == 24 ? 1 : -1];

#define CommonStringDestroy ScePafCommon_96777934
#define CommonStringConstruct ScePafCommon_298D25EE
#define CommonStringConstructBase ScePafCommon_D9F31F44
#define CommonStringConstructChar ScePafCommon_4107391F
#define CommonStringConstructCharBase ScePafCommon_E23D7BA7
#define CommonStringConstructWideChar ScePafCommon_5DF278C9
#define CommonStringConstructWideCharBase ScePafCommon_DB4B2313
#define CommonStringConstructString ScePafCommon_5AB8BCDE
#define CommonStringConstructStringBase ScePafCommon_CE934628
#define CommonStringConstructWString ScePafCommon_9C4ABDBE
#define CommonStringConstructWStringBase ScePafCommon_7607A514
#define CommonStringDestroyBase ScePafCommon_E2544213
#define CommonStringGetString ScePafCommon_0665AABD
#define CommonStringGetWString ScePafCommon_C4936D3E

CommonString *CommonStringDestroy(CommonString *self);
CommonString *CommonStringConstruct(CommonString *self);
CommonString *CommonStringConstructBase(CommonString *self);
CommonString *CommonStringConstructChar(CommonString *self, const char *str);
CommonString *CommonStringConstructCharBase(CommonString *self, const char *str);
CommonString *CommonStringConstructWideChar(CommonString *self, const uint16_t *str);
CommonString *CommonStringConstructWideCharBase(CommonString *self, const uint16_t *str);
CommonString *CommonStringConstructString(CommonString *self, const PafString *src);
CommonString *CommonStringConstructStringBase(CommonString *self, const PafString *src);
CommonString *CommonStringConstructWString(CommonString *self, const PafWString *src);
CommonString *CommonStringConstructWStringBase(CommonString *self, const PafWString *src);
CommonString *CommonStringDestroyBase(CommonString *self);
void CommonStringGetString(PafString *result, const CommonString *self);
void CommonStringGetWString(PafWString *result, const CommonString *self);

#endif
