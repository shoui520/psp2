/*
 * libSceCes's GB.c (Sony's internal/usermode/src/ces/libces/src/GB): a two-byte GBK code's index
 * and back. The GB2312 area (first and second byte 0xA1-0xFE) takes the first 94 * 94 indexes,
 * then the first bytes 0x81-0xA0 (190 second bytes each: 0x40-0x7E, 0x80-0xFE), then 0xA1-0xFE
 * with the 96 second bytes 0x40-0x7E, 0x80-0xA0.
 */
#include <stdint.h>
#include "ces_gb_profile.h"

#define CES_GB2312_CHARS	(94 * 94)		/* 8836 */
#define CES_GBK_ROW_81_A0	(CES_GB2312_CHARS + 32 * 190)	/* 14916: rows 0x81-0xA0 */

uint16_t _sceCesGBGetIndex(uint16_t gbCode)
{
	uint8_t gbc1 = gbCode >> 8;
	uint8_t gbc2 = gbCode;
	int32_t index;

	if (gbc1 < 0xa1) {
		index = (gbc1 - 0x81) * 190;
		if (gbc2 >= 0x40 && gbc2 <= 0x7e)
			index += gbc2 - 0x40;
		else if (gbc2 >= 0x80 && gbc2 <= 0xfe)
			index += gbc2 - 0x41;
	} else {
		index = (gbc1 - 0xa1) * 96 + 32 * 190;
		if (gbc2 >= 0x40 && gbc2 <= 0x7e)
			index += gbc2 - 0x40;
		else if (gbc2 >= 0x80 && gbc2 <= 0xa0)
			index += gbc2 - 0x41;
		else
			return (gbc1 - 0xa1) * 94 + gbc2 - 0xa1;
	}
	return index + CES_GB2312_CHARS;
}

uint16_t _sceCesGBGetIndexCode(uint16_t index)
{
	uint16_t gbc;
	int i;

	if (index < CES_GB2312_CHARS)
		return ((index / 94) << 8) + index % 94 + 0xa1a1;
	if (index < CES_GBK_ROW_81_A0) {
		i = index - CES_GB2312_CHARS;
		gbc = ((i / 190) << 8) + 0x8100;
		i %= 190;
	} else {
		i = index - CES_GBK_ROW_81_A0;
		gbc = ((i / 96) << 8) + 0xa100;
		i %= 96;
	}
	gbc += i;
	/* the second byte skips 0x7F */
	if (i < 63)
		return gbc + 0x40;
	return gbc + 0x41;
}
