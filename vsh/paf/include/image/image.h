/*
 * paf::image::Image: an image opened from a file or memory, decoded on Load
 * into its buffers, and the operations that make a new image from it. An
 * ImageSource gives an image its header and pixels: a decoder, or an
 * ImageOperation that works on another image.
 */
#ifndef PAF_IMAGE_IMAGE_H
#define PAF_IMAGE_IMAGE_H

#include <stddef.h>
#include <stdint.h>
#include <image/image_types.h>
#include <image/image_region.h>
#include <image/image_array.h>
#include <image/image_attribute.h>
#include <common/shared_ptr.h>
#include <std/string>

namespace paf {

class Buffer;
class File;

namespace memory {
class HeapAllocator;
}

namespace image {

class Image;

typedef bool (*DecodeCancelFunc)(void *userdata);

class ImageSource {
public:
	ImageSource()
	{
	}

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts) = 0;
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata) = 0;
	virtual int32_t GetErrorType() = 0;
	virtual bool SetErrorType(int32_t type) = 0;

	virtual ~ImageSource()
	{
	}
};

class ImageOperation : public ImageSource {
public:
	ImageOperation(memory::HeapAllocator *allocator);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~ImageOperation();
	virtual bool Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
			  const ImageArray *srccluts, ImageAttribute *dstattr, ImageArray *dstimages,
			  ImageArray *dstcluts);
	virtual bool OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
				  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
				  ImageAttribute *dstattr, ImageBuffer *dstimage, ImageArray *dstclut) = 0;

	/* Takes parent as the image to work on and makes the header of the result. */
	bool SetParent(Image *parent, ImageAttribute *dstattr, ImageArray *dstimages,
		       ImageArray *dstcluts);

protected:
	memory::HeapAllocator *m_allocator;
	Image *m_parent;
	int32_t m_errortype;
};

class Image {
public:
	static ImageFormat DetectFormat(void *addr, size_t size);
	static ImageFormat DetectFormat(const char *filename, int64_t offset, size_t size);
	static ImageFormat DetectFormat(common::SharedPtr<File> file, int64_t offset, size_t size);
	static ImageFormat DetectFormat(common::SharedPtr<Buffer> buf);

	static common::SharedPtr<Image> Open(const char *filename, memory::HeapAllocator *allocator,
					     ImageFormat format, int64_t offset, size_t size);
	static common::SharedPtr<Image> Open(common::SharedPtr<File> file,
					     memory::HeapAllocator *allocator, ImageFormat format,
					     int64_t offset, size_t size);
	static common::SharedPtr<Image> Open(void *addr, size_t size,
					     memory::HeapAllocator *allocator, ImageFormat format);
	static common::SharedPtr<Image> Open(common::SharedPtr<Buffer> buf,
					     memory::HeapAllocator *allocator, ImageFormat format);

	/* An image over pixels (and a palette) already in memory, rows pitch bytes apart. */
	static common::SharedPtr<Image> CreateFromBuffer(void *addr, size_t size, void *clut,
							 size_t clutsize, uint32_t pitch,
							 const ImageExtent &ext, ImageMode mode,
							 memory::HeapAllocator *allocator);
	static common::SharedPtr<Image> CreateFromBuffer(common::SharedPtr<Buffer> buf,
							 common::SharedPtr<Buffer> clut,
							 uint32_t pitch, const ImageExtent &ext,
							 ImageMode mode,
							 memory::HeapAllocator *allocator);

	static int32_t SaveAsPng(const char *filename, const void *addr, uint32_t pitch,
				 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
				 memory::HeapAllocator *allocator);
	static int32_t SaveAsPng(void *dst, uint32_t max_size, uint32_t &written, const void *addr,
				 uint32_t pitch, const ImageExtent &ext, ImageMode mode,
				 memory::HeapAllocator *allocator);
	static int32_t SaveAsBmp(const char *filename, const void *addr, uint32_t pitch,
				 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
				 memory::HeapAllocator *allocator);
	static int32_t SaveAsDds(const char *filename, const void *addr, uint32_t pitch,
				 const ImageExtent &ext, ImageMode mode, uint32_t io_mode,
				 memory::HeapAllocator *allocator);
	static int32_t SaveAsDds(void *dst, uint32_t max_size, const void *addr, uint32_t pitch,
				 const ImageExtent &ext, ImageMode mode, uint32_t io_mode);

	static int32_t GetErrorType();
	static bool SetErrorType(int32_t error);

	Image(memory::HeapAllocator *allocator)
		: m_imagesrc(NULL), m_images(allocator), m_cluts(allocator), m_imageattr(NULL),
		  m_cancelfunc(NULL), m_cancelfunc_userdata(NULL), m_openext_mem_addr(NULL),
		  m_openext_mem_size(0), m_openext_mem_allocator(NULL)
	{
	}

	~Image();

