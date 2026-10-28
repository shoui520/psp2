#include "httpfile.h"
#include "httpfile_reader.h"
#include "../paf_types.h"
#include "../std/new.h"

extern int sce_paf_strcasecmp(const char *left, const char *right);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern int sce_paf_look_ctype_table(int ch);

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

PafMap *HttpFileHeaderMapClear(PafMap *self)
{
	HttpFileHeaderMapNode *head = self->_Myhead;
	HttpFileHeaderMapNode *node;

	if (!head)
		return self;
	node = head->_Left;
	while (!node->_Isnil) {
		if (!node->_Left->_Isnil) {
			node = node->_Left;
		} else if (!node->_Right->_Isnil) {
			node = node->_Right;
		} else {
			HttpFileHeaderMapNode *parent = node->_Parent;
			PafStringVector *values = &node->_Myval.second;

			if (!parent->_Isnil) {
				if (parent->_Left == node)
					parent->_Left = head;
				else
					parent->_Right = head;
			}
			if (values->_data) {
				size_t count = values->_size;

				while (count) {
					PafStringDestroy(&values->_data[count - 1]);
					--count;
				}
				ScePafStdc_1B77082E(values->_data);
			}
			values->_data = (PafString *)-1;
			values->_size = (size_t)-1;
			values->_capacity = (size_t)-1;
			PafStringDestroy(&node->_Myval.first);
			ScePafStdc_1B77082E(node);
			head = self->_Myhead;
			node = parent;
		}
	}
	head->_Parent = head;
	head->_Left = head;
	head->_Right = head;
	self->_Mysize = 0;
	return NULL;
}

int32_t HttpFileDeleteReader(HttpFile *self)
{
	HttpFileReaderParam *param;

	if (self->m_http_reader)
		self->m_http_reader->vtable->Delete(self->m_http_reader);
	param = self->m_reader_param;
	self->m_http_reader = NULL;
	if (param) {
		HttpFileReaderContextSharedPtrRelease(&param->context);
		BufferSharedPtrRelease(&param->msg_body);
		HttpFileHeaderMapClear(&param->header);
		if (param->header._Myhead)
			ScePafStdc_1B77082E(param->header._Myhead);
		param->header._Myhead = (void *)-1;
		param->header._Mysize = (size_t)-1;
		PafStringDestroy(&param->passwd);
		PafStringDestroy(&param->user);
		PafStringDestroy(&param->uri);
		PafStringDestroy(&param->version);
		PafStringDestroy(&param->method);
		PafStringDestroy(&param->host);
		PafStringDestroy(&param->scheme);
		ScePafStdc_1B77082E(param);
	}
	self->m_reader_param = NULL;
	return 0;
}

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

int32_t HttpFileClose(HttpFile *self)
{
	int32_t ret;

	ScePafThread_F9FFA0BE(&self->m_lock_mutex);
	if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
	} else if (self->m_is_opened) {
		RWLockLockExclusive(&self->m_lock_rwlock);
		ret = self->m_http_reader->vtable->Close(self->m_http_reader);
		HttpFileDeleteReader(self);
		self->m_is_opened = 0;
		RWLockUnlockExclusive(&self->m_lock_rwlock);
	} else {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	ScePafThread_95F9C45F(&self->m_lock_mutex);
	return ret;
}

HttpFile *HttpFileDestroy(HttpFile *self)
{
	self->base.vtable = &httpFileClass.methods;
	if (self->m_io_thread) {
		ThreadCancel(self->m_io_thread);
		ThreadJoin(self->m_io_thread);
		if (self->m_io_thread)
			self->m_io_thread->vtable->Delete(self->m_io_thread);
	}
	self->m_io_thread = NULL;
	if (self->m_http_reader)
		self->m_http_reader->vtable->Close(self->m_http_reader);
	HttpFileDeleteReader(self);
	HttpFileReaderContextSharedPtrRelease(&self->m_reader_context);
	ScePafThread_8CCECAC9(&self->m_lock_mutex);
	RWLockDestroy(&self->m_lock_rwlock);
	self->base.vtable = &fileClass.methods;
	return self;
}

void HttpFileDelete(HttpFile *self)
{
	HttpFileDestroy(self);
	ScePafStdc_1B77082E(self);
}

HttpFile *HttpFileDestroyBase(HttpFile *self)
{
	return HttpFileDestroy(self);
}

