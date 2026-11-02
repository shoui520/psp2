/*
 * paf::graph::TextSurfaceCache (the file's name ours): the glyph surfaces the text threads
 * draw, kept in a pool of their own and found again by the string's hash, the style's font and
 * the size. When the cache is full the oldest surfaces nobody else holds are let go.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/frame_clock.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/text/text_style.h>
#include <graphics/text/text_surface_cache.h>
#include <memory/heap_allocator.h>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <std/stdlib.h>
#include <std/string.h>
#include <timer/timer.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

/* a minute in milliseconds: the frame times wrap at it; m_frame_time is never one */
#define FRAME_TIME_WRAP		60000.0f
#define FRAME_TIME_NONE		60000.0f

/* a cache entry per 4 KiB of the pool */
#define CACHE_ENTRY_SHIFT	12

/* the colour tables of the indexed formats */
#define CLUT_ENTRY_INDEX4	16
#define CLUT_ENTRY_INDEX8	256

static FrameClock s_frame_clock;

TextSurfaceCache::TextSurfaceCache(void *mem, uint32_t size, const char *name) : m_pool(NULL)
{
	memory::HeapAllocator::Option option;

	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_dbgprint = true;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;
	m_max = size >> CACHE_ENTRY_SHIFT;
	m_pool = new SurfacePool(mem, size, name, &option);
}

int32_t TextSurfaceCache::Clear()
{
	for (EntryList::iterator it = m_list.begin(); it != m_list.end(); ++it)
		delete *it;
	m_list.clear();
	m_map.clear();
	return SCE_PAF_OK;
}

TextSurfaceCache::~TextSurfaceCache()
{
	Clear();
	delete m_pool;
}

int32_t TextSurfaceCache::Find(int32_t width, int32_t height,
			       const common::SharedPtr<TextStyle> &style, const wchar_t *str,
			       uint32_t len, uint32_t hash, intrusive_ptr<Surface> &surface)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;
	EntryMap::iterator found = m_map.find(hash);

	if (found == m_map.end())
		return ret;

	for (EntryList::iterator it = found->second; it != m_list.end(); ++it) {
		Entry *entry = *it;

		if (entry->hash != hash)
			continue;
		if (width != entry->surface->GetWidth() || height != entry->surface->GetHeight())
			continue;
		if (style.get() != entry->style.get() &&
		    (style.get() == NULL || entry->style.get() == NULL ||
		     !style->EqualFontType(*entry->style)))
			continue;
		if (entry->str.length() != len || sce_paf_wcsncmp(entry->str.c_str(), str, len) != 0)
			continue;

		m_list.erase(it);
		m_list.push_front(entry);
		found->second = m_list.begin();
		surface = entry->surface;
		ret = SCE_PAF_OK;
		break;
	}
	return ret;
}

