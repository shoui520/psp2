/*
 * paf::image::PngCodec: PNG images through libpng, decoded whole or line by
 * line into RGBA8888, or into 4- or 8-bit indices with an RGBA palette; and
 * the PNG encoder, RGBA8888 into a file or a buffer.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <std/setjmp.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "image_codec.h"
#include "image_source.h"
#include "../../../paf_types.h"

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

static png_voidp PngMalloc(png_structp png, png_alloc_size_t size);
static void PngFree(png_structp png, png_voidp ptr);
static void ReadFile(png_structp png, png_bytep data, png_size_t length);
static void ReadBuffer(png_structp png, png_bytep data, png_size_t length);
static void WriteFile(png_structp png, png_bytep data, png_size_t length);
static void FlushFile(png_structp png);
static void WriteBuffer(png_structp png, png_bytep data, png_size_t length);

/*
 * A PNG image being read: libpng's state, the image's header, and what the
 * image is read from, a File or a buffer's bytes with a position.
 */
class paf::image::PngReader {
public:
	PngReader(common::SharedPtr<File> file, int64_t offset, size_t size,
		  HeapAllocator *allocator);
	PngReader(common::SharedPtr<Buffer> buf, HeapAllocator *allocator);

	~PngReader()
	{
		if (!m_error) {
			if (m_line != NULL) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(m_line);
				else
					sce_paf_free(m_line);
				m_line = NULL;
			}
			m_src = NULL;
			m_dst = NULL;
			m_attr = NULL;
			png_destroy_read_struct(&m_png, &m_info, &m_endinfo);
		}
	}

	void Create(HeapAllocator *allocator);
	void Open(common::SharedPtr<File> file, int64_t offset, size_t size);
	int ReadInfo();
	int Decode(uint8_t *buff, size_t size, int32_t pitch, DecodeCancelFunc cancelfunc,
		   void *userdata);
	int DecodeIndex4(uint8_t *buff, int32_t pitch, DecodeCancelFunc cancelfunc,
			 void *userdata);
	int DecodeIndex2(uint8_t *buff, int32_t pitch, DecodeCancelFunc cancelfunc,
			 void *userdata);
	int BeginLines(const ImageAttribute *attr, const ImageArray *images, uint32_t *pitch,
		       bool *topdown);
	int ReadLine(void *dst, const ImageRect *rect);

	void *m_data;			/* the File, or the next byte of the buffer */
	int64_t m_size;
	uint32_t m_pos;			/* bytes of the buffer read */
	uint32_t m_reserved;
	png_rw_ptr m_read;
	int64_t (*m_seek)(PngReader *reader, int64_t offset, int32_t whence);
	png_structp m_png;
	png_infop m_info;
	png_infop m_endinfo;
	png_uint_32 m_width;
	png_uint_32 m_height;
	int m_bitdepth;
	int m_colortype;
	int m_interlace;
	png_colorp m_palette;
	int m_numpalette;
	uint8_t *m_line;		/* a row as libpng reads it, for line decoding */
	uint8_t *m_src;
	uint8_t *m_dst;
	const ImageAttribute *m_attr;	/* the attribute line decoding began with */
	bool m_opened;
	bool m_inforead;
	bool m_error;
	bool m_buffer;			/* read from a buffer, not a File */
	HeapAllocator *m_allocator;
};

static __attribute__((noinline)) int64_t SeekFile(PngReader *reader, int64_t offset,
						 int32_t whence);

bool PngAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool PngAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool PngAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool PngAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

PngAttribute::PngAttribute()
{
}

PngAttribute::~PngAttribute()
{
}

bool paf::image::IsPng(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 8)
		return false;

	const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	return sce_paf_memcmp(signature, buf->GetAddr(), 8) == 0;
}

