/* paf::image's image sources: the decoders over a file or a buffer, by format. */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <file/localfile.h>
#include "../../../paf_types.h"
#include "image_codec.h"
#include "image_source.h"

using namespace paf;
using namespace paf::image;

bool ImageFileDecoder::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
				 ImageArray *cluts)
{
	m_codec = OpenCodec(m_file, m_offset, m_size, format, attr, images, cluts);
	return m_codec != NULL && *attr != NULL;
}

bool ImageFileDecoder::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
				 bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	bool result;

	switch (m_codec->GetLoadType()) {
	case ImageCodec::LoadType_Line:
		images->SetSize(attr);
		if (!images->Alloc() || !cluts->Alloc()) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			return false;
		}
		result = LoadLines(m_codec, m_file, m_offset, m_size, common::SharedPtr<Buffer>(),
				   attr, m_codec, images, cluts, cancelfunc, userdata);
		if (!result) {
			images->Free();
			cluts->Free();
		}
		break;
	case ImageCodec::LoadType_Image:
		result = LoadWhole(m_codec, m_file, m_offset, m_size, common::SharedPtr<Buffer>(),
				   attr, m_codec, images, cluts, cancelfunc, userdata);
		break;
	case ImageCodec::LoadType_Scaled:
		result = LoadScaled(m_codec, m_file, m_offset, m_size, common::SharedPtr<Buffer>(),
				    attr, m_codec, images, cluts, cancelfunc, userdata);
		break;
	default:
		result = false;
		break;
	}
	return result;
}

int32_t ImageFileDecoder::GetErrorType()
{
	int32_t type = 0;

	if (m_codec != NULL)
		type = m_codec->GetErrorType();
	return type;
}

bool ImageFileDecoder::SetErrorType(int32_t type)
{
	bool result = false;

	if (m_codec != NULL)
		result = m_codec->SetErrorType(type);
	return result;
}

ImageFileDecoder::ImageFileDecoder(const char *filename, int64_t offset, size_t size)
	: m_offset(offset), m_size(size), m_opened(false), m_codec(NULL)
{
	int32_t error;

	m_file = LocalFile::Open(filename, 0x10001, 0, &error);
	m_opened = true;
}

ImageFileDecoder::ImageFileDecoder(common::SharedPtr<File> file, int64_t offset, size_t size)
	: m_offset(offset), m_size(size), m_opened(false), m_codec(NULL)
{
	m_file = file;
}

ImageFileDecoder::~ImageFileDecoder()
{
	if (m_file.get() != NULL && m_opened)
		m_file->Close();
	if (m_codec != NULL)
		delete m_codec;
}

bool ImageDecoder::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			     ImageArray *cluts)
{
	m_codec = OpenCodec(m_buffer, format, attr, images, cluts);
	return m_codec != NULL && *attr != NULL;
}

bool ImageDecoder::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			     bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	bool result;

	switch (m_codec->GetLoadType()) {
	case ImageCodec::LoadType_Line:
		images->SetSize(attr);
		if (!images->Alloc() || !cluts->Alloc()) {
			SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			return false;
		}
		result = LoadLines(m_codec, common::SharedPtr<File>(), 0, 0, m_buffer, attr,
				   m_codec, images, cluts, cancelfunc, userdata);
		if (!result) {
			images->Free();
			cluts->Free();
		}
		break;
	case ImageCodec::LoadType_Image:
		result = LoadWhole(m_codec, common::SharedPtr<File>(), 0, 0, m_buffer, attr,
				   m_codec, images, cluts, cancelfunc, userdata);
		break;
	case ImageCodec::LoadType_Scaled:
		result = LoadScaled(m_codec, common::SharedPtr<File>(), 0, 0, m_buffer, attr,
				    m_codec, images, cluts, cancelfunc, userdata);
		break;
	default:
		result = false;
		break;
	}
	return result;
}

int32_t ImageDecoder::GetErrorType()
{
	int32_t type = 0;

	if (m_codec != NULL)
		type = m_codec->GetErrorType();
	return type;
}

