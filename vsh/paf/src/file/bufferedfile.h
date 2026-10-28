#ifndef PAF_FILE_BUFFEREDFILE_H
#define PAF_FILE_BUFFEREDFILE_H

#include "file.h"
#include "../buffer/buffer.h"
#include "../thread/mutex.h"

typedef struct BufferedFileState {
	void *m_addr;
	size_t m_capacity;
	size_t m_count;
	int64_t m_buffer_start;
	int64_t m_position;
	int64_t m_file_position;
	int64_t m_file_size;
	unsigned char m_dirty;
} BufferedFileState;

typedef struct BufferedFile {
	File base;
	FileSharedPtr m_file;
	RMutex lock_mutex;
	BufferSharedPtr m_buffer;
	BufferedFileState *m_state;
} BufferedFile;

typedef struct BufferedFileVTable {
	FileVTable base;
	void (*SetFile)(FileSharedPtr *result, BufferedFile *self,
			FileSharedPtr *file);
	void (*ClearFile)(FileSharedPtr *result, BufferedFile *self);
	void (*GetFile)(FileSharedPtr *result, const BufferedFile *self);
	int32_t (*Open)(BufferedFile *self, const FileOpenArg *param,
			BufferSharedPtr *buffer);
} BufferedFileVTable;

typedef char BufferedFileStateSizeCheck[sizeof(BufferedFileState) == 56 ? 1 : -1];
typedef char BufferedFileSizeCheck[sizeof(BufferedFile) == 64 ? 1 : -1];
typedef char BufferedFileMutexOffsetCheck[offsetof(BufferedFile, lock_mutex) == 16 ? 1 : -1];
typedef char BufferedFileBufferOffsetCheck[offsetof(BufferedFile, m_buffer) == 48 ? 1 : -1];
typedef char BufferedFileStateOffsetCheck[offsetof(BufferedFile, m_state) == 56 ? 1 : -1];
typedef char BufferedFileVTableSizeCheck[sizeof(BufferedFileVTable) == 112 ? 1 : -1];

#define BufferedFileGetFileType ScePafMisc_AD71B82B
#define BufferedFileGetCapability ScePafMisc_4CE3C136
#define BufferedFileOpenArg ScePafMisc_0BF3BB31
#define BufferedFileOpenAsync ScePafMisc_4ECC56A4
#define BufferedFileClose ScePafMisc_3D960AD2
#define BufferedFileCloseAsync ScePafMisc_C089B864
#define BufferedFileIsOpened ScePafMisc_A5DE4664
#define BufferedFileIsEof ScePafMisc_772634EA
#define BufferedFileAbort ScePafMisc_364D2AE7
#define BufferedFileRead ScePafMisc_04D2DDC1
#define BufferedFileReadAsync ScePafMisc_72D393AD
#define BufferedFileWrite ScePafMisc_A79D9852
#define BufferedFileWriteAsync ScePafMisc_899AD604
#define BufferedFileSeek ScePafMisc_326FDFA7
#define BufferedFileSeekAsync ScePafMisc_C09E34B0
#define BufferedFileFlush ScePafMisc_4338FEC7
#define BufferedFileFlushAsync ScePafMisc_AD99F77E
#define BufferedFileWaitAsync ScePafMisc_DDC527B9
#define BufferedFilePollAsync ScePafMisc_CD57D018

#define BufferedFileOpen ScePafMisc_228686A8

void BufferedFileOpen(FileSharedPtr *result, FileSharedPtr *file,
		      int32_t *error, BufferSharedPtr *buffer);
void BufferedFileCreateDefaultBuffer(BufferSharedPtr *result);
int64_t BufferedFileGetFileSize(const BufferedFile *self);
int32_t BufferedFileGetFileType(const BufferedFile *self);
int32_t BufferedFileGetCapability(const BufferedFile *self);
int32_t BufferedFileOpenArg(BufferedFile *self, const FileOpenArg *param);
int32_t BufferedFileOpenAsync(BufferedFile *self, const FileOpenArg *param);
int32_t BufferedFileWriteBuffer(FileSharedPtr *file, BufferedFileState *state);
int32_t BufferedFileFlushDirty(FileSharedPtr *file, BufferedFileState *state);
void BufferedFileResetBuffer(FileSharedPtr *file, BufferedFileState *state);
int32_t BufferedFileClose(BufferedFile *self);
int32_t BufferedFileCloseAsync(BufferedFile *self);
unsigned char BufferedFileIsOpened(const BufferedFile *self);
unsigned char BufferedFileIsEof(BufferedFile *self);
int32_t BufferedFileAbort(BufferedFile *self);
int32_t BufferedFileRead(BufferedFile *self, void *buf, size_t nbyte);
int32_t BufferedFileReadAsync(BufferedFile *self, void *buf, size_t nbyte);
int32_t BufferedFileWrite(BufferedFile *self, const void *buf, size_t nbyte);
int32_t BufferedFileWriteAsync(BufferedFile *self, const void *buf, size_t nbyte);
int64_t BufferedFileSeek(BufferedFile *self, int64_t offset, int32_t whence);
int32_t BufferedFileSeekAsync(BufferedFile *self, int64_t offset, int32_t whence);
int32_t BufferedFileFlush(BufferedFile *self);
int32_t BufferedFileFlushAsync(BufferedFile *self);
int32_t BufferedFileWaitAsync(BufferedFile *self, int64_t *result);
int32_t BufferedFilePollAsync(BufferedFile *self, int64_t *result);

#endif
