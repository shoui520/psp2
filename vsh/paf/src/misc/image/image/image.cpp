/* paf::image::Image: opening, decoding and operating on images. */
#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/stdlib.h>
#include "image_operation.h"
#include "image_source.h"
extern "C" {
#include "../../../paf_types.h"
}

using namespace paf;
using namespace paf::image;
using paf::memory::HeapAllocator;

static int32_t s_errortype;

Image::~Image()
{
	if (m_imageattr != NULL) {
		delete m_imageattr;
		m_imageattr = NULL;
	}
	if (m_imagesrc != NULL) {
		delete m_imagesrc;
		m_imagesrc = NULL;
	}
	if (m_openext_mem_addr != NULL) {
		if (m_openext_mem_allocator != NULL)
			m_openext_mem_allocator->sce_paf_free(m_openext_mem_addr);
		else
			sce_paf_free(m_openext_mem_addr);
		m_openext_mem_addr = NULL;
		m_openext_mem_size = 0;
	}
}

ImageExtent Image::GetExtent() const
{
	return m_imageattr->m_extent;
}

common::SharedPtr<Image> Image::OpenSource(ImageSource *source, HeapAllocator *allocator,
					   ImageFormat format)
{
	Image *image = new Image(allocator);

	if (image == NULL)
		return common::SharedPtr<Image>();
	image->m_imagesrc = source;
	if (!source->GetHeader(format, &image->m_imageattr, &image->m_images, &image->m_cluts)) {
		delete image;
		return common::SharedPtr<Image>();
	}

	ImageExtent extent = image->m_imageattr->m_extent;
	image->m_imageattr->m_originalextent = extent;
	image->m_imageattr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	image->m_imageattr->m_subrect = ImageRect(0, 0, extent.w, extent.h);
	return common::SharedPtr<Image>(image);
}

bool Image::SetErrorType(int32_t error)
{
	s_errortype = error;
	return true;
}

ImageFormat Image::DetectFormatInMemory(void *addr, size_t size)
{
	common::SharedPtr<UnmanagedBuffer> buf = UnmanagedBuffer::Create(addr, size);

	if (buf.get() == NULL)
		return ImageFormat_None;
	if (IsGim(buf))
		return ImageFormat_Gim;
	if (IsPng(buf))
		return ImageFormat_Png;
	if (IsJpg(buf))
		return ImageFormat_Jpg;
	if (IsTif(buf))
		return ImageFormat_Tif;
	if (IsGif(buf))
		return ImageFormat_Gif;
	if (IsBmp(buf))
		return ImageFormat_Bmp;
	if (IsDds(buf))
		return ImageFormat_Dds;
	if (IsGxt(buf))
		return ImageFormat_Gxt;
	s_errortype = SCE_PAF_ERROR_IMAGE_UNKNOWN_FORMAT;
	return ImageFormat_None;
}

ImageFormat Image::DetectFormat(void *addr, size_t size)
{
	if (addr == NULL)
		return ImageFormat_None;
	return DetectFormatInMemory(addr, size);
}

common::SharedPtr<Image> Image::OpenGxt(void *addr, size_t size, HeapAllocator *allocator)
{
	if (addr == NULL || DetectFormatInMemory(addr, size) != ImageFormat_Gxt)
		return common::SharedPtr<Image>();

	GxtDecoder *decoder = new GxtDecoder(addr, size);
	if (decoder == NULL || decoder->m_buffer.get() == NULL)
		return common::SharedPtr<Image>();
	return OpenSource(decoder, allocator, ImageFormat_Gxt);
}

bool Image::ReadFile(common::SharedPtr<File> file, HeapAllocator *allocator, int64_t offset,
		     size_t size, void **addr, size_t *readsize)
{
	size_t filesize = file->GetFileSize();

	if (offset != 0) {
		if (file->Seek(offset, SCE_SEEK_SET) < 0) {
			s_errortype = SCE_PAF_ERROR_IMAGE_SEEK;
			return false;
		}
		filesize = size;
	}

	if (allocator != NULL)
		*addr = allocator->sce_paf_malloc(filesize);
	else
		*addr = sce_paf_malloc(filesize);
	if (*addr == NULL) {
		file->Seek(0, SCE_SEEK_SET);
		return false;
	}

	*readsize = file->Read(*addr, filesize);
	if (*readsize != filesize) {
		if (allocator != NULL)
			allocator->sce_paf_free(*addr);
		else
			sce_paf_free(*addr);
		*addr = NULL;
		file->Seek(0, SCE_SEEK_SET);
		s_errortype = SCE_PAF_ERROR_IMAGE_READ;
		return false;
	}
	file->Seek(0, SCE_SEEK_SET);
	return true;
}

