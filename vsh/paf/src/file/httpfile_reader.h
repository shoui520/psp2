#ifndef PAF_FILE_HTTPFILE_READER_H
#define PAF_FILE_HTTPFILE_READER_H

#include "httpfile.h"
#include "../std/map.h"
#include "../std/list.h"
#include "../buffer/buffer.h"

typedef struct HttpFileHeaderEntry {
	PafString first;
	PafStringVector second;
} HttpFileHeaderEntry;

typedef struct HttpFileHeaderMapNode {
	struct HttpFileHeaderMapNode *_Parent;
	struct HttpFileHeaderMapNode *_Left;
	struct HttpFileHeaderMapNode *_Right;
	HttpFileHeaderEntry _Myval;
	unsigned char _Color;
	unsigned char _Isnil;
} HttpFileHeaderMapNode;

struct HttpFileReaderParam {
	PafString scheme;
	PafString host;
	uint16_t port;
	PafString method;
	PafString version;
	PafString uri;
	PafString user;
	PafString passwd;
	PafMap header;
	BufferSharedPtr msg_body;
	uint32_t open_flag;
	uint32_t https_flag;
	HttpFileTimeout timeout;
	int32_t accept_encoding_gzip;
	HttpFileReaderContextSharedPtr context;
	uint32_t template_option;
	size_t response_header_max_size;
};

typedef char HttpFileHeaderMapNodeSizeCheck[sizeof(HttpFileHeaderMapNode) == 44 ? 1 : -1];
typedef char HttpFileReaderParamSizeCheck[sizeof(HttpFileReaderParam) == 152 ? 1 : -1];
typedef char HttpFileReaderParamBodyOffsetCheck[offsetof(HttpFileReaderParam, msg_body) == 100 ? 1 : -1];
typedef char HttpFileReaderParamContextOffsetCheck[offsetof(HttpFileReaderParam, context) == 136 ? 1 : -1];

PafMap *HttpFileHeaderMapClear(PafMap *self);
PafMap *HttpFileHeaderMapCopy(PafMap *self, const PafMap *src);
HttpFileHeaderMapNode *HttpFileHeaderMapRebalance(PafMap *self,
					       HttpFileHeaderMapNode *node);
HttpFileHeaderMapNode *HttpFileHeaderMapInsert(PafMap *self,
					    const HttpFileHeaderEntry *value,
					    unsigned char *inserted);
PafStringVector *HttpFileHeaderMapSubscript(PafMap *self, const PafString *key);
uintptr_t HttpFileHeaderMapEraseRebalance(PafMap *self,
					HttpFileHeaderMapNode *node,
					HttpFileHeaderMapNode *parent);
uintptr_t HttpFileHeaderMapEraseNode(PafMap *self, HttpFileHeaderMapNode *node);
uintptr_t HttpFileHeaderMapErase(PafMap *self, HttpFileHeaderMapNode *const *where);

#define HttpFileHeaderMapSet ScePafMisc_1156F852
#define HttpFileHeaderMapParseLine ScePafMisc_DB5A6E15
#define HttpFileHeaderMapRemove ScePafMisc_E21E796D
#define HttpFileHeaderMapForEach ScePafMisc_5D64A09C
#define HttpFileHeaderMapParse ScePafMisc_DBF32F34

typedef int32_t (*HttpFileHeaderCallback)(const char *name, const char *value,
					void *userdata);

int32_t HttpFileHeaderMapForEach(const PafMap *self, HttpFileHeaderCallback callback,
			       void *userdata);
int32_t HttpFileHeaderMapParse(PafMap *self, const char *header, size_t len);

void HttpFileHeaderMapSet(PafMap *self, const char *name, const char *value,
			 unsigned char replace);
PafMap *ScePafMisc_F39DF886(PafMap *self);
int32_t HttpFileHeaderMapParseLine(PafMap *self, const char *line, size_t len);
int32_t HttpFileHeaderMapRemove(PafMap *self, const char *name);
HttpFileHeaderMapNode *HttpFileHeaderMapCopyNodes(PafMap *self,
					      HttpFileHeaderMapNode *parent,
					      const HttpFileHeaderMapNode *src);
int32_t HttpFileDeleteReader(HttpFile *self);
int32_t HttpFileCreateReader(HttpFile *self, const HttpFileOpenArg *arg);
int32_t HttpFileResponseCodeToError(uint16_t code);

