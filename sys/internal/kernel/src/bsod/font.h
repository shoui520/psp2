#ifndef BSOD_FONT_H
#define BSOD_FONT_H

/* Both native fonts contain 128 ASCII glyphs, each with sixteen rows.
 * Keep their adjacency: wide-font character128 selects narrow-font glyph0.
 * Narrow-font characters above127 read subsequent linker data, as in Sony's
 * unclamped renderer. Such bytes depend on the compiler/linker layout.
 */
typedef struct BsodFontStorage {
	uint8_t wide[128][16];
	uint8_t narrow[128][16];
} BsodFontStorage;

_Static_assert(offsetof(BsodFontStorage, narrow) == 2048, "BSOD font adjacency");
static const BsodFontStorage s_bsodFonts = {
	.wide = {
#include "font8_glyphs.h"
	},
	.narrow = {
#include "font6_glyphs.h"
	},
};
#define s_bsodFont8 ((const uint8_t *)&s_bsodFonts)
#define s_bsodFont6 ((const uint8_t *)&s_bsodFonts + offsetof(BsodFontStorage, narrow))

#endif