common::SharedPtr<Image> Image::OpenDds(void *addr, size_t size, HeapAllocator *allocator)
{
	if (addr == NULL || DetectFormatInMemory(addr, size) != ImageFormat_Dds)
		return common::SharedPtr<Image>();

	DdsDecoder *decoder = new DdsDecoder(addr, size);
	if (decoder == NULL || decoder->m_buffer.get() == NULL)
		return common::SharedPtr<Image>();
	return OpenSource(decoder, allocator, ImageFormat_Dds);
}

common::SharedPtr<Image> Image::OpenGim(void *addr, size_t size, HeapAllocator *allocator)
{
	if (addr == NULL || DetectFormatInMemory(addr, size) != ImageFormat_Gim)
		return common::SharedPtr<Image>();

	GimDecoder *decoder = new GimDecoder(addr, size);
	if (decoder == NULL || decoder->m_buffer.get() == NULL)
		return common::SharedPtr<Image>();
	return OpenSource(decoder, allocator, ImageFormat_Gim);
}

ImageFormat Image::DetectFormat(const char *filename, int64_t offset, size_t size)
{
	int32_t error;
	uint8_t header[16];
	common::SharedPtr<File> file = LocalFile::Open(filename, 0x10001, 0, &error);

	if (file.get() == NULL)
		return ImageFormat_None;
	if (offset != 0 && file->Seek(offset, SCE_SEEK_SET) < 0) {
		file->Close();
		s_errortype = SCE_PAF_ERROR_IMAGE_SEEK;
		return ImageFormat_None;
	}
	if (file->Read(header, sizeof(header)) != sizeof(header)) {
		file->Close();
		s_errortype = SCE_PAF_ERROR_IMAGE_READ;
		return ImageFormat_None;
	}
	file->Close();
	return DetectFormatInMemory(header, sizeof(header));
}

/*
 * Reads a GIM, DDS or GXT file whole and opens it from memory; the image
 * frees the memory with itself.
 */
#define OPEN_FROM_FILE(open, file)							\
	do {										\
		void *addr = NULL;							\
		size_t readsize = 0;							\
		bool read = ReadFile(file, allocator, offset, size, &addr, &readsize);	\
											\
		if (!read) {								\
			file->Close();							\
			return common::SharedPtr<Image>();				\
		}									\
		file->Close();								\
		common::SharedPtr<Image> image = open(addr, readsize, allocator);	\
		if (image.get() != NULL) {						\
			image->m_openext_mem_addr = addr;				\
			image->m_openext_mem_size = readsize;				\
			image->m_openext_mem_allocator = allocator;			\
		} else if (addr != NULL) {						\
			if (allocator != NULL)						\
				allocator->sce_paf_free(addr);				\
			else								\
				sce_paf_free(addr);					\
			addr = NULL;							\
			readsize = 0;							\
		}									\
		return image;								\
	} while (0)

common::SharedPtr<Image> Image::Open(const char *filename, HeapAllocator *allocator,
				     ImageFormat format, int64_t offset, size_t size)
{
	if (format == ImageFormat_None)
		format = DetectFormat(filename, offset, size);

	switch (format) {
	case ImageFormat_Png:
	case ImageFormat_Jpg:
	case ImageFormat_Tif:
	case ImageFormat_Gif:
	case ImageFormat_Bmp: {
		ImageFileDecoder *decoder = new ImageFileDecoder(filename, offset, size);
		if (decoder == NULL || decoder->m_file.get() == NULL) {
			if (decoder != NULL)
				delete decoder;
			return common::SharedPtr<Image>();
		}
		return OpenSource(decoder, allocator, format);
	}
	case ImageFormat_Gim: {
		int32_t error;
		common::SharedPtr<File> file = LocalFile::Open(filename, 0x10001, 0, &error);
		if (file.get() == NULL)
			return common::SharedPtr<Image>();
		OPEN_FROM_FILE(OpenGim, file);
	}
	case ImageFormat_Dds: {
		int32_t error;
		common::SharedPtr<File> file = LocalFile::Open(filename, 0x10001, 0, &error);
		if (file.get() == NULL)
			return common::SharedPtr<Image>();
		OPEN_FROM_FILE(OpenDds, file);
	}
	case ImageFormat_Gxt: {
		int32_t error;
		common::SharedPtr<File> file = LocalFile::Open(filename, 0x10001, 0, &error);
		if (file.get() == NULL)
			return common::SharedPtr<Image>();
		OPEN_FROM_FILE(OpenGxt, file);
	}
	default:
		return common::SharedPtr<Image>();
	}
}

