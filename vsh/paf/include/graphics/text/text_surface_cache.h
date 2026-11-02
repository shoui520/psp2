/*
 * paf::graph::TextSurfaceCache: the surfaces the text threads draw glyphs into, kept for
 * reuse in a surface pool of its own (Sony's name, from TextLayout::Update's mangled name; its
 * methods ours), and the surfaces it keeps. Not in C yet: its members are left as bytes until
 * it is.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H
#define PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H

#include <stdint.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {

/* a surface the cache keeps a glyph in (names ours) */
class TextSurface : public Surface {
public:
	virtual ~TextSurface();
	/* told before the glyph is drawn: nothing */
	virtual int32_t Prepare();

	/* a font's glyph, drawn in the text's color; else an image's (the emoji), drawn as it is */
	bool IsAlphaMask();

	bool m_alpha_mask;
};

class TextSurfaceCache {
public:
	/* over size bytes of memory at mem, the pool named name */
	TextSurfaceCache(void *mem, uint32_t size, const char *name);
	virtual ~TextSurfaceCache();

	/* every surface kept let go */
	int32_t Clear();

	uint8_t m_data[0x20];
};

/* the hash a string's glyph surface is kept under (name ours) */
uint32_t HashString(const wchar_t *str, uint32_t len);

}
}

#endif
