#include "httpfile.h"
#include "httpfile_reader.h"
#include "../common/main_thread_call.h"
#include "../paf_types.h"
#include "../std/new.h"
#include "../std/stdlib.h"
#include "../std/ctype.h"
#include <libhttp.h>
#include <kernel/iofilemgr.h>

int ScePafMisc_104CC765(uint8_t *self)
{
	MainThreadCall *callback = *(MainThreadCall **)(self + 4);
	int32_t request;
	int32_t connection;

	if (callback) {
		PafDelete(MainThreadCallDestroy(callback));
		*(void **)(self + 4) = 0;
	}
	request = *(int32_t *)(self + 12);
	if (request > 0) {
		sceHttpDeleteRequest(request);
		*(int32_t *)(self + 12) = -1;
	}
	connection = *(int32_t *)(self + 8);
	if (connection > 0) {
		sceHttpDeleteConnection(connection);
		*(int32_t *)(self + 8) = -1;
	}
	PafStringAssign((PafString *)(self + 20), NULL, 0);
	return 0;
}

uint8_t *ScePafMisc_947EDCC9(uint8_t *self)
{
	MainThreadCall *callback = *(MainThreadCall **)(self + 4);
	int32_t request;
	int32_t connection;
	PafString *name = (PafString *)(self + 20);
	char *ptr;

	if (callback) {
		PafDelete(MainThreadCallDestroy(callback));
		*(void **)(self + 4) = 0;
	}
	request = *(int32_t *)(self + 12);
	if (request > 0) {
		sceHttpDeleteRequest(request);
		*(int32_t *)(self + 12) = -1;
	}
	connection = *(int32_t *)(self + 8);
	if (connection > 0) {
		sceHttpDeleteConnection(connection);
		*(int32_t *)(self + 8) = -1;
	}
	PafStringAssign(name, 0, 0);
	ptr = name->ptr;
	if (ptr != &PafStringEmpty)
		PafDelete(ptr);
	name->ptr = (char *)-1;
	name->count = (size_t)-1;
	return self;
}

int ScePafMisc_0A5D5353(const uint8_t *self, const char *name,
			 PafString *resultString)
{
	char *headers;
	SceSize headerSize;
	const char *value;
	SceSize valueLength;
	int result = sceHttpGetAllResponseHeaders(
		*(const int32_t *)(self + 12), &headers, &headerSize);

	if (result >= 0) {
		result = sceHttpParseResponseHeader(headers, headerSize,
						name, &value, &valueLength);
		if (result >= 0) {
			PafStringAssign(resultString, value, valueLength);
			result = 0;
		}
	}
	return result;
}

extern int sce_paf_strcasecmp(const char *left, const char *right);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern int sce_paf_strcmp(const char *left, const char *right);
extern int sce_paf_snprintf(char *dst, size_t max, const char *format, ...);
extern int sce_paf_memcmp(const void *left, const void *right, size_t len);
extern void *ScePafStdc_75ECC54E(void *dst, int value, size_t len);
extern int32_t SceHttp_43A7CC70(int32_t id, uint32_t flags);
extern int32_t SceHttp_F30073EC(int32_t id, uint32_t flags);

static unsigned char httpFileHttpReaderSeekBuffer[1024];

int32_t HttpFileHeaderMapForEach(const PafMap *self, HttpFileHeaderCallback callback,
			       void *userdata)
{
	HttpFileHeaderMapNode *head;
	HttpFileHeaderMapNode *node;

	if (!callback)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	head = self->_Myhead;
	node = head ? head->_Left : NULL;
	while (node != head) {
		int32_t count = node->_Myval.second._size;
		int32_t i;

		for (i = 0; i < count; ++i) {
			int32_t result;

			result = callback(node->_Myval.first.ptr,
					  node->_Myval.second._data[i].ptr,
					  userdata);
			if (result)
				return result;
		}
		if (count > 0)
			head = self->_Myhead;
		if (node) {
			HttpFileHeaderMapNode *next = node->_Right;

			if (next->_Isnil) {
				next = node->_Parent;
				while (!next->_Isnil) {
					if (next->_Left == node)
						break;
					node = next;
					next = next->_Parent;
				}
			} else {
				HttpFileHeaderMapNode *left = next->_Left;

				while (!left->_Isnil) {
					next = left;
					left = left->_Left;
				}
			}
			node = next;
		}
	}
	return 0;
}

int32_t HttpFileHeaderMapParse(PafMap *self, const char *header, size_t len)
{
	size_t pos = 0;
	size_t start = 0;
	int32_t errors = 0;
	unsigned char terminated = 0;

	HttpFileHeaderMapClear(self);
	if (!header)
		return 0;
	if (len > 1) {
		do {
			terminated = 0;
			if (header[pos] == '\r' && header[pos + 1] == '\n') {
				if (pos + 2 < len &&
				    (header[pos + 2] == ' ' || header[pos + 2] == '\t')) {
					pos += 3;
				} else {
					if (start < pos &&
					    HttpFileHeaderMapParseLine(self, header + start,
							       pos - start))
						errors = (uint32_t)errors + 1;
					pos += 2;
					start = pos;
					terminated = 1;
				}
			} else {
				++pos;
			}
		} while (pos + 1 < len);
	}
	if (!terminated && start < len &&
	    HttpFileHeaderMapParseLine(self, header + start, len - start))
		errors = (uint32_t)errors + 1;
	return errors;
}

int32_t HttpFileHttpReaderAddHeader(const char *name, const char *value,
				  void *userdata)
{
	HttpFileRequestHeaderParam *param = userdata;

	if (!param)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	return sceHttpAddRequestHeader(param->id, name, value, param->mode);
}

static inline __attribute__((always_inline))
void HttpFileReaderConnectionSharedPtrRelease(const HttpFileReaderConnectionSharedPtr *self)
{
	if (self->m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_counter) == 1) {
		self->m_ref_counter->m_deleter(self->m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->m_ref_counter);
	}
}

static inline __attribute__((always_inline))
void HttpFileReaderConnectionSharedPtrAssign(HttpFileReaderConnectionSharedPtr *self,
					   const HttpFileReaderConnectionSharedPtr *src)
{
	HttpFileReaderConnection *ptr = src->m_ptr;
	HttpFileReaderConnectionRefCounter *counter = src->m_ref_counter;

	if (ptr != self->m_ptr) {
		HttpFileReaderConnectionSharedPtrRelease(self);
		self->m_ptr = ptr;
		self->m_ref_counter = counter;
		if (ptr)
			sceAtomicIncrement32AcqRel(&counter->m_counter);
	}
}

HttpFileReaderConnection *HttpFileReaderConnectionDestroy(HttpFileReaderConnection *self)
{
	if (self->m_connection_id > 0)
		sceHttpDeleteConnection(self->m_connection_id);
	self->m_connection_id = 0;
	if (self->m_template_id > 0)
		sceHttpDeleteTemplate(self->m_template_id);
	self->m_template_id = 0;
	PafStringDestroy(&self->param.scheme);
	PafStringDestroy(&self->param.host);
	PafStringDestroy(&self->param.user_agent);
	return self;
}

HttpFileReaderConnection *HttpFileReaderConnectionConstruct(HttpFileReaderConnection *self,
							 const HttpFileReaderConnectionParam *param)
{
	PafStringInitialize(&self->param.user_agent);
	PafStringInitialize(&self->param.host);
	PafStringInitialize(&self->param.scheme);
	self->param.http_version = param->http_version;
	PafStringCopy(&self->param.user_agent, &param->user_agent);
	PafStringCopy(&self->param.host, &param->host);
	PafStringCopy(&self->param.scheme, &param->scheme);
	self->param.https_flag = param->https_flag;
	self->param.auto_proxy = param->auto_proxy;
	self->param.port = param->port;
	self->param.template_option = param->template_option;
	self->param.enable_keepalive = param->enable_keepalive;
	self->param.response_header_max_size = param->response_header_max_size;
	self->m_template_id = 0;
	self->m_connection_id = 0;
	self->m_https_flag = HTTPS_FLAG_DEFAULT & ~HTTPS_FLAG_CLIENT_VERIFY;
	return self;
}

void HttpFileReaderConnectionDelete(HttpFileReaderConnection *self)
{
	if (self) {
		HttpFileReaderConnectionDestroy(self);
		ScePafStdc_1B77082E(self);
	}
}

int32_t HttpFileReaderConnectionCreateHandles(const HttpFileReaderConnectionParam *param,
					    int32_t *template_id, int32_t *connection_id)
{
	int32_t id;
	int32_t result;
	uint32_t enabled;
	uint32_t disabled;

	id = sceHttpCreateTemplate(param->user_agent.ptr, param->http_version,
				   param->auto_proxy);
	if (id <= 0)
		return id;
	*template_id = id;
	if (param->template_option) {
		result = SceHttp_43A7CC70(id, param->template_option);
		if (result < 0)
			return result;
		id = *template_id;
	}
	if (param->response_header_max_size) {
		result = sceHttpSetResponseHeaderMaxSize(id,
						      param->response_header_max_size);
		if (result < 0)
			return result;
		id = *template_id;
	}
	id = sceHttpCreateConnection(id, param->host.ptr, param->scheme.ptr,
				     param->port, param->enable_keepalive);
	if (id <= 0) {
		if (*template_id > 0)
			sceHttpDeleteTemplate(*template_id);
		*template_id = 0;
		return id;
	}
	*connection_id = id;
	enabled = param->https_flag & HTTPS_FLAG_MASK;
	disabled = ~param->https_flag & HTTPS_FLAG_MASK;
	result = sceHttpsEnableOption2(id, enabled);
	if (result < 0)
		return result;
	result = sceHttpsDisableOption2(*connection_id, disabled);
	if (result < 0)
		return result;
	return 0;
}

