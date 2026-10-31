/* paf::image::ResizeOperation: an image resized to a new extent. */
#include <image/image.h>
#include <image/image_filter.h>
#include "../../../paf_types.h"
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool ResizeOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			   const ImageArray *srccluts, ImageAttribute *dstattr,
			   ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;

	/*
	 * A linear image of one buffer per row; a 4-bit indexed one only by
	 * nearest neighbour, the filters only over RGBA and BGRA.
	 */
	ImageMode mode = srcattr->m_mode;
	if (mode == ImageMode_index4 && m_type != ImageResizeType_NearestNeighbor) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	if (srcattr->m_order != ImageOrder_Linear) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	if (m_type != ImageResizeType_NearestNeighbor && mode != ImageMode_RGBA8888 &&
	    mode != ImageMode_BGRA8888) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	if (srcimages->GetArraySize().x > 1) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	ImageExtent extent = m_extent;
	dstattr->m_originalextent = extent;
	dstattr->m_extent = extent;
	dstattr->m_subrect = ImageRect(0, 0, extent.w, extent.h);
	dstattr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	dstimages->Init(srcimages->GetArraySize());
	dstcluts->Init(srccluts->GetArraySize());
	return true;
}

bool ResizeOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
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

	return filter::Resize(dstimage, &src, dst_pitch, src_pitch, srcattr->m_clip.extent,
			      srcattr->m_mode, srcattr->m_format, extent, m_type, m_allocator);
}

ResizeOperation::ResizeOperation(const ImageExtent &extent, ImageResizeType type,
				 memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_extent(extent), m_type(type)
{
}

ResizeOperation::~ResizeOperation()
{
}

bool ResizeWithConvertModeOperation::Init(const ImageAttribute *srcattr,
					  const ImageArray *srcimages,
					  const ImageArray *srccluts, ImageAttribute *dstattr,
					  ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;

	/* The modes the conversion reads; an indexed one with a full RGBA palette. */
	ImageMode mode = srcattr->m_mode;
	if (mode == ImageMode_index4) {
		if (srcattr->m_clutsize != 16 || srcattr->m_clutmode != ImageMode_RGBA8888)
			return false;
	} else if (mode == ImageMode_index8) {
		if (srcattr->m_clutsize != 256 || srcattr->m_clutmode != ImageMode_RGBA8888)
			return false;
	} else if (mode != ImageMode_XBGR0565 && mode != ImageMode_ABGR1555 &&
		   mode != ImageMode_ABGR4444 && mode != ImageMode_BGRA8888 &&
		   mode != ImageMode_RGBA8888) {
		return false;
	}

	ImageExtent clip = srcattr->m_clip.extent;
	ImageExtent extent = srcattr->m_extent;
	if (!(extent == clip))
		return false;
	if (srcattr->m_order != ImageOrder_Linear)
		return false;
	if (srcimages->GetArraySize().x > 1)
		return false;

	extent = m_extent;
	dstattr->m_mode = ImageMode_RGBA8888;
	dstattr->m_clutmode = ImageMode_NONE;
	dstattr->m_clutsize = 0;
	dstattr->m_originalextent = extent;
	dstattr->m_extent = extent;
	dstattr->m_subrect = ImageRect(0, 0, extent.w, extent.h);
	dstattr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool ResizeWithConvertModeOperation::OperateImage(const ImagePoint &index,
						  const ImageAttribute *srcattr,
						  const ImageBuffer *srcimage,
						  const ImageBuffer *srcclut,
						  ImageAttribute *dstattr, ImageBuffer *dstimage,
						  ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent dst_extent = dstattr->GetExtent(index.x);
	ImageExtent src_extent = srcattr->GetExtent(index.x);

	return filter::ResizeWithConvertMode(dstimage, srcimage, srcclut, dst_pitch, src_pitch,
					     dst_extent, src_extent, srcattr->m_mode,
					     srcattr->m_format, ImageMode_RGBA8888, m_resize_type,
					     m_type, m_allocator);
}

ResizeWithConvertModeOperation::ResizeWithConvertModeOperation(const ImageExtent &extent,
							       ImageResizeType resize_type,
							       ImageMode mode,
							       ImageConvertModeType type,
							       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_extent(extent), m_resize_type(resize_type), m_mode(mode),
	  m_type(type)
{
}

ResizeWithConvertModeOperation::~ResizeWithConvertModeOperation()
{
}
