/*
 * paf::image::JpgCodec: JPEG images through libjpeg, decoded whole into
 * RGBA8888, at 1/1, 1/2, 1/4 or 1/8 of their size, or only a region of them.
 */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <kernel/iofilemgr.h>
#include <std/setjmp.h>
#include <std/string.h>
#include "image_codec.h"
#include "image_source.h"
#include "../../../paf_types.h"

using namespace paf;
using namespace paf::image;

static void ErrorExit(j_common_ptr cinfo);

/* size / scale, rounded up. */
static inline int32_t DivideRoundUp(int32_t size, uint32_t scale)
{
	if (size % scale == 0)
		return size / scale;
	return (int32_t)((float)size / (float)scale + 1.0f);
}

static inline int32_t DivideRoundUp(int32_t size, int32_t scale)
{
	if (size % scale == 0)
		return size / scale;
	return (int32_t)((float)size / (float)scale + 1.0f);
}

/* The largest power of two up to 8 that keeps size at least target. */
static inline int32_t CalcScale(int32_t size, int32_t target)
{
	int32_t scale = 1;
	do {
		if (size / (scale * 2) < target)
			break;
		scale *= 2;
	} while (scale <= 8);
	if (scale > 8)
		scale = 8;
	return scale;
}

bool JpgAttribute::GetAttribute(uint32_t key, bool &value)
{
	if (key != 0)
		return false;
	value = m_progressive;
	return true;
}

bool JpgAttribute::GetAttribute(uint32_t key, int &value)
{
	if (key != 1)
		return false;
	value = m_sampling;
	return true;
}

bool JpgAttribute::GetAttribute(uint32_t key, float &value)
{
	return false;
}

bool JpgAttribute::GetAttribute(uint32_t key, string &value)
{
	return false;
}

JpgAttribute::JpgAttribute()
{
	m_progressive = false;
	m_sampling = -1;
}

JpgAttribute::~JpgAttribute()
{
}

bool paf::image::IsJpg(common::SharedPtr<Buffer> buf)
{
	if (buf->GetSize() < 2)
		return false;

	const uint8_t soi[2] = { 0xff, 0xd8 };
	return sce_paf_memcmp(soi, buf->GetAddr(), 2) == 0;
}

/* The image error for a libjpeg message code. */
static void SetJpegErrorType(int code)
{
	switch (code) {
	case JERR_EMPTY_IMAGE:
	case JERR_SOF_UNSUPPORTED:
	case JERR_UNKNOWN_MARKER:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		break;
	case JERR_OUT_OF_MEMORY:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		break;
	default:
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		break;
	}
}

/* Decodes the attribute's subrect, at the decode scale, into images. */
bool JpgCodec::DecodeRegion(const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			    DecodeCancelFunc cancelfunc, void *userdata)
{
	int code = sce_paf_setjmp(m_error.setjmp_buffer);
	if (code != 0) {
		SetJpegErrorType(code);
		return false;
	}

	if (images->GetMax() != 1)
		return false;
	ImageBuffer *image = images->GetBuffer(0);
	uint8_t *dst = (uint8_t *)image->buff;
	if (dst == NULL || image->size < attr->m_extent.w * attr->m_extent.h * 4)
		return false;

	m_cinfo.scale_num = 1;
	m_cinfo.scale_denom = m_scale;

	int32_t x = attr->m_subrect.origin.x;
	int32_t y = attr->m_subrect.origin.y;
	int32_t width = attr->m_subrect.extent.w;
	if (width <= 0)
		return false;
	int32_t height = attr->m_subrect.extent.h;
	if (height <= 0)
		return false;

	int32_t left = DivideRoundUp(x, m_cinfo.scale_denom);
	int32_t top = DivideRoundUp(y, m_cinfo.scale_denom);
	int32_t w = DivideRoundUp(width, m_cinfo.scale_denom);
	uint32_t right = left + w;
	int32_t h = DivideRoundUp(height, m_cinfo.scale_denom);

	jpeg_calc_output_dimensions(&m_cinfo);
	jpeg_start_decompress(&m_cinfo);
	JSAMPARRAY line = m_cinfo.mem->alloc_sarray((j_common_ptr)&m_cinfo, JPOOL_IMAGE,
						    m_cinfo.output_width *
						    m_cinfo.output_components, 1);
	int32_t skip = attr->GetMemPitch(0) - w * 4;
	uint32_t bottom = top + h;
	if (right > m_cinfo.output_width)
		left = m_cinfo.output_width - w;
	if (bottom > m_cinfo.output_height) {
		top -= bottom - m_cinfo.output_height;
		bottom = m_cinfo.output_height;
	}

	int components = m_cinfo.output_components;
	if (components == 3) {
		while (m_cinfo.output_scanline < top) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
		}
		while (m_cinfo.output_scanline < bottom) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = &(*line)[left * 3];
			for (int32_t i = 0; i < w; i++) {
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = 0xff;
				dst += 4;
				src += 3;
			}
			dst += skip;
		}
		jpeg_abort((j_common_ptr)&m_cinfo);
		return true;
	} else if (components == 1) {
		while (m_cinfo.output_scanline < top) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
		}
		while (m_cinfo.output_scanline < bottom) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = *line;
			for (int32_t i = 0; i < left; i++)
				src++;
			for (int32_t i = 0; i < w; i++) {
				dst[0] = *src;
				dst[1] = *src;
				dst[2] = *src;
				dst[3] = 0xff;
				dst += 4;
				src++;
			}
			dst += skip;
		}
		while (m_cinfo.output_scanline < m_cinfo.output_height) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
		}
	} else if (components == 4) {
		while (m_cinfo.output_scanline < top) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
		}
		while (m_cinfo.output_scanline < bottom) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = *line;
			for (int32_t i = 0; i < left; i++)
				src += 4;
			for (int32_t i = 0; i < w; i++) {
				dst[0] = src[0] * src[3] / 0xff;
				dst[1] = src[1] * src[3] / 0xff;
				dst[2] = src[2] * src[3] / 0xff;
				dst[3] = 0xff;
				dst += 4;
				src += 4;
			}
			dst += skip;
		}
		while (m_cinfo.output_scanline < m_cinfo.output_height) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
		}
	} else {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	jpeg_finish_decompress(&m_cinfo);
	return true;
}

