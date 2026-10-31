/*
 * paf::image::ImageCodec and what the image sources do with a codec: make the
 * codec of a format, open an image with it, and decode it to the size asked
 * for, line by line or whole, then cropped and resized.
 */
#include <image/image.h>
#include <image/image_filter.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/stdlib.h>
#include "image_codec.h"
#include "image_operation.h"
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

bool ImageCodec::SetErrorType(int32_t type)
{
	m_errortype = type;
	return true;
}

ImageCodec *paf::image::CreateCodec(common::SharedPtr<Buffer> buf, ImageFormat format,
				    ImageAttribute **attr)
{
	ImageCodec *codec = NULL;
	ImageAttribute *attribute = NULL;

	switch (format) {
	case ImageFormat_Gim:
		if (IsGim(buf)) {
			codec = new GimCodec();
			attribute = new GimAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Png:
		if (IsPng(buf)) {
			codec = new PngCodec();
			attribute = new PngAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Jpg:
		if (IsJpg(buf)) {
			codec = new JpgCodec();
			attribute = new JpgAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Tif:
		if (IsTif(buf)) {
			codec = new TifCodec();
			attribute = new TifAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Gif:
		if (IsGif(buf)) {
			codec = new GifCodec();
			attribute = new GifAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Bmp:
		if (IsBmp(buf)) {
			codec = new BmpCodec();
			attribute = new BmpAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Dds:
		if (IsDds(buf)) {
			codec = new DdsCodec();
			attribute = new DdsAttribute();
			*attr = attribute;
		}
		break;
	case ImageFormat_Gxt:
		if (IsGxt(buf)) {
			codec = new GxtCodec();
			attribute = new GxtAttribute();
			*attr = attribute;
		}
		break;
	default:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return codec;
	}

	if (codec == NULL || attribute == NULL)
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_NO_CODEC);
	return codec;
}

ImageCodec *paf::image::OpenCodec(common::SharedPtr<File> file, int64_t offset, size_t size,
				  ImageFormat format, ImageAttribute **attr, ImageArray *images,
				  ImageArray *cluts)
{
	uint8_t header[16];

	if (offset != 0 && file->Seek(offset, SCE_SEEK_SET) < 0) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OFFSET);
		return NULL;
	}

	int64_t ret = file->Read(header, sizeof(header));
	if (ret != sizeof(header))
		return NULL;

	common::SharedPtr<UnmanagedBuffer> buf = UnmanagedBuffer::Create(header, sizeof(header));
	ImageCodec *codec = CreateCodec(buf, format, attr);
	if (codec != NULL && *attr != NULL) {
		ret = codec->GetHeader(file, offset, size, *attr, images, cluts);
		if (ret == 0) {
			delete codec;
			codec = NULL;
			delete *attr;
			*attr = NULL;
			if (Image::GetErrorType() == 0)
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		}
	}
	return codec;
}

ImageCodec *paf::image::OpenCodec(common::SharedPtr<Buffer> buf, ImageFormat format,
				  ImageAttribute **attr, ImageArray *images, ImageArray *cluts)
{
	ImageCodec *codec = CreateCodec(buf, format, attr);

	if (codec != NULL && *attr != NULL) {
		bool result = codec->GetHeader(buf, *attr, images, cluts);
		if (!result) {
			delete codec;
			codec = NULL;
			delete *attr;
			*attr = NULL;
			if (Image::GetErrorType() == 0)
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		}
	}
	return codec;
}

/*
 * Reads attr's subrect line by line, each line resized across into a
 * transposed buffer, then resizes that down into images.
 */
static bool ReadLines(ImageCodec *codec, ImageAttribute *attr, ImageArray *images,
		      ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata)
{
	uint32_t pitch;
	bool topdown = true;

	if (!codec->BeginLines(attr, images, &pitch, &topdown))
		return false;

	ImageRect rect = attr->m_subrect;
	int32_t dst_w = attr->m_extent.w;
	int32_t dst_h = attr->m_extent.h;
	if (rect.extent.w <= 0 || rect.extent.h <= 0) {
		codec->EndLines();
		return false;
	}

	HeapAllocator *allocator = images->GetHeapAllocator();
	ContribList *contrib = CalcContrib(rect.extent.w, dst_w, attr->m_resizetype, allocator);

	uint8_t *line;
	if (allocator != NULL)
		line = (uint8_t *)allocator->sce_paf_malloc(pitch);
	else
		line = (uint8_t *)sce_paf_malloc(pitch);
	if (line == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_w, allocator);
		codec->EndLines();
		return false;
	}

	uint8_t *tmp;
	if (allocator != NULL)
		tmp = (uint8_t *)allocator->sce_paf_malloc(4 * dst_w * rect.extent.h);
	else
		tmp = (uint8_t *)sce_paf_malloc(4 * dst_w * rect.extent.h);
	if (tmp == NULL) {
		if (allocator != NULL)
			allocator->sce_paf_free(line);
		else
			sce_paf_free(line);
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_w, allocator);
		codec->EndLines();
		return false;
	}

	int32_t skip = rect.origin.y;
	int32_t step;
	uint8_t *column;
	if (topdown) {
		step = 4;
		column = tmp + 4 * (rect.extent.h * dst_w - rect.extent.h);
	} else {
		skip = attr->m_originalextent.h - (rect.origin.y + rect.extent.h);
		step = -4;
		column = tmp + dst_w * rect.extent.h * 4 - 4;
	}
	int32_t column_pitch = 4 * rect.extent.h;

	for (int32_t y = 0; y < skip; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (allocator != NULL) {
				allocator->sce_paf_free(tmp);
				allocator->sce_paf_free(line);
			} else {
				sce_paf_free(tmp);
				sce_paf_free(line);
			}
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
			FreeContribList(contrib, dst_w, allocator);
			codec->EndLines();
			return false;
		}
		if (!codec->SkipLine()) {
			if (allocator != NULL) {
				allocator->sce_paf_free(tmp);
				allocator->sce_paf_free(line);
			} else {
				sce_paf_free(tmp);
				sce_paf_free(line);
			}
			FreeContribList(contrib, dst_w, allocator);
			codec->EndLines();
			return false;
		}
	}

	for (int32_t y = 0; y < rect.extent.h; y++) {
		if (cancelfunc != NULL && cancelfunc(userdata)) {
			if (allocator != NULL) {
				allocator->sce_paf_free(tmp);
				allocator->sce_paf_free(line);
			} else {
				sce_paf_free(tmp);
				sce_paf_free(line);
			}
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
			FreeContribList(contrib, dst_w, allocator);
			codec->EndLines();
			return false;
		}
		if (!codec->ReadLine(line, &rect)) {
			if (allocator != NULL) {
				allocator->sce_paf_free(tmp);
				allocator->sce_paf_free(line);
			} else {
				sce_paf_free(tmp);
				sce_paf_free(line);
			}
			FreeContribList(contrib, dst_w, allocator);
			codec->EndLines();
			return false;
		}
		ApplyContribH(line, column, dst_w, column_pitch, contrib);
		column += step;
	}

	if (allocator != NULL)
		allocator->sce_paf_free(line);
	else
		sce_paf_free(line);
	FreeContribList(contrib, dst_w, allocator);

	if (!codec->EndLines()) {
		if (allocator != NULL)
			allocator->sce_paf_free(tmp);
		else
			sce_paf_free(tmp);
		return false;
	}

	uint8_t *dst = (uint8_t *)images->GetBuffer(0)->buff;
	if (dst == NULL) {
		if (allocator != NULL)
			allocator->sce_paf_free(tmp);
		else
			sce_paf_free(tmp);
		return false;
	}

	contrib = CalcContrib(rect.extent.h, dst_h, attr->m_resizetype, allocator);
	if (contrib == NULL) {
		FreeContribList(contrib, dst_h, allocator);
		if (allocator != NULL)
			allocator->sce_paf_free(tmp);
		else
			sce_paf_free(tmp);
		return false;
	}
	ApplyContribV(rect.extent.h, dst_h, attr->GetMemPitch(0), tmp, dst_w, dst,
		      contrib);
	FreeContribList(contrib, dst_h, allocator);
	if (allocator != NULL)
		allocator->sce_paf_free(tmp);
	else
		sce_paf_free(tmp);
	return true;
}