/* The I/O thread owns the separately allocated member-call object. */
typedef struct HttpFileIoThread {
	Thread base;
	int32_t m_operation;
	int64_t m_result;
	void *m_call;
} HttpFileIoThread;

typedef char HttpFileIoThreadSizeCheck[sizeof(HttpFileIoThread) == 32 ? 1 : -1];
typedef char HttpFileIoThreadResultOffsetCheck[offsetof(HttpFileIoThread, m_result) == 16 ? 1 : -1];
typedef char HttpFileIoThreadCallOffsetCheck[offsetof(HttpFileIoThread, m_call) == 24 ? 1 : -1];

#define HttpFileCompleteAsync ScePafMisc_C098F659
int32_t HttpFileCompleteAsync(HttpFile *self, int64_t *result);

typedef struct HttpFileReaderConfig HttpFileReaderConfig;
typedef struct HttpFileReaderConfigRefCounter HttpFileReaderConfigRefCounter;

typedef struct HttpFileReaderConfigSharedPtr {
	HttpFileReaderConfig *m_ptr;
	HttpFileReaderConfigRefCounter *m_ref_counter;
} HttpFileReaderConfigSharedPtr;

struct HttpFileReaderConfig {
	const char *scheme;
	const char *host;
	uint16_t port;
	const char *method;
	const char *version;
	const char *uri;
	const char *user;
	const char *passwd;
	PafMap header;
	BufferSharedPtr msg_body;
	uint32_t open_flag;
	uint32_t https_flag;
	HttpFileTimeout timeout;
	int32_t accept_encoding_gzip;
	HttpFileReaderContextSharedPtr context;
	uint32_t template_option;
	size_t response_header_max_size;
};

struct HttpFileReaderConfigRefCounter {
	HttpFileReaderConfig *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(HttpFileReaderConfig *ptr);
};

typedef char HttpFileReaderConfigSizeCheck[sizeof(HttpFileReaderConfig) == 96 ? 1 : -1];
typedef char HttpFileReaderConfigBodyOffsetCheck[offsetof(HttpFileReaderConfig, msg_body) == 44 ? 1 : -1];
typedef char HttpFileReaderConfigContextOffsetCheck[offsetof(HttpFileReaderConfig, context) == 80 ? 1 : -1];
typedef char HttpFileReaderConfigRefCounterSizeCheck[sizeof(HttpFileReaderConfigRefCounter) == 16 ? 1 : -1];

HttpFileReaderConfig *HttpFileReaderConfigConstruct(HttpFileReaderConfig *self,
						  const HttpFileReaderParam *param);
HttpFileReaderConfigRefCounter *HttpFileReaderConfigCreate(HttpFileReaderConfigSharedPtr *result,
							 const HttpFileReaderParam *param);
void HttpFileReaderConfigDelete(HttpFileReaderConfig *self);
void HttpFileReaderConfigClone(HttpFileReaderConfigSharedPtr *result,
			       const HttpFileReaderConfigSharedPtr *src);
int32_t HttpFileReaderConfigIsHttpsCheckEnabled(const HttpFileReaderConfig *self);
int32_t HttpFileReaderConfigIsProxyIgnored(const HttpFileReaderConfig *self);

typedef struct HttpFileResponse {
	uint16_t code;
	PafString phrase;
	unsigned char major;
	unsigned char minor;
	unsigned char valid;
} HttpFileResponse;

#define HttpFileResponseConstruct ScePafMisc_6C99DC26
#define HttpFileResponseConstructBase ScePafMisc_8D31D15D
#define HttpFileResponseParse ScePafMisc_EA63635D
#define HttpFileResponseConstructFromLine ScePafMisc_7A05075E
#define HttpFileResponseConstructBaseFromLine ScePafMisc_78D68A6B
#define HttpFileResponseDestroy ScePafMisc_75985405
#define HttpFileResponseDestroyBase ScePafMisc_D7CB06F9
#define HttpFileHeaderMapGet ScePafMisc_82171187

HttpFileResponse *HttpFileResponseConstruct(HttpFileResponse *self);
HttpFileResponse *HttpFileResponseConstructBase(HttpFileResponse *self);
int32_t HttpFileResponseParse(HttpFileResponse *self, const char *line, size_t len);
HttpFileResponse *HttpFileResponseConstructFromLine(HttpFileResponse *self,
						const char *line, size_t len);
