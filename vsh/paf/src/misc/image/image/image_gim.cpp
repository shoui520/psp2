/*
 * paf::image::GimCodec: GIM images. A GIM file is a tree of blocks; the codec
 * takes the first picture's image and palette and points the image arrays at
 * their levels and frames in the buffer, without decoding or copying them.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include "image_codec.h"
#include "image_source.h"
#include "../../../paf_types.h"

using namespace paf;
using namespace paf::image;

/* The file's signature: "MIG.00.1PSP". */
#define GIM_SIGNATURE		0x2e47494d	/* "MIG." */
#define GIM_VERSION		0x312e3030	/* "00.1" */
#define GIM_STYLE		0x00505350	/* "PSP" */

/* The blocks of the tree. */
#define GIM_BLOCK_ROOT		2
#define GIM_BLOCK_PICTURE	3
#define GIM_BLOCK_IMAGE		4
#define GIM_BLOCK_PALETTE	5

struct GimHeader {
	uint32_t signature;
	uint32_t version;
	uint32_t style;
	uint32_t option;
};

/* A block; next, child and data are byte offsets from the block. */
struct GimBlock {
	uint16_t type;
	uint16_t unused;
	uint32_t next;
	uint32_t child;
	uint32_t data;
};

/* The data of an image or palette block; offsets from the header. */
struct paf::image::GimImageHeader {
	uint16_t header_size;
	uint16_t reference;
	uint16_t format;
	uint16_t order;
	uint16_t width;
	uint16_t height;
	uint16_t bpp;
	uint16_t pitch_align;
	uint16_t height_align;
	uint16_t dim_count;
	uint16_t reserved;
	uint16_t reserved2;
	uint32_t offsets;	/* the table of each level's and frame's offset */
	uint32_t images;
	uint32_t total;
	uint32_t plane_mask;
	uint16_t level_type;
	uint16_t level_count;
	uint16_t frame_type;
	uint16_t frame_count;
};

bool GimAttribute::GetAttribute(uint32_t key, bool &value)
{
	return false;
}

bool GimAttribute::GetAttribute(uint32_t key, int &value)
{
	return false;
}

bool GimAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool GimAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

GimAttribute::GimAttribute()
{
}

GimAttribute::~GimAttribute()
{
}

bool paf::image::IsGim(common::SharedPtr<Buffer> buf)
{
	if (buf->GetAddr() == NULL || buf->GetSize() < sizeof(GimHeader))
		return false;

	const GimHeader *header = (const GimHeader *)buf->GetAddr();
	if (header->signature != GIM_SIGNATURE)
		return false;
	if (header->version == GIM_VERSION)
		return header->style == GIM_STYLE;
	return false;
}

bool GimCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	return false;
}

bool GimCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	return false;
}

/* The image and palette of the file's first picture, NULL where it has none. */
bool GimCodec::FindBlocks(common::SharedPtr<Buffer> buf, const GimImageHeader **image,
			  const GimImageHeader **palette)
{
	const uint8_t *file = (const uint8_t *)buf->GetAddr();
	const GimBlock *root = (const GimBlock *)(file + sizeof(GimHeader));
	if (root->type != GIM_BLOCK_ROOT)
		return false;
	if (buf->GetSize() < root->next + sizeof(GimHeader))
		return false;

	const uint8_t *end = (const uint8_t *)root + root->next;
	const uint8_t *picture = (const uint8_t *)root + root->child;
	int32_t index = 0;
	for (; picture < end; picture += ((const GimBlock *)picture)->next) {
		if (((const GimBlock *)picture)->type == GIM_BLOCK_PICTURE && index-- == 0)
			break;
	}
	if (picture == end)
		return false;

	*image = NULL;
	*palette = NULL;
	const GimBlock *block = (const GimBlock *)picture;
	const uint8_t *last = picture + block->next;
	for (const uint8_t *child = picture + block->child; child < last;
	     child += ((const GimBlock *)child)->next) {
		const GimBlock *b = (const GimBlock *)child;
		const GimImageHeader *data = (const GimImageHeader *)(child + b->data);
		switch (b->type) {
		case GIM_BLOCK_IMAGE:
			if (*image == NULL)
				*image = data;
			break;
		case GIM_BLOCK_PALETTE:
			if (*palette == NULL)
				*palette = data;
			break;
		}
	}
	return true;
}

