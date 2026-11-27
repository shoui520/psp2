/*
 * picture_file_creator_internal.h: internal streams, EXIF/MakerNote builders,
 * and JPEG/BMP/RAW encoders for SceShell's ScePictureFileCreator library.
 * 3.65-CEX: 0x81365BFC - 0x8136D04F
 */
#ifndef SHELL_PICTURE_FILE_CREATOR_INTERNAL_H
#define SHELL_PICTURE_FILE_CREATOR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <file/localfile.h>
#include <memory/heap_allocator.h>

#include "../screenshot/server/screenshot_server.h"

/* ScePictureFileCreator error codes (facility 0x1030) */
#define PIC_CREATOR_ERROR_CANCELED		(-2146422783)	/* 0x80103001 */
#define PIC_CREATOR_ERROR_INVALID_ARGUMENT	(-2146422782)	/* 0x80103002 */
#define PIC_CREATOR_ERROR_OUT_OF_MEMORY		(-2146422781)	/* 0x80103003 */
#define PIC_CREATOR_ERROR_IO_ERROR		(-2146422780)	/* 0x80103004 */
#define PIC_CREATOR_ERROR_GENERIC		(-2146422779)	/* 0x80103005 */
#define PIC_CREATOR_ERROR_INVALID_STATE		(-2146422778)	/* 0x80103006 */

/* Internal EXIF/MakerNote builder error codes */
#define PIC_EXIF_ERROR_GENERIC			(-2147483647)	/* 0x80000001 */
#define PIC_EXIF_ERROR_INVALID_ARGUMENT		(-2147483646)	/* 0x80000002 */
#define PIC_EXIF_ERROR_INVALID_STATE		(-2147483645)	/* 0x80000003 */
#define PIC_EXIF_ERROR_STRING_TOO_LONG		(-2147483638)	/* 0x8000000A */
#define PIC_EXIF_ERROR_OUT_OF_MEMORY		(-2147483636)	/* 0x8000000C */

/* Picture output format enum */
enum PictureOutputFormat {
	PIC_FORMAT_JPEG	= 0,
	PIC_FORMAT_BMP	= 1,
	PIC_FORMAT_RAW	= 2
};

/* Output flags bitmask in PictureEncodeConfig::outputFlags */
#define PIC_OUTPUT_FLAG_FILE			0x01u
#define PIC_OUTPUT_FLAG_THUMB_TEMP_FILE		0x02u

/* Full GPS IFD metadata referenced by PictureExifData::gpsData */
struct PictureExifGpsData {
	PictureExifField<SceUInt8[4]> version;			/* Tag 0 */
	PictureExifField<paf::string> latRef;			/* Tag 1 */
	PictureExifField<PictureExifRational[3]> latitude;	/* Tag 2 */
	PictureExifField<paf::string> lonRef;			/* Tag 3 */
	PictureExifField<PictureExifRational[3]> longitude;	/* Tag 4 */
	PictureExifField<SceUInt8> altitudeRef;			/* Tag 5 */
	PictureExifField<PictureExifRational> altitude;		/* Tag 6 */
	PictureExifField<PictureExifRational[3]> timeStamp;	/* Tag 7 */
	PictureExifField<paf::string> measureMode;		/* Tag 10 */
	PictureExifField<PictureExifRational> dop;		/* Tag 11 */
	PictureExifField<paf::string> speedRef;			/* Tag 12 */
	PictureExifField<PictureExifRational> speed;		/* Tag 13 */
	PictureExifField<paf::string> trackRef;			/* Tag 14 */
	PictureExifField<PictureExifRational> track;		/* Tag 15 */
	PictureExifField<paf::string> imgDirectionRef;		/* Tag 16 */
	PictureExifField<PictureExifRational> imgDirection;	/* Tag 17 */
	PictureExifField<paf::string> mapDatum;			/* Tag 18 */
	PictureExifField<paf::string> dateStamp;		/* Tag 29 */
};

/* Cancel hook forwarded through encode loops */
struct PictureCancelHook {
	int (*isCanceled)(void *userData);
	void *userData;
};

