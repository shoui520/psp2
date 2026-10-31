/*
 * DDS: the format's attribute, its check, the reader of DXT1, DXT3, DXT5 and
 * uncompressed 32, 24 and 8-bit images in memory (the decoded image is the
 * file's own data) and the writer of DXT and 32-bit images.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <file/localfile.h>
#include <memory/heap_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "image_codec.h"
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

#define DDS_FOURCC(a, b, c, d) \
	((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)

/* DdsHeader flags */
#define DDSD_CAPS		0x1
#define DDSD_HEIGHT		0x2
#define DDSD_WIDTH		0x4
#define DDSD_PITCH		0x8
#define DDSD_PIXELFORMAT	0x1000
#define DDSD_MIPMAPCOUNT	0x20000
#define DDSD_LINEARSIZE		0x80000

/* DdsPixelFormat flags */
#define DDPF_ALPHAPIXELS	0x1
#define DDPF_ALPHA		0x2
#define DDPF_FOURCC		0x4
#define DDPF_RGB		0x40
#define DDPF_LUMINANCE		0x20000

#define DDSCAPS_COMPLEX		0x8
#define DDSCAPS_TEXTURE		0x1000
#define DDSCAPS_MIPMAP		0x400000

#define DDSCAPS2_CUBEMAP	0x200
#define DDSCAPS2_VOLUME		0x200000

/* What a file holds: one image, a cube map's six faces, or a volume. */
#define DDS_TEXTURE_2D		1
#define DDS_TEXTURE_VOLUME	2
#define DDS_TEXTURE_CUBE	3

namespace paf {
namespace image {

struct DdsPixelFormat {
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t RGBBitCount;
	uint32_t RBitMask;
	uint32_t GBitMask;
	uint32_t BBitMask;
	uint32_t ABitMask;
};

/* The header after the "DDS " signature. */
struct DdsHeader {
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	DdsPixelFormat ddspf;
	uint32_t caps;
	uint32_t caps2;
	uint32_t caps3;
	uint32_t caps4;
	uint32_t reserved2;
};

}
}

static inline bool IsDxt(ImageMode mode)
{
	return mode == ImageMode_DXT1 || mode == ImageMode_DXT3 || mode == ImageMode_DXT5;
}

size_t DdsInfo::GetSurfaceSize(uint32_t w, uint32_t h, ImageMode mode)
{
	switch (mode) {
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_ARGB8888:
	case ImageMode_BGRA8888:
		return w * h * 4;
	case ImageMode_A8:
	case ImageMode_L8:
		return w * h;
	case ImageMode_DXT1:
		return ((w + 3) >> 2) * ((h + 3) >> 2) * 8;
	case ImageMode_DXT3:
	case ImageMode_DXT5:
		return ((w + 3) >> 2) * ((h + 3) >> 2) * 16;
	case ImageMode_BGR888:
	case ImageMode_RGB888:
		return w * h * 3;
	default:
		return 0;
	}
}

