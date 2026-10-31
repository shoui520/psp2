/* paf::image::FlipOperation: an image mirrored left to right or top to bottom. */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool FlipOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			 const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			 ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode == ImageMode_index4)
		return false;
	if (srcattr->m_order != ImageOrder_Linear) {
		ImageExtent extent = srcattr->m_extent;
		ImageExtent clip = srcattr->m_clip.extent;
		if (!(extent == clip))
			return false;
	}

	int32_t w = dstattr->m_extent.w;
	int32_t h = dstattr->m_extent.h;
	dstattr->m_originalextent = ImageExtent(w, h);
	dstattr->m_clip = ImageRect(0, 0, w, h);
	dstattr->m_subrect = ImageRect(0, 0, w, h);

	dstimages->Init(srcimages->GetArraySize());
	dstcluts->Init(srccluts->GetArraySize());
	return true;
}

bool FlipOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				 const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				 ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	ImageRect clip = srcattr->GetClip(index.x);
	ImageBuffer src = *srcimage;
	src.buff = (uint8_t *)src.buff + clip.origin.y * srcattr->GetMemPitch(index.x) +
		   (int)(srcattr->GetPixelSizeInBits() * 0.125f * clip.origin.x);
	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent extent = dstattr->GetExtent(index.x);

	return filter::Flip(dstimage, &src, dst_pitch, src_pitch, extent, srcattr->m_mode,
			    srcattr->m_format, m_type);
}

FlipOperation::FlipOperation(ImageFlipType type, memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_type(type)
{
}

FlipOperation::~FlipOperation()
{
}
