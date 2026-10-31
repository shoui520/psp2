/*
 * paf::image::GifCodec: GIF images through giflib, decoded whole into 8-bit
 * indices with an RGBA palette. Only the first image of the file is read.
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

/* The rows of each pass of an interlaced image: the first row and the step. */
static int InterlacedOffset[] = { 0, 4, 2, 1 };
static int InterlacedJumps[] = { 8, 8, 4, 2 };

static void *GifMalloc(void *heap, size_t size);
static void GifFree(void *heap, void *ptr);
static void *GifRealloc(void *heap, void *ptr, size_t size);
static void *GifCalloc(void *heap, size_t nitems, size_t size);

bool GifAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool GifAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool GifAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool GifAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

GifAttribute::GifAttribute()
{
}

GifAttribute::~GifAttribute()
{
}

bool paf::image::IsGif(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 6)
		return false;

	const char gif87a[6] = { 'G', 'I', 'F', '8', '7', 'a' };
	const char gif89a[6] = { 'G', 'I', 'F', '8', '9', 'a' };
	const void *addr = buf->GetAddr();
	bool result = false;
	if (sce_paf_memcmp(gif87a, addr, 6) == 0 || sce_paf_memcmp(gif89a, addr, 6) == 0)
		result = true;
	return result;
}

/* The palette as RGBA, opaque but for the transparent index; the rest black. */
bool GifCodec::MakeCLUT(uint8_t *dst, const ColorMapObject *map, int32_t transparent,
			int32_t count)
{
	if (dst == NULL || map == NULL || count == 0)
		return false;

	uint8_t *clut = dst;
	int32_t i = 0;
	for (; i < map->ColorCount; i++) {
		clut[0] = map->Colors[i].Red;
		clut[1] = map->Colors[i].Green;
		clut[2] = map->Colors[i].Blue;
		clut[3] = 0xff;
		clut += 4;
	}
	for (; i < count; i++) {
		clut[0] = 0;
		clut[1] = 0;
		clut[2] = 0;
		clut[3] = 0xff;
		clut += 4;
	}
	if (transparent >= 0 && count >= transparent)
		dst[transparent * 4 + 3] = 0;
	return true;
}

bool GifCodec::DecodeImage(GifFileType *gif, const ImageBuffer *image,
			   const ImageAttribute *attr, int32_t *transparent,
			   DecodeCancelFunc cancelfunc, void *userdata)
{
	if (gif == NULL || image == NULL || attr == NULL)
		return false;

	size_t pitch = attr->GetMemPitch(0);
	uint8_t *dst = (uint8_t *)image->buff;
	bool more = true;
	bool result = true;
	sce_paf_memset(dst, gif->SBackGroundColor, image->size);

	GifRecordType type;
	int code;
	GifByteType *ext;
	do {
		if (DGifGetRecordType(gif, &type) == GIF_ERROR)
			return false;

		switch (type) {
		case IMAGE_DESC_RECORD_TYPE: {
			if (DGifGetImageDesc(gif) == GIF_ERROR)
				return false;

			int left = gif->Image.Left;
			int width = gif->Image.Width;
			int top = gif->Image.Top;
			int height = gif->Image.Height;
			if (left + width > gif->SWidth || top + height > gif->SHeight)
				return false;

			if (gif->Image.Interlace) {
				for (int i = 0; i < 4; i++) {
					for (int y = top + InterlacedOffset[i]; y < top + height;
					     y += InterlacedJumps[i]) {
						if (cancelfunc != NULL && cancelfunc(userdata)) {
							Image::SetErrorType(
								SCE_PAF_ERROR_IMAGE_CANCELED);
							return false;
						}
						if (DGifGetLine(gif, dst + y * pitch + left, width) ==
						    GIF_ERROR)
							return false;
					}
				}
			} else {
				int y = top;
				uint8_t *line = dst + top * pitch + left;
				for (; y < top + height; y++) {
					if (cancelfunc != NULL && cancelfunc(userdata)) {
						Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
						return false;
					}
					if (DGifGetLine(gif, line, width) == GIF_ERROR)
						return false;
					line += pitch;
				}
			}
			more = false;
			break;
		}
		case EXTENSION_RECORD_TYPE:
			if (DGifGetExtension(gif, &code, &ext) == GIF_ERROR)
				return false;
			if (code == GRAPHICS_EXT_FUNC_CODE) {
				if (ext == NULL || ext[0] != 4)
					return false;
				if (ext[1] & 1)
					*transparent = ext[4];
			}
			while (ext != NULL) {
				if (DGifGetExtensionNext(gif, &ext) == GIF_ERROR)
					return false;
			}
			break;
		default:
			break;
		}
	} while (type != TERMINATE_RECORD_TYPE && more);

	return result;
}

