/* paf::image::BlendOperation: an image blended with another at alpha. */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

bool BlendOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode != ImageMode_BGRA8888 && srcattr->m_mode != ImageMode_RGBA8888)
		return false;
	if (srcattr->m_order != ImageOrder_Linear && srcattr->m_extent != srcattr->m_clip.extent)
		return false;

	ImageExtent extent = srcattr->m_extent;
	if (extent != m_image->GetAttribute()->m_extent ||
	    srcattr->m_mode != m_image->GetAttribute()->m_mode ||
	    srcattr->m_order != m_image->GetAttribute()->m_order)
		return false;

	dstimages->Init(srcimages->GetArraySize());
	dstcluts->Init(srccluts->GetArraySize());
	return true;
}

bool BlendOperation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
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

	ImageBuffer *image = m_image->GetImage().GetBuffer(m_image->GetArrayPos());
	bool loaded = false;
	if (image->buff == NULL) {
		if (!m_image->Load(true))
			return false;
		loaded = true;
		image = m_image->GetImage().GetBuffer(m_image->GetArrayPos());
	}
	m_image->SetClip(clip);

	ImageBuffer blend;
	blend = *image;
	blend.buff = (uint8_t *)blend.buff + clip.origin.y * srcattr->GetMemPitch(index.x) +
		     (int)(srcattr->GetPixelSizeInBits() * 0.125f * clip.origin.x);
	bool blended = filter::Blend(dstimage, &src, &blend, dst_pitch, src_pitch, extent,
				     srcattr->m_mode, srcattr->m_format, m_alpha);
	if (!blended) {
		if (loaded)
			m_image->UnLoad();
		return false;
	}
	if (loaded)
		m_image->UnLoad();
	return blended;
}

BlendOperation::BlendOperation(common::SharedPtr<Image> image, float alpha,
			       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_image(image), m_alpha(alpha)
{
}

BlendOperation::~BlendOperation()
{
}
