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
#include "gif_lib.h"

namespace paf {

class Buffer;
class File;

namespace image {

class ImageCodec {
public:
	/* How the image sources load what the codec decodes. */
	enum {
		LoadType_Line = 1,	/* line by line (ReadLine), resized as it is read */
		LoadType_Image = 2,	/* the whole image (LoadImage), then cropped and resized */
		LoadType_Scaled = 3	/* decoded at the size CalcDecodeExtent gives, then resized */
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
	/*
	 * The size the codec decodes subrect of an image of original to, for a
	 * result of extent; the base codec decodes only the whole image.
	 */
	virtual bool CalcDecodeExtent(const ImageExtent &original, const ImageRect &subrect,
				      const ImageExtent &extent, ImageExtent *out);
	virtual bool GetHeader(common::SharedPtr<File> file, int64_t offset, size_t size,
			       ImageAttribute *attr, ImageArray *images, ImageArray *cluts) = 0;
	virtual bool GetHeader(common::SharedPtr<Buffer> buf, ImageAttribute *attr,
			       ImageArray *images, ImageArray *cluts) = 0;
	virtual bool Open(common::SharedPtr<File> file, int64_t offset, size_t size,
			  const ImageAttribute *attr);
	virtual bool Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr);
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
	virtual bool Open(common::SharedPtr<File> file, int64_t offset, size_t size,
			  const ImageAttribute *attr);
	virtual bool Open(common::SharedPtr<Buffer> buf, const ImageAttribute *attr);
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

class DdsAttribute : public ImageAttribute {
public:
	DdsAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~DdsAttribute();
};

struct DdsHeader;

/* The mipmap levels of one face of a DDS image, in the file's data. */
struct DdsSurface {
	DdsSurface()
	{
		for (int32_t i = 0; i < 16; i++)
			mip[i] = NULL;
	}

	const uint8_t *mip[16];
};

/* What a DDS file's header says, and where its surfaces are. */
struct DdsInfo {
	DdsInfo()
		: data(NULL), mode(ImageMode_NONE), reserved(0), channels(0), width(0), height(0),
		  mipcount(0), facecount(0), error(0)
	{
	}

	/* Reads the file at buf; error is set when it cannot be read. */
	DdsInfo(common::SharedPtr<Buffer> buf);

	/* The bytes of a w x h surface in mode. */
	static size_t GetSurfaceSize(uint32_t w, uint32_t h, ImageMode mode);
	bool GetFormat(const DdsHeader *header, ImageMode *mode, int32_t *channels);

	const uint8_t *data;
	ImageMode mode;
	int32_t reserved;
	int32_t channels;
	uint32_t width;
	uint32_t height;
	uint32_t mipcount;
	uint32_t facecount;
	DdsSurface surface[6];
	int32_t error;
};

/* DDS: DXT1, DXT3, DXT5, RGBA8888, BGRA8888, RGB888, A8 and L8, from memory. */
class DdsCodec : public ImageCodec {
public:
	DdsCodec();

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
	virtual ~DdsCodec();

private:
	DdsInfo m_info;
};

class PngAttribute : public ImageAttribute {
public:
	PngAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~PngAttribute();
};

/* PNG. */
class PngCodec : public ImageCodec {
public:
	PngCodec();

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
	virtual ~PngCodec();

private:
	/* PROVISIONAL: the codec's state, until image_png.cpp is written (0x40 bytes in all). */
	uint8_t m_state[0x34];
};

class JpgAttribute : public ImageAttribute {
public:
	JpgAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~JpgAttribute();

private:
	/* PROVISIONAL: until image_jpg.cpp is written (0x60 bytes in all). */
	uint8_t m_state[8];
};

/* JPEG. */
class JpgCodec : public ImageCodec {
public:
	JpgCodec();

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
	virtual ~JpgCodec();

private:
	/* PROVISIONAL: the codec's state, until image_jpg.cpp is written (0x370 bytes in all). */
	uint8_t m_state[0x364];
};

class TifAttribute : public ImageAttribute {
public:
	TifAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~TifAttribute();
};

/* TIFF. */
class TifCodec : public ImageCodec {
public:
	TifCodec();

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
	virtual ~TifCodec();

private:
	/* PROVISIONAL: the codec's state, until image_tif.cpp is written (0xa8 bytes in all). */
	uint8_t m_state[0x9c];
};

class GifAttribute : public ImageAttribute {
public:
	GifAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~GifAttribute();
};

/* GIF. */
class GifCodec : public ImageCodec {
public:
	GifCodec();

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
	virtual ~GifCodec();

private:
	/* What the decoder reads from: a File, or a buffer's bytes with a position. */
	struct Source {
		Source()
		{
			data = NULL;
			pos = 0;
			size = 0;
			allocator = NULL;
		}

