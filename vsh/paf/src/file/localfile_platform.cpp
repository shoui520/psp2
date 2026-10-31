#include <kernel/threadmgr.h>
#include <std/new>
#include <std/string.h>
#include "localfile_platform.h"

extern "C" {
#include "error.h"
}

namespace paf {

LocalFilePsp2Impl::LocalFilePsp2Impl()
{
	m_fd = -1;
	m_is_opened = false;
	m_async_type = TYPE_NONE;
	sce_paf_memset(&m_async, 0, sizeof(m_async));
	m_as_result = -1;
}

LocalFilePsp2Impl::~LocalFilePsp2Impl()
{
}

int32_t LocalFilePsp2Impl::Open(const char *filename, uint32_t flag, uint32_t mode)
{
	int32_t ret;

	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoOpen(filename, flag, mode);
	if (ret < 0)
		ret = ToPafError(ret);
	if (ret >= 0) {
		m_fd = ret;
		m_is_opened = true;
		m_async_type = TYPE_NONE;
		return 0;
	}
	return ret;
}

int32_t LocalFilePsp2Impl::Close()
{
	int32_t ret;

	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoClose(m_fd);
	if (ret < 0)
		ret = ToPafError(ret);
	if (ret == 0) {
		m_is_opened = false;
		m_fd = -1;
		m_async_type = TYPE_NONE;
	}
	return ret;
}

int32_t LocalFilePsp2Impl::Read(void *buf, size_t nbyte)
{
	char *cursor = (char *)buf;
	uint32_t total = 0;

	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	SceUID fd = m_fd;

	while (nbyte != 0) {
		int32_t ret = sceIoRead(fd, cursor, nbyte);

		if (ret < 0) {
			ret = ToPafError(ret);
			if (ret < 0)
				return ret;
		}
		if (ret == 0)
			break;
		nbyte -= ret;
		total += (uint32_t)ret;
		cursor += ret;
	}
	return (int32_t)total;
}

int32_t LocalFilePsp2Impl::Write(const void *buf, size_t nbyte)
{
	const char *cursor = (const char *)buf;
	uint32_t total = 0;

	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	SceUID fd = m_fd;

	while (nbyte != 0) {
		int32_t ret = sceIoWrite(fd, cursor, nbyte);

		if (ret < 0) {
			ret = ToPafError(ret);
			if (ret < 0)
				return ret;
		}
		if (ret == 0)
			break;
		nbyte -= ret;
		total += (uint32_t)ret;
		cursor += ret;
	}
	return (int32_t)total;
}

int64_t LocalFilePsp2Impl::Seek(int64_t offset, int32_t whence)
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int64_t ret = sceIoLseek(m_fd, offset, whence);

	if (ret < 0)
		return ToPafError((int32_t)ret);
	return ret;
}

int32_t LocalFilePsp2Impl::Flush()
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoSyncByFd(m_fd, 0);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2Impl::OpenAsync(const char *filename, uint32_t flag, uint32_t mode)
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoOpenAsync(filename, flag, mode, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_OPEN;
	return ret;
}

int32_t LocalFilePsp2Impl::CloseAsync()
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoCloseAsync(m_fd, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_CLOSE;
	return ret;
}

int32_t LocalFilePsp2Impl::ReadAsync(void *buf, size_t nbyte)
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoReadAsync(m_fd, buf, nbyte, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_READ;
	return ret;
}

int32_t LocalFilePsp2Impl::WriteAsync(const void *buf, size_t nbyte)
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoWriteAsync(m_fd, buf, nbyte, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_WRITE;
	return ret;
}

int32_t LocalFilePsp2Impl::SeekAsync(int64_t offset, int32_t whence)
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoLseekAsync(m_fd, offset, whence, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_SEEK;
	return ret;
}

int32_t LocalFilePsp2Impl::FlushAsync()
{
	if (m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;

	int32_t ret = sceIoSyncByFdAsync(m_fd, 0, &m_async);

	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret != 0)
			return ret;
	} else {
		m_as_result = ret;
		ret = 0;
	}
	m_async_type = TYPE_FLUSH;
	return ret;
}

