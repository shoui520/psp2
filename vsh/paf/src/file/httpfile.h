#ifndef PAF_FILE_HTTPFILE_H
#define PAF_FILE_HTTPFILE_H

#include "file.h"
#include "../thread/mutex.h"

typedef struct Thread Thread;
typedef struct HttpFileReader HttpFileReader;
typedef struct HttpFileReaderParam HttpFileReaderParam;
typedef struct HttpFileReaderContext HttpFileReaderContext;
typedef struct HttpFileReaderContextRefCounter HttpFileReaderContextRefCounter;

typedef struct HttpFileReaderContextSharedPtr {
	HttpFileReaderContext *m_ptr;
	HttpFileReaderContextRefCounter *m_ref_counter;
} HttpFileReaderContextSharedPtr;

typedef struct HttpFile {
	File base;
	unsigned char m_is_opened;
	RWLock m_lock_rwlock;
	RMutex m_lock_mutex;
	Thread *m_io_thread;
	HttpFileReader *m_http_reader;
	HttpFileReaderParam *m_reader_param;
	int32_t m_reader_result[3];
	HttpFileReaderContextSharedPtr m_reader_context;
} HttpFile;

typedef char HttpFileSizeCheck[sizeof(HttpFile) == 80 ? 1 : -1];
typedef char HttpFileMutexOffsetCheck[offsetof(HttpFile, m_lock_mutex) == 16 ? 1 : -1];
typedef char HttpFileReaderOffsetCheck[offsetof(HttpFile, m_http_reader) == 52 ? 1 : -1];
typedef char HttpFileContextOffsetCheck[offsetof(HttpFile, m_reader_context) == 72 ? 1 : -1];

#define HttpFileConstruct ScePafMisc_541FFC11

HttpFile *HttpFileConstruct(HttpFile *self);
void HttpFileDefaultDelete(File *file);

#endif
