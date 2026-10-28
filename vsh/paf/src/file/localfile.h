#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include "file.h"
#include "localfile_impl.h"
#include "../thread/mutex.h"
#include "../memory/heap_allocator.h"

typedef struct LocalFile {
	File base;
	RMutex m_lock_mutex;
	LocalFileImpl *m_pimpl;
	HeapAllocator *m_allocator;
} LocalFile;

typedef char LocalFileSizeCheck[sizeof(LocalFile) == 48 ? 1 : -1];
typedef char LocalFileMutexOffsetCheck[offsetof(LocalFile, m_lock_mutex) == 8 ? 1 : -1];
typedef char LocalFileImplOffsetCheck[offsetof(LocalFile, m_pimpl) == 40 ? 1 : -1];

typedef struct LocalFileOpenParam {
	uint32_t file_type;
	const char *filename;
	uint32_t flag;
	uint32_t mode;
} LocalFileOpenParam;

typedef char LocalFileOpenParamSizeCheck[sizeof(LocalFileOpenParam) == 16 ? 1 : -1];

#define LocalFileGetFileType ScePafMisc_7D13C35B
#define LocalFileGetCapability ScePafMisc_4300E1B0
#define LocalFileGetFileSize ScePafMisc_AF6A9878
#define LocalFileOpenArg ScePafMisc_499F4FAA
#define LocalFileOpenAsync ScePafMisc_20F863FD
#define LocalFileClose ScePafMisc_00383A45
#define LocalFileCloseAsync ScePafMisc_F035266A
#define LocalFileIsOpened ScePafMisc_74D9B6F4
#define LocalFileIsEof ScePafMisc_55E04F37
#define LocalFileAbort ScePafMisc_D184BC69
#define LocalFileRead ScePafMisc_BD49E2B3
#define LocalFileReadAsync ScePafMisc_0CD740C5
#define LocalFileWrite ScePafMisc_5A26025A
#define LocalFileWriteAsync ScePafMisc_79277F10
#define LocalFileSeek ScePafMisc_99AAA9F7
#define LocalFileSeekAsync ScePafMisc_CE5848E1
#define LocalFileFlush ScePafMisc_4A447655
#define LocalFileFlushAsync ScePafMisc_439847D5
#define LocalFileWaitAsync ScePafMisc_01A03F2E
#define LocalFilePollAsync ScePafMisc_3636127A
#define LocalFileGetstat ScePafMisc_5BEF2351
#define LocalFileSetPriority ScePafMisc_718411BE

int32_t LocalFileGetFileType(const LocalFile *self);
int32_t LocalFileGetCapability(const LocalFile *self);
int64_t LocalFileGetFileSize(const LocalFile *self);
int32_t LocalFileOpenArg(LocalFile *self, const FileOpenArg *param);
int32_t LocalFileOpenAsync(LocalFile *self, const FileOpenArg *param);
int32_t LocalFileClose(LocalFile *self);
int32_t LocalFileCloseAsync(LocalFile *self);
unsigned char LocalFileIsOpened(const LocalFile *self);
unsigned char LocalFileIsEof(LocalFile *self);
int32_t LocalFileAbort(LocalFile *self);
int32_t LocalFileRead(LocalFile *self, void *buf, size_t nbyte);
int32_t LocalFileReadAsync(LocalFile *self, void *buf, size_t nbyte);
int32_t LocalFileWrite(LocalFile *self, const void *buf, size_t nbyte);
int32_t LocalFileWriteAsync(LocalFile *self, const void *buf, size_t nbyte);
int64_t LocalFileSeek(LocalFile *self, int64_t offset, int32_t whence);
int32_t LocalFileSeekAsync(LocalFile *self, int64_t offset, int32_t whence);
int32_t LocalFileFlush(LocalFile *self);
int32_t LocalFileFlushAsync(LocalFile *self);
int32_t LocalFileWaitAsync(LocalFile *self, int64_t *result);
int32_t LocalFilePollAsync(LocalFile *self, int64_t *result);
int32_t LocalFileGetstat(LocalFile *self, FileStat *stat);
int32_t LocalFileSetPriority(LocalFile *self, int32_t priority);

#define LocalFileOpen ScePafMisc_B9FB9BD6
#define LocalFileConstruct ScePafMisc_DFE98F6E

LocalFile *LocalFileConstruct(LocalFile *self);
void LocalFileDefaultDelete(File *file);

void LocalFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		   uint32_t mode, int32_t *error);

#endif