bool paf::image::LoadLines(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset,
			   size_t size, common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			   ImageCodec *codec, ImageArray *images, ImageArray *cluts,
			   DecodeCancelFunc cancelfunc, void *userdata)
{
	Image::SetErrorType(0);
	reader->SetErrorType(0);

	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL)
		return false;
	int32_t w = attr->m_extent.w;
	int32_t h = attr->m_extent.h;
	if (image->size < (size_t)(w * h * 4))
		return false;

	if (buf.get() != NULL) {
		if (w == attr->m_originalextent.w && h == attr->m_originalextent.h &&
		    w == attr->m_subrect.extent.w && h == attr->m_subrect.extent.h) {
			bool result = reader->LoadImage(buf, attr, images, cluts, cancelfunc,
							userdata);
			if (!result)
				return false;
		} else {
			bool result = reader->Open(buf, attr);
			if (!result) {
				reader->Close();
				return false;
			}
			if (!ReadLines(reader, attr, images, cluts, cancelfunc, userdata)) {
				reader->Close();
				return false;
			}
			reader->Close();
		}
	} else {
		if (w == attr->m_originalextent.w && h == attr->m_originalextent.h &&
		    w == attr->m_subrect.extent.w && h == attr->m_subrect.extent.h) {
			bool result = reader->LoadImage(file, offset, size, attr, images, cluts,
							cancelfunc, userdata);
			if (!result)
				return false;
		} else {
			bool result = reader->Open(file, offset, size, attr);
			if (!result) {
				reader->Close();
				return false;
			}
			if (!ReadLines(reader, attr, images, cluts, cancelfunc, userdata)) {
				reader->Close();
				return false;
			}
			reader->Close();
		}
	}
	return true;
}