	common::SharedPtr<Image> ConvertMode(ImageMode mode,
					     ImageConvertModeType type = ImageConvertModeType_Default);
	common::SharedPtr<Image> Copy();
	common::SharedPtr<Image> Paste(common::SharedPtr<Image> image, const ImagePoint &point);
	common::SharedPtr<Image> Crop(const ImageRect &crop);
	common::SharedPtr<Image> CopyWithEdge(uint32_t extlines);
	common::SharedPtr<Image> FadeWithEdge();
	common::SharedPtr<Image> Resize(const ImageExtent &extent, ImageResizeType type);
	common::SharedPtr<Image> ResizeWithConvertMode(const ImageExtent &extent,
						       ImageResizeType resize_type, ImageMode mode,
						       ImageConvertModeType type);
	common::SharedPtr<Image> Rotate(ImageRotateType type);
	common::SharedPtr<Image> Flip(ImageFlipType type);
	common::SharedPtr<Image> Blur(float sigma);
	common::SharedPtr<Image> Sharpen(float amount, int32_t threshold);
	common::SharedPtr<Image> Transparentize(float alpha);
	common::SharedPtr<Image> RoundCorner(int32_t width, int32_t height);
	common::SharedPtr<Image> Blend(common::SharedPtr<Image> image, float alpha);

	bool Load(bool delete_flag);
	bool UnLoad();
	ImageBuffer ToBuffer(bool delete_flag);
	ImageBuffer ToCLUTBuffer(bool delete_flag);

	int32_t GetDecErrorType() const;
	bool SetDecErrorType(int32_t error);
	bool SetDecExtent(const ImageExtent &extent, ImageResizeType type);
	bool SetDecSubRect(const ImageRect &rect);
	bool SetDecodeCancelFunc(DecodeCancelFunc cancelfunc, void *userdata);
	bool SetClip(const ImageRect &rect);
	bool CheckAlignArgument(int32_t align);
	bool SetPitchAlign(uint32_t align);
	bool SetHeightAlign(uint32_t align);
	bool SetArrayPos(ImagePoint &point);
	bool SetArrayPosCLUT(ImagePoint &point);

	/* A format's own attribute by key, from its decoder; false when it has none. */
	bool GetAttribute(uint32_t key, bool &value);
	bool GetAttribute(uint32_t key, int &value);
	bool GetAttribute(uint32_t key, float &value);
	bool GetAttribute(uint32_t key, string &value);

	ImageExtent GetExtent() const;
	ImageExtent GetOriginalExtent() const;
	ImageRect GetSubRect() const;
	ImageRect GetClip() const;
	ImageMode GetMode() const;
	ImageMode GetCLUTMode() const;
	ImageOrder GetOrder() const;
	uint32_t GetCLUTSize() const;
	ImageXType GetXType() const;
	ImageYType GetYType() const;
	ImageFormat GetFormat() const;
	ImagePoint GetArraySize() const;
	ImagePoint GetCLUTArraySize() const;
	ImagePoint GetArrayPos() const;
	ImagePoint GetArrayPosCLUT() const;
	size_t GetMemPitch(uint32_t index = 0) const;
	uint32_t GetMemHeight(uint32_t index = 0) const;
	uint32_t GetPitchAlign() const;
	uint32_t GetHeightAlign() const;
	bool CheckCLUT() const;

	bool HasImage() const;
	const ImageArray &GetImage() const;
	const ImageArray &GetCLUT() const;
	ImageAttribute *GetAttribute() const;

private:
	static ImageFormat DetectFormatInMemory(void *addr, size_t size);
	static common::SharedPtr<Image> OpenSource(ImageSource *source,
						   memory::HeapAllocator *allocator,
						   ImageFormat format);
	static common::SharedPtr<Image> OpenGxt(void *addr, size_t size,
						memory::HeapAllocator *allocator);
	static bool ReadFile(common::SharedPtr<File> file, memory::HeapAllocator *allocator,
			     int64_t offset, size_t size, void **addr, size_t *readsize);
	static common::SharedPtr<Image> OpenDds(void *addr, size_t size,
						memory::HeapAllocator *allocator);
	static common::SharedPtr<Image> OpenGim(void *addr, size_t size,
						memory::HeapAllocator *allocator);
	static common::SharedPtr<Image> CreateFromSource(ImageSource *source, uint32_t pitch,
							 const ImageExtent &ext, ImageMode mode,
							 memory::HeapAllocator *allocator);
	common::SharedPtr<Image> Operate(ImageOperation *operation);

	ImageSource *m_imagesrc;
	ImageArray m_images;
	ImageArray m_cluts;
	ImageAttribute *m_imageattr;
	ImagePoint m_current;
	ImagePoint m_current_clut;
	DecodeCancelFunc m_cancelfunc;
	void *m_cancelfunc_userdata;
	void *m_openext_mem_addr;
	size_t m_openext_mem_size;
	memory::HeapAllocator *m_openext_mem_allocator;
};

}
}

#endif