bool ImageDecoder::SetErrorType(int32_t type)
{
	bool result = false;

	if (m_codec != NULL)
		result = m_codec->SetErrorType(type);
	return result;
}

ImageDecoder::ImageDecoder(void *addr, size_t size) : m_codec(NULL)
{
	m_buffer = UnmanagedBuffer::Create(addr, size);
}

ImageDecoder::~ImageDecoder()
{
	if (m_codec != NULL)
		delete m_codec;
}

/* GIM images are used in place: the buffers point into the data. */
bool GimDecoder::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			   ImageArray *cluts)
{
	m_codec = OpenCodec(m_buffer, format, attr, images, cluts);
	return m_codec != NULL && *attr != NULL;
}

bool GimDecoder::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			   bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	images->SetSize(attr);
	cluts->SetCLUTSize(attr);
	if (!images->SetLink() || !cluts->SetLink())
		return false;
	return m_codec->LoadImage(m_buffer, attr, images, cluts);
}

int32_t GimDecoder::GetErrorType()
{
	int32_t type = 0;

	if (m_codec != NULL)
		type = m_codec->GetErrorType();
	return type;
}

bool GimDecoder::SetErrorType(int32_t type)
{
	bool result = false;

	if (m_codec != NULL)
		result = m_codec->SetErrorType(type);
	return result;
}

GimDecoder::GimDecoder(void *addr, size_t size) : m_codec(NULL)
{
	m_buffer = UnmanagedBuffer::Create(addr, size);
}

GimDecoder::~GimDecoder()
{
	if (m_codec != NULL)
		delete m_codec;
}

/* DDS images are used in place: the buffers point into the data. */
bool DdsDecoder::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			   ImageArray *cluts)
{
	m_codec = OpenCodec(m_buffer, format, attr, images, cluts);
	return m_codec != NULL && *attr != NULL;
}

bool DdsDecoder::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			   bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	images->SetSize(attr);
	cluts->SetCLUTSize(attr);
	if (!images->SetLink() || !cluts->SetLink())
		return false;
	return m_codec->LoadImage(m_buffer, attr, images, cluts);
}

int32_t DdsDecoder::GetErrorType()
{
	int32_t type = 0;

	if (m_codec != NULL)
		type = m_codec->GetErrorType();
	return type;
}

bool DdsDecoder::SetErrorType(int32_t type)
{
	bool result = false;

	if (m_codec != NULL)
		result = m_codec->SetErrorType(type);
	return result;
}

DdsDecoder::DdsDecoder(void *addr, size_t size) : m_codec(NULL)
{
	m_buffer = UnmanagedBuffer::Create(addr, size);
}

DdsDecoder::~DdsDecoder()
{
	if (m_codec != NULL)
		delete m_codec;
}

/* GXT images are used in place: the buffers point into the data. */
bool GxtDecoder::GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			   ImageArray *cluts)
{
	m_codec = OpenCodec(m_buffer, format, attr, images, cluts);
	return m_codec != NULL && *attr != NULL;
}

bool GxtDecoder::LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			   bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata)
{
	images->SetSize(attr);
	cluts->SetCLUTSize(attr);
	if (!images->SetLink() || !cluts->SetLink())
		return false;
	return m_codec->LoadImage(m_buffer, attr, images, cluts);
}

int32_t GxtDecoder::GetErrorType()
{
	int32_t type = 0;

	if (m_codec != NULL)
		type = m_codec->GetErrorType();
	return type;
}

bool GxtDecoder::SetErrorType(int32_t type)
{
	bool result = false;

	if (m_codec != NULL)
		result = m_codec->SetErrorType(type);
	return result;
}

GxtDecoder::GxtDecoder(void *addr, size_t size) : m_codec(NULL)
{
	m_buffer = UnmanagedBuffer::Create(addr, size);
}

GxtDecoder::~GxtDecoder()
{
	if (m_codec != NULL)
		delete m_codec;
}
