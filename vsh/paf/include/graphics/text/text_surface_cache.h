/*
 * paf::graph::TextSurfaceCache: the surfaces the text threads draw glyphs into, kept for
 * reuse in a surface pool of its own (Sony's name, from TextLayout::Update's mangled name; its
 * methods ours), and the surfaces it keeps. The cache's entries are a list, the latest used
 * first, and a map from a string's hash to the first entry of that hash.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H
#define PAF_GRAPHICS_TEXT_TEXT_SURFACE_CACHE_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image_types.h>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <std/string>

namespace paf {
namespace graph {

class TextStyle;

/* a surface the cache keeps a glyph in (names ours) */
class TextSurface : public Surface {
public:
	TextSurface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		    ImageOrder order, int32_t num_page, int32_t num_level, bool reserved,
		    bool alpha_mask);
	virtual ~TextSurface();
	/* told before the glyph is drawn: nothing */
	virtual int32_t Prepare();

	/* a font's glyph, drawn in the text's color; else an image's (the emoji), drawn as it is */
	bool IsAlphaMask();

	bool m_alpha_mask;
};

/* an animated glyph: a page per frame, each shown for its delay in milliseconds (names ours) */
class TextSurfaceAnim : public TextSurface {
public:
	TextSurfaceAnim(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			ImageOrder order, int32_t num_page, int32_t num_level, bool reserved,
			bool alpha_mask);
	virtual ~TextSurfaceAnim();
	/* the next frame shown once the frame's delay has passed */
	virtual int32_t Prepare();

	int32_t SetFrameDelay(int32_t frame, float delay);

	float *m_frame_delays;
	/* when the frame was shown, in milliseconds of the minute; FRAME_TIME_NONE at first */
	float m_frame_time;
};

class TextSurfaceCache {
public:
	/* a glyph surface and what it was drawn from (names ours) */
	class Entry {
	public:
		Entry() : str(L""), hash(0)
		{
		}

		virtual ~Entry()
		{
		}

		common::SharedPtr<TextStyle> style;
		wstring str;
		uint32_t hash;
		intrusive_ptr<Surface> surface;
	};

	typedef list<Entry *> EntryList;
	typedef map<uint32_t, EntryList::iterator> EntryMap;

	/* over size bytes of memory at mem, the pool named name */
	TextSurfaceCache(void *mem, uint32_t size, const char *name);
	virtual ~TextSurfaceCache();

	/* every surface kept let go */
	int32_t Clear();
	/* the surface kept for str in a style of the same font, made the latest used */
	int32_t Find(int32_t width, int32_t height, const common::SharedPtr<TextStyle> &style,
		     const wchar_t *str, uint32_t len, uint32_t hash, intrusive_ptr<Surface> &surface);
	/* a new surface kept for str, the oldest surfaces nobody else holds let go to make room */
	int32_t Allocate(int32_t width, int32_t height, ImageMode format, int32_t num_page,
			 bool alpha_mask, const common::SharedPtr<TextStyle> &style,
			 const wchar_t *str, uint32_t len, uint32_t hash,
			 intrusive_ptr<SurfaceCLUT> &clut, intrusive_ptr<Surface> &surface);

	SurfacePool *m_pool;
	EntryList m_list;
	EntryMap m_map;
	/* the most surfaces kept: one per 4 KiB of the pool */
	uint32_t m_max;
};

/* the hash a string's glyph surface is kept under (name ours) */
uint32_t HashString(const wchar_t *str, uint32_t len);

}
}

#endif
