/*
 * system_update_mtphttpfile.cpp: a paf::File that reads a file the PC serves over the MTP
 * connection (0x81134E5C-0x8113566C): opened (retried while the connection is busy), read
 * from where it stands, sought by opening it again at the new place, and its size and date.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <datetime/datetime.h>
#include <file/file.h>
#include <thread/thread.h>

#include "system_update_mtphttpfile.h"

using namespace paf;

#define MTP_HTTP_URL_MAX		1024	/* PLACEHOLDER names */
#define MTP_HTTP_OPEN_RETRY_MS		10
#define MTP_HTTP_OPEN_WAIT_MS		1000

#define MTP_ERROR_BUSY			((int)0x80105F04)	/* PLACEHOLDER names */
#define MTP_ERROR_NOT_STARTED		((int)0x80105F01)
#define MTP_ERROR_KIND1_FILE		((int)0x805400FD)	/* the file is not a kind 1 one */
#define MTP_ERROR_KIND2_FILE		((int)0x805400FE)

MtpHttpFile::MtpHttpFile() : m_lock("MtpHttpFileLock", 0)
{
	m_offset = 0;
	m_error = 0;
	m_state = MTP_HTTP_FILE_CLOSED;
	m_client = -1;
	sce_paf_memset(&m_request, 0, sizeof(m_request));
	sce_paf_memset(&m_info, 0, sizeof(m_info));
	m_aborted = false;
}

int32_t MtpHttpFile::GetFileType() const
{
	return MTP_HTTP_FILE_TYPE;
}

int32_t MtpHttpFile::GetCapability() const
{
	return MTP_HTTP_FILE_CAPABILITY;
}

int64_t MtpHttpFile::GetFileSize() const
{
	if (IsOpened() == false)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	return m_info.size;
}

/* an MTP error that says the file is not of the kind asked for, as Open gives it */
static inline int mapError(int kind, int error, int *saved)
{
	if (kind == 1 && (error == (int)0x80540020 || error == (int)0x80540029 ||
			  error == (int)0x8054002B || error == (int)0x80540027 ||
			  error == (int)0x80540021)) {
		*saved = error;
		return MTP_ERROR_KIND1_FILE;
	}
	if (kind == 2 && (error == (int)0x8054001F || error == (int)0x80540028 ||
			  error == (int)0x8054002A || error == (int)0x80540026 ||
			  error == (int)0x80540022)) {
		*saved = error;
		return MTP_ERROR_KIND2_FILE;
	}
	*saved = 0;
	return error;
}

/* the request made */
inline int MtpHttpFile::connect(void)
{
	if (IsOpened() != false)
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	if (m_state == MTP_HTTP_FILE_OPENED)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;

	int ret = MTPR2StartDownloadStep1(&m_request, (unsigned int *)&m_client);

	if (ret < 0)
		return ret;
	m_state = MTP_HTTP_FILE_OPENING;
	return 0;
}

/* the file's size, date and kind taken from the request */
inline int MtpHttpFile::fetchInfo(void)
{
	m_lock.Lock();
	if (m_state != MTP_HTTP_FILE_OPENING) {
		m_lock.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	}

	int ret = MTPR2StartDownloadStep2(m_client, &m_info);

	if (ret < 0) {
		m_lock.Unlock();
		return ret;
	}
	m_state = MTP_HTTP_FILE_OPENED;
	m_lock.Unlock();
	return 0;
}

