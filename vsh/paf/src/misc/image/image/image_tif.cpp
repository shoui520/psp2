/*
 * paf::image::TifCodec: TIFF images through libtiff. Palette images decode to
 * 4- or 8-bit indices with an RGBA palette, every other photometric to RGBA
 * through libtiff's RGBA interface, by strip or by tile.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "image_codec.h"
#include "image_source.h"
#include "../../../paf_types.h"

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

static tsize_t ReadFileProc(thandle_t handle, tdata_t buf, tsize_t size);
static tsize_t ReadBufferProc(thandle_t handle, tdata_t buf, tsize_t size);
static tsize_t WriteProc(thandle_t handle, tdata_t buf, tsize_t size);
static toff_t SeekFileProc(thandle_t handle, toff_t offset, int whence);
static toff_t SeekBufferProc(thandle_t handle, toff_t offset, int whence);
static int CloseProc(thandle_t handle);
static toff_t SizeFileProc(thandle_t handle);
static toff_t SizeBufferProc(thandle_t handle);
static int MapProc(thandle_t handle, tdata_t *base, toff_t *size);
static void UnmapProc(thandle_t handle, tdata_t base, toff_t size);
static tdata_t MallocProc(void *allocator, tsize_t size);
static tdata_t ReallocProc(void *allocator, tdata_t ptr, tsize_t size);
static void FreeProc(void *allocator, tdata_t ptr);

bool TifAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool TifAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool TifAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool TifAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

TifAttribute::TifAttribute()
{
}

TifAttribute::~TifAttribute()
{
}

bool paf::image::IsTif(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 4)
		return false;

	const char little[4] = { 'I', 'I', '*', '\0' };
	const char big[4] = { 'M', 'M', '\0', '*' };
	const void *data = buf->GetAddr();
	if (sce_paf_memcmp(little, data, 4) == 0 || sce_paf_memcmp(big, data, 4) == 0)
		return true;
	return false;
}

/* The bytes of a row of width pixels as libtiff reads it, 0 when unknown. */
static inline tsize_t GetRowSize(TIFF *tif, uint32_t width)
{
	uint16_t samples = 0;
	uint16_t bits;
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits) || bits < 1 || bits > 16)
		return 0;
	if (samples == 0) {
		if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples))
			return 0;
		if (samples != 1 && samples != 3 && samples != 4)
			return 0;
	}
	return (bits * samples * width + 7) >> 3;
}

/*
 * Any photometric but a palette, by libtiff's RGBA interface: strip by strip
 * or tile by tile, each read bottom row first.
 */
