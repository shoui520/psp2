#ifndef PAF_TYPES_H
#define PAF_TYPES_H

#define SCE_PAF_OK 0
#define SCE_PAF_ERROR_CXML_NO_ALLOCATOR (-2136013555) /* 0x80AF050D */

typedef enum Locale {
	Locale_Default = -1,
	Locale_JP = 0,
	Locale_MIN = 0,
	Locale_EN = 1,
	Locale_FR = 2,
	Locale_ES = 3,
	Locale_DE = 4,
	Locale_IT = 5,
	Locale_NL = 6,
	Locale_PT = 7,
	Locale_RU = 8,
	Locale_KO = 9,
	Locale_ZH_T = 10,
	Locale_ZH_S = 11,
	Locale_FI = 12,
	Locale_SV = 13,
	Locale_DA = 14,
	Locale_NO = 15,
	Locale_PL = 16,
	Locale_TR = 17,
	Locale_MAX = 17
} Locale;

#endif