/* Decodes the whole image, at the decode scale, into images. */
bool JpgCodec::DecodeWhole(const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			   DecodeCancelFunc cancelfunc, void *userdata)
{
	int code = sce_paf_setjmp(m_error.setjmp_buffer);
	if (code != 0) {
		SetJpegErrorType(code);
		return false;
	}

	m_cinfo.scale_num = 1;
	m_cinfo.scale_denom = m_scale;
	jpeg_calc_output_dimensions(&m_cinfo);
	jpeg_start_decompress(&m_cinfo);
	JDIMENSION width = m_cinfo.output_width;
	uint32_t rowsize = width * m_cinfo.output_components;
	JSAMPARRAY line = m_cinfo.mem->alloc_sarray((j_common_ptr)&m_cinfo, JPOOL_IMAGE,
						    rowsize, 1);
	uint8_t *dst = (uint8_t *)images->GetBuffer(0)->buff;
	int32_t skip = attr->GetMemPitch(0) - width * 4;

	int components = m_cinfo.output_components;
	if (components == 3) {
		while (m_cinfo.output_scanline < m_cinfo.output_height) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = *line;
			for (uint32_t i = 0; i < rowsize; i += 3) {
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = 0xff;
				dst += 4;
				src += 3;
			}
			dst += skip;
		}
	} else if (components == 1) {
		while (m_cinfo.output_scanline < m_cinfo.output_height) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = *line;
			for (uint32_t i = 0; i < rowsize; i++) {
				dst[0] = *src;
				dst[1] = *src;
				dst[2] = *src;
				dst[3] = 0xff;
				dst += 4;
				src++;
			}
			dst += skip;
		}
	} else if (components == 4) {
		while (m_cinfo.output_scanline < m_cinfo.output_height) {
			if (cancelfunc != NULL && cancelfunc(userdata)) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_CANCELED);
				return false;
			}
			jpeg_read_scanlines(&m_cinfo, line, 1);
			const uint8_t *src = *line;
			for (uint32_t i = 0; i < m_cinfo.output_width; i++) {
				dst[0] = src[0] * src[3] / 0xff;
				dst[1] = src[1] * src[3] / 0xff;
				dst[2] = src[2] * src[3] / 0xff;
				dst[3] = 0xff;
				dst += 4;
				src += 4;
			}
			dst += skip;
		}
	} else {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT);
		return false;
	}
	jpeg_finish_decompress(&m_cinfo);
	return true;
}

/*
 * Creates the decompressor over m_source (a file when file is set, memory
 * otherwise) and reads the header into attr, when given.
 */