bool DdsInfo::GetFormat(const DdsHeader *header, ImageMode *mode, int32_t *channels)
{
	const DdsPixelFormat *pf = &header->ddspf;

	if (pf->flags & DDPF_FOURCC) {
		switch (pf->fourCC) {
		case DDS_FOURCC('D', 'X', 'T', '5'):
			*mode = ImageMode_DXT5;
			*channels = 4;
			break;
		case DDS_FOURCC('D', 'X', 'T', '3'):
			*mode = ImageMode_DXT3;
			*channels = 4;
			break;
		case DDS_FOURCC('D', 'X', 'T', '1'):
			*mode = ImageMode_DXT1;
			*channels = 3;
			break;
		default:
			error = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
			return false;
		}
	} else if (pf->flags == (DDPF_RGB | DDPF_ALPHAPIXELS) && pf->RGBBitCount == 32) {
		if (pf->RBitMask == 0xff && pf->GBitMask == 0xff00 && pf->BBitMask == 0xff0000 &&
		    pf->ABitMask == 0xff000000) {
			*mode = ImageMode_RGBA8888;
			*channels = 4;
		} else if (pf->RBitMask == 0xff0000 && pf->GBitMask == 0xff00 &&
			   pf->BBitMask == 0xff && pf->ABitMask == 0xff000000) {
			*mode = ImageMode_BGRA8888;
			*channels = 4;
		} else {
			error = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
			return false;
		}
	} else if (pf->flags == DDPF_RGB && pf->RGBBitCount == 24) {
		*mode = ImageMode_RGB888;
		*channels = 3;
	} else if (pf->flags == DDPF_ALPHA && pf->RGBBitCount == 8) {
		*mode = ImageMode_A8;
		*channels = 1;
	} else if (pf->flags == DDPF_LUMINANCE && pf->RGBBitCount == 8) {
		*mode = ImageMode_L8;
		*channels = 1;
	} else {
		error = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
		return false;
	}
	return true;
}

DdsInfo::DdsInfo(common::SharedPtr<Buffer> buf)
{
	*this = DdsInfo();
	const uint8_t *addr = (const uint8_t *)buf->GetAddr();
	if (sce_paf_strncmp((const char *)addr, "DDS ", 4) != 0)
		return;

	DdsHeader header;
	sce_paf_memcpy(&header, addr + 4, sizeof(header));
	const uint8_t *surface_data = addr + 4 + sizeof(header);

	int32_t type = DDS_TEXTURE_2D;
	if (header.caps2 & DDSCAPS2_CUBEMAP)
		type = DDS_TEXTURE_CUBE;
	if ((header.caps2 & DDSCAPS2_VOLUME) && header.depth != 0) {
		/* volumes are not read */
		*this = DdsInfo();
		return;
	}

	if (GetFormat(&header, &mode, &channels)) {
		width = header.width;
		height = header.height;
		data = addr;
		if (header.mipMapCount == 0)
			header.mipMapCount++;
		mipcount = header.mipMapCount;
		facecount = 1;
		if (type == DDS_TEXTURE_CUBE)
			facecount = 6;

		for (uint32_t face = 0; face < facecount; face++) {
			uint32_t w = width;
			uint32_t h = height;
			for (uint32_t i = 0; i < header.mipMapCount; i++) {
				surface[face].mip[i] = surface_data;
				surface_data += GetSurfaceSize(w, h, mode);
				w >>= 1;
				h >>= 1;
			}
		}
	}
}

bool DdsAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool DdsAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool DdsAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool DdsAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

DdsAttribute::DdsAttribute()
{
}

DdsAttribute::~DdsAttribute()
{
}

bool paf::image::IsDds(common::SharedPtr<Buffer> buf)
{
	if (buf->GetAddr() == NULL || buf->GetSize() < 4)
		return false;

	const char signature[4] = { 'D', 'D', 'S', ' ' };
	return sce_paf_memcmp(signature, buf->GetAddr(), 4) == 0;
}

bool DdsCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	return false;
}

bool DdsCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	return false;
}

/* The images are the file's own mipmap levels, not copies. */
bool DdsCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || images == NULL || attr == NULL)
		return false;

	Image::SetErrorType(0);
	m_info = DdsInfo(buf);
	if (m_info.error != 0)
		return false;

	if (!(images->GetArraySize() == ImagePoint(m_info.mipcount, 1))) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}
	if (m_info.facecount != 1) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	uint32_t w = m_info.width;
	uint32_t h = m_info.height;
	for (uint32_t i = 0; i < images->GetMax(); i++) {
		ImageBuffer *image = images->GetBuffer(i);
		image->buff = (void *)m_info.surface[0].mip[i];
		image->size = DdsInfo::GetSurfaceSize(w, h, m_info.mode);
		w = w == 1 ? 1 : w >> 1;
		h = h == 1 ? 1 : h >> 1;
	}
	return true;
}