/* 4-bit indices, the first pixel of each byte moved to the low nibble. */
int PngReader::DecodeIndex4(uint8_t *buff, int32_t pitch, DecodeCancelFunc cancelfunc,
			     void *userdata)
{
	if (sce_paf_setjmp(png_jmpbuf(m_png))) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}

	int passes = png_set_interlace_handling(m_png);
	size_t rowbytes = (m_width + 1) >> 1;
	int32_t skip = pitch - rowbytes;
	uint8_t *row;
	if (m_allocator != NULL)
		row = (uint8_t *)m_allocator->sce_paf_malloc(rowbytes);
	else
		row = (uint8_t *)sce_paf_malloc(rowbytes);
	if (row == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	png_start_read_image(m_png);
	for (int pass = 0; pass < passes; pass++) {
		uint8_t *dst = buff;
		for (uint32_t y = 0; y < m_height; y++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(row);
				else
					sce_paf_free(row);
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			png_read_row(m_png, row, NULL);
			const uint8_t *src = row;
			for (size_t i = rowbytes; i != 0; i--) {
				int pixels = *src++;
				*dst++ = ((pixels >> 4) & 0x0f) | ((pixels << 4) & 0xf0);
			}
			dst += skip;
		}
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(row);
	else
		sce_paf_free(row);
	png_read_end(m_png, NULL);
	return true;
}

/* 2-bit indices widened to 4 bits, the first pixel of each byte in the low nibble. */
int PngReader::DecodeIndex2(uint8_t *buff, int32_t pitch, DecodeCancelFunc cancelfunc,
			     void *userdata)
{
	if (sce_paf_setjmp(png_jmpbuf(m_png))) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}

	int32_t skip = pitch - ((m_width + 1) >> 1);
	size_t rowbytes = (m_width + 3) >> 2;
	uint8_t *row;
	if (m_allocator != NULL)
		row = (uint8_t *)m_allocator->sce_paf_malloc(rowbytes);
	else
		row = (uint8_t *)sce_paf_malloc(rowbytes);
	if (row == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	int passes = png_set_interlace_handling(m_png);
	png_start_read_image(m_png);
	for (int pass = 0; pass < passes; pass++) {
		uint8_t *dst = buff;
		for (uint32_t y = 0; y < m_height; y++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				if (m_allocator != NULL)
					m_allocator->sce_paf_free(row);
				else
					sce_paf_free(row);
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			png_read_row(m_png, row, NULL);
			uint32_t x;
			for (x = 0; x < m_width >> 2; x++) {
				dst[0] = (row[x] & 0x30) | ((row[x] >> 6) & 3);
				dst[1] = ((row[x] >> 2) & 3) | ((row[x] << 4) & 0x30);
				dst += 2;
			}
			uint32_t rest = m_width & 3;
			if (rest != 0) {
				*dst++ = (row[x] & 0x30) | ((row[x] >> 6) & 3);
				if (rest >= 3)
					*dst++ = ((row[x] >> 2) & 3) | ((row[x] << 4) & 0x30);
			}
			dst += skip;
		}
	}

	if (m_allocator != NULL)
		m_allocator->sce_paf_free(row);
	else
		sce_paf_free(row);
	png_read_end(m_png, NULL);
	return true;
}

/* Reads the image's header, once, after the reader is opened. */
int PngReader::ReadInfo()
{
	if (!m_opened || m_inforead)
		return false;
	if (sce_paf_setjmp(png_jmpbuf(m_png)))
		return false;

	int compression;
	int filter;
	png_read_info(m_png, m_info);
	png_get_IHDR(m_png, m_info, &m_width, &m_height, &m_bitdepth, &m_colortype,
		     &m_interlace, &compression, &filter);
	png_get_PLTE(m_png, m_info, &m_palette, &m_numpalette);
	m_inforead = true;
	return true;
}

int PngReader::Decode(uint8_t *buff, size_t size, int32_t pitch, DecodeCancelFunc cancelfunc,
		      void *userdata)
{
	if (!m_inforead && !ReadInfo())
		return false;

	if (m_colortype != PNG_COLOR_TYPE_PALETTE) {
		if (sce_paf_setjmp(png_jmpbuf(m_png))) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}
		int passes = png_set_interlace_handling(m_png);
		png_start_read_image(m_png);
		for (int pass = 0; pass < passes; pass++) {
			uint8_t *dst = buff;
			for (int32_t y = 0; y < (int32_t)m_height; y++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				png_read_row(m_png, dst, NULL);
				dst += pitch;
			}
		}
		png_read_end(m_png, NULL);
		return true;
	}

	if (m_interlace != PNG_INTERLACE_NONE) {
		if (sce_paf_setjmp(png_jmpbuf(m_png))) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}
		int passes = png_set_interlace_handling(m_png);
		png_start_read_image(m_png);
		for (int pass = 0; pass < passes; pass++) {
			uint8_t *dst = buff;
			for (int32_t y = 0; y < (int32_t)m_height; y++) {
				if (cancelfunc != NULL && cancelfunc(userdata)) {
					Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
					return false;
				}
				png_read_row(m_png, dst, NULL);
				dst += pitch;
			}
		}
		png_read_end(m_png, NULL);
		return true;
	}

	if (m_bitdepth == 2) {
		if (!DecodeIndex2(buff, pitch, cancelfunc, userdata))
			return false;
		return true;
	}
	if (m_bitdepth == 4) {
		if (!DecodeIndex4(buff, pitch, cancelfunc, userdata))
			return false;
		return true;
	}

	if (sce_paf_setjmp(png_jmpbuf(m_png))) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}
	int passes = png_set_interlace_handling(m_png);
	png_start_read_image(m_png);
	for (int pass = 0; pass < passes; pass++) {
		uint8_t *dst = buff;
		for (int32_t y = 0; y < (int32_t)m_height; y++) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			png_read_row(m_png, dst, NULL);
			dst += pitch;
		}
	}
	png_read_end(m_png, NULL);
	return true;
}

