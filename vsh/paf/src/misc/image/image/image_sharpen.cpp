/* paf::image::SharpenOperation: an image sharpened by an unsharp mask. */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool SharpenOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			    const ImageArray *srccluts, ImageAttribute *dstattr,
			    ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	ImageMode mode = srcattr->m_mode;
	if (mode != ImageMode_BGRA8888 && mode != ImageMode_RGBA8888)
		return false;
	if (srcattr->m_order != ImageOrder_Linear)
		return false;

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool SharpenOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				    const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				    ImageAttribute *dstattr, ImageBuffer *dstimage,
				    ImageArray *dstclut)
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

	return filter::Sharpen(dstimage, &src, dst_pitch, src_pitch, extent, srcattr->m_mode,
			       srcattr->m_format, m_amount, m_threshold, NULL);
}

SharpenOperation::SharpenOperation(float amount, int32_t threshold,
				   memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_amount(amount), m_threshold(threshold)
{
}

SharpenOperation::~SharpenOperation()
{
}
