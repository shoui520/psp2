/*
 * paf::graph::Surface on GXM: the texture's sampling, the memory a surface's pages take for
 * its format and order, the copies into tiled and swizzled pages, the GXM textures over the
 * pages, and the mipmaps (the file's name ours: 0x8108fd84..0x81090b98, after
 * surface_display.cpp).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/surface/surface.h>
#include <image/image_filter.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <src/paf_types.h>

namespace paf {
namespace graph {

void Surface::SetPageOffset(int32_t offset)
{
	m_page_offset = offset % m_num_page;
}

int32_t Surface::SetMinFilter(TextureFilter value)
{
	uint32_t min_filter;
	uint32_t mip_filter;

	switch (value) {
	case NEAREST:
	case NEAREST_MIPMAP_NEAREST:
		mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
		min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
		break;
	case LINEAR:
	case LINEAR_MIPMAP_NEAREST:
		min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
		mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
		break;
	case NEAREST_MIPMAP_LINEAR:
		mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
		min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
		break;
	case LINEAR_MIPMAP_LINEAR:
		min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
		mip_filter = SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
		break;
	default:
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	}
	if (min_filter != m_texture_param.min_filter || mip_filter != m_texture_param.mip_filter) {
		m_texture_param.min_filter = min_filter;
		m_texture_param.mip_filter = mip_filter;
		m_texture_param_changed = true;
	}
	return 0;
}

int32_t Surface::SetMagFilter(TextureFilter value)
{
	int32_t mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;

	if (value != NEAREST)
		mag_filter = value == LINEAR ? SCE_GXM_TEXTURE_FILTER_LINEAR : -1;
	if ((uint32_t)mag_filter != m_texture_param.mag_filter) {
		if (mag_filter == -1)
			return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
		m_texture_param.mag_filter = mag_filter;
		m_texture_param_changed = true;
	}
	return 0;
}

/* GXM's addressing mode for a wrap; -1 for none */
static inline int32_t WrapToAddrMode(TextureWrap value)
{
	if (value == CLAMP)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	else if (value == REPEAT)
		return SCE_GXM_TEXTURE_ADDR_REPEAT;
	else if (value == CLAMP_TO_EDGE)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	else if (value == MIRROR)
		return SCE_GXM_TEXTURE_ADDR_MIRROR;
	else if (value == MIRROR_CLAMP)
		return SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP;
	else if (value == REPEAT_IGNORE_BORDER)
		return SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER;
	else if (value == CLAMP_IGNORE_BORDER)
		return SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER;
	else if (value == CLAMP_FULL_BORDER)
		return SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER;
	else if (value == CLAMP_HALF_BORDER)
		return SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER;
	return -1;
}

int32_t Surface::SetWrapS(TextureWrap value)
{
	int32_t mode = WrapToAddrMode(value);

	if (mode == -1)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	if ((uint32_t)mode != m_texture_param.wrap_s) {
		m_texture_param.wrap_s = mode;
		m_texture_param_changed = true;
	}
	return 0;
}

int32_t Surface::SetWrapT(TextureWrap value)
{
	int32_t mode = WrapToAddrMode(value);

	if (mode == -1)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	if ((uint32_t)mode != m_texture_param.wrap_t) {
		m_texture_param.wrap_t = mode;
		m_texture_param_changed = true;
	}
	return 0;
}

int32_t Surface::GetCopyHeight(int32_t pixel_align, int32_t level)
{
	int32_t height = m_height;

	for (;;) {
		if (level-- <= 0)
			break;
		if (height > 1)
			height >>= 1;
	}
	return height;
}

}
}

using namespace paf;
using namespace paf::graph;

/* the alignment's power of two at least that of the format's blocks (name ours) */
static inline int16_t AlignForFormat(ImageMode format, int16_t align)
{
	if (format >= ImageMode_PVRT2BPP && format <= ImageMode_PVRTII4BPP) {
		if (align < 3)
			align = 3;
	} else if (format >= ImageMode_DXT1 && format <= ImageMode_DXT5) {
		if (align < 3)
			align = 3;
	} else if (align < 2) {
		align = 2;
	}
	return align;
}

