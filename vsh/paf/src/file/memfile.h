#ifndef PAF_FILE_MEMFILE_H
#define PAF_FILE_MEMFILE_H

#include "file.h"
#include "../buffer/buffer.h"
#include "../thread/mutex.h"

typedef struct MemFile {
	File base;
	unsigned char m_is_opened;
	unsigned char m_is_writable;
	BufferSharedPtr m_buffer;
	const void *m_mem_addr;
	size_t m_mem_size;
	int64_t m_mem_pos;
	unsigned char m_waiting_for_sync;
	int64_t m_io_result;
	RMutex m_lock_mutex;
} MemFile;

typedef struct MemFileOpenArg {
	uint32_t file_type;
	BufferSharedPtr buffer;
} MemFileOpenArg;

typedef char MemFileSizeCheck[sizeof(MemFile) == 80 ? 1 : -1];
typedef char MemFileBufferOffsetCheck[offsetof(MemFile, m_buffer) == 8 ? 1 : -1];
typedef char MemFilePositionOffsetCheck[offsetof(MemFile, m_mem_pos) == 24 ? 1 : -1];
typedef char MemFileResultOffsetCheck[offsetof(MemFile, m_io_result) == 40 ? 1 : -1];
typedef char MemFileMutexOffsetCheck[offsetof(MemFile, m_lock_mutex) == 48 ? 1 : -1];
typedef char MemFileOpenArgSizeCheck[sizeof(MemFileOpenArg) == 12 ? 1 : -1];

#define MemFileConstruct ScePafMisc_995435F4
#define MemFileConstructBase ScePafMisc_19BD0EAB
#define MemFileDestroy ScePafMisc_8AF3CC45
#define MemFileDestroyBase ScePafMisc_F4D60C5C
#define MemFileDelete ScePafMisc_71148CB2
#define MemFileOpenBuffer ScePafMisc_502E1BB0
#define MemFileOpen ScePafMisc_19FE55A8
#define MemFileOpenWritable ScePafMisc_26BF1B5B
#define MemFileOpenWritableBuffer ScePafMisc_77B378EC
#define MemFileGetFileType ScePafMisc_4BB2B084
#define MemFileGetCapability ScePafMisc_A6B33A00
#define MemFileGetFileSize ScePafMisc_6784C895
#define MemFileOpenArgMethod ScePafMisc_C4875233
#define MemFileOpenAsync ScePafMisc_F2A15807
#define MemFileClose ScePafMisc_4E981745
#define MemFileCloseAsync ScePafMisc_807CCDB4
#define MemFileIsOpened ScePafMisc_5C9FFE3E
#define MemFileIsEof ScePafMisc_B456B41B
#define MemFileAbort ScePafMisc_E3F118A8
#define MemFileRead ScePafMisc_871AFA61
#define MemFileReadAsync ScePafMisc_44FE3320
#define MemFileWrite ScePafMisc_A7756292
#define MemFileWriteAsync ScePafMisc_494E8CF8
#define MemFileSeek ScePafMisc_EF7C46F1
#define MemFileSeekAsync ScePafMisc_7BED7D7F
#define MemFileFlush ScePafMisc_4FBD37E4
#define MemFileFlushAsync ScePafMisc_964C67E5
#define MemFileWaitAsync ScePafMisc_4B8F8811
#define MemFilePollAsync ScePafMisc_564A5CA7
#define memFileClass ScePafMisc_2CBDFB23

extern const FileClass memFileClass;

MemFile *MemFileConstruct(MemFile *self);
MemFile *MemFileConstructBase(MemFile *self);
MemFile *MemFileDestroy(MemFile *self);
MemFile *MemFileDestroyBase(MemFile *self);
void MemFileDelete(MemFile *self);
void MemFileDefaultDelete(File *file);
void MemFileOpenBuffer(FileSharedPtr *result, const BufferSharedPtr *buffer, int32_t *error);
void MemFileOpen(FileSharedPtr *result, const void *addr, size_t size, int32_t *error);
void MemFileOpenWritable(FileSharedPtr *result, void *addr, size_t size, int32_t *error);
void MemFileOpenWritableBuffer(FileSharedPtr *result, const BufferSharedPtr *buffer,
			       int32_t *error);
int32_t MemFileGetFileType(const MemFile *self);
int32_t MemFileGetCapability(const MemFile *self);
int64_t MemFileGetFileSize(const MemFile *self);
int32_t MemFileOpenArgMethod(MemFile *self, const FileOpenArg *param);
int32_t MemFileOpenAsync(MemFile *self, const FileOpenArg *param);
int32_t MemFileClose(MemFile *self);
int32_t MemFileCloseAsync(MemFile *self);
unsigned char MemFileIsOpened(const MemFile *self);
unsigned char MemFileIsEof(MemFile *self);
int32_t MemFileAbort(MemFile *self);
int32_t MemFileRead(MemFile *self, void *buf, size_t nbyte);
int32_t MemFileReadAsync(MemFile *self, void *buf, size_t nbyte);
int32_t MemFileWrite(MemFile *self, const void *buf, size_t nbyte);
int32_t MemFileWriteAsync(MemFile *self, const void *buf, size_t nbyte);
int64_t MemFileSeek(MemFile *self, int64_t offset, int32_t whence);
int32_t MemFileSeekAsync(MemFile *self, int64_t offset, int32_t whence);
int32_t MemFileFlush(MemFile *self);
int32_t MemFileFlushAsync(MemFile *self);
int32_t MemFileWaitAsync(MemFile *self, int64_t *result);
int32_t MemFilePollAsync(MemFile *self, int64_t *result);

#endif