HttpFileHeaderMapNode *HttpFileHeaderMapCopyNodes(PafMap *self,
					       HttpFileHeaderMapNode *parent,
					       const HttpFileHeaderMapNode *src)
{
	HttpFileHeaderMapNode *node = ScePafStdc_FC5CD359(sizeof(*node));
	HttpFileHeaderMapNode *entry;

	if (!node) {
		ScePafStdc_D608246A();
		return self->_Myhead;
	}
	entry = PafPlacementNew(sizeof(*entry), node);
	if (entry) {
		HttpFileHeaderMapNode *head = self->_Myhead;
		unsigned char color = src->_Color;

		entry->_Parent = parent;
		entry->_Left = head;
		entry->_Right = head;
		PafStringInitialize(&entry->_Myval.first);
		PafStringCopy(&entry->_Myval.first, &src->_Myval.first);
		PafStringVectorConstructCopy(&entry->_Myval.second, &src->_Myval.second);
		entry->_Color = color;
		entry->_Isnil = 0;
	}
	if (!src->_Left->_Isnil)
		node->_Left = HttpFileHeaderMapCopyNodes(self, node, src->_Left);
	if (!src->_Right->_Isnil)
		node->_Right = HttpFileHeaderMapCopyNodes(self, node, src->_Right);
	return node;
}

PafMap *HttpFileHeaderMapCopy(PafMap *self, const PafMap *src)
{
	HttpFileHeaderMapNode *head;
	HttpFileHeaderMapNode *source;

	self->_Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->_Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		head->_Isnil = 1;
		source = src->_Myhead;
		if (source && !source->_Parent->_Isnil) {
			HttpFileHeaderMapNode *root = HttpFileHeaderMapCopyNodes(self,
								       head, source->_Parent);

			head = self->_Myhead;
			if (root != head) {
				HttpFileHeaderMapNode *node = root;

				head->_Parent = root;
				while (!node->_Left->_Isnil)
					node = node->_Left;
				head->_Left = node;
				node = root;
				while (!node->_Right->_Isnil)
					node = node->_Right;
				head->_Right = node;
				self->_Mysize = src->_Mysize;
			}
		}
	}
	return self;
}

HttpFileReaderConfig *HttpFileReaderConfigConstruct(HttpFileReaderConfig *self,
						  const HttpFileReaderParam *param)
{
	self->scheme = param->scheme.ptr;
	self->host = param->host.ptr;
	self->method = param->method.ptr;
	self->version = param->version.ptr;
	self->port = param->port;
	self->uri = param->uri.ptr;
	self->user = param->user.ptr;
	self->passwd = param->passwd.ptr;
	HttpFileHeaderMapCopy(&self->header, &param->header);
	BufferSharedPtrCopy(&self->msg_body, &param->msg_body);
	self->open_flag = param->open_flag;
	self->https_flag = param->https_flag;
	self->timeout.resolve_timeout = param->timeout.resolve_timeout;
	self->timeout.connect_timeout = param->timeout.connect_timeout;
	self->timeout.send_timeout = param->timeout.send_timeout;
	self->timeout.recv_timeout = param->timeout.recv_timeout;
	self->accept_encoding_gzip = param->accept_encoding_gzip;
	self->context.m_ptr = param->context.m_ptr;
	self->context.m_ref_counter = param->context.m_ref_counter;
	if (self->context.m_ptr)
		sceAtomicIncrement32AcqRel(&self->context.m_ref_counter->m_counter);
	self->template_option = param->template_option;
	self->response_header_max_size = param->response_header_max_size;
	return self;
}

HttpFileReaderConfigRefCounter *HttpFileReaderConfigCreate(HttpFileReaderConfigSharedPtr *result,
							 const HttpFileReaderParam *param)
{
	HttpFileReaderConfig *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self)
		self = HttpFileReaderConfigConstruct(self, param);
	result->m_ptr = self;
	if (self) {
		HttpFileReaderConfigRefCounter *counter = ScePafStdc_FC5CD359(sizeof(*counter));

		if (counter) {
			result->m_ref_counter = counter;
			counter->m_ptr = self;
			result->m_ref_counter->m_counter = 1;
			result->m_ref_counter->m_weak_counter = 1;
			result->m_ref_counter->m_deleter = HttpFileReaderConfigDelete;
			return result->m_ref_counter;
		}
		result->m_ptr = NULL;
	}
	result->m_ref_counter = NULL;
	return NULL;
}