HttpFileResponse *HttpFileResponseConstructBaseFromLine(HttpFileResponse *self,
						    const char *line, size_t len);
HttpFileResponse *HttpFileResponseDestroy(HttpFileResponse *self);
HttpFileResponse *HttpFileResponseDestroyBase(HttpFileResponse *self);

typedef struct HttpFileReaderVTable {
	HttpFileReader *(*Destroy)(HttpFileReader *self);
	void (*Delete)(HttpFileReader *self);
	int32_t (*Open)(HttpFileReader *self);
	int32_t (*Close)(HttpFileReader *self);
	int32_t (*Read)(HttpFileReader *self, void *buf, size_t nbyte);
	int64_t (*Seek)(HttpFileReader *self, int64_t offset, int32_t whence);
	int32_t (*Abort)(HttpFileReader *self);
	int64_t (*GetFileSize)(HttpFileReader *self);
	HttpFileResponse *(*GetResponse)(HttpFileReader *self);
	PafMap *(*GetResponseHeaders)(HttpFileReader *self);
} HttpFileReaderVTable;

typedef struct HttpFileReaderClass {
	int offset_to_top;
	const void *type_info;
	HttpFileReaderVTable methods;
} HttpFileReaderClass;

extern const HttpFileReaderClass httpFileReaderClass;

HttpFileReader *HttpFileReaderDestroy(HttpFileReader *self);
void HttpFileReaderDelete(HttpFileReader *self);
HttpFileResponse *HttpFileReaderGetResponse(HttpFileReader *self);
PafMap *HttpFileReaderGetResponseHeaders(HttpFileReader *self);

struct HttpFileReader {
	const HttpFileReaderVTable *vtable;
	HttpFileReaderConfigSharedPtr m_config;
	PafMap m_response_headers;
	HttpFileResponse m_response;
	int32_t m_result[3];
};

typedef char HttpFileResponseSizeCheck[sizeof(HttpFileResponse) == 20 ? 1 : -1];
typedef char HttpFileResponseValidOffsetCheck[offsetof(HttpFileResponse, valid) == 18 ? 1 : -1];
typedef char HttpFileReaderSizeCheck[sizeof(HttpFileReader) == 56 ? 1 : -1];
typedef char HttpFileReaderResultOffsetCheck[offsetof(HttpFileReader, m_result) == 44 ? 1 : -1];
typedef char HttpFileReaderVTableSizeCheck[sizeof(HttpFileReaderVTable) == 40 ? 1 : -1];
typedef char HttpFileReaderClassSizeCheck[sizeof(HttpFileReaderClass) == 48 ? 1 : -1];

typedef struct HttpFileReaderConnection HttpFileReaderConnection;
typedef struct HttpFileReaderConnectionRefCounter HttpFileReaderConnectionRefCounter;

typedef struct HttpFileReaderConnectionSharedPtr {
	HttpFileReaderConnection *m_ptr;
	HttpFileReaderConnectionRefCounter *m_ref_counter;
} HttpFileReaderConnectionSharedPtr;

typedef struct HttpFileReaderConnectionParam {
	int32_t http_version;
	PafString user_agent;
	PafString host;
	PafString scheme;
	uint16_t port;
	unsigned char auto_proxy;
	unsigned char enable_keepalive;
	uint32_t https_flag;
	uint32_t template_option;
	size_t response_header_max_size;
} HttpFileReaderConnectionParam;

struct HttpFileReaderConnection {
	HttpFileReaderConnectionParam param;
	int32_t m_template_id;
	int32_t m_connection_id;
	uint32_t m_https_flag;
};

struct HttpFileReaderConnectionRefCounter {
	HttpFileReaderConnection *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(HttpFileReaderConnection *ptr);
};

typedef struct HttpFileReaderConnectionListNode {
	struct HttpFileReaderConnectionListNode *prev;
	struct HttpFileReaderConnectionListNode *next;
	HttpFileReaderConnectionSharedPtr val;
} HttpFileReaderConnectionListNode;

struct HttpFileReaderContext {
	size_t m_capacity;
	PafList m_connections;
	Mutex m_mutex;
};