int32_t HttpFileReaderContextGetConnection(const HttpFileReaderContextSharedPtr *self,
					 const HttpFileReaderConnectionParam *param,
					 HttpFileReaderConnectionSharedPtr *result)
{
	HttpFileReaderConnectionSharedPtr connection;
	int32_t template_id = 0;
	int32_t connection_id = 0;
	int32_t error;

	if (self->m_ptr && param->enable_keepalive) {
		Mutex *mutex = &self->m_ptr->m_mutex;
		HttpFileReaderConnectionListNode *root;
		HttpFileReaderConnectionListNode *node;

		MutexLock(mutex);
		root = self->m_ptr->m_connections.root_node;
		node = root ? root->next : NULL;
		while (node != root) {
			HttpFileReaderConnection *ptr = node->val.m_ptr;
			const HttpFileReaderConnectionParam *cached;

			if (ptr) {
				cached = &ptr->param;
				if (cached->port == param->port &&
				    *cached->host.ptr == *param->host.ptr &&
				    !sce_paf_strcmp(cached->host.ptr, param->host.ptr) &&
				    *cached->scheme.ptr == *param->scheme.ptr &&
				    !sce_paf_strcmp(cached->scheme.ptr, param->scheme.ptr) &&
				    *cached->user_agent.ptr == *param->user_agent.ptr &&
				    !sce_paf_strcmp(cached->user_agent.ptr, param->user_agent.ptr) &&
				    cached->http_version == param->http_version &&
				    cached->auto_proxy == param->auto_proxy &&
				    cached->https_flag == param->https_flag &&
				    cached->template_option == param->template_option &&
				    cached->response_header_max_size == param->response_header_max_size) {
					HttpFileReaderConnectionListNode *next;

					HttpFileReaderConnectionSharedPtrAssign(result, &node->val);
					HttpFileReaderConnectionListErase(&next,
							&self->m_ptr->m_connections, &node);
					MutexUnlock(mutex);
					return 0;
				}
			}
			if (node)
				node = node->next;
			root = self->m_ptr->m_connections.root_node;
		}
		MutexUnlock(mutex);
	}
	error = HttpFileReaderConnectionCreateHandles(param, &template_id,
						      &connection_id);
	if (error)
		return error;
	connection.m_ptr = ScePafStdc_FC5CD359(sizeof(*connection.m_ptr));
	if (connection.m_ptr)
		HttpFileReaderConnectionConstruct(connection.m_ptr, param);
	connection.m_ref_counter = NULL;
	if (connection.m_ptr) {
		HttpFileReaderConnectionRefCounter *counter;

		counter = ScePafStdc_FC5CD359(sizeof(*counter));
		if (counter) {
			counter->m_ptr = connection.m_ptr;
			counter->m_counter = 1;
			counter->m_weak_counter = 1;
			counter->m_deleter = HttpFileReaderConnectionDelete;
			connection.m_ref_counter = counter;
		} else {
			connection.m_ptr = NULL;
		}
	}
	HttpFileReaderConnectionSharedPtrAssign(result, &connection);
	HttpFileReaderConnectionSharedPtrRelease(&connection);
	if (result->m_ptr) {
		result->m_ptr->m_template_id = template_id;
		result->m_ptr->m_connection_id = connection_id;
		return 0;
	}
	if (connection_id > 0)
		sceHttpDeleteConnection(connection_id);
	if (template_id > 0) {
		connection_id = 0;
		sceHttpDeleteTemplate(template_id);
	}
	return SCE_PAF_ERROR_FILE_NO_MEMORY;
}

uintptr_t HttpFileReaderConnectionListPushBack(PafList *self,
					    const HttpFileReaderConnectionSharedPtr *value)
{
	HttpFileReaderConnectionListNode *node;
	HttpFileReaderConnectionListNode *root = self->root_node;
	uintptr_t result = (uintptr_t)root;

	if (!root)
		return result;
	node = ScePafStdc_FC5CD359(sizeof(*node));
	if (!node)
		ScePafStdc_D608246A();
	if (node) {
		HttpFileReaderConnectionSharedPtr *ptr;

		ptr = PafPlacementNew(sizeof(*ptr), &node->val);
		if (ptr) {
			HttpFileReaderConnection *connection = value->m_ptr;
			HttpFileReaderConnectionRefCounter *counter;

			ptr->m_ptr = connection;
			counter = value->m_ref_counter;
			ptr->m_ref_counter = counter;
			if (connection)
				sceAtomicIncrement32AcqRel(&counter->m_counter);
		}
	} else {
		node = self->root_node;
	}
	root = self->root_node;
	result = (uintptr_t)root;
	if (node != root) {
		node->prev = root->prev;
		node->next = self->root_node;
		((HttpFileReaderConnectionListNode *)self->root_node)->prev->next = node;
		result = (uint32_t)self->count + 1;
		((HttpFileReaderConnectionListNode *)self->root_node)->prev = node;
		self->count = result;
	}
	return result;
}

int32_t HttpFileReaderConnectionListPopFront(PafList *self)
{
	int32_t result = self->count;

	if (result && self->root_node) {
		HttpFileReaderConnectionListNode *root = self->root_node;
		HttpFileReaderConnectionListNode *node = root->next;
		HttpFileReaderConnectionListNode *prev = node->prev;

		node->next->prev = prev;
		prev->next = node->next;
		HttpFileReaderConnectionSharedPtrRelease(&node->val);
		ScePafStdc_1B77082E(node);
		result = (uint32_t)self->count - 1;
		self->count = result;
	}
	return result;
}

int32_t HttpFileReaderConnectionListErase(HttpFileReaderConnectionListNode **result,
					PafList *self,
					HttpFileReaderConnectionListNode *const *where)
{
	HttpFileReaderConnectionListNode *node = *where;
	HttpFileReaderConnectionListNode *next = node->next;
	int32_t count;

	next->prev = node->prev;
	node->prev->next = node->next;
	HttpFileReaderConnectionSharedPtrRelease(&node->val);
	ScePafStdc_1B77082E(node);
	count = (uint32_t)self->count - 1;
	self->count = count;
	*result = next;
	return count;
}

PafList *HttpFileReaderConnectionListDestroy(PafList *self)
{
	HttpFileReaderConnectionListNode *root = self->root_node;

	if (root) {
		HttpFileReaderConnectionListNode *node = root->next;

		while (node != root) {
			HttpFileReaderConnectionListNode *next = node->next;

			HttpFileReaderConnectionSharedPtrRelease(&node->val);
			ScePafStdc_1B77082E(node);
			root = self->root_node;
			node = next;
		}
		ScePafStdc_1B77082E(root);
	}
	self->root_node = (void *)-1;
	self->count = -1;
	return self;
}

HttpFileReaderContext *HttpFileReaderContextConstruct(HttpFileReaderContext *self,
						   size_t capacity)
{
	HttpFileReaderConnectionListNode *root;

	self->m_capacity = capacity;
	self->m_connections.count = 0;
	root = ScePafStdc_FC5CD359(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->m_connections.root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	_ZN3paf6thread5MutexC1EPKci(&self->m_mutex, "ScePafHttpConnectionPool", 0);
	return self;
}

void HttpFileReaderContextDelete(HttpFileReaderContext *self)
{
	if (self) {
		_ZN3paf6thread5MutexD1Ev(&self->m_mutex);
		HttpFileReaderConnectionListDestroy(&self->m_connections);
		ScePafStdc_1B77082E(self);
	}
}

HttpFileReaderContextRefCounter *HttpFileReaderContextCreate(HttpFileReaderContextSharedPtr *result,
							 size_t capacity)
{
	HttpFileReaderContext *context = ScePafStdc_FC5CD359(sizeof(*context));
	HttpFileReaderContextRefCounter *counter = NULL;

	if (context)
		HttpFileReaderContextConstruct(context, capacity);
	result->m_ptr = context;
	if (context) {
		counter = ScePafStdc_FC5CD359(sizeof(*counter));
		if (counter) {
			result->m_ref_counter = counter;
			counter->m_ptr = context;
			counter = result->m_ref_counter;
			counter->m_counter = 1;
			counter->m_weak_counter = 1;
			counter->m_deleter = HttpFileReaderContextDelete;
		} else {
			result->m_ptr = NULL;
			result->m_ref_counter = NULL;
		}
	} else {
		result->m_ref_counter = NULL;
	}
	return counter;
}

int32_t HttpFileReaderContextReturnConnection(const HttpFileReaderContextSharedPtr *self,
					    HttpFileReaderConnectionSharedPtr *connection)
{
	HttpFileReaderContext *context = self->m_ptr;

	if (context && connection->m_ptr &&
	    connection->m_ptr->param.enable_keepalive && context->m_capacity) {
		Mutex *mutex = &context->m_mutex;

		_ZN3paf6thread5Mutex4LockEv(mutex);
		while ((uint32_t)self->m_ptr->m_connections.count >=
		       self->m_ptr->m_capacity)
			HttpFileReaderConnectionListPopFront(&self->m_ptr->m_connections);
		HttpFileReaderConnectionListPushBack(&self->m_ptr->m_connections,
						    connection);
		_ZN3paf6thread5Mutex6UnlockEv(mutex);
	}
	HttpFileReaderConnectionSharedPtrRelease(connection);
	connection->m_ptr = NULL;
	connection->m_ref_counter = NULL;
	return 0;
}

int32_t HttpFileHttpReaderRead(HttpFileHttpReader *self, void *buf, size_t nbyte)
{
	int32_t request_id;
	int32_t result;
	int32_t error;
	uint32_t detail;

	RWLockLockShared(&self->m_lock);
	request_id = self->m_request_id;
	RWLockUnlockShared(&self->m_lock);
	result = sceHttpReadData(request_id, buf, nbyte);
	if (result == SCE_HTTP_ERROR_SSL) {
		self->base.m_result[1] = sceHttpsGetSslError(request_id, &error,
							 &detail);
		if (!self->base.m_result[1]) {
			self->base.m_result[1] = error;
			self->base.m_result[2] = detail;
		}
	} else {
		self->base.m_result[1] = 0;
		self->base.m_result[2] = 0;
	}
	if (result >= 0)
		self->m_position = (uint64_t)self->m_position + result;
	return result;
}

int64_t HttpFileHttpReaderSeek(HttpFileHttpReader *self, int64_t offset,
			       int32_t whence)
{
	HttpFileReader *reader = &self->base;
	int64_t remaining = offset;
	int32_t result;

	if (whence == SCE_SEEK_END) {
		int64_t length = self->m_content_length;

		if (length < 0)
			return SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
		remaining = (uint64_t)offset + length - self->m_position;
	} else if (whence == SCE_SEEK_SET) {
		remaining = (uint64_t)offset - self->m_position;
	}
	if (remaining < 0) {
		int64_t position = (uint64_t)remaining + self->m_position;

		if (position < 0)
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		reader->vtable->Close(reader);
		result = reader->vtable->Open(reader);
		if (result)
			return result;
		return reader->vtable->Seek(reader, position, SCE_SEEK_SET);
	}
	while (remaining > 0) {
		size_t size = sizeof(httpFileHttpReaderSeekBuffer);

		if (remaining < size)
			size = (size_t)remaining;
		result = reader->vtable->Read(reader,
					   httpFileHttpReaderSeekBuffer, size);
		if (result < 0)
			return result;
		if (!result)
			return self->m_position;
		remaining = (uint64_t)remaining - result;
	}
	return self->m_position;
}

int32_t HttpFileHttpReaderAbort(HttpFileHttpReader *self)
{
	int32_t request_id;
	int32_t result;

	RWLockLockExclusive(&self->m_lock);
	request_id = self->m_request_id;
	self->m_aborted = 1;
	result = sceHttpAbortRequest(request_id);
	RWLockUnlockExclusive(&self->m_lock);
	return result;
}

int64_t HttpFileHttpReaderGetFileSize(HttpFileHttpReader *self)
{
	int64_t length = self->m_content_length;

	if (length >= 0)
		return length;
	return SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
}

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

static inline __attribute__((always_inline))
void HttpFileReaderParamDelete(HttpFileReaderParam *param)
{
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
}

int32_t HttpFileDeleteReader(HttpFile *self)
{
	HttpFileReaderParam *param;

	if (self->m_http_reader)
		self->m_http_reader->vtable->Delete(self->m_http_reader);
	param = self->m_reader_param;
	self->m_http_reader = NULL;
	HttpFileReaderParamDelete(param);
	self->m_reader_param = NULL;
	return 0;
}

void HttpFileOpenUrl(FileSharedPtr *result, Url *url, int32_t *error, uint32_t flag)
{
	HttpFile *self;

	if (!url) {
		/* The native null-URL path requires an error output. */
		*error = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (self) {
		HttpFileOpenArg arg;
		int32_t ret;

		HttpFileConstruct(self);
		HttpFileOpenArgConstruct(&arg);
		HttpFileOpenArgParseUrl(&arg, url);
		arg.open_flag = flag;
		ret = self->base.vtable->Open(&self->base, (const FileOpenArg *)&arg);
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
				result->m_ref_counter->m_deleter = HttpFileDefaultDelete;
			} else {
				/* Native allocation failure leaves the opened file alive. */
				result->m_ptr = NULL;
				result->m_ref_counter = NULL;
			}
		} else {
			self->base.vtable->Delete(&self->base);
			result->m_ptr = NULL;
			result->m_ref_counter = NULL;
		}
		HttpFileOpenArgDestroyMembers(&arg);
		return;
	}
	if (error)
		*error = SCE_PAF_ERROR_FILE_NO_MEMORY;
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void HttpFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		  uint32_t mode, int32_t *error)
{
	PafString value;
	Url url;

	(void)mode;
	PafStringInitialize(&value);
	PafStringAssign(&value, path, path ? sce_paf_strlen(path) : 0);
	UrlConstructString(&url, &value);
	PafStringDestroy(&value);
	HttpFileOpenUrl(result, &url, error, flag);
	UrlDestroy(&url);
}

