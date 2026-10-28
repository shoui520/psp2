#include "memfile.h"
#include "../paf_types.h"

extern void *sce_paf_memcpy(void *dst, const void *src, size_t size);

const FileClass memFileClass = {
	0, NULL, {
		(int32_t (*)(const File *))MemFileGetFileType,
		(int32_t (*)(const File *))MemFileGetCapability,
		(int64_t (*)(const File *))MemFileGetFileSize,
		(int32_t (*)(File *, const FileOpenArg *))MemFileOpenArgMethod,
		(int32_t (*)(File *, const FileOpenArg *))MemFileOpenAsync,
		(int32_t (*)(File *))MemFileClose,
		(int32_t (*)(File *))MemFileCloseAsync,
		(unsigned char (*)(const File *))MemFileIsOpened,
		(unsigned char (*)(File *))MemFileIsEof,
		(int32_t (*)(File *))MemFileAbort,
		(int32_t (*)(File *, void *, size_t))MemFileRead,
		(int32_t (*)(File *, void *, size_t))MemFileReadAsync,
		(int32_t (*)(File *, const void *, size_t))MemFileWrite,
		(int32_t (*)(File *, const void *, size_t))MemFileWriteAsync,
		(int64_t (*)(File *, int64_t, int32_t))MemFileSeek,
		(int32_t (*)(File *, int64_t, int32_t))MemFileSeekAsync,
		(int32_t (*)(File *))MemFileFlush,
		(int32_t (*)(File *))MemFileFlushAsync,
		(int32_t (*)(File *, int64_t *))MemFileWaitAsync,
		(int32_t (*)(File *, int64_t *))MemFilePollAsync,
		FileGetstat, FileSetPriority,
		(File *(*)(File *))MemFileDestroy,
		(void (*)(File *))MemFileDelete
	}
};

MemFile *MemFileConstruct(MemFile *self)
{
	self->base.vtable = &memFileClass.methods;
	self->m_waiting_for_sync = 0;
	self->m_is_writable = 0;
	self->m_is_opened = 0;
	self->m_buffer.m_ptr = NULL;
	self->m_buffer.m_ref_counter = NULL;
	self->m_mem_addr = NULL;
	self->m_mem_size = 0;
	self->m_mem_pos = 0;
	self->m_io_result = 0;
	ScePafThread_C4BD9753(&self->m_lock_mutex, "ScePafMemFileCsLock", 0);
	return self;
}