bool GifCodec::Decode(GifFileType *gif, const ImageAttribute *attr, ImageArray *images,
		      ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata)
{
	if (cluts == NULL)
		return false;
	uint8_t *clut = (uint8_t *)cluts->GetBuffer(0)->buff;
	if (clut == NULL)
		return false;

	int32_t transparent = -1;
	if (!DecodeImage(gif, images->GetBuffer(0), attr, &transparent, cancelfunc, userdata))
		return false;

	ColorMapObject *map = gif->Image.ColorMap;
	if (map == NULL)
		map = gif->SColorMap;
	if (map == NULL)
		return false;
	switch (map->BitsPerPixel) {
	case 1:
		if (map->ColorCount != 2) {
			return false;
		}
		break;
	case 2:
		if (map->ColorCount != 4) {
			return false;
		}
		break;
	case 3:
		if (map->ColorCount != 8) {
			return false;
		}
		break;
	case 4:
		if (map->ColorCount != 16) {
			return false;
		}
		break;
	case 5:
		if (map->ColorCount != 32) {
			return false;
		}
		break;
	case 6:
		if (map->ColorCount != 64) {
			return false;
		}
		break;
	case 7:
		if (map->ColorCount != 128) {
			return false;
		}
		break;
	case 8:
		if (map->ColorCount != 256) {
			return false;
		}
		break;
	default:
		return false;
	}
	if (!MakeCLUT(clut, map, transparent, attr->m_clutsize))
		return false;
	return true;
}

GifFileType *GifCodec::OpenFile(common::SharedPtr<File> file, int64_t offset, size_t size)
{
	m_source.data = file.get();
	m_source.size = file->GetFileSize();
	m_source.pos = 0;
	m_source.read = ReadFile;
	m_source.seek = SeekFile;
	m_source.malloc = GifMalloc;
	m_source.free = GifFree;
	m_source.realloc = GifRealloc;
	m_source.calloc = GifCalloc;

	if (m_source.seek(&m_source, 0, SCE_SEEK_SET) < 0)
		return NULL;
	if (offset != 0 && m_source.seek(&m_source, offset, SCE_SEEK_SET) < 0)
		return NULL;

	GifFileType *gif = DGifOpen(m_source.data, m_source.allocator, m_source.read,
				    m_source.malloc, m_source.free, m_source.realloc,
				    m_source.calloc);
	if (gif == NULL)
		return NULL;
	return gif;
}

bool GifCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
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
	if (image->buff == NULL || image->size < attr->m_extent.w * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < attr->m_clutsize * 4) {
			m_mutex.Unlock();
			return false;
		}
	}

	GifFileType *gif = OpenFile(file, offset, size);
	if (gif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(gif, attr, images, cluts, cancelfunc, userdata)) {
		DGifCloseFile(gif);
		m_mutex.Unlock();
		return false;
	}
	DGifCloseFile(gif);
	m_mutex.Unlock();
	return true;
}

GifFileType *GifCodec::OpenBuffer(common::SharedPtr<Buffer> buf)
{
	m_source.read = ReadBuffer;
	m_source.seek = NULL;
	m_source.malloc = GifMalloc;
	m_source.free = GifFree;
	m_source.realloc = GifRealloc;
	m_source.calloc = GifCalloc;
	m_source.data = buf->GetAddr();
	m_source.size = buf->GetSize();
	m_source.pos = 0;

	GifFileType *gif = DGifOpen(&m_source, m_source.allocator, m_source.read, m_source.malloc,
				    m_source.free, m_source.realloc, m_source.calloc);
	if (gif == NULL)
		return NULL;
	return gif;
}

bool GifCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
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
	if (image->buff == NULL || image->size < attr->m_extent.w * attr->m_extent.h) {
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1) {
			m_mutex.Unlock();
			return false;
		}
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < attr->m_clutsize * 4) {
			m_mutex.Unlock();
			return false;
		}
	}

	GifFileType *gif = OpenBuffer(buf);
	if (gif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!Decode(gif, attr, images, cluts, cancelfunc, userdata)) {
		DGifCloseFile(gif);
		m_mutex.Unlock();
		return false;
	}
	DGifCloseFile(gif);
	m_mutex.Unlock();
	return true;
}

