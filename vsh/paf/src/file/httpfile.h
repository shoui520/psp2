#ifndef PAF_FILE_HTTPFILE_H
#define PAF_FILE_HTTPFILE_H

#include "file.h"
#include "../thread/mutex.h"
#include "../thread/thread.h"

typedef struct HttpFileReader HttpFileReader;
typedef struct HttpFileReaderParam HttpFileReaderParam;
typedef struct HttpFileReaderContext HttpFileReaderContext;
typedef struct HttpFileReaderContextRefCounter HttpFileReaderContextRefCounter;

typedef enum HttpFileOptionType {
	OptionType_ResolveTimeOut = 1,
	OptionType_ConnectTimeOut = 2,
	OptionType_SendTimeOut = 4,
	OptionType_RecvTimeOut = 8
} HttpFileOptionType;

typedef enum HttpFileHttpsFlag {
	HTTPS_FLAG_SERVER_VERIFY = 1,
	HTTPS_FLAG_CLIENT_VERIFY = 2,
	HTTPS_FLAG_CN_CHECK = 4,
	HTTPS_FLAG_NOT_AFTER_CHECK = 8,
	HTTPS_FLAG_NOT_BEFORE_CHECK = 16,
	HTTPS_FLAG_KNOWN_CA_CHECK = 32,
	HTTPS_FLAG_DEFAULT = 63,
	HTTPS_FLAG_MASK = 63
} HttpFileHttpsFlag;

typedef struct HttpFileTimeout {
	int32_t resolve_timeout;
	int32_t connect_timeout;
	int32_t send_timeout;
	int32_t recv_timeout;
} HttpFileTimeout;

typedef struct HttpFileOpenArg {
	uint32_t file_type;
	PafString scheme;
	PafString host;
	uint16_t port;
	PafString method;
	PafString uri;
	PafString version;
	PafString user_agent;
	PafString referer;
	PafString user;
	PafString passwd;
	PafStringVector header;
	const void *msg_body;
	size_t msg_body_len;
	uint32_t https_flag;
	uint32_t open_flag;
	int32_t accept_encoding_gzip;
	HttpFileTimeout timeout;
	uint32_t template_option;
	size_t response_header_max_size;
	uint16_t *response_code;
} HttpFileOpenArg;

typedef char HttpFileOpenArgSizeCheck[sizeof(HttpFileOpenArg) == 180 ? 1 : -1];
typedef char HttpFileOpenArgHeaderOffsetCheck[offsetof(HttpFileOpenArg, header) == 116 ? 1 : -1];
typedef char HttpFileOpenArgTimeoutOffsetCheck[offsetof(HttpFileOpenArg, timeout) == 152 ? 1 : -1];
typedef char HttpFileOpenArgResponseOffsetCheck[offsetof(HttpFileOpenArg, response_code) == 176 ? 1 : -1];

#define HttpFileOpenArgConstruct ScePafMisc_37A4ADA9
#define HttpFileOpenArgConstructBase ScePafMisc_F8213E1C
#define HttpFileOpenArgSetOption ScePafMisc_ED1839C2
#define HttpFileSetDefaultTimeout ScePafMisc_30A8AD46
#define HttpFileOpenArgAddRequestHeader ScePafMisc_99A8215F

extern HttpFileTimeout httpFileDefaultTimeout;
HttpFileOpenArg *HttpFileOpenArgConstruct(HttpFileOpenArg *self);
HttpFileOpenArg *HttpFileOpenArgConstructBase(HttpFileOpenArg *self);
int32_t HttpFileOpenArgSetOption(HttpFileOpenArg *self, int32_t value,
				HttpFileOptionType type);
int32_t HttpFileSetDefaultTimeout(int32_t value, HttpFileOptionType type);
int32_t HttpFileOpenArgAddRequestHeader(HttpFileOpenArg *self, const char *name,
				       const char *value);

typedef struct HttpFileReaderContextSharedPtr {
	HttpFileReaderContext *m_ptr;
	HttpFileReaderContextRefCounter *m_ref_counter;
} HttpFileReaderContextSharedPtr;

struct HttpFileReaderContextRefCounter {
	HttpFileReaderContext *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(HttpFileReaderContext *ptr);
};

typedef char HttpFileReaderContextRefCounterSizeCheck[
	sizeof(HttpFileReaderContextRefCounter) == 16 ? 1 : -1];

static inline __attribute__((always_inline))
void HttpFileReaderContextSharedPtrRelease(const HttpFileReaderContextSharedPtr *self)
{
	if (self->m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_counter) == 1) {
		self->m_ref_counter->m_deleter(self->m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->m_ref_counter);
	}
}

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
#define HttpFileOpenWithArg ScePafMisc_D2221E7B
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
#define HttpFileDestroyBase ScePafMisc_E93C0EBB
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
int32_t HttpFileOpenWithArg(HttpFile *self, const FileOpenArg *param);
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
HttpFile *HttpFileDestroyBase(HttpFile *self);
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
