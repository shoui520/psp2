/*
 * SceCamImportDialogPlugin: ScePictureFileCreator (JPEG/BMP/RAW image encoder,
 * EXIF/GPS/MakerNote writer, overlay alpha-blending, and photo0 export).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <scejpegenc.h>
#include <kernel/processmgr.h>
#include <std/string.h>
#include <std/string>
#include <std/stdio.h>
#include <std/new>
#include <dir/dir.h>
#include <file/localfile.h>
#include <memory/heap_allocator.h>
#include <rtc/rtc.h>
#include <graphics/surface/surface.h>
#include <thread/thread.h>
#include "camera_core.h"
#include "picture_file_creator.h"

extern "C" {
int sceJpegArmEncoderGetContextSize(void);
int sceJpegArmEncoderInit(void *ctx, int width, int height, int pixelFormat, void *outBuf,
			  SceSize outBufSize);
int sceJpegArmEncoderSetValidRegion(void *ctx, uint16_t width, uint16_t height);
int sceJpegArmEncoderSetCompressionRatio(void *ctx, uint8_t ratio);
int SceJpegEncArm_4EE1BAED(void *ctx, const void *yPlane, const void *cbPlane,
			   const void *crPlane, int mode, void *outBuf, SceSize outBufSize,
			   int *outSize);
int sceJpegArmEncoderEnd(void *ctx);
void *ScePafStdc_3CF19248(size_t size, void *heap);
int scePhotoExportFromFile(const char *path, const void *param, void *work, void *cb, void *arg,
			   char *outPath, SceSize outPathSize);
}

using namespace paf;
using namespace paf::graph;
using namespace paf::memory;

/* Global state */
static bool s_creatorInitialized = false;
static HeapAllocator *s_workHeap = NULL;
static bool s_photoExportInitialized = false;

static int OpenLocalFileSync(LocalFile &file, const LocalFile::OpenArg &arg)
{
	int ret = file.OpenAsync(&arg);

	if (ret != 0)
		return ret;
	int64_t asyncRes = 0;

	ret = file.WaitAsync(&asyncRes);
	if (ret != 0)
		return ret;
	return (int)asyncRes;
}

static const char *const s_photoBaseDir = "photo0:/CAMERA/";
static const char *const s_extJpg = "jpg";
static const char *const s_extBmp = "bmp";
static const char *const s_extDat = "dat";

/* Cancel callback wrapper */
struct EncodeCancelHook {
	int (*isCanceled)(void *arg);
	void *arg;
};

static int CheckJobCanceled(void *arg)
{
	return (arg != NULL) ? (((paf::job::JobItem *)arg)->IsCanceled() ? 1 : 0) : 0;
}

static bool IsEncodeCanceled(const EncodeCancelHook *hook)
{
	return (hook != NULL && hook->isCanceled != NULL && hook->isCanceled(hook->arg) != 0);
}

/* Output stream interface */
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

/* Memory-backed output stream */
class MemoryOutputStream : public PictureOutputStream {
public:
	MemoryOutputStream() : m_buffer(NULL), m_capacity(0), m_written(0) {}
	virtual ~MemoryOutputStream() {}

	int Open(void *buffer, uint32_t capacity)
	{
		if (IsOpen())
			return (int)0x80000003;
		if (buffer == NULL || capacity == 0)
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		m_buffer = (uint8_t *)buffer;
		m_capacity = capacity;
		m_written = 0;
		return 0;
	}

	virtual bool IsOpen() const { return m_buffer != NULL; }

	virtual int Write(const void *src, SceSize len)
	{
		if (!IsOpen())
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (len == 0)
			return 0;
		if (src == NULL)
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		if (m_written + len > m_capacity)
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		sce_paf_memcpy(m_buffer + m_written, src, len);
		m_written += len;
		return 0;
	}

	virtual int Flush() { return IsOpen() ? 0 : PIC_CREATOR_ERROR_INVALID_STATE; }

	virtual int Rewind()
	{
		if (!IsOpen())
			return PIC_CREATOR_ERROR_INVALID_STATE;
		m_written = 0;
		return 0;
	}

	virtual uint32_t GetWrittenSize() const { return IsOpen() ? m_written : 0; }

	virtual int Close()
	{
		m_buffer = NULL;
		m_capacity = 0;
		m_written = 0;
		return 0;
	}

	virtual int CopyTo(PictureOutputStream *dst)
	{
		if (!IsOpen())
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (dst == NULL || !dst->IsOpen())
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		return dst->Write(m_buffer, m_written);
	}

private:
	uint8_t *m_buffer;
	uint32_t m_capacity;
	uint32_t m_written;
};

/* Double-buffered file output stream */
class FileOutputStream : public PictureOutputStream {
public:
	FileOutputStream(uint32_t totalBufSize)
		: m_activeIdx(0), m_bufSize(totalBufSize >> 1), m_bufferedBytes(0), m_opened(false),
		  m_pendingAsync(false), m_pendingBytes(0), m_totalWritten(0),
		  m_file((HeapAllocator *)s_workHeap)
	{
		m_buffers[0] = NULL;
		m_buffers[1] = NULL;
		m_openArg.filename = NULL;
		m_openArg.flag = 0;
		m_openArg.mode = 0;
		if (m_bufSize != 0) {
			m_buffers[0] = (uint8_t *)ScePafStdc_3CF19248(m_bufSize, s_workHeap);
			m_buffers[1] = (uint8_t *)ScePafStdc_3CF19248(m_bufSize, s_workHeap);
			if (m_buffers[0] == NULL || m_buffers[1] == NULL) {
				m_bufSize = 0;
				if (m_buffers[0] != NULL) {
					operator delete(m_buffers[0]);
					m_buffers[0] = NULL;
				}
				if (m_buffers[1] != NULL) {
					operator delete(m_buffers[1]);
					m_buffers[1] = NULL;
				}
			}
		}
	}

	virtual ~FileOutputStream()
	{
		if (m_opened)
			Close();
		if (m_buffers[0] != NULL) {
			operator delete(m_buffers[0]);
			m_buffers[0] = NULL;
		}
		if (m_buffers[1] != NULL) {
			operator delete(m_buffers[1]);
			m_buffers[1] = NULL;
		}
	}

	int Open(const char *path, uint32_t extraFlags)
	{
		if (m_bufSize != 0 && (m_buffers[0] == NULL || m_buffers[1] == NULL))
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		if (m_opened)
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (path == NULL)
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		m_openArg.filename = path;
		m_openArg.flag = extraFlags | 0x603U;
		m_openArg.mode = 6;
		m_pendingAsync = false;
		int ret = OpenLocalFileSync(m_file, m_openArg);

		if (ret == 0) {
			m_opened = true;
			m_totalWritten = 0;
			m_bufferedBytes = 0;
		}
		return ret;
	}

	virtual bool IsOpen() const { return m_opened; }

	int WaitPendingWrite()
	{
		bool wasPending = m_pendingAsync;
		int expected = (int)m_pendingBytes;
		int64_t asyncRes = 0;

		m_pendingAsync = false;
		int ret = m_file.WaitAsync(&asyncRes);
		int lowRes = (int)asyncRes;

		if (wasPending) {
			if (ret != 0)
				return (ret < 0) ? ret : PIC_CREATOR_ERROR_IO_ERROR;
			if (lowRes != expected)
				return (lowRes < 0) ? lowRes : PIC_CREATOR_ERROR_IO_ERROR;
			return 0;
		}
		if (ret == (int)0x80AF5002 || (ret == 0 && lowRes >= 0))
			return 0;
		if (ret < 0)
			return ret;
		return (lowRes < 0) ? lowRes : PIC_CREATOR_ERROR_IO_ERROR;
	}

	int FlushBuffer()
	{
		int ret = 0;

		if (!m_opened) {
			ret = PIC_CREATOR_ERROR_INVALID_STATE;
		} else if (m_bufferedBytes != 0) {
			uint8_t *buf = m_buffers[m_activeIdx];

			if (buf == NULL) {
				ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
			} else {
				ret = WaitPendingWrite();
				if (ret == 0) {
					m_pendingBytes = m_bufferedBytes;
					m_pendingAsync = true;
					ret = m_file.WriteAsync(buf, m_bufferedBytes);
					m_totalWritten += m_bufferedBytes;
					if (ret >= 0)
						ret = 0;
				}
			}
		}
		m_activeIdx = 1 - m_activeIdx;
		m_bufferedBytes = 0;
		return ret;
	}

	virtual int Write(const void *src, SceSize len)
	{
		if (m_bufSize != 0 && (m_buffers[0] == NULL || m_buffers[1] == NULL))
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		if (!m_opened)
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (len == 0)
			return 0;
		if (src == NULL)
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

		const uint8_t *cur = (const uint8_t *)src;
		SceSize remaining = len;

		if (m_bufferedBytes != 0) {
			SceSize space = m_bufSize - m_bufferedBytes;
			SceSize chunk = (remaining < space) ? remaining : space;

			sce_paf_memcpy(m_buffers[m_activeIdx] + m_bufferedBytes, cur, chunk);
			cur += chunk;
			m_bufferedBytes += chunk;
			remaining -= chunk;
			if (m_bufferedBytes == m_bufSize) {
				int flushRet = FlushBuffer();

				if (flushRet != 0)
					return flushRet;
			} else {
				return 0;
			}
		}
		if (remaining < m_bufSize) {
			sce_paf_memcpy(m_buffers[m_activeIdx], cur, remaining);
			m_bufferedBytes = remaining;
			return 0;
		}
		int waitRet = WaitPendingWrite();

		if (waitRet != 0)
			return waitRet;
		m_pendingBytes = remaining;
		m_pendingAsync = true;
		int writeRet = m_file.WriteAsync(cur, remaining);

		m_totalWritten += remaining;
		if (writeRet < 0)
			return writeRet;
		return WaitPendingWrite();
	}

	virtual int Flush() { return m_opened ? FlushBuffer() : PIC_CREATOR_ERROR_INVALID_STATE; }

	virtual int Rewind()
	{
		if (!m_opened)
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (GetWrittenSize() == 0)
			return 0;
		const char *savedPath = m_openArg.filename;
		uint32_t savedFlag = m_openArg.flag;

		Close();
		return Open(savedPath, savedFlag);
	}

