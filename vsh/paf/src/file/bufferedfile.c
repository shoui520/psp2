#include "bufferedfile.h"
#include "localfile.h"
#include "httpfile.h"
#include "../std/stdlib.h"
#include "../paf_types.h"
#include <kernel/iofilemgr.h>

const BufferedFileClass bufferedFileClass = {
	0, NULL, {
		{
			{
				(int32_t (*)(const File *))BufferedFileGetFileType,
				(int32_t (*)(const File *))BufferedFileGetCapability,
				(int64_t (*)(const File *))BufferedFileGetFileSize,
				(int32_t (*)(File *, const FileOpenArg *))BufferedFileOpenArg,
				(int32_t (*)(File *, const FileOpenArg *))BufferedFileOpenAsync,
				(int32_t (*)(File *))BufferedFileClose,
				(int32_t (*)(File *))BufferedFileCloseAsync,
				(unsigned char (*)(const File *))BufferedFileIsOpened,
				(unsigned char (*)(File *))BufferedFileIsEof,
				(int32_t (*)(File *))BufferedFileAbort,
				(int32_t (*)(File *, void *, size_t))BufferedFileRead,
				(int32_t (*)(File *, void *, size_t))BufferedFileReadAsync,
				(int32_t (*)(File *, const void *, size_t))BufferedFileWrite,
				(int32_t (*)(File *, const void *, size_t))BufferedFileWriteAsync,
				(int64_t (*)(File *, int64_t, int32_t))BufferedFileSeek,
				(int32_t (*)(File *, int64_t, int32_t))BufferedFileSeekAsync,
				(int32_t (*)(File *))BufferedFileFlush,
				(int32_t (*)(File *))BufferedFileFlushAsync,
				(int32_t (*)(File *, int64_t *))BufferedFileWaitAsync,
				(int32_t (*)(File *, int64_t *))BufferedFilePollAsync,
				(int32_t (*)(File *, FileStat *))FileHolderGetstat,
				(int32_t (*)(File *, int32_t))FileHolderSetPriority,
				(File *(*)(File *))BufferedFileDestroy,
				(void (*)(File *))BufferedFileDelete
			},
			BufferedFileSetFile, FileHolderClearFile, FileHolderGetFile
		},
		BufferedFileOpenBuffer
	}
};

int64_t BufferedFileGetFileSize(const BufferedFile *self)
{
	if (self->base.m_file.m_ptr)
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
	file = self->base.m_file.m_ptr;
	if (file)
		result = file->vtable->GetCapability(file) & 63;
	ScePafThread_95F9C45F(mutex);
	return result;
}

