/*
 * paf::graph::FontRasterizer: what draws a text style's glyphs, one per text thread: the PVF
 * fonts' (FontRasterizerPvf) and the emoji font's (FontRasterizerImage). FontRasterizer is
 * Sony's (TextLayout::Update's mangled name); FontRasterizerPvf after Sony's
 * ScePafFontRasterizerPvfHeap and ScePafFontRasterizerPvfMutex, FontRasterizerImage after the
 * emoji font's file imagefont.bin, their params and the methods no export names are ours.
 * FontMetrics, CharMetrics and the methods TextLayout calls through the vtable (GetFontMetrics,
 * GetCharMetrics, Rasterize) are named by us. FontRasterizerPvf is not in C yet: its members
 * are left as bytes until it is.
 */
#ifndef PAF_GRAPHICS_TEXT_FONT_RASTERIZER_H
#define PAF_GRAPHICS_TEXT_FONT_RASTERIZER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <graphics/text/text_style.h>
#include <std/map>
#include <std/memory>
#include <std/string>

namespace paf {
namespace graph {

class Surface;
class TextSurfaceCache;

/* the font of the style set: its line height and ascender at the scale it is drawn at (names
   ours) */
class FontMetrics {
public:
	FontMetrics() : height(0.0f), ascender(0.0f), scale(1.0f)
	{
	}

	virtual ~FontMetrics()
	{
	}

	float height;
	float ascender;
	float scale;
};

/* a character's glyph: where its ink starts and ends and how far it advances, in its font
   (names ours) */
class CharMetrics {
public:
	CharMetrics() : left(0.0f), right(0.0f), advance(0.0f)
	{
	}

	virtual ~CharMetrics()
	{
	}

	float left;
	float right;
	float advance;
	FontMetrics font;
};

class FontRasterizer {
public:
	FontRasterizer();
	virtual ~FontRasterizer();
	/* the font made again for the style set */
	virtual int32_t Update() = 0;
	virtual int32_t SetStyle(const common::SharedPtr<TextStyle> &style) = 0;
	/* the style let go, back to the default one */
	virtual int32_t ResetStyle() = 0;
	virtual int32_t GetFontMetrics(FontMetrics &metrics) = 0;
	virtual int32_t GetCharMetrics(wchar_t c, CharMetrics &metrics) = 0;
	/* the len characters drawn into a surface of the cache, width by height, found again
	   by the string's hash */
	virtual int32_t Rasterize(TextSurfaceCache *cache, int32_t width, int32_t height,
				  const wchar_t *str, uint32_t len, uint32_t hash,
				  intrusive_ptr<Surface> &surface) = 0;
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
	virtual ~FontRasterizerPvf();
	virtual int32_t Update();
	virtual int32_t SetStyle(const common::SharedPtr<TextStyle> &style);
	virtual int32_t ResetStyle();
	virtual int32_t GetFontMetrics(FontMetrics &metrics);
	virtual int32_t GetCharMetrics(wchar_t c, CharMetrics &metrics);
	virtual int32_t Rasterize(TextSurfaceCache *cache, int32_t width, int32_t height,
				  const wchar_t *str, uint32_t len, uint32_t hash,
				  intrusive_ptr<Surface> &surface);

	/* the PVF library and its heap, set up once for every rasterizer */
	static int32_t Init(const InitParam &param);
	static int32_t Term();
	static int32_t SetLocale(int32_t locale);

	uint8_t m_data[0x54];
};

/*
 * The emoji font, imagefont.bin: an index of glyphs by character, each a zlib-compressed
 * image of one or more frames of 8-bit colour indices with its colour table, drawn into an
 * indexed text surface (an animated one for several frames). Only GetCharMetrics and
 * Rasterize do anything.
 */
class FontRasterizerImage : public FontRasterizer {
public:
	/* the font read from its file; own_data: read whole into memory the rasterizer owns */
	FontRasterizerImage(const string &path, bool own_data);
	/* the font already in memory */
	FontRasterizerImage(void *addr, uint32_t size);
	virtual ~FontRasterizerImage();
	virtual int32_t Update();
	virtual int32_t SetStyle(const common::SharedPtr<TextStyle> &style);
	virtual int32_t ResetStyle();
	virtual int32_t GetFontMetrics(FontMetrics &metrics);
	virtual int32_t GetCharMetrics(wchar_t c, CharMetrics &metrics);
	virtual int32_t Rasterize(TextSurfaceCache *cache, int32_t width, int32_t height,
				  const wchar_t *str, uint32_t len, uint32_t hash,
				  intrusive_ptr<Surface> &surface);

private:
	/* the file's first bytes (names ours) */
	class Header {
	public:
		/* 0x100 */
		int16_t version;
		int16_t num_glyphs;
		/* where the glyph index starts */
		int32_t index_offset;
	};

	/* a character's entry in the glyph index (names ours) */
	class Glyph {
	public:
		/* the compressed image: where it is and its size */
		int32_t offset;
		int16_t size;
		/* its size inflated */
		int16_t image_size;
		uint16_t code;
		int16_t width;
		int16_t height;
		/* bit 0: the surface keeps alpha only */
		int16_t flags;
	};

	typedef map<uint16_t, Glyph *> GlyphMap;

	/* the file, or the memory it was read into, opened */
	int32_t Open();
	int32_t Close();
	/* the glyph index read and mapped by character */
	int32_t LoadIndex();

	string m_path;
	bool m_own_data;
	void *m_data;
	uint32_t m_size;
	common::SharedPtr<File> m_file;
	Glyph *m_glyphs;
	GlyphMap m_glyph_map;
};

}
}

#endif
