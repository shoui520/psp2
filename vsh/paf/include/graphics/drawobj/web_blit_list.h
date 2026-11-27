/*
 * paf::graph::WebBlitList: toroidal-texture blit slice decomposition for 2014 PAF
 * WebObj and MapObj. Splits a rectangular update across wrap boundaries into 1, 2,
 * or 4 contiguous destination slices.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_WEB_BLIT_LIST_H
#define PAF_GRAPHICS_DRAWOBJ_WEB_BLIT_LIST_H

#include <stdint.h>
#include <vectormath.h>
#include <widget/web_rect.h>

namespace paf {
namespace graph {

class WebBlitSurface {
public:
	ui::WebRect m_rect;
	int32_t m_width;
	int32_t m_height;
	int32_t m_bytes_per_pixel;
};

struct WebBlitSlice {
	int32_t m_dst_x;
	int32_t m_dst_y;
	int32_t m_width;
	int32_t m_height;
	int32_t m_src_stride;
	const uint8_t *m_src_pixels;
};

class WebBlitList {
public:
	WebBlitList();
	~WebBlitList();

	int32_t Build(WebBlitSurface src, WebBlitSurface dst,
		      const sce::Vectormath::Simd::Aos::Matrix3 &pixel_matrix,
		      const void *src_pixels);
	WebBlitSlice *GetSlice(int32_t index);
	void Clear();

	int32_t m_count;
	WebBlitSlice *m_slices;
};

}
}

#endif
