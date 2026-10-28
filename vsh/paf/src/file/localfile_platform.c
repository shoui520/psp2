#include "localfile_platform.h"
#include "error.h"
#include "../paf_types.h"
#include <kernel/threadmgr.h>

extern void ScePafStdc_2730262C(void); /* __cxa_pure_virtual */

static const LocalFileImplClass localFileImplClass = {
	0, NULL, {
		LocalFileImplDestroy, LocalFileImplDelete,
		(int32_t (*)(LocalFileImpl *, const char *, uint32_t, uint32_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, void *, size_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, const void *, size_t))ScePafStdc_2730262C,
		(int64_t (*)(LocalFileImpl *, int64_t, int32_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, const char *, uint32_t, uint32_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, void *, size_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, const void *, size_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, int64_t, int32_t))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, int64_t *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, int64_t *))ScePafStdc_2730262C,
		(unsigned char (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(unsigned char (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int64_t (*)(LocalFileImpl *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, FileStat *))ScePafStdc_2730262C,
		(int32_t (*)(LocalFileImpl *, int32_t))ScePafStdc_2730262C
	}
};

const LocalFileImplClass localFilePsp2ImplClass = {
	0, NULL, {
		(LocalFileImpl *(*)(LocalFileImpl *))LocalFilePsp2ImplDestroy,
		(void (*)(LocalFileImpl *))LocalFilePsp2ImplDelete,
		(int32_t (*)(LocalFileImpl *, const char *, uint32_t, uint32_t))LocalFilePsp2ImplOpen,
		(int32_t (*)(LocalFileImpl *))LocalFilePsp2ImplClose,
		(int32_t (*)(LocalFileImpl *, void *, size_t))LocalFilePsp2ImplRead,
		(int32_t (*)(LocalFileImpl *, const void *, size_t))LocalFilePsp2ImplWrite,
		(int64_t (*)(LocalFileImpl *, int64_t, int32_t))LocalFilePsp2ImplSeek,
		(int32_t (*)(LocalFileImpl *))LocalFilePsp2ImplFlush,
		(int32_t (*)(LocalFileImpl *, const char *, uint32_t, uint32_t))LocalFilePsp2ImplOpenAsync,
		(int32_t (*)(LocalFileImpl *))LocalFilePsp2ImplCloseAsync,
		(int32_t (*)(LocalFileImpl *, void *, size_t))LocalFilePsp2ImplReadAsync,
		(int32_t (*)(LocalFileImpl *, const void *, size_t))LocalFilePsp2ImplWriteAsync,
		(int32_t (*)(LocalFileImpl *, int64_t, int32_t))LocalFilePsp2ImplSeekAsync,
		(int32_t (*)(LocalFileImpl *))LocalFilePsp2ImplFlushAsync,
		(int32_t (*)(LocalFileImpl *, int64_t *))LocalFilePsp2ImplWaitAsync,
		(int32_t (*)(LocalFileImpl *, int64_t *))LocalFilePsp2ImplPollAsync,
		(unsigned char (*)(LocalFileImpl *))LocalFilePsp2ImplIsOpened,
		(unsigned char (*)(LocalFileImpl *))LocalFilePsp2ImplIsEof,
		(int32_t (*)(LocalFileImpl *))LocalFilePsp2ImplAbort,
		(int64_t (*)(LocalFileImpl *))LocalFilePsp2ImplGetFileSize,
		(int32_t (*)(LocalFileImpl *, FileStat *))LocalFilePsp2ImplGetstat,
		(int32_t (*)(LocalFileImpl *, int32_t))LocalFilePsp2ImplSetPriority
	}
};

LocalFilePsp2Impl *LocalFilePsp2ImplConstruct(LocalFilePsp2Impl *self)
{
	self->base.vtable = &localFilePsp2ImplClass.methods;
	self->m_fd = -1;
	self->m_is_opened = 0;
	self->m_async_type = TYPE_NONE;
	sce_paf_memset(&self->m_async, 0, sizeof(self->m_async));
	self->m_as_result = -1;
	return self;
}

LocalFilePsp2Impl *LocalFilePsp2ImplConstructBase(LocalFilePsp2Impl *self)
{
	self->base.vtable = &localFilePsp2ImplClass.methods;
	self->m_fd = -1;
	self->m_is_opened = 0;
	self->m_async_type = TYPE_NONE;
	sce_paf_memset(&self->m_async, 0, sizeof(self->m_async));
	self->m_as_result = -1;
	return self;
}

LocalFilePsp2Impl *LocalFilePsp2ImplDestroy(LocalFilePsp2Impl *self)
{
	self->base.vtable = &localFileImplClass.methods;
	return self;
}

void LocalFilePsp2ImplDelete(LocalFilePsp2Impl *self)
{
	self->base.vtable = &localFileImplClass.methods;
	ScePafStdc_1B77082E(self);
}

LocalFilePsp2Impl *LocalFilePsp2ImplDestroyBase(LocalFilePsp2Impl *self)
{
	self->base.vtable = &localFileImplClass.methods;
	return self;
}

int32_t LocalFilePsp2ImplOpen(LocalFilePsp2Impl *self, const char *filename,
			   uint32_t flag, uint32_t mode)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoOpen(filename, flag, mode);
	if (ret < 0)
		ret = ToPafError(ret);
	if (ret >= 0) {
		self->m_fd = ret;
		self->m_is_opened = 1;
		self->m_async_type = TYPE_NONE;
		return 0;
	}
	return ret;
}