int32_t BufferedFileOpenArg(BufferedFile *self, const FileOpenArg *param)
{
	const BufferedFileVTable *vtable = (const BufferedFileVTable *)self->base.base.vtable;
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
	if (self->base.m_file.m_ptr) {
		FileSharedPtrCopy(&copy, &self->base.m_file);
		BufferedFileFlushDirty(&copy, state);
		FileSharedPtrRelease(&copy);
		result = self->base.m_file.m_ptr->vtable->Close(self->base.m_file.m_ptr);
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
	file = self->base.m_file.m_ptr;
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
	if (self->base.m_file.m_ptr) {
		state = self->m_state;
		if (state->m_position == state->m_file_position) {
			result = self->base.m_file.m_ptr->vtable->IsEof(self->base.m_file.m_ptr);
		} else if (state->m_position <= state->m_file_position) {
			result = 0;
		} else {
			FileSharedPtrCopy(&copy, &self->base.m_file);
			BufferedFileResetBuffer(&copy, state);
			FileSharedPtrRelease(&copy);
			result = self->base.m_file.m_ptr->vtable->IsEof(self->base.m_file.m_ptr);
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
	if (!self->base.m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->base.m_file.m_ptr->vtable->IsOpened(self->base.m_file.m_ptr)) {
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
	FileSharedPtrCopy(&file, &self->base.m_file);
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
		result = self->base.m_file.m_ptr->vtable->Read(self->base.m_file.m_ptr,
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
	result = self->base.m_file.m_ptr->vtable->Read(self->base.m_file.m_ptr,
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
	if (!self->base.m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->base.m_file.m_ptr->vtable->IsOpened(self->base.m_file.m_ptr)) {
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
	FileSharedPtrCopy(&file, &self->base.m_file);
	result = BufferedFileFlushDirty(&file, state);
	FileSharedPtrRelease(&file);
	if (result)
		goto out;
	if (state->m_position != state->m_file_position) {
		position = self->base.m_file.m_ptr->vtable->Seek(self->base.m_file.m_ptr,
							state->m_position, SCE_SEEK_SET);
		if (position < 0) {
			result = position;
			goto out;
		}
		state->m_file_position = position;
	}
	if (nbyte > state->m_capacity) {
		result = self->base.m_file.m_ptr->vtable->Write(self->base.m_file.m_ptr,
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
	if (!self->base.m_file.m_ptr) {
		result = SCE_PAF_ERROR_FILE_PROTECTED;
		goto out;
	}
	if (!self->base.m_file.m_ptr->vtable->IsOpened(self->base.m_file.m_ptr)) {
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
	FileSharedPtrCopy(&file, &self->base.m_file);
	error = BufferedFileFlushDirty(&file, state);
	FileSharedPtrRelease(&file);
	if (error) {
		result = error;
		goto out;
	}
	if (whence == SCE_SEEK_CUR)
		offset = position - state->m_file_position;
	result = self->base.m_file.m_ptr->vtable->Seek(self->base.m_file.m_ptr, offset, whence);
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
	if (!self->base.m_file.m_ptr) {
		ScePafThread_95F9C45F(&self->lock_mutex);
		return SCE_PAF_ERROR_FILE_PROTECTED;
	}
	if (!self->base.m_file.m_ptr->vtable->IsOpened(self->base.m_file.m_ptr)) {
		ScePafThread_95F9C45F(&self->lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	state = self->m_state;
	FileSharedPtrCopy(&copy, &self->base.m_file);
	result = BufferedFileFlushDirty(&copy, state);
	FileSharedPtrRelease(&copy);
	self->base.m_file.m_ptr->vtable->Flush(self->base.m_file.m_ptr);
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

void BufferedFileSetFile(FileSharedPtr *result, FileHolder *holder,
			 FileSharedPtr *file)
{
	BufferedFile *self = (BufferedFile *)holder;
	FileSharedPtr copy;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (file->m_ptr) {
		FileSharedPtrCopy(&copy, file);
		BufferedFileResetBuffer(&copy, self->m_state);
		FileSharedPtrRelease(&copy);
	}
	FileSharedPtrCopy(&copy, file);
	FileHolderSetFile(result, holder, &copy);
	FileSharedPtrRelease(&copy);
	ScePafThread_95F9C45F(&self->lock_mutex);
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

int32_t BufferedFileOpenBuffer(BufferedFile *self, const FileOpenArg *param,
			       BufferSharedPtr *buffer)
{
	const BufferedFileOpenParam *arg = (const BufferedFileOpenParam *)param;
	const FileHolderVTable *vtable;
	BufferedFileState *state;
	FileSharedPtr file;
	FileSharedPtr converted;
	FileSharedPtr copy;
	FileSharedPtr previous;
	BufferSharedPtr default_buffer;
	FileRefCounter *counter;
	LocalFile *local;
	HttpFile *http;
	File *ptr;
	void (*deleter)(File *file);
	int32_t result;

	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (self->base.base.vtable->IsOpened(&self->base.base)) {
		result = SCE_PAF_ERROR_FILE_ALREADY_OPENED;
		goto out;
	}
	if (!param) {
		result = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto out;
	}
	if ((int32_t)arg->fileType == 1 && !(arg->flag & SCE_O_APPEND)) {
		local = ScePafStdc_FC5CD359(sizeof(*local));
		ptr = local ? (File *)LocalFileConstruct(local) : NULL;
		deleter = LocalFileDefaultDelete;
	} else if ((int32_t)arg->fileType == 3) {
		http = ScePafStdc_FC5CD359(sizeof(*http));
		ptr = http ? (File *)HttpFileConstruct(http) : NULL;
		deleter = HttpFileDefaultDelete;
	} else {
		result = SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
		goto out;
	}
	file.m_ptr = ptr;
	file.m_ref_counter = NULL;
	if (ptr) {
		counter = ScePafStdc_FC5CD359(sizeof(*counter));
		if (!counter) {
			/* As in the original, the constructed file remains allocated. */
			result = SCE_PAF_ERROR_FILE_NO_MEMORY;
			goto out;
		}
		counter->m_ptr = ptr;
		counter->m_counter = 1;
		counter->m_weak_counter = 1;
		counter->m_deleter = deleter;
		file.m_ref_counter = counter;
	}
	FileSharedPtrCopy(&converted, &file);
	FileSharedPtrRelease(&file);
	if (!converted.m_ptr) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		goto out;
	}
	if (!buffer->m_ptr) {
		BufferedFileCreateDefaultBuffer(&default_buffer);
		BufferSharedPtrAssign(buffer, &default_buffer);
		BufferSharedPtrRelease(&default_buffer);
	}
	if (!buffer->m_ptr) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		goto release;
	}
	result = converted.m_ptr->vtable->Open(converted.m_ptr, param);
	if (result)
		goto release;
	BufferSharedPtrAssign(&self->m_buffer, buffer);
	state = self->m_state;
	state->m_addr = buffer->m_ptr->vtable->GetAddr(buffer->m_ptr);
	state->m_capacity = buffer->m_ptr->vtable->GetSize(buffer->m_ptr);
	state->m_count = 0;
	state->m_buffer_start = 0;
	state->m_position = 0;
	state->m_file_position = 0;
	state->m_file_size = 0;
	state->m_dirty = 0;
	FileSharedPtrCopy(&copy, &converted);
	vtable = (const FileHolderVTable *)self->base.base.vtable;
	vtable->SetFile(&previous, &self->base, &copy);
	FileSharedPtrRelease(&previous);
	FileSharedPtrRelease(&copy);
	result = 0;
release:
	FileSharedPtrRelease(&converted);
out:
	ScePafThread_95F9C45F(&self->lock_mutex);
	return result;
}

static inline __attribute__((always_inline))
void BufferedFileInitialize(BufferedFile *self)
{
	self->base.m_file.m_ref_counter = NULL;
	self->base.base.vtable = &bufferedFileClass.methods.base.base;
	self->base.m_file.m_ptr = NULL;
	ScePafThread_C4BD9753(&self->lock_mutex, "ScePafBufferedReaderCsLock", 0);
	self->m_buffer.m_ptr = NULL;
	self->m_buffer.m_ref_counter = NULL;
	self->m_state = ScePafStdc_FC5CD359(sizeof(*self->m_state));
}

BufferedFile *BufferedFileConstruct(BufferedFile *self)
{
	BufferedFileInitialize(self);
	return self;
}

void BufferedFileOpen(FileSharedPtr *result, FileSharedPtr *file,
		      int32_t *error, BufferSharedPtr *buffer)
{
	BufferedFile *self = NULL;
	BufferedFileState *state;
	const FileHolderVTable *vtable;
	BufferSharedPtr default_buffer;
	FileSharedPtr copy;
	FileSharedPtr previous;
	FileRefCounter *counter;

	if (!buffer->m_ptr) {
		BufferedFileCreateDefaultBuffer(&default_buffer);
		BufferSharedPtrAssign(buffer, &default_buffer);
		BufferSharedPtrRelease(&default_buffer);
	}
	if (buffer->m_ptr) {
		self = ScePafStdc_FC5CD359(sizeof(*self));
		if (self) {
			BufferedFileInitialize(self);
			state = self->m_state;
			if (state) {
				state->m_addr = buffer->m_ptr->vtable->GetAddr(buffer->m_ptr);
				state->m_capacity = buffer->m_ptr->vtable->GetSize(buffer->m_ptr);
				state->m_count = 0;
				state->m_buffer_start = 0;
				state->m_position = 0;
				state->m_file_position = 0;
				state->m_file_size = 0;
				state->m_dirty = 0;
				BufferSharedPtrAssign(&self->m_buffer, buffer);
				FileSharedPtrCopy(&copy, file);
				vtable = (const FileHolderVTable *)self->base.base.vtable;
				vtable->SetFile(&previous, &self->base, &copy);
				FileSharedPtrRelease(&previous);
				FileSharedPtrRelease(&copy);
			} else {
				self->base.base.vtable->Delete(&self->base.base);
				self = NULL;
			}
		}
	}
	if (error)
		*error = self ? 0 : SCE_PAF_ERROR_FILE_NO_MEMORY;
	result->m_ptr = (File *)self;
	if (self) {
		counter = ScePafStdc_FC5CD359(sizeof(*counter));
		if (counter) {
			result->m_ref_counter = counter;
			counter->m_ptr = (File *)self;
			counter = result->m_ref_counter;
			counter->m_counter = 1;
			counter->m_weak_counter = 1;
			counter->m_deleter = FileDefaultDelete;
		} else {
			/* The original retains the object when its counter allocation fails. */
			result->m_ptr = NULL;
			result->m_ref_counter = NULL;
		}
	} else {
		result->m_ref_counter = NULL;
	}
}

BufferedFile *BufferedFileConstructBase(BufferedFile *self)
{
	BufferedFileInitialize(self);
	return self;
}

BufferedFile *BufferedFileDestroy(BufferedFile *self)
{
	unsigned char opened;

	self->base.base.vtable = &bufferedFileClass.methods.base.base;
	ScePafThread_F9FFA0BE(&self->lock_mutex);
	if (self->base.m_file.m_ptr) {
		opened = self->base.m_file.m_ptr->vtable->IsOpened(self->base.m_file.m_ptr);
		ScePafThread_95F9C45F(&self->lock_mutex);
		if (opened)
			BufferedFileClose(self);
	} else {
		ScePafThread_95F9C45F(&self->lock_mutex);
	}
	ScePafStdc_1B77082E(self->m_state);
	BufferSharedPtrRelease(&self->m_buffer);
	ScePafThread_8CCECAC9(&self->lock_mutex);
	FileHolderDestroy(&self->base);
	return self;
}

void BufferedFileDelete(BufferedFile *self)
{
	BufferedFileDestroy(self);
	ScePafStdc_1B77082E(self);
}

BufferedFile *BufferedFileDestroyBase(BufferedFile *self)
{
	return BufferedFileDestroy(self);
}
