/*
 * paf::image::CopyWithEdgeOperation: an image converted to RGBA8888 inside a
 * border of extlines pixels on each side.
 */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool CopyWithEdgeOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
				 const ImageArray *srccluts, ImageAttribute *dstattr,
				 ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode == ImageMode_index4 &&
	    (srcattr->m_clutsize != 16 || srcattr->m_clutmode != ImageMode_RGBA8888))
		return false;
	if (srcattr->m_mode == ImageMode_index8 &&
	    (srcattr->m_clutsize != 256 || srcattr->m_clutmode != ImageMode_RGBA8888))
		return false;
	if (srcattr->m_mode == ImageMode_index16 || srcattr->m_mode == ImageMode_index32)
		return false;
	if (srcattr->m_order != ImageOrder_Linear)
		return false;
	/* One image only: the border is made for the first. */
	if (srcimages->GetArraySize().x > 1)
		return false;

	dstattr->m_clutsize = 0;
	dstattr->m_mode = ImageMode_RGBA8888;
	dstattr->m_clutmode = ImageMode_NONE;

	int32_t w = srcattr->m_clip.extent.w + m_extlines * 2;
	int32_t h = srcattr->m_clip.extent.h + m_extlines * 2;
	dstattr->m_originalextent = ImageExtent(w, h);
	dstattr->m_extent = ImageExtent(w, h);
	dstattr->m_clip = ImageRect(0, 0, w, h);
	dstattr->m_subrect = ImageRect(0, 0, w, h);

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool CopyWithEdgeOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
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

	return filter::CopyWithEdge(dstimage, &src, srcclut, dst_pitch, src_pitch, extent,
				    srcattr->m_mode, srcattr->m_format, m_extlines);
}

CopyWithEdgeOperation::CopyWithEdgeOperation(uint32_t extlines, memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_extlines(extlines)
{
}

CopyWithEdgeOperation::~CopyWithEdgeOperation()
{
}