int32_t LocalFilePsp2ImplClose(LocalFilePsp2Impl *self)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoClose(self->m_fd);
	if (ret < 0)
		ret = ToPafError(ret);
	if (!ret) {
		self->m_is_opened = 0;
		self->m_fd = -1;
		self->m_async_type = TYPE_NONE;
	}
	return ret;
}

int32_t LocalFilePsp2ImplRead(LocalFilePsp2Impl *self, void *buf, size_t nbyte)
{
	char *cursor = buf;
	SceUID fd;
	uint32_t total = 0;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	fd = self->m_fd;
	while (nbyte) {
		int32_t ret = sceIoRead(fd, cursor, nbyte);

		if (ret < 0) {
			ret = ToPafError(ret);
			if (ret < 0)
				return ret;
		}
		if (!ret)
			break;
		nbyte -= ret;
		total += (uint32_t)ret;
		cursor += ret;
	}
	return (int32_t)total;
}

int32_t LocalFilePsp2ImplWrite(LocalFilePsp2Impl *self, const void *buf, size_t nbyte)
{
	const char *cursor = buf;
	SceUID fd;
	uint32_t total = 0;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	fd = self->m_fd;
	while (nbyte) {
		int32_t ret = sceIoWrite(fd, cursor, nbyte);

		if (ret < 0) {
			ret = ToPafError(ret);
			if (ret < 0)
				return ret;
		}
		if (!ret)
			break;
		nbyte -= ret;
		total += (uint32_t)ret;
		cursor += ret;
	}
	return (int32_t)total;
}

int64_t LocalFilePsp2ImplSeek(LocalFilePsp2Impl *self, int64_t offset, int32_t whence)
{
	int64_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoLseek(self->m_fd, offset, whence);
	if (ret < 0)
		return ToPafError((int32_t)ret);
	return ret;
}

int32_t LocalFilePsp2ImplFlush(LocalFilePsp2Impl *self)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoSyncByFd(self->m_fd, 0);
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2ImplOpenAsync(LocalFilePsp2Impl *self, const char *filename, uint32_t flag, uint32_t mode)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoOpenAsync(filename, flag, mode, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_OPEN;
	return ret;
}

int32_t LocalFilePsp2ImplCloseAsync(LocalFilePsp2Impl *self)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoCloseAsync(self->m_fd, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_CLOSE;
	return ret;
}

int32_t LocalFilePsp2ImplReadAsync(LocalFilePsp2Impl *self, void *buf, size_t nbyte)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoReadAsync(self->m_fd, buf, nbyte, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_READ;
	return ret;
}

int32_t LocalFilePsp2ImplWriteAsync(LocalFilePsp2Impl *self, const void *buf, size_t nbyte)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoWriteAsync(self->m_fd, buf, nbyte, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_WRITE;
	return ret;
}

int32_t LocalFilePsp2ImplSeekAsync(LocalFilePsp2Impl *self, int64_t offset, int32_t whence)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoLseekAsync(self->m_fd, offset, whence, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_SEEK;
	return ret;
}

int32_t LocalFilePsp2ImplFlushAsync(LocalFilePsp2Impl *self)
{
	int32_t ret;

	if (self->m_async_type != TYPE_NONE)
		return SCE_PAF_ERROR_FILE_BUSY;
	ret = sceIoSyncByFdAsync(self->m_fd, 0, &self->m_async);
	if (ret < 0) {
		ret = ToPafError(ret);
		if (ret)
			return ret;
	} else {
		self->m_as_result = ret;
		ret = 0;
	}
	self->m_async_type = TYPE_FLUSH;
	return ret;
}

