/*
 * paf::graph::FontRasterizerImage: the emoji font, imagefont.bin (the file's name ours). A
 * glyph is a zlib-compressed image: a header, its frames, its colour table and, for each
 * frame, the compressed 8-bit colour indices; Rasterize draws every frame into a page of an
 * indexed text surface, four rows and a column in from its corner.
 */
#include <stddef.h>
#include <stdint.h>
#include <compress/compress.h>
#include <file/localfile.h>
#include <file/memfile.h>
#include <graphics/surface/surface.h>
#include <graphics/text/font_rasterizer.h>
#include <graphics/text/text_surface_cache.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

/* a glyph image's first bytes (names ours) */
class GlyphImageHeader {
public:
	int16_t num_colors;
	/* a colour table entry's bytes: 3 (RGB) or 4 (RGBA) */
	int8_t color_bytes;
	int8_t num_frames;
	/* the animation's length, in hundredths of a second */
	int16_t duration;
};

/* a frame of a glyph image, after the header: 12 bytes, not aligned (names ours) */
class GlyphFrame {
public:
	/* the compressed indices: where they are and their size */
	int32_t offset;
	int16_t size;
	/* how long the frame shows, in hundredths of a second */
	int16_t delay;
	/* the colour index that is transparent, when transparent is set */
	uint8_t transparent;
	uint8_t transparent_index;
	int16_t reserved;
};

/* size bytes read from offset of the file (name ours) */
static inline __attribute__((always_inline)) int32_t ReadAt(common::SharedPtr<File> &file,
							   int64_t offset, void *buf,
							   uint32_t size)
{
	int32_t ret = 0;
	int64_t pos = file->Seek(offset, SCE_SEEK_SET);

	if (pos < 0 || pos > 0xffffffffLL)
		return SCE_PAF_ERROR_TEXT_ABORTED;

	int32_t read = file->Read(buf, size);

	if (read < 0)
		ret = read;
	return ret;
}

/* size bytes of the file at offset inflated into dst (name ours) */
static inline __attribute__((always_inline)) int32_t ReadCompressed(common::SharedPtr<File> &file,
								     int32_t offset, int32_t size,
								     void *dst, int32_t dst_size)
{
	int32_t ret;
	void *src = new uint8_t[size];

	if (src == NULL)
		return SCE_PAF_ERROR_TEXT_NO_MEMORY;
	ret = ReadAt(file, offset, src, size);
	if (ret == 0) {
		int32_t inflated = compress::Decompress(dst, dst_size, src, size,
						      compress::DecompressType_Zlib);

		if (inflated < 0)
			ret = inflated;
	}
	delete[] (uint8_t *)src;
	return ret;
}

FontRasterizerImage::FontRasterizerImage(const string &path, bool own_data)
	: m_path(path), m_own_data(own_data), m_data(NULL), m_size(0), m_glyphs(NULL)
{
}

FontRasterizerImage::FontRasterizerImage(void *addr, uint32_t size)
	: m_path(""), m_own_data(false), m_data(addr), m_size(size), m_glyphs(NULL)
{
}

int32_t FontRasterizerImage::Close()
{
	if (m_file.get() != NULL) {
		m_file->Close();
		m_file.reset();
	}
	if (m_own_data && m_data != NULL) {
		delete[] (uint8_t *)m_data;
		m_data = NULL;
	}
	return 0;
}

FontRasterizerImage::~FontRasterizerImage()
{
	m_glyph_map.clear();
	if (m_glyphs != NULL) {
		delete[] m_glyphs;
		m_glyphs = NULL;
	}
	Close();
}

int32_t FontRasterizerImage::Update()
{
	return SCE_PAF_ERROR_TEXT_NOT_SUPPORTED;
}

int32_t FontRasterizerImage::SetStyle(const common::SharedPtr<TextStyle> &style)
{
	return SCE_PAF_ERROR_TEXT_NOT_SUPPORTED;
}

int32_t FontRasterizerImage::ResetStyle()
{
	return SCE_PAF_ERROR_TEXT_NOT_SUPPORTED;
}

int32_t FontRasterizerImage::GetFontMetrics(FontMetrics &metrics)
{
	return SCE_PAF_ERROR_TEXT_NOT_SUPPORTED;
}

