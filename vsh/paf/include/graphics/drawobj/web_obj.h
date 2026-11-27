/*
 * paf::graph::WebObj: toroidal-mapped plane draw object for 2014 PAF WebRenderer.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_WEB_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_WEB_OBJ_H

#include <stdint.h>
#include <vectormath.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <widget/web_core.h>
#include <widget/web_rect.h>

namespace paf {
namespace graph {

class WebObj : public PlaneObj, public ui::WebTileObserver {
public:
	static const char *DrawObjType();
	static const char m_drawobj_type[];

	WebObj(const DrawStyle &style);
	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~WebObj();
	virtual void SetSize(math::v3 size);

	virtual int32_t Initialize(const math::v2 *size, int32_t tex_width, int32_t tex_height,
				   const ui::WebRect *surface_rect, const ui::WebRect *view_rect);
	virtual int32_t Finalize();
	virtual int32_t SetViewRect(ui::WebRect rect, bool zooming);
	virtual int32_t SetSurfaceRect(ui::WebRect rect, bool skip_clear);
	virtual int32_t GetTextureSize(uint32_t *out_width, uint32_t *out_height);
	virtual int32_t ResizeSurface(int32_t width, int32_t height);
	virtual void SetZoomState(int32_t state);
	virtual void SetZoomActive(bool active);
	virtual void SetBlitEnabled(bool enabled);
	virtual PlaneObj *GetZoomPlane();
	virtual ui::WebRect GetSurfaceRect();
	virtual ui::WebRect GetZoomOriginRect();

	virtual int32_t SetSurfaceOwner(void *owner);
	virtual int32_t ReleaseSurface();
	virtual int32_t SetPixelSize(int32_t width, int32_t height);
	virtual int32_t Blit(float pos_x, float pos_y, float width, float height,
			     int32_t src_width, int32_t src_height, int32_t bytes_per_pixel,
			     const void *src_pixels);
	virtual int32_t NotifyViewportRect(float pos_x, float pos_y, float width, float height);
	virtual int32_t OnRenderEnd();

	void ClearSurfaceWhite();
	void DetachSurface();
	int32_t EnsureSurface(int32_t *out_width, int32_t *out_height);

protected:
	void UpdateTexRegion(const ui::WebRect *rect);
	int32_t UpdatePixelMatrix(const ui::WebRect *rect);
	int32_t UpdateUvMatrix(const ui::WebRect *rect);
	int32_t CreateOrResizeSurface(int32_t width, int32_t height);
	void BuildUncoveredRects(int32_t count, ui::WebRect old_rect, ui::WebRect new_rect);
	int32_t CalcUncoveredRects(ui::WebRect old_rect, ui::WebRect new_rect);
	void ClearUncoveredArea(ui::WebRect next_rect, ui::WebRect prev_rect, bool use_prev);
	void RestoreOverlapAlpha(ui::WebRect blit_rect);

public:
	sce::Vectormath::Simd::Aos::Matrix3 m_uv_matrix;
	sce::Vectormath::Simd::Aos::Matrix3 m_pixel_matrix;
	intrusive_ptr<Surface> m_surface;
	ui::WebSurfaceOwner *m_owner;
	ui::WebRect m_surface_rect;
	ui::WebRect m_view_rect;
	int32_t m_tex_width;
	int32_t m_tex_height;
	ui::WebRect *m_uncovered_rects;
	int32_t m_zoom_state;
	bool m_accept_blit;
	bool m_blit_enabled;
	ui::WebRect m_valid_rect;
	PlaneObj *m_zoom_plane;
	ui::WebRect m_zoom_origin_rect;
	ui::WebRect m_pending_surface_rect;
};

}
}

#endif
