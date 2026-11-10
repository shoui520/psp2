/*
 * libSceCes's UCS settings (Sony's internal/usermode/src/ces/libces/src/UCS/ces_ucs_setting.h):
 * the byte order and byte order mark policy of a UCS context. Laid out and named from the
 * library's debug information (SDK 3.570.011 libSceCes.a).
 */
#ifndef _CES_UCS_SETTING_H
#define _CES_UCS_SETTING_H

#include <stdint.h>

/* the policies of detectBom and outputBom: the SDK's libces_ucs.h has the same values */
#define SCE_CES_DETECT_DISABLE	(0)
#define SCE_CES_DETECT_ENABLE	(1)

#define SCE_CES_OUTPUT_DISABLE	(0)
#define SCE_CES_OUTPUT_ENABLE	(1)
#define SCE_CES_OUTPUT_TOP_CUT	(2)
#define SCE_CES_OUTPUT_TOP_SET	(3)

typedef struct SceCesUcsSetting {
	uint8_t ucs2;
	uint8_t ucs4;
	uint8_t utf16;
	uint8_t utf32;
	uint8_t detectBom;
	uint8_t outputBom;
	uint8_t padding;
	uint8_t utf7;
} SceCesUcsSetting;

#endif /* _CES_UCS_SETTING_H */