void HttpFileOpenChar(FileSharedPtr *result, const char *path, int32_t *error,
		      uint32_t flag)
{
	PafString value;
	Url url;

	PafStringInitialize(&value);
	PafStringAssign(&value, path, path ? sce_paf_strlen(path) : 0);
	UrlConstructString(&url, &value);
	PafStringDestroy(&value);
	HttpFileOpenUrl(result, &url, error, flag);
	UrlDestroy(&url);
}

void HttpFileOpenWide(FileSharedPtr *result, const uint16_t *path, int32_t *error,
		      uint32_t flag)
{
	PafWString value;
	Url url;

	PafWStringInitialize(&value);
	PafWStringAssign(&value, path, path ? sce_paf_wcslen(path) : 0);
	UrlConstructWString(&url, &value);
	PafWStringDestroy(&value);
	HttpFileOpenUrl(result, &url, error, flag);
	UrlDestroy(&url);
}

void HttpFileOpenCallback(FileSharedPtr *result, const char *path, uint32_t flag,
			  uint32_t mode, int32_t *error, const void *userdata)
{
	if (!userdata || *(const unsigned char *)userdata) {
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return;
	}
	HttpFileOpen(result, path, flag, mode, error);
}

int32_t HttpFileGetstat(HttpFile *self, FileStat *stat)
{
	int32_t ret;
	int64_t size;
	PafString value;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!stat) {
		ret = SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		goto unlock;
	}
	if (!self->m_is_opened || !self->m_http_reader) {
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto unlock;
	}
	if (self->m_io_thread) {
		ret = SCE_PAF_ERROR_FILE_BUSY;
		goto unlock;
	}
	size = self->base.vtable->GetFileSize(&self->base);
	if (size < 0) {
		ret = (int32_t)size;
		goto unlock;
	}
	stat->st_size = size;
	PafStringInitialize(&value);
	ret = HttpFileGetResponseHeader(self, "Last-Modified", &value);
	if (!ret) {
		if (value.count) {
			DateTime date;

			HttpFileParseDate(&date, value.ptr);
			stat->st_ctime = date;
			stat->st_atime = date;
			stat->st_mtime = date;
			stat->st_attr = 0;
			stat->st_mode = 0;
			DateTimeDestroy(&date);
		} else {
			ret = SCE_PAF_ERROR_FILE_UNKNOWN_DATE;
		}
	}
	PafStringDestroy(&value);
unlock:
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetstatUrl(const char *path, HttpFileStat *stat)
{
	HttpFile *self;
	FileSharedPtr owner;
	FileRefCounter *counter;
	HttpFileOpenArg arg;
	PafString value;
	Url url;
	int32_t ret;

	if (!stat)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	self = ScePafStdc_FC5CD359(sizeof(*self));
	if (!self)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	HttpFileConstruct(self);
	owner.m_ptr = &self->base;
	counter = ScePafStdc_FC5CD359(sizeof(*counter));
	owner.m_ref_counter = counter;
	if (counter) {
		counter->m_ptr = &self->base;
		counter->m_counter = 1;
		counter->m_weak_counter = 1;
		counter->m_deleter = HttpFileDefaultDelete;
	} else {
		/* The native control-block failure leaves the file alive. */
		owner.m_ptr = NULL;
	}
	HttpFileOpenArgConstruct(&arg);
	PafStringInitialize(&value);
	PafStringAssign(&value, path, path ? sce_paf_strlen(path) : 0);
	UrlConstructString(&url, &value);
	PafStringDestroy(&value);
	HttpFileOpenArgParseUrl(&arg, &url);
	UrlDestroy(&url);
	PafStringAssign(&arg.method, "HEAD", sce_paf_strlen("HEAD"));
	ret = self->base.vtable->Open(&self->base, (const FileOpenArg *)&arg);
	if (!ret) {
		ret = self->base.vtable->Getstat(&self->base, (FileStat *)stat);
		self->base.vtable->Close(&self->base);
	}
	HttpFileOpenArgDestroyMembers(&arg);
	FileSharedPtrRelease(&owner);
	return ret;
}

HttpFile *HttpFileConstruct(HttpFile *self)
{
	self->base.vtable = &httpFileClass.methods;
	self->m_is_opened = 0;
	RWLockConstruct(&self->m_lock_rwlock, "ScePafHttpFileRwLock", 0);
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafHttpFileCsLock", 0);
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
	_ZN3paf6thread6RMutexC1EPKci(&self->m_lock_mutex, "ScePafHttpFileCsLock", 0);
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

int32_t HttpFileResponseCodeToError(uint16_t code)
{
	if (code >= 200 && code <= 299)
		return 0;
	switch (code) {
	case 100:
	case 101:
	case 102:
		return HTTPFILE_ERROR_STATUS_100 + code - 100;
	case 300:
	case 301:
	case 302:
	case 303:
	case 304:
	case 305:
	case 307:
		return HTTPFILE_ERROR_STATUS_300 + code - 300;
	case 400:
	case 401:
	case 402:
	case 403:
	case 404:
	case 405:
	case 406:
	case 407:
	case 408:
	case 409:
		return HTTPFILE_ERROR_STATUS_400 + code - 400;
	case 410:
	case 411:
	case 412:
	case 413:
	case 414:
	case 415:
	case 416:
	case 417:
		return HTTPFILE_ERROR_STATUS_410 + code - 410;
	case 422:
	case 423:
	case 424:
	case 426:
		return HTTPFILE_ERROR_STATUS_420 + code - 420;
	case 500:
	case 501:
	case 502:
	case 503:
	case 504:
	case 505:
	case 507:
		return HTTPFILE_ERROR_STATUS_500 + code - 500;
	default:
		return HTTPFILE_ERROR_UNKNOWN_RESPONSE;
	}
}

int32_t HttpFileOpenWithArg(HttpFile *self, const FileOpenArg *param)
{
	const HttpFileOpenArg *arg = (const HttpFileOpenArg *)param;
	HttpFileReader *reader;
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!arg || self->base.vtable->GetFileType(&self->base) != arg->file_type) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (self->m_is_opened) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	}
	if (arg->response_code)
		*arg->response_code = 0;
	RWLockLockExclusive(&self->m_lock_rwlock);
	result = HttpFileCreateReader(self, arg);
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	if (result) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return result;
	}
	RWLockLockShared(&self->m_lock_rwlock);
	result = self->m_http_reader->vtable->Open(self->m_http_reader);
	reader = self->m_http_reader;
	if (reader) {
		int32_t error = reader->m_result[1];

		self->m_reader_result[0] = reader->m_result[0];
		self->m_reader_result[1] = error;
		self->m_reader_result[2] = reader->m_result[2];
	}
	RWLockUnlockShared(&self->m_lock_rwlock);
	if (arg->response_code) {
		reader = self->m_http_reader;
		*arg->response_code = reader->vtable->GetResponse(reader)->code;
	}
	if (!result && !(arg->open_flag & HTTPFILE_OPEN_SKIP_STATUS_CHECK)) {
		reader = self->m_http_reader;
		result = HttpFileResponseCodeToError(reader->vtable->GetResponse(reader)->code);
	}
	RWLockLockExclusive(&self->m_lock_rwlock);
	if (result)
		HttpFileDeleteReader(self);
	else
		self->m_is_opened = 1;
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

const ThreadClass httpFileIoThreadBaseClass = {
	0, NULL,
	{
		HttpFileIoThreadDestroyBase,
		HttpFileIoThreadDeleteBase,
		(void (*)(Thread *))__cxa_pure_virtual
	}
};

const ThreadClass httpFileIoThreadClass = {
	0, NULL,
	{
		HttpFileIoThreadDestroy,
		HttpFileIoThreadDelete,
		HttpFileIoThreadEntryFunction
	}
};

Thread *HttpFileIoThreadDestroyBase(Thread *self)
{
	self->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(self);
	return self;
}

void HttpFileIoThreadDeleteBase(Thread *self)
{
	self->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(self);
	ScePafStdc_1B77082E(self);
}

Thread *HttpFileIoThreadDestroy(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	return thread;
}

void HttpFileIoThreadDelete(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	ScePafStdc_1B77082E(thread);
}

void HttpFileIoThreadEntryFunction(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	const HttpFileIoCall *call = self->m_call;
	int32_t adjustment = call->m_adjustment;
	HttpFileReader *object = (HttpFileReader *)
		((unsigned char *)call->m_object + (adjustment >> 1));
	HttpFileIoFunction function = call->m_method.function;

	if (adjustment & 1)
		function = *(const HttpFileIoFunction *)
			((const unsigned char *)object->vtable + call->m_method.vtable_offset);
	self->m_result = function(object);
}

