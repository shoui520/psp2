/* paf::image::PasteOperation: an image with another pasted over it at a point. */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool PasteOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode != ImageMode_BGRA8888 && srcattr->m_mode != ImageMode_RGBA8888)
		return false;
	if (srcattr->m_order != ImageOrder_Linear) {
		ImageExtent clip = srcattr->m_clip.extent;
		ImageExtent extent = srcattr->m_extent;
		if (!(extent == clip))
			return false;
	}

	/* The pasted image: whole, and in the same mode and order. */
	ImageExtent extent = m_image->GetAttribute()->m_extent;
	ImageExtent clip = m_image->GetAttribute()->m_clip.extent;
	if (!(extent == clip))
		return false;
	if (srcattr->m_mode != m_image->GetAttribute()->m_mode)
		return false;
	if (srcattr->m_order != m_image->GetAttribute()->m_order)
		return false;

	dstimages->Init(srcimages->GetArraySize());
	dstcluts->Init(srccluts->GetArraySize());
	return true;
}

bool PasteOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
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
	int image_pitch = m_image->GetAttribute()->GetMemPitch(index.x);
	ImageExtent extent = dstattr->GetExtent(index.x);
	ImageExtent image_extent = m_image->GetAttribute()->GetExtent(index.x);

	/* The pasted image is decoded for the paste when it is not yet. */
	const ImageBuffer *image = m_image->GetImage().GetBuffer(m_image->GetArrayPos());
	bool loaded = false;
	if (image->buff == NULL) {
		if (!m_image->Load(true))
			return false;
		loaded = true;
		image = m_image->GetImage().GetBuffer(m_image->GetArrayPos());
	}

	bool result = filter::Paste(dstimage, &src, image, dst_pitch, src_pitch, image_pitch,
				    extent, image_extent, srcattr->m_mode, srcattr->m_format,
				    m_point);
	if (!result) {
		if (loaded)
			m_image->UnLoad();
		return false;
	}
	if (loaded)
		m_image->UnLoad();
	return result;
}

PasteOperation::PasteOperation(common::SharedPtr<Image> image, const ImagePoint &point,
			       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_image(image), m_point(point)
{
}

PasteOperation::~PasteOperation()
{
}
