/*
 * paf::graph::Surface: a texture of one or more pages and levels in memory from a surface pool
 * (or the caller's), reference counted by the surface itself and kept in the list of surfaces;
 * and SurfaceCLUT, the colour table of the indexed formats (the file's name ours, after the
 * header).
 */
#include <stddef.h>
#include <stdint.h>
#include <sce_atomic.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/stdio.h>
#include <thread/thread.h>
#include <file/file.h>
#include <memory/heap_allocator.h>
#include <src/paf_types.h>

namespace paf {
namespace graph {

/* a size at a mipmap level: halved per level, never below 1 */
static inline int32_t LevelSize(int32_t size, int32_t level)
{
	while (level-- > 0) {
		if (size > 1)
			size >>= 1;
	}
	return size;
}

int32_t Surface::AddRef()
{
	sceAtomicIncrement32AcqRel(&m_ref_count);
	return m_ref_count;
}

int32_t Surface::Release()
{
	if (sceAtomicDecrement32AcqRel(&m_ref_count) == 1) {
		delete this;
		return 0;
	}
	return m_ref_count;
}

int32_t SurfaceCLUT::AddRef()
{
	sceAtomicIncrement32AcqRel(&m_ref_count);
	return m_ref_count;
}

int32_t SurfaceCLUT::Release()
{
	if (sceAtomicDecrement32AcqRel(&m_ref_count) == 1) {
		delete this;
		return 0;
	}
	return m_ref_count;
}

int32_t Surface::AttachCLUT(const intrusive_ptr<SurfaceCLUT> &clut)
{
	if (clut.get() == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	m_clut = clut.get();
	for (int32_t i = 0; i < m_num_page; i++)
		SetPalette(&m_tex_pages[i], clut->GetCLUT());
	return 0;
}

}
}

using namespace paf;
using namespace paf::graph;

/* called whole in Sony's DecodeImage */
__attribute__((noinline))
int32_t ScePafGraphics_F4986A3A(SurfaceCLUT *clut, const void *src)
{
	if (clut->m_clut != NULL) {
		sce_paf_memcpy(clut->m_clut, src,
			       ((clut->m_num_entry << clut->m_pixel_bits) + 7) >> 3);
		return 0;
	}
	return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
}

namespace paf {
namespace graph {

SurfaceCLUT::SurfaceCLUT(SurfacePool *pool, ImageMode format, int32_t num_entry)
	: SurfaceBase(format, false)
{
	m_num_entry = num_entry;
	m_ref_count = 0;
	m_clut_size = GetCLUTSize();
	m_clut = NULL;
	m_pool = pool;
	if (pool != NULL) {
		m_clut = pool->Allocate(64, m_clut_size);
		if (m_clut == NULL && ScePafGraphics_404AC3D7(m_pool) != 0)
			sce_paf_printf_dummy();
	}
	m_clut_mutex.Lock();
	m_prev = &m_clut_list;
	m_next = m_clut_list.m_next;
	m_clut_list.m_next->m_prev = this;
	m_clut_list.m_next = this;
	m_clut_mutex.Unlock();
}

void *SurfaceCLUT::operator new(size_t size)
{
	return sce_paf_malloc(size);
}

int32_t Surface::FillPadding(int32_t page, const void *color, int32_t level)
{
	if (m_order != ImageOrder_Linear)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;

	uint8_t *page_ptr = GetPagePtr(page, level);

	if (page_ptr == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;

	int32_t ret = 0;
	int32_t pixel_align = 0;

	if (m_format >= ImageMode_DXT1 && m_format <= ImageMode_DXT5)
		pixel_align = 2;

	int32_t copy_height = GetCopyHeight(pixel_align, level);
	int32_t stride = GetStride(level);
	int32_t line = ((LevelSize(m_width, level) << m_pixel_bits) + 7) >> 3;
	int32_t padding = stride - line;
	ImageMode format = m_format;

	if (copy_height == m_height && stride == line)
		return 0;

	uint8_t fill[4];

	fill[0] = 0;
	fill[1] = 0;
	fill[2] = 0;
	fill[3] = 0;
	if (color != NULL) {
		int32_t size;

		switch (format) {
		case ImageMode_RGBA8888:
		case ImageMode_BGRA8888:
			*(uint32_t *)fill = *(const uint32_t *)color;
			size = 4;
			break;
		case ImageMode_A8:
		case ImageMode_L8:
		case ImageMode_index8:
			fill[0] = *(const uint8_t *)color;
			size = 1;
			break;
		case ImageMode_index4:
			fill[0] = *(const uint8_t *)color | (*(const uint8_t *)color << 4);
			size = 1;
			break;
		default:
			return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
		}

		/* an odd 4-bit line ends in half a byte: its high nibble is padding */
		if (GetPixelBits(format) < 3 && (m_width & 1) != 0) {
			uint8_t *row = page_ptr;

			for (int32_t y = 0; y < m_height; y++) {
				row[line - 1] = (row[line - 1] & 0xf) | (fill[0] & 0xf0);
				row += stride;
			}
		}

		uint8_t *row = page_ptr;

		for (int32_t y = 0; y < m_height; y++) {
			for (int32_t x = 0; x < padding; x += size)
				FastMemcpy(row + line + x, fill, size);
			row += stride;
		}

		int32_t rows = LevelSize(m_tex_height, level) - copy_height;

		row = page_ptr + copy_height * stride;
		for (int32_t y = 0; y < rows; y++) {
			for (int32_t x = 0; x < stride; x += size)
				FastMemcpy(row + x, fill, size);
			row += stride;
		}
	} else {
		/* the line's last pixel repeated to the end, the last line down */
		int32_t size = ((1 << GetPixelBits(format)) + 7) >> 3;
		uint8_t *row = page_ptr;

		for (int32_t y = 0; y < m_height; y++) {
			if (GetPixelBits(m_format) < 3) {
				uint8_t last = row[line - 1];

				if ((m_width & 1) != 0) {
					last = (last & 0xf) | (last << 4);
					row[line - 1] = last;
				}
				fill[0] = (last & 0xf0) | (last >> 4);
			} else {
				FastMemcpy(fill, row + line - size, size);
			}
			for (int32_t x = 0; x < padding; x += size)
				FastMemcpy(row + line + x, fill, size);
			row += stride;
		}

		int32_t rows = LevelSize(m_tex_height, level) - copy_height;

		row = page_ptr + copy_height * stride;
		for (int32_t y = 0; y < rows; y++) {
			for (int32_t x = 0; x < stride; x += size)
				FastMemcpy(row + x, row - stride + x, size);
			row += stride;
		}
	}
	return ret;
}

int32_t Surface::Copy_(int32_t page, const void *src, ImageOrder src_order, int32_t line_bytes,
		       int32_t level)
{
	uint8_t *dst = GetPagePtr(page, level);

	if (dst == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	if (line_bytes == 0)
		line_bytes = ((m_width << m_pixel_bits) + 7) >> 3;

	int32_t copy_height = GetCopyHeight(0, level);
	int32_t stride = GetStride(level);
	uint32_t line = ((LevelSize(m_width, level) << m_pixel_bits) + 7) >> 3;

	if ((m_order == ImageOrder_Linear && src_order == ImageOrder_Linear) ||
	    (m_order == ImageOrder_Swizzle && src_order == ImageOrder_Swizzle)) {
		if (stride == line_bytes) {
			FastMemcpy(dst, src, copy_height * stride);
		} else {
			const uint8_t *p = (const uint8_t *)src;

			for (int32_t y = 0; y < copy_height; y++) {
				sce_paf_memcpy(dst, p, line);
				p += line_bytes;
				dst += stride;
			}
		}
	} else if (m_order == ImageOrder_Tiled && src_order == ImageOrder_Linear) {
		ConvertTiledFormat(dst, src, LevelSize(m_width, level), copy_height);
	} else if (m_order == ImageOrder_Swizzle && src_order == ImageOrder_Linear) {
		int32_t ret = ConvertSwizzleFormat(dst, src, LevelSize(m_width, level), copy_height);

		if (ret < 0)
			return ret;
		if (ret != 0)
			return 0;
	} else {
		sce_paf_printf_dummy();
		return 0;
	}
	if (m_num_level > 1 && level < 0)
		CreateMipmap();
	return 0;
}

int32_t Surface::Copy(int32_t page, const void *src, ImageOrder src_order, int32_t line_bytes,
		      int32_t level)
{
	if (page < 0 || page >= m_num_page || src == NULL || level >= m_num_level)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	if (m_format == ImageMode_YUV420P2 || m_format == ImageMode_YUV420P3)
		return Copy_YUV420PX(page, src, src_order, line_bytes, level);
	if (m_format >= ImageMode_DXT1 && m_format <= ImageMode_DXT5)
		return ScePafGraphics_D811125C(this, page, src, src_order, line_bytes, level);
	return Copy_(page, src, src_order, line_bytes, level);
}

void Surface::GenTextureAll()
{
	if (m_tex_pages == NULL || m_mem.bp == NULL)
		return;
	for (int32_t i = 0; i < m_num_page; i++)
		GenTexture(&m_tex_pages[i], (uint8_t *)m_mem.bp + m_page_size * i);
}

Surface::Texture::Texture()
{
	texture = NULL;
	m_owned = true;
	texture = (SceGxmTexture *)::operator new(0x10);
}

int16_t Surface::GetAdjustHeight(SurfacePool *pool, int32_t height)
{
	return height;
}

int16_t Surface::GetAdjustWidth(SurfacePool *pool, int32_t width)
{
	return width;
}

void Surface::InitObj(int32_t line_bytes)
{
	if (m_width > 4096) {
		sce_paf_printf_dummy();
		return;
	}
	if (m_height > 4096) {
		sce_paf_printf_dummy();
		return;
	}
	if (m_num_level <= 0 || m_num_level > 13) {
		sce_paf_printf_dummy();
		return;
	}
	if ((m_format == ImageMode_YUV420P2 || m_format == ImageMode_YUV420P3) &&
	    (m_num_level > 1 || m_order != ImageOrder_Linear || line_bytes != 0)) {
		sce_paf_printf_dummy();
		return;
	}
	if (m_format >= ImageMode_DXT1 && m_format <= ImageMode_DXT5 &&
	    m_order == ImageOrder_Linear)
		m_order = ImageOrder_Swizzle;
	if (m_pixel_bits == 0)
		line_bytes = 0;
	SetMemParam(line_bytes, m_width, m_height);
	m_factor_u = (float)m_width / (float)m_tex_width;
	m_factor_v = (float)m_height / (float)m_tex_height;
	m_clut = NULL;
	m_score = GetScore();
	sce_paf_memset(&m_texture_param, 0, sizeof(m_texture_param));
	SetMinFilter(LINEAR_MIPMAP_NEAREST);
	SetMagFilter(LINEAR);
	SetWrapS(CLAMP_TO_EDGE);
	SetWrapT(CLAMP_TO_EDGE);
	m_texture_param.lod_bias = 0;
	if (m_num_page != 0) {
		m_tex_pages = new Texture[m_num_page];
		if (m_tex_pages == NULL) {
			sce_paf_printf_dummy();
			return;
		}
	}
	SurfaceListNode *list = m_surface_list.m_prev;

	m_prev = list;
	m_next = list->m_next;
	list->m_next->m_prev = this;
	list->m_next = this;
}

uint32_t Surface::GetN(uint32_t value)
{
	uint32_t bit = 0x80000000;

	for (uint32_t n = 31; n != 0; n--) {
		if ((value & bit) != 0)
			return n;
		bit >>= 1;
	}
	return 0;
}

void Surface::InitMember()
{
	m_div_ratio_v = 0;
	m_div_ratio_h = 0;
	m_texture_param_changed = false;
	m_ref_count = 0;
	m_locked = 0;
	m_texturized = 0;
	m_symmetry_u = 0;
	m_symmetry_v = 0;
	m_cleared = 0;
	m_tex_pages = NULL;
	m_divide_mode = 0;
	m_div_ratio = NULL;
	m_margin_left = 0;
	m_margin_right = 0;
	m_margin_top = 0;
	m_margin_bottom = 0;
	m_page_counter = 0;
	m_reserved0 = 0;
	m_page_size = 0;
	m_depth = NULL;
}

int32_t Surface::Initialize(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			    ImageOrder order, SurfaceType surf_type, int32_t num_page,
			    int32_t align_bytes, int32_t size_byte, int32_t line_bytes,
			    int32_t num_level)
{
	m_list_cs.Lock();
	InitMember();
	m_width = width;
	m_height = height;
	m_order = order;
	m_surf_type = surf_type;
	m_mem_class = Unallocated;
	m_num_level = num_level;
	m_num_page = num_page;
	m_pool = pool;
	sce_paf_memset(&m_mem, 0, sizeof(m_mem));
	m_mem.align = GetN(align_bytes);
	InitObj(line_bytes);
	m_mem.bp = NULL;
	if (pool != NULL) {
		if (num_page != 0) {
			if (size_byte != 0)
				m_mem.bp = pool->Allocate(align_bytes, num_page * size_byte);
			else
				m_mem.bp = pool->Allocate(align_bytes, m_page_size * num_page);
		}
		if ((num_page == 0 || m_mem.bp == NULL) && ScePafGraphics_404AC3D7(m_pool) != 0) {
			sce_paf_printf_dummy();
			sce_paf_printf_dummy();
		}
	}
	SetMemAttr();
	if (m_tex_pages != NULL && m_mem.bp != NULL) {
		for (int32_t i = 0; i < m_num_page; i++)
			GenTexture(&m_tex_pages[i], (uint8_t *)m_mem.bp + m_page_size * i);
	}
	m_list_cs.Unlock();
	return 0;
}

Surface::Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		 ImageOrder order, int32_t num_page, int32_t align_bytes, int32_t line_bytes,
		 int32_t num_level, bool reserved)
	: SurfaceBase(format, reserved)
{
	Initialize(pool, width, height, format, order, TypeTexture, num_page, align_bytes, 0,
		   line_bytes, num_level);
}

void *Surface::operator new(size_t size)
{
	return sce_paf_malloc(size);
}

int32_t Surface::SetDecodeImageOption(SurfacePool *pool, LoadOption *option,
				      intrusive_ptr<Surface> &surf,
				      common::SharedPtr<image::Image> image)
{
	if (option == NULL)
		return 0;
	if (option->cancel != NULL)
		image->SetDecodeCancelFunc(option->cancel, option->cancel_userdata);
	if (image->GetFormat() == image::ImageFormat_Gim ||
	    image->GetFormat() == image::ImageFormat_Dds ||
	    image->GetFormat() == image::ImageFormat_Gxt)
		return 0;

	image::ImageExtent extent = image->GetExtent();
	int32_t width = extent.w;
	int32_t height = extent.h;
	image::ImageRect rect(0, 0, width, height);

	if (option->sub_rect.extent.w > 0 && option->sub_rect.extent.h > 0) {
		int32_t right = option->sub_rect.origin.x + option->sub_rect.extent.w;
		int32_t bottom = option->sub_rect.origin.y + option->sub_rect.extent.h;

		if (option->sub_rect.origin.x > 0)
			rect.origin.x = option->sub_rect.origin.x;
		if (option->sub_rect.origin.y > 0)
			rect.origin.y = option->sub_rect.origin.y;
		rect.extent.w = (width > right ? right : width) - rect.origin.x;

		int32_t h = (height > bottom ? bottom : height) - rect.origin.y;

		if (rect.extent.w <= 0 || h <= 0)
			return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
		rect.extent.h = h;
		image->SetDecSubRect(rect);
		width = rect.extent.w;
		height = rect.extent.h;
	}
	if (option->scale.w > 0 && option->scale.h > 0) {
		int32_t w = option->scale.w;
		int32_t h = option->scale.h;

		if (option->keep_aspect) {
			float fw = (float)w;
			float fh = (float)h;
			float aspect = (float)width / (float)height;

			if (fw / fh <= aspect) {
				fh = fw / aspect;
				if (fh < 1.0f)
					fh = 1.0f;
			} else {
				fw = aspect * fh;
				if (fw < 1.0f)
					fw = 1.0f;
			}
			w = (int32_t)fw;
			h = (int32_t)fh;
		}
		image->SetDecExtent(image::ImageExtent(w, h), option->resize_type);
	}
	return 0;
}

int32_t Surface::DecodeImage(SurfacePool *pool, LoadOption *option, intrusive_ptr<Surface> &surf,
			     common::SharedPtr<image::Image> image)
{
	int32_t rotate_type = option != NULL ? option->rotate_type : 0;
	int32_t flip_type = option != NULL ? option->flip_type : 0;
	ImageOrder image_order = option != NULL ? option->image_order : ImageOrder_NONE;
	int32_t image_order_param = option != NULL ? option->image_order_param : 0;
	uint32_t extlines = option != NULL ? option->extlines : 0;
	common::SharedPtr<image::Image> rotate_image;
	common::SharedPtr<image::Image> flip_image;
	common::SharedPtr<image::Image> convert_image;
	common::SharedPtr<image::Image> edge_image;

	SetDecodeImageOption(pool, option, surf, image);
	if (image->GetFormat() == image::ImageFormat_Dds && option != NULL &&
	    option->scale.w > 0 && option->scale.h > 0 && !option->keep_aspect) {
		rotate_image = image->ConvertMode(ImageMode_RGBA8888,
						  image::ImageConvertModeType_Default);
		if (rotate_image.get() != NULL)
			flip_image = rotate_image->Resize(option->scale, option->resize_type);
	} else {
		if (rotate_type != 0)
			rotate_image = image->Rotate((image::ImageRotateType)rotate_type);
		if (flip_type != 0) {
			if (rotate_image.get() != NULL)
				flip_image = rotate_image->Flip((image::ImageFlipType)flip_type);
			else
				flip_image = image->Flip((image::ImageFlipType)flip_type);
		}
		if (image_order != ImageOrder_NONE) {
			if (flip_image.get() != NULL)
				convert_image = flip_image->ConvertMode(
					(ImageMode)image_order,
					(image::ImageConvertModeType)image_order_param);
			else if (rotate_image.get() != NULL)
				convert_image = rotate_image->ConvertMode(
					(ImageMode)image_order,
					(image::ImageConvertModeType)image_order_param);
			else
				convert_image = image->ConvertMode(
					(ImageMode)image_order,
					(image::ImageConvertModeType)image_order_param);
		}
		if (extlines != 0) {
			/* the flipped image when there is a converted one, as Sony's */
			common::SharedPtr<image::Image> *src;

			if (convert_image.get() != NULL)
				src = &flip_image;
			else if (rotate_image.get() != NULL)
				src = &rotate_image;
			else
				src = &image;
			edge_image = (*src)->CopyWithEdge(extlines);
		}
	}

	common::SharedPtr<image::Image> decode_image;

	if (edge_image.get() != NULL)
		decode_image = edge_image;
	else if (convert_image.get() != NULL)
		decode_image = convert_image;
	else if (flip_image.get() != NULL)
		decode_image = flip_image;
	else if (rotate_image.get() != NULL)
		decode_image = rotate_image;
	else
		decode_image = image;

	if (option != NULL && option->check_image != NULL) {
		int32_t ret = option->check_image(decode_image, option->check_image_userdata);

		if (ret < 0)
			return ret;
	}

	image::ImageExtent extent = decode_image->GetExtent();
	int32_t num_level = 1;

	if (decode_image->GetArraySize().x > 1)
		num_level = decode_image->GetArraySize().x;

	int32_t level = num_level == 1 ? -1 : 0;

	if (option != NULL && option->alloc_surface != NULL) {
		surf = option->alloc_surface(pool, decode_image, decode_image->GetArraySize().y,
					     num_level, option->alloc_surface_userdata);
	} else {
		ImageOrder order;

		if (option != NULL && option->surface_order == ImageOrder_Tiled) {
			order = ImageOrder_Tiled;
			if (decode_image->GetOrder() != ImageOrder_Linear)
				order = decode_image->GetOrder();
		} else {
			order = decode_image->GetOrder();
		}
		surf = intrusive_ptr<Surface>(new Surface(pool, decode_image->GetExtent().w,
							  decode_image->GetExtent().h,
							  decode_image->GetMode(), order,
							  decode_image->GetArraySize().y,
							  option->align_bytes, 0, num_level,
							  option->reserved5));
	}
	if (surf.get() == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	if (surf->GetMemory() == NULL) {
		surf = intrusive_ptr<Surface>();
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	}

	if (pool->m_type != SurfacePool::MemoryType_SharedClient) {
		image::ImagePoint pos(0, 0);

		for (; pos.x < num_level; pos.x++, level++) {
			decode_image->SetArrayPos(pos);

			image::ImageBuffer buf = decode_image->ToBuffer(true);

			if (buf.buff == NULL) {
				surf = intrusive_ptr<Surface>();
				return image::Image::GetErrorType();
			}

			int32_t ret = surf->Copy(0, buf.buff, decode_image->GetOrder(),
						 decode_image->GetMemPitch(pos.x), level);

			if (ret != 0) {
				surf.clear();
				return ret;
			}
			surf->FillPadding(0, NULL, level);
		}
	}

	ImageMode mode = decode_image->GetMode();

	if (mode == ImageMode_index4 || mode == ImageMode_index8) {
		intrusive_ptr<SurfaceCLUT> clut;
		SurfacePool *clut_pool = pool;

		if (option != NULL && option->clut_pool != NULL)
			clut_pool = option->clut_pool;

		int16_t num_entry = 256;

		if (decode_image->GetMode() == ImageMode_index4)
			num_entry = 16;
		if (decode_image->GetCLUTMode() == ImageMode_RGBA8888) {
			clut = intrusive_ptr<SurfaceCLUT>(
				new SurfaceCLUT(clut_pool, decode_image->GetCLUTMode(), num_entry));
			if (clut.get() == NULL) {
				surf.clear();
				return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
			}
			if (clut->GetCLUT() == NULL) {
				surf.clear();
				return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
			}
			if (pool->m_type != SurfacePool::MemoryType_SharedClient) {
				image::ImageBuffer buf = decode_image->ToCLUTBuffer(true);

				if (buf.buff != NULL)
					ScePafGraphics_F4986A3A(clut.get(), buf.buff);
			}
		}
		surf->AttachCLUT(intrusive_ptr<SurfaceCLUT>(clut));
	}
	return 0;
}

int32_t Surface::DecodeImage(SurfacePool *pool, LoadOption *option, intrusive_ptr<Surface> &surf,
			     void *buf, size_t size)
{
	common::SharedPtr<image::Image> image;

	image = image::Image::Open(
		buf, size, option != NULL ? option->decode_heap : NULL, image::ImageFormat_None);

	if (image.get() == NULL)
		return image::Image::GetErrorType();
	return DecodeImage(pool, option, surf, image);
}

intrusive_ptr<Surface> Surface::Load(SurfacePool *pool, common::SharedPtr<File> fp,
				    LoadOption *option)
{
	if (fp.get() == NULL) {
		if (option != NULL) {
			option->error = SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;
			option->data_buff = NULL;
			option->size = 0;
		}
		return intrusive_ptr<Surface>();
	}

	LoadOption default_option;

	if (option == NULL)
		option = &default_option;

	bool direct_load = option->direct_load;
	size_t buf_size = 0;
	size_t read_size = option->read_size;
	size_t read_offset = option->read_offset;

	if (!direct_load) {
		int64_t file_size = fp->GetFileSize();

		if (file_size < 0) {
			if ((int32_t)file_size != SCE_PAF_ERROR_FILE_UNKNOWN_SIZE) {
				option->error = (int32_t)file_size;
				return intrusive_ptr<Surface>();
			}
			direct_load = true;
			buf_size = 0;
		} else {
			buf_size = (size_t)file_size;
			if (buf_size != file_size) {
				direct_load = true;
				buf_size = 0;
			} else if (buf_size != 0) {
				if (read_offset > buf_size)
					read_offset = buf_size;
				if (buf_size < read_offset + read_size)
					read_size = buf_size - read_offset;
				if (read_size == 0)
					read_size = buf_size;
				if (option->max_size_kb != 0 &&
				    option->max_size_kb < (read_size + 1023) >> 10) {
					option->error = SCE_PAF_ERROR_GRAPHICS_TOO_LARGE;
					return intrusive_ptr<Surface>();
				}
			}
		}
	}

	intrusive_ptr<Surface> surf;
	bool load = true;
	void *buf = NULL;

	if (!direct_load && buf_size != 0) {
		if (option->data_load_heap != NULL)
			buf = option->data_load_heap->sce_paf_malloc(buf_size);
		else if (option->alloc_heap != NULL)
			buf = option->alloc_heap(buf_size, &load, option->alloc_heap_userdata);
		else
			buf = sce_paf_malloc(buf_size);
	}
	if (buf != NULL) {
		int32_t ret;

		if (!load || (ret = fp->Read(buf, buf_size)) >= 0) {
			ret = DecodeImage(pool, option, surf, (uint8_t *)buf + read_offset, read_size);
			if (ret >= 0)
				ret = 0;
			else
				surf.clear();
		}
		if (option->data_load_heap != NULL) {
			option->data_load_heap->sce_paf_free(buf);
		} else if (option->alloc_heap != NULL || option->free_heap != NULL) {
			if (option->free_heap != NULL)
				option->free_heap(buf, option->free_heap_userdata);
		} else {
			sce_paf_free(buf);
		}
		option->error = ret;
		option->data_buff = buf;
		option->size = buf_size;
		return surf;
	}

	common::SharedPtr<image::Image> image;

	image = image::Image::Open(
		fp, option->decode_heap, image::ImageFormat_None, read_offset, read_size);

	if (image.get() == NULL) {
		option->error = image::Image::GetErrorType();
		return intrusive_ptr<Surface>();
	}

	int32_t ret = DecodeImage(pool, option, surf, image);

	if (ret != 0) {
		option->error = ret;
		return intrusive_ptr<Surface>();
	}
	return surf;
}

intrusive_ptr<Surface> Surface::Load(SurfacePool *pool, void *buf, int64_t size, LoadOption *option)
{
	intrusive_ptr<Surface> surf;
	int32_t ret = DecodeImage(pool, option, surf, buf, size);

	if (ret < 0)
		surf.clear();
	if (option != NULL)
		option->error = ret;
	return surf;
}

}
}

/* Load for a DXT1 surface */
intrusive_ptr<Surface> ScePafGraphics_AD93FF1B(SurfacePool *pool, common::SharedPtr<File> fp,
					       Surface::LoadOption *option)
{
	if (fp.get() == NULL) {
		if (option != NULL) {
			option->error = SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;
			option->data_buff = NULL;
			option->size = 0;
		}
		return intrusive_ptr<Surface>();
	}

	Surface::LoadOption default_option;

	if (option == NULL)
		option = &default_option;

	bool direct_load = option->direct_load;
	size_t buf_size = 0;
	size_t read_size = option->read_size;
	size_t read_offset = option->read_offset;

	if (!direct_load) {
		int64_t file_size = fp->GetFileSize();

		if (file_size < 0) {
			if ((int32_t)file_size != SCE_PAF_ERROR_FILE_UNKNOWN_SIZE) {
				option->error = (int32_t)file_size;
				return intrusive_ptr<Surface>();
			}
			direct_load = true;
			buf_size = 0;
		} else {
			buf_size = (size_t)file_size;
			if (buf_size != file_size) {
				direct_load = true;
				buf_size = 0;
			} else if (buf_size != 0) {
				if (read_offset > buf_size)
					read_offset = buf_size;
				if (buf_size < read_offset + read_size)
					read_size = buf_size - read_offset;
				if (read_size == 0)
					read_size = buf_size;
				if (option->max_size_kb != 0 &&
				    option->max_size_kb < (read_size + 1023) >> 10) {
					option->error = SCE_PAF_ERROR_GRAPHICS_TOO_LARGE;
					return intrusive_ptr<Surface>();
				}
			}
		}
	}

	intrusive_ptr<Surface> surf;
	bool load = true;
	void *buf = NULL;

	if (!direct_load && buf_size != 0) {
		if (option->data_load_heap != NULL)
			buf = option->data_load_heap->sce_paf_malloc(buf_size);
		else if (option->alloc_heap != NULL)
			buf = option->alloc_heap(buf_size, &load, option->alloc_heap_userdata);
		else
			buf = sce_paf_malloc(buf_size);
	}
	if (buf != NULL) {
		int32_t ret;

		if (!load || (ret = fp->Read(buf, buf_size)) >= 0) {
			if (option->image_order != (ImageOrder)ImageMode_DXT1)
				option->image_order = (ImageOrder)ImageMode_DXT1;
			ret = Surface::DecodeImage(pool, option, surf, (uint8_t *)buf + read_offset, read_size);
			if (ret >= 0)
				ret = 0;
			else
				surf.clear();
		}
		if (option->data_load_heap != NULL) {
			option->data_load_heap->sce_paf_free(buf);
		} else if (option->alloc_heap != NULL || option->free_heap != NULL) {
			if (option->free_heap != NULL)
				option->free_heap(buf, option->free_heap_userdata);
		} else {
			sce_paf_free(buf);
		}
		option->data_buff = buf;
		option->size = buf_size;
		option->error = ret;
		return surf;
	}

	common::SharedPtr<image::Image> image;

	image = image::Image::Open(
		fp, option->decode_heap, image::ImageFormat_None, read_offset, read_size);

	if (image.get() == NULL) {
		option->error = image::Image::GetErrorType();
		return intrusive_ptr<Surface>();
	}
		if (option->image_order != (ImageOrder)ImageMode_DXT1)
			option->image_order = (ImageOrder)ImageMode_DXT1;

	int32_t ret = Surface::DecodeImage(pool, option, surf, image);

	if (ret != 0) {
		option->error = ret;
		return intrusive_ptr<Surface>();
	}
	return surf;
}

/* Load from memory for a DXT1 surface */
intrusive_ptr<Surface> ScePafGraphics_476AB004(SurfacePool *pool, void *buf, int64_t size,
					       Surface::LoadOption *option)
{
	intrusive_ptr<Surface> surf;
	Surface::LoadOption default_option;

	if (option == NULL)
		option = &default_option;
	if (option->image_order != (ImageOrder)ImageMode_DXT1)
		option->image_order = (ImageOrder)ImageMode_DXT1;

	int32_t ret = Surface::DecodeImage(pool, option, surf, buf, size);

	if (ret < 0)
		surf.clear();
	option->error = ret;
	return surf;
}

namespace paf {
namespace graph {

int32_t Surface::Initialize(int32_t width, int32_t height, ImageMode format, ImageOrder order,
			    SurfaceType surf_type, int32_t num_page, void *bp, int32_t line_bytes,
			    int32_t num_level)
{
	m_list_cs.Lock();
	InitMember();
	m_width = width;
	m_height = height;
	m_order = order;
	m_surf_type = surf_type;
	m_mem_class = bp != NULL ? Unallocated : Fixed;
	m_num_level = num_level;
	m_num_page = num_page;
	m_pool = NULL;
	sce_paf_memset(&m_mem, 0, sizeof(m_mem));
	InitObj(line_bytes);
	m_mem.bp = bp;
	SetMemAttr();
	if (m_tex_pages != NULL && m_mem.bp != NULL) {
		for (int32_t i = 0; i < m_num_page; i++)
			GenTexture(&m_tex_pages[i], (uint8_t *)m_mem.bp + m_page_size * i);
	}
	m_list_cs.Unlock();
	return 0;
}

Surface::Surface(int32_t width, int32_t height, ImageMode format, ImageOrder order,
		 int32_t num_page, void *bp, int32_t line_bytes, int32_t num_level, bool reserved)
	: SurfaceBase(format, reserved)
{
	Initialize(width, height, format, order, TypeTexture, num_page, bp, line_bytes, num_level);
}

Surface::Surface(SceGxmTexture *texture, int32_t width, int32_t height)
	: SurfaceBase(ImageMode_ABGR8888, false)
{
	InitMember();
	m_width = width;
	m_height = height;
	m_tex_width = width;
	m_tex_height = height;
	m_order = ImageOrder_Linear;
	m_surf_type = TypeTexture;
	m_mem_class = Unallocated;
	m_num_level = 1;
	m_num_page = 1;
	m_pool = NULL;
	sce_paf_memset(&m_mem, 0, sizeof(m_mem));
	InitObj(0);
	m_tex_pages = new Texture[1];
	::operator delete(m_tex_pages->texture);
	m_tex_pages->texture = texture;
	m_tex_pages->m_owned = false;
}

int32_t Surface::Initialize(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			    ImageOrder order, SurfaceType surf_type, int32_t num_page,
			    int32_t num_level)
{
	m_list_cs.Lock();
	InitMember();
	m_width = width;
	m_height = height;
	m_order = order;
	m_surf_type = surf_type;
	m_mem_class = Unallocated;
	m_num_level = num_level;
	m_num_page = num_page;
	m_pool = pool;
	sce_paf_memset(&m_mem, 0, sizeof(m_mem));
	InitObj(0);
	m_mem.bp = NULL;
	if (pool != NULL) {
		if ((num_page == 0 ||
		     (m_mem.bp = pool->Allocate(1 << m_mem.align, m_page_size * num_page)) == NULL) &&
		    ScePafGraphics_404AC3D7(m_pool) != 0) {
			sce_paf_printf_dummy();
			sce_paf_printf_dummy();
		}
	}
	SetMemAttr();
	if (m_tex_pages != NULL && m_mem.bp != NULL) {
		for (int32_t i = 0; i < m_num_page; i++)
			GenTexture(&m_tex_pages[i], (uint8_t *)m_mem.bp + m_page_size * i);
	}
	m_list_cs.Unlock();
	return 0;
}

Surface::Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		 ImageOrder order, int32_t num_page, int32_t num_level, bool reserved)
	: SurfaceBase(format, reserved)
{
	Initialize(pool, width, height, format, order, TypeTexture, num_page, num_level);
}

}
}

namespace paf {
namespace graph {

void Surface::DetachCLUT()
{
	m_clut.clear();
}

int32_t Surface::Texturize(uint32_t map_type, float offset_u, float offset_v, float scale_u,
			   float scale_v)
{
	if (m_mem.bp == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	return ActivateTexture(map_type, offset_u, offset_v, scale_u, scale_v);
}

int32_t Surface::ActivatePage(int32_t page)
{
	if (page < 0 || page >= m_num_page)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	m_page_counter = page;
	if (m_surf_type == TypeTexture && m_texturized)
		Texturize();
	return 0;
}

}
}

int32_t ScePafGraphics_B000FA14(Surface *surface)
{
	int32_t page = surface->GetPageCounter() - 1;

	if (page < 0)
		return surface->GetNumPage() - 1;
	return page;
}

namespace paf {
namespace graph {

void Surface::Clear(const math::v4 &color, uint32_t clear_mask)
{
	if (m_cleared)
		return;

	arl::ArlInstance *gl = arl::Gl();

	if ((clear_mask & arl::COLOR_BUFFER_BIT) != 0)
		gl->ColorMask(true, true, true, true);
	if ((clear_mask & arl::DEPTH_BUFFER_BIT) != 0)
		gl->DepthMask(true, arl::FRONT_AND_BACK);
	gl->ClearColor(color);
	gl->Clear(clear_mask);
	m_cleared = 1;
}

void Surface::Swap()
{
	m_page_counter++;
	if (m_page_counter >= m_num_page)
		m_page_counter = 0;
	m_cleared = 0;
}

int32_t Surface::BeginScene()
{
	if (m_surf_type == TypeTexture)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	arl::Gl()->SetSceneData(intrusive_ptr<Surface>(this));
	ScePafGraphics_199BD5F2(arl::Gl(), 0);
	return 0;
}

int32_t Surface::EndScene()
{
	if (m_surf_type == TypeTexture)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	ScePafGraphics_29F5AA8B(arl::Gl(), NULL, false);
	return 0;
}

uint8_t *Surface::GetPagePtr(int32_t page) const
{
	if (m_mem.bp == NULL)
		return NULL;
	return (uint8_t *)m_mem.bp + m_page_size * page;
}

void *Surface::Lock(int32_t page)
{
	m_lock_mutex.Lock();
	if (m_locked) {
		m_lock_mutex.Unlock();
		return NULL;
	}
	m_locked = 1;

	void *ptr = GetPagePtr(page);

	m_lock_mutex.Unlock();
	return ptr;
}

void Surface::Unlock()
{
	m_lock_mutex.Lock();
	if (m_locked)
		m_locked = 0;
	m_lock_mutex.Unlock();
}

int32_t Surface::Copy(int32_t page, const intrusive_ptr<Surface> &src, int32_t src_page)
{
	if (src_page < 0 || src.get() == NULL || src_page >= src->m_num_page)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	if (src->m_format != m_format || src->m_width != m_width || src->m_height != m_height)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;

	uint8_t *src_ptr = src->GetPagePtr(src_page);

	if (src_ptr == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;

	ImageOrder src_order = src->m_order;
	int32_t line_bytes = src->m_stride;

	if (page < 0 || page >= m_num_page || m_num_level < 0)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_PARAMETER;
	if (m_format == ImageMode_YUV420P2 || m_format == ImageMode_YUV420P3)
		return Copy_YUV420PX(page, src_ptr, src_order, line_bytes, -1);
	if (m_format >= ImageMode_DXT1 && m_format <= ImageMode_DXT5)
		return ScePafGraphics_D811125C(this, page, src_ptr, src_order, line_bytes, -1);
	return Copy_(page, src_ptr, src_order, line_bytes, -1);
}

Surface::Texture::~Texture()
{
	if (m_owned)
		::operator delete(texture);
	texture = NULL;
}

/* a surface nothing sets any more: FreeMemory and the destructor forget it (name ours) */
static Surface *s_current_surface = NULL;

void Surface::FreeMemory()
{
	m_list_cs.Lock();
	if (m_pool != NULL && m_mem.bp != NULL) {
		m_pool->Free(m_mem.bp);
		m_mem.bp = NULL;
	}
	if (s_current_surface == this)
		s_current_surface = NULL;
	m_list_cs.Unlock();
}

int32_t Surface::Resize(int32_t width, int32_t height)
{
	m_list_cs.Lock();
	if (width == m_width && height == m_height) {
		m_list_cs.Unlock();
		return 0;
	}
	if (m_locked || (m_surf_type != TypeTexture && m_pool != NULL)) {
		m_list_cs.Unlock();
		return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
	}
	FreeMemory();
	Unlink();
	if (m_tex_pages != NULL) {
		DeleteTexture();
		delete[] m_tex_pages;
	}
	m_width = width;
	m_height = height;
	InitObj(0);
	if (m_pool != NULL) {
		m_mem.bp = m_pool->Allocate(1 << m_mem.align, m_page_size * m_num_page);
		if (m_mem.bp == NULL) {
			if (ScePafGraphics_404AC3D7(m_pool) != 0) {
				sce_paf_printf_dummy();
				sce_paf_printf_dummy();
			}
			m_list_cs.Unlock();
			return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
		}
	} else if (m_surf_type == TypeTexture) {
		m_mem.bp = NULL;
	}
	SetMemAttr();
	if (m_tex_pages != NULL && m_mem.bp != NULL) {
		for (int32_t i = 0; i < m_num_page; i++)
			GenTexture(&m_tex_pages[i], (uint8_t *)m_mem.bp + m_page_size * i);
	}
	m_list_cs.Unlock();
	return 0;
}

uint32_t Surface::GetLodBias()
{
	return m_texture_param.lod_bias;
}

int32_t Surface::SetLodBias(uint32_t lod_bias)
{
	if (lod_bias > 63)
		return -1;
	m_texture_param.lod_bias = lod_bias;
	return 0;
}

bool Surface::SetSymmetryU(bool symmetry)
{
	bool old = m_symmetry_u;

	m_symmetry_u = symmetry;
	return old;
}

bool Surface::SetSymmetryV(bool symmetry)
{
	bool old = m_symmetry_v;

	m_symmetry_v = symmetry;
	return old;
}

int32_t Surface::IsAllocatableSurfacePool(SurfacePool *pool, uint32_t size, uint32_t align)
{
	if (pool == NULL || pool->m_size < size || align == 0)
		return SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;

	void *bp = pool->Allocate(align, size);

	if (bp == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_SPACE;
	pool->Free(bp);
	return 0;
}

int32_t Surface::GetPageIndex(FrameBufferType type, int32_t *page)
{
	if (page == NULL)
		return SCE_PAF_ERROR_GRAPHICS_NO_ENTRY;
	if (type == FrameBufferType_Front) {
		int32_t index = m_page_counter - 1;

		if (index < 0)
			index = m_num_page - 1;
		*page = index;
	} else if (type == FrameBufferType_Back) {
		*page = m_page_counter;
	}
	return 0;
}

int32_t Surface::GenTexture()
{
	return SCE_PAF_ERROR_GRAPHICS_INVALID_SURFACE;
}

void Surface::operator delete(void *ptr)
{
	sce_paf_free(ptr);
}

void Surface::DeactivateSurfacePool(SurfacePool *pool)
{
	thread::RMutexLock lock(&m_list_cs);

	for (SurfaceListNode *node = m_surface_list.m_next; node != &m_surface_list;
	     node = node->m_next) {
		if (node == &m_active_mark)
			continue;

		Surface *surface = static_cast<Surface *>(node);

		if (surface->m_pool == pool) {
			surface->m_mem.bp = NULL;
			surface->m_pool = NULL;
		}
	}
}

int32_t Surface::FloorLog2(int32_t value)
{
	if (value <= 0)
		return 1;
	if (((value - 1) & value) == 0)
		return value;

	int32_t step8 = ((value - 0x100) >> 16) & 8;
	int32_t current = value << step8;
	int32_t step4 = ((current - 0x1000) >> 16) & 4;

	current <<= step4;

	int32_t step2 = ((current - 0x4000) >> 16) & 2;

	current <<= step2;

	int32_t high = current >> 14;

	return 1 << ((high & ~(high >> 1)) - (step4 + step8 + step2) + 14);
}

void Surface::SetDivideMode(uint32_t mode)
{
	if (m_divide_mode == mode)
		return;
	m_divide_mode = mode;
	switch ((uint16_t)mode) {
	case 0:
		m_div_ratio_v = 0;
		m_div_ratio_h = 0;
		break;
	case 1:
		m_div_ratio_h = 0;
		m_div_ratio_v = 1;
		break;
	case 2:
		m_div_ratio_h = 1;
		m_div_ratio_v = 0;
		break;
	case 3:
		m_div_ratio_v = 1;
		m_div_ratio_h = 1;
		break;
	case 4:
		m_div_ratio_v = 1;
		m_div_ratio_h = 1;
		break;
	case 5:
		m_div_ratio_v = 1;
		m_div_ratio_h = 1;
		break;
	case 6:
	case 7:
	case 8:
	case 9:
		m_div_ratio_v = 1;
		m_div_ratio_h = 1;
		break;
	}
}

}
}

using namespace paf::math;
using sce::Vectormath::Simd::Aos::Vector2;

/* which preset a division ratio is: DivideRatio_Other when it is none of them */
static inline __attribute__((always_inline)) uint8_t DivideRatioType(v2 ratio)
{
	if (ratio.extract_x() != ratio.extract_y())
		return Surface::DivideRatio_Other;
	if (ratio.extract_x() == 0.0f)
		return Surface::DivideRatio_None;
	if (ratio.extract_x() == 1.0f / 3)
		return Surface::DivideRatio_Third;
	if (ratio.extract_x() == 0.49f)
		return Surface::DivideRatio_049;
	if (ratio.extract_x() == 0.5f)
		return Surface::DivideRatio_Half;
	return Surface::DivideRatio_Other;
}

/* the value of a preset */
static inline __attribute__((always_inline)) v2 DivideRatioValue(uint8_t type)
{
	switch (type) {
	case Surface::DivideRatio_Third:
		return v2(1.0f / 3, 1.0f / 3);
	case Surface::DivideRatio_049:
		return v2(0.49f, 0.49f);
	case Surface::DivideRatio_Half:
		return v2(0.5f, 0.5f);
	default:
		return v2(0.0f, 0.0f);
	}
}

int32_t ScePafGraphics_36960B24(Surface *surface, v2 ratio_h, v2 ratio_v)
{
	v2 h(maxPerElem(Vector2(0.0f), minPerElem(Vector2(1.0f), ratio_h.getImpl())));
	v2 v(maxPerElem(Vector2(0.0f), minPerElem(Vector2(1.0f), ratio_v.getImpl())));
	float *ratio = surface->m_div_ratio;
	uint8_t type_h = DivideRatioType(h);
	uint8_t type_v;

	surface->m_div_ratio_h = type_h;
	type_v = DivideRatioType(v);
	surface->m_div_ratio_v = type_v;
	if (ratio == NULL) {
		if (type_h != Surface::DivideRatio_Other && type_v != Surface::DivideRatio_Other)
			return 0;
		surface->m_div_ratio = new float[6];
		surface->m_div_ratio[5] = 0.0f;
		surface->m_div_ratio[4] = 0.0f;
		ratio = surface->m_div_ratio;
	}
	ratio[0] = h.extract_x();
	surface->m_div_ratio[1] = h.extract_y();
	surface->m_div_ratio[2] = v.extract_x();
	surface->m_div_ratio[3] = v.extract_y();
	return 0;
}

void ScePafGraphics_AFAE7807(Surface *surface, v2 *ratio_h, v2 *ratio_v)
{
	uint8_t type_h = surface->m_div_ratio_h;
	float *ratio = surface->m_div_ratio;

	*ratio_h = DivideRatioValue(type_h);

	uint8_t type_v = surface->m_div_ratio_v;

	*ratio_v = DivideRatioValue(type_v);
	if (ratio != NULL) {
		if (type_h == Surface::DivideRatio_Other)
			*ratio_h = *(v2 *)&ratio[0];
		if (type_v == Surface::DivideRatio_Other)
			*ratio_v = *(v2 *)&ratio[2];
	}
}

namespace paf {
namespace graph {

int32_t Surface::SetDivideStretchRatio(v2 ratio)
{
	float *div_ratio = m_div_ratio;

	if (div_ratio == NULL) {
		if (ratio == v2(0.0f, 0.0f))
			return 0;
		div_ratio = m_div_ratio = new float[6];
		div_ratio[2] = 1.0f / 3;
		div_ratio[3] = 1.0f / 3;
		div_ratio[0] = 1.0f / 3;
		div_ratio[1] = 1.0f / 3;
	}
	div_ratio[4] = ratio.extract_x();
	div_ratio[5] = ratio.extract_y();
	return 0;
}

void Surface::GetDivideStretchRatio(v2 &ratio) const
{
	ratio = m_div_ratio != NULL ? *(const v2 *)&m_div_ratio[4] : v2(0.0f, 0.0f);
}

Surface::~Surface()
{
	m_list_cs.Lock();
	FreeMemory();
	Unlink();
	if (m_tex_pages != NULL) {
		DeleteTexture();
		delete[] m_tex_pages;
		m_tex_pages = NULL;
	}
	if (m_div_ratio != NULL) {
		delete[] m_div_ratio;
		m_div_ratio = NULL;
	}
	m_list_cs.Unlock();
}

Surface::Surface(int32_t width, int32_t height, ImageMode format, ImageOrder order,
		 SurfaceType surf_type, int32_t num_page, void *bp, int32_t line_bytes,
		 int32_t num_level, bool reserved)
	: SurfaceBase(format, reserved)
{
	Initialize(width, height, format, order, surf_type, num_page, bp, line_bytes, num_level);
}

Surface::Surface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
		 ImageOrder order, SurfaceType surf_type, int32_t num_page, int32_t num_level,
		 bool reserved)
	: SurfaceBase(format, reserved)
{
	Initialize(pool, width, height, format, order, surf_type, num_page, num_level);
}

void Surface::DelayedRelease()
{
}

void Surface::Reorder()
{
	thread::RMutexLock lock(&m_list_cs);

	for (SurfaceListNode *node = m_surface_list.m_next; node != &m_surface_list;
	     node = node->m_next) {
		if (node == &m_active_mark || static_cast<Surface &>(*node).m_score <= m_score) {
			Unlink();
			InsertAfter(node->m_prev);
			return;
		}
	}
}

SurfaceCLUT::SurfaceCLUT(ImageMode format, void *clut, int32_t num_entry)
	: SurfaceBase(format, false)
{
	m_num_entry = num_entry;
	m_ref_count = 0;
	m_clut_size = GetCLUTSize();
	m_pool = NULL;
	m_clut = clut;
	m_clut_mutex.Lock();
	m_prev = &m_clut_list;
	m_next = m_clut_list.m_next;
	m_clut_list.m_next->m_prev = this;
	m_clut_list.m_next = this;
	m_clut_mutex.Unlock();
}

void SurfaceCLUT::operator delete(void *ptr)
{
	sce_paf_free(ptr);
}

void SurfaceCLUT::DeactivateSurfacePool(SurfacePool *pool)
{
	m_clut_mutex.Lock();
	for (SurfaceListNode *node = m_clut_list.m_next; node != &m_clut_list;
	     node = node->m_next) {
		SurfaceCLUT *clut = static_cast<SurfaceCLUT *>(node);

		if (clut->m_pool == pool) {
			clut->m_pool = NULL;
			clut->m_clut = NULL;
		}
	}
	m_clut_mutex.Unlock();
}

SurfaceCLUT::~SurfaceCLUT()
{
	if (m_pool != NULL && m_clut != NULL)
		m_pool->Free(m_clut);
	m_clut_mutex.Lock();
	Unlink();
	m_clut_mutex.Unlock();
}

/* the value both of these start with (name ours) */
static const float s_unused_init = 32.0f;

/* two values set when the module starts and never read (names ours) */
static int32_t s_unused0 = (int32_t)s_unused_init;
static int32_t s_unused1 = (int32_t)s_unused_init;

SurfaceListNode Surface::m_surface_list;
SurfaceListNode Surface::m_active_mark;
int32_t Surface::m_initialized;
thread::RMutex Surface::m_list_cs("ScePafSurfaceList", 0);
thread::Mutex Surface::m_lock_mutex("ScePafSurface", 0);
SurfaceListNode SurfaceCLUT::m_clut_list;
thread::Mutex SurfaceCLUT::m_clut_mutex("ScePafSurfaceCLUT", 0);

}
}