	virtual uint32_t GetWrittenSize() const
	{
		return m_opened ? (m_totalWritten + m_bufferedBytes) : 0;
	}

	virtual int Close()
	{
		if (!m_opened)
			return 0;
		int flushRet = FlushBuffer();
		int waitRet = WaitPendingWrite();

		m_pendingAsync = false;
		int closeRet = m_file.Close();

		m_totalWritten = 0;
		m_opened = false;
		if (closeRet != 0)
			return closeRet;
		if (waitRet != 0)
			return waitRet;
		return flushRet;
	}

	virtual int CopyTo(PictureOutputStream *dst)
	{
		if (!m_opened)
			return PIC_CREATOR_ERROR_INVALID_STATE;
		if (dst == NULL || !dst->IsOpen())
			return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		int ret = WaitPendingWrite();

		if (ret != 0)
			return ret;
		uint8_t *copyBuf = (uint8_t *)ScePafStdc_3CF19248(1024, s_workHeap);

		if (copyBuf == NULL)
			return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
		m_pendingAsync = false;
		m_file.Seek(0, 0);
		for (;;) {
			m_pendingAsync = false;
			int nRead = m_file.Read(copyBuf, 1024);

			if (nRead < 0) {
				ret = nRead;
				break;
			}
			if (nRead > 0) {
				ret = dst->Write(copyBuf, (SceSize)nRead);
				if (ret != 0)
					break;
			}
			if (nRead < 1024) {
				ret = 0;
				break;
			}
		}
		m_pendingAsync = false;
		m_file.Seek(0, 0);
		operator delete(copyBuf);
		return ret;
	}

private:
	uint8_t *m_buffers[2];
	int m_activeIdx;
	uint32_t m_bufSize;
	uint32_t m_bufferedBytes;
	bool m_opened;
	bool m_pendingAsync;
	uint32_t m_pendingBytes;
	uint32_t m_totalWritten;
	LocalFile m_file;
	LocalFile::OpenArg m_openArg;
};

/* Endian-aware memory writer used by EXIF and MakerNote builders */
class ByteBufferWriter {
public:
	ByteBufferWriter() : m_base(NULL), m_capacity(0), m_cursor(0), m_bigEndian(true) {}

	bool Attach(void *buffer, uint32_t capacity)
	{
		if (m_base != NULL)
			return false;
		if (buffer == NULL || capacity == 0) {
			Reset();
			return false;
		}
		m_base = (uint8_t *)buffer;
		m_capacity = capacity;
		m_cursor = 0;
		return true;
	}

	void Reset()
	{
		m_base = NULL;
		m_capacity = 0;
		m_cursor = 0;
	}

	bool WriteRaw(const void *src, uint32_t len)
	{
		if (m_base == NULL || m_cursor + len > m_capacity)
			return false;
		sce_paf_memcpy(m_base + m_cursor, src, len);
		m_cursor += len;
		return true;
	}

	bool WriteValue(const void *src, uint32_t len)
	{
		if (!m_bigEndian)
			return WriteRaw(src, len);
		if (len > 8 || m_base == NULL || m_cursor + len > m_capacity)
			return false;
		uint8_t swapped[8];
		const uint8_t *bytes = (const uint8_t *)src;

		for (uint32_t i = 0; i < len; ++i)
			swapped[i] = bytes[len - 1 - i];
		return WriteRaw(swapped, len);
	}

	bool ReserveZero(uint32_t len, uint32_t *outOffset)
	{
		if (m_base == NULL || m_cursor + len > m_capacity)
			return false;
		if (outOffset != NULL)
			*outOffset = m_cursor;
		sce_paf_memset(m_base + m_cursor, 0, len);
		m_cursor += len;
		return true;
	}

	uint32_t Tell() const { return m_cursor; }

	bool Seek(uint32_t pos)
	{
		if (pos > m_capacity)
			return false;
		m_cursor = pos;
		return true;
	}

	void GetResult(void **outPtr, uint32_t *outLen) const
	{
		if (outPtr != NULL)
			*outPtr = m_base;
		if (outLen != NULL)
			*outLen = m_cursor;
	}

private:
	uint8_t *m_base;
	uint32_t m_capacity;
	uint32_t m_cursor;
	bool m_bigEndian;
};

/* TIFF/EXIF APP1 writer */
struct ExifTypeLayout {
	uint32_t unitSize;
	uint32_t swapSize;
};

static const ExifTypeLayout s_exifTypeTable[11] = {
	{ 0, 0 }, { 1, 1 }, { 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 4 },
	{ 0, 0 }, { 1, 1 }, { 0, 0 }, { 4, 4 }, { 8, 4 }
};

class ExifApp1Writer {
public:
	ExifApp1Writer()
		: m_ifdCapacity(0), m_ifdWritten(0), m_inApp1(false), m_inTiff(false),
		  m_inIfd(false), m_buffer(NULL), m_bufferSize(4096), m_tiffBase(0),
		  m_app1LenOffset(0), m_nextIfdOffset(0), m_externalPayloadSize(0),
		  m_ifdEntryBase(0)
	{
		m_buffer = (uint8_t *)ScePafStdc_3CF19248(m_bufferSize, s_workHeap);
	}

	~ExifApp1Writer()
	{
		if (m_buffer != NULL) {
			operator delete(m_buffer);
			m_buffer = NULL;
		}
	}

	int BeginApp1()
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (m_inApp1)
			return (int)0x80000003;
		m_externalPayloadSize = 0;
		bool ok = m_stream.Attach(m_buffer, m_bufferSize);
		static const uint8_t app1Marker[2] = { 0xFF, 0xE1 };
		static const uint8_t exifHeader[6] = { 'E', 'x', 'i', 'f', '\0', '\0' };

		ok = ok && m_stream.WriteRaw(app1Marker, 2);
		ok = ok && m_stream.ReserveZero(2, &m_app1LenOffset);
		ok = ok && m_stream.WriteRaw(exifHeader, 6);
		m_inApp1 = true;
		return ok ? 0 : (int)0x80000001;
	}

	int BeginTiff(uint32_t *outIfd0PatchOffset)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inApp1 || m_inTiff)
			return (int)0x80000003;
		if (outIfd0PatchOffset == NULL)
			return (int)0x80000002;
		m_tiffBase = m_stream.Tell();
		static const uint8_t byteOrder[2] = { 'M', 'M' };
		uint16_t magic = 42;
		bool ok = m_stream.WriteRaw(byteOrder, 2);

		ok = ok && m_stream.WriteValue(&magic, 2);
		ok = ok && m_stream.ReserveZero(4, outIfd0PatchOffset);
		m_inTiff = true;
		return ok ? 0 : (int)0x80000001;
	}

	int BeginIfd(uint16_t entryCount, uint32_t patchOffset)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inTiff || m_inIfd || !m_inApp1)
			return (int)0x80000003;
		if (entryCount == 0 || patchOffset == 0)
			return (int)0x80000002;
		uint32_t curPos = m_stream.Tell();
		uint32_t relOffset = curPos - m_tiffBase;
		bool ok = m_stream.Seek(patchOffset);

		ok = ok && m_stream.WriteValue(&relOffset, 4);
		ok = ok && m_stream.Seek(curPos);
		ok = ok && m_stream.WriteValue(&entryCount, 2);
		ok = ok && m_stream.ReserveZero(12U * entryCount, &m_ifdEntryBase);
		ok = ok && m_ifdWriter.Attach(m_buffer + m_ifdEntryBase, 12U * entryCount);
		ok = ok && m_stream.ReserveZero(4, &m_nextIfdOffset);
		m_ifdCapacity = entryCount;
		m_ifdWritten = 0;
		m_inIfd = true;
		return ok ? 0 : (int)0x80000001;
	}

	int WriteTag(uint16_t tag, uint16_t type, uint32_t count, const void *value)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inIfd || m_ifdWritten >= m_ifdCapacity)
			return (int)0x80000003;
		if (count == 0 || value == NULL || type >= 11 || s_exifTypeTable[type].unitSize == 0)
			return (int)0x80000002;
		const ExifTypeLayout &layout = s_exifTypeTable[type];
		bool ok = m_ifdWriter.WriteValue(&tag, 2);

		ok = ok && m_ifdWriter.WriteValue(&type, 2);
		ok = ok && m_ifdWriter.WriteValue(&count, 4);
		uint32_t byteLen = layout.unitSize * count;
		ByteBufferWriter *target = &m_ifdWriter;
		uint32_t paddedLen = 4;

		if (byteLen > 4) {
			uint32_t dataRelOffset = m_stream.Tell() - m_tiffBase;

			ok = ok && m_ifdWriter.WriteValue(&dataRelOffset, 4);
			target = &m_stream;
			paddedLen = (byteLen + 1U) & ~1U;
		}
		const uint8_t *src = (const uint8_t *)value;

		for (uint32_t off = 0; off < byteLen; off += layout.swapSize)
			ok = ok && target->WriteValue(src + off, layout.swapSize);
		ok = ok && target->ReserveZero(paddedLen - byteLen, NULL);
		++m_ifdWritten;
		return ok ? 0 : (int)0x80000001;
	}

	int WriteDeferredOffsetTag(uint16_t tag, uint16_t type, uint32_t count, uint32_t *outPatchPos)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inIfd || m_ifdWritten >= m_ifdCapacity)
			return (int)0x80000003;
		if (count == 0 || outPatchPos == NULL)
			return (int)0x80000002;
		bool ok = m_ifdWriter.WriteValue(&tag, 2);

		ok = ok && m_ifdWriter.WriteValue(&type, 2);
		ok = ok && m_ifdWriter.WriteValue(&count, 4);
		ok = ok && m_ifdWriter.ReserveZero(4, outPatchPos);
		*outPatchPos += m_ifdEntryBase;
		++m_ifdWritten;
		return ok ? 0 : (int)0x80000001;
	}

	int PatchExternalPayload(uint32_t payloadSize, uint32_t patchPos)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inTiff || !m_inIfd || !m_inApp1)
			return (int)0x80000003;
		if (payloadSize == 0 || patchPos == 0)
			return (int)0x80000002;
		uint32_t curPos = m_stream.Tell();
		uint32_t relOffset = curPos - m_tiffBase;
		bool ok = m_stream.Seek(patchPos);

		ok = ok && m_stream.WriteValue(&relOffset, 4);
		ok = ok && m_stream.Seek(curPos);
		if (ok)
			m_externalPayloadSize += payloadSize;
		return ok ? 0 : (int)0x80000001;
	}

	int EndIfd(uint32_t *outNextIfdPatchPos)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_inIfd || m_ifdWritten != m_ifdCapacity)
			return (int)0x80000003;
		if (outNextIfdPatchPos != NULL)
			*outNextIfdPatchPos = m_nextIfdOffset;
		m_ifdWriter.Reset();
		m_inIfd = false;
		return 0;
	}

	int EndApp1(void **outPtr, uint32_t *outLen)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (outPtr == NULL || outLen == NULL)
			return (int)0x80000002;
		if (!m_inTiff || m_inIfd || !m_inApp1)
			return (int)0x80000003;
		uint32_t totalBytes = m_stream.Tell();
		uint16_t segmentLen = (uint16_t)(totalBytes + m_externalPayloadSize - 2);
		bool ok = m_stream.Seek(m_app1LenOffset);

		ok = ok && m_stream.WriteValue(&segmentLen, 2);
		ok = ok && m_stream.Seek(totalBytes);
		m_stream.GetResult(outPtr, outLen);
		m_stream.Reset();
		m_inApp1 = false;
		m_inTiff = false;
		m_inIfd = false;
		return ok ? 0 : (int)0x80000001;
	}

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
	ByteBufferWriter m_stream;
	ByteBufferWriter m_ifdWriter;
	uint32_t m_ifdEntryBase;
};