typedef char HttpFileReaderConnectionParamSizeCheck[sizeof(HttpFileReaderConnectionParam) == 56 ? 1 : -1];
typedef char HttpFileReaderConnectionSizeCheck[sizeof(HttpFileReaderConnection) == 68 ? 1 : -1];
typedef char HttpFileReaderConnectionRefCounterSizeCheck[sizeof(HttpFileReaderConnectionRefCounter) == 16 ? 1 : -1];
typedef char HttpFileReaderConnectionListNodeSizeCheck[sizeof(HttpFileReaderConnectionListNode) == 16 ? 1 : -1];
typedef char HttpFileReaderContextSizeCheck[sizeof(HttpFileReaderContext) == 48 ? 1 : -1];
typedef char HttpFileReaderContextMutexOffsetCheck[offsetof(HttpFileReaderContext, m_mutex) == 16 ? 1 : -1];

HttpFileReaderConnection *HttpFileReaderConnectionConstruct(HttpFileReaderConnection *self,
							 const HttpFileReaderConnectionParam *param);
HttpFileReaderConnection *HttpFileReaderConnectionDestroy(HttpFileReaderConnection *self);
void HttpFileReaderConnectionDelete(HttpFileReaderConnection *self);
int32_t HttpFileReaderConnectionCreateHandles(const HttpFileReaderConnectionParam *param,
					    int32_t *template_id, int32_t *connection_id);
uintptr_t HttpFileReaderConnectionListPushBack(PafList *self,
					    const HttpFileReaderConnectionSharedPtr *value);
int32_t HttpFileReaderConnectionListPopFront(PafList *self);
int32_t HttpFileReaderConnectionListErase(HttpFileReaderConnectionListNode **result,
					PafList *self,
					HttpFileReaderConnectionListNode *const *where);
PafList *HttpFileReaderConnectionListDestroy(PafList *self);
HttpFileReaderContext *HttpFileReaderContextConstruct(HttpFileReaderContext *self,
						   size_t capacity);
void HttpFileReaderContextDelete(HttpFileReaderContext *self);
int32_t HttpFileReaderContextReturnConnection(const HttpFileReaderContextSharedPtr *self,
					    HttpFileReaderConnectionSharedPtr *connection);
int32_t HttpFileReaderContextGetConnection(const HttpFileReaderContextSharedPtr *self,
					 const HttpFileReaderConnectionParam *param,
					 HttpFileReaderConnectionSharedPtr *result);

#define HttpFileReaderContextCreate ScePafMisc_84A50E1F
HttpFileReaderContextRefCounter *HttpFileReaderContextCreate(HttpFileReaderContextSharedPtr *result,
							 size_t capacity);

typedef struct HttpFileHttpReader {
	HttpFileReader base;
	HttpFileReaderConnectionSharedPtr m_connection;
	int32_t m_request_id;
	int64_t m_position;
	int64_t m_content_length;
	RWLock m_lock;
	unsigned char m_aborted;
} HttpFileHttpReader;

typedef struct HttpFileRequestHeaderParam {
	int32_t id;
	int32_t mode;
} HttpFileRequestHeaderParam;

typedef char HttpFileRequestHeaderParamSizeCheck[sizeof(HttpFileRequestHeaderParam) == 8 ? 1 : -1];

enum {
	HTTPFILE_OPEN_SKIP_STATUS_CHECK = 0x00800000,
	HTTPFILE_OPEN_HTTP_OPTION4 = 0x10000000
};

enum {
	HTTPFILE_ERROR_UNKNOWN_RESPONSE = -2135994284, /* 0x80AF5054 */
	HTTPFILE_ERROR_STATUS_100 = -2135994224, /* 0x80AF5090 */
	HTTPFILE_ERROR_STATUS_300 = -2135994208, /* 0x80AF50A0 */
	HTTPFILE_ERROR_STATUS_400 = -2135994192, /* 0x80AF50B0 */
	HTTPFILE_ERROR_STATUS_410 = -2135994176, /* 0x80AF50C0 */
	HTTPFILE_ERROR_STATUS_420 = -2135994160, /* 0x80AF50D0 */
	HTTPFILE_ERROR_STATUS_500 = -2135994144 /* 0x80AF50E0 */
};

extern const HttpFileReaderClass httpFileHttpReaderClass;

HttpFileHttpReader *HttpFileHttpReaderConstruct(HttpFileHttpReader *self,
					     const HttpFileReaderConfigSharedPtr *config);
HttpFileHttpReader *HttpFileHttpReaderDestroy(HttpFileHttpReader *self);
void HttpFileHttpReaderDelete(HttpFileHttpReader *self);
int32_t HttpFileHttpReaderOpen(HttpFileHttpReader *self);
int32_t HttpFileHttpReaderClose(HttpFileHttpReader *self);