int32_t FontRasterizerImage::Open()
{
	int32_t ret = 0;

	if (m_file.get() == NULL) {
		int32_t error = 0;

		if (m_path != "") {
			m_file = LocalFile::Open(m_path.c_str(), SCE_O_RDONLY, 0, &error);
			if (error == 0 && m_own_data) {
				int32_t read;
				int64_t size = m_file->GetFileSize();

				if (size <= 0 || size > 0xffffffffLL) {
					read = SCE_PAF_ERROR_TEXT_ABORTED;
				} else {
					m_size = (uint32_t)size;
					m_data = new uint8_t[m_size];
					read = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					if (m_data != NULL)
						read = ReadAt(m_file, 0, m_data, m_size);
				}
				error = read;
				m_file->Close();
				m_file.reset();
			}
		}
		if (error == 0 && m_data != NULL)
			m_file = MemFile::Open(m_data, m_size, &error);
		ret = error;
		if (error < 0) {
			Close();
			ret = error;
		}
	}
	return ret;
}

int32_t FontRasterizerImage::LoadIndex()
{
	int32_t ret = 0;

	if (m_glyphs != NULL)
		return ret;
	ret = Open();
	if (ret != 0)
		return ret;

	Header header;

	ret = ReadAt(m_file, 0, &header, sizeof(header));
	if (ret == 0) {
		if (header.version != 0x100 || header.num_glyphs <= 0) {
			ret = SCE_PAF_ERROR_TEXT_ABORTED;
			goto failed;
		}
		m_glyphs = new Glyph[header.num_glyphs];
		if (m_glyphs == NULL) {
			ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
			goto failed;
		}
		ret = ReadAt(m_file, header.index_offset, m_glyphs,
			     header.num_glyphs * sizeof(Glyph));
		if (ret == 0) {
			for (int32_t i = 0; i < header.num_glyphs; i++)
				m_glyph_map[m_glyphs[i].code] = &m_glyphs[i];
		}
	}
	if (ret >= 0)
		return ret;
failed:
	m_glyph_map.clear();
	if (m_glyphs != NULL) {
		delete[] m_glyphs;
		m_glyphs = NULL;
	}
	return ret;
}

int32_t FontRasterizerImage::GetCharMetrics(wchar_t c, CharMetrics &metrics)
{
	int32_t ret = LoadIndex();

	if (ret == 0) {
		GlyphMap::iterator it = m_glyph_map.find(c);

		if (it == m_glyph_map.end())
			return SCE_PAF_ERROR_TEXT_ABORTED;

		Glyph *glyph = it->second;

		metrics.right = (float)(glyph->width + 4);
		metrics.left = -1.0f;
		metrics.advance = (float)glyph->width;
		metrics.font.height = (float)(glyph->height + 8);
		metrics.font.scale = 1.0f;
		metrics.font.ascender = (float)((int32_t)(glyph->height * 0.9f) + 4);
	}
	return ret;
}

/*
 * A frame drawn: its colour table, RGBA entries, into the surface's, and its indices into the
 * surface's page frame, the rest of the page the transparent index (name ours).
 */
static __attribute__((noinline)) int32_t DrawFrameRGBA(const uint8_t *pixels, const uint8_t *palette, uint32_t width,
			     int32_t height, int16_t num_colors, bool transparent,
			     uint8_t transparent_index, intrusive_ptr<Surface> &surface,
			     int32_t frame)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;
	SurfaceCLUT *clut = surface->GetCLUT().get();

	if (clut != NULL && clut->GetCLUT() != NULL) {
		uint32_t *entry = (uint32_t *)clut->GetCLUT();

		sce_paf_memset(entry, 0, clut->m_clut_size);
		for (int32_t i = 0; i < num_colors; i++, palette += 4) {
			uint32_t alpha = palette[3];

			if (transparent && i == transparent_index)
				alpha = 0;
			*entry++ = alpha << 24 | palette[2] << 16 | palette[0] | palette[1] << 8;
		}

		uint8_t *bits = (uint8_t *)surface->Lock(frame);

		if (bits != NULL) {
			sce_paf_memset(bits, transparent_index, surface->GetPageSize());
			for (int32_t y = 0; y < height; y++, pixels += width)
				sce_paf_memcpy(bits + (y + 4) * surface->GetStride() + 1, pixels,
					       width);
			surface->Unlock();
			return 0;
		}
	}
	return ret;
}