static inline bool pictureIsCanceled(const PictureCancelHook *hook)
{
	return (hook != NULL && hook->isCanceled != NULL &&
		hook->isCanceled(hook->userData) != 0);
}

/* Active work-memory allocator (0x81366960 - 0x813669A7) */
int pictureWorkHeapSet(paf::memory::HeapAllocator *heap);
paf::memory::HeapAllocator *pictureWorkHeapGet(void);
void pictureWorkHeapClear(void);

extern "C" {
void *ScePafStdc_3CF19248(size_t size, void *heap);
}

/* Abstract output stream */
class PictureOutputStream {
public:
	virtual ~PictureOutputStream() {}
	virtual bool IsOpen() const = 0;
	virtual int Write(const void *src, SceSize len) = 0;
	virtual int Flush() = 0;
	virtual int Rewind() = 0;
	virtual uint32_t GetWrittenSize() const = 0;
	virtual int Close() = 0;
	virtual int CopyTo(PictureOutputStream *dst) = 0;
};

/* Memory-backed output stream (0x81368BE8 - 0x81368D0F) */
class PictureMemoryStream : public PictureOutputStream {
public:
	PictureMemoryStream();
	virtual ~PictureMemoryStream() {}

	int Open(void *buffer, uint32_t capacity);
	virtual bool IsOpen() const;
	virtual int Write(const void *src, SceSize len);
	virtual int Flush();
	virtual int Rewind();
	virtual uint32_t GetWrittenSize() const;
	virtual int Close();
	virtual int CopyTo(PictureOutputStream *dst);

private:
	uint8_t *m_buffer;
	uint32_t m_capacity;
	uint32_t m_written;
};

/* Double-buffered file output stream (0x81368670 - 0x81368BE7) */
class PictureFileStream : public PictureOutputStream {
public:
	explicit PictureFileStream(uint32_t totalBufSize);
	virtual ~PictureFileStream();

	int Open(const char *path, uint32_t extraFlags);
	virtual bool IsOpen() const;
	virtual int Write(const void *src, SceSize len);
	virtual int Flush();
	virtual int Rewind();
	virtual uint32_t GetWrittenSize() const;
	virtual int Close();
	virtual int CopyTo(PictureOutputStream *dst);

	static int RemoveFile(const char *path);

private:
	int WaitPendingWrite();
	int FlushBuffer();

	uint8_t *m_buffers[2];
	int m_activeIndex;
	uint32_t m_bufSize;
	uint32_t m_bufferedBytes;
	bool m_opened;
	bool m_pendingAsync;
	uint32_t m_pendingBytes;
	uint32_t m_totalWritten;
	paf::LocalFile m_file;
	paf::LocalFile::OpenArg m_openArg;
};

/* Endian-aware memory writer used by EXIF APP1 and MakerNote builders */
class PictureByteWriter {
public:
	PictureByteWriter();

	bool Attach(void *buffer, uint32_t capacity);
	void Reset();
	bool WriteRaw(const void *src, uint32_t len);
	bool WriteValue(const void *src, uint32_t len);
	bool ReserveZero(uint32_t len, uint32_t *outOffset);
	uint32_t Tell() const;
	bool Seek(uint32_t pos);
	void GetResult(void **outPtr, uint32_t *outLen) const;

private:
	uint8_t *m_base;
	uint32_t m_capacity;
	uint32_t m_cursor;
	bool m_bigEndian;
};

/* TIFF/EXIF APP1 writer (0x8136C28C - 0x8136D04F) */
class PictureExifApp1Writer {
public:
	PictureExifApp1Writer();
	~PictureExifApp1Writer();