/* The RGBA palette of count entries: the image's palette, then black, then its alphas. */
bool PngCodec::MakeCLUT(PngReader *reader, uint8_t *dst, const png_color *palette,
			int32_t count)
{
	if (dst == NULL || palette == NULL)
		return false;

	uint8_t *entry = dst;
	int32_t numpalette = reader->m_numpalette;
	int32_t i;
	for (i = 0; i < numpalette; i++) {
		entry[0] = palette->red;
		entry[1] = palette->green;
		entry[2] = palette->blue;
		entry[3] = 0xff;
		palette++;
		entry += 4;
	}
	for (; i < count; i++) {
		entry[0] = 0;
		entry[1] = 0;
		entry[2] = 0;
		entry[3] = 0xff;
		entry += 4;
	}

	png_bytep alpha;
	int numalpha;
	png_color_16p color;
	if (png_get_valid(reader->m_png, reader->m_info, PNG_INFO_tRNS) &&
	    png_get_tRNS(reader->m_png, reader->m_info, &alpha, &numalpha, &color)) {
		for (int32_t j = 0; j < numalpha; j++) {
			dst[3] = alpha[j];
			dst += 4;
		}
	}
	return true;
}

bool PngCodec::Decode(PngReader *reader, const ImageAttribute *attr, ImageArray *images,
		      ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata)
{
	if (!reader->ReadInfo())
		return false;
	if (reader->m_colortype == PNG_COLOR_TYPE_PALETTE) {
		if (reader->m_numpalette <= 0)
			return false;
		uint8_t *clut = (uint8_t *)cluts->GetBuffer(0)->buff;
		if (clut == NULL || reader->m_palette == NULL)
			return false;
		if (!MakeCLUT(reader, clut, reader->m_palette, attr->m_clutsize))
			return false;
	}

	int32_t pitch = attr->GetMemPitch(0);
	if (m_transforms & Transform_StripAlpha)
		png_set_strip_alpha(reader->m_png);
	if (m_transforms & Transform_Packing)
		png_set_packing(reader->m_png);
	if (m_transforms & Transform_Filler)
		png_set_filler(reader->m_png, 0xff, PNG_FILLER_AFTER);
	if (m_transforms & Transform_Strip16)
		png_set_strip_16(reader->m_png);
	if (m_transforms & Transform_GrayToRGB)
		png_set_gray_to_rgb(reader->m_png);
	if (m_transforms & Transform_ExpandGray)
		png_set_expand_gray_1_2_4_to_8(reader->m_png);

	uint8_t *buff = (uint8_t *)images->GetBuffer(0)->buff;
	if (!reader->Decode(buff, images->GetBuffer(0)->size, pitch, cancelfunc, userdata))
		return false;
	return true;
}

