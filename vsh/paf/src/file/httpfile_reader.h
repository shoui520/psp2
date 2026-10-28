#ifndef PAF_FILE_HTTPFILE_READER_H
#define PAF_FILE_HTTPFILE_READER_H

#include "httpfile.h"
#include "../std/map.h"
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
HttpFileHeaderMapNode *HttpFileHeaderMapCopyNodes(PafMap *self,
					      HttpFileHeaderMapNode *parent,
					      const HttpFileHeaderMapNode *src);
int32_t HttpFileDeleteReader(HttpFile *self);

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
int32_t HttpFileReaderConfigIsHttpsCheckEnabled(const HttpFileReaderConfig *self);
int32_t HttpFileReaderConfigIsProxyIgnored(const HttpFileReaderConfig *self);

typedef struct HttpFileResponse {
	uint16_t code;
	PafString phrase;
	unsigned char major;
	unsigned char minor;
	unsigned char valid;
} HttpFileResponse;

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

const char *ScePafMisc_82171187(const PafMap *headers, const char *name,
			      int32_t index);

#endif