void ScePafGraphics_6CCB24AE(int16_t width, int16_t height, ImageMode format, ImageOrder order,
			     int32_t pixel_bits, int32_t num_level, int32_t line_bytes,
			     Surface::SurfaceType surf_type, int16_t num_page, int16_t *adj_width,
			     int16_t *adj_height, int32_t *stride, int16_t *align, int32_t *size)
{
	int32_t block;
	int32_t shift;
	int16_t page_align;

	if (order == ImageOrder_Swizzle || num_level > 1) {
		*adj_width = Surface::FloorLog2(width);
		*adj_height = Surface::FloorLog2(height);
	} else if (order == ImageOrder_Tiled) {
		*adj_width = (width + 31) & ~31;
		*adj_height = (height + 31) & ~31;
	} else if (order == ImageOrder_Linear) {
		*adj_width = (width + 7) & ~7;
		*adj_height = height;
	} else {
		*adj_width = 0;
		*adj_height = 0;
		return;
	}

	/* log2 of the pixels a DXT block is across */
	block = 0;
	if (format >= ImageMode_DXT1 && format <= ImageMode_DXT5)
		block = 2;

	if (surf_type == Surface::TypeDisplay) {
		if (*adj_width < 512)
			*adj_width = 512;
		if (line_bytes == 0)
			line_bytes = ((((*adj_width + (1 << block) - 1) & (-1 << block)) << pixel_bits) +
				      7) >> 3;
		else
			*adj_width = (line_bytes * 8) >> pixel_bits;
		*stride = line_bytes;
		if (*align == 0)
			*align = 9;
		*adj_height = (*adj_height + 15) & ~15;
		if (*size == 0)
			*size = (line_bytes * *adj_height + (1 << *align) - 1) & (-1 << *align);
		return;
	}

	if (line_bytes == 0)
		line_bytes = ((((*adj_width + (1 << block) - 1) & (-1 << block)) << pixel_bits) + 7) >>
			     3;
	else
		*adj_width = (line_bytes * 8) >> pixel_bits;
	page_align = *align;

	if (format == ImageMode_YUV420P2) {
		int32_t chroma = height * (((width >> 1) + 7) & ~7);
		int32_t luma;

		*stride = line_bytes;
		luma = line_bytes * *adj_height;
		if (surf_type == Surface::TypeRenderTarget) {
			if (page_align == 0) {
				page_align = 9;
				*align = page_align;
			}
		} else {
			page_align = AlignForFormat(format, page_align);
			*align = page_align;
		}
		shift = num_page != 1 ? page_align : 0;
		*size = (luma + chroma + (1 << shift) - 1) & (-1 << shift);
	} else if (format == ImageMode_YUV420P3) {
		int32_t luma;
		int32_t chroma;

		*stride = line_bytes;
		luma = line_bytes * *adj_height;
		chroma = (height >> 1) * (((width >> 1) + 7) & ~7);
		if (surf_type == Surface::TypeRenderTarget) {
			if (page_align == 0) {
				page_align = 9;
				*align = page_align;
			}
		} else {
			page_align = AlignForFormat(format, page_align);
			*align = page_align;
		}
		shift = num_page != 1 ? page_align : 0;
		*size = (luma + chroma * 2 + (1 << shift) - 1) & (-1 << shift);
	} else if (surf_type == Surface::TypeRenderTarget) {
		*stride = line_bytes;
		if (page_align == 0) {
			page_align = 9;
			*align = page_align;
		}
		shift = num_page != 1 ? page_align : 0;
		*size = (line_bytes * *adj_height + (1 << shift) - 1) & (-1 << shift);
	} else {
		int32_t lines;
		int32_t level_lines;
		int32_t level_height;
		int32_t rounded;
		int32_t i;

		*stride = line_bytes;
		page_align = AlignForFormat(format, page_align);
		*align = page_align;
		shift = num_page != 1 ? page_align : 0;

		/* the lines of every mipmap level, each rounded to whole blocks */
		level_height = *adj_height;
		rounded = level_height;
		level_lines = (level_height + (1 << block) - 1) & (-1 << block);
		lines = level_lines;
		i = num_level - 1;
		for (;;) {
			if (i-- <= 0)
				break;
			if ((1 << block) < level_height) {
				rounded = (1 << block) + (rounded >> 1);
				level_lines = (rounded - 1) & (-1 << block);
				level_height >>= 1;
			}
			lines += level_lines;
		}
		*size = (line_bytes * lines + (1 << shift) - 1) & (-1 << shift);
	}
}