/* libpng's warning handler, which the codec does not install. */
void PngWarning(png_structp png, png_const_charp message)
{
}

/* Reads from file, from offset on. */
void PngReader::Open(common::SharedPtr<File> file, int64_t offset, size_t size)
{
	m_data = file.get();
	m_size = file->GetFileSize();
	m_pos = 0;
	m_read = ReadFile;
	m_seek = SeekFile;
	if (SeekFile(this, 0, SCE_SEEK_SET) < 0) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
		m_error = true;
		m_opened = false;
	}
	if (offset != 0) {
		if (m_seek(this, offset, SCE_SEEK_SET) < 0) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
			m_error = true;
			m_opened = false;
			png_destroy_read_struct(&m_png, &m_info, &m_endinfo);
		}
	}
	if (!m_error) {
		m_read = ReadFile;
		png_set_read_fn(m_png, m_data, ReadFile);
		m_opened = true;
	}
	m_buffer = false;
}

/* libpng's read and info structures, allocated from allocator. */
void PngReader::Create(HeapAllocator *allocator)
{
	m_error = false;
	m_inforead = false;
	m_opened = false;
	m_png = NULL;
	m_info = NULL;
	m_endinfo = NULL;
	m_width = 0;
	m_height = 0;
	m_bitdepth = 0;
	m_colortype = 0;
	m_interlace = 0;
	m_palette = NULL;
	m_numpalette = 0;
	m_line = NULL;
	m_src = NULL;
	m_dst = NULL;
	m_attr = NULL;
	m_allocator = allocator;

	m_png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL, allocator,
					 PngMalloc, PngFree);
	png_set_mem_fn(m_png, allocator, PngMalloc, PngFree);
	m_info = png_create_info_struct(m_png);
	m_endinfo = png_create_info_struct(m_png);
	if (m_png == NULL || m_info == NULL || m_endinfo == NULL) {
		m_error = true;
		png_destroy_read_struct(&m_png, &m_info, &m_endinfo);
		return;
	}
	png_set_read_status_fn(m_png, NULL);
}

PngReader::PngReader(common::SharedPtr<File> file, int64_t offset, size_t size,
		     HeapAllocator *allocator)
{
	m_data = NULL;
	m_pos = 0;
	m_size = 0;
	Create(allocator);
	if (!m_error)
		Open(file, offset, size);
}

bool PngCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
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

	PngReader reader(file, offset, size, m_allocator);
	if (reader.m_error) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(&reader, attr, images, cluts, cancelfunc, userdata)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

PngReader::PngReader(common::SharedPtr<Buffer> buf, HeapAllocator *allocator)
{
	m_size = 0;
	m_data = NULL;
	m_pos = 0;
	Create(allocator);
	if (!m_error) {
		common::SharedPtr<Buffer> source = buf;
		m_data = source->GetAddr();
		m_size = source->GetSize();
		m_pos = 0;
		m_seek = NULL;
		m_read = ReadBuffer;
		png_set_read_fn(m_png, this, ReadBuffer);
		m_buffer = true;
		m_opened = true;
	}
}

bool PngCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
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

	PngReader reader(buf, m_allocator);
	if (reader.m_error) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(&reader, attr, images, cluts, cancelfunc, userdata)) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool PngCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	return false;
}