int32_t paf::image::SaveDds(const char *filename, const void *addr, uint32_t pitch,
			    const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			    HeapAllocator *allocator)
{
	if (addr == NULL || ext.w <= 0 || ext.h <= 0 ||
	    (mode != ImageMode_DXT1 && mode != ImageMode_DXT3 && mode != ImageMode_DXT5 &&
	     mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return 0;
	if (IsDxt(mode) && (ext.w % 4 != 0 || ext.h % 4 != 0))
		return 0;

	int32_t error;
	common::SharedPtr<File> file = LocalFile::Open(filename, 0x10602, io_mode, &error);
	if (file.get() == NULL)
		return 0;
	if (file->IsOpened()) {
		const char signature[4] = { 'D', 'D', 'S', ' ' };
		if (file->Write(signature, 4) != 4) {
			file->Close();
			return 0;
		}

		DdsHeader header = { 0 };
		header.size = sizeof(header);
		header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
		header.height = ext.h;
		header.width = ext.w;
		header.depth = 0;
		header.mipMapCount = 1;
		header.ddspf.size = sizeof(header.ddspf);
		if (IsDxt(mode)) {
			header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT |
				       DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
			header.ddspf.flags = DDPF_FOURCC;
			header.pitchOrLinearSize = DdsInfo::GetSurfaceSize(pitch, ext.h, mode);
			if (mode == ImageMode_DXT1)
				header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '1');
			else if (mode == ImageMode_DXT3)
				header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '3');
			else if (mode == ImageMode_DXT5)
				header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '5');
		} else if (mode == ImageMode_RGBA8888 || mode == ImageMode_BGRA8888) {
			header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH |
				       DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
			header.ddspf.flags = DDPF_RGB | DDPF_ALPHAPIXELS;
			header.pitchOrLinearSize = ext.w * 4;
			header.ddspf.RGBBitCount = 32;
			if (mode == ImageMode_RGBA8888) {
				header.ddspf.RBitMask = 0xff;
				header.ddspf.GBitMask = 0xff00;
				header.ddspf.BBitMask = 0xff0000;
				header.ddspf.ABitMask = 0xff000000;
			} else {
				header.ddspf.RBitMask = 0xff0000;
				header.ddspf.GBitMask = 0xff00;
				header.ddspf.BBitMask = 0xff;
				header.ddspf.ABitMask = 0xff000000;
			}
		}
		header.caps = DDSCAPS_COMPLEX | DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
		if (file->Write(&header, sizeof(header)) != sizeof(header)) {
			file->Close();
			return 0;
		}

		if (IsDxt(mode)) {
			size_t size = DdsInfo::GetSurfaceSize(pitch, ext.h, mode);
			if (file->Write(addr, size) != size) {
				file->Close();
				return 0;
			}
		} else if (mode == ImageMode_RGBA8888 || mode == ImageMode_BGRA8888) {
			size_t size = pitch * ext.h * 4;
			uint8_t *data;
			if (allocator != NULL)
				data = (uint8_t *)allocator->sce_paf_malloc(size);
			else
				data = (uint8_t *)sce_paf_malloc(size);
			if (data == NULL) {
				file->Close();
				return 0;
			}

			/* the rows without the pitch's padding */
			uint8_t *dst = data;
			const uint8_t *src = (const uint8_t *)addr;
			if (mode == ImageMode_RGBA8888) {
				for (int32_t y = 0; y < ext.h; y++) {
					uint8_t *d = dst;
					for (int32_t x = 0; x < ext.w; x++) {
						const uint8_t *s = &src[x * 4 + y * 4 * pitch];
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
						d += 4;
					}
					dst += ext.w * 4;
				}
			} else {
				for (int32_t y = 0; y < ext.h; y++) {
					uint8_t *d = dst;
					for (int32_t x = 0; x < ext.w; x++) {
						const uint8_t *s = &src[x * 4 + y * 4 * pitch];
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
						d += 4;
					}
					dst += ext.w * 4;
				}
			}

			if (file->Write(data, size) != size) {
				if (allocator != NULL)
					allocator->sce_paf_free(data);
				else
					sce_paf_free(data);
				file->Close();
				return 0;
			}
			if (allocator != NULL)
				allocator->sce_paf_free(data);
			else
				sce_paf_free(data);
		}
		file->Close();
	}
	return 1;
}

