/*
 * paf::image::RawImageSource: the source of an image made from pixels (and a
 * palette) already in memory, copied as they are.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <std/string.h>
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;

bool RawImageSource::SetHeader(uint32_t pitch, const ImageExtent &ext, ImageMode mode,
			       ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	if (attr == NULL)
		return false;

	ImageExtent extent = ext;
	attr->m_extent = extent;
	attr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	attr->m_format = ImageFormat_None;
	attr->m_xtype = ImageXType_ConstantSize;
	attr->m_ytype = ImageYType_Sequence;
	attr->m_pitchalign = 1;
	attr->m_heightalign = 1;
	attr->m_mode = mode;
	attr->m_clutmode = ImageMode_NONE;
	attr->m_order = ImageOrder_Linear;
	attr->m_clutsize = 0;

	switch (mode) {
	case ImageMode_index4:
		attr->m_clutsize = 16;
		m_pitch = (pitch + 1) >> 1;
		break;
	case ImageMode_index8:
		attr->m_clutsize = 256;
		m_pitch = pitch;
		break;
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
		m_pitch = pitch * 2;
		break;
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
		m_pitch = pitch * 4;
		break;
	default:
		SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}

	images->Init(1, 1);
	if (attr->m_clutsize != 0) {
		cluts->Init(1, 1);
		attr->m_clutmode = ImageMode_RGBA8888;
	}
	return true;
}

/* The header is made by SetHeader. */
bool RawImageSource::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts)
{
	return false;
}

bool RawImageSource::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	SetErrorType(0);

	/* Only the whole image, as it is. */
	ImageExtent extent = attr->m_subrect.extent;
	if (attr->m_subrect.origin.x != 0 || attr->m_subrect.origin.y != 0 ||
	    extent.w != attr->m_originalextent.w || extent.h != attr->m_originalextent.h ||
	    extent.w != attr->m_extent.w || extent.h != attr->m_extent.h)
		return false;

	images->SetSize(0, m_pitch * attr->m_extent.h);
	cluts->SetCLUTSize(attr);
	if (!images->Alloc() || !cluts->Alloc()) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	if (m_buffer->GetAddr() == NULL || m_buffer->GetSize() == 0)
		return false;
	if (images == NULL || attr == NULL)
		return false;
	if (images->GetMax() != 1)
		return false;
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < m_pitch * attr->m_extent.h)
		return false;
	if (attr->m_clutsize != 0) {
		if (cluts->GetMax() != 1)
			return false;
		const ImageBuffer *clut = cluts->GetBuffer(0);
		if (clut->buff == NULL || clut->size < attr->m_clutsize * 4)
			return false;
	}

	sce_paf_memcpy(images->GetBuffer(0)->buff, m_buffer->GetAddr(), m_buffer->GetSize());
	if (attr->m_clutsize != 0)
		sce_paf_memcpy(cluts->GetBuffer(0)->buff, m_clut->GetAddr(), m_clut->GetSize());
	return true;
}

int32_t RawImageSource::GetErrorType()
{
	return m_errortype;
}

bool RawImageSource::SetErrorType(int32_t type)
{
	m_errortype = type;
	return true;
}

RawImageSource::RawImageSource(void *addr, size_t size, void *clut, size_t clutsize,
			       memory::HeapAllocator *allocator)
	: m_pitch(0), m_errortype(0)
{
	m_buffer = UnmanagedBuffer::Create(addr, size);
	if (clut != NULL)
		m_clut = UnmanagedBuffer::Create(clut, clutsize);
}

RawImageSource::~RawImageSource()
{
}
