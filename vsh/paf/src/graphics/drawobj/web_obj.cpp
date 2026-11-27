/*
 * paf::graph::WebObj: toroidal-mapped plane draw object for 2014 PAF WebRenderer.
 */
#include <stdint.h>
#include <std/math.h>
#include <std/string.h>
#include <vectormath.h>
#include <graphics/gutil.h>
#include <graphics/drawobj/web_blit_list.h>
#include <graphics/drawobj/web_obj.h>
#include <image/image_types.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

const char WebObj::m_drawobj_type[] = "WebObj";

const char *WebObj::DrawObjType()
{
	return m_drawobj_type;
}

bool WebObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return PlaneObj::IsInherit(type);
}

const char *WebObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

WebObj::WebObj(const DrawStyle &style)
	: PlaneObj(style), m_uv_matrix(Matrix3::identity()), m_pixel_matrix(Matrix3::identity()),
	  m_surface(), m_owner(NULL), m_surface_rect(), m_view_rect(), m_tex_width(0),
	  m_tex_height(0), m_uncovered_rects(NULL), m_zoom_state(0), m_accept_blit(true),
	  m_blit_enabled(true), m_valid_rect(), m_zoom_plane(NULL), m_zoom_origin_rect(),
	  m_pending_surface_rect()
{
	m_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_valid_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_zoom_origin_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_view_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	m_pending_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
	DrawObj::SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

WebObj::~WebObj()
{
	Finalize();
}

int32_t WebObj::Finalize()
{
	m_surface.clear();
	if (m_uncovered_rects != NULL) {
		delete[] m_uncovered_rects;
		m_uncovered_rects = NULL;
	}
	if (m_owner != NULL) {
		m_owner->OnSurfaceReleased();
		m_owner = NULL;
	}
	return 0;
}

void WebObj::UpdateTexRegion(const ui::WebRect *rect)
{
	float rect_width = rect->Width();

	if (rect_width <= 0.0f)
		return;

	float rect_height = rect->Height();

	if (rect_height <= 0.0f)
		return;

	Vector3 top_left = m_uv_matrix * Vector3(rect->m_left, rect->m_top, 1.0f);
	Vector3 bottom_right = m_uv_matrix * Vector3(rect->m_right, rect->m_bottom, 1.0f);

	float scale_u = 1.0f;
	float scale_v = 1.0f;
	float surf_width = m_surface_rect.Width();
	float surf_height = m_surface_rect.Height();

	if (surf_width > 0.0f)
		scale_u = rect_width / surf_width;
	if (surf_height > 0.0f)
		scale_v = rect_height / surf_height;

	float tex_u = top_left.getX();
	float tex_v = bottom_right.getY();

	tex_u -= sce_paf_floorf(tex_u);
	tex_v -= sce_paf_floorf(tex_v);
	SetTexRegion(math::v4(tex_u, tex_v, scale_u, scale_v));

	if (m_zoom_plane != NULL) {
		math::v4 zoom_region(0.0f, 0.0f, 1.0f, 1.0f);

		if (m_zoom_plane->m_tex_region != NULL)
			zoom_region = *m_zoom_plane->m_tex_region;

		if (zoom_region == math::v4(0.0f, 0.0f, 0.0f, 0.0f) && m_zoom_state == 1) {
			Matrix3 zoom_uv(Vector3(1.0f / m_surface_rect.Width(), 0.0f, 0.0f),
					Vector3(0.0f, 1.0f / m_surface_rect.Height(), 0.0f),
					Vector3(0.0f, 1.0f, 1.0f));
			Vector3 zoom_tl =
				zoom_uv * Vector3(m_surface_rect.m_left, m_surface_rect.m_top, 1.0f);
			Vector3 zoom_br =
				zoom_uv * Vector3(m_surface_rect.m_right, m_surface_rect.m_bottom, 1.0f);
			float zoom_u = zoom_tl.getX();
			float zoom_v = zoom_br.getY();

			zoom_u -= sce_paf_floorf(zoom_u);
			zoom_v -= sce_paf_floorf(zoom_v);
			m_zoom_plane->SetTexRegion(math::v4(zoom_u, zoom_v, 1.0f, 1.0f));
		}
	}
}

int32_t WebObj::UpdatePixelMatrix(const ui::WebRect *rect)
{
	if (m_surface.get() != NULL && m_surface->GetWidth() > 0 && m_surface->GetHeight() > 0 &&
	    rect->Width() > 0.0f && rect->Height() > 0.0f) {
		float scale_x = (float)m_surface->GetWidth() / rect->Width();
		float scale_y = (float)m_surface->GetHeight() / rect->Height();

		m_pixel_matrix = Matrix3(Vector3(scale_x, 0.0f, 0.0f),
					 Vector3(0.0f, scale_y, 0.0f),
					 Vector3(0.0f, 0.0f, 1.0f));
	} else {
		m_pixel_matrix = Matrix3::identity();
	}
	return 0;
}

int32_t WebObj::UpdateUvMatrix(const ui::WebRect *rect)
{
	float rect_width = rect->Width();
	float rect_height = rect->Height();

	if (rect_width <= 0.0f || rect_height <= 0.0f) {
		m_uv_matrix = Matrix3::identity();
	} else {
		m_uv_matrix = Matrix3(Vector3(1.0f / rect_width, 0.0f, 0.0f),
				      Vector3(0.0f, 1.0f / rect_height, 0.0f),
				      Vector3(0.0f, 1.0f, 1.0f));
	}
	return 0;
}

void WebObj::ClearSurfaceWhite()
{
	if (m_surface.get() != NULL) {
		void *pixels = m_surface->Lock(0);

		sce_paf_memset(pixels, 0xFF, m_surface->GetPageSize());
		m_surface->Unlock();
	}
}

int32_t WebObj::CreateOrResizeSurface(int32_t width, int32_t height)
{
	int32_t ret = (int32_t)0x80AF008E;

	if (m_surface.get() == NULL) {
		ui::WebCore *core = ui::WebCore::GetInstance();
		SurfacePool *pool = NULL;

		if (core != NULL)
			pool = core->GetSurfacePool();
		if (pool == NULL)
			pool = gutil::GetDefaultSurfacePool();

		m_surface = intrusive_ptr<Surface>(new Surface(pool, width, height,
							       ImageMode_BGRA8888,
							       ImageOrder_Linear, 1, 1, false));
		if (m_surface->GetMemory() != NULL && m_surface.get() != NULL)
			ClearSurfaceWhite();
	} else if (m_surface->GetWidth() != width || m_surface->GetHeight() != height) {
		ret = m_surface->Resize(width, height);
	}

	if (m_surface->GetMemory() != NULL) {
		m_tex_width = width;
		m_tex_height = height;
		SetTexture(m_surface, 0);
		SetWrapS(REPEAT, 0);
		SetWrapT(REPEAT, 0);
		if (m_zoom_plane != NULL) {
			m_zoom_plane->ReleaseAll();
			m_zoom_plane->SetTexture(m_surface, 0);
			m_zoom_plane->SetVisible(false);
			m_zoom_plane->SetWrapS(REPEAT, 0);
			m_zoom_plane->SetWrapT(REPEAT, 0);
			m_zoom_plane->SetTexRegion(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		ret = UpdatePixelMatrix(&m_surface_rect);
		if (ret == 0)
			UpdateTexRegion(&m_view_rect);
	} else {
		m_surface.clear();
		ReleaseAll();
	}
	return ret;
}

int32_t WebObj::Initialize(const math::v2 *size, int32_t tex_width, int32_t tex_height,
			   const ui::WebRect *surface_rect, const ui::WebRect *view_rect)
{
	SetPos(math::v3(0.0f, 0.0f, 0.0f));
	SetSize(math::v3(size->extract_x(), size->extract_y(), 1.0f));
	if (m_zoom_plane != NULL)
		m_zoom_plane->SetSize(math::v3(size->extract_x(), size->extract_y(), 1.0f));

	int32_t ret = UpdateUvMatrix(surface_rect);

	if (ret == 0) {
		ret = UpdatePixelMatrix(surface_rect);
		if (ret == 0) {
			m_surface_rect = *surface_rect;
			UpdateTexRegion(view_rect);
			CreateOrResizeSurface(tex_width, tex_height);
		}
	}
	return ret;
}

int32_t WebObj::SetViewRect(ui::WebRect rect, bool zooming)
{
	if (zooming) {
		if (m_zoom_state == 0)
			m_zoom_state = 1;
		UpdateTexRegion(&rect);
		if (m_zoom_plane != NULL) {
			if (m_zoom_origin_rect.Width() == 0.0f ||
			    m_zoom_origin_rect.Height() == 0.0f) {
				m_zoom_origin_rect = rect;
			}
			if ((m_zoom_origin_rect.Width() / rect.Width()) < 1.0f ||
			    m_zoom_state == 2) {
				if (!m_zoom_plane->m_flags.m_visible)
					m_zoom_plane->SetSize(m_size);
				m_zoom_plane->SetVisible(true);
				SetVisible(false);
			} else if (m_zoom_state != 3) {
				SetVisible(true);
				m_zoom_plane->SetVisible(false);
			}
		}
	} else if (m_zoom_state == 1) {
		m_zoom_state = 2;
	} else if (m_zoom_state != 2) {
		UpdateTexRegion(&rect);
	}
	m_view_rect = rect;
	return 0;
}

void WebObj::BuildUncoveredRects(int32_t count, ui::WebRect old_rect, ui::WebRect new_rect)
{
	ui::WebRect overlap = old_rect;

	overlap.Intersect(new_rect);
	if (count > 0) {
		if (m_uncovered_rects != NULL)
			delete[] m_uncovered_rects;
		m_uncovered_rects = new ui::WebRect[count];
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
		m_uncovered_rects[0] = band;
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
		m_uncovered_rects[0] = side_band;
		m_uncovered_rects[1] = end_band;
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
		m_uncovered_rects[0] = left_band;
		m_uncovered_rects[1] = bottom_band;
		m_uncovered_rects[2] = right_band;
		m_uncovered_rects[3] = top_band;
	}
}

int32_t WebObj::CalcUncoveredRects(ui::WebRect old_rect, ui::WebRect new_rect)
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

	BuildUncoveredRects(count, old_rect, new_rect);
	return count;
}

void WebObj::ClearUncoveredArea(ui::WebRect next_rect, ui::WebRect prev_rect, bool use_prev)
{
	if (!m_flags.m_visible && m_zoom_plane->m_flags.m_visible)
		return;
	if (m_surface.get() == NULL)
		return;

	ui::WebRect base_rect = use_prev ? prev_rect : next_rect;
	WebBlitSurface dst_surf;
	int32_t stride = m_surface->GetStride();

	dst_surf.m_rect = base_rect;
	dst_surf.m_width = stride / 4;
	dst_surf.m_height = m_surface->GetHeight();
	dst_surf.m_bytes_per_pixel = stride / dst_surf.m_width;

	int32_t rect_count = CalcUncoveredRects(prev_rect, next_rect);

	for (int32_t rect_idx = 0; rect_idx < rect_count; rect_idx++) {
		ui::WebRect band = m_uncovered_rects[rect_idx];
		WebBlitSurface src_surf;

		src_surf.m_rect = band;
		src_surf.m_width = (int32_t)band.Width();
		src_surf.m_height = (int32_t)band.Height();
		src_surf.m_bytes_per_pixel = 4;

		WebBlitList blit_list;

		if (blit_list.Build(src_surf, dst_surf, m_pixel_matrix, NULL) == 0) {
			if (blit_list.m_count > 0) {
				uint8_t *base_ptr = static_cast<uint8_t *>(m_surface->Lock(0));
				uint8_t *end_ptr = base_ptr + m_surface->GetPageSize();

				for (int32_t slice_idx = 0; slice_idx < blit_list.m_count;
				     slice_idx++) {
					WebBlitSlice *slice = blit_list.GetSlice(slice_idx);

					if (slice == NULL)
						continue;

					if (slice->m_width > m_tex_width)
						slice->m_width = m_tex_width;
					if (slice->m_height > m_tex_height)
						slice->m_height = m_tex_height;
					if (slice->m_dst_x + slice->m_width > m_tex_width)
						slice->m_dst_x = m_tex_width - slice->m_width;
					if (slice->m_dst_y + slice->m_height > m_tex_height)
						slice->m_dst_y = m_tex_height - slice->m_height;

					uint8_t *row_ptr =
						base_ptr + m_surface->GetStride() * slice->m_dst_y;

					for (int32_t row = 0; row < slice->m_height; row++) {
						uint8_t *alpha_ptr =
							row_ptr +
							slice->m_dst_x *
								dst_surf.m_bytes_per_pixel +
							3;

						if (alpha_ptr > end_ptr)
							break;
						for (int32_t col = 0; col < slice->m_width; col++) {
							sce_paf_memset(alpha_ptr, 0, 1);
							alpha_ptr += dst_surf.m_bytes_per_pixel;
						}
						row_ptr += m_surface->GetStride();
					}
				}
				m_surface->Unlock();
			}
			blit_list.Clear();
		}
	}
}

int32_t WebObj::SetSurfaceRect(ui::WebRect rect, bool skip_clear)
{
	int32_t ret = 0;

	if (m_surface.get() == NULL || rect.ApproxEquals(m_surface_rect) || skip_clear) {
		if (m_zoom_origin_rect.Width() == 0.0f || m_zoom_origin_rect.Height() == 0.0f)
			m_zoom_origin_rect = rect;
	} else if (m_zoom_state == 3) {
		ClearUncoveredArea(rect, m_pending_surface_rect, false);
		m_pending_surface_rect = rect;
	} else {
		ClearUncoveredArea(rect, m_surface_rect, false);
		ret = UpdateUvMatrix(&rect);
		if (ret == 0) {
			ret = UpdatePixelMatrix(&rect);
			m_surface_rect = rect;
			if (m_valid_rect.Width() == 0.0f || m_valid_rect.Height() == 0.0f) {
				m_valid_rect.m_right = rect.Width();
				m_valid_rect.m_top = rect.Height();
			}
		}
	}
	return ret;
}

int32_t WebObj::GetTextureSize(uint32_t *out_width, uint32_t *out_height)
{
	*out_width = (uint32_t)m_tex_width;
	*out_height = (uint32_t)m_tex_height;
	return m_tex_height;
}

int32_t WebObj::ResizeSurface(int32_t width, int32_t height)
{
	return CreateOrResizeSurface(width, height);
}

void WebObj::SetZoomState(int32_t state)
{
	if (m_zoom_plane == NULL)
		return;

	if (state != 0) {
		if (state == 3 && m_zoom_state == 0) {
			SetVisible(true);
			m_zoom_plane->SetVisible(false);
			state = 0;
		}
	} else {
		if (m_zoom_state == 3)
			state = 3;
		if (m_zoom_state == 0) {
			SetVisible(true);
			m_zoom_plane->SetVisible(false);
		}
	}
	m_zoom_state = state;
}

void WebObj::SetZoomActive(bool active)
{
	SetZoomState(active ? 2 : 0);
}

void WebObj::SetBlitEnabled(bool enabled)
{
	m_blit_enabled = enabled;
}

PlaneObj *WebObj::GetZoomPlane()
{
	if (m_zoom_plane == NULL)
		m_zoom_plane = new PlaneObj(PlaneObjStyle());
	return m_zoom_plane;
}

ui::WebRect WebObj::GetSurfaceRect()
{
	return m_surface_rect;
}

ui::WebRect WebObj::GetZoomOriginRect()
{
	return m_zoom_origin_rect;
}

void WebObj::DetachSurface()
{
	intrusive_ptr<Surface> empty_surface;

	SetTexture(empty_surface, 0);
	m_zoom_plane->SetTexture(empty_surface, 0);
	m_surface.clear();
}

int32_t WebObj::EnsureSurface(int32_t *out_width, int32_t *out_height)
{
	if (m_surface.get() == NULL && m_tex_width != 0 && m_tex_height != 0)
		CreateOrResizeSurface(m_tex_width, m_tex_height);
	*out_width = m_tex_width;
	*out_height = m_tex_height;
	return m_tex_height;
}

void WebObj::SetSize(math::v3 size)
{
	if (size.extract_x() * size.extract_y() == 0.0f)
		return;

	PlaneObj::SetSize(size);
	if (m_zoom_plane != NULL)
		m_zoom_plane->SetSize(m_size);
	if (m_surface.get() == NULL && m_tex_width != 0 && m_tex_height != 0)
		CreateOrResizeSurface(m_tex_width, m_tex_height);
}

int32_t WebObj::SetSurfaceOwner(void *owner)
{
	m_owner = static_cast<ui::WebSurfaceOwner *>(owner);
	return 0;
}

int32_t WebObj::ReleaseSurface()
{
	Finalize();
	return 0;
}

int32_t WebObj::SetPixelSize(int32_t width, int32_t height)
{
	SetSize(math::v3((float)width, (float)height, 0.0f));
	return 0;
}

void WebObj::RestoreOverlapAlpha(ui::WebRect blit_rect)
{
	if (m_surface.get() == NULL)
		return;

	if (m_valid_rect.Width() != m_surface_rect.Width() ||
	    m_valid_rect.Height() != m_surface_rect.Height()) {
		m_valid_rect = m_surface_rect;
		return;
	}

	if (m_valid_rect.m_left == m_surface_rect.m_left &&
	    m_valid_rect.m_right == m_surface_rect.m_right &&
	    m_valid_rect.m_top == m_surface_rect.m_top &&
	    m_valid_rect.m_bottom == m_surface_rect.m_bottom) {
		return;
	}

	int32_t stride = m_surface->GetStride();
	WebBlitSurface dst_surf;

	dst_surf.m_rect = m_valid_rect;
	dst_surf.m_width = stride / 4;
	dst_surf.m_height = m_surface->GetHeight();
	dst_surf.m_bytes_per_pixel = stride / dst_surf.m_width;

	WebBlitSurface src_surf;

	src_surf.m_rect = m_surface_rect;
	src_surf.m_width = stride / 4;
	src_surf.m_height = m_surface->GetHeight();
	src_surf.m_bytes_per_pixel = stride / src_surf.m_width;

	WebBlitList blit_list;

	if (blit_list.Build(src_surf, dst_surf, m_pixel_matrix, NULL) == 0) {
		if (blit_list.m_count > 0) {
			uint8_t *base_ptr = static_cast<uint8_t *>(m_surface->Lock(0));
			uint8_t *end_ptr = base_ptr + m_surface->GetPageSize();

			for (int32_t slice_idx = 0; slice_idx < blit_list.m_count; slice_idx++) {
				WebBlitSlice *slice = blit_list.GetSlice(slice_idx);

				if (slice == NULL)
					continue;

				if (slice->m_width > m_tex_width)
					slice->m_width = m_tex_width;
				if (slice->m_height > m_tex_height)
					slice->m_height = m_tex_height;
				if (slice->m_dst_x + slice->m_width > m_tex_width)
					slice->m_dst_x = m_tex_width - slice->m_width;
				if (slice->m_dst_y + slice->m_height > m_tex_height)
					slice->m_dst_y = m_tex_height - slice->m_height;

				uint8_t *row_ptr =
					base_ptr + m_surface->GetStride() * slice->m_dst_y;

				for (int32_t row = 0; row < slice->m_height; row++) {
					uint8_t *alpha_ptr =
						row_ptr +
						slice->m_dst_x * dst_surf.m_bytes_per_pixel + 3;

					if (alpha_ptr > end_ptr)
						break;
					for (int32_t col = 0; col < slice->m_width; col++) {
						sce_paf_memset(alpha_ptr, 0xFF, 1);
						alpha_ptr += dst_surf.m_bytes_per_pixel;
					}
					row_ptr += m_surface->GetStride();
				}
			}
			m_surface->Unlock();
		}
		blit_list.Clear();
	}

	ui::WebRect merged = m_valid_rect;

	merged.Intersect(m_surface_rect);
	if (merged.m_top == blit_rect.m_bottom)
		merged.m_top = blit_rect.m_top;
	else if (merged.m_right == blit_rect.m_left)
		merged.m_right = blit_rect.m_right;
	else if (merged.m_bottom == blit_rect.m_top)
		merged.m_bottom = blit_rect.m_bottom;
	else if (merged.m_left == blit_rect.m_right)
		merged.m_left = blit_rect.m_left;
	m_valid_rect = merged;
}

int32_t WebObj::Blit(float pos_x, float pos_y, float width, float height, int32_t src_width,
		     int32_t src_height, int32_t bytes_per_pixel, const void *src_pixels)
{
	if (m_zoom_state == 1)
		return 0;
	if (!m_accept_blit) {
		m_accept_blit = true;
		return 0;
	}
	if (!m_blit_enabled)
		return 0;

	int32_t ret = (int32_t)0x80AF0001;

	if (m_surface.get() == NULL)
		return (int32_t)0x80AF0022;

	if (m_surface->GetWidth() > 0 && m_surface->GetHeight() > 0) {
		WebBlitSurface src_surf;

		src_surf.m_rect.Set(pos_x, pos_y, pos_x + width, pos_y + height);
		src_surf.m_width = src_width;
		src_surf.m_height = src_height;
		src_surf.m_bytes_per_pixel = bytes_per_pixel;

		if (m_pending_surface_rect.Width() > 0.0f ||
		    m_pending_surface_rect.Height() > 0.0f) {
			ClearUncoveredArea(m_surface_rect, m_pending_surface_rect, false);
			m_surface_rect = m_pending_surface_rect;
			UpdateUvMatrix(&m_surface_rect);
			UpdatePixelMatrix(&m_surface_rect);
			m_pending_surface_rect.Set(0.0f, 0.0f, 0.0f, 0.0f);
		}

		if (src_width == m_surface->GetWidth() || src_height == m_surface->GetHeight() ||
		    m_zoom_state == 3) {
			RestoreOverlapAlpha(src_surf.m_rect);
		} else {
			m_valid_rect = m_surface_rect;
		}

		WebBlitSurface dst_surf;
		int32_t stride = m_surface->GetStride();

		dst_surf.m_rect = m_surface_rect;
		dst_surf.m_width = stride / bytes_per_pixel;
		dst_surf.m_height = m_surface->GetHeight();
		dst_surf.m_bytes_per_pixel = stride / dst_surf.m_width;

		WebBlitList blit_list;

		if (blit_list.Build(src_surf, dst_surf, m_pixel_matrix, src_pixels) == 0) {
			if (blit_list.m_count > 0) {
				uint8_t *base_ptr = static_cast<uint8_t *>(m_surface->Lock(0));

				for (int32_t slice_idx = 0; slice_idx < blit_list.m_count;
				     slice_idx++) {
					WebBlitSlice *slice = blit_list.GetSlice(slice_idx);

					if (slice == NULL)
						continue;

					if (slice->m_width > m_tex_width)
						slice->m_width = m_tex_width;
					if (slice->m_height > m_tex_height)
						slice->m_height = m_tex_height;
					if (slice->m_dst_x + slice->m_width > m_tex_width)
						slice->m_dst_x = m_tex_width - slice->m_width;
					if (slice->m_dst_y + slice->m_height > m_tex_height)
						slice->m_dst_y = m_tex_height - slice->m_height;

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
				m_surface->Unlock();
			}
			blit_list.Clear();
		}
		ret = 0;
	}

	if (src_width == m_surface->GetWidth() || src_height == m_surface->GetHeight() ||
	    m_zoom_state == 3) {
		UpdateTexRegion(&m_view_rect);
		if (m_zoom_state == 3)
			m_valid_rect = m_surface_rect;
		m_zoom_state = 0;
		if (m_zoom_plane != NULL) {
			SetVisible(true);
			m_zoom_plane->SetVisible(false);
			m_zoom_plane->SetTexRegion(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			m_zoom_origin_rect = m_surface_rect;
		}
	}
	return ret;
}

int32_t WebObj::NotifyViewportRect(float pos_x, float pos_y, float width, float height)
{
	ui::WebRect ref_rect = (m_zoom_state == 3) ? m_pending_surface_rect : m_surface_rect;

	m_accept_blit = ((int32_t)(width * 100.0f) == (int32_t)(ref_rect.Width() * 100.0f)) &&
			((int32_t)(height * 100.0f) == (int32_t)(ref_rect.Height() * 100.0f));

	ui::WebRect incoming_rect;

	incoming_rect.Set(pos_x, pos_y, pos_x + width, pos_y + height);

	ui::WebRect overlap = incoming_rect;

	overlap.Intersect(ref_rect);
	if (overlap.Width() == 0.0f || overlap.Height() == 0.0f)
		m_accept_blit = false;
	return 0;
}

int32_t WebObj::OnRenderEnd()
{
	return 0;
}

}
}