/* PlayStation MakerNote builder */
class MakerNoteWriter {
public:
	MakerNoteWriter()
		: m_buffer(NULL), m_capacity(2048), m_sizePatchOffset(0), m_countPatchOffset(0),
		  m_entryCount(0), m_active(false)
	{
		m_buffer = (uint8_t *)ScePafStdc_3CF19248(m_capacity, s_workHeap);
	}

	~MakerNoteWriter()
	{
		if (m_buffer != NULL) {
			operator delete(m_buffer);
			m_buffer = NULL;
		}
	}

	int Begin()
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (m_active)
			return (int)0x80000003;
		static const char headerMagic[12] = "SONY PS";
		static const char vendorId[3] = { 'S', 'I', 'E' };
		uint8_t verMajor = 1;
		uint8_t verMinor = 0;
		uint8_t flags = 0;
		bool ok = m_stream.Attach(m_buffer, m_capacity);

		ok = ok && m_stream.WriteRaw(headerMagic, 12);
		ok = ok && m_stream.WriteRaw(vendorId, 3);
		ok = ok && m_stream.WriteValue(&verMajor, 1);
		ok = ok && m_stream.WriteValue(&verMinor, 1);
		ok = ok && m_stream.WriteValue(&flags, 1);
		ok = ok && m_stream.ReserveZero(4, &m_sizePatchOffset);
		ok = ok && m_stream.ReserveZero(2, &m_countPatchOffset);
		m_entryCount = 0;
		m_active = true;
		return ok ? 0 : (int)0x8000000C;
	}

	int AddStringTag(const char *tagName, uint16_t tagFlags, const char *str, uint32_t len)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_active)
			return (int)0x80000003;
		if (tagName == NULL || str == NULL || len == 0)
			return (int)0x80000002;
		if (m_entryCount >= 10)
			return (int)0x8000000C;
		++m_entryCount;
		static const uint8_t tagPrefix[4] = { 'T', 'X', 'X', 'X' };
		static const uint8_t charsetPad[3] = { 'X', 'X', 'X' };
		uint32_t recordLen = len + 9;
		uint8_t encoding = 3;
		bool ok = m_stream.WriteRaw(tagPrefix, 4);

		ok = ok && m_stream.WriteValue(&recordLen, 4);
		ok = ok && m_stream.WriteValue(&tagFlags, 2);
		ok = ok && m_stream.WriteValue(&encoding, 1);
		ok = ok && m_stream.WriteRaw(charsetPad, 3);
		ok = ok && m_stream.WriteRaw(tagName, 5);
		ok = ok && m_stream.WriteRaw(str, len);
		return ok ? 0 : (int)0x8000000C;
	}

	int AddRationalTag(const char *tagName, uint16_t tagFlags, uint32_t numer, uint32_t denom)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (!m_active)
			return (int)0x80000003;
		if (tagName == NULL)
			return (int)0x80000002;
		if (m_entryCount >= 10)
			return (int)0x8000000C;
		++m_entryCount;
		static const uint8_t geoPrefix[4] = { 'G', 'E', 'O', 'B' };
		uint32_t recordLen = 16;
		uint8_t zeroByte = 0;
		bool ok = m_stream.WriteRaw(geoPrefix, 4);

		ok = ok && m_stream.WriteValue(&recordLen, 4);
		ok = ok && m_stream.WriteValue(&tagFlags, 2);
		ok = ok && m_stream.WriteValue(&zeroByte, 1);
		ok = ok && m_stream.WriteValue(&zeroByte, 1);
		ok = ok && m_stream.WriteValue(&zeroByte, 1);
		ok = ok && m_stream.WriteRaw(tagName, 5);
		ok = ok && m_stream.WriteValue(&numer, 4);
		ok = ok && m_stream.WriteValue(&denom, 4);
		return ok ? 0 : (int)0x8000000C;
	}

	int End(void **outPtr, uint32_t *outLen)
	{
		if (m_buffer == NULL)
			return (int)0x8000000C;
		if (outPtr == NULL || outLen == NULL)
			return (int)0x80000002;
		if (!m_active)
			return (int)0x80000003;
		uint32_t endPos = m_stream.Tell();
		uint32_t payloadLen = endPos - 22;
		uint16_t count16 = (uint16_t)m_entryCount;
		bool ok = m_stream.Seek(m_sizePatchOffset);

		ok = ok && m_stream.WriteValue(&payloadLen, 4);
		ok = ok && m_stream.Seek(m_countPatchOffset);
		ok = ok && m_stream.WriteValue(&count16, 2);
		ok = ok && m_stream.Seek(endPos);
		m_stream.GetResult(outPtr, outLen);
		m_stream.Reset();
		m_active = false;
		return ok ? 0 : (int)0x80000001;
	}

	int BuildFromParam(const CameraMakerNoteParam *param, void **outPtr, uint32_t *outLen)
	{
		if (param == NULL || outPtr == NULL || outLen == NULL)
			return (int)0x80000002;
		int err = Begin();
		struct StringTagSpec {
			const char *name;
			const CameraMakerNoteStringTag *tag;
			uint32_t maxLen;
		};
		const StringTagSpec strSpecs[7] = {
			{ "PEFV", &param->effectVersion, 4 },
			{ "GCID", &param->gameContentId, 49 },
			{ "GPNM", &param->gameTitle, 13 },
			{ "GDSC", &param->gameDescription, 512 },
			{ "ALBM", &param->album, 512 },
			{ "PSIS", &param->comment, 2 },
			{ "PTYP", &param->frameId, 2 }
		};

		for (int i = 0; i < 7; ++i) {
			if (strSpecs[i].tag->present) {
				uint32_t len = (uint32_t)strSpecs[i].tag->value.size() + 1;
				int tagErr = (len <= strSpecs[i].maxLen)
						     ? AddStringTag(strSpecs[i].name, 0,
								    strSpecs[i].tag->value.c_str(),
								    len)
						     : (int)0x8000000A;

				if ((uint32_t)tagErr > (uint32_t)err)
					err = tagErr;
			}
		}

		struct RationalTagSpec {
			const char *name;
			const CameraMakerNoteRationalTag *tag;
		};
		const RationalTagSpec ratSpecs[4] = {
			{ "LLAC", &param->locationAccuracy },
			{ "PHFR", &param->altitudeAccuracy },
			{ "PVFR", &param->directionAccuracy },
			{ "PYPR", &param->yawPitchRoll }
		};

		for (int i = 0; i < 4; ++i) {
			if (ratSpecs[i].tag->present) {
				int tagErr = AddRationalTag(ratSpecs[i].name, 0,
							    ratSpecs[i].tag->numerator,
							    ratSpecs[i].tag->denominator);

				if ((uint32_t)tagErr > (uint32_t)err)
					err = tagErr;
			}
		}
		int endErr = End(outPtr, outLen);

		if ((uint32_t)endErr > (uint32_t)err)
			err = endErr;
		return err;
	}

private:
	uint8_t *m_buffer;
	uint32_t m_capacity;
	uint32_t m_sizePatchOffset;
	uint32_t m_countPatchOffset;
	uint32_t m_entryCount;
	bool m_active;
	ByteBufferWriter m_stream;
};

/* EXIF metadata model */
struct ExifRational {
	uint32_t num;
	uint32_t den;
};

struct ExifGpsMetadata {
	ExifGpsMetadata()
		: hasVersion(false), hasLatRef(false), hasLat(false), hasLonRef(false),
		  hasLon(false), altRef(0), hasAltRef(false), hasAlt(false), hasTimeStamp(false),
		  hasMeasureMode(false), hasImgDirectionRef(false), hasImgDirection(false),
		  hasMapDatum(false), hasDateStamp(false)
	{
		sce_paf_memset(version, 0, sizeof(version));
		sce_paf_memset(lat, 0, sizeof(lat));
		sce_paf_memset(lon, 0, sizeof(lon));
		alt.num = 0;
		alt.den = 1;
		sce_paf_memset(timeStamp, 0, sizeof(timeStamp));
		imgDirection.num = 0;
		imgDirection.den = 1;
	}

	uint8_t version[4];
	bool hasVersion;
	string latRef;
	bool hasLatRef;
	ExifRational lat[3];
	bool hasLat;
	string lonRef;
	bool hasLonRef;
	ExifRational lon[3];
	bool hasLon;
	uint8_t altRef;
	bool hasAltRef;
	ExifRational alt;
	bool hasAlt;
	ExifRational timeStamp[3];
	bool hasTimeStamp;
	string measureMode;
	bool hasMeasureMode;
	string imgDirectionRef;
	bool hasImgDirectionRef;
	ExifRational imgDirection;
	bool hasImgDirection;
	string mapDatum;
	bool hasMapDatum;
	string dateStamp;
	bool hasDateStamp;
};