void HttpFileReaderConfigDelete(HttpFileReaderConfig *self)
{
	if (self) {
		HttpFileReaderContextSharedPtrRelease(&self->context);
		BufferSharedPtrRelease(&self->msg_body);
		HttpFileHeaderMapClear(&self->header);
		if (self->header._Myhead)
			ScePafStdc_1B77082E(self->header._Myhead);
		self->header._Myhead = (void *)-1;
		self->header._Mysize = (size_t)-1;
		ScePafStdc_1B77082E(self);
	}
}

int32_t HttpFileReaderConfigIsHttpsCheckEnabled(const HttpFileReaderConfig *self)
{
	return !(self->open_flag & DISABLE_HTTPS_CHECK);
}

int32_t HttpFileReaderConfigIsProxyIgnored(const HttpFileReaderConfig *self)
{
	return (self->open_flag & IGNORE_PROXY_SETTING) != 0;
}

const char *HttpFileHeaderMapGet(const PafMap *headers, const char *name,
			       int32_t index)
{
	PafString key;
	HttpFileHeaderMapNode *node = NULL;

	PafStringInitialize(&key);
	PafStringAssign(&key, name, name ? sce_paf_strlen(name) : 0);
	if (headers->_Myhead) {
		node = ((HttpFileHeaderMapNode *)headers->_Myhead)->_Parent;
		while (!node->_Isnil) {
			if (sce_paf_strcasecmp(key.ptr, node->_Myval.first.ptr) <= 0) {
				if (sce_paf_strcasecmp(node->_Myval.first.ptr, key.ptr) <= 0)
					break;
				node = node->_Right;
			} else {
				node = node->_Left;
			}
		}
	}
	PafStringDestroy(&key);
	if (node == headers->_Myhead || index >= (int32_t)node->_Myval.second._size)
		return NULL;
	return node->_Myval.second._data[index].ptr;
}

HttpFileResponse *HttpFileResponseConstruct(HttpFileResponse *self)
{
	self->valid = 0;
	self->minor = 0;
	self->major = 0;
	self->code = 0;
	PafStringInitialize(&self->phrase);
	return self;
}

HttpFileResponse *HttpFileResponseConstructBase(HttpFileResponse *self)
{
	self->valid = 0;
	self->minor = 0;
	self->major = 0;
	self->code = 0;
	PafStringInitialize(&self->phrase);
	return self;
}

int32_t HttpFileResponseParse(HttpFileResponse *self, const char *line, size_t len)
{
	size_t pos;
	size_t remaining;
	size_t count;
	size_t phrase_len;
	const char *status;
	const char *phrase;

	self->valid = 0;
	PafStringAssign(&self->phrase, NULL, 0);
	self->major = 0;
	self->minor = 0;
	if (len < 9)
		return 0;
	if (sce_paf_strncmp(line, "HTTP/", sce_paf_strlen("HTTP/")))
		return 0;
	pos = sce_paf_strlen("HTTP/");
	if (!(sce_paf_look_ctype_table((signed char)line[pos]) & 4))
		return 0;
	while (sce_paf_look_ctype_table((signed char)line[pos]) & 4) {
		self->major = self->major * 10 + (signed char)line[pos] - '0';
		++pos;
	}
	if (line[pos] != '.')
		return 0;
	++pos;
	if (!(sce_paf_look_ctype_table((signed char)line[pos]) & 4))
		return 0;
	while (sce_paf_look_ctype_table((signed char)line[pos]) & 4) {
		self->minor = self->minor * 10 + (signed char)line[pos] - '0';
		++pos;
	}
	if (line[pos] != ' ')
		return 0;
	++pos;
	if ((int32_t)pos < 0)
		return 0;
	status = line + pos;
	remaining = len - pos;
	if (remaining < sce_paf_strlen("200 "))
		return 0;
	if (!(sce_paf_look_ctype_table((signed char)status[0]) & 4) ||
	    !(sce_paf_look_ctype_table((signed char)status[1]) & 4) ||
	    !(sce_paf_look_ctype_table((signed char)status[2]) & 4))
		return 0;
	self->code = (signed char)status[0] * 100 + (signed char)status[1] * 10 +
		     (signed char)status[2] - '0' * 111;
	pos = sce_paf_strlen("200");
	if ((int32_t)pos < 0)
		return 0;
	phrase = status + pos;
	remaining -= pos;
	count = 0;
	while (phrase[count] != '\n') {
		++count;
		if (count > remaining)
			return 0;
	}
	phrase_len = count;
	if (count && phrase[count - 1] == '\r')
		--phrase_len;
	if ((int32_t)(count + 1) < 0)
		return 0;
	PafStringAssign(&self->phrase, phrase, phrase_len);
	self->valid = 1;
	return 1;
}

