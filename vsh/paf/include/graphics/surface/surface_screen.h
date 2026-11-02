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
	/* what a screen surface is made of (names ours) */
	class Param {
	public:
		int32_t width;
		int32_t height;
		ImageMode format;
		ImageOrder order;
		int32_t num_page;
		/* the depth surface's, ImageMode_NONE for none */
		ImageMode depth_format;
		ImageOrder depth_order;
		int32_t scale_mode;
		bool full_size;
	};

	/* an empty surface, made later */
	ScreenSurface();
	ScreenSurface(const Param *param, void *bp, SurfaceType surf_type);
	virtual ~ScreenSurface();
	virtual void Swap();
	virtual void *Lock(int32_t page);
	virtual void Unlock();
	virtual int32_t Resize(int32_t width, int32_t height);
	virtual int32_t GenTexture();
	/* a scene into the page drawn */
	virtual int32_t Begin(uint32_t flags);
	virtual int32_t End(const SceGxmNotification *vertex_notification,
			    const SceGxmNotification *fragment_notification);

	/* the context the scenes go to: the one SetRawContext set, else the front end's */
	static SceGxmContext *GetRawContext();
	/* the GXM depth format of an image mode; -1 for none */
	static int32_t ConvertDepthFormat(ImageMode format);
	static int32_t GetDepthSize(int32_t width, int32_t height, ImageMode format,
				    ImageOrder order);

	SceGxmColorSurface *GetRawColorSurface(int32_t page);
	int32_t InitColorSurface();
	int32_t InitDepthSurface();
	SceGxmColorFormat GetColorSurfaceFormat();
	/* -1 for an order GXM cannot draw into */
	SceGxmColorSurfaceType GetColorSurfaceOrder();
	/* the surface's make, back in a Param (name ours) */
	void GetParam(Param *param);
	/* a render target the surface's size, made again (name ours) */
	int32_t CreateRenderTarget(SceGxmRenderTarget **target, int32_t reserved);
	/* a page's colour surface over memory someone else gives (name ours) */
	int32_t SetColorSurface(int32_t page, SceGxmColorFormat format,
				SceGxmColorSurfaceType type, uint32_t stride, void *data);

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