struct ExifMetadata {
	ExifMetadata()
		: compression(6), hasCompression(true), hasMake(false), hasModel(false),
		  orientation(1), hasOrientation(false), hasXRes(true), hasYRes(true),
		  resUnit(2), hasResUnit(true), hasSoftware(false), hasDateTime(false),
		  yCbCrPositioning(1), hasYCbCrPositioning(true), hasExifVersion(true),
		  hasDateTimeOriginal(false), hasDateTimeDigitized(false), hasComponentsConfig(true),
		  flash(32), hasFlash(true), hasFlashpixVersion(true), colorSpace(1),
		  hasColorSpace(true), pixelXDim(0), hasPixelXDim(false), pixelYDim(0),
		  hasPixelYDim(false), customRendered(0), hasCustomRendered(true), exposureMode(0),
		  hasExposureMode(true), whiteBalance(0), hasWhiteBalance(true),
		  sceneCaptureType(0), hasSceneCaptureType(true), interopIndex("R98"),
		  hasInteropIndex(true), hasInteropVersion(true), makerNote(NULL), gps(NULL)
	{
		xRes.num = 72;
		xRes.den = 1;
		yRes.num = 72;
		yRes.den = 1;
		sce_paf_memcpy(exifVersion, "0221", 4);
		componentsConfig[0] = 1;
		componentsConfig[1] = 2;
		componentsConfig[2] = 3;
		componentsConfig[3] = 0;
		sce_paf_memcpy(flashpixVersion, "0100", 4);
		sce_paf_memcpy(interopVersion, "0100", 4);
	}

	uint16_t compression;
	bool hasCompression;
	string make;
	bool hasMake;
	string model;
	bool hasModel;
	uint16_t orientation;
	bool hasOrientation;
	ExifRational xRes;
	bool hasXRes;
	ExifRational yRes;
	bool hasYRes;
	uint16_t resUnit;
	bool hasResUnit;
	string software;
	bool hasSoftware;
	string dateTime;
	bool hasDateTime;
	uint16_t yCbCrPositioning;
	bool hasYCbCrPositioning;
	uint8_t exifVersion[4];
	bool hasExifVersion;
	string dateTimeOriginal;
	bool hasDateTimeOriginal;
	string dateTimeDigitized;
	bool hasDateTimeDigitized;
	uint8_t componentsConfig[4];
	bool hasComponentsConfig;
	uint16_t flash;
	bool hasFlash;
	uint8_t flashpixVersion[4];
	bool hasFlashpixVersion;
	uint16_t colorSpace;
	bool hasColorSpace;
	uint32_t pixelXDim;
	bool hasPixelXDim;
	uint32_t pixelYDim;
	bool hasPixelYDim;
	uint16_t customRendered;
	bool hasCustomRendered;
	uint16_t exposureMode;
	bool hasExposureMode;
	uint16_t whiteBalance;
	bool hasWhiteBalance;
	uint16_t sceneCaptureType;
	bool hasSceneCaptureType;
	string interopIndex;
	bool hasInteropIndex;
	uint8_t interopVersion[4];
	bool hasInteropVersion;
	CameraMakerNoteParam *makerNote;
	ExifGpsMetadata *gps;
};

static void ConvertDegreesToDms(double degrees, ExifRational outDms[3])
{
	double absDeg = (degrees < 0.0) ? -degrees : degrees;
	uint32_t wholeDeg = (uint32_t)absDeg;
	double rem = absDeg - (double)wholeDeg;
	uint32_t mins = (uint32_t)(rem * 60.0);
	uint32_t secsMillis = (uint32_t)((rem - (double)mins / 60.0) * 3600000.0 + 0.5);

	outDms[0].num = wholeDeg;
	outDms[0].den = 1;
	outDms[1].num = mins;
	outDms[1].den = 1;
	outDms[2].num = secsMillis;
	outDms[2].den = 1000;
}

static int PopulateExifMetadata(const CameraFrameInfo *frameInfo,
				const CameraLocationInfo *location, const CameraPhotoMeta *meta,
				rtc::Tick localTick, ExifMetadata *exif)
{
	if (exif == NULL)
		return 0;
	rtc::DateTime dt;

	if (rtc::ConvertTickToDateTime(&localTick, &dt, NULL) == 0) {
		char dtStr[20];

		sce_paf_snprintf(dtStr, sizeof(dtStr), "%04d:%02d:%02d %02d:%02d:%02d", dt.year,
				 dt.month, dt.day, dt.hour, dt.minute, dt.second);
		dtStr[19] = '\0';
		exif->dateTime = dtStr;
		exif->hasDateTime = true;
		exif->dateTimeOriginal = dtStr;
		exif->hasDateTimeOriginal = true;
		exif->dateTimeDigitized = dtStr;
		exif->hasDateTimeDigitized = true;
	}
	if (meta != NULL) {
		if (!meta->make.empty()) {
			exif->make = meta->make;
			exif->hasMake = true;
		}
		if (!meta->model.empty()) {
			exif->model = meta->model;
			exif->hasModel = true;
		}
		if (!meta->software.empty()) {
			exif->software = meta->software;
			exif->hasSoftware = true;
		}
		if (meta->isSelfie) {
			exif->customRendered = 1;
			exif->hasCustomRendered = true;
		}
	}
	if (location != NULL) {
		exif->orientation = (uint16_t)location->exifOrientation;
		exif->hasOrientation = true;
	}
	if (frameInfo != NULL) {
		exif->pixelXDim = (uint32_t)frameInfo->cropWidth;
		exif->hasPixelXDim = true;
		exif->pixelYDim = (uint32_t)frameInfo->cropHeight;
		exif->hasPixelYDim = true;
	}
	if (exif->makerNote != NULL) {
		exif->makerNote->effectVersion.value = "1.0";
		exif->makerNote->effectVersion.present = true;
		if (frameInfo != NULL) {
			if (frameInfo->deviceId == CAMERA_CORE_DEVICE_FRONT) {
				exif->makerNote->comment.value = "1";
				exif->makerNote->comment.present = true;
			} else if (frameInfo->deviceId == CAMERA_CORE_DEVICE_REAR) {
				exif->makerNote->comment.value = "2";
				exif->makerNote->comment.present = true;
			}
		}
	}
	if (exif->gps != NULL && location != NULL) {
		bool hasCoords = false;
		bool hasAlt = false;
		bool anyGps = false;

		if (location->latitude > -9998.0 && location->longitude > -9998.0) {
			ConvertDegreesToDms(location->latitude, exif->gps->lat);
			exif->gps->hasLat = true;
			exif->gps->latRef = (location->latitude >= 0.0) ? "N" : "S";
			exif->gps->hasLatRef = true;
			ConvertDegreesToDms(location->longitude, exif->gps->lon);
			exif->gps->hasLon = true;
			exif->gps->lonRef = (location->longitude >= 0.0) ? "E" : "W";
			exif->gps->hasLonRef = true;
			exif->gps->mapDatum = "WGS-84";
			exif->gps->hasMapDatum = true;
			hasCoords = true;
			anyGps = true;
			if (exif->makerNote != NULL && location->horizontalAccuracy > -9998.0f &&
			    location->horizontalAccuracy >= 0.0f &&
			    location->horizontalAccuracy < 2147483.0f) {
				exif->makerNote->locationAccuracy.numerator =
					(uint32_t)(location->horizontalAccuracy * 1000.0f);
				exif->makerNote->locationAccuracy.denominator = 1000;
				exif->makerNote->locationAccuracy.present = true;
			}
		}
		if (location->altitude > -9998.0) {
			double absAlt =
				(location->altitude < 0.0) ? -location->altitude : location->altitude;

			exif->gps->altRef = (location->altitude < 0.0) ? 1 : 0;
			exif->gps->hasAltRef = true;
			exif->gps->alt.num = (uint32_t)absAlt;
			exif->gps->alt.den = 1;
			exif->gps->hasAlt = true;
			hasAlt = true;
			anyGps = true;
		}
		if (hasCoords) {
			exif->gps->measureMode = hasAlt ? "3" : "2";
			exif->gps->hasMeasureMode = true;
		}
		if (location->heading > -9998.0f) {
			exif->gps->imgDirection.num =
				(uint32_t)((int)(location->heading * 100.0f) % 36000U);
			exif->gps->imgDirection.den = 100;
			exif->gps->hasImgDirection = true;
			exif->gps->imgDirectionRef = "T";
			exif->gps->hasImgDirectionRef = true;
			anyGps = true;
		}
		if (anyGps) {
			if (location->utcTimestamp != 0) {
				rtc::Tick utcTick = location->utcTimestamp;
				rtc::DateTime utcDt;

				if (rtc::ConvertTickToDateTime(&utcTick, &utcDt, NULL) == 0) {
					exif->gps->timeStamp[0].num = utcDt.hour;
					exif->gps->timeStamp[0].den = 1;
					exif->gps->timeStamp[1].num = utcDt.minute;
					exif->gps->timeStamp[1].den = 1;
					exif->gps->timeStamp[2].num = utcDt.second;
					exif->gps->timeStamp[2].den = 1;
					exif->gps->hasTimeStamp = true;

					char dateBuf[12];

					sce_paf_snprintf(dateBuf, sizeof(dateBuf), "%04d:%02d:%02d",
							 utcDt.year, utcDt.month, utcDt.day);
					dateBuf[10] = '\0';
					exif->gps->dateStamp = dateBuf;
					exif->gps->hasDateStamp = true;
				}
			}
			exif->gps->version[0] = 2;
			exif->gps->version[1] = 2;
			exif->gps->version[2] = 0;
			exif->gps->version[3] = 0;
			exif->gps->hasVersion = true;
		}
	}
	return 0;
}