HttpFileResponse *HttpFileResponseConstructFromLine(HttpFileResponse *self,
						const char *line, size_t len)
{
	self->valid = 0;
	self->minor = 0;
	self->major = 0;
	self->code = 0;
	PafStringInitialize(&self->phrase);
	self->valid = HttpFileResponseParse(self, line, len);
	return self;
}

HttpFileResponse *HttpFileResponseConstructBaseFromLine(HttpFileResponse *self,
						    const char *line, size_t len)
{
	self->valid = 0;
	self->minor = 0;
	self->major = 0;
	self->code = 0;
	PafStringInitialize(&self->phrase);
	self->valid = HttpFileResponseParse(self, line, len);
	return self;
}

HttpFileResponse *HttpFileResponseDestroy(HttpFileResponse *self)
{
	PafStringDestroy(&self->phrase);
	return self;
}

HttpFileResponse *HttpFileResponseDestroyBase(HttpFileResponse *self)
{
	PafStringDestroy(&self->phrase);
	return self;
}

static inline __attribute__((always_inline))
void HttpFileHeaderMapLrotate(HttpFileHeaderMapNode *head,
			     HttpFileHeaderMapNode *node)
{
	HttpFileHeaderMapNode *right = node->_Right;

	node->_Right = right->_Left;
	if (!right->_Left->_Isnil)
		right->_Left->_Parent = node;
	right->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = right;
	else if (node == node->_Parent->_Left)
		node->_Parent->_Left = right;
	else
		node->_Parent->_Right = right;
	right->_Left = node;
	node->_Parent = right;
}

static inline __attribute__((always_inline))
void HttpFileHeaderMapRrotate(HttpFileHeaderMapNode *head,
			     HttpFileHeaderMapNode *node)
{
	HttpFileHeaderMapNode *left = node->_Left;

	node->_Left = left->_Right;
	if (!left->_Right->_Isnil)
		left->_Right->_Parent = node;
	left->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = left;
	else if (node == node->_Parent->_Right)
		node->_Parent->_Right = left;
	else
		node->_Parent->_Left = left;
	left->_Right = node;
	node->_Parent = left;
}

HttpFileHeaderMapNode *HttpFileHeaderMapRebalance(PafMap *self,
					       HttpFileHeaderMapNode *node)
{
	HttpFileHeaderMapNode *head = self->_Myhead;

	if (!head)
		return (HttpFileHeaderMapNode *)self;
	node->_Color = 1;
	while (node != head->_Parent && node->_Parent->_Color) {
		HttpFileHeaderMapNode *parent = node->_Parent;
		HttpFileHeaderMapNode *grandparent = parent->_Parent;
		HttpFileHeaderMapNode *uncle;

		if (parent == grandparent->_Left) {
			uncle = grandparent->_Right;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Right) {
					node = node->_Parent;
					HttpFileHeaderMapLrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				HttpFileHeaderMapRrotate(head, node->_Parent->_Parent);
			}
		} else {
			uncle = grandparent->_Left;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Left) {
					node = node->_Parent;
					HttpFileHeaderMapRrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				HttpFileHeaderMapLrotate(head, node->_Parent->_Parent);
			}
		}
	}
	head->_Parent->_Color = 0;
	return head->_Parent;
}

