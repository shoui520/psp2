#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include "file.h"
#include "../thread/mutex.h"
#include "../memory/heap_allocator.h"

typedef struct LocalFileImpl LocalFileImpl;

typedef struct LocalFile {
	File base;
	RMutex m_lock_mutex;
	LocalFileImpl *m_pimpl;
	HeapAllocator *m_allocator;
} LocalFile;

typedef char LocalFileSizeCheck[sizeof(LocalFile) == 48 ? 1 : -1];
typedef char LocalFileMutexOffsetCheck[offsetof(LocalFile, m_lock_mutex) == 8 ? 1 : -1];
typedef char LocalFileImplOffsetCheck[offsetof(LocalFile, m_pimpl) == 40 ? 1 : -1];

#define LocalFileOpen ScePafMisc_B9FB9BD6
#define LocalFileConstruct ScePafMisc_DFE98F6E

LocalFile *LocalFileConstruct(LocalFile *self);
void LocalFileDefaultDelete(File *file);

void LocalFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		   uint32_t mode, int32_t *error);

#endif
