/* paf::image::CropOperation: the part of an image inside a rectangle. */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool CropOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			 const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			 ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	ImageExtent extent = srcattr->m_extent;
	ImageExtent clip = srcattr->m_clip.extent;
	if (!(extent == clip))
		return false;

	dstimages->Init(srcimages->GetArraySize());
	dstcluts->Init(srccluts->GetArraySize());

	/* The crop rectangle, inside the image. */
	int32_t left = 0;
	int32_t top = 0;
	if (m_crop.origin.x > 0)
		left = m_crop.origin.x;
	if (m_crop.origin.y > 0)
		top = m_crop.origin.y;
	int32_t right = m_crop.extent.w + m_crop.origin.x;
	if (dstattr->m_extent.w <= right)
		right = dstattr->m_extent.w;
	int32_t bottom = m_crop.extent.h + m_crop.origin.y;
	if (dstattr->m_extent.h <= bottom)
		bottom = dstattr->m_extent.h;
	int32_t w = right - left;
	int32_t h = bottom - top;
	if (w <= 0 || h <= 0)
		return false;

	dstattr->m_extent = ImageExtent(w, h);
	dstattr->m_clip = ImageRect(left, top, w, h);
	dstattr->m_subrect = ImageRect(left, top, w, h);
	return true;
}

bool CropOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				 const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				 ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent extent = dstattr->GetExtent(index.x);

	return filter::Crop(dstimage, srcimage, dst_pitch, src_pitch, extent, srcattr->m_mode,
			    srcattr->m_format, m_crop);
}

CropOperation::CropOperation(const ImageRect &crop, memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_crop(crop)
{
}

CropOperation::~CropOperation()
{
}
