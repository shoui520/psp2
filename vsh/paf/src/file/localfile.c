#include "localfile.h"
#include "../paf_types.h"

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

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
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
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileOpenAsync(LocalFile *self, const FileOpenArg *param)
{
	const LocalFileOpenParam *local = (const LocalFileOpenParam *)param;
	int32_t result;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
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
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileClose(LocalFile *self)
{
	int32_t result;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base)) {
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (!self->m_pimpl) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
	} else {
		LocalFileImpl *impl = self->m_pimpl;

		result = impl->vtable->Close(impl);
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return result;
}

int32_t LocalFileCloseAsync(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->CloseAsync(impl);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

unsigned char LocalFileIsOpened(const LocalFile *self)
{
	LocalFileImpl *impl;
	unsigned char ret = 0;

	ScePafThread_F9FFA0BE((RMutex *)&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->IsOpened(impl);
	ScePafThread_95F9C45F((RMutex *)&self->m_lock_mutex);
	return ret;
}

unsigned char LocalFileIsEof(LocalFile *self)
{
	LocalFileImpl *impl;
	unsigned char ret = 0;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->IsEof(impl);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileAbort(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Abort(impl);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileRead(LocalFile *self, void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Read(impl, buf, nbyte);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileReadAsync(LocalFile *self, void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->ReadAsync(impl, buf, nbyte);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWrite(LocalFile *self, const void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Write(impl, buf, nbyte);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWriteAsync(LocalFile *self, const void *buf, size_t nbyte)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->WriteAsync(impl, buf, nbyte);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int64_t LocalFileSeek(LocalFile *self, int64_t offset, int32_t whence)
{
	LocalFileImpl *impl;
	int64_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Seek(impl, offset, whence);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileSeekAsync(LocalFile *self, int64_t offset, int32_t whence)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->SeekAsync(impl, offset, whence);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileFlush(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Flush(impl);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileFlushAsync(LocalFile *self)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->FlushAsync(impl);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileWaitAsync(LocalFile *self, int64_t *result)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->WaitAsync(impl, result);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFilePollAsync(LocalFile *self, int64_t *result)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->PollAsync(impl, result);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileGetstat(LocalFile *self, FileStat *stat)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->Getstat(impl, stat);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t LocalFileSetPriority(LocalFile *self, int32_t priority)
{
	LocalFileImpl *impl;
	int32_t ret = SCE_PAF_ERROR_FILE_NO_MEMORY;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	impl = self->m_pimpl;
	if (impl)
		ret = impl->vtable->SetPriority(impl, priority);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}
