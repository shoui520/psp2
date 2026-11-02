/*
 * paf::graph::TextSurfaceCache: the surfaces the text threads draw glyphs into, kept for
 * reuse in a surface pool of its own (Sony's name, from TextLayout::Update's mangled name; its
 * methods ours). Not in C yet: its members are left as bytes until it is.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H
#define PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H

#include <stdint.h>

namespace paf {
namespace graph {

class TextSurfaceCache {
public:
	/* over size bytes of memory at mem, the pool named name */
	TextSurfaceCache(void *mem, uint32_t size, const char *name);
	virtual ~TextSurfaceCache();

	/* every surface kept let go */
	int32_t Clear();

	uint8_t m_data[0x20];
};

}
}

#endif
