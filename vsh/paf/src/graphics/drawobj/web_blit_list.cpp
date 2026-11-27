/*
 * paf::graph::WebBlitList implementation for 2014 PAF WebObj/MapObj.
 */
#include <graphics/drawobj/web_blit_list.h>
#include <std/math.h>
#include <stddef.h>

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Vector3;

static inline Vector3 TransformPoint2D(const Matrix3 &mat, float coord_x, float coord_y)
{
	return mat.getCol0() * coord_x + mat.getCol1() * coord_y + mat.getCol2();
}

WebBlitList::WebBlitList()
	: m_count(0), m_slices(NULL)
{
}

WebBlitList::~WebBlitList()
{
	if (m_slices != NULL) {
		delete[] m_slices;
		m_slices = NULL;
	}
}

int32_t WebBlitList::Build(WebBlitSurface src, WebBlitSurface dst,
			   const Matrix3 &pixel_matrix, const void *src_pixels)
{
	ui::WebRect clipped = dst.m_rect;

	clipped.Intersect(src.m_rect);
	if (clipped.Width() <= 0.0f || clipped.Height() <= 0.0f)
		return (int32_t)0x80AF0001;

	Vector3 clip_top_left = TransformPoint2D(pixel_matrix, clipped.m_left, clipped.m_top);
	Vector3 clip_bottom_right = TransformPoint2D(pixel_matrix, clipped.m_right, clipped.m_bottom);
	Vector3 src_top_left = TransformPoint2D(pixel_matrix, src.m_rect.m_left, src.m_rect.m_top);
	Vector3 src_bottom_right = TransformPoint2D(pixel_matrix, src.m_rect.m_right, src.m_rect.m_bottom);

	float clip_left_px = clip_top_left.getX();
	if (clip_left_px < 0.0f)
		clip_left_px = 0.0f;
	float clip_top_px = clip_top_left.getY();
	float clip_right_px = clip_bottom_right.getX();
	float clip_bottom_px = clip_bottom_right.getY();

	int32_t dst_y_end = 0;
	int32_t dst_y_start = 0;
	if (dst.m_height > 0) {
		int32_t top_mod = (int32_t)sce_paf_ceilf(clip_top_px) % dst.m_height;
		int32_t bottom_mod = (int32_t)sce_paf_floorf(clip_bottom_px) % dst.m_height;
		int32_t src_top_mod = (int32_t)sce_paf_ceilf(src_top_left.getY()) % dst.m_height;

		(void)sce_paf_floorf(src_bottom_right.getY());
		if (top_mod == 0)
			top_mod = (int32_t)sce_paf_ceilf(clip_top_px);
		if (src_top_mod == 0)
			(void)sce_paf_ceilf(src_top_left.getY());

		dst_y_end = top_mod;
		dst_y_start = bottom_mod;
		if (dst_y_end == dst_y_start)
			dst_y_start = bottom_mod + 1;
	}

	int32_t dst_x_start = 0;
	int32_t dst_x_end = 0;
	if (dst.m_width > 0) {
		int32_t left_mod = (int32_t)sce_paf_floorf(clip_left_px) % dst.m_width;
		int32_t right_mod = (int32_t)sce_paf_ceilf(clip_right_px) % dst.m_width;
		int32_t src_right_mod = (int32_t)sce_paf_ceilf(src_bottom_right.getX()) % dst.m_width;

		(void)sce_paf_floorf(src_top_left.getX());
		if (right_mod == 0)
			right_mod = (int32_t)sce_paf_ceilf(clip_right_px);
		if (src_right_mod == 0)
			(void)sce_paf_ceilf(src_bottom_right.getX());

		dst_x_start = left_mod;
		dst_x_end = right_mod;
		if (dst_x_start == dst_x_end)
			dst_x_start = left_mod + 1;
	}

	int32_t tile_x_begin = (int32_t)sce_paf_floorf(clip_left_px) / dst.m_width;
	int32_t tile_y_end = (int32_t)sce_paf_floorf(clip_top_px) / dst.m_height;
	int32_t tile_x_end = (int32_t)sce_paf_ceilf(clip_right_px) / dst.m_width;
	int32_t tile_y_begin = (int32_t)sce_paf_ceilf(clip_bottom_px) / dst.m_height;

	if (((int32_t)sce_paf_floorf(clip_top_px) % dst.m_height) == 0)
		tile_y_end--;
	if (((int32_t)sce_paf_ceilf(clip_right_px) % dst.m_width) == 0)
		tile_x_end--;

	bool split_horizontal = false;
	if (tile_x_begin == tile_x_end) {
		if (tile_y_end == tile_y_begin)
			m_count = 1;
		else
			m_count = 2;
	} else if (tile_y_end == tile_y_begin) {
		m_count = 2;
		split_horizontal = true;
	} else {
		m_count = 4;
	}

	int32_t src_x_offset = (int32_t)sce_paf_floorf(clip_left_px - src_top_left.getX());
	int32_t src_y_offset = (int32_t)sce_paf_floorf(clip_bottom_px - src_bottom_right.getY());

	if (m_slices != NULL)
		delete[] m_slices;
	m_slices = new WebBlitSlice[m_count];
	if (m_slices != NULL) {
		const uint8_t *base_pixels = static_cast<const uint8_t *>(src_pixels);
		int32_t src_stride = src.m_width * src.m_bytes_per_pixel;

		if (m_count == 2) {
			if (split_horizontal) {
				int32_t left_width = dst.m_width - dst_x_start;
				int32_t slice_height = (int32_t)sce_paf_ceilf(clip_top_px - clip_bottom_px);
				const uint8_t *first_row =
					base_pixels + (src_x_offset + src_y_offset * src.m_width) * src.m_bytes_per_pixel;

				m_slices[0].m_dst_x = dst_x_start;
				m_slices[0].m_dst_y = dst_y_start;
				m_slices[0].m_width = left_width;
				m_slices[0].m_height = slice_height;
				m_slices[0].m_src_stride = src_stride;
				m_slices[0].m_src_pixels = first_row;

				m_slices[1].m_dst_x = 0;
				m_slices[1].m_dst_y = dst_y_start;
				m_slices[1].m_width = dst_x_end;
				m_slices[1].m_height = slice_height;
				m_slices[1].m_src_stride = src_stride;
				m_slices[1].m_src_pixels = first_row + left_width * src.m_bytes_per_pixel;
			} else {
				int32_t slice_width = (int32_t)sce_paf_ceilf(clip_right_px - clip_left_px);
				int32_t bottom_height = dst.m_height - dst_y_start;
				int32_t second_src_y = src_y_offset + bottom_height;

				m_slices[0].m_dst_x = dst_x_start;
				m_slices[0].m_dst_y = dst_y_start;
				m_slices[0].m_width = slice_width;
				m_slices[0].m_height = bottom_height;
				m_slices[0].m_src_stride = src_stride;
				m_slices[0].m_src_pixels =
					base_pixels + (src_x_offset + src_y_offset * src.m_width) * src.m_bytes_per_pixel;

				m_slices[1].m_dst_x = dst_x_start;
				m_slices[1].m_dst_y = 0;
				m_slices[1].m_width = slice_width;
				m_slices[1].m_height = dst_y_end;
				m_slices[1].m_src_stride = src_stride;
				m_slices[1].m_src_pixels =
					base_pixels + (src_x_offset + second_src_y * src.m_width) * src.m_bytes_per_pixel;
			}
		} else if (m_count == 4) {
			int32_t left_width = dst.m_width - dst_x_start;
			int32_t bottom_height = dst.m_height - dst_y_start;
			const uint8_t *bottom_row =
				base_pixels + (src_x_offset + src_y_offset * src.m_width) * src.m_bytes_per_pixel;
			const uint8_t *top_row =
				base_pixels + (src_x_offset + (src_y_offset + bottom_height) * src.m_width) * src.m_bytes_per_pixel;

			m_slices[0].m_dst_x = dst_x_start;
			m_slices[0].m_dst_y = dst_y_start;
			m_slices[0].m_width = left_width;
			m_slices[0].m_height = bottom_height;
			m_slices[0].m_src_stride = src_stride;
			m_slices[0].m_src_pixels = bottom_row;

			m_slices[1].m_dst_x = 0;
			m_slices[1].m_dst_y = dst_y_start;
			m_slices[1].m_width = dst_x_end;
			m_slices[1].m_height = bottom_height;
			m_slices[1].m_src_stride = src_stride;
			m_slices[1].m_src_pixels = bottom_row + left_width * src.m_bytes_per_pixel;

			m_slices[2].m_dst_x = dst_x_start;
			m_slices[2].m_dst_y = 0;
			m_slices[2].m_width = left_width;
			m_slices[2].m_height = dst_y_end;
			m_slices[2].m_src_stride = src_stride;
			m_slices[2].m_src_pixels = top_row;

			m_slices[3].m_dst_x = 0;
			m_slices[3].m_dst_y = 0;
			m_slices[3].m_width = dst_x_end;
			m_slices[3].m_height = dst_y_end;
			m_slices[3].m_src_stride = src_stride;
			m_slices[3].m_src_pixels = top_row + left_width * src.m_bytes_per_pixel;
		} else if (m_count == 1) {
			m_slices[0].m_dst_x = dst_x_start;
			m_slices[0].m_dst_y = dst_y_start;
			m_slices[0].m_width = (int32_t)sce_paf_ceilf(clip_right_px - clip_left_px);
			m_slices[0].m_height = (int32_t)sce_paf_ceilf(clip_top_px - clip_bottom_px);
			m_slices[0].m_src_stride = src_stride;
			m_slices[0].m_src_pixels =
				base_pixels + (src_x_offset + src_y_offset * src.m_width) * src.m_bytes_per_pixel;
		}
	}

	return 0;
}

WebBlitSlice *WebBlitList::GetSlice(int32_t index)
{
	if (m_slices != NULL && index >= 0 && index < m_count)
		return &m_slices[index];
	return NULL;
}

void WebBlitList::Clear()
{
	m_count = 0;
	if (m_slices != NULL) {
		delete[] m_slices;
		m_slices = NULL;
	}
}

}
}
