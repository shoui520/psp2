#include "localfile.h"
#include "localfile_platform.h"
#include "../std/new.h"
#include "../std/stdlib.h"
#include "../paf_types.h"

const FileClass localFileClass = {
	0, NULL, {
		(int32_t (*)(const File *))LocalFileGetFileType,
		(int32_t (*)(const File *))LocalFileGetCapability,
		(int64_t (*)(const File *))LocalFileGetFileSize,
		(int32_t (*)(File *, const FileOpenArg *))LocalFileOpenArg,
		(int32_t (*)(File *, const FileOpenArg *))LocalFileOpenAsync,
		(int32_t (*)(File *))LocalFileClose,
		(int32_t (*)(File *))LocalFileCloseAsync,
		(unsigned char (*)(const File *))LocalFileIsOpened,
		(unsigned char (*)(File *))LocalFileIsEof,
		(int32_t (*)(File *))LocalFileAbort,
		(int32_t (*)(File *, void *, size_t))LocalFileRead,
		(int32_t (*)(File *, void *, size_t))LocalFileReadAsync,
		(int32_t (*)(File *, const void *, size_t))LocalFileWrite,
		(int32_t (*)(File *, const void *, size_t))LocalFileWriteAsync,
		(int64_t (*)(File *, int64_t, int32_t))LocalFileSeek,
		(int32_t (*)(File *, int64_t, int32_t))LocalFileSeekAsync,
		(int32_t (*)(File *))LocalFileFlush,
		(int32_t (*)(File *))LocalFileFlushAsync,
		(int32_t (*)(File *, int64_t *))LocalFileWaitAsync,
		(int32_t (*)(File *, int64_t *))LocalFilePollAsync,
		(int32_t (*)(File *, FileStat *))LocalFileGetstat,
		(int32_t (*)(File *, int32_t))LocalFileSetPriority,
		(File *(*)(File *))LocalFileDestroy,
		(void (*)(File *))LocalFileDelete
	}
};

void LocalFileCreateImpl(LocalFile *self)
{
	if (self->m_allocator) {
		void *buffer = HeapAllocatorMemalign(self->m_allocator, 8,
							LocalFilePsp2ImplSize);

		if (buffer) {
			LocalFilePsp2Impl *impl = PafPlacementNew(LocalFilePsp2ImplSize, buffer);

			self->m_pimpl = impl ?
				(LocalFileImpl *)LocalFilePsp2ImplConstruct(impl) : NULL;
		}
	}
}

LocalFile *LocalFileConstruct(LocalFile *self)
{
	self->base.vtable = &localFileClass.methods;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafLocalFileCsLock", 0);
	self->m_pimpl = NULL;
	self->m_allocator = GetGlobalHeapAllocator();
	LocalFileCreateImpl(self);
	return self;
}

void LocalFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		   uint32_t mode, int32_t *error)
{
	LocalFile *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self) {
		LocalFileOpenParam param;
		int32_t ret;

		LocalFileConstruct(self);
		param.file_type = Type_Local;
		param.filename = path;
		param.flag = flag;
		param.mode = mode;
		ret = self->base.vtable->Open(&self->base, (const FileOpenArg *)&param);
		if (error)
			*error = ret;
		if (self->base.vtable->IsOpened(&self->base)) {
			FileRefCounter *counter;

			result->m_ptr = &self->base;
			counter = ScePafStdc_FC5CD359(sizeof(*counter));
			if (counter) {
				result->m_ref_counter = counter;
				counter->m_ptr = &self->base;
				result->m_ref_counter->m_counter = 1;
				result->m_ref_counter->m_weak_counter = 1;
				result->m_ref_counter->m_deleter = LocalFileDefaultDelete;
				return;
			}
		} else {
			self->base.vtable->Delete(&self->base);
		}
	} else if (error) {
		*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

int32_t LocalFileGetFileType(const LocalFile *self)
{
	return Type_Local;
}

int32_t LocalFileGetCapability(const LocalFile *self)
{
	return 0x7fff;
}

int64_t LocalFileGetFileSize(const LocalFile *self)
{
	LocalFileImpl *impl = self->m_pimpl;

	if (impl)
		return impl->vtable->GetFileSize(impl);
	return SCE_PAF_ERROR_FILE_NO_MEMORY;
}

int32_t LocalFileOpenArg(LocalFile *self, const FileOpenArg *param)
{
	const LocalFileOpenParam *local = (const LocalFileOpenParam *)param;
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!param || self->base.vtable->GetFileType(&self->base) != local->file_type) {
		result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	} else {
		const char *filename = local->filename;
		LocalFileImpl *impl = self->m_pimpl;
		uint32_t flag = local->flag;
		uint32_t mode = local->mode;

		if (impl)
			result = impl->vtable->Open(impl, filename, flag, mode);
		else
			result = SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileOpenAsync(LocalFile *self, const FileOpenArg *param)
{
	const LocalFileOpenParam *local = (const LocalFileOpenParam *)param;
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!param || self->base.vtable->GetFileType(&self->base) != local->file_type) {
		result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	} else {
		const char *filename = local->filename;
		uint32_t flag = local->flag;
		uint32_t mode = local->mode;

		if (self->base.vtable->IsOpened(&self->base)) {
			result = SCE_PAF_ERROR_FILE_ALREADY_OPENED;
		} else if (!self->m_pimpl) {
			result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		} else {
			LocalFileImpl *impl = self->m_pimpl;

			result = impl->vtable->OpenAsync(impl, filename, flag, mode);
		}
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileClose(LocalFile *self)
{
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base)) {
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (!self->m_pimpl) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
	} else {
		LocalFileImpl *impl = self->m_pimpl;

		result = impl->vtable->Close(impl);
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileCloseAsync(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->CloseAsync(impl);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

unsigned char LocalFileIsOpened(const LocalFile *self)
{
	LocalFileImpl *impl;
	unsigned char ret = 0;

	_ZN3paf6thread6RMutex4LockEv((RMutex *)&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->IsOpened(impl);
	_ZN3paf6thread6RMutex6UnlockEv((RMutex *)&self->m_lock_mutex);
	return ret;
}

unsigned char LocalFileIsEof(LocalFile *self)
{
	LocalFileImpl *impl;
	unsigned char ret = 0;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->IsEof(impl);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileAbort(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Abort(impl);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileRead(LocalFile *self, void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Read(impl, buf, nbyte);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileReadAsync(LocalFile *self, void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->ReadAsync(impl, buf, nbyte);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWrite(LocalFile *self, const void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Write(impl, buf, nbyte);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWriteAsync(LocalFile *self, const void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->WriteAsync(impl, buf, nbyte);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int64_t LocalFileSeek(LocalFile *self, int64_t offset, int32_t whence)
{
	LocalFileImpl *impl;
	int64_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Seek(impl, offset, whence);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileSeekAsync(LocalFile *self, int64_t offset, int32_t whence)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->SeekAsync(impl, offset, whence);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileFlush(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Flush(impl);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileFlushAsync(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->FlushAsync(impl);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWaitAsync(LocalFile *self, int64_t *result)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->WaitAsync(impl, result);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFilePollAsync(LocalFile *self, int64_t *result)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->PollAsync(impl, result);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileGetstat(LocalFile *self, FileStat *stat)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Getstat(impl, stat);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileSetPriority(LocalFile *self, int32_t priority)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->SetPriority(impl, priority);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

LocalFile *LocalFileConstructBase(LocalFile *self)
{
	self->base.vtable = &localFileClass.methods;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafLocalFileCsLock", 0);
	self->m_pimpl = NULL;
	self->m_allocator = GetGlobalHeapAllocator();
	LocalFileCreateImpl(self);
	return self;
}

LocalFile *LocalFileConstructWithAllocator(LocalFile *self, HeapAllocator *allocator)
{
	self->base.vtable = &localFileClass.methods;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafLocalFileCsLock", 0);
	self->m_allocator = allocator;
	self->m_pimpl = NULL;
	LocalFileCreateImpl(self);
	return self;
}

LocalFile *LocalFileConstructBaseWithAllocator(LocalFile *self, HeapAllocator *allocator)
{
	self->base.vtable = &localFileClass.methods;
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafLocalFileCsLock", 0);
	self->m_allocator = allocator;
	self->m_pimpl = NULL;
	LocalFileCreateImpl(self);
	return self;
}

void LocalFileDeleteImpl(LocalFile *self)
{
	LocalFileImpl *impl = self->m_pimpl;

	if (impl) {
		impl->vtable->Destroy(impl);
		if (self->m_allocator)
			HeapAllocatorFree(self->m_allocator, self->m_pimpl);
		self->m_pimpl = NULL;
	}
}

LocalFile *LocalFileDestroy(LocalFile *self)
{
	self->base.vtable = &localFileClass.methods;
	LocalFileWaitAsync(self, NULL);
	LocalFileClose(self);
	LocalFileDeleteImpl(self);
	_ZN3paf6thread6RMutexD1Ev(&self->m_lock_mutex);
	self->base.vtable = &fileClass.methods;
	return self;
}

void LocalFileDelete(LocalFile *self)
{
	LocalFileDestroy(self);
	ScePafStdc_1B77082E(self);
}

LocalFile *LocalFileDestroyBase(LocalFile *self)
{
	return LocalFileDestroy(self);
}

void LocalFileOpenUrl(FileSharedPtr *result, const char *path, uint32_t flag,
		      uint32_t mode, int32_t *error, unsigned char *ignored)
{
	if (!ignored || *ignored) {
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
	} else {
		LocalFileOpen(result, path, flag, mode, error);
	}
}

int32_t LocalFileGetstatPath(const char *path, LocalFileStat *stat)
{
	return LocalFilePsp2ImplGetstatPath(path, stat);
}

int64_t LocalFileGetFileSizePath(const char *path)
{
	return LocalFilePsp2ImplGetFileSizePath(path);
}

int32_t LocalFileRemoveFile(const char *path)
{
	return LocalFilePsp2ImplRemoveFile(path);
}

int32_t LocalFileRenameFile(const char *oldname, const char *newname)
{
	return LocalFilePsp2ImplRenameFile(oldname, newname);
}

int32_t LocalFileChstat(const char *path, const LocalFileStat *stat, uint32_t cbit)
{
	return LocalFilePsp2ImplChstat(path, stat, cbit);
}

static unsigned char FileHasMode(const char *path, uint16_t mode)
{
	FileStat stat;
	unsigned char found = 0;

	DateTimeConstruct(&stat.st_ctime, 0, 0, 0, 0, 0, 0, 0);
	DateTimeConstruct(&stat.st_atime, 0, 0, 0, 0, 0, 0, 0);
	DateTimeConstruct(&stat.st_mtime, 0, 0, 0, 0, 0, 0, 0);
	if (!LocalFilePsp2ImplGetstatPath(path, &stat))
		found = (stat.st_mode & mode) != 0;
	DateTimeDestroy(&stat.st_mtime);
	DateTimeDestroy(&stat.st_atime);
	DateTimeDestroy(&stat.st_ctime);
	return found;
}

unsigned char LocalFileExists(const char *path)
{
	return FileHasMode(path, SCE_STM_FREG | SCE_STM_FDIR);
}

unsigned char LocalFileIsFile(const char *path)
{
	return FileHasMode(path, SCE_STM_FREG);
}

unsigned char LocalFileIsDirectory(const char *path)
{
	return FileHasMode(path, SCE_STM_FDIR);
}

int32_t LocalFileSetDefaultPriority(int32_t priority)
{
	return LocalFilePsp2ImplSetDefaultPriority(priority);
}
