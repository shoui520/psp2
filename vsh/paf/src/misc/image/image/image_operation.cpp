/*
 * paf::image::ImageOperation: the image source of an operation's result. It
 * takes the header of its parent image and runs OperateImage on each of the
 * parent's buffers into the result's.
 */
#include <image/image.h>
#include "../../../paf_types.h"

using namespace paf;
using namespace paf::image;

bool ImageOperation::SetParent(Image *parent, ImageAttribute *dstattr, ImageArray *dstimages,
			       ImageArray *dstcluts)
{
	if (!parent->HasImage())
		return false;

	m_parent = parent;
	return Init(parent->GetAttribute(), &parent->GetImage(), &parent->GetCLUT(), dstattr,
		    dstimages, dstcluts);
}

bool ImageOperation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts)
{
	ImageMode mode = srcattr->m_mode;
	if (mode == ImageMode_index4 || mode == ImageMode_index8 || mode == ImageMode_index16 ||
	    mode == ImageMode_index32) {
		/* An indexed image needs a palette per buffer, or one for all. */
		if (!(srccluts->GetArraySize() == srcimages->GetArraySize()) &&
		    !(srccluts->GetArraySize() == ImagePoint(1, 1)))
			return false;
	}

	*dstattr = *srcattr;
	ImageExtent extent = srcattr->m_clip.extent;
	dstattr->m_originalextent = extent;
	dstattr->m_extent = extent;
	dstattr->m_subrect = ImageRect(0, 0, extent.w, extent.h);
	dstattr->m_format = ImageFormat_None;
	dstattr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	return true;
}

bool ImageOperation::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts)
{
	return false;
}

bool ImageOperation::Operate(const ImageAttribute *srcattr, const ImageArray *srcimages,
			     const ImageArray *srccluts, ImageAttribute *dstattr,
			     ImageArray *dstimages, ImageArray *dstcluts)
{
	dstimages->SetSize(dstattr);
	if (!dstimages->Alloc()) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}
	if (dstcluts->GetMax() != 0)
		*dstcluts = *srccluts;

	/* One palette for all buffers, or one per buffer. */
	ImagePoint index(0, 0);
	ImagePoint first(0, 0);
	const ImagePoint *clutindex;
	if (srccluts->GetArraySize() == ImagePoint(1, 1))
		clutindex = &first;
	else
		clutindex = &index;

	bool result = false;
	for (index.x = 0; index.x < srcimages->GetArraySize().x; index.x++) {
		for (index.y = 0; index.y < srcimages->GetArraySize().y; index.y++) {
			result = OperateImage(index, srcattr, srcimages->GetBuffer(index),
					      srccluts->GetBuffer(*clutindex), dstattr,
					      dstimages->GetBuffer(index), dstcluts);
			if (!result)
				break;
		}
	}
	if (!result) {
		dstimages->Free();
		dstcluts->Free();
	}
	return result;
}

bool ImageOperation::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	if (!m_parent->Load(delete_flag)) {
		m_errortype = m_parent->GetDecErrorType();
		return false;
	}

	bool result = Operate(m_parent->GetAttribute(), &m_parent->GetImage(),
			      &m_parent->GetCLUT(), attr, images, cluts);
	if (delete_flag)
		m_parent->UnLoad();
	return result;
}

int32_t ImageOperation::GetErrorType()
{
	return m_errortype;
}

bool ImageOperation::SetErrorType(int32_t type)
{
	m_errortype = type;
	return true;
}

ImageOperation::ImageOperation(memory::HeapAllocator *allocator) : m_allocator(allocator)
{
	m_errortype = 0;
	m_parent = NULL;
}

ImageOperation::~ImageOperation()
{
	if (m_parent != NULL)
		m_parent = NULL;
}
