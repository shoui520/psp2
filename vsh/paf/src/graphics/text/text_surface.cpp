/*
 * paf::graph::TextSurface (the file's name ours): a surface the text cache keeps a glyph in,
 * marked as a font's (an alpha mask, drawn in the text's color) or an image's.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_surface_cache.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

TextSurface::TextSurface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			 ImageOrder order, int32_t num_page, int32_t num_level, bool reserved,
			 bool alpha_mask)
	: Surface(pool, width, height, format, order, num_page, num_level, reserved),
	  m_alpha_mask(alpha_mask)
{
}

TextSurface::~TextSurface()
{
}

int32_t TextSurface::Prepare()
{
	return SCE_PAF_OK;
}

bool TextSurface::IsAlphaMask()
{
	return m_alpha_mask;
}

}
}