bool GifCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	return false;
}

bool GifCodec::ReadHeader(GifFileType *gif, ImageAttribute *attr, ImageArray *images,
			  ImageArray *cluts)
{
	attr->m_extent = ImageExtent(gif->SWidth, gif->SHeight);
	attr->m_clip = ImageRect(0, 0, gif->SWidth, gif->SHeight);
	attr->m_order = ImageOrder_Linear;
	attr->m_format = ImageFormat_Gif;
	attr->m_xtype = ImageXType_ConstantSize;
	attr->m_ytype = ImageYType_Sequence;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;

	const ColorMapObject *map = gif->SColorMap;
	if (map != NULL) {
		switch (map->BitsPerPixel) {
		case 1:
			if (map->ColorCount != 2) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 2:
			if (map->ColorCount != 4) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 3:
			if (map->ColorCount != 8) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 4:
			if (map->ColorCount != 16) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 5:
			if (map->ColorCount != 32) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 6:
			if (map->ColorCount != 64) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 7:
			if (map->ColorCount != 128) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		case 8:
			if (map->ColorCount != 256) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
				return false;
			}
			break;
		default:
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
			return false;
		}
	}

	attr->m_clutsize = 256;
	attr->m_mode = ImageMode_index8;
	SetLoadType(LoadType_Image);
	images->Init(1, 1);
	cluts->Init(1, 1);
	attr->m_clutmode = ImageMode_RGBA8888;
	return true;
}

bool GifCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	m_source.allocator = images->GetHeapAllocator();

	GifFileType *gif = OpenFile(file, offset, size);
	if (gif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(gif, attr, images, cluts)) {
		DGifCloseFile(gif);
		m_mutex.Unlock();
		return false;
	}
	DGifCloseFile(gif);
	m_mutex.Unlock();
	return true;
}

bool GifCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	m_source.allocator = images->GetHeapAllocator();

	GifFileType *gif = OpenBuffer(buf);
	if (gif == NULL) {
		m_mutex.Unlock();
		return false;
	}
	if (!ReadHeader(gif, attr, images, cluts)) {
		DGifCloseFile(gif);
		m_mutex.Unlock();
		return false;
	}
	DGifCloseFile(gif);
	m_mutex.Unlock();
	return true;
}

GifCodec::GifCodec() : m_mutex("ScePafImageGifMutex", 0)
{
}

GifCodec::~GifCodec()
{
}

int GifCodec::ReadFile(GifFileType *gif, GifByteType *buf, int len)
{
	if (gif == NULL || buf == NULL)
		return 0;
	return ((File *)gif->UserData)->Read(buf, len);
}

int64_t GifCodec::SeekFile(Source *src, int64_t offset, int32_t whence)
{
	if (src == NULL)
		return 0;
	return ((File *)src->data)->Seek(offset, whence);
}

int GifCodec::ReadBuffer(GifFileType *gif, GifByteType *buf, int len)
{
	if (gif == NULL || buf == NULL)
		return 0;

	Source *src = (Source *)gif->UserData;
	int64_t remaining = src->size - src->pos;
	size_t n = len;
	if (remaining <= n) {
		n = remaining;
		if (n == 0)
			return 0;
	}
	sce_paf_memcpy(buf, src->data, n);
	src->data = (uint8_t *)src->data + n;
	src->pos += n;
	return n;
}

static void *GifMalloc(void *heap, size_t size)
{
	if (heap != NULL)
		return ((HeapAllocator *)heap)->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

static void GifFree(void *heap, void *ptr)
{
	if (heap != NULL)
		((HeapAllocator *)heap)->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}

static void *GifRealloc(void *heap, void *ptr, size_t size)
{
	if (heap != NULL)
		return ((HeapAllocator *)heap)->sce_paf_realloc(ptr, size);
	return ScePafStdc_0ADEA3AD(ptr, size);
}

static void *GifCalloc(void *heap, size_t nitems, size_t size)
{
	if (heap != NULL)
		return ((HeapAllocator *)heap)->sce_paf_calloc(nitems, size);
	return ScePafStdc_C9532982(nitems, size);
}