int32_t ScePafGraphics_48FBF58D(Surface::SurfaceType surf_type, int32_t width, int32_t height,
				ImageMode format, ImageOrder order, int32_t num_level,
				int32_t line_bytes, int32_t align, int32_t num_page)
{
	int16_t adj_width;
	int16_t adj_height;
	int16_t page_align;
	int32_t stride;
	int32_t size;
	int32_t pixel_bits = SurfaceBase::GetPixelBits(format);

	page_align = align;
	adj_width = 0;
	adj_height = 0;
	stride = 0;
	size = 0;
	ScePafGraphics_6CCB24AE(width, height, format, order, pixel_bits, num_level, line_bytes,
				surf_type, num_page, &adj_width, &adj_height, &stride, &page_align,
				&size);
	return size;
}

namespace paf {
namespace graph {

int32_t Surface::GetPageSize(int32_t width, int32_t height, ImageMode format, ImageOrder order,
			     int32_t num_level, int32_t line_bytes, int32_t align, int32_t num_page)
{
	int16_t adj_width;
	int16_t adj_height;
	int16_t page_align;
	int32_t stride;
	int32_t size;
	int32_t pixel_bits = GetPixelBits(format);

	page_align = align;
	adj_width = 0;
	adj_height = 0;
	stride = 0;
	size = 0;
	ScePafGraphics_6CCB24AE(width, height, format, order, pixel_bits, num_level, line_bytes,
				TypeTexture, num_page, &adj_width, &adj_height, &stride, &page_align,
				&size);
	return size;
}

void Surface::FastMemcpy(void *dst, const void *src, int32_t n)
{
	sce_paf_memcpy(dst, src, n);
}

void Surface::ConvertTiledFormat(void *dst, const void *src, int32_t width, int32_t height)
{
	/* a line of the source; of a tile's 32 x 32 pixels, the whole tile and one line */
	int32_t line_bytes = ((width << m_pixel_bits) + 7) >> 3;
	int32_t tile_bytes = ((1024 << m_pixel_bits) + 7) >> 3;
	int32_t tile_line_bytes = ((32 << m_pixel_bits) + 7) >> 3;
	int32_t tile_row_bytes = tile_bytes * (width / 32);
	int32_t tile_row_offset = 0;
	int32_t tile_y;

	for (tile_y = 0; tile_y < height / 32; tile_y++) {
		uint8_t *dst_line = (uint8_t *)dst + tile_row_offset;
		const uint8_t *src_line = (const uint8_t *)src + tile_row_offset;
		int32_t y;

		for (y = 0; y < 32; y++) {
			uint8_t *d = dst_line;
			const uint8_t *s = src_line;
			int32_t x;

			for (x = 0; x < line_bytes; x += tile_line_bytes) {
				sce_paf_memcpy(d, s, tile_line_bytes);
				s += tile_line_bytes;
				d += tile_bytes;
			}
			dst_line += tile_line_bytes;
			src_line += line_bytes;
		}
		tile_row_offset += tile_row_bytes;
	}
}

int32_t Surface::ConvertSwizzleFormat(void *dst, const void *src, int32_t width, int32_t height)
{
	uint32_t num_x;
	uint32_t num_y;
	uint32_t bits;
	uint32_t bytes;
	uint32_t n_x;
	uint32_t n;
	uint32_t row;
	uint32_t y;

	/* in pixels, or in the 4 x 4 blocks of the DXT formats */
	switch (m_format) {
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
		num_x = width;
		num_y = height;
		bits = 1 << GetPixelBits(m_format);
		break;
	case ImageMode_DXT1:
		bits = 64;
		num_x = width >> 2;
		num_y = height >> 2;
		break;
	case ImageMode_DXT3:
	case ImageMode_DXT5:
		bits = 128;
		num_x = width >> 2;
		num_y = height >> 2;
		break;
	default:
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	}

	bytes = bits >> 3;
	n_x = GetN(num_x);
	n = GetN(num_y);
	if (n > n_x)
		n = n_x;
	row = 0;
	for (y = 0; y < num_y; y++) {
		const uint8_t *s = (const uint8_t *)src + row * bytes;
		uint32_t x;

		for (x = 0; x < num_x; x++) {
			uint32_t morton = 0;
			uint32_t i;

			/* the low bits of x and y interleaved, y's first */
			for (i = 0; i < n; i++)
				morton |= ((y & (1 << i)) << i) | ((x & (1 << i)) << (i + 1));
			sce_paf_memcpy((uint8_t *)dst +
					       (morton | ((x & ~(num_y - 1)) << n) |
						((y & ~(num_x - 1)) << n)) *
						       bytes,
				       s, bytes);
			s += bits >> 3;
		}
		row += num_x;
	}
	return 0;
}

int32_t Surface::GetScore() const
{
	return m_page_size * (3 - m_order);
}

void Surface::TexFlush()
{
}

int32_t Surface::GenTexture(Texture *tex, void *addr)
{
	uint32_t num_level = m_num_level;
	int32_t ret;

	if (m_format == ImageMode_YUV420P2 || m_format == ImageMode_YUV420P3)
		num_level = m_num_level != 1 ? m_num_level : 0;

	switch (m_order) {
	case ImageOrder_Linear:
		ret = sceGxmTextureInitLinear(tex->texture, addr, GetTextureFormat(this), m_tex_width,
					      m_tex_height, num_level);
		break;
	case ImageOrder_Swizzle:
		ret = sceGxmTextureInitSwizzled(tex->texture, addr, GetTextureFormat(this),
						m_tex_width, m_tex_height, num_level);
		break;
	case ImageOrder_Tiled:
		ret = sceGxmTextureInitTiled(tex->texture, addr, GetTextureFormat(this), m_tex_width,
					     m_tex_height, num_level);
		break;
	default:
		sce_paf_printf_dummy();
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	}
	if (ret < 0)
		return ret;
	return 0;
}

int32_t Surface::Copy_YUV420PX(int32_t page, const void *src, ImageOrder src_order,
			       int32_t line_bytes, int32_t level)
{
	const uint8_t *s = (const uint8_t *)src;
	uint8_t *dst;
	uint32_t num_chroma;

	if (src_order != ImageOrder_Linear || (line_bytes != 0 && m_stride != line_bytes) ||
	    level != -1)
		return SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;

	dst = (uint8_t *)m_mem.bp + m_page_size * page;
	/* YUV420P2 has one plane of chroma pairs, YUV420P3 two planes */
	num_chroma = 2;
	if (m_format == ImageMode_YUV420P2)
		num_chroma = 1;
	if (m_tex_pages != NULL) {
		int32_t width = m_width;
		int32_t height = m_height;
		int32_t tex_width = m_tex_width;
		int32_t chroma_width = width >> 1;
		int32_t chroma_height = height >> 1;
		uint32_t chroma_line;

		if (m_format == ImageMode_YUV420P2)
			chroma_height = m_height;
		chroma_line = ((tex_width >> 1) + 7) & ~7;
		if (width == tex_width && chroma_line == (uint32_t)chroma_width) {
			sce_paf_memcpy(dst, s,
				       width * height + num_chroma * (chroma_height * chroma_width));
		} else {
			int16_t y;
			uint32_t plane;

			for (y = 0; y < m_height; y++) {
				sce_paf_memcpy(dst, s, width);
				s += width;
				dst += tex_width;
			}
			for (plane = 0; plane < num_chroma; plane++) {
				int32_t i;

				for (i = 0; i < chroma_height; i++) {
					sce_paf_memcpy(dst, s, chroma_width);
					s += width >> 1;
					dst += chroma_line;
				}
			}
		}
	}
	return 0;
}

uint8_t *Surface::GetPagePtr(int32_t page, int32_t level) const
{
	uint8_t *ptr = GetPagePtr(page);

	if (ptr == NULL)
		return NULL;
	if (level > 0) {
		int32_t stride = m_stride;
		int32_t height = m_tex_height;
		int32_t offset = stride * height;
		int32_t i = level - 1;

		for (;;) {
			if (i-- <= 0)
				break;
			if (stride > 1)
				stride >>= 1;
			if (height > 1)
				height >>= 1;
			offset += stride * height;
		}
		ptr += offset;
	}
	return ptr;
}

}
}