void MemFileOpenBuffer(FileSharedPtr *result, const BufferSharedPtr *buffer, int32_t *error)
{
	MemFile *self = ScePafStdc_FC5CD359(sizeof(*self));
	MemFileOpenArg param;
	const FileVTable *vtable;
	int32_t ret;

	if (!self) {
		if (error)
			*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	MemFileConstruct(self);
	param.buffer.m_ptr = NULL;
	param.buffer.m_ref_counter = NULL;
	param.file_type = Type_Mem;
	if (buffer->m_ptr)
		BufferSharedPtrCopy(&param.buffer, buffer);
	ret = self->base.vtable->Open(&self->base, (const FileOpenArg *)&param);
	vtable = self->base.vtable;
	if (error)
		*error = ret;
	if (vtable->IsOpened(&self->base)) {
		FileRefCounter *counter;

		result->m_ptr = &self->base;
		counter = ScePafStdc_FC5CD359(sizeof(*counter));
		if (counter) {
			result->m_ref_counter = counter;
			counter->m_ptr = &self->base;
			counter->m_counter = 1;
			counter->m_weak_counter = 1;
			counter->m_deleter = MemFileDefaultDelete;
		} else {
			result->m_ptr = NULL;
			result->m_ref_counter = NULL;
		}
	} else {
		self->base.vtable->Delete(&self->base);
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
	}
	BufferSharedPtrRelease(&param.buffer);
}

void MemFileOpen(FileSharedPtr *result, const void *addr, size_t size, int32_t *error)
{
	BufferSharedPtr buffer, arg;

	if (!addr) {
		if (error)
			*error = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	UnmanagedConstBufferCreate(&buffer, addr, size);
	if (!buffer.m_ptr) {
		if (error)
			*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	BufferSharedPtrCopy(&arg, &buffer);
	MemFileOpenBuffer(result, &arg, error);
	BufferSharedPtrRelease(&arg);
	BufferSharedPtrRelease(&buffer);
}

void MemFileOpenWritable(FileSharedPtr *result, void *addr, size_t size, int32_t *error)
{
	MemFileOpen(result, addr, size, error);
	if (result->m_ptr)
		((MemFile *)result->m_ptr)->m_is_writable = 1;
}

void MemFileOpenWritableBuffer(FileSharedPtr *result, const BufferSharedPtr *buffer,
			       int32_t *error)
{
	BufferSharedPtr arg;

	BufferSharedPtrCopy(&arg, buffer);
	MemFileOpenBuffer(result, &arg, error);
	BufferSharedPtrRelease(&arg);
	if (result->m_ptr)
		((MemFile *)result->m_ptr)->m_is_writable = 1;
}

int32_t MemFileGetFileType(const MemFile *self)
{
	return Type_Mem;
}

int32_t MemFileGetCapability(const MemFile *self)
{
	return 0x7bf7;
}

int64_t MemFileGetFileSize(const MemFile *self)
{
	RMutex *mutex = (RMutex *)&self->m_lock_mutex;
	int64_t result;

	ScePafThread_F9FFA0BE(mutex);
	if (self->base.vtable->IsOpened(&self->base))
		result = self->m_mem_size;
	else
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
	ScePafThread_95F9C45F(mutex);
	return result;
}

static inline __attribute__((always_inline))
int32_t MemFileGetSyncResult(MemFile *self)
{
	int64_t result;
	int32_t ret = self->base.vtable->WaitAsync(&self->base, &result);

	return ret ? ret : (int32_t)result;
}

int32_t MemFileOpenArgMethod(MemFile *self, const FileOpenArg *param)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ret = self->base.vtable->OpenAsync(&self->base, param);
	if (!ret)
		ret = MemFileGetSyncResult(self);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileOpenAsync(MemFile *self, const FileOpenArg *param)
{
	const MemFileOpenArg *arg = (const MemFileOpenArg *)param;
	BufferSharedPtr buffer;
	int32_t ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->base.vtable->IsOpened(&self->base)) {
		ret = SCE_PAF_ERROR_FILE_ALREADY_OPENED;
		goto done;
	}
	if (self->m_waiting_for_sync) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
		goto done;
	}
	if (!arg || self->base.vtable->GetFileType(&self->base) != arg->file_type)
		goto done;
	BufferSharedPtrCopy(&buffer, &arg->buffer);
	if (buffer.m_ptr && buffer.m_ptr->vtable->GetConstAddr(buffer.m_ptr)) {
		Buffer *ptr;

		BufferSharedPtrAssign(&self->m_buffer, &buffer);
		ptr = self->m_buffer.m_ptr;
		self->m_mem_addr = ptr->vtable->GetConstAddr(ptr);
		ptr = self->m_buffer.m_ptr;
		self->m_mem_size = ptr->vtable->GetSize(ptr);
		self->m_is_writable = 0;
		self->m_mem_pos = 0;
		self->m_waiting_for_sync = 1;
		self->m_is_opened = 1;
		self->m_io_result = 0;
		ret = 0;
	}
	BufferSharedPtrRelease(&buffer);
done:
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileClose(MemFile *self)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ret = self->base.vtable->CloseAsync(&self->base);
	if (!ret)
		ret = MemFileGetSyncResult(self);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileCloseAsync(MemFile *self)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base))
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	else if (self->m_waiting_for_sync)
		ret = SCE_PAF_ERROR_FILE_BUSY;
	else {
		self->m_io_result = 0;
		self->m_waiting_for_sync = 1;
		ret = 0;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

unsigned char MemFileIsOpened(const MemFile *self)
{
	RMutex *mutex = (RMutex *)&self->m_lock_mutex;
	unsigned char result;

	ScePafThread_F9FFA0BE(mutex);
	result = self->m_is_opened;
	ScePafThread_95F9C45F(mutex);
	return result;
}

unsigned char MemFileIsEof(MemFile *self)
{
	unsigned char result;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	result = self->m_mem_pos == self->m_mem_size;
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return result;
}

int32_t MemFileAbort(MemFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->m_waiting_for_sync)
		self->m_io_result = 0;
	self->m_waiting_for_sync = 0;
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return 0;
}

int32_t MemFileRead(MemFile *self, void *buf, size_t nbyte)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ret = self->base.vtable->ReadAsync(&self->base, buf, nbyte);
	if (!ret)
		ret = MemFileGetSyncResult(self);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

