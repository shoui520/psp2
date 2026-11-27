/*
 * paf::graph::MapObj: toroidal-mapped plane draw object for 2014 PAF MapView / JsDocument.
 */
#include <stdint.h>
#include <std/math.h>
#include <std/string.h>
#include <vectormath.h>
#include <graphics/gutil.h>
#include <graphics/drawobj/map_obj.h>
#include <graphics/drawobj/web_blit_list.h>
#include <image/image_types.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

const char MapObj::m_drawobj_type[] = "MapObj";

const char *MapObj::DrawObjType()
{
	return m_drawobj_type;
}

bool MapObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return WebObj::IsInherit(type);
}

const char *MapObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

MapObj::MapObj(const DrawStyle &style)
	: WebObj(style), m_map_uv_matrix(Matrix3::identity()),
	  m_map_pixel_matrix(Matrix3::identity()), m_map_surface(), m_map_owner(NULL),
	  m_map_surface_rect(), m_map_view_rect(), m_map_tex_width(0), m_map_tex_height(0),
	  m_map_uncovered_rects(NULL), m_map_saved_view_rect(), m_map_zoom_state(0)
{
	m_map_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_map_saved_view_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_map_view_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	DrawObj::SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

MapObj::~MapObj()
{
	Finalize();
}

int32_t MapObj::Finalize()
{
	m_map_surface.clear();
	if (m_map_uncovered_rects != NULL) {
		delete[] m_map_uncovered_rects;
		m_map_uncovered_rects = NULL;
	}
	if (m_map_owner != NULL) {
		m_map_owner->OnSurfaceReleased();
		m_map_owner = NULL;
	}
	return 0;
}

void MapObj::UpdateMapTexRegion(const ui::WebRect *rect)
{
	float rect_width = rect->Width();

	if (rect_width <= 0.0f)
		return;

	float rect_height = rect->Height();

	if (rect_height <= 0.0f)
		return;

	Vector3 top_left = m_map_uv_matrix * Vector3(rect->m_left, rect->m_top, 1.0f);
	Vector3 bottom_right = m_map_uv_matrix * Vector3(rect->m_right, rect->m_bottom, 1.0f);

	float scale_u = 1.0f;
	float scale_v = 1.0f;
	float surf_width = m_map_surface_rect.Width();
	float surf_height = m_map_surface_rect.Height();

	if (surf_width > 0.0f)
		scale_u = rect_width / surf_width;
	if (surf_height > 0.0f)
		scale_v = rect_height / surf_height;

	SetTexRegion(math::v4(top_left.getX(), bottom_right.getY(), scale_u, scale_v));
}

int32_t MapObj::UpdateMapPixelMatrix(const ui::WebRect *rect)
{
	if (m_map_surface.get() != NULL && m_map_surface->GetWidth() > 0 &&
	    m_map_surface->GetHeight() > 0 && rect->Width() > 0.0f && rect->Height() > 0.0f) {
		float scale_x = (float)m_map_surface->GetWidth() / rect->Width();
		float scale_y = (float)m_map_surface->GetHeight() / rect->Height();

		m_map_pixel_matrix = Matrix3(Vector3(scale_x, 0.0f, 0.0f),
					     Vector3(0.0f, scale_y, 0.0f),
					     Vector3(0.0f, 0.0f, 1.0f));
	} else {
		m_map_pixel_matrix = Matrix3::identity();
	}
	return 0;
}

int32_t MapObj::UpdateMapUvMatrix(const ui::WebRect *rect)
{
	float rect_width = rect->Width();
	float rect_height = rect->Height();

	if (rect_width <= 0.0f || rect_height <= 0.0f) {
		m_map_uv_matrix = Matrix3::identity();
	} else {
		m_map_uv_matrix = Matrix3(Vector3(1.0f / rect_width, 0.0f, 0.0f),
					  Vector3(0.0f, 1.0f / rect_height, 0.0f),
					  Vector3(0.0f, 1.0f, 1.0f));
	}
	return 0;
}

int32_t MapObj::CreateOrResizeMapSurface(int32_t width, int32_t height)
{
	int32_t ret = (int32_t)0x80AF008E;

	if (m_map_surface.get() == NULL) {
		SurfacePool *pool = gutil::GetDefaultSurfacePool();

		m_map_surface = intrusive_ptr<Surface>(new Surface(pool, width, height,
								   ImageMode_BGRA8888,
								   ImageOrder_Linear, 1, 1, false));
	} else {
		ret = m_map_surface->Resize(width, height);
		if (ret == 0) {
			void *pixels = m_map_surface->Lock(0);

			sce_paf_memset(pixels, 0xFF, m_map_surface->GetPageSize());
			m_map_surface->Unlock();
		}
	}

	if (m_map_surface.get() != NULL) {
		m_map_tex_width = width;
		m_map_tex_height = height;
		SetTexture(m_map_surface, 0);
		SetWrapS(REPEAT, 0);
		SetWrapT(REPEAT, 0);
		ret = UpdateMapPixelMatrix(&m_map_surface_rect);
		if (ret == 0) {
			ui::WebRect full_rect;

			full_rect.Set(0.0f, 0.0f, (float)(uint32_t)width, (float)(uint32_t)height);
			UpdateMapTexRegion(&full_rect);
		}
	} else {
		ReleaseAll();
	}
	return ret;
}

int32_t MapObj::Initialize(const math::v2 *size, int32_t tex_width, int32_t tex_height,
			   const ui::WebRect *surface_rect, const ui::WebRect *view_rect)
{
	SetPos(math::v3(0.0f, 0.0f, 0.0f));
	SetSize(math::v3(size->extract_x(), size->extract_y(), 1.0f));

	int32_t ret = UpdateMapUvMatrix(surface_rect);

	if (ret == 0) {
		ret = UpdateMapPixelMatrix(surface_rect);
		if (ret == 0) {
			m_map_surface_rect = *surface_rect;
			UpdateMapTexRegion(view_rect);
			CreateOrResizeMapSurface(tex_width, tex_height);
		}
	}
	return ret;
}

int32_t MapObj::SetViewRect(ui::WebRect rect, bool zooming)
{
	if (zooming) {
		UpdateMapTexRegion(&rect);
		m_map_zoom_state = 1;
	} else if (m_map_zoom_state == 1) {
		m_map_zoom_state = 2;
	} else if (m_map_zoom_state != 2) {
		UpdateMapTexRegion(&rect);
	}
	m_map_view_rect = rect;
	m_map_saved_view_rect = rect;
	return 0;
}

void MapObj::BuildMapUncoveredRects(int32_t count, ui::WebRect old_rect, ui::WebRect new_rect)
{
	ui::WebRect overlap = old_rect;

	overlap.Intersect(new_rect);
	if (count > 0) {
		if (m_map_uncovered_rects != NULL)
			delete[] m_map_uncovered_rects;
		m_map_uncovered_rects = new ui::WebRect[count];
	}

	if (count == 1) {
		ui::WebRect band;

		if (overlap.m_left == new_rect.m_left && overlap.m_right == new_rect.m_right) {
			if (overlap.m_bottom == new_rect.m_bottom)
				band.Set(overlap.m_left, overlap.m_top, overlap.m_right,
					 new_rect.m_top);
			else
				band.Set(overlap.m_left, new_rect.m_bottom, overlap.m_right,
					 overlap.m_bottom);
		} else if (overlap.m_left == new_rect.m_left) {
			band.Set(overlap.m_right, overlap.m_bottom, new_rect.m_right,
				 overlap.m_top);
		} else {
			band.Set(new_rect.m_left, overlap.m_bottom, overlap.m_left, overlap.m_top);
		}
		m_map_uncovered_rects[0] = band;
	}

	if (count == 2) {
		ui::WebRect side_band;
		ui::WebRect end_band;

		if (overlap.m_bottom == old_rect.m_bottom) {
			if (overlap.m_right == old_rect.m_right) {
				side_band.Set(overlap.m_right, overlap.m_bottom, new_rect.m_right,
					      overlap.m_top);
				end_band.Set(new_rect.m_left, new_rect.m_bottom, new_rect.m_right,
					     old_rect.m_bottom);
			} else {
				side_band.Set(new_rect.m_left, overlap.m_bottom, overlap.m_left,
					      overlap.m_top);
				end_band.Set(new_rect.m_left, new_rect.m_bottom, new_rect.m_right,
					     old_rect.m_bottom);
			}
		} else {
			if (overlap.m_right == old_rect.m_right) {
				side_band.Set(overlap.m_right, overlap.m_bottom, new_rect.m_right,
					      overlap.m_top);
				end_band.Set(new_rect.m_left, old_rect.m_top, new_rect.m_right,
					     new_rect.m_top);
			} else {
				side_band.Set(new_rect.m_left, overlap.m_bottom, overlap.m_left,
					      overlap.m_top);
				end_band.Set(new_rect.m_left, old_rect.m_top, new_rect.m_right,
					     new_rect.m_top);
			}
		}
		m_map_uncovered_rects[0] = side_band;
		m_map_uncovered_rects[1] = end_band;
	}

	if (count == 4) {
		ui::WebRect left_band;
		ui::WebRect bottom_band;
		ui::WebRect right_band;
		ui::WebRect top_band;

		left_band.Set(old_rect.m_left, overlap.m_bottom, overlap.m_left, overlap.m_top);
		bottom_band.Set(old_rect.m_left, old_rect.m_bottom, old_rect.m_right,
				overlap.m_bottom);
		right_band.Set(overlap.m_right, overlap.m_bottom, old_rect.m_right, overlap.m_top);
		top_band.Set(old_rect.m_left, overlap.m_top, old_rect.m_right, old_rect.m_top);
		m_map_uncovered_rects[0] = left_band;
		m_map_uncovered_rects[1] = bottom_band;
		m_map_uncovered_rects[2] = right_band;
		m_map_uncovered_rects[3] = top_band;
	}
}

int32_t MapObj::CalcMapUncoveredRects(ui::WebRect old_rect, ui::WebRect new_rect)
{
	ui::WebRect overlap = old_rect;

	overlap.Intersect(new_rect);

	float old_w = sce_paf_floorf(old_rect.Width());
	float new_w = sce_paf_floorf(new_rect.Width());
	float new_h = sce_paf_floorf(new_rect.Height());
	float overlap_w = sce_paf_floorf(overlap.Width());
	float overlap_h = sce_paf_floorf(overlap.Height());
	int32_t count;

	if (old_w < new_w) {
		count = 0;
	} else if (old_w > new_w) {
		count = 4;
	} else if (overlap_w == new_w || overlap_h == new_h) {
		count = 1;
	} else {
		count = 2;
	}

	BuildMapUncoveredRects(count, old_rect, new_rect);
	return count;
}

void MapObj::ClearMapUncoveredArea(ui::WebRect next_rect, ui::WebRect prev_rect, bool use_prev)
{
	ui::WebRect shifted_next;
	ui::WebRect shifted_prev;

	shifted_next = next_rect;
	shifted_prev = prev_rect;

	if (shifted_next.m_left < 0.0f || shifted_prev.m_left < 0.0f) {
		shifted_next.m_left += 960.0f;
		shifted_next.m_right += 960.0f;
		shifted_prev.m_left += 960.0f;
		shifted_prev.m_right += 960.0f;
	}
	if (shifted_next.m_bottom < 0.0f || shifted_prev.m_bottom < 0.0f) {
		shifted_next.m_bottom += 540.0f;
		shifted_next.m_top += 540.0f;
		shifted_prev.m_bottom += 540.0f;
		shifted_prev.m_top += 540.0f;
	}

	if (m_map_surface.get() == NULL)
		return;
	if (shifted_next.m_left == shifted_prev.m_left &&
	    shifted_next.m_right == shifted_prev.m_right &&
	    shifted_next.m_top == shifted_prev.m_top &&
	    shifted_next.m_bottom == shifted_prev.m_bottom) {
		return;
	}

	ui::WebRect base_rect = use_prev ? shifted_prev : shifted_next;
	WebBlitSurface dst_surf;
	int32_t stride = m_map_surface->GetStride();

	dst_surf.m_rect = base_rect;
	dst_surf.m_width = stride / 4;
	dst_surf.m_height = m_map_surface->GetHeight();
	dst_surf.m_bytes_per_pixel = stride / dst_surf.m_width;

	int32_t rect_count = CalcMapUncoveredRects(shifted_prev, shifted_next);

	for (int32_t rect_idx = 0; rect_idx < rect_count; rect_idx++) {
		ui::WebRect band = m_map_uncovered_rects[rect_idx];
		WebBlitSurface src_surf;

		src_surf.m_rect = band;
		src_surf.m_width = (int32_t)band.Width();
		src_surf.m_height = (int32_t)band.Height();
		src_surf.m_bytes_per_pixel = 4;

		WebBlitList blit_list;

		if (blit_list.Build(src_surf, dst_surf, m_map_pixel_matrix, NULL) == 0) {
			if (blit_list.m_count > 0) {
				uint8_t *base_ptr = static_cast<uint8_t *>(m_map_surface->Lock(0));

				for (int32_t slice_idx = 0; slice_idx < blit_list.m_count;
				     slice_idx++) {
					WebBlitSlice *slice = blit_list.GetSlice(slice_idx);

					if (slice == NULL)
						continue;

					if (slice->m_width > m_map_tex_width)
						slice->m_width = m_map_tex_width;
					if (slice->m_height > m_map_tex_height)
						slice->m_height = m_map_tex_height;

					int32_t clear_bytes =
						slice->m_width * dst_surf.m_bytes_per_pixel;
					uint8_t *dst_ptr =
						base_ptr +
						(slice->m_dst_x +
						 slice->m_dst_y * dst_surf.m_width) *
							dst_surf.m_bytes_per_pixel;

					for (int32_t row = 0; row < slice->m_height; row++) {
						sce_paf_memset(dst_ptr, 0, clear_bytes);
						dst_ptr +=
							dst_surf.m_width *
							dst_surf.m_bytes_per_pixel;
					}
				}
				m_map_surface->Unlock();
			}
			blit_list.Clear();
		}
	}
}

int32_t MapObj::SetSurfaceRect(ui::WebRect rect, bool skip_clear)
{
	int32_t ret = 0;

	if (m_map_surface.get() == NULL ||
	    (rect.m_left == m_map_surface_rect.m_left &&
	     rect.m_right == m_map_surface_rect.m_right &&
	     rect.m_top == m_map_surface_rect.m_top &&
	     rect.m_bottom == m_map_surface_rect.m_bottom) ||
	    skip_clear) {
		ClearMapUncoveredArea(m_map_surface_rect, rect, true);
	} else {
		ClearMapUncoveredArea(rect, m_map_surface_rect, false);
		ret = UpdateMapUvMatrix(&rect);
		if (ret == 0) {
			ret = UpdateMapPixelMatrix(&rect);
			m_map_surface_rect = rect;
		}
	}
	return ret;
}

int32_t MapObj::GetTextureSize(uint32_t *out_width, uint32_t *out_height)
{
	*out_width = (uint32_t)m_map_tex_width;
	*out_height = (uint32_t)m_map_tex_height;
	return m_map_tex_height;
}

int32_t MapObj::ResizeSurface(int32_t width, int32_t height)
{
	return CreateOrResizeMapSurface(width, height);
}

void MapObj::SetSize(math::v3 size)
{
	if (size.extract_x() * size.extract_y() == 0.0f)
		return;

	PlaneObj::SetSize(size);
	if (m_map_surface.get() == NULL && m_map_tex_width != 0 && m_map_tex_height != 0)
		CreateOrResizeMapSurface(m_map_tex_width, m_map_tex_height);
}

int32_t MapObj::SetSurfaceOwner(void *owner)
{
	m_map_owner = static_cast<ui::WebSurfaceOwner *>(owner);
	return 0;
}

int32_t MapObj::ReleaseSurface()
{
	Finalize();
	return 0;
}

int32_t MapObj::SetPixelSize(int32_t width, int32_t height)
{
	SetSize(math::v3((float)width, (float)height, 0.0f));
	return 0;
}

int32_t MapObj::Blit(float pos_x, float pos_y, float width, float height, int32_t src_width,
		     int32_t src_height, int32_t bytes_per_pixel, const void *src_pixels)
{
	ui::WebRect screen_rect;

	screen_rect.Set(0.0f, 0.0f, 960.0f, 540.0f);
	SetViewRect(screen_rect, false);
	SetSurfaceRect(screen_rect, false);

	int32_t ret = (int32_t)0x80AF0001;

	if (m_map_surface->GetWidth() > 0 && m_map_surface->GetHeight() > 0) {
		WebBlitSurface src_surf;

		src_surf.m_rect.Set(pos_x, pos_y, pos_x + width, pos_y + height);
		src_surf.m_width = src_width;
		src_surf.m_height = src_height;
		src_surf.m_bytes_per_pixel = bytes_per_pixel;

		WebBlitSurface dst_surf;
		int32_t stride = m_map_surface->GetStride();

		dst_surf.m_rect = m_map_surface_rect;
		dst_surf.m_width = stride / bytes_per_pixel;
		dst_surf.m_height = m_map_surface->GetHeight();
		dst_surf.m_bytes_per_pixel = stride / dst_surf.m_width;

		WebBlitList blit_list;

		if (blit_list.Build(src_surf, dst_surf, m_map_pixel_matrix, src_pixels) == 0) {
			if (blit_list.m_count > 0) {
				uint8_t *base_ptr = static_cast<uint8_t *>(m_map_surface->Lock(0));

				for (int32_t slice_idx = 0; slice_idx < blit_list.m_count;
				     slice_idx++) {
					WebBlitSlice *slice = blit_list.GetSlice(slice_idx);

					if (slice == NULL)
						continue;

					int32_t copy_bytes =
						slice->m_width * dst_surf.m_bytes_per_pixel;
					uint8_t *dst_ptr =
						base_ptr +
						(slice->m_dst_x +
						 slice->m_dst_y * dst_surf.m_width) *
							dst_surf.m_bytes_per_pixel;

					for (int32_t row = 0; row < slice->m_height; row++) {
						sce_paf_memcpy(dst_ptr, slice->m_src_pixels,
							       copy_bytes);
						dst_ptr +=
							dst_surf.m_width *
							dst_surf.m_bytes_per_pixel;
						slice->m_src_pixels += slice->m_src_stride;
					}
				}
				m_map_surface->Unlock();
			}
			blit_list.Clear();
		}
		ret = 0;
	}

	if (m_map_zoom_state != 0) {
		UpdateMapTexRegion(&m_map_view_rect);
		m_map_zoom_state = 0;
	}
	return ret;
}

int32_t MapObj::NotifyViewportRect(float pos_x, float pos_y, float width, float height)
{
	return 0;
}

int32_t MapObj::OnRenderEnd()
{
	return 0;
}

}
}
