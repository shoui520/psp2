/*
 * paf::graph::MapObj: toroidal-mapped plane draw object for 2014 PAF MapView / JsDocument.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_MAP_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_MAP_OBJ_H

#include <stdint.h>
#include <vectormath.h>
#include <graphics/drawobj/web_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <widget/web_rect.h>

namespace paf {
namespace graph {

class MapObj : public WebObj {
public:
	static const char *DrawObjType();
	static const char m_drawobj_type[];

	MapObj(const DrawStyle &style);
	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~MapObj();
	virtual void SetSize(math::v3 size);

	virtual int32_t Initialize(const math::v2 *size, int32_t tex_width, int32_t tex_height,
				   const ui::WebRect *surface_rect, const ui::WebRect *view_rect);
	virtual int32_t Finalize();
	virtual int32_t SetViewRect(ui::WebRect rect, bool zooming);
	virtual int32_t SetSurfaceRect(ui::WebRect rect, bool skip_clear);
	virtual int32_t GetTextureSize(uint32_t *out_width, uint32_t *out_height);
	virtual int32_t ResizeSurface(int32_t width, int32_t height);

	virtual int32_t SetSurfaceOwner(void *owner);
	virtual int32_t ReleaseSurface();
	virtual int32_t SetPixelSize(int32_t width, int32_t height);
	virtual int32_t Blit(float pos_x, float pos_y, float width, float height,
			     int32_t src_width, int32_t src_height, int32_t bytes_per_pixel,
			     const void *src_pixels);
	virtual int32_t NotifyViewportRect(float pos_x, float pos_y, float width, float height);
	virtual int32_t OnRenderEnd();

protected:
	void UpdateMapTexRegion(const ui::WebRect *rect);
	int32_t UpdateMapPixelMatrix(const ui::WebRect *rect);
	int32_t UpdateMapUvMatrix(const ui::WebRect *rect);
	int32_t CreateOrResizeMapSurface(int32_t width, int32_t height);
	void BuildMapUncoveredRects(int32_t count, ui::WebRect old_rect, ui::WebRect new_rect);
	int32_t CalcMapUncoveredRects(ui::WebRect old_rect, ui::WebRect new_rect);
	void ClearMapUncoveredArea(ui::WebRect next_rect, ui::WebRect prev_rect, bool use_prev);

public:
	sce::Vectormath::Simd::Aos::Matrix3 m_map_uv_matrix;
	sce::Vectormath::Simd::Aos::Matrix3 m_map_pixel_matrix;
	intrusive_ptr<Surface> m_map_surface;
	ui::WebSurfaceOwner *m_map_owner;
	ui::WebRect m_map_surface_rect;
	ui::WebRect m_map_view_rect;
	int32_t m_map_tex_width;
	int32_t m_map_tex_height;
	ui::WebRect *m_map_uncovered_rects;
	ui::WebRect m_map_saved_view_rect;
	int32_t m_map_zoom_state;
};

}
}

#endif