/* the same with RGB entries, opaque but for the transparent index (name ours) */
static __attribute__((noinline)) int32_t DrawFrameRGB(const uint8_t *pixels, const uint8_t *palette, uint32_t width,
			    int32_t height, int16_t num_colors, bool transparent,
			    uint8_t transparent_index, intrusive_ptr<Surface> &surface,
			    int32_t frame)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;
	SurfaceCLUT *clut = surface->GetCLUT().get();

	if (clut != NULL && clut->GetCLUT() != NULL) {
		uint32_t *entry = (uint32_t *)clut->GetCLUT();

		sce_paf_memset(entry, 0, clut->m_clut_size);
		for (int32_t i = 0; i < num_colors; i++, palette += 3) {
			uint32_t alpha = 0xff;

			if (transparent && i == transparent_index)
				alpha = 0;
			*entry++ = alpha << 24 | palette[2] << 16 | palette[0] | palette[1] << 8;
		}

		uint8_t *bits = (uint8_t *)surface->Lock(frame);

		if (bits != NULL) {
			sce_paf_memset(bits, transparent_index, surface->GetPageSize());
			for (int32_t y = 0; y < height; y++, pixels += width)
				sce_paf_memcpy(bits + (y + 4) * surface->GetStride() + 1, pixels,
					       width);
			surface->Unlock();
			return 0;
		}
	}
	return ret;
}

int32_t FontRasterizerImage::Rasterize(TextSurfaceCache *cache, int32_t width, int32_t height,
				       const wchar_t *str, uint32_t len, uint32_t hash,
				       intrusive_ptr<Surface> &surface)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	if (cache == NULL || str == NULL || len != 1)
		return ret;
	{
		common::SharedPtr<TextStyle> style;

		ret = cache->Find(width, height, style, str, len, hash, surface);
	}
	if (ret >= 0)
		return ret;
	ret = LoadIndex();
	if (ret != 0)
		return ret;

	GlyphMap::iterator it = m_glyph_map.find(*str);

	if (it == m_glyph_map.end())
		return SCE_PAF_ERROR_TEXT_ABORTED;

	Glyph *glyph = it->second;

	if (glyph->image_size <= 0)
		return SCE_PAF_ERROR_TEXT_ABORTED;

	uint8_t *image = new uint8_t[glyph->image_size];

	if (image == NULL)
		return SCE_PAF_ERROR_TEXT_NO_MEMORY;
	ret = ReadCompressed(m_file, glyph->offset, glyph->size, image, glyph->image_size);
	if (ret == 0) {
		GlyphImageHeader *header = (GlyphImageHeader *)image;
		int32_t pixels_size = glyph->width * glyph->height;
		const uint8_t *frames = image + 6;
		const uint8_t *palette = frames + header->num_frames * sizeof(GlyphFrame);
		uint8_t *pixels = new uint8_t[pixels_size];

		if (pixels == NULL) {
			ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
		} else {
			{
				common::SharedPtr<TextStyle> style;
				intrusive_ptr<SurfaceCLUT> clut;

				ret = cache->Allocate(width, height, ImageMode_index8,
						      header->num_frames, (glyph->flags & 1) != 0,
						      style, str, len, hash, clut, surface);
			}
			if (ret == 0) {
				int16_t total = 0;
				int16_t first = 0;

				for (int8_t i = 0; i < header->num_frames; i++) {
					GlyphFrame frame;

					sce_paf_memcpy(&frame, frames, sizeof(frame));
					frames += sizeof(frame);
					ret = ReadCompressed(m_file, frame.offset, frame.size, pixels,
							     pixels_size);
					if (ret != 0)
						continue;
					switch (header->color_bytes) {
					case 3:
						DrawFrameRGB(pixels, palette, glyph->width, glyph->height,
							     header->num_colors, frame.transparent,
							     frame.transparent_index, surface, i);
						break;
					case 4:
						DrawFrameRGBA(pixels, palette, glyph->width, glyph->height,
							      header->num_colors, frame.transparent,
							      frame.transparent_index, surface, i);
						break;
					}
					if (header->num_frames > 1) {
						static_cast<TextSurfaceAnim *>(surface.get())->SetFrameDelay(
							i, (float)(frame.delay * 10));
						total += frame.delay;
						if (i == 0)
							first = frame.delay;
					}
				}
				if (header->num_frames > 1 && header->duration > total)
					static_cast<TextSurfaceAnim *>(surface.get())->SetFrameDelay(
						0, (float)((header->duration - total + first) * 10));
			}
			delete[] pixels;
		}
	}
	delete[] image;
	return ret;
}

}
}