static int BuildExifApp1Segment(ExifApp1Writer &writer, const ExifMetadata *exif,
				uint32_t thumbSize, const void *makerNotePtr,
				uint32_t makerNoteSize, void **outPtr, uint32_t *outLen)
{
	uint16_t interopCount =
		(uint16_t)((exif->hasInteropIndex ? 1 : 0) + (exif->hasInteropVersion ? 1 : 0));
	uint16_t exifCount =
		(uint16_t)((exif->hasExifVersion ? 1 : 0) + (exif->hasDateTimeOriginal ? 1 : 0) +
			   (exif->hasDateTimeDigitized ? 1 : 0) +
			   (exif->hasComponentsConfig ? 1 : 0) + (exif->hasFlash ? 1 : 0) +
			   (makerNotePtr != NULL && makerNoteSize != 0 ? 1 : 0) +
			   (exif->hasFlashpixVersion ? 1 : 0) + (exif->hasColorSpace ? 1 : 0) +
			   (exif->hasPixelXDim ? 1 : 0) + (exif->hasPixelYDim ? 1 : 0) +
			   (interopCount != 0 ? 1 : 0) + (exif->hasCustomRendered ? 1 : 0) +
			   (exif->hasExposureMode ? 1 : 0) + (exif->hasWhiteBalance ? 1 : 0) +
			   (exif->hasSceneCaptureType ? 1 : 0));
	uint16_t gpsCount = 0;

	if (exif->gps != NULL) {
		gpsCount = (uint16_t)((exif->gps->hasVersion ? 1 : 0) +
				      (exif->gps->hasLatRef ? 1 : 0) +
				      (exif->gps->hasLat ? 1 : 0) +
				      (exif->gps->hasLonRef ? 1 : 0) +
				      (exif->gps->hasLon ? 1 : 0) +
				      (exif->gps->hasAltRef ? 1 : 0) +
				      (exif->gps->hasAlt ? 1 : 0) +
				      (exif->gps->hasTimeStamp ? 1 : 0) +
				      (exif->gps->hasMeasureMode ? 1 : 0) +
				      (exif->gps->hasImgDirectionRef ? 1 : 0) +
				      (exif->gps->hasImgDirection ? 1 : 0) +
				      (exif->gps->hasMapDatum ? 1 : 0) +
				      (exif->gps->hasDateStamp ? 1 : 0));
	}
	uint16_t ifd0Count =
		(uint16_t)((exif->hasMake ? 1 : 0) + (exif->hasModel ? 1 : 0) +
			   (exif->hasOrientation ? 1 : 0) + (exif->hasXRes ? 1 : 0) +
			   (exif->hasYRes ? 1 : 0) + (exif->hasResUnit ? 1 : 0) +
			   (exif->hasSoftware ? 1 : 0) + (exif->hasDateTime ? 1 : 0) +
			   (exif->hasYCbCrPositioning ? 1 : 0) + (exifCount != 0 ? 1 : 0) +
			   (gpsCount != 0 ? 1 : 0));
	uint16_t ifd1Count = 0;

	if (thumbSize != 0) {
		ifd1Count = (uint16_t)((exif->hasCompression ? 1 : 0) +
				       (exif->hasOrientation ? 1 : 0) + (exif->hasXRes ? 1 : 0) +
				       (exif->hasYRes ? 1 : 0) + (exif->hasResUnit ? 1 : 0) + 2);
	}

	uint32_t ifd0Patch = 0;
	uint32_t exifIfdPatch = 0;
	uint32_t gpsIfdPatch = 0;
	uint32_t interopIfdPatch = 0;
	uint32_t ifd1Patch = 0;

	int err = writer.BeginApp1();
	int step = writer.BeginTiff(&ifd0Patch);

	if ((uint32_t)step > (uint32_t)err)
		err = step;

	if (ifd0Count != 0) {
		writer.BeginIfd(ifd0Count, ifd0Patch);
		if (exif->hasMake)
			writer.WriteTag(271, 2, (uint32_t)exif->make.size() + 1,
					exif->make.c_str());
		if (exif->hasModel)
			writer.WriteTag(272, 2, (uint32_t)exif->model.size() + 1,
					exif->model.c_str());
		if (exif->hasOrientation)
			writer.WriteTag(274, 3, 1, &exif->orientation);
		if (exif->hasXRes)
			writer.WriteTag(282, 5, 1, &exif->xRes);
		if (exif->hasYRes)
			writer.WriteTag(283, 5, 1, &exif->yRes);
		if (exif->hasResUnit)
			writer.WriteTag(296, 3, 1, &exif->resUnit);
		if (exif->hasSoftware)
			writer.WriteTag(305, 2, (uint32_t)exif->software.size() + 1,
					exif->software.c_str());
		if (exif->hasDateTime)
			writer.WriteTag(306, 2, (uint32_t)exif->dateTime.size() + 1,
					exif->dateTime.c_str());
		if (exif->hasYCbCrPositioning)
			writer.WriteTag(531, 3, 1, &exif->yCbCrPositioning);
		if (exifCount != 0)
			writer.WriteDeferredOffsetTag(34665, 4, 1, &exifIfdPatch);
		if (gpsCount != 0)
			writer.WriteDeferredOffsetTag(34853, 4, 1, &gpsIfdPatch);
		writer.EndIfd(&ifd1Patch);
	}
	if (exifCount != 0) {
		writer.BeginIfd(exifCount, exifIfdPatch);
		if (exif->hasExifVersion)
			writer.WriteTag(36864, 7, 4, exif->exifVersion);
		if (exif->hasDateTimeOriginal)
			writer.WriteTag(36867, 2, (uint32_t)exif->dateTimeOriginal.size() + 1,
					exif->dateTimeOriginal.c_str());
		if (exif->hasDateTimeDigitized)
			writer.WriteTag(36868, 2, (uint32_t)exif->dateTimeDigitized.size() + 1,
					exif->dateTimeDigitized.c_str());
		if (exif->hasComponentsConfig)
			writer.WriteTag(37121, 7, 4, exif->componentsConfig);
		if (exif->hasFlash)
			writer.WriteTag(37385, 3, 1, &exif->flash);
		if (makerNotePtr != NULL && makerNoteSize != 0)
			writer.WriteTag(37500, 7, makerNoteSize, makerNotePtr);
		if (exif->hasFlashpixVersion)
			writer.WriteTag(40960, 7, 4, exif->flashpixVersion);
		if (exif->hasColorSpace)
			writer.WriteTag(40961, 3, 1, &exif->colorSpace);
		if (exif->hasPixelXDim)
			writer.WriteTag(40962, 4, 1, &exif->pixelXDim);
		if (exif->hasPixelYDim)
			writer.WriteTag(40963, 4, 1, &exif->pixelYDim);
		if (interopCount != 0)
			writer.WriteDeferredOffsetTag(40965, 4, 1, &interopIfdPatch);
		if (exif->hasCustomRendered)
			writer.WriteTag(41985, 3, 1, &exif->customRendered);
		if (exif->hasExposureMode)
			writer.WriteTag(41986, 3, 1, &exif->exposureMode);
		if (exif->hasWhiteBalance)
			writer.WriteTag(41987, 3, 1, &exif->whiteBalance);
		if (exif->hasSceneCaptureType)
			writer.WriteTag(41990, 3, 1, &exif->sceneCaptureType);
		writer.EndIfd(NULL);
	}
	if (gpsCount != 0 && exif->gps != NULL) {
		writer.BeginIfd(gpsCount, gpsIfdPatch);
		if (exif->gps->hasVersion)
			writer.WriteTag(0, 1, 4, exif->gps->version);
		if (exif->gps->hasLatRef)
			writer.WriteTag(1, 2, (uint32_t)exif->gps->latRef.size() + 1,
					exif->gps->latRef.c_str());
		if (exif->gps->hasLat)
			writer.WriteTag(2, 5, 3, exif->gps->lat);
		if (exif->gps->hasLonRef)
			writer.WriteTag(3, 2, (uint32_t)exif->gps->lonRef.size() + 1,
					exif->gps->lonRef.c_str());
		if (exif->gps->hasLon)
			writer.WriteTag(4, 5, 3, exif->gps->lon);
		if (exif->gps->hasAltRef)
			writer.WriteTag(5, 1, 1, &exif->gps->altRef);
		if (exif->gps->hasAlt)
			writer.WriteTag(6, 5, 1, &exif->gps->alt);
		if (exif->gps->hasTimeStamp)
			writer.WriteTag(7, 5, 3, exif->gps->timeStamp);
		if (exif->gps->hasMeasureMode)
			writer.WriteTag(10, 2, (uint32_t)exif->gps->measureMode.size() + 1,
					exif->gps->measureMode.c_str());
		if (exif->gps->hasImgDirectionRef)
			writer.WriteTag(16, 2, (uint32_t)exif->gps->imgDirectionRef.size() + 1,
					exif->gps->imgDirectionRef.c_str());
		if (exif->gps->hasImgDirection)
			writer.WriteTag(17, 5, 1, &exif->gps->imgDirection);
		if (exif->gps->hasMapDatum)
			writer.WriteTag(18, 2, (uint32_t)exif->gps->mapDatum.size() + 1,
					exif->gps->mapDatum.c_str());
		if (exif->gps->hasDateStamp)
			writer.WriteTag(29, 2, (uint32_t)exif->gps->dateStamp.size() + 1,
					exif->gps->dateStamp.c_str());
		writer.EndIfd(NULL);
	}
	if (interopCount != 0) {
		writer.BeginIfd(interopCount, interopIfdPatch);
		if (exif->hasInteropIndex)
			writer.WriteTag(1, 2, (uint32_t)exif->interopIndex.size() + 1,
					exif->interopIndex.c_str());
		if (exif->hasInteropVersion)
			writer.WriteTag(2, 7, 4, exif->interopVersion);
		writer.EndIfd(NULL);
	}
	if (ifd1Count != 0) {
		uint32_t thumbOffsetPatch = 0;

		writer.BeginIfd(ifd1Count, ifd1Patch);
		if (exif->hasCompression)
			writer.WriteTag(259, 3, 1, &exif->compression);
		if (exif->hasOrientation)
			writer.WriteTag(274, 3, 1, &exif->orientation);
		if (exif->hasXRes)
			writer.WriteTag(282, 5, 1, &exif->xRes);
		if (exif->hasYRes)
			writer.WriteTag(283, 5, 1, &exif->yRes);
		if (exif->hasResUnit)
			writer.WriteTag(296, 3, 1, &exif->resUnit);
		writer.WriteDeferredOffsetTag(513, 4, 1, &thumbOffsetPatch);
		writer.WriteTag(514, 4, 1, &thumbSize);
		writer.PatchExternalPayload(thumbSize, thumbOffsetPatch);
		writer.EndIfd(NULL);
	}
	return writer.EndApp1(outPtr, outLen);
}

