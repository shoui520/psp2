/*
 * libime/libime_api.h: the IME's constants under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libime/libime_api.h), as far as the tree uses them.
 */
#ifndef _SCE_IME_LIBIME_API_H
#define _SCE_IME_LIBIME_API_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SCE_IME_LANGUAGE_DANISH               0x00000001ULL
#define SCE_IME_LANGUAGE_GERMAN               0x00000002ULL
#define SCE_IME_LANGUAGE_ENGLISH_US           0x00000004ULL
#define SCE_IME_LANGUAGE_SPANISH              0x00000008ULL
#define SCE_IME_LANGUAGE_FRENCH               0x00000010ULL
#define SCE_IME_LANGUAGE_ITALIAN              0x00000020ULL
#define SCE_IME_LANGUAGE_DUTCH                0x00000040ULL
#define SCE_IME_LANGUAGE_NORWEGIAN            0x00000080ULL
#define SCE_IME_LANGUAGE_POLISH               0x00000100ULL
#define SCE_IME_LANGUAGE_PORTUGUESE_PT        0x00000200ULL
#define SCE_IME_LANGUAGE_RUSSIAN              0x00000400ULL
#define SCE_IME_LANGUAGE_FINNISH              0x00000800ULL
#define SCE_IME_LANGUAGE_SWEDISH              0x00001000ULL
#define SCE_IME_LANGUAGE_JAPANESE             0x00002000ULL
#define SCE_IME_LANGUAGE_KOREAN               0x00004000ULL
#define SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE   0x00008000ULL
#define SCE_IME_LANGUAGE_TRADITIONAL_CHINESE  0x00010000ULL
#define SCE_IME_LANGUAGE_PORTUGUESE_BR        0x00020000ULL
#define SCE_IME_LANGUAGE_ENGLISH_GB           0x00040000ULL
#define SCE_IME_LANGUAGE_TURKISH              0x00080000ULL

#define SCE_IME_TYPE_DEFAULT             0
#define SCE_IME_TYPE_BASIC_LATIN         1
#define SCE_IME_TYPE_NUMBER              2
#define SCE_IME_TYPE_EXTENDED_NUMBER     3
#define SCE_IME_TYPE_URL                 4
#define SCE_IME_TYPE_MAIL                5
#define SCE_IME_ENTER_LABEL_DEFAULT 0
#define SCE_IME_ENTER_LABEL_SEND    1
#define SCE_IME_ENTER_LABEL_SEARCH  2
#define SCE_IME_ENTER_LABEL_GO      3

#define SCE_IME_OPTION_MULTILINE               0x01
#define SCE_IME_OPTION_NO_AUTO_CAPITALIZATION  0x02
#define SCE_IME_OPTION_NO_ASSISTANCE           0x04
#define SCE_IME_EVENT_OPEN             0
#define SCE_IME_EVENT_UPDATE_TEXT      1
#define SCE_IME_EVENT_UPDATE_CARET     2
#define SCE_IME_EVENT_CHANGE_SIZE      3
#define SCE_IME_EVENT_PRESS_CLOSE      4
#define SCE_IME_EVENT_PRESS_ENTER      5
#define SCE_IME_WORK_BUFFER_SIZE      (20*1024)

/* the IME text filter */
typedef SceInt32 (*SceImeTextFilter)(
	SceWChar16 *outText, SceUInt32 *outTextLength,
	const SceWChar16 *srcText, SceUInt32 srcTextLength);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SCE_IME_LIBIME_API_H */