bool JpgCodec::ReadHeader(common::SharedPtr<File> file, ImageAttribute *attr,
			  ImageArray *images, ImageArray *cluts)
{
	m_cinfo.client_data = images->GetHeapAllocator();
	m_cinfo.err = jpeg_std_error(&m_error.pub);
	m_error.pub.error_exit = ErrorExit;

	int code = sce_paf_setjmp(m_error.setjmp_buffer);
	if (code != 0) {
		SetJpegErrorType(code);
		return false;
	}

	jpeg_create_decompress(&m_cinfo);
	if (file.get() != NULL)
		jpeg_paf_file_src(&m_cinfo, &m_source);
	else
		jpeg_paf_mem_src(&m_cinfo, &m_source);
	if (jpeg_read_header(&m_cinfo, 1) != JPEG_HEADER_OK)
		return false;

	SetLoadType(LoadType_Scaled);
	if (attr != NULL) {
		attr->m_extent = ImageExtent(m_cinfo.image_width, m_cinfo.image_height);
		attr->m_clip = ImageRect(0, 0, m_cinfo.image_width, m_cinfo.image_height);
		attr->m_format = ImageFormat_Jpg;
		attr->m_xtype = ImageXType_ConstantSize;
		attr->m_mode = ImageMode_RGBA8888;
		attr->m_order = ImageOrder_Linear;
		attr->m_ytype = ImageYType_Sequence;
		attr->m_pitchalign = 1;
		attr->m_heightalign = 1;

		JpgAttribute *jpg = static_cast<JpgAttribute *>(attr);
		jpg->m_progressive = m_cinfo.progressive_mode != 0;

		struct jpeg_decompress_struct cinfo = m_cinfo;
		const jpeg_component_info *comp = cinfo.comp_info;
		if (comp != NULL && cinfo.num_components == 3) {
			if (comp[0].h_samp_factor == 2 && comp[0].v_samp_factor == 2 &&
			    comp[1].h_samp_factor == 1 && comp[1].v_samp_factor == 1 &&
			    comp[2].h_samp_factor == 1 && comp[2].v_samp_factor == 1)
				jpg->m_sampling = 0;
			else if (comp[0].h_samp_factor == 2 && comp[0].v_samp_factor == 1 &&
				 comp[1].h_samp_factor == 1 && comp[1].v_samp_factor == 1 &&
				 comp[2].h_samp_factor == 1 && comp[2].v_samp_factor == 1)
				jpg->m_sampling = 1;
			else if (comp[0].h_samp_factor == 1 && comp[0].v_samp_factor == 1 &&
				 comp[1].h_samp_factor == 1 && comp[1].v_samp_factor == 1 &&
				 comp[2].h_samp_factor == 1 && comp[2].v_samp_factor == 1)
				jpg->m_sampling = 2;
			else
				jpg->m_sampling = -1;
		} else {
			jpg->m_sampling = -1;
		}
	}
	return true;
}

bool JpgCodec::LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			 const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			 DecodeCancelFunc cancelfunc, void *userdata)
{
	m_mutex.Lock();

	if (images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < attr->m_extent.w * attr->m_extent.h * 4) {
		m_mutex.Unlock();
		return false;
	}

	bool result = true;
	bool seeked;
	{
		common::SharedPtr<File> source = file;
		m_source.data = source.get();
		m_source.size = offset;
		m_source.pos = size;
		seeked = true;
		if (source->Seek(offset, SCE_SEEK_SET) < 0) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
			seeked = false;
		}
	}
	if (!seeked) {
		m_mutex.Unlock();
		return false;
	}
	bool read = ReadHeader(file, NULL, images, cluts);
	if (!read) {
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_subrect.origin.x != 0 || attr->m_subrect.origin.y != 0 ||
	    attr->m_subrect.extent.w != attr->m_originalextent.w ||
	    attr->m_subrect.extent.h != attr->m_originalextent.h) {
		if (!DecodeRegion(attr, images, cluts, cancelfunc, userdata)) {
			jpeg_destroy_decompress(&m_cinfo);
			m_mutex.Unlock();
			return false;
		}
	} else if (!DecodeWhole(attr, images, cluts, cancelfunc, userdata)) {
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return false;
	}
	jpeg_destroy_decompress(&m_cinfo);
	m_mutex.Unlock();
	return result;
}

bool JpgCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			 void *userdata)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || images == NULL || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);
	if (images->GetMax() != 1) {
		m_mutex.Unlock();
		return false;
	}
	const ImageBuffer *image = images->GetBuffer(0);
	if (image->buff == NULL || image->size < attr->m_extent.w * attr->m_extent.h * 4) {
		m_mutex.Unlock();
		return false;
	}

	{
		common::SharedPtr<Buffer> source = buf;
		m_source.data = source->GetAddr();
		m_source.size = source->GetSize();
		m_source.pos = 0;
	}
	bool read = ReadHeader(common::SharedPtr<File>(), NULL, images, cluts);
	if (!read) {
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return false;
	}
	if (attr->m_subrect.origin.x != 0 || attr->m_subrect.origin.y != 0 ||
	    attr->m_subrect.extent.w != attr->m_originalextent.w ||
	    attr->m_subrect.extent.h != attr->m_originalextent.h) {
		if (!DecodeRegion(attr, images, cluts, cancelfunc, userdata)) {
			jpeg_destroy_decompress(&m_cinfo);
			m_mutex.Unlock();
			return false;
		}
	} else if (!DecodeWhole(attr, images, cluts, cancelfunc, userdata)) {
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return false;
	}
	jpeg_destroy_decompress(&m_cinfo);
	m_mutex.Unlock();
	return true;
}