int MtpHttpFile::Bgdl_81134E8A(const MtpHttpFileOpenArg *param)
{
	m_lock.Lock();
	if (param == NULL || GetFileType() != (int32_t)param->file_type || param->url == NULL ||
	    sce_paf_strlen(param->url) > MTP_HTTP_URL_MAX) {
		m_lock.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	m_request.url = param->url;
	m_request.offset = param->offset;
	m_request.ringBufSize = param->bufferSize;
	m_request.optionFlags = param->options[0];
	m_request.optionType = param->options[1];
	m_request.optionValue = param->options[2];

	int ret = connect();

	if (ret < 0) {
		ret = mapError(param->kind, ret, &m_error);
		m_lock.Unlock();
		return ret;
	}

	ret = fetchInfo();
	if (ret >= 0) {
		int error;

		if (param->kind == 1 && m_info.kind != 1) {
			error = MTP_ERROR_KIND1_FILE;
		} else if (param->kind == 2 && m_info.kind != 2) {
			error = MTP_ERROR_KIND2_FILE;
		} else {
			m_lock.Unlock();
			return 0;
		}
		Abort();
		m_state = MTP_HTTP_FILE_CLOSED;
		m_error = 0;
		m_lock.Unlock();
		return error;
	}
	Abort();
	m_state = MTP_HTTP_FILE_CLOSED;
	ret = mapError(param->kind, ret, &m_error);
	m_lock.Unlock();
	return ret;
}

int32_t MtpHttpFile::Open(const OpenArg *param)
{
	SceInt64 waited = 0;
	int ret;

	do {
		ret = Bgdl_81134E8A((const MtpHttpFileOpenArg *)param);
		if (ret == MTP_ERROR_BUSY) {
			thread::Sleep(MTP_HTTP_OPEN_RETRY_MS);
			waited += MTP_HTTP_OPEN_RETRY_MS;
		}
	} while (ret == MTP_ERROR_BUSY && m_aborted == false && waited < MTP_HTTP_OPEN_WAIT_MS);
	if (ret != 0)
		Close();
	return ret;
}

int32_t MtpHttpFile::Close()
{
	Abort();
	MTPR2EndDownload(m_client, NULL);
	m_lock.Lock();
	if (IsOpened() != false) {
		m_state = MTP_HTTP_FILE_CLOSED;
		m_lock.Unlock();
		return 0;
	}
	m_lock.Unlock();
	return SCE_PAF_ERROR_FILE_NOT_OPENED;
}

bool MtpHttpFile::IsOpened() const
{
	return m_state != MTP_HTTP_FILE_CLOSED;
}

bool MtpHttpFile::IsEof()
{
	return m_offset == m_info.size;
}

int32_t MtpHttpFile::Abort()
{
	MTPR2AbortDownload();
	m_aborted = true;
	return 0;
}

int32_t MtpHttpFile::Read(void *buf, size_t nbyte)
{
	if (buf == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	m_lock.Lock();
	if (m_state != MTP_HTTP_FILE_OPENED) {
		m_lock.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	if (nbyte + m_offset > m_info.size)
		nbyte = m_info.size - m_offset;
	if (nbyte == 0) {
		m_lock.Unlock();
		return 0;
	}

	char *dst = (char *)buf;
	size_t done = 0;
	int n;

	do {
		n = 0;

		int ret = MTPR2Read(m_client, dst + done, nbyte - done, m_offset, (unsigned int *)&n);

		if (ret < 0) {
			m_lock.Unlock();
			return ret;
		}
		done += n;
		m_offset += n;
	} while (done < nbyte && n != 0);
	m_lock.Unlock();
	return done;
}

int32_t MtpHttpFile::Write(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t MtpHttpFile::WriteAsync(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

/* a new place: the file opened again there */
int64_t MtpHttpFile::Seek(int64_t offset, int32_t whence)
{
	if (IsOpened() == false)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	m_lock.Lock();

	SceInt64 pos;

	switch (whence) {
	case 0:
		pos = offset;
		break;
	case 1:
		pos = m_offset + offset;
		break;
	case 2:
		pos = m_info.size + offset;
		break;
	default:
		m_lock.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (pos != m_offset) {
		int ret = MTPR2AbortDownload();

		if ((ret < 0 && ret != MTP_ERROR_NOT_STARTED) ||
		    (ret = MTPR2EndDownload(m_client, NULL)) < 0) {
			m_lock.Unlock();
			return ret;
		}
		m_state = MTP_HTTP_FILE_CLOSED;
		m_request.offset = pos;
		ret = connect();
		if (ret < 0) {
			m_lock.Unlock();
			return ret;
		}
		ret = fetchInfo();
		if (ret < 0) {
			m_lock.Unlock();
			return ret;
		}
	}
	m_lock.Unlock();
	return pos;
}

int32_t MtpHttpFile::Flush()
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t MtpHttpFile::FlushAsync()
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t MtpHttpFile::Getstat(FileStat *stat)
{
	if (stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	sce_paf_memset(stat, 0, sizeof(*stat));
	if (&stat->st_mtime == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	if (IsOpened() == false)
		return SCE_PAF_ERROR_FILE_NOT_OPENED;

	rtc::Tick tick = m_info.time;

	stat->st_mtime = datetime::DateTime::ConvertRtcTickToDateTime(&tick);
	stat->st_size = m_info.size;
	stat->st_mode = MTP_HTTP_FILE_TYPE;
	return 0;
}

MtpHttpFile::~MtpHttpFile()
{
}