bool GimCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	const GimImageHeader *image = NULL;
	const GimImageHeader *palette = NULL;

	bool found = FindBlocks(buf, &image, &palette);
	if (!found) {
		SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return found;
	}

	if (image != NULL) {
		if (!(images->GetArraySize() == ImagePoint(image->level_count, image->frame_count))) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}
		if (attr->m_pitchalign != image->pitch_align ||
		    attr->m_heightalign != image->height_align) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}

		const uint32_t *offsets = (const uint32_t *)((const uint8_t *)image + image->offsets);
		for (uint32_t i = 0; i < images->GetMax(); i++) {
			ImageBuffer *buffer = images->GetBuffer(i);
			buffer->buff = (uint8_t *)image + *offsets++;
		}
	}
	if (palette != NULL) {
		if (!(cluts->GetArraySize() ==
		      ImagePoint(palette->level_count, palette->frame_count))) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
			return false;
		}

		const uint32_t *offsets =
			(const uint32_t *)((const uint8_t *)palette + palette->offsets);
		for (uint32_t i = 0; i < cluts->GetMax(); i++) {
			ImageBuffer *buffer = cluts->GetBuffer(i);
			buffer->buff = (uint8_t *)palette + *offsets++;
		}
	}
	return true;
}

GimCodec::GimCodec()
{
}

GimCodec::~GimCodec()
{
}

bool GimCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	return false;
}

/* The ImageMode of a GIM pixel format. */
ImageMode GimCodec::GetImageMode(uint16_t format)
{
	ImageMode mode = ImageMode_NONE;
	switch (format) {
	case 0:
		mode = ImageMode_XBGR0565;
		break;
	case 1:
		mode = ImageMode_ABGR1555;
		break;
	case 2:
		mode = ImageMode_ABGR4444;
		break;
	case 3:
		mode = ImageMode_RGBA8888;
		break;
	case 4:
		mode = ImageMode_index4;
		break;
	case 5:
		mode = ImageMode_index8;
		break;
	case 6:
		mode = ImageMode_index16;
		break;
	case 7:
		mode = ImageMode_index32;
		break;
	case 8:
		mode = ImageMode_DXT1;
		break;
	case 9:
		mode = ImageMode_DXT3;
		break;
	case 10:
		mode = ImageMode_DXT5;
		break;
	}
	return mode;
}

bool GimCodec::ReadHeader(ImageAttribute *attr, const GimImageHeader *image,
			  const GimImageHeader *palette, ImageArray *images, ImageArray *cluts)
{
	if (image != NULL) {
		attr->m_extent = ImageExtent(image->width, image->height);
		attr->m_clip = ImageRect(0, 0, image->width, image->height);
		attr->m_format = ImageFormat_Gim;
		attr->m_mode = GetImageMode(image->format);
		ImageOrder order;
		switch (image->order) {
		case 0:
			order = ImageOrder_Normal;
			break;
		case 1:
			order = ImageOrder_Swizzle;
			break;
		default:
			order = (ImageOrder)0;
			break;
		}
		attr->m_order = order;
		ImageXType xtype;
		switch (image->level_type) {
		case 1:
			xtype = ImageXType_HalfSize;
			break;
		case 2:
			xtype = ImageXType_ConstantSize;
			break;
		default:
			xtype = ImageXType_None;
			break;
		}
		attr->m_xtype = xtype;
		attr->m_ytype = (ImageYType)(image->frame_type == 3);
		attr->m_pitchalign = image->pitch_align;
		attr->m_heightalign = image->height_align;
		images->Init(image->level_count, image->frame_count);
	}
	if (palette != NULL) {
		attr->m_clutsize = palette->width;
		attr->m_clutmode = GetImageMode(palette->format);
		cluts->Init(palette->level_count, palette->frame_count);
	}
	return true;
}

bool GimCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	const GimImageHeader *image = NULL;
	const GimImageHeader *palette = NULL;

	bool found = FindBlocks(buf, &image, &palette);
	if (found && ReadHeader(attr, image, palette, images, cluts))
		return true;

	SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
	return false;
}
