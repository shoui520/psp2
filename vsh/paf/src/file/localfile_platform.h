#ifndef PAF_FILE_LOCALFILE_PLATFORM_H
#define PAF_FILE_LOCALFILE_PLATFORM_H

#include "../compiler.h"
#include "localfile_impl.h"
#include <iofilemgr/iofilemgr.h>

typedef struct LocalFilePsp2Impl {
	LocalFileImpl base;
	unsigned char m_is_opened;
	SceUID m_fd;
	IoAsyncParam m_async;
	int32_t m_as_result;
	uint32_t m_async_type;
} LocalFilePsp2Impl;

enum LocalFileAsyncType {
	TYPE_NONE = 0,
	TYPE_OPEN = 1,
	TYPE_CLOSE = 2,
	TYPE_READ = 3,
	TYPE_WRITE = 4,
	TYPE_SEEK = 5,
	TYPE_FLUSH = 6
};

#define LocalFilePsp2ImplSize sizeof(LocalFilePsp2Impl)

typedef char LocalFilePsp2ImplSizeCheck[sizeof(LocalFilePsp2Impl) == 48 ? 1 : -1];
typedef char LocalFilePsp2ImplAsyncOffsetCheck[offsetof(LocalFilePsp2Impl, m_async) == 16 ? 1 : -1];
typedef char LocalFilePsp2ImplResultOffsetCheck[offsetof(LocalFilePsp2Impl, m_as_result) == 40 ? 1 : -1];

#define LocalFilePsp2ImplConstruct ScePafMisc_E73FE3E5

LocalFilePsp2Impl *LocalFilePsp2ImplConstruct(LocalFilePsp2Impl *self);

#define LocalFilePsp2ImplConstructBase ScePafMisc_A495CC6B
#define LocalFilePsp2ImplDestroy ScePafMisc_EEDD1701
#define LocalFilePsp2ImplDelete ScePafMisc_DF3A09D4
#define LocalFilePsp2ImplDestroyBase ScePafMisc_A47418DF
#define LocalFilePsp2ImplOpen ScePafMisc_9DD7B62D
#define LocalFilePsp2ImplClose ScePafMisc_757CC8B1
#define LocalFilePsp2ImplRead ScePafMisc_999CDF39
#define LocalFilePsp2ImplWrite ScePafMisc_132F4E2C
#define LocalFilePsp2ImplSeek ScePafMisc_38D851FF
#define LocalFilePsp2ImplFlush ScePafMisc_61B3C8C6
#define LocalFilePsp2ImplOpenAsync ScePafMisc_BDE466D2
#define LocalFilePsp2ImplCloseAsync ScePafMisc_E1624D8E
#define LocalFilePsp2ImplReadAsync ScePafMisc_C6220CE0
#define LocalFilePsp2ImplWriteAsync ScePafMisc_604D3C31
#define LocalFilePsp2ImplSeekAsync ScePafMisc_69798A47
#define LocalFilePsp2ImplFlushAsync ScePafMisc_F626DC7D
#define LocalFilePsp2ImplWaitAsync ScePafMisc_7486256D
#define LocalFilePsp2ImplPollAsync ScePafMisc_EE7027E7
#define LocalFilePsp2ImplIsEof ScePafMisc_8A2A4C16
#define LocalFilePsp2ImplAbort ScePafMisc_00AB4F34
#define LocalFilePsp2ImplGetFileSize ScePafMisc_3AB24248
#define LocalFilePsp2ImplGetstat ScePafMisc_223EAD7D
#define LocalFilePsp2ImplSetPriority ScePafMisc_619FA110

#define localFilePsp2ImplClass ScePafMisc_3EC47082
extern const LocalFileImplClass localFilePsp2ImplClass;

LocalFilePsp2Impl * LocalFilePsp2ImplConstructBase(LocalFilePsp2Impl *self);
LocalFilePsp2Impl * LocalFilePsp2ImplDestroy(LocalFilePsp2Impl *self);
void LocalFilePsp2ImplDelete(LocalFilePsp2Impl *self);
LocalFilePsp2Impl * LocalFilePsp2ImplDestroyBase(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplOpen(LocalFilePsp2Impl *self, const char *filename, uint32_t flag, uint32_t mode);
int32_t LocalFilePsp2ImplClose(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplRead(LocalFilePsp2Impl *self, void *buf, size_t nbyte);
int32_t LocalFilePsp2ImplWrite(LocalFilePsp2Impl *self, const void *buf, size_t nbyte);
int64_t LocalFilePsp2ImplSeek(LocalFilePsp2Impl *self, int64_t offset, int32_t whence);
int32_t LocalFilePsp2ImplFlush(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplOpenAsync(LocalFilePsp2Impl *self, const char *filename, uint32_t flag, uint32_t mode);
int32_t LocalFilePsp2ImplCloseAsync(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplReadAsync(LocalFilePsp2Impl *self, void *buf, size_t nbyte);
int32_t LocalFilePsp2ImplWriteAsync(LocalFilePsp2Impl *self, const void *buf, size_t nbyte);
int32_t LocalFilePsp2ImplSeekAsync(LocalFilePsp2Impl *self, int64_t offset, int32_t whence);
int32_t LocalFilePsp2ImplFlushAsync(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplWaitAsync(LocalFilePsp2Impl *self, int64_t *result);
int32_t LocalFilePsp2ImplPollAsync(LocalFilePsp2Impl *self, int64_t *result);
unsigned char LocalFilePsp2ImplIsOpened(LocalFilePsp2Impl *self);
unsigned char LocalFilePsp2ImplIsEof(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplAbort(LocalFilePsp2Impl *self);
int64_t LocalFilePsp2ImplGetFileSize(LocalFilePsp2Impl *self);
int32_t LocalFilePsp2ImplGetstat(LocalFilePsp2Impl *self, FileStat *stat);
int32_t LocalFilePsp2ImplSetPriority(LocalFilePsp2Impl *self, int32_t priority);

#endif