static inline __attribute__((always_inline))
int32_t MemFileTransfer(MemFile *self, void *buf, size_t nbyte, unsigned char write)
{
	int32_t ret = 0;
	int64_t position;
	size_t count = nbyte;
	void *addr;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base)) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto done;
	}
	if (self->m_waiting_for_sync) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
		goto done;
	}
	if (!buf) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto done;
	}
	if (write && !self->m_is_writable) {
		ret = SCE_PAF_ERROR_FILE_PROTECTED;
		goto done;
	}
	self->m_waiting_for_sync = 1;
	if (self->base.vtable->IsEof(&self->base)) {
		self->m_io_result = 0;
		goto done;
	}
	position = self->m_mem_pos;
	if ((int64_t)((uint64_t)position + nbyte) > self->m_mem_size) {
		uint64_t remaining = (uint64_t)self->m_mem_size - (uint64_t)position;

		if (remaining > nbyte) {
			ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
			goto done;
		}
		count = remaining;
	}
	addr = (void *)((uintptr_t)self->m_mem_addr + (uint32_t)position);
	if (write)
		sce_paf_memcpy(addr, buf, count);
	else
		sce_paf_memcpy(buf, addr, count);
	position = self->m_mem_pos;
	self->m_io_result = count;
	self->m_mem_pos = (int64_t)((uint64_t)position + count);
done:
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileReadAsync(MemFile *self, void *buf, size_t nbyte)
{
	return MemFileTransfer(self, buf, nbyte, 0);
}

int32_t MemFileWrite(MemFile *self, const void *buf, size_t nbyte)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ret = self->base.vtable->WriteAsync(&self->base, buf, nbyte);
	if (!ret)
		ret = MemFileGetSyncResult(self);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileWriteAsync(MemFile *self, const void *buf, size_t nbyte)
{
	return MemFileTransfer(self, (void *)buf, nbyte, 1);
}

int64_t MemFileSeek(MemFile *self, int64_t offset, int32_t whence)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ret = self->base.vtable->SeekAsync(&self->base, offset, whence);
	if (!ret)
		ret = MemFileGetSyncResult(self);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileSeekAsync(MemFile *self, int64_t offset, int32_t whence)
{
	int32_t ret = 0;
	int64_t position;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base)) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto done;
	}
	if (self->m_waiting_for_sync) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
		goto done;
	}
	switch (whence) {
	case 0:
		position = offset;
		break;
	case 1:
		position = (int64_t)((uint64_t)self->m_mem_pos + (uint64_t)offset);
		break;
	case 2:
		position = (int64_t)((uint64_t)self->m_mem_size + (uint64_t)offset);
		break;
	default:
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto done;
	}
	if (position < 0) {
		ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		goto done;
	}
	if (position > self->m_mem_size) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto done;
	}
	self->m_mem_pos = position;
	self->m_io_result = position;
	self->m_waiting_for_sync = 1;
done:
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFileFlush(MemFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return 0;
}

int32_t MemFileFlushAsync(MemFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return 0;
}

int32_t MemFileWaitAsync(MemFile *self, int64_t *result)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->m_waiting_for_sync) {
		int64_t value = self->m_io_result;

		self->m_waiting_for_sync = 0;
		*result = value;
		ret = 0;
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t MemFilePollAsync(MemFile *self, int64_t *result)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!self->base.vtable->IsOpened(&self->base))
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	else if (self->m_waiting_for_sync) {
		int64_t value = self->m_io_result;

		self->m_waiting_for_sync = 0;
		*result = value;
		ret = 0;
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

MemFile *MemFileConstructBase(MemFile *self)
{
	return MemFileConstruct(self);
}

MemFile *MemFileDestroy(MemFile *self)
{
	self->base.vtable = &memFileClass.methods;
	ScePafThread_8CCECAC9(&self->m_lock_mutex);
	BufferSharedPtrRelease(&self->m_buffer);
	self->base.vtable = &fileClass.methods;
	return self;
}

void MemFileDelete(MemFile *self)
{
	MemFileDestroy(self);
	ScePafStdc_1B77082E(self);
}

MemFile *MemFileDestroyBase(MemFile *self)
{
	return MemFileDestroy(self);
}

void MemFileDefaultDelete(File *file)
{
	if (file)
		file->vtable->Delete(file);
}
