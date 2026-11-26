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

#endif /* SHELL_PICTURE_FILE_CREATOR_INTERNAL_H */