int32_t paf::image::SavePng(const char *filename, const void *addr, uint32_t pitch,
			    const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			    HeapAllocator *allocator)
{
	if (addr == NULL || pitch == 0 || ext.w <= 0 || ext.h <= 0 ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return SCE_PAF_ERROR_IMAGE_INVALID_ARGUMENT;

	int32_t error;
	common::SharedPtr<File> file = LocalFile::Open(
		filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, io_mode, &error);
	if (file.get() == NULL)
		return SCE_PAF_ERROR_IMAGE_OFFSET;

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) {
		file->Close();
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	}
	png_infop info = png_create_info_struct(png);
	if (info == NULL) {
		png_destroy_write_struct(&png, NULL);
		file->Close();
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	}
	if (sce_paf_setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		file->Close();
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	}

	png_set_write_fn(png, file.get(), WriteFile, FlushFile);
	png_set_filter(png, 0, PNG_ALL_FILTERS);
	png_set_compression_level(png, 1);
	png_set_IHDR(png, info, ext.w, ext.h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png, info);

	png_bytepp rows;
	if (allocator != NULL)
		rows = (png_bytepp)allocator->sce_paf_malloc(4 * ext.h);
	else
		rows = (png_bytepp)sce_paf_malloc(4 * ext.h);
	if (rows == NULL) {
		png_destroy_write_struct(&png, &info);
		file->Close();
		return SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY;
	}
	for (int32_t y = 0; y < ext.h; y++) {
		rows[y] = (png_bytep)addr;
		addr = (const uint8_t *)addr + 4 * pitch;
	}
	png_write_image(png, rows);
	png_write_end(png, info);
	if (allocator != NULL)
		allocator->sce_paf_free(rows);
	else
		sce_paf_free(rows);

	png_destroy_write_struct(&png, &info);
	file->Close();
	return SCE_OK;
}

/* Where the encoder writes in memory: the next byte, the room and the bytes written. */
struct PngWriteBuffer {
	uint8_t *data;
	uint32_t size;
	uint32_t written;
};

int32_t paf::image::SavePng(void *dst, uint32_t max_size, uint32_t &written, const void *addr,
			    uint32_t pitch, const ImageExtent &ext, ImageMode mode,
			    HeapAllocator *allocator)
{
	written = 0;
	if (dst == NULL || addr == NULL || pitch == 0 || ext.w <= 0 || ext.h <= 0 ||
	    mode != ImageMode_RGBA8888)
		return SCE_PAF_ERROR_IMAGE_INVALID_ARGUMENT;

	PngWriteBuffer buffer;
	buffer.data = (uint8_t *)dst;
	buffer.size = max_size;
	buffer.written = 0;

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL)
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	png_infop info = png_create_info_struct(png);
	if (info == NULL) {
		png_destroy_write_struct(&png, NULL);
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	}
	if (sce_paf_setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		return SCE_PAF_ERROR_IMAGE_ENCODE;
	}

	png_set_write_fn(png, &buffer, WriteBuffer, NULL);
	png_set_filter(png, 0, PNG_ALL_FILTERS);
	png_set_compression_level(png, 1);
	png_set_IHDR(png, info, ext.w, ext.h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png, info);

	png_bytepp rows;
	if (allocator != NULL)
		rows = (png_bytepp)allocator->sce_paf_malloc(4 * ext.h);
	else
		rows = (png_bytepp)sce_paf_malloc(4 * ext.h);
	if (rows == NULL) {
		png_destroy_write_struct(&png, &info);
		return SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY;
	}
	for (int32_t y = 0; y < ext.h; y++) {
		rows[y] = (png_bytep)addr;
		addr = (const uint8_t *)addr + 4 * pitch;
	}
	png_write_image(png, rows);
	png_write_end(png, info);
	if (allocator != NULL)
		allocator->sce_paf_free(rows);
	else
		sce_paf_free(rows);

	png_destroy_write_struct(&png, &info);
	written = buffer.data - (uint8_t *)dst;
	return SCE_OK;
}