	int BeginApp1();
	int BeginTiff(uint32_t *outMainIfdPatch);
	int BeginIfd(uint16_t entryCount, uint32_t patchOffset);
	int WriteTag(uint16_t tag, uint16_t tagType, uint32_t count, const void *value);
	int WriteDeferredOffsetTag(uint16_t tag, uint16_t tagType, uint32_t count,
				   uint32_t *outPatchPos);
	int PatchExternalPayload(uint32_t payloadSize, uint32_t patchPos);
	int EndIfd(uint32_t *outNextIfdPatchPos);
	int EndApp1(void **outPtr, uint32_t *outLen);

private:
	uint32_t m_ifdCapacity;
	uint32_t m_ifdWritten;
	bool m_inApp1;
	bool m_inTiff;
	bool m_inIfd;
	uint8_t *m_buffer;
	uint32_t m_bufferSize;
	uint32_t m_tiffBase;
	uint32_t m_app1LenOffset;
	uint32_t m_nextIfdOffset;
	uint32_t m_externalPayloadSize;
	PictureByteWriter m_stream;
	PictureByteWriter m_ifdWriter;
	uint32_t m_ifdEntryBase;
};

/* PlayStation MakerNote builder (0x81367F78 - 0x8136866F) */
class PictureMakerNoteWriter {
public:
	PictureMakerNoteWriter();
	~PictureMakerNoteWriter();

	int Begin();
	int AddStringTag(const char *tagName, uint16_t tagFlags, const char *str, uint32_t len);
	int AddGeoPairTag(const char *tagName, uint16_t tagFlags, int32_t firstVal,
			  int32_t secondVal);
	int End(void **outPtr, uint32_t *outLen);
	int Build(const PictureMakerNote *makerNote, void **outPtr, uint32_t *outLen);

private:
	uint8_t *m_buffer;
	uint32_t m_capacity;
	uint32_t m_sizePatchOffset;
	uint32_t m_countPatchOffset;
	uint32_t m_entryCount;
	bool m_active;
	PictureByteWriter m_stream;
};

/* Build complete EXIF APP1 header segment (0x8136752E) */
int pictureBuildExifApp1(PictureExifApp1Writer &writer, const PictureExifData *exif,
			 uint32_t thumbSize, const void *makerNotePtr, uint32_t makerNoteSize,
			 void **outPtr, uint32_t *outLen);

/* Image dimensions and cursor coordinates used by the pixel pipeline */
struct PictureSize {
	uint32_t width;
	uint32_t height;
};

struct PicturePoint {
	uint32_t posX;
	uint32_t posY;
};

/* Pipeline color spaces (returned by GetColorSpace) */
enum PictureColorSpace {
	PIC_COLOR_SPACE_NONE	= 0,
	PIC_COLOR_SPACE_YUV	= 1,
	PIC_COLOR_SPACE_RGB	= 2
};

/* YUV chroma subsampling modes */
enum PictureYuvSubsampling {
	PIC_YUV_444	= 1,
	PIC_YUV_422	= 2,
	PIC_YUV_420	= 3,
	PIC_YUV_411	= 4
};

/* RGB channel packing layouts */
enum PictureRgbLayout {
	PIC_RGB_LAYOUT_BGR888	= 1,
	PIC_RGB_LAYOUT_RGB888	= 2,
	PIC_RGB_LAYOUT_ARGB8888	= 3,
	PIC_RGB_LAYOUT_ABGR8888	= 4,
	PIC_RGB_LAYOUT_BGRA8888	= 5,
	PIC_RGB_LAYOUT_RGBA8888	= 6
};

/* Abstract pixel source base (vtable 0x8153D95C, 16 bytes) */
class PicturePixelSource {
public:
	PicturePixelSource();
	virtual ~PicturePixelSource();

	virtual void Term(bool deleteChild);
	virtual bool IsInitialized() const;
	virtual PictureSize GetSize() const;
	virtual int GetColorSpace() const = 0;
	virtual uint32_t GetPixel(uint32_t posX, uint32_t posY);
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY) = 0;

protected:
	int InitBase(uint32_t width, uint32_t height);

	uint32_t m_width;
	uint32_t m_height;
	bool m_initialized;
};

/* Centered canvas / letterbox wrapper (vtable 0x8153D930, 44 bytes) */
class PictureCanvasSource : public PicturePixelSource {
public:
	PictureCanvasSource();
	virtual ~PictureCanvasSource();

	int Init(PicturePixelSource *source, uint32_t width, uint32_t height);
	virtual void Term(bool deleteChild);
	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);