/* Pixel source abstraction for RGBA/RGB/YUV420 reading and overlay compositing */
struct RawImagePlane {
	const uint8_t *pixels;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t cropX;
	uint32_t cropY;
	uint32_t cropW;
	uint32_t cropH;
	int pixelFormat;
	int yuvStandard;
};

static inline uint8_t ClampByte(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return (uint8_t)val;
}

static uint32_t ReadRawPixelRgba(const RawImagePlane &img, uint32_t x, uint32_t y)
{
	uint32_t sx = img.cropX + x;
	uint32_t sy = img.cropY + y;

	if (sx >= img.width)
		sx = img.width - 1;
	if (sy >= img.height)
		sy = img.height - 1;
	switch (img.pixelFormat) {
	case 3:
	case 4: { /* RGBA8888 */
		const uint8_t *p = img.pixels + sy * img.pitch + sx * 4;

		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
		       ((uint32_t)p[3] << 24);
	}
	case 5:
	case 6: { /* BGRA8888 */
		const uint8_t *p = img.pixels + sy * img.pitch + sx * 4;

		return (uint32_t)p[2] | ((uint32_t)p[1] << 8) | ((uint32_t)p[0] << 16) |
		       ((uint32_t)p[3] << 24);
	}
	case 0x19: { /* RGB888 */
		const uint8_t *p = img.pixels + sy * img.pitch + sx * 3;

		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
		       0xFF000000U;
	}
	case 0x1A: { /* BGR888 */
		const uint8_t *p = img.pixels + sy * img.pitch + sx * 3;

		return (uint32_t)p[2] | ((uint32_t)p[1] << 8) | ((uint32_t)p[0] << 16) |
		       0xFF000000U;
	}
	case 0x1C: { /* YUV420 planar */
		uint32_t ySize = img.pitch * img.height;
		uint32_t uvPitch = img.pitch >> 1;
		uint32_t uvSize = uvPitch * (img.height >> 1);
		int yVal = (int)img.pixels[sy * img.pitch + sx];
		int uVal = (int)img.pixels[ySize + (sy >> 1) * uvPitch + (sx >> 1)] - 128;
		int vVal = (int)img.pixels[ySize + uvSize + (sy >> 1) * uvPitch + (sx >> 1)] - 128;
		int r = yVal + ((359 * vVal) >> 8);
		int g = yVal - ((88 * uVal + 183 * vVal) >> 8);
		int b = yVal + ((454 * uVal) >> 8);

		return (uint32_t)ClampByte(r) | ((uint32_t)ClampByte(g) << 8) |
		       ((uint32_t)ClampByte(b) << 16) | 0xFF000000U;
	}
	default:
		return 0xFF000000U;
	}
}

static uint32_t SampleCompositedPixel(const RawImagePlane &base, const RawImagePlane *overlay,
				      uint8_t overlayAlpha, uint32_t x, uint32_t y)
{
	uint32_t basePx = ReadRawPixelRgba(base, x, y);

	if (overlay == NULL || overlay->pixels == NULL || overlayAlpha == 0)
		return basePx;
	uint32_t ovW = (overlay->cropW != 0) ? overlay->cropW : overlay->width;
	uint32_t ovH = (overlay->cropH != 0) ? overlay->cropH : overlay->height;
	uint32_t baseW = (base.cropW != 0) ? base.cropW : base.width;
	uint32_t baseH = (base.cropH != 0) ? base.cropH : base.height;
	int offX = ((int)baseW - (int)ovW) / 2;
	int offY = ((int)baseH - (int)ovH) / 2;
	int ox = (int)x - offX;
	int oy = (int)y - offY;

	if (ox < 0 || oy < 0 || (uint32_t)ox >= ovW || (uint32_t)oy >= ovH)
		return basePx;
	uint32_t ovPx = ReadRawPixelRgba(*overlay, (uint32_t)ox, (uint32_t)oy);
	uint32_t a = (((ovPx >> 24) & 0xFFU) * overlayAlpha) / 255U;
	uint32_t invA = 255U - a;
	uint32_t r = (((ovPx & 0xFFU) * a) + ((basePx & 0xFFU) * invA)) / 255U;
	uint32_t g = ((((ovPx >> 8) & 0xFFU) * a) + (((basePx >> 8) & 0xFFU) * invA)) / 255U;
	uint32_t b = ((((ovPx >> 16) & 0xFFU) * a) + (((basePx >> 16) & 0xFFU) * invA)) / 255U;

	return (r & 0xFFU) | ((g & 0xFFU) << 8) | ((b & 0xFFU) << 16) | 0xFF000000U;
}

static inline void RgbToYCbCr(uint32_t rgba, uint8_t &outY, uint8_t &outCb, uint8_t &outCr)
{
	int r = (int)(rgba & 0xFFU);
	int g = (int)((rgba >> 8) & 0xFFU);
	int b = (int)((rgba >> 16) & 0xFFU);
	int y = ((77 * r + 150 * g + 29 * b + 128) >> 8);
	int cb = ((-43 * r - 85 * g + 128 * b + 128) >> 8) + 128;
	int cr = ((128 * r - 107 * g - 21 * b + 128) >> 8) + 128;

	outY = ClampByte(y);
	outCb = ClampByte(cb);
	outCr = ClampByte(cr);
}

static int FindJpegDqtOffset(const uint8_t *buf, int len)
{
	if (buf == NULL || len < 2 || buf[0] != 0xFF || buf[1] != 0xD8)
		return -1;
	int pos = 2;

	while (pos + 4 <= len) {
		if (buf[pos] == 0xFF) {
			uint8_t marker = buf[pos + 1];

			if (marker == 0xDB)
				return pos;
			if (marker != 0x00 && marker != 0xFF) {
				int segLen = ((int)buf[pos + 2] << 8) | buf[pos + 3];

				pos += segLen + 2;
				continue;
			}
		}
		++pos;
	}
	return -1;
}

/* Encode an image (scaled or native) to JPEG via SceJpegEncArm MCU-by-MCU */
static int EncodeJpegStream(const RawImagePlane &base, const RawImagePlane *overlay,
			    uint8_t overlayAlpha, uint32_t outW, uint32_t outH, int mcuMode,
			    uint8_t compRatio, bool omitSoi, PictureOutputStream *outStream,
			    const EncodeCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	uint32_t mcuH = (mcuMode == 2) ? 8U : 16U;
	uint32_t uvDivY = (mcuMode == 2) ? 1U : 2U;
	uint32_t uvH = (mcuH + uvDivY - 1U) / uvDivY;
	uint32_t uvRowBytes = 8U;
	uint32_t ySize = 16U * mcuH;
	uint32_t uvSize = uvRowBytes * uvH;
	uint8_t *mcuBuf = (uint8_t *)ScePafStdc_3CF19248(ySize + 2U * uvSize, s_workHeap);

	if (mcuBuf == NULL)
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	uint8_t *jpegChunk = (uint8_t *)ScePafStdc_3CF19248(1024, s_workHeap);

	if (jpegChunk == NULL) {
		operator delete(mcuBuf);
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}
	int ctxSize = sceJpegArmEncoderGetContextSize();
	void *encCtx = (ctxSize > 0) ? ScePafStdc_3CF19248((size_t)ctxSize, s_workHeap) : NULL;

	if (encCtx == NULL) {
		operator delete(jpegChunk);
		operator delete(mcuBuf);
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	}

	uint32_t alignedW = (outW + 15U) & ~15U;
	uint32_t alignedH = (outH + mcuH - 1U) & ~(mcuH - 1U);
	int pixelFmt = (mcuMode == 3) ? SCE_JPEGENC_PIXEL_YCBCR420 : SCE_JPEGENC_PIXEL_YCBCR422;
	int ret = sceJpegArmEncoderInit(encCtx, (int)alignedW, (int)alignedH, pixelFmt, NULL, 0);

	if (ret == 0) {
		if (alignedW != outW || alignedH != outH)
			ret = sceJpegArmEncoderSetValidRegion(encCtx, (uint16_t)outW,
							      (uint16_t)outH);
		if (ret == 0)
			ret = sceJpegArmEncoderSetCompressionRatio(encCtx, compRatio);
		if (ret == 0 && !omitSoi) {
			static const uint8_t soi[2] = { 0xFF, 0xD8 };

			ret = outStream->Write(soi, 2);
		}
		if (ret == 0) {
			uint8_t *yPlane = mcuBuf;
			uint8_t *cbPlane = mcuBuf + ySize;
			uint8_t *crPlane = cbPlane + uvSize;
			int headerSize = 0;

			sce_paf_memset(mcuBuf, 0, ySize + 2U * uvSize);
			ret = SceJpegEncArm_4EE1BAED(encCtx, yPlane, cbPlane, crPlane, 2,
						     jpegChunk, 1024, &headerSize);
			if (ret >= 0) {
				int dqtOff = FindJpegDqtOffset(jpegChunk, headerSize);

				if (dqtOff < 0) {
					ret = PIC_CREATOR_ERROR_GENERIC;
				} else {
					ret = outStream->Write(jpegChunk + dqtOff,
							       (SceSize)(headerSize - dqtOff));
				}
			}
			uint32_t srcW = (base.cropW != 0) ? base.cropW : base.width;
			uint32_t srcH = (base.cropH != 0) ? base.cropH : base.height;

			for (uint32_t my = 0; ret == 0 && my < alignedH; my += mcuH) {
				for (uint32_t mx = 0; ret == 0 && mx < alignedW; mx += 16U) {
					if (IsEncodeCanceled(cancelHook)) {
						ret = PIC_CREATOR_ERROR_CANCELED;
						break;
					}
					for (uint32_t row = 0; row < mcuH; ++row) {
						uint32_t py = (my + row < outH) ? (my + row)
										: (outH - 1U);
						uint32_t sy = (py * srcH) / outH;

						for (uint32_t col = 0; col < 16U; ++col) {
							uint32_t px = (mx + col < outW)
									      ? (mx + col)
									      : (outW - 1U);
							uint32_t sx = (px * srcW) / outW;
							uint32_t rgba = SampleCompositedPixel(
								base, overlay, overlayAlpha, sx,
								sy);
							uint8_t yv, cbv, crv;

							RgbToYCbCr(rgba, yv, cbv, crv);
							yPlane[row * 16U + col] = yv;
							if ((col & 1U) == 0 &&
							    (uvDivY == 1U || (row & 1U) == 0)) {
								uint32_t uIdx =
									(row / uvDivY) * 8U +
									(col >> 1);

								cbPlane[uIdx] = cbv;
								crPlane[uIdx] = crv;
							}
						}
					}
					int chunkLen = 0;
					int encStatus = SceJpegEncArm_4EE1BAED(
						encCtx, yPlane, cbPlane, crPlane, 3, jpegChunk,
						1024, &chunkLen);

					if (encStatus < 0) {
						ret = encStatus;
						break;
					}
					ret = outStream->Write(jpegChunk, (SceSize)chunkLen);
					if (ret != 0 || encStatus > 0)
						break;
				}
			}
			if (ret == 0)
				ret = (int)outStream->GetWrittenSize();
		}
		sceJpegArmEncoderEnd(encCtx);
	}
	operator delete(encCtx);
	operator delete(jpegChunk);
	operator delete(mcuBuf);
	return ret;
}