int32_t HttpFileHttpReaderAddHeader(const char *name, const char *value,
				  void *userdata);

typedef char HttpFileHttpReaderSizeCheck[sizeof(HttpFileHttpReader) == 104 ? 1 : -1];
typedef char HttpFileHttpReaderPositionOffsetCheck[offsetof(HttpFileHttpReader, m_position) == 72 ? 1 : -1];
typedef char HttpFileHttpReaderLockOffsetCheck[offsetof(HttpFileHttpReader, m_lock) == 88 ? 1 : -1];
typedef char HttpFileHttpReaderAbortedOffsetCheck[offsetof(HttpFileHttpReader, m_aborted) == 96 ? 1 : -1];

int32_t HttpFileHttpReaderRead(HttpFileHttpReader *self, void *buf, size_t nbyte);
int64_t HttpFileHttpReaderSeek(HttpFileHttpReader *self, int64_t offset,
			       int32_t whence);
int32_t HttpFileHttpReaderAbort(HttpFileHttpReader *self);
int64_t HttpFileHttpReaderGetFileSize(HttpFileHttpReader *self);

typedef struct HttpFileRangeReader {
	HttpFileReader base;
	unsigned char m_range_enabled;
	HttpFileReader *m_reader;
	int64_t m_position;
	int64_t m_range_begin;
	int64_t m_range_end;
	int64_t m_content_length;
} HttpFileRangeReader;

typedef char HttpFileRangeReaderSizeCheck[sizeof(HttpFileRangeReader) == 96 ? 1 : -1];
typedef char HttpFileRangeReaderReaderOffsetCheck[offsetof(HttpFileRangeReader, m_reader) == 60 ? 1 : -1];
typedef char HttpFileRangeReaderPositionOffsetCheck[offsetof(HttpFileRangeReader, m_position) == 64 ? 1 : -1];
typedef char HttpFileRangeReaderContentLengthOffsetCheck[offsetof(HttpFileRangeReader, m_content_length) == 88 ? 1 : -1];

typedef struct HttpFileRangeReaderVTable {
	HttpFileReaderVTable base;
	int32_t (*FormatRange)(HttpFileRangeReader *self, char *dst,
			       size_t max, size_t nbyte);
} HttpFileRangeReaderVTable;

typedef struct HttpFileRangeReaderClass {
	int offset_to_top;
	const void *type_info;
	HttpFileRangeReaderVTable methods;
} HttpFileRangeReaderClass;

typedef char HttpFileRangeReaderVTableSizeCheck[sizeof(HttpFileRangeReaderVTable) == 44 ? 1 : -1];
typedef char HttpFileRangeReaderClassSizeCheck[sizeof(HttpFileRangeReaderClass) == 52 ? 1 : -1];

enum {
	HTTPFILE_ERROR_INVALID_CONTENT_RANGE = -2135994283 /* 0x80AF5055 */
};

extern const HttpFileRangeReaderClass httpFileRangeReaderClass;

HttpFileRangeReader *HttpFileRangeReaderConstruct(HttpFileRangeReader *self,
					       const HttpFileReaderConfigSharedPtr *config);
HttpFileRangeReader *HttpFileRangeReaderDestroy(HttpFileRangeReader *self);
void HttpFileRangeReaderDelete(HttpFileRangeReader *self);
int32_t HttpFileRangeReaderOpen(HttpFileRangeReader *self);
int32_t HttpFileRangeReaderClose(HttpFileRangeReader *self);
int32_t HttpFileRangeReaderRead(HttpFileRangeReader *self, void *buf, size_t nbyte);
int64_t HttpFileRangeReaderSeek(HttpFileRangeReader *self, int64_t offset,
				int32_t whence);
int32_t HttpFileRangeReaderAbort(HttpFileRangeReader *self);
int64_t HttpFileRangeReaderGetFileSize(HttpFileRangeReader *self);
HttpFileResponse *HttpFileRangeReaderGetResponse(HttpFileRangeReader *self);
PafMap *HttpFileRangeReaderGetResponseHeaders(HttpFileRangeReader *self);
int32_t HttpFileRangeReaderFormatRange(HttpFileRangeReader *self, char *dst,
				     size_t max, size_t nbyte);

const char *HttpFileHeaderMapGet(const PafMap *headers, const char *name,
			       int32_t index);

#endif
