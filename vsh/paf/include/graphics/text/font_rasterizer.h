/*
 * paf::graph::FontRasterizer: what draws a text style's glyphs, one per text thread: the PVF
 * fonts' (FontRasterizerPvf) and the emoji font's (FontRasterizerImage). FontRasterizer is
 * Sony's (TextLayout::Update's mangled name); FontRasterizerPvf after Sony's
 * ScePafFontRasterizerPvfHeap and ScePafFontRasterizerPvfMutex, FontRasterizerImage after the
 * emoji font's file imagefont.bin, their params and the methods no export names are ours.
 * The rasterizers are not in C yet: their members are left as bytes until they are.
 */
#ifndef PAF_GRAPHICS_TEXT_FONT_RASTERIZER_H
#define PAF_GRAPHICS_TEXT_FONT_RASTERIZER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/text/text_style.h>
#include <std/string>

namespace paf {
namespace graph {

class FontRasterizer {
public:
	FontRasterizer();
	virtual ~FontRasterizer();
	/* the font made again for the style set */
	virtual int32_t Update();
	virtual int32_t SetStyle(const common::SharedPtr<TextStyle> &style);
	/* the style let go, back to the default one */
	virtual int32_t ResetStyle();
};

class FontRasterizerPvf : public FontRasterizer {
public:
	/* the fonts the PVF library opens: from memory, else from their files */
	class FontParam {
	public:
		FontParam()
		{
		}

		virtual ~FontParam()
		{
		}

		void *mem_japanese_addr;
		uint32_t mem_japanese_size;
		void *mem_latin_addr;
		uint32_t mem_latin_size;
		void *mem_hangul_addr;
		uint32_t mem_hangul_size;
		void *mem_chinese_addr;
		uint32_t mem_chinese_size;
		char *path_japanese;
		char *path_latin;
		uint32_t reserved[5];
	};

	class InitParam {
	public:
		InitParam()
		{
		}

		virtual ~InitParam()
		{
		}

		uint32_t heap_size;
		uint32_t option;
		/* the Locale */
		int32_t locale;
		FontParam font;
	};

	FontRasterizerPvf();

	/* the PVF library and its heap, set up once for every rasterizer */
	static int32_t Init(const InitParam &param);
	static int32_t Term();
	static int32_t SetLocale(int32_t locale);

	uint8_t m_data[0x54];
};

class FontRasterizerImage : public FontRasterizer {
public:
	/* the font read from its file; own_data: the font's memory is the rasterizer's */
	FontRasterizerImage(const string &path, bool own_data);
	/* the font already in memory */
	FontRasterizerImage(void *addr, uint32_t size);

	uint8_t m_data[0x30];
};

}
}

#endif