bool TifCodec::DecodeRGBA(TIFF *tif, uint8_t *dst, const ImageAttribute *attr,
			  DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t width = attr->m_originalextent.w;
	uint32_t height = attr->m_originalextent.h;
	int32_t pitch = attr->GetMemPitch(0);
	int32_t skip = pitch - 4 * attr->m_extent.w;
	uint32_t rowsperstrip;
	uint32_t tilewidth;
	uint32_t tilelength;

	if (!TIFFIsTiled(tif)) {
		if (!TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip))
			return false;
		if (rowsperstrip > 0x3fff) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		if (height < rowsperstrip) {
			if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height))
				return false;
			rowsperstrip = height;
		}

		uint32_t npixels = rowsperstrip * width;
		uint32_t strips = (height + rowsperstrip - 1) / rowsperstrip;
		uint32_t lastrows = height % rowsperstrip;
		uint32_t *raster;
		if (m_allocator != NULL)
			raster = (uint32_t *)m_allocator->sce_paf_malloc(npixels * sizeof(uint32_t));
		else
			raster = (uint32_t *)sce_paf_malloc(npixels * sizeof(uint32_t));
		if (raster == NULL) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			return false;
		}

		uint32_t rows = rowsperstrip;
		for (uint32_t strip = 0; strip < strips; strip++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(raster);
				else
					sce_paf_free(raster);
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			if (!TIFFReadRGBAStrip(tif, rowsperstrip * strip, raster)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(raster);
				else
					sce_paf_free(raster);
				return false;
			}
			if (strip == strips - 1 && lastrows != 0)
				rows = lastrows;
			for (int32_t y = rows - 1; y != -1; y--) {
				const uint32_t *src = &raster[y * width];
				for (uint32_t x = 0; x < width; x++) {
					uint32_t pixel = *src++;
					dst[0] = pixel;
					dst[1] = (pixel >> 8) & 0xff;
					dst[2] = (pixel >> 16) & 0xff;
					dst[3] = pixel >> 24;
					dst += 4;
				}
				dst += skip;
			}
		}
		if (m_allocator != NULL)
			m_allocator->sce_paf_free(raster);
		else
			sce_paf_free(raster);
		return true;
	}

	if (!TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tilewidth))
		return false;
	if (!TIFFGetField(tif, TIFFTAG_TILELENGTH, &tilelength))
		return false;
	if (tilewidth > 0x400 || tilelength > 0x400) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	uint32_t npixels = tilelength * tilewidth;
	uint32_t across = (width + tilewidth - 1) / tilewidth;
	uint32_t *raster;
	if (m_allocator != NULL)
		raster = (uint32_t *)m_allocator->sce_paf_malloc(npixels * sizeof(uint32_t));
	else
		raster = (uint32_t *)sce_paf_malloc(npixels * sizeof(uint32_t));
	if (raster == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	/* The bytes the last column of tiles reaches past the image. */
	uint32_t over = across * (4 * tilewidth) - 4 * width;
	uint32_t lastrows = height % tilelength;
	int32_t tilepitch = pitch * tilelength;
	int32_t end = -1;
	for (uint32_t y = 0; y < height; y += tilelength) {
		uint32_t cols = tilewidth;
		if (y + tilelength > height)
			end = tilelength - lastrows;
		uint8_t *tile = dst;
		uint32_t col = 0;
		for (uint32_t x = 0; x < width; x += tilewidth, col++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(raster);
				else
					sce_paf_free(raster);
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			if (!TIFFReadRGBATile(tif, x, y, raster)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(raster);
				else
					sce_paf_free(raster);
				return false;
			}
			if (col == across - 1 && over != 0)
				cols = tilewidth - (over >> 2);
			uint8_t *out = tile;
			for (int32_t row = tilelength - 1; row != end; row--) {
				const uint32_t *src = &raster[row * tilewidth];
				for (uint32_t i = 0; i < cols; i++) {
					uint32_t pixel = *src++;
					out[0] = pixel;
					out[1] = (pixel >> 8) & 0xff;
					out[2] = (pixel >> 16) & 0xff;
					out[3] = pixel >> 24;
					out += 4;
				}
				out += pitch - 4 * cols;
			}
			tile += 4 * tilewidth;
		}
		dst += tilepitch;
	}
	if (m_allocator != NULL)
		m_allocator->sce_paf_free(raster);
	else
		sce_paf_free(raster);
	return true;
}

/*
 * A palette image as 4- or 8-bit indices, by scanline or by tile; 4-bit
 * indices with the first pixel of each byte moved to the low nibble.
 */