ImageFormat Image::DetectFormat(common::SharedPtr<File> file, int64_t offset, size_t size)
{
	uint8_t header[16];

	if (offset != 0 && file->Seek(offset, SCE_SEEK_SET) < 0) {
		s_errortype = SCE_PAF_ERROR_IMAGE_SEEK;
		return ImageFormat_None;
	}
	if (file->Read(header, sizeof(header)) != sizeof(header)) {
		file->Seek(0, SCE_SEEK_SET);
		s_errortype = SCE_PAF_ERROR_IMAGE_READ;
		return ImageFormat_None;
	}
	file->Seek(0, SCE_SEEK_SET);
	return DetectFormatInMemory(header, sizeof(header));
}

common::SharedPtr<Image> Image::Open(common::SharedPtr<File> file, HeapAllocator *allocator,
				     ImageFormat format, int64_t offset, size_t size)
{
	if (format == ImageFormat_None)
		format = DetectFormat(file, offset, size);

	switch (format) {
	case ImageFormat_Png:
	case ImageFormat_Jpg:
	case ImageFormat_Tif:
	case ImageFormat_Gif:
	case ImageFormat_Bmp: {
		ImageFileDecoder *decoder = new ImageFileDecoder(file, offset, size);
		if (decoder == NULL || decoder->m_file.get() == NULL) {
			if (decoder != NULL)
				delete decoder;
			return common::SharedPtr<Image>();
		}
		return OpenSource(decoder, allocator, format);
	}
	case ImageFormat_Gim:
		OPEN_FROM_FILE(OpenGim, file);
	case ImageFormat_Dds:
		OPEN_FROM_FILE(OpenDds, file);
	case ImageFormat_Gxt:
		OPEN_FROM_FILE(OpenGxt, file);
	default:
		return common::SharedPtr<Image>();
	}
}

common::SharedPtr<Image> Image::Open(void *addr, size_t size, HeapAllocator *allocator,
				     ImageFormat format)
{
	if (addr == NULL || size == 0)
		return common::SharedPtr<Image>();
	if (format == ImageFormat_None)
		format = DetectFormatInMemory(addr, size);

	switch (format) {
	case ImageFormat_Png:
	case ImageFormat_Jpg:
	case ImageFormat_Tif:
	case ImageFormat_Gif:
	case ImageFormat_Bmp: {
		ImageDecoder *decoder = new ImageDecoder(addr, size);
		if (decoder == NULL || decoder->m_buffer.get() == NULL) {
			if (decoder != NULL)
				delete decoder;
			return common::SharedPtr<Image>();
		}
		return OpenSource(decoder, allocator, format);
	}
	case ImageFormat_Gim:
		return OpenGim(addr, size, allocator);
	case ImageFormat_Dds:
		return OpenDds(addr, size, allocator);
	case ImageFormat_Gxt:
		return OpenGxt(addr, size, allocator);
	default:
		return common::SharedPtr<Image>();
	}
}

common::SharedPtr<Image> Image::Open(common::SharedPtr<Buffer> buf, HeapAllocator *allocator,
				     ImageFormat format)
{
	void *addr = NULL;
	size_t size = 0;

	if (buf.get() != NULL) {
		addr = buf->GetAddr();
		size = buf->GetSize();
	}
	return Open(addr, size, allocator, format);
}