HttpFileHeaderMapNode *HttpFileHeaderMapInsert(PafMap *self,
					    const HttpFileHeaderEntry *value,
					    unsigned char *inserted)
{
	HttpFileHeaderMapNode *parent = self->_Myhead;
	HttpFileHeaderMapNode *node;
	HttpFileHeaderMapNode *entry;
	int side = 0;

	if (!parent)
		return NULL;
	node = parent->_Parent;
	if (inserted)
		*inserted = 0;
	while (!node->_Isnil) {
		parent = node;
		if (sce_paf_strcasecmp(value->first.ptr, node->_Myval.first.ptr) <= 0) {
			if (sce_paf_strcasecmp(node->_Myval.first.ptr, value->first.ptr) <= 0)
				return node;
			node = node->_Right;
			side = 1;
		} else {
			node = node->_Left;
			side = -1;
		}
	}
	node = ScePafStdc_FC5CD359(sizeof(*node));
	if (!node) {
		ScePafStdc_D608246A();
		return self->_Myhead;
	}
	entry = PafPlacementNew(sizeof(*entry), node);
	if (entry) {
		HttpFileHeaderMapNode *head = self->_Myhead;

		entry->_Parent = parent;
		entry->_Left = head;
		entry->_Right = head;
		PafStringInitialize(&entry->_Myval.first);
		PafStringCopy(&entry->_Myval.first, &value->first);
		PafStringVectorConstructCopy(&entry->_Myval.second, &value->second);
		entry->_Isnil = 0;
		entry->_Color = 1;
	}
	if (side < 0) {
		parent->_Left = node;
		if (parent == ((HttpFileHeaderMapNode *)self->_Myhead)->_Left)
			((HttpFileHeaderMapNode *)self->_Myhead)->_Left = node;
	} else if (side > 0) {
		parent->_Right = node;
		if (parent == ((HttpFileHeaderMapNode *)self->_Myhead)->_Right)
			((HttpFileHeaderMapNode *)self->_Myhead)->_Right = node;
	} else {
		HttpFileHeaderMapNode *head = self->_Myhead;

		head->_Parent = node;
		head->_Left = node;
		head->_Right = node;
	}
	++self->_Mysize;
	HttpFileHeaderMapRebalance(self, node);
	if (inserted)
		*inserted = 1;
	return node;
}

static inline __attribute__((always_inline))
void HttpFileHeaderValuesDestroy(PafStringVector *self)
{
	if (self->_data) {
		size_t count = self->_size;

		while (count) {
			--count;
			PafStringDestroy(&self->_data[count]);
		}
		ScePafStdc_1B77082E(self->_data);
	}
	self->_data = (PafString *)-1;
	self->_size = (size_t)-1;
	self->_capacity = (size_t)-1;
}

PafStringVector *HttpFileHeaderMapSubscript(PafMap *self, const PafString *key)
{
	PafStringVector empty;
	HttpFileHeaderEntry value;
	HttpFileHeaderMapNode *node;

	empty._data = NULL;
	empty._size = 0;
	empty._capacity = 0;
	PafStringInitialize(&value.first);
	PafStringCopy(&value.first, key);
	PafStringVectorConstructCopy(&value.second, &empty);
	node = HttpFileHeaderMapInsert(self, &value, NULL);
	HttpFileHeaderValuesDestroy(&value.second);
	PafStringDestroy(&value.first);
	HttpFileHeaderValuesDestroy(&empty);
	return &node->_Myval.second;
}

static inline __attribute__((always_inline))
HttpFileHeaderMapNode *HttpFileHeaderMapFind(const PafMap *self, const PafString *key)
{
	HttpFileHeaderMapNode *node = self->_Myhead;

	if (node) {
		node = node->_Parent;
		while (!node->_Isnil) {
			if (sce_paf_strcasecmp(key->ptr, node->_Myval.first.ptr) <= 0) {
				if (sce_paf_strcasecmp(node->_Myval.first.ptr, key->ptr) <= 0)
					break;
				node = node->_Right;
			} else {
				node = node->_Left;
			}
		}
	}
	return node;
}

static inline __attribute__((always_inline))
void HttpFileHeaderValuesPushBack(PafStringVector *self, const PafString *value)
{
	if ((int32_t)(self->_size + 1) <= (int32_t)self->_capacity ||
	    PafStringVectorReallocate(self, self->_capacity + 10)) {
		PafString *entry = PafPlacementNew(sizeof(*entry),
						 &self->_data[self->_size]);

		if (entry) {
			PafStringInitialize(entry);
			PafStringCopy(entry, value);
		}
		++self->_size;
	}
}