private:
	PicturePixelSource *m_source;
	uint32_t m_clipLeft;
	uint32_t m_clipRight;
	uint32_t m_clipTop;
	uint32_t m_clipBottom;
	int32_t m_offsetX;
	int32_t m_offsetY;
};

/* Abstract scaler source base (vtable 0x8153D988, 36 bytes) */
class PictureScalerSource : public PicturePixelSource {
public:
	PictureScalerSource();
	virtual ~PictureScalerSource();

	virtual void Term(bool deleteChild);
	virtual int GetColorSpace() const;

protected:
	int InitScaler(PicturePixelSource *source, float scaleX, float scaleY);
	int InitUniformScaler(PicturePixelSource *source, float scale);

	float m_scaleX;
	float m_invScaleX;
	float m_scaleY;
	float m_invScaleY;
	PicturePixelSource *m_source;
};

/* Bilinear upscaler with 8-entry pixel cache (vtable 0x8153D8FC, 44 bytes) */
class PictureBilinearScaler : public PictureScalerSource {
public:
	PictureBilinearScaler();
	virtual ~PictureBilinearScaler();

	int Init(PicturePixelSource *source, float scaleX, float scaleY);
	virtual void Term(bool deleteChild);
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);
	virtual uint32_t SampleClampedPixel(int32_t posX, int32_t posY);
	virtual uint32_t SampleCachedPixel(int32_t posX, int32_t posY);

private:
	int32_t m_halfStepX;
	int32_t m_halfStepY;
};

/* Nearest-neighbor thumbnail scaler (vtable 0x8153D9B4, 44 bytes) */
class PictureNearestScaler : public PictureScalerSource {
public:
	PictureNearestScaler();
	virtual ~PictureNearestScaler();

	int Init(PicturePixelSource *source, float scale);
	virtual void Term(bool deleteChild);
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);

private:
	uint32_t m_offsetX;
	uint32_t m_offsetY;
};

/* Centered alpha-blending overlay compositor (vtable 0x8153D9E0, 36 bytes) */
class PictureOverlayCompositor : public PicturePixelSource {
public:
	PictureOverlayCompositor();
	virtual ~PictureOverlayCompositor();

	int Init(PicturePixelSource *base, PicturePixelSource *overlay, uint8_t overlayAlpha);
	virtual void Term(bool deleteChild);
	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);

private:
	PicturePixelSource *m_base;
	PicturePixelSource *m_overlay;
	int32_t m_offsetX;
	int32_t m_offsetY;
	uint8_t m_overlayAlpha;
};

/* Cropped memory buffer pixel source base (vtable 0x8153DBCC, 40 bytes) */
class PictureBufferSource : public PicturePixelSource {
public:
	PictureBufferSource();
	virtual ~PictureBufferSource();

	virtual void Term(bool deleteChild);
	virtual uint32_t GetPixel(uint32_t posX, uint32_t posY);

protected:
	int InitRegion(uint32_t bufWidth, uint32_t bufHeight, uint32_t cropX, uint32_t cropY,
		       uint32_t cropWidth, uint32_t cropHeight);

	uint32_t m_bufWidth;
	uint32_t m_bufHeight;
	uint32_t m_cropX;
	uint32_t m_cropY;
	uint32_t m_cropWidth;
	uint32_t m_cropHeight;
};

/* Shared RGB buffer source fields & setup (88 bytes) */
class PictureRgbSourceBase : public PictureBufferSource {
public:
	PictureRgbSourceBase();
	virtual ~PictureRgbSourceBase();

	int Init(const void *pixels, uint32_t bufWidth, uint32_t bufHeight, uint32_t pitchBytes,
		 uint32_t cropX, uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight,
		 int layoutCode);
	virtual void Term(bool deleteChild);

protected:
	const uint8_t *m_pixels;
	int m_layoutCode;
	uint32_t m_bytesPerPixel;
	uint32_t m_hasAlpha;
	uint32_t m_alphaByteOffset;
	uint32_t m_redByteOffset;
	uint32_t m_greenByteOffset;
	uint32_t m_blueByteOffset;
	uint32_t m_sourceWidth;
	uint32_t m_sourceHeight;
	uint32_t m_pitchBytes;
	uint32_t m_bufferByteSize;
};

