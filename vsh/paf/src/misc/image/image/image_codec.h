/*
 * The readers of each image format that the image sources drive. A codec
 * reads an image's header and decodes it whole, from a file or from memory,
 * or opens it and decodes it line by line. Each format also has its own
 * ImageAttribute. Declared as far as the codecs written in C++ need.
 */
#ifndef PAF_IMAGE_IMAGE_CODEC_H
#define PAF_IMAGE_IMAGE_CODEC_H

#include <image/image.h>
#include <thread/thread.h>

namespace paf {

class Buffer;
class File;

namespace image {

class ImageCodec {
public:
	/* How the image sources load what the codec decodes. */
	enum {
		LoadType_Line = 1,	/* line by line (ReadLine) */
		LoadType_Image = 2	/* the whole image at once (LoadImage) */
	};

	ImageCodec();

	virtual bool LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			       const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       DecodeCancelFunc cancelfunc, void *userdata) = 0;
	virtual bool LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			       void *userdata) = 0;
	virtual bool LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts) = 0;
	/* The size the image decodes to for subrect; true only for the whole image. */
	virtual bool CalcDecodeExtent(const ImageExtent &extent, const ImageRect &subrect,
				      ImageResizeType type, ImageExtent *out);
	virtual bool GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			       ImageAttribute *attr, ImageArray *images, ImageArray *cluts) = 0;
	virtual bool GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts) = 0;
	virtual bool Open(common::SharedPtr<File> file, int64_t offset, size_t size);
	virtual bool Open(common::SharedPtr<Buffer> buf);
	virtual bool Close();
	/* Starts line decoding into images; pitch and top-down order of the lines. */
	virtual bool BeginLines(const ImageAttribute *attr, const ImageArray *images,
				uint32_t *pitch, bool *topdown);
	virtual bool EndLines();
	virtual bool PrepareLine();
	/* Decodes the next line's pixels of rect into dst. */
	virtual bool ReadLine(void *dst, const ImageRect *rect);
	virtual bool SkipLine();
	virtual ~ImageCodec();

	int32_t GetLoadType() const;
	bool SetLoadType(int32_t type);
	int32_t GetErrorType() const;
	bool SetErrorType(int32_t type);

protected:
	int32_t m_loadtype;
	int32_t m_errortype;
};

class BmpAttribute : public ImageAttribute {
public:
	BmpAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~BmpAttribute();
};

class BmpContext;
struct BmpMasks;

class BmpCodec : public ImageCodec {
public:
	/* Decodes the line at rect from buf, size bytes read from ctx. */
	typedef bool (*LineFunc)(uint8_t *dst, const ImageRect *rect, BmpContext *ctx, void *buf,
				 size_t size, int32_t arg0, int32_t arg1, const BmpMasks *masks);

	BmpCodec();

	virtual bool LoadImage(common::SharedPtr<File> file, int64_t offset, size_t size,
			       const ImageAttribute *attr, ImageArray *images, ImageArray *cluts,
			       DecodeCancelFunc cancelfunc, void *userdata);
	virtual bool LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
			       void *userdata);
	virtual bool LoadImage(common::SharedPtr<Buffer> buf, const ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts);
	virtual bool GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			       ImageAttribute *attr, ImageArray *images, ImageArray *cluts);
	virtual bool GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts);
	virtual bool Open(common::SharedPtr<File> file, int64_t offset, size_t size);
	virtual bool Open(common::SharedPtr<Buffer> buf);
	virtual bool Close();
	virtual bool BeginLines(const ImageAttribute *attr, const ImageArray *images,
				uint32_t *pitch, bool *topdown);
	virtual bool EndLines();
	virtual bool PrepareLine();
	virtual bool ReadLine(void *dst, const ImageRect *rect);
	virtual bool SkipLine();
	virtual ~BmpCodec();

private:
	void SetDecodeError(int32_t error)
	{
		Image::SetErrorType(error);
		SetErrorType(error);
	}

	bool Decode32(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		      DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode24(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		      DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode16(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		      DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode8(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		     DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode4(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		     DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode1(BmpContext *ctx, uint8_t *dst, const ImageAttribute *attr,
		     DecodeCancelFunc cancelfunc, void *userdata);
	void TraceDecode();
	bool SetPalette(uint8_t *dst, size_t size, const struct BmpPalette *palette,
			int32_t count);
	bool Decode(BmpContext *ctx, const ImageAttribute *attr, ImageArray *images,
		    ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata);
	bool ReadHeader(BmpContext *ctx, ImageAttribute *attr, ImageArray *images,
			ImageArray *cluts);

	uint32_t m_pitch;		/* bytes of a decoded line */
	BmpContext *m_context;		/* the image Open opened */
	void *m_linebuf;		/* one line of the file, for ReadLine */
	size_t m_linesize;
	int32_t m_linearg[2];		/* passed to m_linefunc; never set for BMP */
	LineFunc m_linefunc;
	const BmpMasks *m_masks;
	memory::HeapAllocator *m_allocator;
	thread::Mutex m_mutex;
};

}
}

#endif