int32_t paf::image::SaveDds(void *dst, uint32_t max_size, const void *addr, uint32_t pitch,
			    const ImageExtent &ext, ImageMode mode, uint32_t io_mode)
{
	if (dst == NULL || addr == NULL || ext.w <= 0 || ext.h <= 0 ||
	    (mode != ImageMode_DXT1 && mode != ImageMode_DXT3 && mode != ImageMode_DXT5 &&
	     mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return 0;
	if (IsDxt(mode) && (ext.w % 4 != 0 || ext.h % 4 != 0))
		return 0;
	size_t size = DdsInfo::GetSurfaceSize(pitch, ext.h, mode);
	if (max_size < size + 4 + sizeof(DdsHeader))
		return 0;

	const char signature[4] = { 'D', 'D', 'S', ' ' };
	sce_paf_memcpy(dst, signature, 4);

	DdsHeader header = { 0 };
	header.size = sizeof(header);
	header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
	header.height = ext.h;
	header.width = ext.w;
	header.pitchOrLinearSize = 0;
	header.depth = 0;
	header.mipMapCount = 1;
	header.ddspf.size = sizeof(header.ddspf);
	if (IsDxt(mode)) {
		header.ddspf.flags = DDPF_FOURCC;
		if (mode == ImageMode_DXT1)
			header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '1');
		else if (mode == ImageMode_DXT3)
			header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '3');
		else if (mode == ImageMode_DXT5)
			header.ddspf.fourCC = DDS_FOURCC('D', 'X', 'T', '5');
	} else if (mode == ImageMode_RGBA8888 || mode == ImageMode_BGRA8888) {
		header.ddspf.flags = DDPF_RGB | DDPF_ALPHAPIXELS;
		header.ddspf.RGBBitCount = 32;
		if (mode == ImageMode_RGBA8888) {
			header.ddspf.RBitMask = 0xff;
			header.ddspf.GBitMask = 0xff00;
			header.ddspf.BBitMask = 0xff0000;
			header.ddspf.ABitMask = 0xff000000;
		} else {
			header.ddspf.RBitMask = 0xff0000;
			header.ddspf.GBitMask = 0xff00;
			header.ddspf.BBitMask = 0xff;
			header.ddspf.ABitMask = 0xff000000;
		}
	}
	header.caps = DDSCAPS_COMPLEX | DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
	sce_paf_memcpy((uint8_t *)dst + 4, &header, sizeof(header));
	sce_paf_memcpy((uint8_t *)dst + 4 + sizeof(header), addr, size);
	return 1;
}

DdsCodec::DdsCodec()
{
}

DdsCodec::~DdsCodec()
{
}

bool DdsCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	return false;
}

bool DdsCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr, ImageArray *images,
			 ImageArray *cluts)
{
	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL)
		return false;

	m_info = DdsInfo(buf);
	if (m_info.error != 0 || m_info.mode == ImageMode_NONE)
		return false;

	attr->m_mode = m_info.mode;
	attr->m_clutmode = ImageMode_NONE;
	attr->m_extent = ImageExtent(m_info.width, m_info.height);
	attr->m_format = ImageFormat_Dds;
	attr->m_xtype = ImageXType_None;
	attr->m_ytype = ImageYType_None;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;
	attr->m_resizetype = ImageResizeType_None;
	attr->m_order = ImageOrder_Linear;
	attr->m_clutsize = 0;
	images->Init(m_info.mipcount, 1);
	return true;
}
