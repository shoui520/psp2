/*
 * GXT: the format's attribute, its check and the reader of its first texture
 * in memory (the decoded image is the file's own texture data).
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <gxt.h>
#include "image_codec.h"
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;

bool GxtAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool GxtAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool GxtAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool GxtAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

GxtAttribute::GxtAttribute()
{
}

GxtAttribute::~GxtAttribute()
{
}

bool paf::image::IsGxt(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 4)
		return false;

	const char signature[4] = { 'G', 'X', 'T', '\0' };
	if (sce_paf_memcmp(signature, buf->GetAddr(), 4) != 0)
		return false;

	return sceGxtCheckData(buf->GetAddr()) == SCE_OK;
}

bool GxtCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	return false;
}

bool GxtCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	return false;
}

/* The header of a file of one texture, and where its texture data starts. */
bool GxtCodec::FindBlocks(common::SharedPtr<Buffer> buf, const void **image,
			  const void **palette)
{
	const SceGxtHeader *header = (const SceGxtHeader *)buf->GetAddr();
	if (header->tag != SCE_GXT_TAG)
		return false;

	m_header.tag = header->tag;
	m_header.version = header->version;
	m_header.numTextures = sceGxtGetTextureCount(buf->GetAddr());
	m_header.dataOffset = sceGxtGetHeaderSize(buf->GetAddr());
	m_header.dataSize = sceGxtGetDataSize(buf->GetAddr());
	*image = sceGxtGetDataAddress(buf->GetAddr());
	return m_header.numTextures == 1;
}

/* The images are the file's own texture data, not copies. */
bool GxtCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	const void *image = NULL;
	const void *palette = NULL;

	bool found = FindBlocks(buf, &image, &palette);
	if (!found) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return found;
	}

	if (image != NULL) {
		SceGxmTexture texture;
		sceGxtInitTexture(&texture, buf->GetAddr(), image, 0);
		if (!(images->GetArraySize() ==
		      ImagePoint(sceGxmTextureGetMipmapCount(&texture), m_header.numTextures))) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}
		for (uint32_t i = 0; i < images->GetMax(); i++) {
			ImageBuffer *buffer = images->GetBuffer(i);
			buffer->buff = (void *)image;
		}
	}

	if (palette != NULL) {
		if (!(cluts->GetArraySize() == ImagePoint(1, 1))) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}
		for (uint32_t i = 0; i < cluts->GetMax(); i++) {
			ImageBuffer *buffer = cluts->GetBuffer(i);
			buffer->buff = (void *)palette;
		}
	}
	return true;
}

GxtCodec::GxtCodec()
{
}

GxtCodec::~GxtCodec()
{
}

bool GxtCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	return false;
}

ImageMode GxtCodec::GetImageMode(SceGxmTextureFormat format)
{
	ImageMode mode = ImageMode_NONE;

	switch (format) {
	case SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR:
		return ImageMode_DXT5;
	case SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR:
		return ImageMode_DXT3;
	case SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR:
		return ImageMode_DXT1;
	case SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_ABGR:
		return ImageMode_PVRTII4BPP;
	case SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR:
		return ImageMode_PVRTII2BPP;
	case SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR:
		return ImageMode_PVRT4BPP;
	case SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR:
		return ImageMode_PVRT2BPP;
	case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB:
		return ImageMode_BGRA8888;
	case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
		return ImageMode_RGBA8888;
	case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ARGB:
	case SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ARGB:
		break;
	case SCE_GXM_TEXTURE_FORMAT_U8_R000:
		return ImageMode_A8;
	case SCE_GXM_TEXTURE_FORMAT_U8_1RRR:
		return ImageMode_L8;
	default:
		break;
	}
	return mode;
}

/* A linear or swizzled texture's rows are whole pixels; the others any width. */
size_t GxtCodec::GetPitchAlign(SceGxmTextureFormat format, SceGxmTextureType type)
{
	size_t align = 1;

	switch (type) {
	case SCE_GXM_TEXTURE_LINEAR_STRIDED:
	case SCE_GXM_TEXTURE_TILED:
		break;
	case SCE_GXM_TEXTURE_LINEAR:
		return ImageAttribute::GetPixelSize(GetImageMode(format));
	case SCE_GXM_TEXTURE_CUBE:
		break;
	case SCE_GXM_TEXTURE_SWIZZLED:
		return ImageAttribute::GetPixelSize(GetImageMode(format));
	default:
		break;
	}
	return align;
}

ImageOrder GxtCodec::GetImageOrder(SceGxmTextureType type)
{
	ImageOrder order = (ImageOrder)0;

	switch (type) {
	case SCE_GXM_TEXTURE_LINEAR_STRIDED:
		break;
	case SCE_GXM_TEXTURE_TILED:
		return ImageOrder_Tiled;
	case SCE_GXM_TEXTURE_LINEAR:
		return ImageOrder_Linear;
	case SCE_GXM_TEXTURE_CUBE:
		break;
	case SCE_GXM_TEXTURE_SWIZZLED:
		return ImageOrder_Swizzle;
	default:
		break;
	}
	return order;
}

/* attr from the texture's own description. */
bool GxtCodec::ReadHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr, const void *image,
			  const void *palette, ImageArray *images, ImageArray *cluts)
{
	if (image != NULL) {
		SceGxmTexture texture;
		sceGxtInitTexture(&texture, buf->GetAddr(), image, 0);
		attr->m_extent.w = sceGxmTextureGetWidth(&texture);
		attr->m_extent.h = sceGxmTextureGetHeight(&texture);
		attr->m_clip = ImageRect(0, 0, attr->m_extent.w, attr->m_extent.h);
		attr->m_format = ImageFormat_Gxt;
		attr->m_mode = GetImageMode(sceGxmTextureGetFormat(&texture));
		attr->m_order = GetImageOrder(sceGxmTextureGetType(&texture));
		attr->m_xtype = ImageXType_None;
		attr->m_ytype = ImageYType_None;
		attr->m_pitchalign = GetPitchAlign(sceGxmTextureGetFormat(&texture),
						   sceGxmTextureGetType(&texture));
		attr->m_heightalign = 1;
		images->Init(sceGxmTextureGetMipmapCount(&texture), m_header.numTextures);
	}

	if (palette != NULL) {
		attr->m_clutsize = 0;
		attr->m_clutmode = ImageMode_RGBA8888;
		cluts->Init(1, 1);
	}
	return true;
}

bool GxtCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL)
		return false;

	const void *image = NULL;
	const void *palette = NULL;

	bool found = FindBlocks(buf, &image, &palette);
	if (!found) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return found;
	}

	bool read = ReadHeader(buf, attr, image, palette, images, cluts);
	if (!read) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return read;
	}
	return true;
}