int32_t LocalFilePsp2ImplWaitAsync(LocalFilePsp2Impl *self, int64_t *result)
{
	int64_t value = -1;
	SceUID event;
	SceUInt32 pattern = 0;
	int32_t ret;

	if (self->m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	event = self->m_as_result;
	ret = sceKernelWaitEvent(event, 1, &pattern, NULL, NULL);
	if (ret) {
		if (ret < 0)
			ret = ToPafError(ret);
	} else {
		value = self->m_async.result;
		sceIoComplete(event);
	}
	if (!ret) {
		int32_t type = (int32_t)self->m_async_type;

		if (type > TYPE_OPEN) {
			if (type <= TYPE_CLOSE && value == 0) {
				self->m_fd = -1;
				self->m_is_opened = 0;
			}
		} else if (type > TYPE_NONE && value >= 0) {
			self->m_is_opened = 1;
			self->m_fd = (int32_t)value;
			value = 0;
		}
		if (result)
			*result = value;
		self->m_async_type = TYPE_NONE;
	}
	return ret;
}

int32_t LocalFilePsp2ImplPollAsync(LocalFilePsp2Impl *self, int64_t *result)
{
	int64_t value = -1;
	SceUID event;
	SceUInt32 pattern = 0;
	int32_t ret;

	if (self->m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	event = self->m_as_result;
	ret = sceKernelPollEvent(event, 1, &pattern, NULL);
	if (ret) {
		if (ret < 0)
			ret = ToPafError(ret);
	} else {
		value = self->m_async.result;
		sceIoComplete(event);
	}
	if (!ret) {
		int32_t type = (int32_t)self->m_async_type;

		if (type > TYPE_OPEN) {
			if (type <= TYPE_CLOSE && value == 0) {
				self->m_fd = -1;
				self->m_is_opened = 0;
			}
		} else if (type > TYPE_NONE && value >= 0) {
			self->m_is_opened = 1;
			self->m_fd = (int32_t)value;
		}
		if (result)
			*result = value;
		self->m_async_type = TYPE_NONE;
	}
	return ret;
}

unsigned char LocalFilePsp2ImplIsEof(LocalFilePsp2Impl *self)
{
	int64_t position, end;

	position = self->base.vtable->Seek(&self->base, 0, SCE_SEEK_CUR);
	end = self->base.vtable->Seek(&self->base, 0, SCE_SEEK_END);
	self->base.vtable->Seek(&self->base, position, SCE_SEEK_SET);
	return position == end;
}

int32_t LocalFilePsp2ImplAbort(LocalFilePsp2Impl *self)
{
	int32_t ret;

	if (self->m_async_type == TYPE_NONE)
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	ret = sceIoCancel(self->m_as_result);
	if (ret < 0)
		ret = ToPafError(ret);
	if (!ret)
		self->m_async_type = TYPE_NONE;
	else if (ret == SCE_PAF_ERROR_FILE_BUSY)
		return self->base.vtable->WaitAsync(&self->base, NULL);
	return ret;
}

int64_t LocalFilePsp2ImplGetFileSize(LocalFilePsp2Impl *self)
{
	SceIoStat stat;
	int32_t ret = sceIoGetstatByFd(self->m_fd, &stat);

	if (ret < 0)
		return ToPafError(ret);
	return stat.st_size;
}

int32_t LocalFilePsp2ImplSetPriority(LocalFilePsp2Impl *self, int32_t priority)
{
	int32_t ret = sceIoSetPriorityForSystem(self->m_fd, priority);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

LocalFileImpl *LocalFileImplDestroy(LocalFileImpl *self)
{
	self->vtable = &localFileImplClass.methods;
	return self;
}

void LocalFileImplDelete(LocalFileImpl *self)
{
	self->vtable = &localFileImplClass.methods;
	ScePafStdc_1B77082E(self);
}

unsigned char LocalFilePsp2ImplIsOpened(LocalFilePsp2Impl *self)
{
	return self->m_is_opened;
}

int32_t LocalFilePsp2ImplGetstat(LocalFilePsp2Impl *self, FileStat *stat)
{
	SceIoStat native;
	int32_t ret;

	if (!stat)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	ret = sceIoGetstatByFd(self->m_fd, &native);
	if (!ret) {
		stat->st_mode = (uint16_t)native.st_mode;
		stat->st_attr = native.st_attr;
		stat->st_size = native.st_size;
		stat->st_ctime.m_year = native.st_ctime.year;
		stat->st_ctime.m_month = native.st_ctime.month;
		stat->st_ctime.m_day = native.st_ctime.day;
		stat->st_ctime.m_hour = native.st_ctime.hour;
		stat->st_ctime.m_minute = native.st_ctime.minute;
		stat->st_ctime.m_second = native.st_ctime.second;
		stat->st_ctime.m_microsecond = native.st_ctime.microsecond;
		stat->st_atime.m_year = native.st_atime.year;
		stat->st_atime.m_month = native.st_atime.month;
		stat->st_atime.m_day = native.st_atime.day;
		stat->st_atime.m_hour = native.st_atime.hour;
		stat->st_atime.m_minute = native.st_atime.minute;
		stat->st_atime.m_second = native.st_atime.second;
		stat->st_atime.m_microsecond = native.st_atime.microsecond;
		stat->st_mtime.m_year = native.st_mtime.year;
		stat->st_mtime.m_month = native.st_mtime.month;
		stat->st_mtime.m_day = native.st_mtime.day;
		stat->st_mtime.m_hour = native.st_mtime.hour;
		stat->st_mtime.m_minute = native.st_mtime.minute;
		stat->st_mtime.m_second = native.st_mtime.second;
		stat->st_mtime.m_microsecond = native.st_mtime.microsecond;
	}
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int64_t LocalFilePsp2ImplGetFileSizePath(const char *path)
{
	SceIoStat stat;
	int32_t ret = sceIoGetstat(path, &stat);

	if (ret < 0)
		return ToPafError(ret);
	return stat.st_size;
}

int32_t LocalFilePsp2ImplRemoveFile(const char *path)
{
	int32_t ret = sceIoRemove(path);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2ImplRenameFile(const char *oldname, const char *newname)
{
	int32_t ret = sceIoRename(oldname, newname);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2ImplGetstatPath(const char *path, LocalFileStat *stat)
{
	SceIoStat native;
	int32_t ret;

	if (!path || !stat)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	ret = sceIoGetstat(path, &native);
	if (!ret) {
		stat->st_mode = (uint16_t)native.st_mode;
		stat->st_attr = native.st_attr;
		stat->st_size = native.st_size;
		stat->st_ctime.m_year = native.st_ctime.year;
		stat->st_ctime.m_month = native.st_ctime.month;
		stat->st_ctime.m_day = native.st_ctime.day;
		stat->st_ctime.m_hour = native.st_ctime.hour;
		stat->st_ctime.m_minute = native.st_ctime.minute;
		stat->st_ctime.m_second = native.st_ctime.second;
		stat->st_ctime.m_microsecond = native.st_ctime.microsecond;
		stat->st_atime.m_year = native.st_atime.year;
		stat->st_atime.m_month = native.st_atime.month;
		stat->st_atime.m_day = native.st_atime.day;
		stat->st_atime.m_hour = native.st_atime.hour;
		stat->st_atime.m_minute = native.st_atime.minute;
		stat->st_atime.m_second = native.st_atime.second;
		stat->st_atime.m_microsecond = native.st_atime.microsecond;
		stat->st_mtime.m_year = native.st_mtime.year;
		stat->st_mtime.m_month = native.st_mtime.month;
		stat->st_mtime.m_day = native.st_mtime.day;
		stat->st_mtime.m_hour = native.st_mtime.hour;
		stat->st_mtime.m_minute = native.st_mtime.minute;
		stat->st_mtime.m_second = native.st_mtime.second;
		stat->st_mtime.m_microsecond = native.st_mtime.microsecond;
	}
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2ImplChstat(const char *path, const LocalFileStat *stat, uint32_t cbit)
{
	SceIoStat native;
	int32_t ret;

	if (!path || !stat)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	sce_paf_memset(&native, 0, sizeof(native));
	native.st_mode = stat->st_mode;
	native.st_attr = stat->st_attr;
	native.st_size = stat->st_size;
	native.st_ctime.year = stat->st_ctime.m_year;
	native.st_ctime.month = stat->st_ctime.m_month;
	native.st_ctime.day = stat->st_ctime.m_day;
	native.st_ctime.hour = stat->st_ctime.m_hour;
	native.st_ctime.minute = stat->st_ctime.m_minute;
	native.st_ctime.second = stat->st_ctime.m_second;
	native.st_ctime.microsecond = stat->st_ctime.m_microsecond;
	native.st_atime.year = stat->st_atime.m_year;
	native.st_atime.month = stat->st_atime.m_month;
	native.st_atime.day = stat->st_atime.m_day;
	native.st_atime.hour = stat->st_atime.m_hour;
	native.st_atime.minute = stat->st_atime.m_minute;
	native.st_atime.second = stat->st_atime.m_second;
	native.st_atime.microsecond = stat->st_atime.m_microsecond;
	native.st_mtime.year = stat->st_mtime.m_year;
	native.st_mtime.month = stat->st_mtime.m_month;
	native.st_mtime.day = stat->st_mtime.m_day;
	native.st_mtime.hour = stat->st_mtime.m_hour;
	native.st_mtime.minute = stat->st_mtime.m_minute;
	native.st_mtime.second = stat->st_mtime.m_second;
	native.st_mtime.microsecond = stat->st_mtime.m_microsecond;
	ret = sceIoChstat(path, &native, cbit);
	if (ret < 0)
		return ToPafError(ret);
	return ret;
}

int32_t LocalFilePsp2ImplSetDefaultPriority(int32_t priority)
{
	int32_t ret = sceIoSetThreadDefaultPriorityForSystem(priority);

	if (ret < 0)
		return ToPafError(ret);
	return ret;
}