/*
 * The image's mode, palette and row size, and the transformations that give
 * them: RGBA8888 for gray and colour images, 4- or 8-bit indices for palettes.
 */
bool PngCodec::ReadHeader(PngReader *reader, ImageAttribute *attr, ImageArray *images,
			  ImageArray *cluts)
{
	if (!reader->ReadInfo())
		return false;

	int32_t w = reader->m_width;
	int32_t h = reader->m_height;
	attr->m_extent = ImageExtent(w, h);
	attr->m_clip = ImageRect(0, 0, w, h);
	attr->m_format = ImageFormat_Png;

	int bitdepth = reader->m_bitdepth;
	int colortype = reader->m_colortype;
	uint32_t transforms = 0;
	m_transforms = 0;
	int interlace = reader->m_interlace;
	if (bitdepth == 16) {
		transforms = Transform_Strip16;
		m_transforms = transforms;
	}

	ImageMode mode;
	int32_t count;
	switch (colortype) {
	case PNG_COLOR_TYPE_GRAY:
		if (bitdepth < 8)
			transforms |= Transform_ExpandGray;
		transforms |= Transform_Filler;
		/* fall through */
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		mode = ImageMode_RGBA8888;
		attr->m_mode = mode;
		m_transforms = transforms | Transform_GrayToRGB;
		count = 0;
		m_pitch = 4 * w;
		break;
	case PNG_COLOR_TYPE_PALETTE:
		count = reader->m_numpalette;
		if (count <= 0)
			return false;
		switch (bitdepth) {
		case 1:
			if (count > 2) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			m_pitch = w;
			mode = ImageMode_index8;
			m_transforms = transforms | Transform_Packing;
			attr->m_mode = mode;
			attr->m_clutsize = 256;
			break;
		case 2:
			if (count > 4) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			if (interlace == PNG_INTERLACE_NONE) {
				mode = ImageMode_index4;
				attr->m_mode = mode;
				attr->m_clutsize = 16;
				m_pitch = (w + 3) / 4;
			} else {
				m_pitch = w;
				mode = ImageMode_index8;
				m_transforms = transforms | Transform_Packing;
				attr->m_mode = mode;
				attr->m_clutsize = 256;
			}
			break;
		case 4:
			if (count > 16) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			if (interlace == PNG_INTERLACE_NONE) {
				mode = ImageMode_index4;
				attr->m_mode = mode;
				attr->m_clutsize = 16;
				m_pitch = (w + 1) / 2;
			} else {
				m_pitch = w;
				mode = ImageMode_index8;
				m_transforms = transforms | Transform_Packing;
				attr->m_mode = mode;
				attr->m_clutsize = 256;
			}
			break;
		case 8:
			if (count > 256) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			mode = ImageMode_index8;
			attr->m_mode = mode;
			attr->m_clutsize = 256;
			m_pitch = w;
			break;
		default:
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
		break;
	case PNG_COLOR_TYPE_RGB:
		m_transforms = transforms | Transform_Filler;
		/* fall through */
	case PNG_COLOR_TYPE_RGB_ALPHA:
		mode = ImageMode_RGBA8888;
		attr->m_mode = mode;
		m_pitch = 4 * w;
		count = 0;
		break;
	default:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	attr->m_order = ImageOrder_Linear;
	attr->m_xtype = ImageXType_ConstantSize;
	attr->m_ytype = ImageYType_Sequence;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;
	if (mode == ImageMode_RGBA8888 && interlace == PNG_INTERLACE_NONE)
		SetLoadType(LoadType_Line);
	else
		SetLoadType(LoadType_Image);
	images->Init(1, 1);
	ImageMode clutmode;
	if (count > 0) {
		cluts->Init(1, 1);
		clutmode = ImageMode_RGBA8888;
	} else {
		attr->m_clutsize = 0;
		clutmode = ImageMode_NONE;
	}
	attr->m_clutmode = clutmode;
	return true;
}

bool PngCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	Image::SetErrorType(SCE_OK);

	PngReader reader(file, offset, size, m_allocator);
	if (reader.m_error) {
		m_mutex.Unlock();
		return false;
	}
	if (ReadHeader(&reader, attr, images, cluts)) {
		m_mutex.Unlock();
		return true;
	}
	m_mutex.Unlock();
	return false;
}

bool PngCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	m_allocator = images->GetHeapAllocator();
	Image::SetErrorType(SCE_OK);

	PngReader reader(buf, m_allocator);
	if (reader.m_error) {
		m_mutex.Unlock();
		return false;
	}
	if (ReadHeader(&reader, attr, images, cluts)) {
		m_mutex.Unlock();
		return true;
	}
	m_mutex.Unlock();
	return false;
}