void HttpFileHeaderMapSet(PafMap *self, const char *name, const char *value,
			 unsigned char replace)
{
	PafString key;
	HttpFileHeaderMapNode *node;
	PafStringVector *values;
	PafString text;

	PafStringInitialize(&key);
	PafStringAssign(&key, name, name ? sce_paf_strlen(name) : 0);
	node = HttpFileHeaderMapFind(self, &key);
	PafStringDestroy(&key);
	if (node == self->_Myhead) {
		PafString new_key;

		PafStringInitialize(&new_key);
		PafStringAssign(&new_key, name, name ? sce_paf_strlen(name) : 0);
		values = HttpFileHeaderMapSubscript(self, &new_key);
		PafStringInitialize(&text);
		PafStringAssign(&text, value, value ? sce_paf_strlen(value) : 0);
		HttpFileHeaderValuesPushBack(values, &text);
		PafStringDestroy(&text);
		PafStringDestroy(&new_key);
		return;
	}
	values = &node->_Myval.second;
	if (replace && values->_data) {
		size_t i;

		for (i = 0; i < values->_size; ++i)
			PafStringDestroy(&values->_data[i]);
		ScePafStdc_1B77082E(values->_data);
		values->_data = NULL;
		values->_size = 0;
		values->_capacity = 0;
	}
	PafStringInitialize(&text);
	PafStringAssign(&text, value, value ? sce_paf_strlen(value) : 0);
	HttpFileHeaderValuesPushBack(values, &text);
	PafStringDestroy(&text);
}

PafMap *ScePafMisc_F39DF886(PafMap *self)
{
	return HttpFileHeaderMapClear(self);
}

int32_t HttpFileHeaderMapParseLine(PafMap *self, const char *line, size_t len)
{
	PafString key;
	PafString value;
	HttpFileHeaderMapNode *node;
	PafStringVector *values;
	size_t pos = 0;
	int32_t result = 0;

	if (!line || len < 2)
		return 0;
	PafStringInitialize(&key);
	PafStringInitialize(&value);
	while (pos < len) {
		if (line[pos] == ':') {
			PafStringAssign(&key, line, pos);
			++pos;
			break;
		}
		if (!(sce_paf_look_ctype_table((signed char)line[pos]) & 0x1f) ||
		    (sce_paf_look_ctype_table((signed char)line[pos]) & 8))
			goto out;
		++pos;
	}
	if (!key.count)
		goto out;
	while (pos < len && (sce_paf_look_ctype_table((signed char)line[pos]) & 8))
		++pos;
	PafStringAssign(&value, line + pos, len - pos);
	node = HttpFileHeaderMapFind(self, &key);
	if (node == self->_Myhead)
		values = HttpFileHeaderMapSubscript(self, &key);
	else
		values = &node->_Myval.second;
	HttpFileHeaderValuesPushBack(values, &value);
	result = 1;
out:
	PafStringDestroy(&value);
	PafStringDestroy(&key);
	return result;
}

uintptr_t HttpFileHeaderMapEraseRebalance(PafMap *self,
					HttpFileHeaderMapNode *node,
					HttpFileHeaderMapNode *parent)
{
	HttpFileHeaderMapNode *head = self->_Myhead;
	uintptr_t result = (uintptr_t)self;

	if (!head)
		return result;
	for (;;) {
		HttpFileHeaderMapNode *sibling;

		result = (int32_t)(signed char)parent->_Isnil;
		if (result)
			break;
		result = (int32_t)(signed char)node->_Color;
		if (result)
			break;
		if (node == parent->_Left) {
			sibling = parent->_Right;
			if (sibling->_Color) {
				sibling->_Color = 0;
				parent->_Color = 1;
				sibling = sibling->_Left;
				HttpFileHeaderMapLrotate(head, parent);
			}
			if (sibling->_Left->_Color || sibling->_Right->_Color) {
				if (!sibling->_Right->_Color) {
					HttpFileHeaderMapNode *left = sibling->_Left;

					left->_Color = 0;
					sibling->_Color = 1;
					HttpFileHeaderMapRrotate(head, sibling);
					sibling = left;
				}
				sibling->_Color = parent->_Color;
				parent->_Color = 0;
				sibling->_Right->_Color = 0;
				HttpFileHeaderMapLrotate(head, parent);
				node = head->_Parent;
				parent = node->_Parent;
			} else {
				node = parent;
				sibling->_Color = 1;
				parent = parent->_Parent;
			}
		} else {
			sibling = parent->_Left;
			if (sibling->_Color) {
				sibling->_Color = 0;
				parent->_Color = 1;
				sibling = sibling->_Right;
				HttpFileHeaderMapRrotate(head, parent);
			}
			if (sibling->_Left->_Color || sibling->_Right->_Color) {
				if (!sibling->_Left->_Color) {
					HttpFileHeaderMapNode *right = sibling->_Right;

					right->_Color = 0;
					sibling->_Color = 1;
					HttpFileHeaderMapLrotate(head, sibling);
					sibling = right;
				}
				sibling->_Color = parent->_Color;
				parent->_Color = 0;
				sibling->_Left->_Color = 0;
				HttpFileHeaderMapRrotate(head, parent);
				node = head->_Parent;
				parent = node->_Parent;
			} else {
				node = parent;
				sibling->_Color = 1;
				parent = parent->_Parent;
			}
		}
	}
	node->_Color = 0;
	return result;
}

