#include "httpfile.h"
#include "../paf_types.h"

const FileClass httpFileClass = {
	0, NULL, {
		(int32_t (*)(const File *))HttpFileGetFileType,
		(int32_t (*)(const File *))HttpFileGetCapability,
		(int64_t (*)(const File *))HttpFileGetFileSize,
		(int32_t (*)(File *, const FileOpenArg *))HttpFileOpenArg,
		(int32_t (*)(File *, const FileOpenArg *))HttpFileOpenAsync,
		(int32_t (*)(File *))HttpFileClose,
		(int32_t (*)(File *))HttpFileCloseAsync,
		(unsigned char (*)(const File *))HttpFileIsOpened,
		(unsigned char (*)(File *))HttpFileIsEof,
		(int32_t (*)(File *))HttpFileAbort,
		(int32_t (*)(File *, void *, size_t))HttpFileRead,
		(int32_t (*)(File *, void *, size_t))HttpFileReadAsync,
		(int32_t (*)(File *, const void *, size_t))HttpFileWrite,
		(int32_t (*)(File *, const void *, size_t))HttpFileWriteAsync,
		(int64_t (*)(File *, int64_t, int32_t))HttpFileSeek,
		(int32_t (*)(File *, int64_t, int32_t))HttpFileSeekAsync,
		(int32_t (*)(File *))HttpFileFlush,
		(int32_t (*)(File *))HttpFileFlushAsync,
		(int32_t (*)(File *, int64_t *))HttpFileWaitAsync,
		(int32_t (*)(File *, int64_t *))HttpFilePollAsync,
		(int32_t (*)(File *, FileStat *))HttpFileGetstat,
		FileSetPriority,
		(File *(*)(File *))HttpFileDestroy,
		(void (*)(File *))HttpFileDelete
	}
};

HttpFile *HttpFileConstruct(HttpFile *self)
{
	self->base.vtable = &httpFileClass.methods;
	self->m_is_opened = 0;
	RWLockConstruct(&self->m_lock_rwlock, "ScePafHttpFileRwLock", 0);
	ScePafThread_C4BD9753(&self->m_lock_mutex, "ScePafHttpFileCsLock", 0);
	self->m_io_thread = NULL;
	self->m_http_reader = NULL;
	self->m_reader_param = NULL;
	self->m_reader_result[0] = 0;
	self->m_reader_result[1] = 0;
	self->m_reader_result[2] = 0;
	self->m_reader_context.m_ptr = NULL;
	self->m_reader_context.m_ref_counter = NULL;
	return self;
}

HttpFile *HttpFileConstructBase(HttpFile *self)
{
	self->base.vtable = &httpFileClass.methods;
	self->m_is_opened = 0;
	RWLockConstruct(&self->m_lock_rwlock, "ScePafHttpFileRwLock", 0);
	ScePafThread_C4BD9753(&self->m_lock_mutex, "ScePafHttpFileCsLock", 0);
	self->m_io_thread = NULL;
	self->m_http_reader = NULL;
	self->m_reader_param = NULL;
	self->m_reader_result[0] = 0;
	self->m_reader_result[1] = 0;
	self->m_reader_result[2] = 0;
	self->m_reader_context.m_ptr = NULL;
	self->m_reader_context.m_ref_counter = NULL;
	return self;
}

int32_t HttpFileGetFileType(const HttpFile *self)
{
	return Type_Http;
}

int32_t HttpFileGetCapability(const HttpFile *self)
{
	return 0x7bf7;
}

unsigned char HttpFileIsOpened(HttpFile *self)
{
	unsigned char opened;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	opened = self->m_is_opened;
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return opened;
}

unsigned char HttpFileIsEof(HttpFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return 0;
}

int32_t HttpFileWrite(HttpFile *self, const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFileWriteAsync(HttpFile *self, const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFileFlush(HttpFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFileFlushAsync(HttpFile *self)
{
	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}