/* Decodes the whole image, from buf or, when buf is empty, from file. */
static bool Decode(common::SharedPtr<File> file, int64_t offset, size_t size,
		   common::SharedPtr<Buffer> buf, ImageAttribute *attr, ImageCodec *codec,
		   ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
		   void *userdata)
{
	if (!images->Alloc() || !cluts->Alloc()) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		codec->SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	bool result;
	if (buf.get() != NULL) {
		result = codec->LoadImage(buf, attr, images, cluts, cancelfunc, userdata);
		if (!result) {
			images->Free();
			cluts->Free();
		}
	} else {
		result = codec->LoadImage(file, offset, size, attr, images, cluts, cancelfunc,
					  userdata);
		if (!result) {
			images->Free();
			cluts->Free();
		}
	}
	return result;
}

bool paf::image::LoadWhole(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset,
			   size_t size, common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			   ImageCodec *codec, ImageArray *images, ImageArray *cluts,
			   DecodeCancelFunc cancelfunc, void *userdata)
{
	cluts->SetCLUTSize(attr);

	bool clipped = false;
	bool resized = false;
	if (attr->m_subrect.origin.x != 0 || attr->m_subrect.origin.y != 0 ||
	    attr->m_subrect.extent.w != attr->m_originalextent.w ||
	    attr->m_subrect.extent.h != attr->m_originalextent.h)
		clipped = true;
	if (attr->m_subrect.extent.w != attr->m_extent.w ||
	    attr->m_subrect.extent.h != attr->m_extent.h)
		resized = true;

	if (!(clipped || resized)) {
		images->SetSize(attr);
		return Decode(file, offset, size, buf, attr, codec, images, cluts, cancelfunc,
			      userdata);
	} else {
		/* The whole image first. */
		ImageAttribute whole(*attr);
		whole.m_extent = attr->m_originalextent;
		whole.m_subrect = ImageRect(0, 0, attr->m_originalextent.w,
					    attr->m_originalextent.h);
		whole.m_clip = ImageRect(0, 0, attr->m_originalextent.w,
					 attr->m_originalextent.h);
		ImageArray decoded(images->GetHeapAllocator());
		decoded.Init(images->GetArraySize());
		decoded.SetSize(&whole);
		if (!Decode(file, offset, size, buf, &whole, codec, &decoded, cluts, cancelfunc,
			    userdata))
			return false;

		ImageArray cropped(images->GetHeapAllocator());
		ImageArray dstcluts(NULL);
		ImageAttribute dstattr;
		ImageArray *src;

		if (clipped) {
			CopyOperation copy(images->GetHeapAllocator());
			whole.m_clip = attr->m_subrect;
			ImageArray *dst = images;
			if (resized)
				dst = &cropped;
			dst->Reset();
			dstcluts.Reset();
			if (!copy.Init(&whole, &decoded, cluts, &dstattr, dst, &dstcluts)) {
				cluts->Free();
				return false;
			}
			if (!copy.Operate(&whole, &decoded, cluts, &dstattr, dst, &dstcluts)) {
				cluts->Free();
				return false;
			}
			decoded.Reset();
			whole.m_extent = attr->m_subrect.extent;
			whole.m_clip = ImageRect(0, 0, attr->m_subrect.extent.w,
						 attr->m_subrect.extent.h);
			src = &cropped;
		} else {
			src = &decoded;
		}

		if (resized) {
			ImageExtent extent = attr->m_extent;
			ResizeOperation resize(extent, attr->m_resizetype,
					       images->GetHeapAllocator());
			images->Reset();
			dstcluts.Reset();
			if (!resize.Init(&whole, src, cluts, &dstattr, images, &dstcluts)) {
				cluts->Free();
				return false;
			}
			if (!resize.Operate(&whole, src, cluts, &dstattr, images, &dstcluts)) {
				cluts->Free();
				return false;
			}
		}
		return true;
	}
}