bool JpgCodec::LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	return false;
}

bool JpgCodec::GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			 ImageAttribute *attr, ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);

	bool result = true;
	bool seeked;
	{
		common::SharedPtr<File> source = file;
		m_source.data = source.get();
		m_source.size = offset;
		m_source.pos = size;
		seeked = true;
		if (source->Seek(offset, SCE_SEEK_SET) < 0) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_SEEK);
			seeked = false;
		}
	}
	if (!seeked) {
		m_mutex.Unlock();
		return false;
	}
	bool read = ReadHeader(file, attr, images, cluts);
	if (read) {
		images->Init(1, 1);
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return result;
	}
	jpeg_destroy_decompress(&m_cinfo);
	m_mutex.Unlock();
	return false;
}

bool JpgCodec::GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			 ImageArray *images, ImageArray *cluts)
{
	m_mutex.Lock();

	if (buf->GetAddr() == NULL || buf->GetSize() == 0 || attr == NULL) {
		m_mutex.Unlock();
		return false;
	}
	Image::SetErrorType(SCE_OK);

	{
		common::SharedPtr<Buffer> source = buf;
		m_source.data = source->GetAddr();
		m_source.size = source->GetSize();
		m_source.pos = 0;
	}
	bool read = ReadHeader(common::SharedPtr<File>(), attr, images, cluts);
	if (read) {
		images->Init(1, 1);
		jpeg_destroy_decompress(&m_cinfo);
		m_mutex.Unlock();
		return true;
	}
	jpeg_destroy_decompress(&m_cinfo);
	m_mutex.Unlock();
	return false;
}

/* The size the whole image decodes to for a result of extent; sets m_scale. */
ImageExtent JpgCodec::CalcScaledExtent(const ImageExtent &original, const ImageExtent &extent)
{
	int32_t w = original.w;
	if (w < extent.w || original.h < extent.h)
		return original;

	int32_t h = original.h;
	int32_t scalex = CalcScale(w, extent.w);
	int32_t scaley = CalcScale(h, extent.h);
	int32_t scale = scaley;
	if (scalex < scaley)
		scale = scalex;
	m_scale = scale;
	return ImageExtent(DivideRoundUp(w, scale), DivideRoundUp(h, scale));
}

bool JpgCodec::CalcDecodeExtent(const ImageExtent &original, const ImageRect &subrect,
				const ImageExtent &extent, ImageExtent *out)
{
	int32_t w = original.w;
	if (w <= 0)
		return false;
	int32_t h = original.h;
	if (h <= 0 || extent.w <= 0 || extent.h <= 0 || subrect.extent.w <= 0 ||
	    subrect.extent.h <= 0)
		return false;

	if (subrect.origin.x == 0 && subrect.origin.y == 0 && subrect.extent.w == w &&
	    subrect.extent.h == h) {
		if (w == extent.w && h == extent.h) {
			out->w = w;
			out->h = original.h;
			m_scale = 1;
			return true;
		}
		*out = CalcScaledExtent(original, extent);
		return true;
	}

	if (subrect.extent.w < extent.w || subrect.extent.h < extent.h) {
		out->w = subrect.extent.w;
		out->h = subrect.extent.h;
		m_scale = 1;
		return true;
	}

	/* The whole image's size at the scale the subrect needs. */
	float fw = (float)w * ((float)extent.w / (float)subrect.extent.w);
	float fh = (float)h * ((float)extent.h / (float)subrect.extent.h);
	if (fw - (float)(int32_t)fw != 0.0f)
		fw += 1.0f;
	if (fh - (float)(int32_t)fh != 0.0f)
		fh += 1.0f;

	int32_t scalex = CalcScale(w, (int32_t)fw);
	int32_t scaley = CalcScale(h, (int32_t)fh);
	int32_t scale = scaley;
	if (scalex < scaley)
		scale = scalex;
	m_scale = scale;
	out->w = DivideRoundUp(subrect.extent.w, scale);
	out->h = DivideRoundUp(subrect.extent.h, scale);
	return true;
}

JpgCodec::JpgCodec() : m_mutex("ScePafImageJpgMutex", 0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	m_reserved[3] = 0;
	m_reserved[4] = 0;
	m_scale = 1;
}

JpgCodec::~JpgCodec()
{
}

/* libjpeg's error_exit: back to the setjmp of the call that failed. */
static void ErrorExit(j_common_ptr cinfo)
{
	sce_paf_longjmp(((JpgCodec::ErrorManager *)cinfo->err)->setjmp_buffer,
			cinfo->err->msg_code);
}
