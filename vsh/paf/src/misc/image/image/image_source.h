/*
 * The image sources Image opens: the decoders of each format, from memory or
 * a file, the source over pixels already in memory, the format checks and
 * the encoders. Declared as far as Image uses them.
 */
#ifndef PAF_IMAGE_IMAGE_SOURCE_H
#define PAF_IMAGE_IMAGE_SOURCE_H

#include <image/image.h>
#include <buffer/buffer.h>
#include <file/file.h>

namespace paf {
namespace image {

/* Whether buf starts with the format's signature. */
bool IsGim(common::SharedPtr<Buffer> buf);
bool IsPng(common::SharedPtr<Buffer> buf);
bool IsJpg(common::SharedPtr<Buffer> buf);
bool IsTif(common::SharedPtr<Buffer> buf);
bool IsGif(common::SharedPtr<Buffer> buf);
bool IsBmp(common::SharedPtr<Buffer> buf);
bool IsDds(common::SharedPtr<Buffer> buf);
bool IsGxt(common::SharedPtr<Buffer> buf);

/* PNG, JPEG, TIFF, GIF and BMP in memory. */
class ImageDecoder : public ImageSource {
public:
	ImageDecoder(void *addr, size_t size);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~ImageDecoder();

	common::SharedPtr<Buffer> m_buffer;
	int32_t m_errortype;
};

/* PNG, JPEG, TIFF, GIF and BMP in a file, read from offset. */
class ImageFileDecoder : public ImageSource {
public:
	ImageFileDecoder(const char *filename, int64_t offset, size_t size);
	ImageFileDecoder(common::SharedPtr<File> file, int64_t offset, size_t size);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~ImageFileDecoder();

	common::SharedPtr<File> m_file;
	void *m_codec;
	int64_t m_offset;
	size_t m_size;
	bool m_opened;
	int32_t m_errortype;
	void *m_userdata;
};

class GimDecoder : public ImageSource {
public:
	GimDecoder(void *addr, size_t size);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~GimDecoder();

	common::SharedPtr<Buffer> m_buffer;
	int32_t m_errortype;
};

class DdsDecoder : public ImageSource {
public:
	DdsDecoder(void *addr, size_t size);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~DdsDecoder();

	common::SharedPtr<Buffer> m_buffer;
	int32_t m_errortype;
};

class GxtDecoder : public ImageSource {
public:
	GxtDecoder(void *addr, size_t size);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~GxtDecoder();

	common::SharedPtr<Buffer> m_buffer;
	int32_t m_errortype;
};

/* Pixels and a palette already in memory, used as they are. */
class RawImageSource : public ImageSource {
public:
	RawImageSource(void *addr, size_t size, void *clut, size_t clutsize,
		       memory::HeapAllocator *allocator);

	virtual bool GetHeader(ImageFormat format, ImageAttribute **attr, ImageArray *images,
			       ImageArray *cluts);
	virtual bool LoadImage(ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       bool delete_flag, DecodeCancelFunc cancelfunc, void *userdata);
	virtual int32_t GetErrorType();
	virtual bool SetErrorType(int32_t type);
	virtual ~RawImageSource();

	/* Makes the header of an image of ext pixels in mode, rows pitch bytes apart. */
	bool SetHeader(uint32_t pitch, const ImageExtent &ext, ImageMode mode,
		       ImageAttribute *attr, ImageArray *images, ImageArray *cluts);

	common::SharedPtr<Buffer> m_buffer;
	common::SharedPtr<Buffer> m_clut;
	uint32_t m_pitch;
	int32_t m_errortype;
};

int32_t SavePng(const char *filename, const void *addr, uint32_t pitch, const ImageExtent &ext,
		ImageMode mode, uint32_t io_mode, memory::HeapAllocator *allocator);
int32_t SavePng(void *dst, uint32_t max_size, uint32_t &written, const void *addr,
		uint32_t pitch, const ImageExtent &ext, ImageMode mode,
		memory::HeapAllocator *allocator);
int32_t SaveBmp(const char *filename, const void *addr, uint32_t pitch, const ImageExtent &ext,
		ImageMode mode, uint32_t io_mode, memory::HeapAllocator *allocator);
int32_t SaveDds(const char *filename, const void *addr, uint32_t pitch, const ImageExtent &ext,
		ImageMode mode, uint32_t io_mode, memory::HeapAllocator *allocator);
int32_t SaveDds(void *dst, uint32_t max_size, const void *addr, uint32_t pitch,
		const ImageExtent &ext, ImageMode mode, uint32_t io_mode);

}
}

#endif