		void *data;			/* the File, or the next byte of the buffer */
		int64_t size;
		uint32_t pos;			/* bytes of the buffer read */
		uint32_t reserved;
		InputFunc read;
		int64_t (*seek)(Source *src, int64_t offset, int32_t whence);
		GifMallocFunc malloc;
		GifFreeFunc free;
		GifReallocFunc realloc;
		GifCallocFunc calloc;
		memory::HeapAllocator *allocator;
	};

	bool MakeCLUT(uint8_t *dst, const ColorMapObject *map, int32_t transparent, int32_t count);
	bool DecodeImage(GifFileType *gif, const ImageBuffer *image, const ImageAttribute *attr,
			 int32_t *transparent, DecodeCancelFunc cancelfunc, void *userdata);
	bool Decode(GifFileType *gif, const ImageAttribute *attr, ImageArray *images,
		    ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata);
	GifFileType *OpenFile(common::SharedPtr<File> file, int64_t offset, size_t size);
	GifFileType *OpenBuffer(common::SharedPtr<Buffer> buf);
	bool ReadHeader(GifFileType *gif, ImageAttribute *attr, ImageArray *images,
			ImageArray *cluts);

	static int ReadFile(GifFileType *gif, GifByteType *buf, int len);
	static int64_t SeekFile(Source *src, int64_t offset, int32_t whence);
	static int ReadBuffer(GifFileType *gif, GifByteType *buf, int len);

	Source m_source;
	thread::Mutex m_mutex;
};

class GimAttribute : public ImageAttribute {
public:
	GimAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~GimAttribute();
};

/* GIM. */
class GimCodec : public ImageCodec {
public:
	GimCodec();

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
	virtual ~GimCodec();
};

class GxtAttribute : public ImageAttribute {
public:
	GxtAttribute();

	virtual bool GetAttribute(uint32_t key, bool &value);
	virtual bool GetAttribute(uint32_t key, int &value);
	virtual bool GetAttribute(uint32_t key, float &value);
	virtual bool GetAttribute(uint32_t key, string &value);
	virtual ~GxtAttribute();
};

/* GXT. */
class GxtCodec : public ImageCodec {
public:
	GxtCodec();

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
	virtual ~GxtCodec();

private:
	/* PROVISIONAL: the codec's state, until image_gxt.cpp is written (0x2c bytes in all). */
	uint8_t m_state[0x20];
};

/*
 * The codec for format, made when buf (the start of the file) is an image of
 * that format; *attr gets the format's attribute.
 */
ImageCodec *CreateCodec(common::SharedPtr<Buffer> buf, ImageFormat format, ImageAttribute **attr);

/* A codec for the image of format in file at offset, with its header read. */
ImageCodec *OpenCodec(common::SharedPtr<File> file, int64_t offset, size_t size,
		      ImageFormat format, ImageAttribute **attr, ImageArray *images,
		      ImageArray *cluts);
/* A codec for the image of format in buf, with its header read. */
ImageCodec *OpenCodec(common::SharedPtr<Buffer> buf, ImageFormat format, ImageAttribute **attr,
		      ImageArray *images, ImageArray *cluts);

/*
 * Decode attr's subrect of the image in buf, or in file when buf is empty,
 * into images at attr's extent, by the codec's load type (reader is the codec
 * for LoadType_Line, codec for the others).
 */
bool LoadLines(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset, size_t size,
	       common::SharedPtr<Buffer> buf, ImageAttribute *attr, ImageCodec *codec,
	       ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata);
bool LoadWhole(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset, size_t size,
	       common::SharedPtr<Buffer> buf, ImageAttribute *attr, ImageCodec *codec,
	       ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc, void *userdata);
bool LoadScaled(ImageCodec *reader, common::SharedPtr<File> file, int64_t offset, size_t size,
		common::SharedPtr<Buffer> buf, ImageAttribute *attr, ImageCodec *codec,
		ImageArray *images, ImageArray *cluts, DecodeCancelFunc cancelfunc,
		void *userdata);

}
}

#endif