bool TifCodec::DecodeIndex(TIFF *tif, uint8_t *dst, const ImageAttribute *attr,
			   DecodeCancelFunc cancelfunc, void *userdata)
{
	int32_t pitch = attr->GetMemPitch(0);
	uint32_t width = attr->m_extent.w;
	uint32_t height = attr->m_extent.h;
	uint16_t bits;
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits))
		return false;
	if (bits < 1 || bits > 16)
		return false;

	if (!TIFFIsTiled(tif)) {
		if (pitch < TIFFScanlineSize(tif)) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}

		if (bits == 4) {
			tsize_t rowsize = GetRowSize(tif, width);
			int32_t skip = pitch - rowsize;
			char *line;
			if (m_allocator != NULL)
				line = (char *)m_allocator->sce_paf_malloc(rowsize);
			else
				line = (char *)sce_paf_malloc(rowsize);
			if (line == NULL) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
				return false;
			}
			for (uint32_t row = 0; row < height; row++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(line);
					else
						sce_paf_free(line);
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				if (TIFFReadScanline(tif, line, row, 0) == -1) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(line);
					else
						sce_paf_free(line);
					return false;
				}
				const char *in = line;
				for (int32_t i = 0; i < rowsize; i++) {
					char pair = *in++;
					*dst++ = ((pair >> 4) & 0xf) | ((pair << 4) & 0xf0);
				}
				dst += skip;
			}
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(line);
			else
				sce_paf_free(line);
			return true;
		}

		if (bits == 8) {
			for (uint32_t row = 0; row < height; row++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				if (TIFFReadScanline(tif, dst, row, 0) == -1)
					return false;
				dst += pitch;
			}
		}
		return true;
	}

	uint32_t tilewidth;
	uint32_t tilelength;
	if (!TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tilewidth) ||
	    !TIFFGetField(tif, TIFFTAG_TILELENGTH, &tilelength))
		return false;
	if (tilewidth > 0x400 || tilelength > 0x400) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	tsize_t tilesize = TIFFTileSize(tif);
	uint32_t across = (width + tilewidth - 1) / tilewidth;
	uint8_t *buff;
	if (m_allocator != NULL)
		buff = (uint8_t *)m_allocator->sce_paf_malloc(tilesize);
	else
		buff = (uint8_t *)sce_paf_malloc(tilesize);
	if (buff == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	tsize_t tilerow = TIFFTileRowSize(tif);
	uint32_t rows = tilelength;
	uint32_t over = across * tilerow - GetRowSize(tif, width);
	uint32_t lastrows = height % tilelength;
	if (bits == 4) {
		for (uint32_t y = 0; y < height; y += tilelength) {
			uint32_t cols = tilerow;
			if (y + tilelength > height)
				rows = lastrows;
			uint8_t *tile = dst;
			uint32_t col = 0;
			for (uint32_t x = 0; x < width; x += tilewidth, col++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(buff);
					else
						sce_paf_free(buff);
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				if (TIFFReadTile(tif, buff, x, y, 0, 0) == -1) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(buff);
					else
						sce_paf_free(buff);
					return false;
				}
				if (col == across - 1 && over != 0)
					cols = tilerow - over;
				uint8_t *out = tile;
				const uint8_t *src = buff;
				for (uint32_t row = 0; row < rows; row++) {
					const uint8_t *in = src;
					for (uint32_t i = cols; i != 0; i--) {
						uint8_t pair = *in++;
						*out++ = ((pair >> 4) & 0xf) | ((pair << 4) & 0xf0);
					}
					out += pitch - cols;
					src += tilerow;
				}
				tile += tilerow;
			}
			dst += pitch * tilelength;
		}
	} else if (bits == 8) {
		for (uint32_t y = 0; y < height; y += tilelength) {
			uint32_t cols = tilerow;
			if (y + tilelength > height)
				rows = lastrows;
			uint8_t *tile = dst;
			uint32_t col = 0;
			for (uint32_t x = 0; x < width; x += tilewidth, col++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(buff);
					else
						sce_paf_free(buff);
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				if (TIFFReadTile(tif, buff, x, y, 0, 0) == -1) {
					if (m_allocator != NULL)
						m_allocator->sce_paf_free(buff);
					else
						sce_paf_free(buff);
					return false;
				}
				if (col == across - 1 && over != 0)
					cols = tilerow - over;
				uint8_t *out = tile;
				const uint8_t *src = buff;
				for (uint32_t row = 0; row < rows; row++) {
					const uint8_t *in = src;
					for (uint32_t i = cols; i != 0; i--)
						*out++ = *in++;
					out += pitch - cols;
					src += tilerow;
				}
				tile += tilerow;
			}
			dst += pitch * tilelength;
		}
	}
	if (m_allocator != NULL)
		m_allocator->sce_paf_free(buff);
	else
		sce_paf_free(buff);
	return true;
}

/* libtiff's colormap test: 16-bit entries when any is above 255. */
static inline int CheckColorMap(int32_t n, const uint16_t *red, const uint16_t *green,
				const uint16_t *blue)
{
	while (n-- > 0) {
		if (*red++ >= 256 || *green++ >= 256 || *blue++ >= 256)
			return 16;
	}
	return 8;
}

