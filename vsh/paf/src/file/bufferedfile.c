#include "bufferedfile.h"
#include "../std/stdlib.h"
#include "../paf_types.h"
#include <kernel/iofilemgr.h>

int64_t BufferedFileGetFileSize(const BufferedFile *self)
{
	if (self->m_file.m_ptr)
		return self->m_state->m_file_size;
	return SCE_PAF_ERROR_FILE_PROTECTED;
}

int32_t BufferedFileGetFileType(const BufferedFile *self)
{
	return 17;
}

int32_t BufferedFileGetCapability(const BufferedFile *self)
{
	RMutex *mutex = (RMutex *)&self->lock_mutex;
	File *file;
	int32_t result = 63;

	ScePafThread_F9FFA0BE(mutex);
	file = self->m_file.m_ptr;
	if (file)
		result = file->vtable->GetCapability(file) & 63;
	ScePafThread_95F9C45F(mutex);
	return result;
}

int32_t BufferedFileOpenArg(BufferedFile *self, const FileOpenArg *param)
{
	const BufferedFileVTable *vtable = (const BufferedFileVTable *)self->base.vtable;
	BufferSharedPtr buffer = { NULL, NULL };
	int32_t result;

	result = vtable->Open(self, param, &buffer);
	BufferSharedPtrRelease(&buffer);
	return result;
}

int32_t BufferedFileOpenAsync(BufferedFile *self, const FileOpenArg *param)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileWriteBuffer(FileSharedPtr *file, BufferedFileState *state)
{
	unsigned char *addr;
	size_t remaining;
	int32_t result;

	if (!file->m_ptr)
		return 0;
	if (state->m_count) {
		if (file->m_ptr->vtable->Seek(file->m_ptr, state->m_buffer_start,
					     SCE_SEEK_SET) !=
		    state->m_buffer_start)
			return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		remaining = state->m_count;
		addr = state->m_addr;
		while (remaining) {
			result = file->m_ptr->vtable->Write(file->m_ptr, addr, remaining);
			if (result < 0)
				return result;
			if (!result || (uint32_t)result > remaining)
				return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
			remaining -= result;
			addr += result;
		}
		state->m_file_position = state->m_buffer_start + state->m_count;
	}
	state->m_dirty = 0;
	return 0;
}

int32_t BufferedFileFlushDirty(FileSharedPtr *file, BufferedFileState *state)
{
	FileSharedPtr copy;
	int32_t result;

	if (!state->m_dirty)
		return 0;
	FileSharedPtrCopy(&copy, file);
	result = BufferedFileWriteBuffer(&copy, state);
	FileSharedPtrRelease(&copy);
	return result;
}

void BufferedFileResetBuffer(FileSharedPtr *file, BufferedFileState *state)
{
	FileSharedPtr copy;
	File *ptr;
	const FileVTable *vtable;
	int64_t position;
	int64_t size;

	FileSharedPtrCopy(&copy, file);
	BufferedFileFlushDirty(&copy, state);
	FileSharedPtrRelease(&copy);
	state->m_count = 0;
	state->m_buffer_start = 0;
	state->m_position = 0;
	state->m_file_position = 0;
	state->m_file_size = 0;
	if (file->m_ptr) {
		position = file->m_ptr->vtable->Seek(file->m_ptr, 0, SCE_SEEK_CUR);
		ptr = file->m_ptr;
		vtable = ptr->vtable;
		if (position >= 0) {
			state->m_buffer_start = position;
			state->m_position = position;
			state->m_file_position = position;
		}
		size = vtable->GetFileSize(ptr);
		if (size >= 0)
			state->m_file_size = size;
	}
}

int32_t BufferedFileClose(BufferedFile *self)
{
	FileSharedPtr copy;
	FileSharedPtr empty;
	BufferedFileState *state;
	int32_t result = SCE_PAF_ERROR_FILE_PROTECTED;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	state = self->m_state;
	if (self->m_file.m_ptr) {
		FileSharedPtrCopy(&copy, &self->m_file);
		BufferedFileFlushDirty(&copy, state);
		FileSharedPtrRelease(&copy);
		result = self->m_file.m_ptr->vtable->Close(self->m_file.m_ptr);
	}
	empty.m_ptr = NULL;
	empty.m_ref_counter = NULL;
	BufferedFileResetBuffer(&empty, state);
	FileSharedPtrRelease(&empty);
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
}

int32_t BufferedFileCloseAsync(BufferedFile *self)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

unsigned char BufferedFileIsOpened(const BufferedFile *self)
{
	RMutex *mutex = (RMutex *)&self->lock_mutex;
	File *file;
	unsigned char result;

	ScePafThread_F9FFA0BE(mutex);
	file = self->m_file.m_ptr;
	if (file) {
		result = file->vtable->IsOpened(file);
		ScePafThread_95F9C45F(mutex);
		return result;
	}
	ScePafThread_95F9C45F(mutex);
	return 0;
}

unsigned char BufferedFileIsEof(BufferedFile *self)
{
	FileSharedPtr copy;
	BufferedFileState *state;
	unsigned char result;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (self->m_file.m_ptr) {
		state = self->m_state;
		if (state->m_position == state->m_file_position) {
			result = self->m_file.m_ptr->vtable->IsEof(self->m_file.m_ptr);
		} else if (state->m_position <= state->m_file_position) {
			result = 0;
		} else {
			FileSharedPtrCopy(&copy, &self->m_file);
			BufferedFileResetBuffer(&copy, state);
			FileSharedPtrRelease(&copy);
			result = self->m_file.m_ptr->vtable->IsEof(self->m_file.m_ptr);
		}
		ScePafThread_95F9C45F(&self->lock_mutex);
		return result;
	}
	ScePafThread_95F9C45F(&self->lock_mutex);
	return 1;
}

int32_t BufferedFileAbort(BufferedFile *self)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileReadAsync(BufferedFile *self, void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileWriteAsync(BufferedFile *self, const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileSeekAsync(BufferedFile *self, int64_t offset, int32_t whence)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileFlush(BufferedFile *self)
{
	FileSharedPtr copy;
	BufferedFileState *state;
	int32_t result;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (!self->m_file.m_ptr) {
		ScePafThread_95F9C45F(&self->lock_mutex);
		return SCE_PAF_ERROR_FILE_PROTECTED;
	}
	if (!self->m_file.m_ptr->vtable->IsOpened(self->m_file.m_ptr)) {
		ScePafThread_95F9C45F(&self->lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	state = self->m_state;
	FileSharedPtrCopy(&copy, &self->m_file);
	result = BufferedFileFlushDirty(&copy, state);
	FileSharedPtrRelease(&copy);
	self->m_file.m_ptr->vtable->Flush(self->m_file.m_ptr);
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
}

int32_t BufferedFileFlushAsync(BufferedFile *self)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileWaitAsync(BufferedFile *self, int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFilePollAsync(BufferedFile *self, int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

void BufferedFileCreateDefaultBuffer(BufferSharedPtr *result)
{
	BufferSharedPtr buffer;
	void *addr;

	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
	addr = sce_paf_memalign(64, 4096);
	if (addr) {
		HeapAllocBufferCreate(&buffer, addr, 4096, NULL);
		BufferSharedPtrAssign(result, &buffer);
		BufferSharedPtrRelease(&buffer);
	}
}
