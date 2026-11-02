/*
 * paf::graph::ScreenSurface: a surface GXM draws into, with a colour surface per page, a render
 * target and a depth surface. Its scenes go to the context SetRawContext sets, else to the front
 * end's (the file's name ours).
 */
#ifndef PAF_GRAPHICS_SURFACE_SURFACE_SCREEN_H
#define PAF_GRAPHICS_SURFACE_SURFACE_SCREEN_H

#include <stdint.h>
#include <gxm.h>
#include <kernel/types.h>
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
	ScreenSurface(int32_t width, int32_t height, ImageMode format, ImageOrder order,
		      int32_t num_page, ImageMode depth_format, ImageOrder depth_order, void *bp,
		      SurfaceType surf_type);
	ScreenSurface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		      ImageOrder order, int32_t num_page, ImageMode depth_format,
		      ImageOrder depth_order, SurfaceType surf_type);
	/* over a render target someone else made and keeps */
	ScreenSurface(SceGxmRenderTarget *render_target, SurfacePool *pool, int32_t width,
		      int32_t height, ImageMode format, ImageOrder order, int32_t num_page,
		      ImageMode depth_format, ImageOrder depth_order);
	virtual ~ScreenSurface();
	virtual void Swap();
	virtual void *Lock(int32_t page);
	virtual void Unlock();
	/* not beyond the size of the memory given; NOT_PERMITTED over a render target someone
	   else keeps */
	virtual int32_t Resize(int32_t width, int32_t height);
	virtual int32_t GenTexture();
	/* a scene into the page drawn */
	virtual int32_t Begin(uint32_t flags);
	virtual int32_t End(const SceGxmNotification *vertex_notification,
			    const SceGxmNotification *fragment_notification);

	/* the context the scenes go to: the one SetRawContext set, else the front end's */
	static SceGxmContext *GetRawContext();
	static void SetRawContext(SceGxmContext *context);
	/* the GXM depth format of an image mode; -1 for none */
	static int32_t ConvertDepthFormat(ImageMode format);
	static int32_t GetDepthSize(int32_t width, int32_t height, ImageMode format,
				    ImageOrder order);

	/* a scene with a render target, into a colour surface */
	int32_t Begin_(SceGxmRenderTarget *render_target, SceGxmSyncObject *sync_object,
		       SceGxmColorSurface *color_surface, SceGxmDepthStencilSurface *depth_stencil,
		       uint32_t flags);
	/* the part of the surface drawn and GXM's region clip, from x0, y0 to x1, y1 (not
	   included), the scene begun again or the clip set when update says */
	void SetScissor(float x0, float y0, float x1, float y1, bool update);
	void SetClip(float x0, float y0, float x1, float y1, bool update);
	int32_t DestroyRenderTarget(SceGxmRenderTarget *render_target);
	SceGxmColorSurface *GetRawColorSurface(int32_t page);
	int32_t InitColorSurface();
	int32_t InitDepthSurface();
	int32_t GetDepthFormat();
	/* -1 for a mode or an order GXM cannot draw into */
	SceGxmColorFormat ConvertColorSurfaceFormat(ImageMode format);
	SceGxmColorFormat GetColorSurfaceFormat();
	SceGxmColorSurfaceType ConvertColorSurfaceOrder(ImageOrder order);
	SceGxmColorSurfaceType GetColorSurfaceOrder();
	/* the surface's make, back in a Param (name ours) */
	void GetParam(Param *param);
	/* the render target the surface's size, made again in a memory block of the type (0: the
	   default) (name ours) */
	int32_t CreateRenderTarget(SceGxmRenderTarget **render_target,
				   SceKernelMemBlockType mem_type);
	/* a page's colour surface over memory someone else gives (name ours) */
	int32_t SetColorSurface(int32_t page, SceGxmColorFormat format,
				SceGxmColorSurfaceType type, uint32_t stride, void *data);

	SceGxmColorSurface *m_color_surfaces;
	SceGxmRenderTarget *m_render_target;
	SceGxmDepthStencilSurface *m_depth_stencil_surface;
	/* the render target is the surface's own, destroyed with it */
	bool m_own_render_target;
	/* the YUV profile each scene sets (and the scene's copies: SceneData) */
	uint32_t m_yuv_csc_index;
	SceGxmYuvProfile m_yuv_profile;
	SceGxmOutputRegisterSize m_output_register_size;
	/* the size the memory given holds, 0 for no limit */
	uint32_t m_max_width;
	uint32_t m_max_height;
	/* 2: the scene is drawn at half the surface's size, unless m_full_size */
	int32_t m_scale_mode;
	bool m_full_size;
	void *m_memory;
	uint32_t m_depth_offset;
	/* SetScissor's: the part of the surface drawn, by any two corners */
	int32_t m_scissor_x0;
	int32_t m_scissor_y0;
	int32_t m_scissor_x1;
	int32_t m_scissor_y1;
	/* SetClip's */
	int32_t m_clip_x0;
	int32_t m_clip_y0;
	int32_t m_clip_x1;
	int32_t m_clip_y1;

private:
	/* the surface's members after its Surface's, set up (name ours) */
	int32_t Initialize(int32_t width, int32_t height, ImageMode format, ImageOrder order,
			   int32_t num_page, ImageMode depth_format, ImageOrder depth_order,
			   void *bp);
};

}
}

#endif