common::SharedPtr<Image> Image::CreateFromSource(ImageSource *source, uint32_t pitch,
						 const ImageExtent &ext, ImageMode mode,
						 HeapAllocator *allocator)
{
	Image *image = new Image(allocator);

	if (image == NULL)
		return common::SharedPtr<Image>();
	image->m_imagesrc = source;
	image->m_imageattr = new ImageAttribute;
	if (image->m_imageattr == NULL ||
	    !((RawImageSource *)source)->SetHeader(pitch, ext, mode, image->m_imageattr,
						   &image->m_images, &image->m_cluts)) {
		delete image;
		return common::SharedPtr<Image>();
	}

	ImageExtent extent = ext;
	ImageAttribute *attr = image->m_imageattr;
	attr->m_originalextent = extent;
	attr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	attr->m_subrect = ImageRect(0, 0, extent.w, extent.h);
	return common::SharedPtr<Image>(image);
}

common::SharedPtr<Image> Image::CreateFromBuffer(void *addr, size_t size, void *clut,
						 size_t clutsize, uint32_t pitch,
						 const ImageExtent &ext, ImageMode mode,
						 HeapAllocator *allocator)
{
	RawImageSource *source = new RawImageSource(addr, size, clut, clutsize, allocator);

	if (source == NULL || (source->m_buffer.get() == NULL && source->m_clut.get() == NULL))
		return common::SharedPtr<Image>();
	return CreateFromSource(source, pitch, ext, mode, allocator);
}

common::SharedPtr<Image> Image::CreateFromBuffer(common::SharedPtr<Buffer> buf,
						 common::SharedPtr<Buffer> clut, uint32_t pitch,
						 const ImageExtent &ext, ImageMode mode,
						 HeapAllocator *allocator)
{
	void *addr = NULL;
	void *clutaddr = NULL;
	size_t size = 0;
	size_t clutsize = 0;

	if (buf.get() != NULL) {
		addr = buf->GetAddr();
		size = buf->GetSize();
	}
	if (clut.get() != NULL) {
		clutaddr = clut->GetAddr();
		clutsize = clut->GetSize();
	}
	return CreateFromBuffer(addr, size, clutaddr, clutsize, pitch, ext, mode, allocator);
}

ImageFormat Image::DetectFormat(common::SharedPtr<Buffer> buf)
{
	if (buf.get() == NULL)
		return ImageFormat_None;

	void *addr = buf->GetAddr();
	size_t size = buf->GetSize();
	if (addr == NULL)
		return ImageFormat_None;
	return DetectFormatInMemory(addr, size);
}

bool Image::SetDecErrorType(int32_t error)
{
	if (m_imagesrc == NULL)
		return false;
	return m_imagesrc->SetErrorType(error);
}

ImageRect Image::GetSubRect() const
{
	return m_imageattr->m_subrect;
}

ImageRect Image::GetClip() const
{
	return m_imageattr->m_clip;
}

bool Image::Load(bool delete_flag)
{
	if (m_imageattr->m_clip.extent.w <= 0 || m_imageattr->m_clip.extent.h <= 0 ||
	    m_imageattr->m_subrect.extent.w <= 0 || m_imageattr->m_subrect.extent.h <= 0) {
		s_errortype = SCE_PAF_ERROR_IMAGE_INVALID_DATA;
		if (m_imagesrc != NULL)
			m_imagesrc->SetErrorType(SCE_PAF_ERROR_IMAGE_INVALID_DATA);
		return false;
	}
	if (m_images.GetMax() == 0 && m_cluts.GetMax() == 0)
		return false;
	if (!m_images.IsEmpty() || !m_cluts.IsEmpty())
		return true;
	if (m_imagesrc == NULL)
		return false;
	return m_imagesrc->LoadImage(m_imageattr, &m_images, &m_cluts, delete_flag, m_cancelfunc,
				     m_cancelfunc_userdata);
}

bool Image::UnLoad()
{
	bool images = m_images.Free();
	bool cluts = m_cluts.Free();

	return images && cluts;
}

ImageBuffer Image::ToBuffer(bool delete_flag)
{
	if (m_images.GetMax() == 0 || !Load(delete_flag))
		return ImageBuffer();
	return *m_images.GetBuffer(m_current);
}

ImageBuffer Image::ToCLUTBuffer(bool delete_flag)
{
	if (m_cluts.GetMax() == 0 || !Load(delete_flag))
		return ImageBuffer();
	return *m_cluts.GetBuffer(m_current_clut);
}