/* The RGBA palette of 1 << bits entries from the TIFF colormap. */
bool TifCodec::MakeCLUT(uint8_t *dst, const uint16_t *red, const uint16_t *green,
			const uint16_t *blue, int32_t bits)
{
	int32_t count = 1 << bits;
	if (CheckColorMap(count, red, green, blue) == 16) {
		for (int32_t i = 0; i < count; i++) {
			dst[0] = *red++ >> 8;
			dst[1] = *green++ >> 8;
			dst[2] = *blue++ >> 8;
			dst[3] = 0xff;
			dst += 4;
		}
		return true;
	}
	for (int32_t i = 0; i < count; i++) {
		dst[0] = *red++;
		dst[1] = *green++;
		dst[2] = *blue++;
		dst[3] = 0xff;
		dst += 4;
	}
	return true;
}

bool TifCodec::Decode(TIFF *tif, const ImageAttribute *attr, ImageArray *images,
		      ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata)
{
	uint16_t photometric;
	uint16_t bits;
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
		return false;
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits))
		return false;
	if (bits == 0 || bits > 16)
		return false;

	if (photometric == PHOTOMETRIC_PALETTE) {
		if (cluts == NULL)
			return false;
		uint8_t *clut = (uint8_t *)cluts->GetBuffer(0)->buff;
		if (clut == NULL)
			return false;
		uint16_t *red;
		uint16_t *green;
		uint16_t *blue;
		if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &red, &green, &blue))
			return false;
		if (red == NULL || green == NULL || blue == NULL)
			return false;
		if (!MakeCLUT(clut, red, green, blue, bits))
			return false;
	}
	if (photometric == PHOTOMETRIC_PALETTE) {
		if (!DecodeIndex(tif, (uint8_t *)images->GetBuffer(0)->buff, attr, cancelfunc,
				 userdata))
			return false;
		return true;
	}
	if (!DecodeRGBA(tif, (uint8_t *)images->GetBuffer(0)->buff, attr, cancelfunc, userdata))
		return false;
	return true;
}

/* Opens the image at offset of file for libtiff. */
TIFF *TifCodec::OpenTiff(common::SharedPtr<File> file, int64_t offset, size_t size)
{
	m_source.data = file.get();
	m_source.size = offset;
	m_source.pos = size;
	m_read = ReadFileProc;
	m_write = WriteProc;
	m_seek = SeekFileProc;
	m_close = CloseProc;
	m_size = SizeFileProc;
	m_map = MapProc;
	m_unmap = UnmapProc;
	m_malloc = MallocProc;
	m_realloc = ReallocProc;
	m_free = FreeProc;
	m_seek(&m_source, 0, SCE_SEEK_SET);
	if (offset != 0)
		m_seek(&m_source, offset, SCE_SEEK_SET);

	TIFF *tif = TIFFClientOpen("dummy", "rm", &m_source, m_read, m_write, m_seek, m_close,
				   m_size, m_map, m_unmap, m_allocator, m_malloc, m_realloc,
				   m_free);
	if (tif == NULL)
		return NULL;
	return tif;
}

bool TifCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	m_mutex.Lock();

	if (images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < m_pitch * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < 4 * attr->m_clutsize) {
			m_mutex.Unlock();
			return false;
		}
	}

	TIFF *tif = OpenTiff(file, offset, size);
	if (tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(tif, attr, images, cluts, cancelfunc, userdata)) {
		TIFFClose(tif);
		m_mutex.Unlock();
		return false;
	}
	TIFFClose(tif);
	m_mutex.Unlock();
	return true;
}

/* Opens the image in buf for libtiff. */
TIFF *TifCodec::OpenTiff(common::SharedPtr<Buffer> buf)
{
	m_source.data = buf->GetAddr();
	m_source.size = buf->GetSize();
	m_source.pos = 0;
	m_read = ReadBufferProc;
	m_write = WriteProc;
	m_seek = SeekBufferProc;
	m_close = CloseProc;
	m_size = SizeBufferProc;
	m_map = MapProc;
	m_unmap = UnmapProc;
	m_malloc = MallocProc;
	m_realloc = ReallocProc;
	m_free = FreeProc;

	TIFF *tif = TIFFClientOpen("dummy", "rm", &m_source, m_read, m_write, m_seek, m_close,
				   m_size, m_map, m_unmap, m_allocator, m_malloc, m_realloc,
				   m_free);
	if (tif == NULL)
		return NULL;
	return tif;
}