int32_t LocalFilePsp2Impl::WaitAsync(int64_t *result)
{
	int64_t value = -1;
	SceUInt32 pattern = 0;

	if (m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;

	SceUID event = m_as_result;
	int32_t ret = sceKernelWaitEvent(event, 1, &pattern, NULL, NULL);

	if (ret != 0) {
		if (ret < 0)
			ret = ToPafError(ret);
	} else {
		value = m_async.result;
		sceIoComplete(event);
	}
	if (ret == 0) {
		int32_t type = (int32_t)m_async_type;

		if (type > TYPE_OPEN) {
			if (type <= TYPE_CLOSE && value == 0) {
				m_fd = -1;
				m_is_opened = false;
			}
		} else if (type > TYPE_NONE && value >= 0) {
			m_is_opened = true;
			m_fd = (SceUID)value;
			value = 0;
		}
		if (result != NULL)
			*result = value;
		m_async_type = TYPE_NONE;
	}
	return ret;
}

int32_t LocalFilePsp2Impl::PollAsync(int64_t *result)
{
	int64_t value = -1;
	SceUInt32 pattern = 0;

	if (m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;

	SceUID event = m_as_result;
	int32_t ret = sceKernelPollEvent(event, 1, &pattern, NULL);

	if (ret != 0) {
		if (ret < 0)
			ret = ToPafError(ret);
	} else {
		value = m_async.result;
		sceIoComplete(event);
	}
	if (ret == 0) {
		int32_t type = (int32_t)m_async_type;

		if (type > TYPE_OPEN) {
			if (type <= TYPE_CLOSE && value == 0) {
				m_fd = -1;
				m_is_opened = false;
			}
		} else if (type > TYPE_NONE && value >= 0) {
			m_is_opened = true;
			m_fd = (SceUID)value;
		}
		if (result != NULL)
			*result = value;
		m_async_type = TYPE_NONE;
	}
	return ret;
}

bool LocalFilePsp2Impl::IsEof()
{
	int64_t position = Seek(0, SCE_SEEK_CUR);
	int64_t end = Seek(0, SCE_SEEK_END);

	Seek(position, SCE_SEEK_SET);
	return position == end;
}

int32_t LocalFilePsp2Impl::Abort()
{
	if (m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;

	int32_t ret = sceIoCancel(m_as_result);

	if (ret < 0)
		ret = ToPafError(ret);
	if (ret == 0)
		m_async_type = TYPE_NONE;
	else if (ret == SCE_PAF_ERROR_FILE_BUSY)
		return WaitAsync(NULL);
	return ret;
}

int64_t LocalFilePsp2Impl::GetFileSize()
{
	SceIoStat stat;
	int32_t ret = sceIoGetstatByFd(m_fd, &stat);

	if (ret < 0)
		return ToPafError(ret);
	return stat.st_size;
}

/* A native time into paf's: field by field, the rest of the DateTime left as it was. */
static inline void ToDateTime(datetime::DateTime *dst, const SceDateTime *src)
{
	dst->m_year = src->year;
	dst->m_month = src->month;
	dst->m_day = src->day;
	dst->m_hour = src->hour;
	dst->m_minute = src->minute;
	dst->m_second = src->second;
	dst->m_microsecond = src->microsecond;
}

static inline void FromDateTime(SceDateTime *dst, const datetime::DateTime *src)
{
	dst->year = src->m_year;
	dst->month = src->m_month;
	dst->day = src->m_day;
	dst->hour = src->m_hour;
	dst->minute = src->m_minute;
	dst->second = src->m_second;
	dst->microsecond = src->m_microsecond;
}

int32_t LocalFilePsp2Impl::Getstat(FileStat *stat)
{
	SceIoStat native;

	if (stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	int32_t ret = sceIoGetstatByFd(m_fd, &native);

	if (ret == 0) {
		stat->st_mode = (uint16_t)native.st_mode;
		stat->st_attr = native.st_attr;
		stat->st_size = native.st_size;
		ToDateTime(&stat->st_ctime, &native.st_ctime);
		ToDateTime(&stat->st_atime, &native.st_atime);
		ToDateTime(&stat->st_mtime, &native.st_mtime);
	}
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2Impl::SetPriority(int32_t priority)
{
	int32_t ret = sceIoSetPriorityForSystem(m_fd, priority);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int64_t LocalFilePsp2Impl::GetFileSize(const char *path)
{
	SceIoStat stat;
	int32_t ret = sceIoGetstat(path, &stat);

	if (ret < 0)
		return ToPafError(ret);
	return stat.st_size;
}

int32_t LocalFilePsp2Impl::RemoveFile(const char *path)
{
	int32_t ret = sceIoRemove(path);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2Impl::RenameFile(const char *oldname, const char *newname)
{
	int32_t ret = sceIoRename(oldname, newname);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2Impl::Getstat(const char *path, LocalFileStat *stat)
{
	SceIoStat native;

	if (path == NULL || stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	int32_t ret = sceIoGetstat(path, &native);

	if (ret == 0) {
		stat->st_mode = (uint16_t)native.st_mode;
		stat->st_attr = native.st_attr;
		stat->st_size = native.st_size;
		ToDateTime(&stat->st_ctime, &native.st_ctime);
		ToDateTime(&stat->st_atime, &native.st_atime);
		ToDateTime(&stat->st_mtime, &native.st_mtime);
	}
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2Impl::Chstat(const char *path, LocalFileStat *stat, uint32_t cbit)
{
	SceIoStat native;

	if (path == NULL || stat == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	sce_paf_memset(&native, 0, sizeof(native));
	native.st_mode = stat->st_mode;
	native.st_attr = stat->st_attr;
	native.st_size = stat->st_size;
	FromDateTime(&native.st_ctime, &stat->st_ctime);
	FromDateTime(&native.st_atime, &stat->st_atime);
	FromDateTime(&native.st_mtime, &stat->st_mtime);

	int32_t ret = sceIoChstat(path, &native, cbit);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

}

int32_t ScePafMisc_A283AD47(int32_t priority)
{
	int32_t ret = sceIoSetThreadDefaultPriorityForSystem(priority);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}