ImageMode Image::GetMode() const
{
	return m_imageattr->m_mode;
}

ImageMode Image::GetCLUTMode() const
{
	return m_imageattr->m_clutmode;
}

ImageOrder Image::GetOrder() const
{
	return m_imageattr->m_order;
}

uint32_t Image::GetCLUTSize() const
{
	return m_imageattr->m_clutsize;
}

ImageXType Image::GetXType() const
{
	return m_imageattr->m_xtype;
}

ImageYType Image::GetYType() const
{
	return m_imageattr->m_ytype;
}

ImagePoint Image::GetArraySize() const
{
	return m_images.GetArraySize();
}

ImagePoint Image::GetCLUTArraySize() const
{
	return m_cluts.GetArraySize();
}

size_t Image::GetMemPitch(uint32_t index) const
{
	return m_imageattr->GetMemPitch(index);
}

uint32_t Image::GetMemHeight(uint32_t index) const
{
	return m_imageattr->GetMemHeight(index);
}

uint32_t Image::GetPitchAlign() const
{
	return m_imageattr->m_pitchalign;
}

/* Whether align is a power of two. */
bool Image::CheckAlignArgument(int32_t align)
{
	if (align <= 0)
		return false;

	int bits = 0;
	for (int i = 0; i < 31; i++) {
		if (align & 1)
			bits++;
		align >>= 1;
	}
	return bits == 1;
}

bool Image::SetPitchAlign(uint32_t align)
{
	if (!CheckAlignArgument(align))
		return false;
	m_imageattr->m_pitchalign = align;
	return true;
}

uint32_t Image::GetHeightAlign() const
{
	return m_imageattr->m_heightalign;
}

bool Image::SetHeightAlign(uint32_t align)
{
	if (!CheckAlignArgument(align))
		return false;
	m_imageattr->m_heightalign = align;
	return true;
}

bool Image::GetAttribute(uint32_t key, bool &value)
{
	return m_imageattr->GetAttribute(key, value);
}

bool Image::GetAttribute(uint32_t key, int &value)
{
	return m_imageattr->GetAttribute(key, value);
}

bool Image::GetAttribute(uint32_t key, float &value)
{
	return m_imageattr->GetAttribute(key, value);
}

bool Image::GetAttribute(uint32_t key, string &value)
{
	return m_imageattr->GetAttribute(key, value);
}

ImagePoint Image::GetArrayPos() const
{
	return m_current;
}

bool Image::SetArrayPos(ImagePoint &point)
{
	if (point.x < 0 || point.y < 0)
		return false;

	ImagePoint size = m_images.GetArraySize();
	if (point.x >= size.x || point.y >= size.y)
		return false;
	m_current = point;
	return true;
}

ImagePoint Image::GetArrayPosCLUT() const
{
	return m_current_clut;
}

bool Image::SetArrayPosCLUT(ImagePoint &point)
{
	if (point.x < 0 || point.y < 0)
		return false;

	ImagePoint size = m_cluts.GetArraySize();
	if (point.x >= size.x || point.y >= size.y)
		return false;
	m_current_clut = point;
	return true;
}

ImageFormat Image::GetFormat() const
{
	return m_imageattr->m_format;
}

ImageExtent Image::GetOriginalExtent() const
{
	return m_imageattr->m_originalextent;
}

/* Only the codecs decode at another size: not GIM, DDS or GXT. */
bool Image::SetDecExtent(const ImageExtent &extent, ImageResizeType type)
{
	if (extent.w <= 0 || extent.h <= 0)
		return false;

	ImageAttribute *attr = m_imageattr;
	ImageFormat format = attr->m_format;
	if (format == ImageFormat_None || format == ImageFormat_Gim || format == ImageFormat_Dds ||
	    format == ImageFormat_Gxt)
		return false;
	attr->m_extent = extent;
	attr->m_clip = ImageRect(0, 0, extent.w, extent.h);
	attr->m_resizetype = type;
	return true;
}