bool PngCodec::Open(common::SharedPtr<File> file, int64_t offset, size_t size,
		    const ImageAttribute *attr)
{
	m_mutex.Lock();
	Image::SetErrorType(SCE_OK);

	bool result = false;
	m_reader = new PngReader(file, offset, size, m_allocator);
	if (m_reader == NULL || m_reader->m_error) {
		m_mutex.Unlock();
		return result;
	}
	if (!m_reader->ReadInfo()) {
		m_mutex.Unlock();
		return result;
	}
	m_mutex.Unlock();
	return true;
}

bool PngCodec::Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr)
{
	m_mutex.Lock();
	Image::SetErrorType(SCE_OK);

	m_reader = new PngReader(buf, m_allocator);
	if (m_reader == NULL || m_reader->m_error) {
		m_mutex.Unlock();
		return false;
	}
	if (!m_reader->ReadInfo()) {
		m_mutex.Unlock();
		return false;
	}
	m_mutex.Unlock();
	return true;
}

bool PngCodec::Close()
{
	if (m_reader != NULL) {
		delete m_reader;
		m_reader = NULL;
	}
	return true;
}

/* Starts reading rows of the subrect of attr; a row is 4 bytes a pixel. */
int PngReader::BeginLines(const ImageAttribute *attr, const ImageArray *images,
			  uint32_t *pitch, bool *topdown)
{
	m_attr = attr;
	if (!m_inforead && !ReadInfo())
		return false;
	if (sce_paf_setjmp(png_jmpbuf(m_png)))
		return false;

	if (m_allocator != NULL)
		m_line = (uint8_t *)m_allocator->sce_paf_malloc(4 * m_width);
	else
		m_line = (uint8_t *)sce_paf_malloc(4 * m_width);
	if (m_line == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	ImageRect subrect = attr->m_subrect;
	if (subrect.extent.w <= 0 || subrect.extent.h <= 0)
		return false;
	if (subrect.origin == ImagePoint(0, 0) && attr->m_originalextent == subrect.extent)
		subrect.extent.w = m_width;
	*pitch = 4 * subrect.extent.w;
	*topdown = true;
	png_start_read_image(m_png);
	return true;
}

bool PngCodec::BeginLines(const ImageAttribute *attr, const ImageArray *images,
			  uint32_t *pitch, bool *topdown)
{
	if (images->GetMax() != 1)
		return false;

	if (m_transforms & Transform_StripAlpha)
		png_set_strip_alpha(m_reader->m_png);
	if (m_transforms & Transform_Packing)
		png_set_packing(m_reader->m_png);
	if (m_transforms & Transform_Filler)
		png_set_filler(m_reader->m_png, 0xff, PNG_FILLER_AFTER);
	if (m_transforms & Transform_Strip16)
		png_set_strip_16(m_reader->m_png);
	if (m_transforms & Transform_GrayToRGB)
		png_set_gray_to_rgb(m_reader->m_png);
	if (m_transforms & Transform_ExpandGray)
		png_set_expand_gray_1_2_4_to_8(m_reader->m_png);
	if (!m_reader->BeginLines(attr, images, pitch, topdown))
		return false;
	return true;
}

/* Reads the rows below the subrect, then the end of the image. */
bool PngCodec::EndLines()
{
	PngReader *reader = m_reader;
	if (sce_paf_setjmp(png_jmpbuf(reader->m_png)))
		return false;

	const ImageAttribute *attr = reader->m_attr;
	int32_t y = attr->m_subrect.extent.h + attr->m_subrect.origin.y;
	int32_t height = attr->m_originalextent.h;
	for (; y < height; y++)
		png_read_row(reader->m_png, NULL, NULL);
	png_read_end(reader->m_png, NULL);
	return true;
}

/* The next row's pixels of rect. */
int PngReader::ReadLine(void *dst, const ImageRect *rect)
{
	if (sce_paf_setjmp(png_jmpbuf(m_png)))
		return false;

	png_read_row(m_png, m_line, NULL);
	m_src = m_line + 4 * rect->origin.x;
	m_dst = (uint8_t *)dst;
	for (int32_t x = 0; x < rect->extent.w; x++) {
		*m_dst++ = *m_src++;
		*m_dst++ = *m_src++;
		*m_dst++ = *m_src++;
		*m_dst++ = *m_src++;
	}
	return true;
}

bool PngCodec::ReadLine(void *dst, const ImageRect *rect)
{
	if (!m_reader->ReadLine(dst, rect))
		return false;
	return true;
}

bool PngCodec::SkipLine()
{
	PngReader *reader = m_reader;
	if (sce_paf_setjmp(png_jmpbuf(reader->m_png)))
		return false;

	png_read_row(reader->m_png, reader->m_line, NULL);
	return true;
}

bool PngCodec::PrepareLine()
{
	return true;
}

PngCodec::PngCodec() : m_mutex("ScePafImagePngMutex", 0)
{
	m_transforms = 0;
	m_pitch = 0;
	m_reader = NULL;
	m_allocator = NULL;
}

PngCodec::~PngCodec()
{
}

static png_voidp PngMalloc(png_structp png, png_alloc_size_t size)
{
	HeapAllocator *allocator = (HeapAllocator *)png_get_mem_ptr(png);
	if (allocator != NULL)
		return allocator->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

static void PngFree(png_structp png, png_voidp ptr)
{
	HeapAllocator *allocator = (HeapAllocator *)png_get_mem_ptr(png);
	if (allocator != NULL)
		allocator->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}

static void ReadFile(png_structp png, png_bytep data, png_size_t length)
{
	File *file = (File *)png_get_io_ptr(png);
	if (file->Read(data, length) != length)
		png_error(png, "Read Error");
}

static int64_t SeekFile(PngReader *reader, int64_t offset, int32_t whence)
{
	if (reader == NULL)
		return 0;
	return ((File *)reader->m_data)->Seek(offset, whence);
}

static void ReadBuffer(png_structp png, png_bytep data, png_size_t length)
{
	PngReader *reader = (PngReader *)png_get_io_ptr(png);
	if (reader->m_size - reader->m_pos < (int64_t)length)
		png_error(png, "Read Error");
	sce_paf_memcpy(data, reader->m_data, length);
	reader->m_data = (uint8_t *)reader->m_data + length;
	reader->m_pos += length;
}

static void WriteFile(png_structp png, png_bytep data, png_size_t length)
{
	File *file = (File *)png_get_io_ptr(png);
	if (file->Write(data, length) != length)
		png_error(png, "Write Error");
}

static void FlushFile(png_structp png)
{
	File *file = (File *)png_get_io_ptr(png);
	if (file->Flush() != 0)
		png_error(png, "Flush Error");
}

static void WriteBuffer(png_structp png, png_bytep data, png_size_t length)
{
	PngWriteBuffer *buffer = (PngWriteBuffer *)png_get_io_ptr(png);
	if (buffer->written + length > buffer->size)
		png_error(png, "Write Error");
	sce_paf_memcpy(buffer->data, data, length);
	buffer->data += length;
	buffer->written += length;
}
