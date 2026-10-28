#include "httpfile.h"
#include "httpfile_reader.h"
#include "../paf_types.h"

const FileClass httpFileClass = {
	0, NULL, {
		(int32_t (*)(const File *))HttpFileGetFileType,
		(int32_t (*)(const File *))HttpFileGetCapability,
		(int64_t (*)(const File *))HttpFileGetFileSize,
		(int32_t (*)(File *, const FileOpenArg *))HttpFileOpenWithArg,
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

int64_t HttpFileGetFileSize(HttpFile *self)
{
	int64_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->m_io_thread)
		ret = SCE_PAF_ERROR_FILE_BUSY;
	else if (self->m_is_opened)
		ret = self->m_http_reader->vtable->GetFileSize(self->m_http_reader);
	else
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileAbort(HttpFile *self)
{
	int32_t ret;

	RWLockLockShared(&self->m_lock_rwlock);
	if (self->m_http_reader)
		ret = self->m_http_reader->vtable->Abort(self->m_http_reader);
	else
		ret = self->m_io_thread ? SCE_PAF_OK : SCE_PAF_ERROR_FILE_NOT_BUSY;
	RWLockUnlockShared(&self->m_lock_rwlock);
	return ret;
}

int32_t HttpFileRead(HttpFile *self, void *buf, size_t nbyte)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (!buf) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	} else if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (self->m_is_opened) {
		HttpFileReader *reader;

		ret = self->m_http_reader->vtable->Read(self->m_http_reader, buf, nbyte);
		reader = self->m_http_reader;
		if (reader) {
			self->m_reader_result[0] = reader->m_result[0];
			self->m_reader_result[1] = reader->m_result[1];
			self->m_reader_result[2] = reader->m_result[2];
		}
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int64_t HttpFileSeek(HttpFile *self, int64_t offset, int32_t whence)
{
	int64_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (self->m_is_opened) {
		HttpFileReader *reader;

		ret = self->m_http_reader->vtable->Seek(self->m_http_reader, offset, whence);
		reader = self->m_http_reader;
		if (reader) {
			self->m_reader_result[0] = reader->m_result[0];
			self->m_reader_result[1] = reader->m_result[1];
			self->m_reader_result[2] = reader->m_result[2];
		}
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseCode(HttpFile *self, uint16_t *code)
{
	int32_t ret;
	unsigned char opened;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	opened = self->m_is_opened;
	*code = 0;
	if (opened && self->m_http_reader) {
		HttpFileResponse *response = self->m_http_reader->vtable->GetResponse(self->m_http_reader);

		if (response->valid) {
			*code = response->code;
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseVersion(HttpFile *self, unsigned char *major,
				 unsigned char *minor)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	*minor = 0;
	*major = 0;
	if (!self->m_is_opened || !self->m_http_reader) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		HttpFileResponse *response = self->m_http_reader->vtable->GetResponse(self->m_http_reader);

		if (response->valid) {
			*major = response->major;
			/* The Vita implementation returns the major byte twice. */
			*minor = response->major;
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponsePhrase(HttpFile *self, PafString *phrase)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	PafStringAssign(phrase, "", sce_paf_strlen(""));
	if (!self->m_is_opened || !self->m_http_reader) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		HttpFileResponse *response = self->m_http_reader->vtable->GetResponse(self->m_http_reader);

		if (response->valid) {
			const char *text = response->phrase.ptr;
			size_t len = text ? sce_paf_strlen(text) : 0;

			PafStringAssign(phrase, text, len);
			ret = SCE_PAF_OK;
		} else {
			ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		}
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseHeader(HttpFile *self, const char *name,
				PafString *value)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	PafStringAssign(value, "", sce_paf_strlen(""));
	if (!self->m_is_opened || !self->m_http_reader) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	} else if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else {
		PafMap *headers = self->m_http_reader->vtable->GetResponseHeaders(self->m_http_reader);
		const char *text = ScePafMisc_82171187(headers, name, 0);
		size_t len = text ? sce_paf_strlen(text) : 0;

		PafStringAssign(value, text, len);
		ret = SCE_PAF_OK;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}
