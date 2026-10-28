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
#define HttpFileConstructBase ScePafMisc_444C9C53
#define HttpFileGetFileType ScePafMisc_065AF035
#define HttpFileGetCapability ScePafMisc_F4C6ED9D
#define HttpFileGetFileSize ScePafMisc_B06C6A6B
#define HttpFileOpenArg ScePafMisc_D2221E7B
#define HttpFileOpenAsync ScePafMisc_878D108C
#define HttpFileClose ScePafMisc_0286E577
#define HttpFileCloseAsync ScePafMisc_CE4DFAD1
#define HttpFileIsOpened ScePafMisc_657B46A1
#define HttpFileIsEof ScePafMisc_F1453F65
#define HttpFileAbort ScePafMisc_39910437
#define HttpFileRead ScePafMisc_FD8D09E7
#define HttpFileReadAsync ScePafMisc_1B7C42FA
#define HttpFileWrite ScePafMisc_47834F0F
#define HttpFileWriteAsync ScePafMisc_341895C8
#define HttpFileSeek ScePafMisc_8536F0EB
#define HttpFileSeekAsync ScePafMisc_CD9CCF7F
#define HttpFileFlush ScePafMisc_9FE47AA4
#define HttpFileFlushAsync ScePafMisc_84E88034
#define HttpFileWaitAsync ScePafMisc_7B2F4627
#define HttpFilePollAsync ScePafMisc_D5543A0D
#define HttpFileGetstat ScePafMisc_A004D31D
#define HttpFileDestroy ScePafMisc_967C0019
#define HttpFileDelete ScePafMisc_13E9B54C
#define HttpFileGetResponseCode ScePafMisc_8E656E2D
#define HttpFileGetResponsePhrase ScePafMisc_45ADBF5A
#define HttpFileGetResponseVersion ScePafMisc_6AE2E808
#define HttpFileGetResponseHeader ScePafMisc_B8282602
#define httpFileClass ScePafMisc_C65A180F

HttpFile *HttpFileConstruct(HttpFile *self);
HttpFile *HttpFileConstructBase(HttpFile *self);
int32_t HttpFileGetFileType(const HttpFile *self);
int32_t HttpFileGetCapability(const HttpFile *self);
int64_t HttpFileGetFileSize(HttpFile *self);
int32_t HttpFileOpenArg(HttpFile *self, const FileOpenArg *param);
int32_t HttpFileOpenAsync(HttpFile *self, const FileOpenArg *param);
int32_t HttpFileClose(HttpFile *self);
int32_t HttpFileCloseAsync(HttpFile *self);
unsigned char HttpFileIsOpened(HttpFile *self);
unsigned char HttpFileIsEof(HttpFile *self);
int32_t HttpFileAbort(HttpFile *self);
int32_t HttpFileRead(HttpFile *self, void *buf, size_t nbyte);
int32_t HttpFileReadAsync(HttpFile *self, void *buf, size_t nbyte);
int32_t HttpFileWrite(HttpFile *self, const void *buf, size_t nbyte);
int32_t HttpFileWriteAsync(HttpFile *self, const void *buf, size_t nbyte);
int64_t HttpFileSeek(HttpFile *self, int64_t offset, int32_t whence);
int32_t HttpFileSeekAsync(HttpFile *self, int64_t offset, int32_t whence);
int32_t HttpFileFlush(HttpFile *self);
int32_t HttpFileFlushAsync(HttpFile *self);
int32_t HttpFileWaitAsync(HttpFile *self, int64_t *result);
int32_t HttpFilePollAsync(HttpFile *self, int64_t *result);
int32_t HttpFileGetstat(HttpFile *self, FileStat *stat);
HttpFile *HttpFileDestroy(HttpFile *self);
void HttpFileDelete(HttpFile *self);
int32_t HttpFileGetResponseCode(HttpFile *self, uint16_t *code);
int32_t HttpFileGetResponsePhrase(HttpFile *self, PafString *phrase);
int32_t HttpFileGetResponseVersion(HttpFile *self, unsigned char *major,
				 unsigned char *minor);
int32_t HttpFileGetResponseHeader(HttpFile *self, const char *name,
				PafString *value);
void HttpFileDefaultDelete(File *file);

extern const FileClass httpFileClass;

#endif