uintptr_t HttpFileHeaderMapEraseNode(PafMap *self, HttpFileHeaderMapNode *node)
{
	HttpFileHeaderMapNode *head = self->_Myhead;
	HttpFileHeaderMapNode *successor;
	HttpFileHeaderMapNode *parent;
	HttpFileHeaderMapNode *fixparent;
	HttpFileHeaderMapNode *fixnode;
	unsigned char black;

	if (!head)
		return (uintptr_t)self;
	if (node->_Left->_Isnil || node->_Right->_Isnil) {
		successor = node;
		parent = node->_Parent;
		fixparent = node->_Parent;
	} else {
		successor = node->_Right;
		if (successor->_Left->_Isnil) {
			fixparent = successor;
		} else {
			while (!successor->_Left->_Isnil)
				successor = successor->_Left;
			fixparent = successor->_Parent;
		}
		parent = successor->_Parent;
	}
	if (successor->_Left->_Isnil)
		fixnode = successor->_Right;
	else
		fixnode = successor->_Left;
	if (!fixnode->_Isnil) {
		fixnode->_Parent = parent;
		parent = successor->_Parent;
	}
	if (parent->_Isnil)
		head->_Parent = fixnode;
	else if (parent->_Left == successor)
		parent->_Left = fixnode;
	else
		parent->_Right = fixnode;
	black = !successor->_Color;
	if (successor != node) {
		successor->_Parent = node->_Parent;
		if (node->_Parent->_Isnil)
			head->_Parent = successor;
		else if (node->_Parent->_Left == node)
			node->_Parent->_Left = successor;
		else
			node->_Parent->_Right = successor;
		successor->_Left = node->_Left;
		if (!node->_Left->_Isnil)
			node->_Left->_Parent = successor;
		successor->_Right = node->_Right;
		if (!node->_Right->_Isnil)
			node->_Right->_Parent = successor;
		successor->_Color = node->_Color;
	}
	if (black) {
		HttpFileHeaderMapEraseRebalance(self, fixnode, fixparent);
		head = self->_Myhead;
	}
	if (head->_Left == node) {
		HttpFileHeaderMapNode *first = fixparent;

		if (!fixnode->_Isnil) {
			first = fixnode;
			while (!first->_Left->_Isnil)
				first = first->_Left;
		}
		head->_Left = first;
	}
	if (head->_Right == node) {
		if (!fixnode->_Isnil) {
			fixparent = fixnode;
			while (!fixparent->_Right->_Isnil)
				fixparent = fixparent->_Right;
		}
		head->_Right = fixparent;
	}
	HttpFileHeaderValuesDestroy(&node->_Myval.second);
	PafStringDestroy(&node->_Myval.first);
	ScePafStdc_1B77082E(node);
	return --self->_Mysize;
}

uintptr_t HttpFileHeaderMapErase(PafMap *self, HttpFileHeaderMapNode *const *where)
{
	HttpFileHeaderMapNode *node = *where;
	uintptr_t result = (uintptr_t)self;

	if (!node || !self->_Myhead)
		return result;
	node = HttpFileHeaderMapFind(self, &node->_Myval.first);
	if (node) {
		result = (int32_t)(signed char)node->_Isnil;
		if (!result)
			result = HttpFileHeaderMapEraseNode(self, node);
	}
	return result;
}

int32_t HttpFileHeaderMapRemove(PafMap *self, const char *name)
{
	PafString key;
	HttpFileHeaderMapNode *node;

	PafStringInitialize(&key);
	PafStringAssign(&key, name, name ? sce_paf_strlen(name) : 0);
	node = HttpFileHeaderMapFind(self, &key);
	PafStringDestroy(&key);
	if (node == self->_Myhead)
		return 0;
	HttpFileHeaderMapErase(self, &node);
	return 1;
}