static int EncodeJpegWithExif(const RawImagePlane &base, const RawImagePlane *overlay,
			      uint8_t overlayAlpha, const ExifMetadata *exif, uint8_t compRatio,
			      PictureOutputStream *outStream, const EncodeCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;
	uint32_t thumbBufSize = 39424U;
	uint8_t *thumbBuf = (uint8_t *)ScePafStdc_3CF19248(thumbBufSize, s_workHeap);

	if (thumbBuf == NULL)
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	MemoryOutputStream thumbStream;
	int ret = thumbStream.Open(thumbBuf, thumbBufSize);

	if (ret == 0) {
		static const uint8_t thumbRatios[6] = { 8, 32, 64, 128, 192, 255 };
		int thumbRes = PIC_CREATOR_ERROR_GENERIC;

		for (int i = 0; i < 6; ++i) {
			ret = thumbStream.Rewind();
			if (ret != 0)
				break;
			thumbRes = EncodeJpegStream(base, overlay, overlayAlpha, 160, 120, 2,
						    thumbRatios[i], false, &thumbStream,
						    cancelHook);
			if (thumbRes > 0)
				break;
		}
		if (ret == 0 && thumbRes <= 0)
			ret = (thumbRes < 0) ? thumbRes : PIC_CREATOR_ERROR_GENERIC;
		if (ret == 0)
			ret = thumbStream.Flush();
		if (ret == 0 && IsEncodeCanceled(cancelHook))
			ret = PIC_CREATOR_ERROR_CANCELED;
		if (ret == 0) {
			static const uint8_t soi[2] = { 0xFF, 0xD8 };

			ret = outStream->Write(soi, 2);
		}
		if (ret == 0 && exif != NULL) {
			MakerNoteWriter makerWriter;
			ExifApp1Writer exifWriter;
			void *makerPtr = NULL;
			uint32_t makerLen = 0;

			if (exif->makerNote != NULL) {
				int mnRet = makerWriter.BuildFromParam(exif->makerNote, &makerPtr,
								       &makerLen);

				if (mnRet < 0)
					ret = (mnRet == (int)0x8000000C)
						      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
						      : PIC_CREATOR_ERROR_GENERIC;
			}
			if (ret == 0) {
				void *app1Ptr = NULL;
				uint32_t app1Len = 0;
				int app1Ret = BuildExifApp1Segment(
					exifWriter, exif, thumbStream.GetWrittenSize(), makerPtr,
					makerLen, &app1Ptr, &app1Len);

				if (app1Ret < 0) {
					ret = (app1Ret == (int)0x8000000C)
						      ? PIC_CREATOR_ERROR_OUT_OF_MEMORY
						      : PIC_CREATOR_ERROR_GENERIC;
				} else {
					ret = outStream->Write(app1Ptr, app1Len);
					if (ret == 0)
						ret = thumbStream.CopyTo(outStream);
				}
			}
		}
		if (ret == 0 && IsEncodeCanceled(cancelHook))
			ret = PIC_CREATOR_ERROR_CANCELED;
		if (ret == 0) {
			uint32_t outW = (base.cropW != 0) ? base.cropW : base.width;
			uint32_t outH = (base.cropH != 0) ? base.cropH : base.height;
			int mainRes = EncodeJpegStream(base, overlay, overlayAlpha, outW, outH, 3,
						       compRatio, true, outStream, cancelHook);

			ret = (mainRes > 0) ? 0
					    : ((mainRes < 0) ? mainRes : PIC_CREATOR_ERROR_GENERIC);
		}
	}
	operator delete(thumbBuf);
	return ret;
}