/* The part of the original image to decode, cut to the image. */
bool Image::SetDecSubRect(const ImageRect &rect)
{
	ImageAttribute *attr = m_imageattr;
	ImageFormat format = attr->m_format;
	if (format == ImageFormat_None || format == ImageFormat_Gim || format == ImageFormat_Dds ||
	    format == ImageFormat_Gxt)
		return false;

	int left = 0;
	int top = 0;
	int right = rect.origin.x + rect.extent.w;
	int bottom = rect.origin.y + rect.extent.h;
	if (rect.origin.x > 0)
		left = rect.origin.x;
	if (rect.origin.y > 0)
		top = rect.origin.y;
	if (right >= attr->m_originalextent.w)
		right = attr->m_originalextent.w;
	int width = right - left;
	if (bottom >= attr->m_originalextent.h)
		bottom = attr->m_originalextent.h;
	int height = bottom - top;
	if (width <= 0 || height <= 0)
		return false;
	attr->m_subrect = ImageRect(left, top, width, height);
	return true;
}

/* The part of the decoded image to use, cut to the image. */
bool Image::SetClip(const ImageRect &rect)
{
	ImageAttribute *attr = m_imageattr;
	int left = 0;
	int top = 0;
	int right = rect.origin.x + rect.extent.w;
	int bottom = rect.origin.y + rect.extent.h;

	if (rect.origin.x > 0)
		left = rect.origin.x;
	if (rect.origin.y > 0)
		top = rect.origin.y;
	if (right >= attr->m_extent.w)
		right = attr->m_extent.w;
	int width = right - left;
	if (bottom >= attr->m_extent.h)
		bottom = attr->m_extent.h;
	int height = bottom - top;
	if (width <= 0 || height <= 0)
		return false;
	attr->m_clip = ImageRect(left, top, width, height);
	return true;
}

/* A new image whose source is operation, run on this image. */
common::SharedPtr<Image> Image::Operate(ImageOperation *operation)
{
	Image *image = new Image(m_images.GetHeapAllocator());

	if (image == NULL)
		return common::SharedPtr<Image>();
	image->m_imagesrc = operation;
	image->m_imageattr = new ImageAttribute;
	if (image->m_imageattr == NULL ||
	    !operation->SetParent(this, image->m_imageattr, &image->m_images, &image->m_cluts)) {
		delete image;
		return common::SharedPtr<Image>();
	}
	return common::SharedPtr<Image>(image);
}

