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

int32_t BufferedFileRead(BufferedFile *self, void *buf, size_t nbyte)
{
	FileSharedPtr file;
	BufferedFileState *state;
	void *src;
	size_t available;
	size_t copied = 0;
	size_t received;
	int64_t offset;
	int64_t remaining;
	int64_t position;
	int32_t result;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (!buf) {
		result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto out;
	}
	if (!self->m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->m_file.m_ptr->vtable->IsOpened(self->m_file.m_ptr)) {
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto out;
	}
	state = self->m_state;
	offset = state->m_position - state->m_buffer_start;
	remaining = state->m_count - offset;
	if (remaining < 0) {
		result = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		goto out;
	}
	src = (unsigned char *)state->m_addr + (uint32_t)offset;
	available = remaining;
	if (nbyte <= (uint32_t)remaining) {
		sce_paf_memcpy(buf, src, nbyte);
		state->m_position += nbyte;
		result = nbyte;
		goto out;
	}
	FileSharedPtrCopy(&file, &self->m_file);
	result = BufferedFileFlushDirty(&file, state);
	FileSharedPtrRelease(&file);
	if (result)
		goto out;
	if (available) {
		sce_paf_memcpy(buf, src, available);
		state->m_position += available;
		copied = available;
		buf = (unsigned char *)buf + available;
		nbyte -= available;
	}
	if (nbyte > state->m_capacity) {
		result = self->m_file.m_ptr->vtable->Read(self->m_file.m_ptr,
						       buf, nbyte);
		if (result >= 0) {
			position = state->m_file_position + result;
			state->m_count = 0;
			state->m_file_position = position;
			state->m_position = position;
			state->m_buffer_start = position;
			result = (uint32_t)result + copied;
		}
		goto out;
	}
	result = self->m_file.m_ptr->vtable->Read(self->m_file.m_ptr,
					       state->m_addr, state->m_capacity);
	if (result < 0)
		goto out;
	received = nbyte;
	if ((uint32_t)result < nbyte)
		received = result;
	position = state->m_file_position;
	src = state->m_addr;
	state->m_buffer_start = position;
	state->m_count = result;
	state->m_file_position = position + result;
	state->m_position = position + received;
	sce_paf_memcpy(buf, src, received);
	result = copied + received;
out:
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
}

int32_t BufferedFileReadAsync(BufferedFile *self, void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t BufferedFileWrite(BufferedFile *self, const void *buf, size_t nbyte)
{
	FileSharedPtr file;
	BufferedFileState *state;
	int64_t offset;
	int64_t remaining;
	int64_t position;
	int64_t size;
	int32_t result;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (!buf) {
		result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto out;
	}
	if (!self->m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->m_file.m_ptr->vtable->IsOpened(self->m_file.m_ptr)) {
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto out;
	}
	state = self->m_state;
	offset = state->m_position - state->m_buffer_start;
	remaining = state->m_capacity - offset;
	if (remaining < 0) {
		result = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		goto out;
	}
	if (nbyte <= (uint32_t)remaining) {
		sce_paf_memcpy((unsigned char *)state->m_addr + (uint32_t)offset,
			       buf, nbyte);
		position = state->m_position + nbyte;
		offset = state->m_buffer_start;
		state->m_position = position;
		size = state->m_file_size;
		if (position > offset + state->m_count)
			state->m_count = position - offset;
		state->m_dirty = 1;
		if (size < position)
			size = position;
		state->m_file_size = size;
		result = nbyte;
		goto out;
	}
	FileSharedPtrCopy(&file, &self->m_file);
	result = BufferedFileFlushDirty(&file, state);
	FileSharedPtrRelease(&file);
	if (result)
		goto out;
	if (state->m_position != state->m_file_position) {
		position = self->m_file.m_ptr->vtable->Seek(self->m_file.m_ptr,
							state->m_position, SCE_SEEK_SET);
		if (position < 0) {
			result = position;
			goto out;
		}
		state->m_file_position = position;
	}
	if (nbyte > state->m_capacity) {
		result = self->m_file.m_ptr->vtable->Write(self->m_file.m_ptr,
							buf, nbyte);
		if (result >= 0) {
			size = state->m_file_size;
			position = state->m_file_position + result;
			state->m_file_position = position;
			state->m_position = position;
			if (size < position)
				size = position;
			state->m_file_size = size;
			state->m_dirty = 0;
			state->m_count = 0;
			state->m_buffer_start = position;
		}
		goto out;
	}
	sce_paf_memcpy(state->m_addr, buf, nbyte);
	position = state->m_file_position;
	state->m_buffer_start = position;
	state->m_count = nbyte;
	size = state->m_file_size;
	state->m_dirty = 1;
	state->m_position = position + nbyte;
	if (size < position + nbyte)
		size = position + nbyte;
	state->m_file_size = size;
	result = nbyte;
out:
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
}

int32_t BufferedFileWriteAsync(BufferedFile *self, const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int64_t BufferedFileSeek(BufferedFile *self, int64_t offset, int32_t whence)
{
	FileSharedPtr file;
	BufferedFileState *state;
	int64_t position = 0;
	int64_t size;
	int64_t result;
	int32_t error;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (!self->m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->m_file.m_ptr->vtable->IsOpened(self->m_file.m_ptr)) {
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto out;
	}
	state = self->m_state;
	if (whence != SCE_SEEK_END) {
		position = offset;
		if (whence == SCE_SEEK_CUR)
			position += state->m_position;
		if (position < 0) {
			result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
			goto out;
		}
		if (position >= state->m_buffer_start &&
		    position <= state->m_buffer_start + state->m_count) {
			state->m_position = position;
			result = position;
			goto out;
		}
	}
	FileSharedPtrCopy(&file, &self->m_file);
	error = BufferedFileFlushDirty(&file, state);
	FileSharedPtrRelease(&file);
	if (error) {
		result = error;
		goto out;
	}
	if (whence == SCE_SEEK_CUR)
		offset = position - state->m_file_position;
	result = self->m_file.m_ptr->vtable->Seek(self->m_file.m_ptr, offset, whence);
	if (result >= 0) {
		size = state->m_file_size;
		state->m_count = 0;
		state->m_file_position = result;
		state->m_position = result;
		state->m_buffer_start = result;
		if (size < result)
			size = result;
		state->m_file_size = size;
	}
out:
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
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