static inline __attribute__((always_inline))
HttpFileIoThread *HttpFileIoThreadCreate(HttpFileReader *reader, int32_t operation,
				       uintptr_t method)
{
	HttpFileIoThread *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self) {
		HttpFileIoCall *call = ScePafStdc_FC5CD359(sizeof(*call));

		if (call) {
			call->m_object = reader;
			call->m_method.vtable_offset = method;
			call->m_adjustment = 1;
		}
		ThreadConstruct(&self->base, ThreadPriority_Normal, 0x4000,
				"ScePafFileIoThread", NULL);
		self->m_operation = operation;
		self->m_call = call;
		self->m_result = 0;
		self->base.vtable = &httpFileIoThreadClass.methods;
	}
	return self;
}

int32_t HttpFileOpenAsync(HttpFile *self, const FileOpenArg *param)
{
	const HttpFileOpenArg *arg = (const HttpFileOpenArg *)param;
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!arg || self->base.vtable->GetFileType(&self->base) != arg->file_type) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (self->m_is_opened) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_ALREADY_OPENED;
	}
	RWLockLockExclusive(&self->m_lock_rwlock);
	result = HttpFileCreateReader(self, arg);
	if (!result) {
		self->m_io_thread = (Thread *)HttpFileIoThreadCreate(self->m_http_reader,
					0, offsetof(HttpFileReaderVTable, Open));
		RWLockUnlockExclusive(&self->m_lock_rwlock);
		if (!self->m_io_thread) {
			RWLockLockExclusive(&self->m_lock_rwlock);
			HttpFileDeleteReader(self);
			RWLockUnlockExclusive(&self->m_lock_rwlock);
			_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
			return SCE_PAF_ERROR_FILE_NO_MEMORY;
		}
		result = ThreadStart(self->m_io_thread);
		if (result) {
			RWLockLockExclusive(&self->m_lock_rwlock);
			if (self->m_io_thread)
				self->m_io_thread->vtable->Delete(self->m_io_thread);
			self->m_io_thread = NULL;
			HttpFileDeleteReader(self);
			RWLockUnlockExclusive(&self->m_lock_rwlock);
		}
	}
	/* The original keeps the exclusive lock when CreateReader fails. */
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t HttpFileCloseAsync(HttpFile *self)
{
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!self->m_is_opened) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	RWLockLockExclusive(&self->m_lock_rwlock);
	self->m_io_thread = (Thread *)HttpFileIoThreadCreate(self->m_http_reader,
					1, offsetof(HttpFileReaderVTable, Close));
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	if (!self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	result = ThreadStart(self->m_io_thread);
	if (result) {
		RWLockLockExclusive(&self->m_lock_rwlock);
		if (self->m_io_thread)
			self->m_io_thread->vtable->Delete(self->m_io_thread);
		self->m_io_thread = NULL;
		RWLockUnlockExclusive(&self->m_lock_rwlock);
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

const ThreadClass httpFileReadIoThreadClass = {
	0, NULL,
	{
		HttpFileReadIoThreadDestroy,
		HttpFileReadIoThreadDelete,
		HttpFileReadIoThreadEntryFunction
	}
};

const ThreadClass httpFileSeekIoThreadClass = {
	0, NULL,
	{
		HttpFileSeekIoThreadDestroy,
		HttpFileSeekIoThreadDelete,
		HttpFileSeekIoThreadEntryFunction
	}
};

Thread *HttpFileReadIoThreadDestroy(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileReadIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	return thread;
}

void HttpFileReadIoThreadDelete(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileReadIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	ScePafStdc_1B77082E(thread);
}

void HttpFileReadIoThreadEntryFunction(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	const HttpFileReadIoCall *call = self->m_call;
	int32_t adjustment = call->m_adjustment;
	HttpFileReader *object = (HttpFileReader *)
		((unsigned char *)call->m_object + (adjustment >> 1));
	HttpFileReadIoFunction function = call->m_method.function;

	if (adjustment & 1)
		function = *(const HttpFileReadIoFunction *)
			((const unsigned char *)object->vtable + call->m_method.vtable_offset);
	self->m_result = function(object, call->m_buf, call->m_nbyte);
}

Thread *HttpFileSeekIoThreadDestroy(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileSeekIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	return thread;
}

void HttpFileSeekIoThreadDelete(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	void *call = self->m_call;

	thread->vtable = &httpFileSeekIoThreadClass.methods;
	ScePafStdc_1B77082E(call);
	thread->vtable = &httpFileIoThreadBaseClass.methods;
	ThreadDestroyBase(thread);
	ScePafStdc_1B77082E(thread);
}

void HttpFileSeekIoThreadEntryFunction(Thread *thread)
{
	HttpFileIoThread *self = (HttpFileIoThread *)thread;
	const HttpFileSeekIoCall *call = self->m_call;
	int32_t adjustment = call->m_adjustment;
	HttpFileReader *object = (HttpFileReader *)
		((unsigned char *)call->m_object + (adjustment >> 1));
	HttpFileSeekIoFunction function = call->m_method.function;

	if (adjustment & 1)
		function = *(const HttpFileSeekIoFunction *)
			((const unsigned char *)object->vtable + call->m_method.vtable_offset);
	self->m_result = function(object, call->m_offset, call->m_whence);
}

static inline __attribute__((always_inline))
HttpFileIoThread *HttpFileReadIoThreadCreate(HttpFileReader *reader, void *buf, size_t nbyte)
{
	HttpFileIoThread *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self) {
		HttpFileReadIoCall *call = ScePafStdc_FC5CD359(sizeof(*call));

		if (call) {
			call->m_object = reader;
			call->m_method.vtable_offset = offsetof(HttpFileReaderVTable, Read);
			call->m_adjustment = 1;
			call->m_buf = buf;
			call->m_nbyte = nbyte;
		}
		ThreadConstruct(&self->base, ThreadPriority_Normal, 0x4000,
				"ScePafFileIoThread", NULL);
		self->m_operation = 2;
		self->m_call = call;
		self->m_result = 0;
		self->base.vtable = &httpFileReadIoThreadClass.methods;
	}
	return self;
}

static inline __attribute__((always_inline))
HttpFileIoThread *HttpFileSeekIoThreadCreate(HttpFileReader *reader, int64_t offset,
					   int32_t whence)
{
	HttpFileIoThread *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self) {
		HttpFileSeekIoCall *call = ScePafStdc_FC5CD359(sizeof(*call));

		if (call) {
			call->m_object = reader;
			call->m_method.vtable_offset = offsetof(HttpFileReaderVTable, Seek);
			call->m_adjustment = 1;
			call->m_offset = offset;
			call->m_whence = whence;
		}
		ThreadConstruct(&self->base, ThreadPriority_Normal, 0x4000,
				"ScePafFileIoThread", NULL);
		self->m_operation = 4;
		self->m_call = call;
		self->m_result = 0;
		self->base.vtable = &httpFileSeekIoThreadClass.methods;
	}
	return self;
}