bool TifCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < m_pitch * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < 4 * attr->m_clutsize) {
			m_mutex.Unlock();
			return false;
		}
	}

	TIFF *tif = OpenTiff(buf);
	if (tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(tif, attr, images, cluts, cancelfunc, userdata)) {
		TIFFClose(tif);
		m_mutex.Unlock();
		return false;
	}
	TIFFClose(tif);
	m_mutex.Unlock();
	return true;
}

bool TifCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	return false;
}

/*
 * The image's size and mode: 4- or 8-bit indices for a palette, RGBA8888
 * otherwise, loaded by line only for RGBA strips.
 */
bool TifCodec::ReadHeader(TIFF *tif, ImageAttribute *attr, ImageArray *images,
			  ImageArray *cluts)
{
	uint16_t bits;
	uint16_t samples;
	uint16_t photometric;
	uint32_t width;
	uint32_t height;
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits) || bits < 1 || bits > 16)
		return false;
	if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples) || samples < 1)
		return false;
	if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width))
		return false;
	if (!TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height))
		return false;
	if (width > 0x3fff || height > 0x3fff) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	attr->m_extent = ImageExtent(width, height);
	attr->m_clip = ImageRect(0, 0, width, height);
	attr->m_order = ImageOrder_Linear;
	attr->m_format = ImageFormat_Tif;
	attr->m_xtype = ImageXType_ConstantSize;
	attr->m_ytype = ImageYType_Sequence;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
		return false;

	switch (photometric) {
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_PALETTE:
	case PHOTOMETRIC_LOGL:
		if (samples != 1) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		break;
	case PHOTOMETRIC_RGB:
	case PHOTOMETRIC_YCBCR:
	case PHOTOMETRIC_CIELAB:
	case PHOTOMETRIC_ICCLAB:
	case PHOTOMETRIC_ITULAB:
	case PHOTOMETRIC_LOGLUV:
		if (samples != 3 && samples != 4) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		if (samples != 4) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		break;
	case PHOTOMETRIC_MASK:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	default:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	ImageMode mode;
	int32_t pitch;
	int32_t count;
	switch (photometric) {
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_RGB:
	case PHOTOMETRIC_MASK:
	case PHOTOMETRIC_SEPARATED:
	case PHOTOMETRIC_YCBCR:
	case PHOTOMETRIC_CIELAB:
	case PHOTOMETRIC_ICCLAB:
	case PHOTOMETRIC_ITULAB:
	case PHOTOMETRIC_LOGL:
	case PHOTOMETRIC_LOGLUV:
		mode = ImageMode_RGBA8888;
		pitch = 4 * attr->m_extent.w;
		count = 0;
		break;
	case PHOTOMETRIC_PALETTE:
		count = 1 << bits;
		switch (bits) {
		case 4:
			mode = ImageMode_index4;
			pitch = (attr->m_extent.w + 1) / 2;
			break;
		case 8:
			mode = ImageMode_index8;
			pitch = attr->m_extent.w;
			break;
		default:
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		attr->m_clutsize = count;
		break;
	default:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	attr->m_mode = mode;
	m_pitch = pitch;
	if (photometric != PHOTOMETRIC_PALETTE && !TIFFIsTiled(tif) &&
	    attr->m_mode == ImageMode_RGBA8888)
		SetLoadType(LoadType_Line);
	else
		SetLoadType(LoadType_Image);
	images->Init(1, 1);
	ImageMode clutmode;
	if (count != 0) {
		cluts->Init(1, 1);
		clutmode = ImageMode_RGBA8888;
	} else {
		attr->m_clutsize = 0;
		clutmode = ImageMode_NONE;
	}
	attr->m_clutmode = clutmode;
	return true;
}

bool TifCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	Image::SetErrorType(SCE_OK);

	TIFF *tif = OpenTiff(file, offset, size);
	if (tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(tif, attr, images, cluts)) {
		TIFFClose(tif);
		m_mutex.Unlock();
		return false;
	}
	TIFFClose(tif);
	m_mutex.Unlock();
	return true;
}

bool TifCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	Image::SetErrorType(SCE_OK);

	TIFF *tif = OpenTiff(buf);
	if (tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(tif, attr, images, cluts)) {
		TIFFClose(tif);
		m_mutex.Unlock();
		return false;
	}
	TIFFClose(tif);
	m_mutex.Unlock();
	return true;
}