bool paf::image::LoadScaled(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset,
			    size_t size, common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			    ImageCodec *codec, ImageArray *images, ImageArray *cluts,
			    DecodeCancelFunc cancelfunc, void *userdata)
{
	cluts->SetCLUTSize(attr);

	ImageExtent decoded(0, 0);
	if (!codec->CalcDecodeExtent(ImageExtent(attr->m_originalextent), ImageRect(attr->m_subrect),
				     ImageExtent(attr->m_extent), &decoded))
		return false;

	if (decoded.w != attr->m_extent.w || decoded.h != attr->m_extent.h) {
		/* Decoded at the codec's size into a buffer of its own, then resized. */
		ImageExtent saved = attr->m_extent;
		attr->m_extent = decoded;
		ImageRect clip = attr->m_clip;
		attr->m_clip = ImageRect(0, 0, decoded.w, decoded.h);
		ImageArray scaled(images->GetHeapAllocator());
		scaled.Init(images->GetArraySize());
		scaled.SetSize(attr);
		if (!Decode(file, offset, size, buf, attr, codec, &scaled, cluts, cancelfunc,
			    userdata)) {
			cluts->Free();
			return false;
		}

		ResizeOperation resize(saved, attr->m_resizetype, images->GetHeapAllocator());
		attr->m_clip = ImageRect(0, 0, decoded.w, decoded.h);
		ImageAttribute dstattr;
		ImageArray dstcluts(NULL);
		images->Reset();
		if (!resize.Init(attr, &scaled, cluts, &dstattr, images, &dstcluts)) {
			cluts->Free();
			return false;
		}
		if (!resize.Operate(attr, &scaled, cluts, &dstattr, images, &dstcluts)) {
			cluts->Free();
			return false;
		}
		attr->m_extent = saved;
		attr->m_clip = clip;
	} else {
		images->SetSize(attr);
		if (!Decode(file, offset, size, buf, attr, codec, images, cluts, cancelfunc,
			    userdata)) {
			cluts->Free();
			return false;
		}
	}
	return true;
}

bool ImageCodec::CalcDecodeExtent(const ImageExtent &original, const ImageRect &subrect,
				  const ImageExtent &extent, ImageExtent *out)
{
	if (subrect.origin.x == 0 && subrect.origin.y == 0 && subrect.extent.w == original.w &&
	    subrect.extent.h == original.h) {
		out->w = original.w;
		out->h = original.h;
		return true;
	}
	return false;
}

int32_t ImageCodec::GetLoadType() const
{
	return m_loadtype;
}

bool ImageCodec::SetLoadType(int32_t type)
{
	m_loadtype = type;
	return true;
}

int32_t ImageCodec::GetErrorType() const
{
	return m_errortype;
}

bool ImageCodec::Open(common::SharedPtr<File> file, int64_t offset, size_t size,
		      const ImageAttribute *attr)
{
	return false;
}

bool ImageCodec::Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr)
{
	return false;
}

bool ImageCodec::Close()
{
	return false;
}

bool ImageCodec::BeginLines(const ImageAttribute *attr, const ImageArray *images,
			    uint32_t *pitch, bool *topdown)
{
	return false;
}

bool ImageCodec::EndLines()
{
	return false;
}

bool ImageCodec::PrepareLine()
{
	return false;
}

bool ImageCodec::ReadLine(void *dst, const ImageRect *rect)
{
	return false;
}

bool ImageCodec::SkipLine()
{
	return false;
}

ImageCodec::ImageCodec() : m_loadtype(LoadType_Image), m_errortype(0)
{
}

ImageCodec::~ImageCodec()
{
}
