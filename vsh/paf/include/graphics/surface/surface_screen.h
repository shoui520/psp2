/*
 * paf::graph::ScreenSurface, as far as converted C++ uses it: a surface GXM draws into, with a
 * colour surface per page, a render target and a depth surface. Fields no converted code
 * reads yet are named when paf::graph is converted (the file's name ours).
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_SCREEN_H
#define PAF_GRAPHICS_SURFACE_SURFACE_SCREEN_H

#include <stdint.h>
#include <gxm.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {

class ScreenSurface : public Surface {
public:
	/* a scene into the page drawn: ScePafGraphics_67FF6CF1 and ScePafGraphics_D5E30C64 */
	virtual int32_t BeginGxmScene(uint32_t flags);
	virtual int32_t EndGxmScene(SceGxmNotification *vertex_notification,
				    SceGxmNotification *fragment_notification);

	SceGxmColorSurface *GetRawColorSurface(int32_t page);

	SceGxmColorSurface *m_color_surfaces;
	SceGxmRenderTarget *m_render_target;
	SceGxmDepthStencilSurface *m_depth_stencil_surface;
	bool m_gxm_surfaces_made;
	/* the scene's copies of these: SceneData */
	int32_t m_extent_width;
	int32_t m_extent_height;
	int32_t m_reserved0;
	int32_t m_max_width;
	int32_t m_max_height;
	/* 2: the scene is drawn at half the surface's size, unless m_full_size */
	int32_t m_scale_mode;
	bool m_full_size;
	void *m_memory;
	uint32_t m_depth_offset;
	/* the part of the surface drawn, by any two corners */
	int32_t m_bounds_x0;
	int32_t m_bounds_y0;
	int32_t m_bounds_x1;
	int32_t m_bounds_y1;
};

}
}

#endif