int32_t TextSurfaceCache::Allocate(int32_t width, int32_t height, ImageMode format,
				   int32_t num_page, bool alpha_mask,
				   const common::SharedPtr<TextStyle> &style, const wchar_t *str,
				   uint32_t len, uint32_t hash, intrusive_ptr<SurfaceCLUT> &clut,
				   intrusive_ptr<Surface> &surface)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
	int32_t num_entry = 0;
	EntryList::iterator it = m_list.end();

	switch (format) {
	case ImageMode_index4:
		num_entry = CLUT_ENTRY_INDEX4;
		break;
	case ImageMode_index8:
		num_entry = CLUT_ENTRY_INDEX8;
		break;
	default:
		break;
	}

	/* the entries kept back while looking for room, the latest of each hash */
	EntryMap kept;

	for (;;) {
		/* full: room made first, unless every older entry is held */
		if (m_list.size() >= m_max && it != m_list.begin())
			goto release;
		{
			intrusive_ptr<SurfaceCLUT> new_clut;
			intrusive_ptr<Surface> new_surface;

			if (num_entry > 0) {
				if (clut.get() != NULL) {
					new_clut = clut;
				} else {
					new_clut = intrusive_ptr<SurfaceCLUT>(
						new SurfaceCLUT(m_pool, ImageMode_RGBA8888, num_entry));
					if (new_clut.get() == NULL)
						break;
					if (new_clut->GetCLUT() == NULL)
						new_clut.clear();
				}
			}
			if (num_entry == 0 || new_clut.get() != NULL) {
				if (num_page == 1)
					new_surface = intrusive_ptr<Surface>(
						new TextSurface(m_pool, width, height, format,
								ImageOrder_Linear, 1, 1, false,
								alpha_mask));
				else
					new_surface = intrusive_ptr<Surface>(
						new TextSurfaceAnim(m_pool, width, height, format,
								    ImageOrder_Linear, num_page, 1,
								    false, alpha_mask));
				if (new_surface.get() == NULL)
					break;
				if (new_surface->GetMemory() != NULL) {
					if (new_clut.get() != NULL)
						new_surface->AttachCLUT(new_clut);
				} else {
					new_surface.clear();
					new_clut.clear();
				}
				if (new_surface.get() != NULL) {
					Entry *entry = new Entry();

					if (entry == NULL)
						break;
					entry->style = style;
					entry->str.assign(str, len);
					entry->hash = hash;
					entry->surface = new_surface;
					m_list.push_front(entry);

					EntryList::iterator first = m_list.begin();

					m_map[hash] = first;
					surface = new_surface;
					ret = SCE_PAF_OK;
					break;
				}
			}
		}

		/* no room: the next older entry let go, unless someone else holds its surface */
release:
		if (it == m_list.begin()) {
			sce_paf_wprintf_dummy();
			break;
		}
		--it;

		Entry *entry = *it;

		if (entry->surface->GetRefCount() <= 1) {
			EntryMap::iterator found = m_map.find(entry->hash);

			if (found != m_map.end() && found->second == it) {
				EntryMap::iterator latest = kept.find(entry->hash);

				if (latest != kept.end())
					found->second = latest->second;
				else
					m_map.erase(found);
			}
			it = m_list.erase(it);
			delete entry;
			continue;
		}
		kept[entry->hash] = it;
		if (m_list.size() < m_max || it != m_list.begin())
			goto release;
	}
	return ret;
}

TextSurfaceAnim::TextSurfaceAnim(SurfacePool *pool, int32_t width, int32_t height,
				 ImageMode format, ImageOrder order, int32_t num_page,
				 int32_t num_level, bool reserved, bool alpha_mask)
	: TextSurface(pool, width, height, format, order, num_page, num_level, reserved,
		      alpha_mask)
{
	m_frame_delays = new float[num_page];
	m_frame_time = FRAME_TIME_NONE;
	if (m_frame_delays != NULL)
		sce_paf_memset(m_frame_delays, 0, num_page * sizeof(float));
}

TextSurfaceAnim::~TextSurfaceAnim()
{
	if (m_frame_delays != NULL)
		delete[] m_frame_delays;
}

int32_t TextSurfaceAnim::Prepare()
{
	int32_t ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;

	if (m_frame_delays != NULL) {
		float time = (float)(get_time_internal() - s_frame_clock.m_start) / 1000000.0f *
			     1000.0f;

		if (time >= FRAME_TIME_WRAP) {
			s_frame_clock.m_start = get_time_internal();
			time = 0.0f;
		}
		if (m_frame_time == FRAME_TIME_NONE) {
			m_frame_time = time;
		} else {
			int32_t frame = m_page_counter;

			if (time < m_frame_time)
				m_frame_time -= FRAME_TIME_WRAP;
			if (time - m_frame_time >= m_frame_delays[frame]) {
				if (frame < m_num_page - 1)
					m_page_counter = frame + 1;
				else
					m_page_counter = 0;
				m_frame_time = time;
			}
		}
		ret = SCE_PAF_OK;
	}
	return ret;
}

int32_t TextSurfaceAnim::SetFrameDelay(int32_t frame, float delay)
{
	if (frame < 0 || frame >= m_num_page)
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
	if (m_frame_delays == NULL)
		return SCE_PAF_ERROR_TEXT_NO_MEMORY;
	m_frame_delays[frame] = delay;
	return SCE_PAF_OK;
}

}
}