common::SharedPtr<Image> Image::ConvertMode(ImageMode mode, ImageConvertModeType type)
{
	ImageOperation *operation;

	switch (mode) {
	case ImageMode_RGBA8888:
		operation = new ConvertToRGBA8888Operation(type, m_images.GetHeapAllocator());
		break;
	case ImageMode_DXT1:
		operation = new ConvertToDXT1Operation(type, m_images.GetHeapAllocator());
		break;
	case ImageMode_DXT3:
		operation = new ConvertToDXT3Operation(type, m_images.GetHeapAllocator());
		break;
	case ImageMode_DXT5:
		operation = new ConvertToDXT5Operation(type, m_images.GetHeapAllocator());
		break;
	default:
		operation = NULL;
		break;
	}
	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Copy()
{
	ImageOperation *operation = new CopyOperation(m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Paste(common::SharedPtr<Image> image, const ImagePoint &point)
{
	ImageOperation *operation =
		new PasteOperation(image, point, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Crop(const ImageRect &crop)
{
	ImageOperation *operation = new CropOperation(crop, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::CopyWithEdge(uint32_t extlines)
{
	ImageOperation *operation;

	if (extlines != 0)
		operation = new CopyWithEdgeOperation(extlines, m_images.GetHeapAllocator());
	else
		operation = new CopyOperation(m_images.GetHeapAllocator());
	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::FadeWithEdge()
{
	ImageOperation *operation = new FadeWithEdgeOperation(m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Resize(const ImageExtent &extent, ImageResizeType type)
{
	ImageOperation *operation =
		new ResizeOperation(extent, type, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

/* Only to RGBA8888 when the size changes too. */
common::SharedPtr<Image> Image::ResizeWithConvertMode(const ImageExtent &extent,
						      ImageResizeType resize_type, ImageMode mode,
						      ImageConvertModeType type)
{
	ImageAttribute *attr = m_imageattr;

	if (attr->m_mode == mode && attr->m_extent == extent)
		return Copy();
	if (attr->m_mode == mode && attr->m_extent != extent)
		return Resize(extent, resize_type);
	if (attr->m_mode != mode && attr->m_extent == extent)
		return ConvertMode(mode, type);
	if (mode == ImageMode_RGBA8888) {
		ImageOperation *operation = new ResizeWithConvertModeOperation(
			extent, resize_type, mode, type, m_images.GetHeapAllocator());
		if (operation != NULL)
			return Operate(operation);
	}
	return common::SharedPtr<Image>();
}

common::SharedPtr<Image> Image::Rotate(ImageRotateType type)
{
	ImageOperation *operation;

	if (type != 0)
		operation = new RotateOperation(type, m_images.GetHeapAllocator());
	else
		operation = new CopyOperation(m_images.GetHeapAllocator());
	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Flip(ImageFlipType type)
{
	ImageOperation *operation;

	if (type != 0)
		operation = new FlipOperation(type, m_images.GetHeapAllocator());
	else
		operation = new CopyOperation(m_images.GetHeapAllocator());
	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Blur(float sigma)
{
	ImageOperation *operation = new BlurOperation(sigma, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Sharpen(float amount, int32_t threshold)
{
	ImageOperation *operation =
		new SharpenOperation(amount, threshold, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Transparentize(float alpha)
{
	ImageOperation *operation =
		new TransparentizeOperation(alpha, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::RoundCorner(int32_t width, int32_t height)
{
	ImageOperation *operation =
		new RoundCornerOperation(width, height, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

common::SharedPtr<Image> Image::Blend(common::SharedPtr<Image> image, float alpha)
{
	ImageOperation *operation =
		new BlendOperation(image, alpha, m_images.GetHeapAllocator());

	if (operation == NULL)
		return common::SharedPtr<Image>();
	return Operate(operation);
}

int32_t Image::SaveAsPng(const char *filename, const void *addr, uint32_t pitch,
			 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			 HeapAllocator *allocator)
{
	return SavePng(filename, addr, pitch, ext, mode, io_mode, allocator);
}

int32_t Image::SaveAsPng(void *dst, uint32_t max_size, uint32_t &written, const void *addr,
			 uint32_t pitch, const ImageExtent &ext, ImageMode mode,
			 HeapAllocator *allocator)
{
	return SavePng(dst, max_size, written, addr, pitch, ext, mode, allocator);
}

int32_t Image::SaveAsBmp(const char *filename, const void *addr, uint32_t pitch,
			 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			 HeapAllocator *allocator)
{
	return SaveBmp(filename, addr, pitch, ext, mode, io_mode, allocator);
}

int32_t Image::SaveAsDds(const char *filename, const void *addr, uint32_t pitch,
			 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
			 HeapAllocator *allocator)
{
	return SaveDds(filename, addr, pitch, ext, mode, io_mode, allocator);
}

int32_t Image::SaveAsDds(void *dst, uint32_t max_size, const void *addr, uint32_t pitch,
			 const ImageExtent &ext, ImageMode mode, uint32_t io_mode)
{
	return SaveDds(dst, max_size, addr, pitch, ext, mode, io_mode);
}

bool Image::SetDecodeCancelFunc(DecodeCancelFunc cancelfunc, void *userdata)
{
	m_cancelfunc = cancelfunc;
	m_cancelfunc_userdata = userdata;
	return true;
}

/* A palette image needs one palette, or one per image. */
bool Image::CheckCLUT() const
{
	ImageMode mode = m_imageattr->m_mode;

	if (mode == ImageMode_index4 || mode == ImageMode_index8 || mode == ImageMode_index16 ||
	    mode == ImageMode_index32) {
		if (!(m_cluts.GetArraySize() == m_images.GetArraySize()) &&
		    !(m_cluts.GetArraySize() == ImagePoint(1, 1)))
			return false;
	}
	return true;
}

int32_t Image::GetDecErrorType() const
{
	if (m_imagesrc == NULL)
		return 0;
	return m_imagesrc->GetErrorType();
}

int32_t Image::GetErrorType()
{
	return s_errortype;
}

bool Image::HasImage() const
{
	return !m_images.IsEmpty() || !m_cluts.IsEmpty() || m_imagesrc != NULL;
}

const ImageArray &Image::GetImage() const
{
	return m_images;
}

const ImageArray &Image::GetCLUT() const
{
	return m_cluts;
}

ImageAttribute *Image::GetAttribute() const
{
	return m_imageattr;
}
