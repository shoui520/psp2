/*
 * picture_stream.cpp: work-heap holder, MemoryOutputStream, and double-buffered FileOutputStream
 * for ScePictureFileCreator.
 * 3.65-CEX: 0x81366960 - 0x813669A7, 0x81368670 - 0x81368D0F
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <file/localfile.h>

#include "picture_file_creator_internal.h"

using namespace paf;
using namespace paf::memory;

static HeapAllocator *s_pictureWorkHeap = NULL; /* 0x81548824 */

/* 0x81366960 */
void pictureWorkHeapClear(void)
{
	s_pictureWorkHeap = NULL;
}

/* 0x81366970 */
int pictureWorkHeapSet(HeapAllocator *heap)
{
	if (s_pictureWorkHeap != NULL)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (heap == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	s_pictureWorkHeap = heap;
	return 0;
}

/* 0x8136699C */
HeapAllocator *pictureWorkHeapGet(void)
{
	return s_pictureWorkHeap;
}

static int openLocalFileSync(LocalFile &file, const LocalFile::OpenArg &openArg)
{
	int ret = file.OpenAsync(&openArg);
	if (ret != 0)
		return ret;
	int64_t asyncResult = 0;
	ret = file.WaitAsync(&asyncResult);
	if (ret != 0)
		return ret;
	return static_cast<int>(asyncResult);
}

/* --- PictureMemoryStream (0x81368BE8 - 0x81368D0F) --- */

PictureMemoryStream::PictureMemoryStream()
	: m_buffer(NULL), m_capacity(0), m_written(0)
{
}

int PictureMemoryStream::Open(void *buffer, uint32_t capacity)
{
	if (IsOpen())
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (buffer == NULL || capacity == 0)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	m_buffer = static_cast<uint8_t *>(buffer);
	m_capacity = capacity;
	m_written = 0;
	return 0;
}

bool PictureMemoryStream::IsOpen() const
{
	return m_buffer != NULL;
}

int PictureMemoryStream::Write(const void *src, SceSize len)
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

int PictureMemoryStream::Flush()
{
	return IsOpen() ? 0 : PIC_CREATOR_ERROR_INVALID_STATE;
}

int PictureMemoryStream::Rewind()
{
	if (!IsOpen())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	m_written = 0;
	return 0;
}

uint32_t PictureMemoryStream::GetWrittenSize() const
{
	return IsOpen() ? m_written : 0;
}

int PictureMemoryStream::Close()
{
	m_buffer = NULL;
	m_capacity = 0;
	m_written = 0;
	return 0;
}

int PictureMemoryStream::CopyTo(PictureOutputStream *dst)
{
	if (!IsOpen())
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (dst == NULL || !dst->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	return dst->Write(m_buffer, m_written);
}

/* --- PictureFileStream (0x81368670 - 0x81368BE7) --- */

PictureFileStream::PictureFileStream(uint32_t totalBufSize)
	: m_activeIndex(0),
	  m_bufSize(totalBufSize >> 1),
	  m_bufferedBytes(0),
	  m_opened(false),
	  m_pendingAsync(false),
	  m_pendingBytes(0),
	  m_totalWritten(0),
	  m_file(s_pictureWorkHeap)
{
	m_buffers[0] = NULL;
	m_buffers[1] = NULL;
	m_openArg.filename = NULL;
	m_openArg.flag = 0;
	m_openArg.mode = 0;

	if (m_bufSize != 0) {
		m_buffers[0] = static_cast<uint8_t *>(
			ScePafStdc_3CF19248(m_bufSize, s_pictureWorkHeap));
		m_buffers[1] = static_cast<uint8_t *>(
			ScePafStdc_3CF19248(m_bufSize, s_pictureWorkHeap));
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

PictureFileStream::~PictureFileStream()
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

int PictureFileStream::Open(const char *path, uint32_t extraFlags)
{
	if (m_bufSize != 0 && (m_buffers[0] == NULL || m_buffers[1] == NULL))
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	if (m_opened)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (path == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	m_openArg.filename = path;
	m_openArg.flag = extraFlags | 0x603u;
	m_openArg.mode = 6;
	m_pendingAsync = false;

	int ret = openLocalFileSync(m_file, m_openArg);
	if (ret == 0) {
		m_opened = true;
		m_totalWritten = 0;
		m_bufferedBytes = 0;
	}
	return ret;
}

bool PictureFileStream::IsOpen() const
{
	return m_opened;
}

int PictureFileStream::WaitPendingWrite()
{
	bool wasPending = m_pendingAsync;
	int expected = static_cast<int>(m_pendingBytes);
	int64_t asyncResult = 0;

	m_pendingAsync = false;
	int ret = m_file.WaitAsync(&asyncResult);
	int writtenResult = static_cast<int>(asyncResult);

	if (wasPending) {
		if (ret != 0)
			return (ret < 0) ? ret : PIC_CREATOR_ERROR_IO_ERROR;
		if (writtenResult != expected)
			return (writtenResult < 0) ? writtenResult : PIC_CREATOR_ERROR_IO_ERROR;
		return 0;
	}
	if (ret == -2135994366 || (ret == 0 && writtenResult >= 0))
		return 0;
	if (ret < 0)
		return ret;
	return (writtenResult < 0) ? writtenResult : PIC_CREATOR_ERROR_IO_ERROR;
}

int PictureFileStream::FlushBuffer()
{
	int ret = 0;
	if (!m_opened) {
		ret = PIC_CREATOR_ERROR_INVALID_STATE;
	} else if (m_bufferedBytes != 0) {
		uint8_t *activeBuf = m_buffers[m_activeIndex];
		if (activeBuf == NULL) {
			ret = PIC_CREATOR_ERROR_INVALID_ARGUMENT;
		} else {
			ret = WaitPendingWrite();
			if (ret == 0) {
				m_pendingBytes = m_bufferedBytes;
				m_pendingAsync = true;
				ret = m_file.WriteAsync(activeBuf, m_bufferedBytes);
				m_totalWritten += m_bufferedBytes;
				if (ret >= 0)
					ret = 0;
			}
		}
	}
	m_activeIndex = 1 - m_activeIndex;
	m_bufferedBytes = 0;
	return ret;
}

int PictureFileStream::Write(const void *src, SceSize len)
{
	if (m_bufSize != 0 && (m_buffers[0] == NULL || m_buffers[1] == NULL))
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;
	if (!m_opened)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (len == 0)
		return 0;
	if (src == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	const uint8_t *cursor = static_cast<const uint8_t *>(src);
	SceSize remaining = len;

	if (m_bufferedBytes != 0) {
		SceSize space = m_bufSize - m_bufferedBytes;
		SceSize chunk = (remaining < space) ? remaining : space;
		sce_paf_memcpy(m_buffers[m_activeIndex] + m_bufferedBytes, cursor, chunk);
		cursor += chunk;
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
		sce_paf_memcpy(m_buffers[m_activeIndex], cursor, remaining);
		m_bufferedBytes = remaining;
		return 0;
	}

	int waitRet = WaitPendingWrite();
	if (waitRet != 0)
		return waitRet;

	m_pendingBytes = remaining;
	m_pendingAsync = true;
	int writeRet = m_file.WriteAsync(cursor, remaining);
	m_totalWritten += remaining;
	if (writeRet < 0)
		return writeRet;
	return WaitPendingWrite();
}

int PictureFileStream::Flush()
{
	return m_opened ? FlushBuffer() : PIC_CREATOR_ERROR_INVALID_STATE;
}

int PictureFileStream::Rewind()
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

uint32_t PictureFileStream::GetWrittenSize() const
{
	return m_opened ? (m_totalWritten + m_bufferedBytes) : 0;
}

int PictureFileStream::Close()
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

int PictureFileStream::CopyTo(PictureOutputStream *dst)
{
	if (!m_opened)
		return PIC_CREATOR_ERROR_INVALID_STATE;
	if (dst == NULL || !dst->IsOpen())
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	int ret = WaitPendingWrite();
	if (ret != 0)
		return ret;

	uint8_t *copyBuf = static_cast<uint8_t *>(
		ScePafStdc_3CF19248(1024, s_pictureWorkHeap));
	if (copyBuf == NULL)
		return PIC_CREATOR_ERROR_OUT_OF_MEMORY;

	m_pendingAsync = false;
	m_file.Seek(0, 0);
	for (;;) {
		m_pendingAsync = false;
		int bytesRead = m_file.Read(copyBuf, 1024);
		if (bytesRead < 0) {
			ret = bytesRead;
			break;
		}
		if (bytesRead > 0) {
			ret = dst->Write(copyBuf, static_cast<SceSize>(bytesRead));
			if (ret != 0)
				break;
		}
		if (bytesRead < 1024) {
			ret = 0;
			break;
		}
	}

	m_pendingAsync = false;
	m_file.Seek(0, 0);
	operator delete(copyBuf);
	return ret;
}

/* 0x81368BDE */
int PictureFileStream::RemoveFile(const char *path)
{
	if (path == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;
	return LocalFile::RemoveFile(path);
}