/* RGB buffer -> RGB pixel source (vtable 0x8153DA0C, 88 bytes) */
class PictureRgbToRgbSource : public PictureRgbSourceBase {
public:
	PictureRgbToRgbSource();
	virtual ~PictureRgbToRgbSource();

	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);
};

/* RGB buffer -> YUV pixel source (vtable 0x8153DA38, 88 bytes) */
class PictureRgbToYuvSource : public PictureRgbSourceBase {
public:
	PictureRgbToYuvSource();
	virtual ~PictureRgbToYuvSource();

	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);
};

/* Shared planar YUV buffer source fields & setup (96 bytes) */
class PictureYuvSourceBase : public PictureBufferSource {
public:
	PictureYuvSourceBase();
	virtual ~PictureYuvSourceBase();

	int Init(const void *pixels, uint32_t bufWidth, uint32_t bufHeight, uint32_t pitchBytes,
		 uint32_t cropX, uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight,
		 int subsampling, int colorRange);
	virtual void Term(bool deleteChild);

protected:
	const uint8_t *m_yPlane;
	const uint8_t *m_cbPlane;
	const uint8_t *m_crPlane;
	int m_subsampling;
	uint32_t m_chromaDivX;
	uint32_t m_chromaDivY;
	uint32_t m_yStride;
	uint32_t m_yHeight;
	uint32_t m_uvStride;
	uint32_t m_uvHeight;
	uint32_t m_yPlaneSize;
	uint32_t m_uvPlaneSize;
	uint32_t m_totalSize;
	int m_colorRange;
};

/* YUV buffer -> RGB pixel source (vtable 0x8153DAE8, 96 bytes) */
class PictureYuvToRgbSource : public PictureYuvSourceBase {
public:
	PictureYuvToRgbSource();
	virtual ~PictureYuvToRgbSource();

	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);

private:
	uint32_t ConvertYuvToRgb(uint32_t yuvPixel);
};

/* YUV buffer -> YUV pixel source (vtable 0x8153DB14, 96 bytes) */
class PictureYuvToYuvSource : public PictureYuvSourceBase {
public:
	PictureYuvToYuvSource();
	virtual ~PictureYuvToYuvSource();

	virtual int GetColorSpace() const;
	virtual uint32_t GetDefaultPixel() const;
	virtual uint32_t GetPixelAt(uint32_t posX, uint32_t posY);
};

/* Abstract pixel sink base (vtable 0x8153DB9C, 40 bytes) */
class PicturePixelSink {
public:
	PicturePixelSink();
	virtual ~PicturePixelSink();

	virtual void Term();
	virtual bool IsInitialized() const;
	virtual PictureSize GetSize() const;
	virtual int GetColorSpace() const = 0;
	virtual int GetSubsampling() const = 0;
	virtual void SetPixel(uint32_t posX, uint32_t posY, uint32_t pixel);
	virtual void Clear() = 0;
	virtual void SetPixelAt(uint32_t posX, uint32_t posY, uint32_t pixel) = 0;

protected:
	int InitBase(uint32_t bufWidth, uint32_t bufHeight, uint32_t cropX, uint32_t cropY,
		     uint32_t cropWidth, uint32_t cropHeight);

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_bufWidth;
	uint32_t m_bufHeight;
	uint32_t m_cropX;
	uint32_t m_cropY;
	uint32_t m_cropWidth;
	uint32_t m_cropHeight;
	bool m_initialized;
};

/* Planar YUV MCU sink for SceJpegEncArm (vtable 0x8153DB40, 92 bytes) */
class PictureYuvMcuSink : public PicturePixelSink {
public:
	PictureYuvMcuSink();
	virtual ~PictureYuvMcuSink();

	int Init(void *buffer, uint32_t bufWidth, uint32_t bufHeight, uint32_t pitchBytes,
		 uint32_t cropX, uint32_t cropY, uint32_t cropWidth, uint32_t cropHeight,
		 int subsampling);
	virtual void Term();
	virtual int GetColorSpace() const;
	virtual int GetSubsampling() const;
	virtual void Clear();
	virtual void SetPixelAt(uint32_t posX, uint32_t posY, uint32_t pixel);

