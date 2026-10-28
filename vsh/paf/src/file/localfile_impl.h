#ifndef PAF_FILE_LOCALFILE_IMPL_H
#define PAF_FILE_LOCALFILE_IMPL_H

#include "file.h"

typedef struct LocalFileImpl LocalFileImpl;

typedef struct LocalFileImplVTable {
	LocalFileImpl *(*Destroy)(LocalFileImpl *self);
	void (*Delete)(LocalFileImpl *self);
	int32_t (*Open)(LocalFileImpl *self, const char *filename, uint32_t flag,
			uint32_t mode);
	int32_t (*Close)(LocalFileImpl *self);
	int32_t (*Read)(LocalFileImpl *self, void *buf, size_t nbyte);
	int32_t (*Write)(LocalFileImpl *self, const void *buf, size_t nbyte);
	int64_t (*Seek)(LocalFileImpl *self, int64_t offset, int32_t whence);
	int32_t (*Flush)(LocalFileImpl *self);
	int32_t (*OpenAsync)(LocalFileImpl *self, const char *filename, uint32_t flag,
			     uint32_t mode);
	int32_t (*CloseAsync)(LocalFileImpl *self);
	int32_t (*ReadAsync)(LocalFileImpl *self, void *buf, size_t nbyte);
	int32_t (*WriteAsync)(LocalFileImpl *self, const void *buf, size_t nbyte);
	int32_t (*SeekAsync)(LocalFileImpl *self, int64_t offset, int32_t whence);
	int32_t (*FlushAsync)(LocalFileImpl *self);
	int32_t (*WaitAsync)(LocalFileImpl *self, int64_t *result);
	int32_t (*PollAsync)(LocalFileImpl *self, int64_t *result);
	unsigned char (*IsOpened)(LocalFileImpl *self);
	unsigned char (*IsEof)(LocalFileImpl *self);
	int32_t (*Abort)(LocalFileImpl *self);
	int64_t (*GetFileSize)(LocalFileImpl *self);
	int32_t (*Getstat)(LocalFileImpl *self, FileStat *stat);
	int32_t (*SetPriority)(LocalFileImpl *self, int32_t priority);
} LocalFileImplVTable;

struct LocalFileImpl {
	const LocalFileImplVTable *vtable;
};

typedef char LocalFileImplSizeCheck[sizeof(LocalFileImpl) == 4 ? 1 : -1];
typedef char LocalFileImplVTableSizeCheck[sizeof(LocalFileImplVTable) == 88 ? 1 : -1];

#endif