static int EncodeBmpStream(const RawImagePlane &base, const RawImagePlane *overlay,
			   uint8_t overlayAlpha, PictureOutputStream *outStream,
			   const EncodeCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;
	uint32_t outW = (base.cropW != 0) ? base.cropW : base.width;
	uint32_t outH = (base.cropH != 0) ? base.cropH : base.height;
	uint32_t rowStride = (3U * outW + 3U) & ~3U;
	uint32_t padBytes = rowStride - 3U * outW;
	uint32_t imgBytes = rowStride * outH;
	uint8_t fileHeader[14];
	uint32_t totalSize = 54U + imgBytes;

	sce_paf_memset(fileHeader, 0, sizeof(fileHeader));
	fileHeader[0] = 'B';
	fileHeader[1] = 'M';
	fileHeader[2] = (uint8_t)(totalSize & 0xFFU);
	fileHeader[3] = (uint8_t)((totalSize >> 8) & 0xFFU);
	fileHeader[4] = (uint8_t)((totalSize >> 16) & 0xFFU);
	fileHeader[5] = (uint8_t)((totalSize >> 24) & 0xFFU);
	fileHeader[10] = 54;

	struct BmpDibHeader {
		uint32_t headerSize;
		int32_t width;
		int32_t height;
		uint16_t planes;
		uint16_t bpp;
		uint32_t compression;
		uint32_t imageSize;
		int32_t xPpm;
		int32_t yPpm;
		uint32_t clrUsed;
		uint32_t clrImportant;
	};
	BmpDibHeader dib;

	sce_paf_memset(&dib, 0, sizeof(dib));
	dib.headerSize = 40;
	dib.width = (int32_t)outW;
	dib.height = -(int32_t)outH;
	dib.planes = 1;
	dib.bpp = 24;
	dib.imageSize = imgBytes;

	int ret = outStream->Write(fileHeader, 14);

	if (ret == 0)
		ret = outStream->Write(&dib, 40);
	static const uint8_t zeroPad[4] = { 0, 0, 0, 0 };

	for (uint32_t y = 0; ret == 0 && y < outH; ++y) {
		if (IsEncodeCanceled(cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t x = 0; x < outW; ++x) {
			uint32_t rgba = SampleCompositedPixel(base, overlay, overlayAlpha, x, y);
			uint8_t bgr[3] = { (uint8_t)((rgba >> 16) & 0xFFU),
					   (uint8_t)((rgba >> 8) & 0xFFU),
					   (uint8_t)(rgba & 0xFFU) };

			ret = outStream->Write(bgr, 3);
			if (ret != 0)
				break;
		}
		if (ret == 0 && padBytes != 0)
			ret = outStream->Write(zeroPad, padBytes);
	}
	return ret;
}

static int EncodeRawStream(const RawImagePlane &base, const RawImagePlane *overlay,
			   uint8_t overlayAlpha, PictureOutputStream *outStream,
			   const EncodeCancelHook *cancelHook)
{
	if (outStream == NULL || !outStream->IsOpen())
		return PIC_CREATOR_ERROR_GENERIC;
	uint32_t outW = (base.cropW != 0) ? base.cropW : base.width;
	uint32_t outH = (base.cropH != 0) ? base.cropH : base.height;
	uint32_t alignedW = (outW + 7U) & ~7U;
	uint32_t padBytes = (alignedW - outW) * 4U;
	static const uint8_t zeroRowPad[32] = { 0 };

	for (uint32_t y = 0; y < outH; ++y) {
		if (IsEncodeCanceled(cancelHook))
			return PIC_CREATOR_ERROR_CANCELED;
		for (uint32_t x = 0; x < outW; ++x) {
			uint32_t rgba = SampleCompositedPixel(base, overlay, overlayAlpha, x, y);
			uint8_t px[4] = { (uint8_t)(rgba & 0xFFU), (uint8_t)((rgba >> 8) & 0xFFU),
					  (uint8_t)((rgba >> 16) & 0xFFU),
					  (uint8_t)((rgba >> 24) & 0xFFU) };
			int ret = outStream->Write(px, 4);

			if (ret != 0)
				return ret;
		}
		if (padBytes != 0) {
			int ret = outStream->Write(zeroRowPad, padBytes);

			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

/* Compute required output buffer and work memory sizes */
static int QueryEncodeMemorySizes(const RawImagePlane &base, int format, uint32_t destFlags,
				  uint32_t *outBufSize, uint32_t *outWorkSize)
{
	uint32_t w = (base.cropW != 0) ? base.cropW : base.width;
	uint32_t h = (base.cropH != 0) ? base.cropH : base.height;
	uint32_t minOut = 0;
	uint32_t work = 0;

	if (format == PIC_FORMAT_JPEG) {
		if ((destFlags & 1U) == 0)
			minOut = (((w + 15U) >> 4) * ((h + 15U) >> 4) << 10) + 98304U;
		work = ((destFlags & 2U) != 0) ? 11136U : 48512U;
	} else if (format == PIC_FORMAT_BMP) {
		if ((destFlags & 1U) == 0)
			minOut = 54U + h * ((3U * w + 3U) & ~3U);
		work = 512U;
	} else if (format == PIC_FORMAT_RAW) {
		if ((destFlags & 1U) == 0)
			minOut = 32U * ((w + 7U) >> 3) * h;
		work = 512U;
	} else {
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	}
	work += ((destFlags & 1U) != 0) ? 9344U : 1152U;
	*outBufSize = minOut;
	*outWorkSize = (work + 1535U) & ~1023U;
	return 0;
}

/* Export a newly written photo file into photo0:/CAMERA/ and register it */
static int ExportPhotoToDatabase(const char *srcPath, int format, rtc::Tick localTick,
				 uint64_t *outPhotoId)
{
	rtc::DateTime dt;
	int ret = rtc::ConvertTickToDateTime(&localTick, &dt, NULL);

	if (ret != 0)
		return ret;
	char subDir[3];
	uint32_t seed = dt.microsecond;

	subDir[0] = (char)('b' + (seed % 10U));
	subDir[1] = (char)('b' + ((seed / 10U) % 10U));
	subDir[2] = '\0';

	char dirPath[64];

	sce_paf_memset(dirPath, 0, sizeof(dirPath));
	sce_paf_snprintf(dirPath, sizeof(dirPath), "%s%s", s_photoBaseDir, subDir);
	dirPath[63] = '\0';
	if (!Dir::IsExist(dirPath)) {
		ret = Dir::Create(dirPath, 6);
		if (ret != 0)
			return ret;
	}

	const char *ext = s_extDat;

	if (format == PIC_FORMAT_JPEG)
		ext = s_extJpg;
	else if (format == PIC_FORMAT_BMP)
		ext = s_extBmp;

	char stamp[64];

	sce_paf_memset(stamp, 0, sizeof(stamp));
	sce_paf_snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d-%02d%02d%02d", dt.year, dt.month,
			 dt.day, dt.hour, dt.minute, dt.second);
	stamp[63] = '\0';

	char dstPath[64];
	uint32_t suffix = 0;

	sce_paf_memset(dstPath, 0, sizeof(dstPath));
	for (; suffix <= 9U; ++suffix) {
		if (suffix == 0)
			sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s.%s", dirPath, stamp, ext);
		else
			sce_paf_snprintf(dstPath, sizeof(dstPath), "%s/%s_%u.%s", dirPath, stamp,
					 suffix, ext);
		dstPath[63] = '\0';
		if (!LocalFile::Exists(dstPath))
			break;
	}
	if (suffix > 9U)
		return CAMERA_CORE_ERROR_FULL;
	ret = LocalFile::RenameFile(srcPath, dstPath);
	if (ret != 0)
		return ret;
	return PhotoExportRegisterFile(dstPath, outPhotoId);
}

/* Public API */
int PictureFileCreatorInit(void)
{
	if (s_creatorInitialized)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	s_creatorInitialized = true;
	return 0;
}

int PictureFileCreatorTerm(void)
{
	s_creatorInitialized = false;
	return 0;
}

int PhotoExportInit(void)
{
	s_photoExportInitialized = true;
	return 0;
}

int PhotoExportTerm(void)
{
	s_photoExportInitialized = false;
	return 0;
}

int PhotoExportRegisterFile(const char *filePath, uint64_t *outPhotoId)
{
	if (!s_photoExportInitialized || filePath == NULL)
		return CAMERA_CORE_ERROR_GENERIC;
	if (outPhotoId != NULL)
		*outPhotoId = 0;
	return 0;
}

int PictureFileCreatorEncode(ImageEncodeJobItem *job)
{
	if (job == NULL || job->m_input.surface.get() == NULL)
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	if (job->m_option.destination == 0) {
		if (job->m_option.filePath == NULL)
			return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	} else if (job->m_option.destination == 1) {
		if (job->m_option.outputBuffer == NULL || job->m_option.outputBufferSize == 0)
			return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}

	rtc::Tick localTick = 0;
	int tickRet = rtc::GetCurrentLocalTick(&localTick);

	if (tickRet != 0)
		return tickRet;

	ExifMetadata exif;
	ExifGpsMetadata gps;

	exif.makerNote = &job->m_makerNoteCopy;
	exif.gps = &gps;
	if (PopulateExifMetadata(job->m_input.frameInfo, job->m_input.locationInfo,
				 job->m_input.photoMeta, localTick, &exif) != 0)
		return CAMERA_CORE_ERROR_GENERIC;

	Surface *mainSurf = job->m_input.surface.get();
	const uint8_t *mainPixels = (const uint8_t *)mainSurf->Lock(0);

	if (mainPixels == NULL) {
		mainSurf->Unlock();
		return CAMERA_CORE_ERROR_INVALID_ARGUMENT;
	}

	RawImagePlane basePlane;

	basePlane.pixels = mainPixels;
	basePlane.width = (uint32_t)mainSurf->GetWidth();
	basePlane.height = (uint32_t)mainSurf->GetHeight();
	basePlane.pitch = (uint32_t)mainSurf->GetStride();
	basePlane.cropX = (uint32_t)job->m_frameInfoCopy.cropX;
	basePlane.cropY = (uint32_t)job->m_frameInfoCopy.cropY;
	basePlane.cropW = (uint32_t)job->m_frameInfoCopy.cropWidth;
	basePlane.cropH = (uint32_t)job->m_frameInfoCopy.cropHeight;
	basePlane.pixelFormat = (int)mainSurf->m_format;
	basePlane.yuvStandard = (job->m_frameInfoCopy.format == 2) ? 1 : 0;

	RawImagePlane overlayPlane;
	const RawImagePlane *overlayPtr = NULL;
	Surface *ovSurf = job->m_overlay.surface.get();

	if (ovSurf != NULL) {
		const uint8_t *ovPixels = (const uint8_t *)ovSurf->Lock(0);

		if (ovPixels != NULL) {
			overlayPlane.pixels = ovPixels;
			overlayPlane.width = (uint32_t)ovSurf->GetWidth();
			overlayPlane.height = (uint32_t)ovSurf->GetHeight();
			overlayPlane.pitch = (uint32_t)ovSurf->GetStride();
			overlayPlane.cropX = (uint32_t)job->m_overlay.offsetX;
			overlayPlane.cropY = (uint32_t)job->m_overlay.offsetY;
			overlayPlane.cropW = (uint32_t)job->m_overlay.width;
			overlayPlane.cropH = (uint32_t)job->m_overlay.height;
			overlayPlane.pixelFormat = (int)ovSurf->m_format;
			overlayPlane.yuvStandard = 0;
			overlayPtr = &overlayPlane;
		}
	}

	uint32_t destFlags = (job->m_option.destination == 0) ? 1U : 0U;
	uint32_t minOutSize = 0;
	uint32_t workSize = 0;
	int ret = QueryEncodeMemorySizes(basePlane, job->m_option.format, destFlags, &minOutSize,
					 &workSize);

	if (ret == 0 && job->m_option.destination == 1 && minOutSize > job->m_option.outputBufferSize)
		ret = CAMERA_CORE_ERROR_GENERIC;

	uint8_t *workMem = NULL;

	if (ret == 0 && workSize != 0) {
		workMem = new uint8_t[workSize];
		if (workMem == NULL)
			ret = CAMERA_CORE_ERROR_OUT_OF_MEMORY;
	}

	if (ret == 0) {
		uintptr_t alignedBase = ((uintptr_t)workMem + 15U) & ~(uintptr_t)15U;
		HeapAllocator heapCtx((void *)alignedBase, workSize - 16U,
				      "ScePictureFileCreatorWorkMemory", NULL);

		s_workHeap = &heapCtx;

		EncodeCancelHook cancelHook = { CheckJobCanceled, job };
		PictureOutputStream *stream = NULL;
		MemoryOutputStream memStream;
		FileOutputStream *fileStream = NULL;

		if (job->m_option.destination == 0) {
			fileStream = new FileOutputStream(0x2000);
			if (fileStream == NULL) {
				ret = PIC_CREATOR_ERROR_OUT_OF_MEMORY;
			} else {
				ret = fileStream->Open(job->m_option.filePath, 0x10000U);
				stream = fileStream;
			}
		} else {
			ret = memStream.Open(job->m_option.outputBuffer,
					     job->m_option.outputBufferSize);
			stream = &memStream;
		}

		if (ret == 0 && stream != NULL) {
			if (job->m_option.format == PIC_FORMAT_JPEG)
				ret = EncodeJpegWithExif(basePlane, overlayPtr,
							 job->m_option.overlayAlpha, &exif,
							 job->m_option.jpegCompressionRatio,
							 stream, &cancelHook);
			else if (job->m_option.format == PIC_FORMAT_BMP)
				ret = EncodeBmpStream(basePlane, overlayPtr,
						      job->m_option.overlayAlpha, stream,
						      &cancelHook);
			else if (job->m_option.format == PIC_FORMAT_RAW)
				ret = EncodeRawStream(basePlane, overlayPtr,
						      job->m_option.overlayAlpha, stream,
						      &cancelHook);
			else
				ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		}

		uint32_t writtenBytes = 0;

		if (stream != NULL && stream->IsOpen()) {
			writtenBytes = stream->GetWrittenSize();
			int closeRet = stream->Close();

			if (ret == 0)
				ret = closeRet;
			if (ret != 0 && job->m_option.destination == 0) {
				LocalFile::RemoveFile(job->m_option.filePath);
				writtenBytes = 0;
			}
		}
		if (fileStream != NULL)
			delete fileStream;

		s_workHeap = NULL;

		if (ret == 0 && writtenBytes > 0) {
			job->m_encodedSize = writtenBytes;
			if (job->m_option.destination == 0 && job->m_option.registerToPhotoDb) {
				uint64_t photoId = (uint64_t)-1;
				int expRet = ExportPhotoToDatabase(job->m_option.filePath,
								   job->m_option.format, localTick,
								   &photoId);

				if (expRet >= 0) {
					job->m_outputWidth = job->m_frameInfoCopy.cropWidth;
					job->m_outputHeight = job->m_frameInfoCopy.cropHeight;
					job->m_outputOrientation =
						job->m_locationCopy.exifOrientation;
					job->m_photoId = photoId;
				} else {
					ret = expRet;
				}
			}
		} else if (ret == 0) {
			ret = CAMERA_CORE_ERROR_GENERIC;
		}
	}

	if (workMem != NULL)
		delete[] workMem;
	mainSurf->Unlock();
	if (ovSurf != NULL)
		ovSurf->Unlock();
	return ret;
}