	uint8_t *GetYPlane() const;
	uint8_t *GetCbPlane() const;
	uint8_t *GetCrPlane() const;

private:
	uint8_t *m_yPlane;
	uint8_t *m_cbPlane;
	uint8_t *m_crPlane;
	int m_subsampling;
	uint32_t m_chromaDivX;
	uint32_t m_chromaDivY;
	uint32_t m_yStride;
	uint32_t m_yHeight;
	uint32_t m_uvStride;
	uint32_t m_uvHeight;
	uint32_t m_yPlaneSize;
	uint32_t m_uvPlaneSize;
	uint32_t m_totalSize;
};

/* Abstract MCU feeder base (vtable 0x8153DB70, 12 bytes) */
class PictureMcuFeederBase {
public:
	PictureMcuFeederBase();
	virtual ~PictureMcuFeederBase();

	virtual bool CanBindSink(PictureYuvMcuSink *sink) = 0;
	virtual int Term() = 0;
	virtual bool IsInitialized() const;
	virtual bool IsReady() const;
	virtual int BindSink(PictureYuvMcuSink *sink);
	virtual PictureSize GetSourceSize() const = 0;
	virtual int FetchNextBlock(PicturePoint *cursor) = 0;

protected:
	int InitBase();
	void TermBase();

	bool m_initialized;
	PictureYuvMcuSink *m_sink;
};

/* Fast-path 960x544 ABGR8888 -> YUV420 MCU feeder (vtable 0x8153DA64, 16 bytes) */
class PictureFastRgbaMcuFeeder : public PictureMcuFeederBase {
public:
	PictureFastRgbaMcuFeeder();
	virtual ~PictureFastRgbaMcuFeeder();

	static bool CanUse(const PictureSourceInfo *source);
	int Init(const PictureSourceInfo *source);
	virtual bool CanBindSink(PictureYuvMcuSink *sink);
	virtual int Term();
	virtual PictureSize GetSourceSize() const;
	virtual int FetchNextBlock(PicturePoint *cursor);

private:
	const PictureSourceInfo *m_source;
};

/* Fast-path planar YUV420 -> YUV420 MCU feeder (vtable 0x8153DA90, 76 bytes) */
class PictureFastYuvMcuFeeder : public PictureMcuFeederBase {
public:
	PictureFastYuvMcuFeeder();
	virtual ~PictureFastYuvMcuFeeder();

	static bool CanUse(const PictureSourceInfo *source);
	int Init(const PictureSourceInfo *source);
	virtual bool CanBindSink(PictureYuvMcuSink *sink);
	virtual int Term();
	virtual PictureSize GetSourceSize() const;
	virtual int FetchNextBlock(PicturePoint *cursor);

private:
	const PictureSourceInfo *m_source;
	int m_subsampling;
	uint32_t m_chromaDivX;
	uint32_t m_chromaDivY;
	uint32_t m_yStride;
	uint32_t m_yHeight;
	uint32_t m_uvStride;
	uint32_t m_uvHeight;
	uint32_t m_yPlaneSize;
	uint32_t m_uvPlaneSize;
	uint32_t m_totalSize;
	uint32_t m_cropWidth;
	uint32_t m_cropHeight;
	const uint8_t *m_yStart;
	const uint8_t *m_cbStart;
	const uint8_t *m_crStart;
};

/* Generic PicturePixelSource -> YUV MCU feeder (vtable 0x8153DABC, 16 bytes) */
class PictureGenericMcuFeeder : public PictureMcuFeederBase {
public:
	PictureGenericMcuFeeder();
	virtual ~PictureGenericMcuFeeder();

	int Init(PicturePixelSource *source);
	virtual bool CanBindSink(PictureYuvMcuSink *sink);
	virtual int Term();
	virtual PictureSize GetSourceSize() const;
	virtual int FetchNextBlock(PicturePoint *cursor);

private:
	PicturePixelSource *m_source;
};

#endif /* SHELL_PICTURE_FILE_CREATOR_INTERNAL_H */