int32_t HttpFileReadAsync(HttpFile *self, void *buf, size_t nbyte)
{
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!buf) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	if (self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!self->m_is_opened) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	RWLockLockExclusive(&self->m_lock_rwlock);
	self->m_io_thread = (Thread *)HttpFileReadIoThreadCreate(self->m_http_reader,
							     buf, nbyte);
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	if (!self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	result = ThreadStart(self->m_io_thread);
	if (result) {
		RWLockLockExclusive(&self->m_lock_rwlock);
		if (self->m_io_thread)
			self->m_io_thread->vtable->Delete(self->m_io_thread);
		self->m_io_thread = NULL;
		RWLockUnlockExclusive(&self->m_lock_rwlock);
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t HttpFileSeekAsync(HttpFile *self, int64_t offset, int32_t whence)
{
	int32_t result;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_BUSY;
	}
	if (!self->m_is_opened) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	RWLockLockExclusive(&self->m_lock_rwlock);
	self->m_io_thread = (Thread *)HttpFileSeekIoThreadCreate(self->m_http_reader,
							     offset, whence);
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	if (!self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	result = ThreadStart(self->m_io_thread);
	if (result) {
		RWLockLockExclusive(&self->m_lock_rwlock);
		if (self->m_io_thread)
			self->m_io_thread->vtable->Delete(self->m_io_thread);
		self->m_io_thread = NULL;
		RWLockUnlockExclusive(&self->m_lock_rwlock);
	}
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return result;
}

int32_t HttpFileCompleteAsync(HttpFile *self, int64_t *result)
{
	HttpFileIoThread *thread = (HttpFileIoThread *)self->m_io_thread;
	int64_t value = thread->m_result;
	int32_t operation = thread->m_operation;
	HttpFileReader *reader;

	RWLockLockExclusive(&self->m_lock_rwlock);
	if (self->m_io_thread)
		self->m_io_thread->vtable->Delete(self->m_io_thread);
	reader = self->m_http_reader;
	self->m_io_thread = NULL;
	if (reader) {
		int32_t error = reader->m_result[1];

		self->m_reader_result[0] = reader->m_result[0];
		self->m_reader_result[1] = error;
		self->m_reader_result[2] = reader->m_result[2];
	}
	if (!operation) {
		if (!value && reader)
			value = HttpFileResponseCodeToError(reader->vtable->GetResponse(reader)->code);
		if (value) {
			self->m_is_opened = 0;
			HttpFileDeleteReader(self);
		} else {
			self->m_is_opened = 1;
		}
	}
	RWLockUnlockExclusive(&self->m_lock_rwlock);
	if (result)
		*result = value;
	return 0;
}

int32_t HttpFileWaitAsync(HttpFile *self, int64_t *result)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	ThreadJoin(self->m_io_thread);
	ret = HttpFileCompleteAsync(self, result);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFilePollAsync(HttpFile *self, int64_t *result)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (!self->m_io_thread) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return SCE_PAF_ERROR_FILE_NOT_BUSY;
	}
	if (ThreadIsAlive(self->m_io_thread)) {
		_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
		return 1;
	}
	ret = HttpFileCompleteAsync(self, result);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

unsigned char HttpFileIsOpened(HttpFile *self)
{
	unsigned char opened;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	opened = self->m_is_opened;
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return opened;
}

unsigned char HttpFileIsEof(HttpFile *self)
{
	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int32_t HttpFileFlushAsync(HttpFile *self)
{
	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return SCE_PAF_ERROR_FILE_READ_ONLY;
}

int64_t HttpFileGetFileSize(HttpFile *self)
{
	int64_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
	if (self->m_io_thread)
		ret = SCE_PAF_ERROR_FILE_BUSY;
	else if (self->m_is_opened)
		ret = self->m_http_reader->vtable->GetFileSize(self->m_http_reader);
	else
		ret = SCE_PAF_ERROR_FILE_NOT_OPENED;
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
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

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int64_t HttpFileSeek(HttpFile *self, int64_t offset, int32_t whence)
{
	int64_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseCode(HttpFile *self, uint16_t *code)
{
	int32_t ret;
	unsigned char opened;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseVersion(HttpFile *self, unsigned char *major,
				 unsigned char *minor)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponsePhrase(HttpFile *self, PafString *phrase)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileGetResponseHeader(HttpFile *self, const char *name,
				PafString *value)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
	return ret;
}

int32_t HttpFileClose(HttpFile *self)
{
	int32_t ret;

	_ZN3paf6thread6RMutex4LockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutex6UnlockEv(&self->m_lock_mutex);
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
	_ZN3paf6thread6RMutexD1Ev(&self->m_lock_mutex);
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

const HttpFileReaderClass httpFileReaderClass = {
	0, NULL, {
		HttpFileReaderDestroy, HttpFileReaderDelete,
		(int32_t (*)(HttpFileReader *))__cxa_pure_virtual,
		(int32_t (*)(HttpFileReader *))__cxa_pure_virtual,
		(int32_t (*)(HttpFileReader *, void *, size_t))__cxa_pure_virtual,
		(int64_t (*)(HttpFileReader *, int64_t, int32_t))__cxa_pure_virtual,
		(int32_t (*)(HttpFileReader *))__cxa_pure_virtual,
		(int64_t (*)(HttpFileReader *))__cxa_pure_virtual,
		HttpFileReaderGetResponse, HttpFileReaderGetResponseHeaders
	}
};

static inline __attribute__((always_inline))
void HttpFileReaderConfigSharedPtrRelease(const HttpFileReaderConfigSharedPtr *self)
{
	if (self->m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_counter) == 1) {
		self->m_ref_counter->m_deleter(self->m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->m_ref_counter);
	}
}

static inline __attribute__((always_inline))
HttpFileReader *HttpFileReaderDestroyMembers(HttpFileReader *self)
{
	self->vtable = &httpFileReaderClass.methods;
	HttpFileResponseDestroy(&self->m_response);
	HttpFileHeaderMapClear(&self->m_response_headers);
	if (self->m_response_headers._Myhead)
		ScePafStdc_1B77082E(self->m_response_headers._Myhead);
	self->m_response_headers._Myhead = (void *)-1;
	self->m_response_headers._Mysize = (size_t)-1;
	HttpFileReaderConfigSharedPtrRelease(&self->m_config);
	return self;
}

HttpFileReader *HttpFileReaderDestroy(HttpFileReader *self)
{
	return HttpFileReaderDestroyMembers(self);
}

void HttpFileReaderDelete(HttpFileReader *self)
{
	HttpFileReaderDestroyMembers(self);
	ScePafStdc_1B77082E(self);
}

HttpFileResponse *HttpFileReaderGetResponse(HttpFileReader *self)
{
	return &self->m_response;
}

PafMap *HttpFileReaderGetResponseHeaders(HttpFileReader *self)
{
	return &self->m_response_headers;
}

static inline __attribute__((always_inline))
void HttpFileHeaderMapInitialize(PafMap *self)
{
	HttpFileHeaderMapNode *head;

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
		((HttpFileHeaderMapNode *)self->_Myhead)->_Isnil = 1;
	}
}

static inline __attribute__((always_inline))
void HttpFileReaderParamInitialize(HttpFileReaderParam *self)
{
	PafStringInitialize(&self->scheme);
	PafStringInitialize(&self->host);
	PafStringInitialize(&self->method);
	PafStringInitialize(&self->version);
	PafStringInitialize(&self->uri);
	PafStringInitialize(&self->user);
	PafStringInitialize(&self->passwd);
	HttpFileHeaderMapInitialize(&self->header);
	self->msg_body.m_ptr = NULL;
	self->msg_body.m_ref_counter = NULL;
	self->context.m_ptr = NULL;
	self->context.m_ref_counter = NULL;
}

const HttpFileReaderClass httpFileHttpReaderClass = {
	0, NULL, {
		(HttpFileReader *(*)(HttpFileReader *))HttpFileHttpReaderDestroy,
		(void (*)(HttpFileReader *))HttpFileHttpReaderDelete,
		(int32_t (*)(HttpFileReader *))HttpFileHttpReaderOpen,
		(int32_t (*)(HttpFileReader *))HttpFileHttpReaderClose,
		(int32_t (*)(HttpFileReader *, void *, size_t))HttpFileHttpReaderRead,
		(int64_t (*)(HttpFileReader *, int64_t, int32_t))HttpFileHttpReaderSeek,
		(int32_t (*)(HttpFileReader *))HttpFileHttpReaderAbort,
		(int64_t (*)(HttpFileReader *))HttpFileHttpReaderGetFileSize,
		HttpFileReaderGetResponse, HttpFileReaderGetResponseHeaders
	}
};

HttpFileHttpReader *HttpFileHttpReaderConstruct(HttpFileHttpReader *self,
					     const HttpFileReaderConfigSharedPtr *config)
{
	HttpFileReaderConfigSharedPtr temporary = *config;

	if (temporary.m_ptr)
		sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
	self->base.m_config = temporary;
	self->base.vtable = &httpFileReaderClass.methods;
	if (temporary.m_ptr)
		sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
	HttpFileHeaderMapInitialize(&self->base.m_response_headers);
	HttpFileResponseConstruct(&self->base.m_response);
	self->base.m_result[0] = 0;
	self->base.m_result[1] = 0;
	self->base.m_result[2] = 0;
	HttpFileReaderConfigSharedPtrRelease(&temporary);
	self->base.vtable = &httpFileHttpReaderClass.methods;
	self->m_connection.m_ptr = NULL;
	self->m_connection.m_ref_counter = NULL;
	self->m_request_id = 0;
	self->m_position = 0;
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	RWLockConstruct(&self->m_lock, "ScePafHttpReaderLock", 0);
	self->m_aborted = 0;
	return self;
}

HttpFileHttpReader *HttpFileHttpReaderDestroy(HttpFileHttpReader *self)
{
	HttpFileReaderContextSharedPtr context;
	HttpFileReaderConfig *config;

	self->base.vtable = &httpFileHttpReaderClass.methods;
	RWLockLockExclusive(&self->m_lock);
	if (self->m_request_id > 0)
		sceHttpDeleteRequest(self->m_request_id);
	config = self->base.m_config.m_ptr;
	context.m_ptr = config->context.m_ptr;
	self->m_request_id = 0;
	context.m_ref_counter = config->context.m_ref_counter;
	if (context.m_ptr)
		sceAtomicIncrement32AcqRel(&context.m_ref_counter->m_counter);
	HttpFileReaderContextReturnConnection(&context, &self->m_connection);
	HttpFileReaderContextSharedPtrRelease(&context);
	RWLockUnlockExclusive(&self->m_lock);
	RWLockDestroy(&self->m_lock);
	HttpFileReaderConnectionSharedPtrRelease(&self->m_connection);
	HttpFileReaderDestroyMembers(&self->base);
	return self;
}

void HttpFileHttpReaderDelete(HttpFileHttpReader *self)
{
	HttpFileHttpReaderDestroy(self);
	ScePafStdc_1B77082E(self);
}

int32_t HttpFileHttpReaderOpen(HttpFileHttpReader *self)
{
	HttpFileReaderConfig *config;
	HttpFileReaderContextSharedPtr context;
	HttpFileReaderConnectionParam param;
	HttpFileRequestHeaderParam request_header;
	PafString user_agent;
	PafString referer;
	const char *value;
	const void *body = NULL;
	size_t body_size = 0;
	int32_t version = SCE_HTTP_VERSION_1_0;
	int32_t result;
	int32_t request_id;
	int32_t error;
	uint32_t detail;
	uint32_t request_flags = 0;
	char *header;
	size_t header_size;
	uint64_t content_length;

	RWLockLockExclusive(&self->m_lock);
	if (self->m_request_id > 0)
		sceHttpDeleteRequest(self->m_request_id);
	self->m_request_id = 0;
	HttpFileReaderConnectionSharedPtrRelease(&self->m_connection);
	self->m_connection.m_ptr = NULL;
	self->m_connection.m_ref_counter = NULL;
	RWLockUnlockExclusive(&self->m_lock);
	config = self->base.m_config.m_ptr;
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	self->m_position = 0;
	PafStringInitialize(&user_agent);
	PafStringInitialize(&referer);
	value = HttpFileHeaderMapGet(&config->header, "UserAgent", 0);
	PafStringAssign(&user_agent, value, value ? sce_paf_strlen(value) : 0);
	HttpFileHeaderMapRemove(&config->header, "UserAgent");
	value = HttpFileHeaderMapGet(&config->header, "Referer", 0);
	PafStringAssign(&referer, value, value ? sce_paf_strlen(value) : 0);
	HttpFileHeaderMapRemove(&config->header, "Referer");
	if (sce_paf_strcmp("HTTP/1.0", config->version)) {
		sce_paf_strcmp("HTTP/1.1", config->version);
		version = SCE_HTTP_VERSION_1_1;
	}
	if (config->msg_body.m_ptr) {
		Buffer *buffer = config->msg_body.m_ptr;

		body = buffer->vtable->GetConstAddr(buffer);
		buffer = config->msg_body.m_ptr;
		body_size = buffer->vtable->GetSize(buffer);
	}
	RWLockLockExclusive(&self->m_lock);
	if (self->m_aborted) {
		self->m_aborted = 0;
		RWLockUnlockExclusive(&self->m_lock);
		result = SCE_PAF_ERROR_FILE_NOT_OPENED;
		goto out_strings;
	}
	PafStringInitialize(&param.user_agent);
	PafStringInitialize(&param.host);
	PafStringInitialize(&param.scheme);
	sce_paf_memset(&param, 0, sizeof(param));
	param.http_version = version;
	PafStringCopy(&param.user_agent, &user_agent);
	value = config->host;
	PafStringAssign(&param.host, value, value ? sce_paf_strlen(value) : 0);
	value = config->scheme;
	PafStringAssign(&param.scheme, value, value ? sce_paf_strlen(value) : 0);
	param.port = config->port;
	param.auto_proxy = !HttpFileReaderConfigIsProxyIgnored(config);
	param.enable_keepalive = HttpFileReaderConfigIsHttpsCheckEnabled(config);
	param.https_flag = config->https_flag;
	param.template_option = config->template_option;
	param.response_header_max_size = config->response_header_max_size;
	context = config->context;
	if (context.m_ptr)
		sceAtomicIncrement32AcqRel(&context.m_ref_counter->m_counter);
	result = HttpFileReaderContextGetConnection(&context, &param,
						  &self->m_connection);
	HttpFileReaderContextSharedPtrRelease(&context);
	if (result)
		goto out_param_error;
	self->m_request_id = sceHttpCreateRequestWithURL2(
		self->m_connection.m_ptr->m_connection_id, config->method,
		config->uri, (uint64_t)body_size);
	if (self->m_request_id <= 0) {
		result = self->m_request_id;
		goto out_param_error;
	}
	if (config->open_flag & SUPPRESS_3G_ACCESS)
		request_flags = 1;
	if (config->open_flag & SUPPRESS_WIFI_ACCESS)
		request_flags |= 2;
	if (config->open_flag & HTTPFILE_OPEN_HTTP_OPTION4)
		request_flags |= 4;
	SceHttp_F30073EC(self->m_request_id, request_flags);
	if (config->timeout.resolve_timeout >= 0)
		sceHttpSetResolveTimeOut(self->m_request_id,
					 config->timeout.resolve_timeout);
	if (config->timeout.connect_timeout >= 0)
		sceHttpSetConnectTimeOut(self->m_request_id,
					 config->timeout.connect_timeout);
	if (config->timeout.send_timeout >= 0)
		sceHttpSetSendTimeOut(self->m_request_id, config->timeout.send_timeout);
	if (config->timeout.recv_timeout >= 0)
		sceHttpSetRecvTimeOut(self->m_request_id, config->timeout.recv_timeout);
	request_header.id = self->m_request_id;
	request_header.mode = SCE_HTTP_HEADER_OVERWRITE;
	HttpFileHeaderMapForEach(&config->header, HttpFileHttpReaderAddHeader,
				&request_header);
	if (config->accept_encoding_gzip == 1)
		sceHttpSetAcceptEncodingGZIPEnabled(self->m_request_id, 1);
	else if (config->accept_encoding_gzip == 0)
		sceHttpSetAcceptEncodingGZIPEnabled(self->m_request_id, 0);
	PafStringDestroy(&param.scheme);
	PafStringDestroy(&param.host);
	PafStringDestroy(&param.user_agent);
	RWLockUnlockExclusive(&self->m_lock);
	RWLockLockShared(&self->m_lock);
	request_id = self->m_request_id;
	RWLockUnlockShared(&self->m_lock);
	result = sceHttpSendRequest(request_id, body, body_size);
	if (result == SCE_HTTP_ERROR_PROXY) {
		self->base.m_result[0] = sceHttpGetLastErrno(request_id, &error);
		if (!self->base.m_result[0])
			self->base.m_result[0] = error;
	} else {
		self->base.m_result[0] = 0;
	}
	if (result == SCE_HTTP_ERROR_SSL) {
		self->base.m_result[1] = sceHttpsGetSslError(request_id, &error, &detail);
		if (!self->base.m_result[1]) {
			self->base.m_result[1] = error;
			self->base.m_result[2] = detail;
		}
	} else {
		self->base.m_result[1] = 0;
		self->base.m_result[2] = 0;
	}
	ScePafMisc_F39DF886(&self->base.m_response_headers);
	RWLockLockShared(&self->m_lock);
	request_id = self->m_request_id;
	RWLockUnlockShared(&self->m_lock);
	if (!sceHttpGetAllResponseHeaders(request_id, &header, &header_size)) {
		HttpFileResponseParse(&self->base.m_response, header, header_size);
		HttpFileHeaderMapParse(&self->base.m_response_headers, header, header_size);
	}
	if (result)
		goto request_error;
	self->m_position = 0;
	RWLockLockShared(&self->m_lock);
	request_id = self->m_request_id;
	RWLockUnlockShared(&self->m_lock);
	if (!sceHttpGetResponseContentLength(request_id, &content_length)) {
		self->m_content_length = content_length;
		if (self->m_content_length < 0)
			self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	} else {
		self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	}
	result = 0;
	goto out_strings;

out_param_error:
	PafStringDestroy(&param.scheme);
	PafStringDestroy(&param.host);
	PafStringDestroy(&param.user_agent);
	RWLockUnlockExclusive(&self->m_lock);
request_error:
	RWLockLockExclusive(&self->m_lock);
	if (self->m_request_id > 0)
		sceHttpDeleteRequest(self->m_request_id);
	self->m_request_id = 0;
	context = self->base.m_config.m_ptr->context;
	if (context.m_ptr)
		sceAtomicIncrement32AcqRel(&context.m_ref_counter->m_counter);
	HttpFileReaderContextReturnConnection(&context, &self->m_connection);
	HttpFileReaderContextSharedPtrRelease(&context);
	self->m_aborted = 0;
	RWLockUnlockExclusive(&self->m_lock);
out_strings:
	PafStringDestroy(&referer);
	PafStringDestroy(&user_agent);
	return result;
}

int32_t HttpFileHttpReaderClose(HttpFileHttpReader *self)
{
	HttpFileReaderContextSharedPtr context;
	HttpFileReaderConfig *config;

	RWLockLockExclusive(&self->m_lock);
	if (self->m_request_id > 0)
		sceHttpDeleteRequest(self->m_request_id);
	config = self->base.m_config.m_ptr;
	context.m_ptr = config->context.m_ptr;
	self->m_request_id = 0;
	context.m_ref_counter = config->context.m_ref_counter;
	if (context.m_ptr)
		sceAtomicIncrement32AcqRel(&context.m_ref_counter->m_counter);
	HttpFileReaderContextReturnConnection(&context, &self->m_connection);
	HttpFileReaderContextSharedPtrRelease(&context);
	self->m_aborted = 0;
	RWLockUnlockExclusive(&self->m_lock);
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	self->m_position = 0;
	return 0;
}

const HttpFileRangeReaderClass httpFileRangeReaderClass = {
	0, NULL, { {
		(HttpFileReader *(*)(HttpFileReader *))HttpFileRangeReaderDestroy,
		(void (*)(HttpFileReader *))HttpFileRangeReaderDelete,
		(int32_t (*)(HttpFileReader *))HttpFileRangeReaderOpen,
		(int32_t (*)(HttpFileReader *))HttpFileRangeReaderClose,
		(int32_t (*)(HttpFileReader *, void *, size_t))HttpFileRangeReaderRead,
		(int64_t (*)(HttpFileReader *, int64_t, int32_t))HttpFileRangeReaderSeek,
		(int32_t (*)(HttpFileReader *))HttpFileRangeReaderAbort,
		(int64_t (*)(HttpFileReader *))HttpFileRangeReaderGetFileSize,
		(HttpFileResponse *(*)(HttpFileReader *))HttpFileRangeReaderGetResponse,
		(PafMap *(*)(HttpFileReader *))HttpFileRangeReaderGetResponseHeaders
	}, HttpFileRangeReaderFormatRange }
};

HttpFileRangeReader *HttpFileRangeReaderConstruct(HttpFileRangeReader *self,
					       const HttpFileReaderConfigSharedPtr *config)
{
	HttpFileReaderConfigSharedPtr temporary = *config;

	if (temporary.m_ptr)
		sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
	self->base.m_config = temporary;
	self->base.vtable = &httpFileReaderClass.methods;
	if (temporary.m_ptr)
		sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
	HttpFileHeaderMapInitialize(&self->base.m_response_headers);
	HttpFileResponseConstruct(&self->base.m_response);
	self->base.m_result[0] = 0;
	self->base.m_result[1] = 0;
	self->base.m_result[2] = 0;
	HttpFileReaderConfigSharedPtrRelease(&temporary);
	self->base.vtable = &httpFileRangeReaderClass.methods.base;
	self->m_range_enabled = 1;
	self->m_reader = NULL;
	self->m_position = 0;
	self->m_range_begin = -1;
	self->m_range_end = -1;
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	return self;
}

HttpFileRangeReader *HttpFileRangeReaderDestroy(HttpFileRangeReader *self)
{
	HttpFileReader *reader = self->m_reader;

	if (reader) {
		self->base.vtable = &httpFileRangeReaderClass.methods.base;
		reader->vtable->Delete(reader);
	}
	self->m_reader = NULL;
	HttpFileReaderDestroyMembers(&self->base);
	return self;
}

void HttpFileRangeReaderDelete(HttpFileRangeReader *self)
{
	HttpFileRangeReaderDestroy(self);
	ScePafStdc_1B77082E(self);
}

int32_t HttpFileRangeReaderOpen(HttpFileRangeReader *self)
{
	HttpFileReaderConfigSharedPtr config = { NULL, NULL };
	HttpFileReaderConfigSharedPtr src;
	HttpFileReaderConfigSharedPtr cloned;
	HttpFileReaderConfigSharedPtr temporary;
	HttpFileHttpReader *http_reader;
	HttpFileReader *reader;
	int32_t result;

	self->m_position = 0;
	self->m_range_begin = -1;
	self->m_range_end = -1;
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	if (self->m_range_enabled) {
		HttpFileResponse *response;

		src = self->base.m_config;
		if (src.m_ptr)
			sceAtomicIncrement32AcqRel(&src.m_ref_counter->m_counter);
		HttpFileReaderConfigClone(&cloned, &src);
		if (cloned.m_ptr) {
			config = cloned;
			sceAtomicIncrement32AcqRel(&config.m_ref_counter->m_counter);
		}
		HttpFileReaderConfigSharedPtrRelease(&cloned);
		HttpFileReaderConfigSharedPtrRelease(&src);
		HttpFileHeaderMapRemove(&config.m_ptr->header, "Range");
		config.m_ptr->method = "HEAD";
		http_reader = ScePafStdc_FC5CD359(sizeof(*http_reader));
		if (http_reader) {
			temporary = config;
			if (temporary.m_ptr)
				sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
			http_reader = HttpFileHttpReaderConstruct(http_reader, &temporary);
			self->m_reader = &http_reader->base;
			HttpFileReaderConfigSharedPtrRelease(&temporary);
		} else {
			self->m_reader = NULL;
		}
		reader = self->m_reader;
		if (!reader) {
			result = SCE_PAF_ERROR_FILE_NO_MEMORY;
			goto out;
		}
		result = reader->vtable->Open(reader);
		self->base.m_result[0] = self->m_reader->m_result[0];
		reader = self->m_reader;
		self->base.m_result[1] = reader->m_result[1];
		self->base.m_result[2] = reader->m_result[2];
		if (result) {
			if (reader)
				reader->vtable->Delete(reader);
			self->m_reader = NULL;
			goto out;
		}
		self->m_content_length = reader->vtable->GetFileSize(reader);
		reader = self->m_reader;
		response = reader->vtable->GetResponse(reader);
		if (response->code == 200 && response->major == 1 && response->minor == 1) {
			result = 0;
			goto out;
		}
		reader = self->m_reader;
		self->m_range_enabled = 0;
		reader->vtable->Close(reader);
		reader = self->m_reader;
		if (reader)
			reader->vtable->Delete(reader);
		self->m_reader = NULL;
	}
	src = self->base.m_config;
	if (src.m_ptr)
		sceAtomicIncrement32AcqRel(&src.m_ref_counter->m_counter);
	HttpFileReaderConfigClone(&cloned, &src);
	if (cloned.m_ptr != config.m_ptr) {
		HttpFileReaderConfigSharedPtr replacement = cloned;

		HttpFileReaderConfigSharedPtrRelease(&config);
		config = replacement;
		if (config.m_ptr)
			sceAtomicIncrement32AcqRel(&config.m_ref_counter->m_counter);
	}
	HttpFileReaderConfigSharedPtrRelease(&cloned);
	HttpFileReaderConfigSharedPtrRelease(&src);
	HttpFileHeaderMapRemove(&config.m_ptr->header, "Range");
	http_reader = ScePafStdc_FC5CD359(sizeof(*http_reader));
	if (http_reader) {
		temporary = config;
		if (temporary.m_ptr)
			sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
		http_reader = HttpFileHttpReaderConstruct(http_reader, &temporary);
		self->m_reader = &http_reader->base;
		HttpFileReaderConfigSharedPtrRelease(&temporary);
	} else {
		self->m_reader = NULL;
	}
	reader = self->m_reader;
	if (!reader) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		goto out;
	}
	result = reader->vtable->Open(reader);
	self->base.m_result[0] = self->m_reader->m_result[0];
	reader = self->m_reader;
	self->base.m_result[1] = reader->m_result[1];
	self->base.m_result[2] = reader->m_result[2];
	if (result) {
		if (reader)
			reader->vtable->Delete(reader);
		self->m_reader = NULL;
	}
out:
	HttpFileReaderConfigSharedPtrRelease(&config);
	return result;
}

int32_t HttpFileRangeReaderClose(HttpFileRangeReader *self)
{
	HttpFileReader *reader = self->m_reader;
	int32_t result = 0;

	if (reader) {
		result = reader->vtable->Close(reader);
		reader = self->m_reader;
		if (reader)
			reader->vtable->Delete(reader);
		self->m_reader = NULL;
	}
	self->m_position = 0;
	self->m_range_begin = -1;
	self->m_range_end = -1;
	self->m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	return result;
}

static inline __attribute__((always_inline))
int32_t HttpFileRangeReaderParseContentRange(HttpFileRangeReader *self,
					   const char *header)
{
	int32_t len;
	int32_t pos = 0;
	size_t prefix_len;
	uint64_t end = 0;

	if (!header)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	len = (int32_t)sce_paf_strlen(header);
	self->m_range_end = -1;
	self->m_range_begin = -1;
	while (pos < len && (sce_paf_look_ctype_table((signed char)header[pos]) & 8))
		pos++;
	prefix_len = sce_paf_strlen("bytes ");
	if ((int32_t)((uint32_t)pos + prefix_len) > len ||
	    sce_paf_memcmp(header + pos, "bytes ", prefix_len))
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	pos = (int32_t)((uint32_t)pos + prefix_len);
	self->m_range_begin = 0;
	while (pos < len) {
		uint64_t begin;

		if ((signed char)header[pos] == '-') {
			pos++;
			break;
		}
		if (!(sce_paf_look_ctype_table((signed char)header[pos]) & 4))
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		begin = (uint64_t)self->m_range_begin * 10;
		self->m_range_begin = begin;
		pos++;
		self->m_range_begin = begin + (int64_t)((signed char)header[pos - 1] - '0');
	}
	self->m_range_end = 0;
	while (pos < len) {
		uint64_t value;

		if ((signed char)header[pos] == '/') {
			self->m_range_end = end + 1;
			return 0;
		}
		if (!(sce_paf_look_ctype_table((signed char)header[pos]) & 4))
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
		value = (uint64_t)self->m_range_end * 10;
		self->m_range_end = value;
		pos++;
		end = value + (int64_t)((signed char)header[pos - 1] - '0');
		self->m_range_end = end;
	}
	return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
}

int32_t HttpFileRangeReaderRead(HttpFileRangeReader *self, void *buf, size_t nbyte)
{
	HttpFileReader *reader;
	HttpFileHttpReader *http_reader;
	HttpFileReaderConfigSharedPtr src;
	HttpFileReaderConfigSharedPtr config;
	char range[64];
	int64_t position;
	int64_t end;
	int32_t result;
	uint16_t code;
	size_t count;

	if (!nbyte)
		return 0;
	position = self->m_position;
	if (position == self->m_content_length)
		return 0;
	if (!self->m_range_enabled)
		goto ordinary;
	reader = self->m_reader;
	if (reader && self->m_range_begin <= position && position < self->m_range_end) {
		int32_t available = (uint64_t)self->m_range_end - (uint64_t)position;
		int32_t remaining = (int32_t)nbyte;
		uint32_t total = 0;
		char *dst = buf;

		if (available < remaining)
			remaining = available;
		if ((uint32_t)remaining > nbyte)
			return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
		while (remaining > 0) {
			result = reader->vtable->Read(reader, dst, (uint32_t)remaining);
			reader = self->m_reader;
			self->base.m_result[0] = reader->m_result[0];
			self->base.m_result[1] = reader->m_result[1];
			self->base.m_result[2] = reader->m_result[2];
			if (result < 0)
				return result;
			if (!result)
				return (int32_t)total;
			self->m_position = (uint64_t)self->m_position + (uint64_t)(int64_t)result;
			dst += result;
			total += (uint32_t)result;
			remaining = (uint32_t)remaining - (uint32_t)result;
		}
		return (int32_t)total;
	}
	if (reader) {
		reader->vtable->Close(reader);
		reader = self->m_reader;
		if (reader)
			reader->vtable->Delete(reader);
		self->m_reader = NULL;
	}
	src = self->base.m_config;
	if (src.m_ptr)
		sceAtomicIncrement32AcqRel(&src.m_ref_counter->m_counter);
	HttpFileReaderConfigClone(&config, &src);
	HttpFileReaderConfigSharedPtrRelease(&src);
	range[0] = 0;
	ScePafStdc_75ECC54E(range + 1, 0, sizeof(range) - 1);
	if (((const HttpFileRangeReaderVTable *)self->base.vtable)->FormatRange(
		self, range, sizeof(range), nbyte))
		HttpFileHeaderMapSet(&config.m_ptr->header, "Range", range, 1);
	http_reader = ScePafStdc_FC5CD359(sizeof(*http_reader));
	if (http_reader) {
		src = config;
		if (src.m_ptr)
			sceAtomicIncrement32AcqRel(&src.m_ref_counter->m_counter);
		http_reader = HttpFileHttpReaderConstruct(http_reader, &src);
		self->m_reader = &http_reader->base;
		HttpFileReaderConfigSharedPtrRelease(&src);
	} else {
		self->m_reader = NULL;
	}
	reader = self->m_reader;
	if (!reader) {
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		goto out;
	}
	result = reader->vtable->Open(reader);
	self->base.m_result[0] = self->m_reader->m_result[0];
	reader = self->m_reader;
	self->base.m_result[1] = reader->m_result[1];
	self->base.m_result[2] = reader->m_result[2];
	if (result)
		goto out_error;
	code = reader->vtable->GetResponse(reader)->code;
	if (code == 206) {
		const char *header;
		PafMap *headers;
		int64_t begin;
		int64_t available;

		reader = self->m_reader;
		headers = reader->vtable->GetResponseHeaders(reader);
		header = HttpFileHeaderMapGet(headers, "Content-Range", 0);
		result = HttpFileRangeReaderParseContentRange(self, header);
		if (result) {
			result = HTTPFILE_ERROR_INVALID_CONTENT_RANGE;
			goto out_error;
		}
		position = self->m_position;
		begin = self->m_range_begin;
		if (begin > position || position >= self->m_range_end) {
			result = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
			goto out_error;
		}
		end = self->m_range_end;
		if (begin < position) {
			int64_t seek_result;

			seek_result = self->base.vtable->Seek(&self->base,
				(uint64_t)position - (uint64_t)begin, SCE_SEEK_CUR);
			position = self->m_position;
			end = self->m_range_end;
			/* The parser's result is retained when this seek fails. */
			if (seek_result < 0)
				goto out_error;
		}
		available = (uint64_t)end - (uint64_t)position;
		count = nbyte;
		if ((int64_t)(uint64_t)nbyte > available)
			count = (uint32_t)available;
		if (count > nbyte) {
			result = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
			goto out;
		}
		reader = self->m_reader;
		result = reader->vtable->Read(reader, buf, count);
		reader = self->m_reader;
		self->base.m_result[0] = reader->m_result[0];
		self->base.m_result[1] = reader->m_result[1];
		self->base.m_result[2] = reader->m_result[2];
		if (result < 0)
			goto out_error;
		self->m_position = (uint64_t)self->m_position + (uint64_t)(int64_t)result;
		goto out;
	}
	if (code == 200) {
		int64_t seek_result;

		position = self->m_position;
		self->m_range_enabled = 0;
		self->m_position = 0;
		seek_result = self->base.vtable->Seek(&self->base, position, SCE_SEEK_SET);
		if (seek_result < 0) {
			result = (int32_t)seek_result;
			goto out_error;
		}
		HttpFileReaderConfigSharedPtrRelease(&config);
		goto ordinary;
	}
	if (code == 416) {
		result = 0;
		goto out;
	}
	result = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
out_error:
	HttpFileReaderConfigSharedPtrRelease(&config);
	reader = self->m_reader;
	if (reader) {
		reader->vtable->Close(reader);
		reader = self->m_reader;
		if (reader)
			reader->vtable->Delete(reader);
		self->m_reader = NULL;
	}
	return result;
out:
	HttpFileReaderConfigSharedPtrRelease(&config);
	return result;
ordinary:
	reader = self->m_reader;
	if (!reader)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	result = reader->vtable->Read(reader, buf, nbyte);
	reader = self->m_reader;
	self->base.m_result[0] = reader->m_result[0];
	self->base.m_result[1] = reader->m_result[1];
	self->base.m_result[2] = reader->m_result[2];
	return result;
}

int64_t HttpFileRangeReaderSeek(HttpFileRangeReader *self, int64_t offset,
				int32_t whence)
{
	HttpFileReader *reader;
	int64_t position;
	int32_t result;

	if (!self->m_range_enabled) {
		reader = self->m_reader;
		if (!reader)
			return SCE_PAF_ERROR_FILE_NO_MEMORY;
		result = (int32_t)reader->vtable->Seek(reader, offset, whence);
		reader = self->m_reader;
		self->base.m_result[0] = reader->m_result[0];
		self->base.m_result[1] = reader->m_result[1];
		self->base.m_result[2] = reader->m_result[2];
		return result;
	}
	if (whence == SCE_SEEK_SET)
		position = offset;
	else if (whence == SCE_SEEK_CUR)
		position = (uint64_t)self->m_position + (uint64_t)offset;
	else
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	reader = self->m_reader;
	if (reader) {
		if (self->m_range_begin < 0 || self->m_range_end <= self->m_range_begin ||
		    self->m_position > position || position >= self->m_range_end) {
			reader->vtable->Close(reader);
			reader = self->m_reader;
			if (reader)
				reader->vtable->Delete(reader);
			self->m_reader = NULL;
		} else {
			offset = (uint64_t)position - (uint64_t)self->m_position;
			result = (int32_t)reader->vtable->Seek(reader, offset, SCE_SEEK_CUR);
			reader = self->m_reader;
			self->base.m_result[0] = reader->m_result[0];
			self->base.m_result[1] = reader->m_result[1];
			self->base.m_result[2] = reader->m_result[2];
			if (result >= 0)
				self->m_position = result;
			return position;
		}
	}
	self->m_position = position;
	self->m_range_begin = -1;
	self->m_range_end = -1;
	return position;
}

int32_t HttpFileRangeReaderAbort(HttpFileRangeReader *self)
{
	HttpFileReader *reader = self->m_reader;

	if (reader)
		return reader->vtable->Abort(reader);
	return 0;
}

int64_t HttpFileRangeReaderGetFileSize(HttpFileRangeReader *self)
{
	HttpFileReader *reader;

	if (self->m_range_enabled)
		return self->m_content_length;
	reader = self->m_reader;
	if (reader)
		return reader->vtable->GetFileSize(reader);
	return -1;
}

HttpFileResponse *HttpFileRangeReaderGetResponse(HttpFileRangeReader *self)
{
	HttpFileReader *reader = self->m_reader;

	if (reader)
		return reader->vtable->GetResponse(reader);
	return &self->base.m_response;
}

PafMap *HttpFileRangeReaderGetResponseHeaders(HttpFileRangeReader *self)
{
	HttpFileReader *reader = self->m_reader;

	if (reader)
		return reader->vtable->GetResponseHeaders(reader);
	return &self->base.m_response_headers;
}

int32_t HttpFileRangeReaderFormatRange(HttpFileRangeReader *self, char *dst,
				     size_t max, size_t nbyte)
{
	HttpFileReaderConfig *config;
	uint32_t flags = 0;

	if (!dst || max < 64)
		return 0;
	config = self->base.m_config.m_ptr;
	if (config)
		flags = config->open_flag;
	if (flags & ENABLE_RANGE_GET) {
		int64_t position = self->m_position;
		int64_t end = (uint64_t)position + (uint64_t)nbyte - 1;

		sce_paf_snprintf(dst, max, "bytes=%lld-%lld", position, end);
		return 1;
	}
	if (!(flags & ENABLE_HALF_RANGE_GET))
		return 0;
	sce_paf_snprintf(dst, max, "bytes=%lld-", self->m_position);
	return 1;
}

int32_t HttpFileCreateReader(HttpFile *self, const HttpFileOpenArg *arg)
{
	uint32_t range = arg->open_flag & (ENABLE_RANGE_GET | ENABLE_HALF_RANGE_GET);
	HttpFileReaderParam *param = ScePafStdc_FC5CD359(sizeof(*param));
	HttpFileReaderConfigSharedPtr config;
	HttpFileReaderConfigSharedPtr temporary;
	PafMap *headers;
	const void *body;
	int32_t count;
	int32_t gzip;
	uint32_t template_option;
	size_t response_header_max_size;
	int32_t result = 0;

	if (param)
		HttpFileReaderParamInitialize(param);
	self->m_reader_param = param;
	if (!param)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	body = arg->msg_body;
	if (body && arg->msg_body_len) {
		BufferSharedPtr buffer;

		if (range) {
			void *data = sce_paf_malloc(arg->msg_body_len);

			if (!data) {
				HttpFileReaderParamDelete(self->m_reader_param);
				self->m_reader_param = NULL;
				return SCE_PAF_ERROR_FILE_NO_MEMORY;
			}
			sce_paf_memcpy(data, arg->msg_body, arg->msg_body_len);
			param = self->m_reader_param;
			HeapAllocBufferCreate(&buffer, data, arg->msg_body_len, NULL);
		} else {
			UnmanagedConstBufferCreate(&buffer, body, arg->msg_body_len);
		}
		BufferSharedPtrAssign(&param->msg_body, &buffer);
		BufferSharedPtrRelease(&buffer);
	}
	count = (int32_t)arg->header._size;
	headers = &self->m_reader_param->header;
	for (int32_t i = 0; i < count; i++)
		HttpFileHeaderMapParseLine(headers, arg->header._data[i].ptr,
					   arg->header._data[i].count);
	if (!HttpFileHeaderMapGet(headers, "UserAgent", 0) && arg->user_agent.count)
		HttpFileHeaderMapSet(headers, "UserAgent", arg->user_agent.ptr, 1);
	if (!HttpFileHeaderMapGet(headers, "Referer", 0) && arg->referer.count)
		HttpFileHeaderMapSet(headers, "Referer", arg->referer.ptr, 1);
	PafStringCopy(&self->m_reader_param->scheme, &arg->scheme);
	PafStringCopy(&self->m_reader_param->host, &arg->host);
	param = self->m_reader_param;
	param->port = arg->port;
	PafStringCopy(&param->method, &arg->method);
	PafStringCopy(&self->m_reader_param->version, &arg->version);
	PafStringCopy(&self->m_reader_param->uri, &arg->uri);
	PafStringCopy(&self->m_reader_param->user, &arg->user);
	PafStringCopy(&self->m_reader_param->passwd, &arg->passwd);
	param = self->m_reader_param;
	gzip = arg->accept_encoding_gzip;
	param->open_flag = arg->open_flag;
	param->https_flag = arg->https_flag;
	param->timeout.resolve_timeout = arg->timeout.resolve_timeout;
	param->timeout.connect_timeout = arg->timeout.connect_timeout;
	param->timeout.send_timeout = arg->timeout.send_timeout;
	template_option = arg->template_option;
	param->timeout.recv_timeout = arg->timeout.recv_timeout;
	param->accept_encoding_gzip = gzip;
	response_header_max_size = arg->response_header_max_size;
	param->template_option = template_option;
	param->response_header_max_size = response_header_max_size;
	HttpFileReaderContextSharedPtrAssign(&param->context, &self->m_reader_context);
	HttpFileReaderConfigCreate(&config, self->m_reader_param);
	if (range) {
		HttpFileRangeReader *reader = ScePafStdc_FC5CD359(sizeof(*reader));

		if (reader) {
			temporary = config;
			if (temporary.m_ptr)
				sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
			reader = HttpFileRangeReaderConstruct(reader, &temporary);
			self->m_http_reader = &reader->base;
			HttpFileReaderConfigSharedPtrRelease(&temporary);
		} else {
			self->m_http_reader = NULL;
		}
	} else {
		HttpFileHttpReader *reader = ScePafStdc_FC5CD359(sizeof(*reader));

		if (reader) {
			temporary = config;
			if (temporary.m_ptr)
				sceAtomicIncrement32AcqRel(&temporary.m_ref_counter->m_counter);
			reader = HttpFileHttpReaderConstruct(reader, &temporary);
			self->m_http_reader = &reader->base;
			HttpFileReaderConfigSharedPtrRelease(&temporary);
		} else {
			self->m_http_reader = NULL;
		}
	}
	if (!self->m_http_reader) {
		HttpFileReaderParamDelete(self->m_reader_param);
		self->m_reader_param = NULL;
		result = SCE_PAF_ERROR_FILE_NO_MEMORY;
	}
	HttpFileReaderConfigSharedPtrRelease(&config);
	return result;
}

void HttpFileReaderConfigClone(HttpFileReaderConfigSharedPtr *result,
			       const HttpFileReaderConfigSharedPtr *src)
{
	HttpFileReaderConfig *self = ScePafStdc_FC5CD359(sizeof(*self));

	if (self) {
		const HttpFileReaderConfig *config;
		PafMap header;
		void *head;
		size_t size;

		HttpFileHeaderMapInitialize(&self->header);
		self->msg_body.m_ptr = NULL;
		self->msg_body.m_ref_counter = NULL;
		self->context.m_ptr = NULL;
		self->context.m_ref_counter = NULL;
		config = src->m_ptr;
		self->scheme = config->scheme;
		self->host = config->host;
		self->method = config->method;
		self->version = config->version;
		self->port = config->port;
		self->uri = config->uri;
		self->user = config->user;
		self->passwd = config->passwd;
		HttpFileHeaderMapCopy(&header, &config->header);
		head = header._Myhead;
		header._Myhead = self->header._Myhead;
		self->header._Myhead = head;
		size = header._Mysize;
		header._Mysize = self->header._Mysize;
		self->header._Mysize = size;
		HttpFileHeaderMapClear(&header);
		if (header._Myhead)
			ScePafStdc_1B77082E(header._Myhead);
		header._Myhead = (void *)-1;
		header._Mysize = (size_t)-1;
		BufferSharedPtrAssign(&self->msg_body, &config->msg_body);
		self->open_flag = config->open_flag;
		self->https_flag = config->https_flag;
		self->timeout.resolve_timeout = config->timeout.resolve_timeout;
		self->timeout.connect_timeout = config->timeout.connect_timeout;
		self->timeout.send_timeout = config->timeout.send_timeout;
		self->timeout.recv_timeout = config->timeout.recv_timeout;
		self->accept_encoding_gzip = config->accept_encoding_gzip;
		HttpFileReaderContextSharedPtrAssign(&self->context, &config->context);
		self->template_option = config->template_option;
		self->response_header_max_size = config->response_header_max_size;
	}
	result->m_ptr = self;
	if (self) {
		HttpFileReaderConfigRefCounter *counter = ScePafStdc_FC5CD359(sizeof(*counter));

		if (counter) {
			result->m_ref_counter = counter;
			counter->m_ptr = self;
			result->m_ref_counter->m_counter = 1;
			result->m_ref_counter->m_weak_counter = 1;
			result->m_ref_counter->m_deleter = HttpFileReaderConfigDelete;
		} else {
			result->m_ptr = NULL;
			result->m_ref_counter = NULL;
		}
	} else {
		result->m_ref_counter = NULL;
	}
}
