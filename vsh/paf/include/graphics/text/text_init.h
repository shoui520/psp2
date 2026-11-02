/*
 * The text system's setup: the PVF fonts, the text surface cache and the emoji
 * font, made once by the Framework.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_INIT_H
#define PAF_GRAPHICS_TEXT_TEXT_INIT_H

#include <stdint.h>

namespace paf {
namespace graph {

class TextInitParam {
public:
	TextInitParam();

	uint32_t text_surface_pool_size;
	uint32_t pvf_heap_size;
	uint32_t pvf_option;
	/* the Locale, -1 for the system's */
	int32_t locale;
	void *mem_pvf_japanese_addr;
	uint32_t mem_pvf_japanese_size;
	void *mem_pvf_latin_addr;
	uint32_t mem_pvf_latin_size;
	void *mem_pvf_hangul_addr;
	uint32_t mem_pvf_hangul_size;
	void *mem_pvf_chinese_addr;
	uint32_t mem_pvf_chinese_size;
	char *path_pvf_japanese;
	char *path_pvf_latin;
	/* handed to the font library with the fields before them */
	uint32_t pvf_param[5];
	char *path_pvf_hangul;
	char *path_pvf_chinese;
	/* the emoji font, vs0:data/external/font/emoji/imagefont.bin when NULL */
	char *path_emoji_font;
};

int32_t InitText(const TextInitParam &param);
/* the text system torn down (0x81092608, name ours) */
void TermText();
/* the threads that lay out text: stopped while the text settings change */
void StopTextThreads();
int32_t StartTextThreads();
/* the Locale the text is laid out for, set with the text threads stopped */
void SetTextLocale(int32_t locale);

}
}

#endif
