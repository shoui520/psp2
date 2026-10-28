#ifndef PAF_FILE_HTTPFILE_READER_H
#define PAF_FILE_HTTPFILE_READER_H

#include "httpfile.h"
#include "../std/map.h"

typedef struct HttpFileReaderConfig HttpFileReaderConfig;
typedef struct HttpFileReaderConfigRefCounter HttpFileReaderConfigRefCounter;

typedef struct HttpFileReaderConfigSharedPtr {
	HttpFileReaderConfig *m_ptr;
	HttpFileReaderConfigRefCounter *m_ref_counter;
} HttpFileReaderConfigSharedPtr;

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