int32_t ScePafGraphics_D811125C(Surface *surface, int32_t page, const void *src,
				ImageOrder src_order, int32_t line_bytes, int32_t level)
{
	uint8_t *dst = surface->GetPagePtr(page, level);
	int32_t height;
	int32_t ret;

	if (dst == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	if (src_order == ImageOrder_Swizzle)
		return surface->Copy_(page, src, ImageOrder_Swizzle, line_bytes, level);
	if (src_order != ImageOrder_Linear)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	height = LevelSize(surface->GetHeight(), level);
	ret = surface->ConvertSwizzleFormat(dst, src, LevelSize(surface->GetWidth(), level), height);
	if (ret < 0)
		return ret;
	return 0;
}

namespace paf {
namespace graph {

int32_t Surface::SetPalette(Texture *tex, const void *palette)
{
	int32_t ret = sceGxmTextureSetPalette(tex->texture, palette);

	if (ret < 0)
		return ret;
	return 0;
}

void Surface::DeleteTexture()
{
}

int32_t Surface::ActivateTexture(uint32_t map_type, float offset_u, float offset_v, float scale_u,
				 float scale_v)
{
	switch (m_format) {
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_A8:
	case ImageMode_L8:
	case ImageMode_DXT1:
	case ImageMode_DXT3:
	case ImageMode_DXT5:
	case ImageMode_index4:
	case ImageMode_index8:
	case ImageMode_YUV420P2:
	case ImageMode_YUV420P3:
		if (m_tex_pages != NULL) {
			Texture *tex = &m_tex_pages[m_page_counter];

			if (m_texture_param_changed) {
				sceGxmTextureSetMinFilter(
					tex->texture, (SceGxmTextureFilter)m_texture_param.min_filter);
				sceGxmTextureSetMagFilter(
					tex->texture, (SceGxmTextureFilter)m_texture_param.mag_filter);
				sceGxmTextureSetMipFilter(
					tex->texture,
					(SceGxmTextureMipFilter)m_texture_param.mip_filter);
				sceGxmTextureSetUAddrMode(
					tex->texture, (SceGxmTextureAddrMode)m_texture_param.wrap_s);
				sceGxmTextureSetVAddrMode(
					tex->texture, (SceGxmTextureAddrMode)m_texture_param.wrap_t);
			}
			m_texture_param_changed = false;
			sceGxmTextureSetLodBias(tex->texture, m_texture_param.lod_bias);
		}
		return 0;
	default:
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	}
}

void Surface::SetMemParam(int32_t line_bytes, int16_t adj_width, int16_t adj_height)
{
	ScePafGraphics_6CCB24AE(adj_width, adj_height, m_format, m_order, m_pixel_bits, m_num_level,
				line_bytes, m_surf_type, m_num_page, &m_tex_width, &m_tex_height,
				&m_stride, &m_mem.align, &m_page_size);
}

int32_t Surface::Copy(void *dst, const void *src, ImageOrder src_order, int32_t line_bytes)
{
	return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
}

int32_t Surface::DuplicateBoundary(uint8_t *buf)
{
	return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
}

void Surface::SetMemAttr()
{
	m_mem.attr = 0;
}

int32_t Surface::CreateMipmap()
{
	uint8_t *src;

	if (m_pixel_bits != 5)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	src = GetPagePtr(m_page_counter);
	if (src != NULL) {
		int32_t pixel_bytes = PixelsToBytes(1);
		int32_t width = m_tex_width;
		int32_t height = m_tex_height;
		int32_t stride = m_stride;
		int32_t level;

		for (level = 0; level < m_num_level - 1; level++) {
			int32_t next_stride = stride;
			uint8_t *dst;

			if (stride > pixel_bytes)
				next_stride = stride >> 1;
			dst = src + height * stride;
			image::filter::ResizeHalf(dst, src, next_stride, stride, width, height);
			src = dst;
			if (width > 0)
				width >>= 1;
			if (height > 0)
				height >>= 1;
			stride = next_stride;
		}
		return 0;
	}
	if (m_page_size > 0)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	return 0;
}

int32_t Surface::GetStride(int32_t level) const
{
	int32_t stride = m_stride;

	for (;;) {
		if (level-- <= 0)
			break;
		if (stride > 1)
			stride >>= 1;
	}
	return stride;
}

SceGxmTexture *Surface::GetRawTexture()
{
	int32_t page;

	if (m_tex_pages == NULL)
		return NULL;
	page = m_page_counter;
	if (m_page_offset != 0) {
		page += m_page_offset;
		if (page < 0)
			page = m_num_page - 1;
		else if (m_num_page < page)
			page = 0;
	}
	return m_tex_pages[page].texture;
}

SceGxmTextureFormat Surface::GetRawTextureFormat()
{
	if (m_tex_pages == NULL || GetRawTexture() == NULL)
		return (SceGxmTextureFormat)0;
	return sceGxmTextureGetFormat(GetRawTexture());
}

int32_t SurfaceCLUT::ActivateCLUT()
{
	return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
}

int32_t SurfaceCLUT::GetCLUTSize() const
{
	return ((m_num_entry << m_pixel_bits) + 7) >> 3;
}

}
}