bool TifCodec::Open(common::SharedPtr<File> file, int64_t offset, size_t size,
		    const ImageAttribute *attr)
{
	m_mutex.Lock();

	Image::SetErrorType(SCE_OK);
	m_tif = OpenTiff(file, offset, size);
	if (m_tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool TifCodec::Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr)
{
	m_mutex.Lock();

	Image::SetErrorType(SCE_OK);
	m_tif = OpenTiff(buf);
	if (m_tif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool TifCodec::Close()
{
	TIFFClose(m_tif);
	return true;
}

/* Starts reading the rows of the subrect of attr, RGBA strip by strip. */
bool TifCodec::BeginLines(const ImageAttribute *attr, const ImageArray *images,
			  uint32_t *pitch, bool *topdown)
{
	if (images->GetMax() != 1)
		return false;

	uint32_t height = attr->m_originalextent.h;
	m_width = attr->m_originalextent.w;
	if (!TIFFGetField(m_tif, TIFFTAG_ROWSPERSTRIP, &m_rowsperstrip))
		return false;
	if (m_rowsperstrip > 0x3fff) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	if (m_rowsperstrip > height) {
		if (!TIFFSetField(m_tif, TIFFTAG_ROWSPERSTRIP, height))
			return false;
		m_rowsperstrip = height;
	}
	m_strips = (height + m_rowsperstrip - 1) / m_rowsperstrip;
	m_lastrows = height % m_rowsperstrip;
	if (m_allocator != NULL)
		m_raster = (uint32_t *)m_allocator->sce_paf_malloc(4 * (m_rowsperstrip * m_width));
	else
		m_raster = (uint32_t *)sce_paf_malloc(4 * (m_rowsperstrip * m_width));
	if (m_raster == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}
	m_rows = 0;
	m_strip = 0;

	ImageRect subrect = attr->m_subrect;
	if (subrect.extent.w <= 0 || subrect.extent.h <= 0)
		return false;
	if (subrect.origin == ImagePoint(0, 0) && attr->m_originalextent == subrect.extent)
		subrect.extent.w = m_width;
	*pitch = 4 * subrect.extent.w;
	*topdown = true;
	m_top = subrect.origin.y;
	return true;
}

bool TifCodec::EndLines()
{
	if (m_raster != NULL) {
		if (m_allocator != NULL)
			m_allocator->sce_paf_free(m_raster);
		else
			sce_paf_free(m_raster);
		m_raster = NULL;
	}
	return true;
}

bool TifCodec::PrepareLine()
{
	return true;
}

/* The pixels of rect in the next row, read from the next strip when needed. */
bool TifCodec::ReadLine(void *dst, const ImageRect *rect)
{
	if (m_rows == 0 && m_strip < m_strips) {
		m_rows = m_rowsperstrip;
		if (!TIFFReadRGBAStrip(m_tif, m_strip * m_rowsperstrip, m_raster)) {
			if (m_allocator != NULL)
				m_allocator->sce_paf_free(m_raster);
			else
				sce_paf_free(m_raster);
			m_raster = NULL;
			return false;
		}
		if (m_strip == m_strips - 1 && m_lastrows != 0)
			m_rows = m_lastrows;
		m_strip++;
	}

	const uint32_t *src = &m_raster[(m_rows - 1) * m_width];
	m_rows--;
	uint32_t x = rect->origin.x;
	uint32_t end = rect->extent.w + x;
	m_dst = (uint8_t *)dst;
	for (uint32_t i = x; i < end; i++) {
		uint32_t pixel = src[i];
		*m_dst++ = pixel;
		*m_dst++ = (pixel >> 8) & 0xff;
		*m_dst++ = (pixel >> 16) & 0xff;
		*m_dst++ = pixel >> 24;
	}
	return true;
}

/* Passes a row; strips that end above the subrect are not decoded. */
bool TifCodec::SkipLine()
{
	if (m_rows == 0 && m_strip < m_strips) {
		if (m_rowsperstrip * m_strip + m_rowsperstrip > m_top) {
			m_rows = m_rowsperstrip;
			if (!TIFFReadRGBAStrip(m_tif, m_strip * m_rowsperstrip, m_raster)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(m_raster);
				else
					sce_paf_free(m_raster);
				m_raster = NULL;
				return false;
			}
		} else {
			m_rows = m_rowsperstrip;
		}
		if (m_strip == m_strips - 1 && m_lastrows != 0)
			m_rows = m_lastrows;
		m_strip++;
	}
	m_rows--;
	return true;
}

TifCodec::TifCodec() : m_mutex("ScePafImageTifMutex", 0)
{
	m_tif = NULL;
	m_strips = 0;
	m_rows = 0;
	m_strip = 0;
	m_rowsperstrip = 0;
	m_lastrows = 0;
	m_raster = NULL;
	m_dst = NULL;
	m_top = 0;
	m_pitch = 0;
	m_width = 0;
	m_allocator = NULL;
}

TifCodec::~TifCodec()
{
}

/* libtiff's client procs over TifCodec::Source. */
static tsize_t ReadFileProc(thandle_t handle, tdata_t buf, tsize_t size)
{
	TifCodec::Source *source = (TifCodec::Source *)handle;
	if (source == NULL || buf == NULL)
		return 0;
	return ((File *)source->data)->Read(buf, size);
}

static tsize_t ReadBufferProc(thandle_t handle, tdata_t buf, tsize_t size)
{
	TifCodec::Source *source = (TifCodec::Source *)handle;
	if (source == NULL || buf == NULL)
		return 0;

	int64_t rest = source->size - source->pos;
	size_t count;
	if (rest > (size_t)size) {
		_TIFFmemcpy(buf, source->data, size);
		count = size;
	} else {
		count = rest;
		if (count == 0)
			return 0;
		_TIFFmemcpy(buf, source->data, count);
	}
	source->data = (uint8_t *)source->data + count;
	source->pos += count;
	return count;
}

static tsize_t WriteProc(thandle_t handle, tdata_t buf, tsize_t size)
{
	return 0;
}

static toff_t SeekFileProc(thandle_t handle, toff_t offset, int whence)
{
	TifCodec::Source *source = (TifCodec::Source *)handle;
	if (source == NULL)
		return 0;
	return ((File *)source->data)->Seek(offset, whence);
}

static toff_t SeekBufferProc(thandle_t handle, toff_t offset, int whence)
{
	TifCodec::Source *source = (TifCodec::Source *)handle;
	if (source == NULL)
		return 0;

	switch (whence) {
	case SCE_SEEK_SET:
		if (offset > (toff_t)source->size)
			return 0;
		source->data = (uint8_t *)source->data - source->pos + offset;
		source->pos = offset;
		return offset;
	case SCE_SEEK_CUR: {
		uint32_t pos = source->pos + offset;
		if (pos > source->size)
			return 0;
		source->data = (uint8_t *)source->data + offset;
		source->pos = pos;
		return pos;
	}
	case SCE_SEEK_END:
		if (offset != 0)
			return 0;
		source->data = (uint8_t *)source->data + (source->size - source->pos) + offset;
		source->pos = source->size + offset;
		return source->pos;
	}
	return 0;
}

static int CloseProc(thandle_t handle)
{
	return 0;
}

static toff_t SizeFileProc(thandle_t handle)
{
	File *file = (File *)((TifCodec::Source *)handle)->data;
	file->Seek(0, SCE_SEEK_END);
	toff_t size = ((File *)((TifCodec::Source *)handle)->data)->GetFileSize();
	((File *)((TifCodec::Source *)handle)->data)->Seek(0, SCE_SEEK_SET);
	return size;
}

static toff_t SizeBufferProc(thandle_t handle)
{
	return ((TifCodec::Source *)handle)->size;
}

static int MapProc(thandle_t handle, tdata_t *base, toff_t *size)
{
	return 0;
}

static void UnmapProc(thandle_t handle, tdata_t base, toff_t size)
{
}

static tdata_t MallocProc(void *allocator, tsize_t size)
{
	if (allocator != NULL)
		return ((HeapAllocator *)allocator)->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

static tdata_t ReallocProc(void *allocator, tdata_t ptr, tsize_t size)
{
	if (allocator != NULL)
		return ((HeapAllocator *)allocator)->sce_paf_realloc(ptr, size);
	return ScePafStdc_0ADEA3AD(ptr, size);
}

static void FreeProc(void *allocator, tdata_t ptr)
{
	if (allocator != NULL)
		((HeapAllocator *)allocator)->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}
