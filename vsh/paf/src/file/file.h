#ifndef PAF_FILE_FILE_H
#define PAF_FILE_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <sce_atomic.h>
#include "../std/string.h"
#include <datetime/datetime.h>

typedef struct File File;
typedef struct FileOpenArg FileOpenArg;
typedef struct FileStat {
	uint16_t st_mode;
	uint32_t st_attr;
	int64_t st_size;
	DateTime st_ctime;
	DateTime st_atime;
	DateTime st_mtime;
} FileStat;

typedef char FileStatSizeCheck[sizeof(FileStat) == 88 ? 1 : -1];
typedef char FileStatTimeOffsetCheck[offsetof(FileStat, st_ctime) == 16 ? 1 : -1];

typedef enum FileType {
	Type_None = 0,
	Type_Local = 1,
	Type_Mem = 2,
	Type_Http = 3
} FileType;

typedef struct FileVTable {
	int32_t (*GetFileType)(const File *self);
	int32_t (*GetCapability)(const File *self);
	int64_t (*GetFileSize)(const File *self);
	int32_t (*Open)(File *self, const FileOpenArg *param);
	int32_t (*OpenAsync)(File *self, const FileOpenArg *param);
	int32_t (*Close)(File *self);
	int32_t (*CloseAsync)(File *self);
	unsigned char (*IsOpened)(const File *self);
	unsigned char (*IsEof)(File *self);
	int32_t (*Abort)(File *self);
	int32_t (*Read)(File *self, void *buf, size_t nbyte);
	int32_t (*ReadAsync)(File *self, void *buf, size_t nbyte);
	int32_t (*Write)(File *self, const void *buf, size_t nbyte);
	int32_t (*WriteAsync)(File *self, const void *buf, size_t nbyte);
	int64_t (*Seek)(File *self, int64_t offset, int32_t whence);
	int32_t (*SeekAsync)(File *self, int64_t offset, int32_t whence);
	int32_t (*Flush)(File *self);
	int32_t (*FlushAsync)(File *self);
	int32_t (*WaitAsync)(File *self, int64_t *result);
	int32_t (*PollAsync)(File *self, int64_t *result);
	int32_t (*Getstat)(File *self, FileStat *stat);
	int32_t (*SetPriority)(File *self, int32_t priority);
	File *(*Destroy)(File *self);
	void (*Delete)(File *self);
} FileVTable;

struct File {
	const FileVTable *vtable;
};

typedef struct FileRefCounter {
	File *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(File *ptr);
} FileRefCounter;

typedef struct FileSharedPtr {
	File *m_ptr;
	FileRefCounter *m_ref_counter;
} FileSharedPtr;

/* The private shared-file base used by BufferedFile. */
typedef struct FileHolder {
	File base;
	FileSharedPtr m_file;
} FileHolder;

typedef struct FileHolderVTable {
	FileVTable base;
	void (*SetFile)(FileSharedPtr *result, FileHolder *self, FileSharedPtr *file);
	void (*ClearFile)(FileSharedPtr *result, FileHolder *self);
	void (*GetFile)(FileSharedPtr *result, const FileHolder *self);
} FileHolderVTable;

typedef struct FileClass {
	int offset_to_top;
	const void *type_info;
	FileVTable methods;
} FileClass;

typedef struct FileHolderClass {
	int offset_to_top;
	const void *type_info;
	FileHolderVTable methods;
} FileHolderClass;

extern const FileClass fileClass;
extern const FileHolderClass fileHolderClass;

int32_t FileGetstat(File *self, FileStat *stat);
int32_t FileSetPriority(File *self, int32_t priority);
File *FileDestroy(File *self);
void FileDelete(File *self);
int32_t FileHolderGetstat(FileHolder *self, FileStat *stat);
int32_t FileHolderSetPriority(FileHolder *self, int32_t priority);
void FileHolderSetFile(FileSharedPtr *result, FileHolder *self, FileSharedPtr *file);
void FileHolderClearFile(FileSharedPtr *result, FileHolder *self);
void FileHolderGetFile(FileSharedPtr *result, const FileHolder *self);
FileHolder *FileHolderDestroy(FileHolder *self);
void FileHolderDelete(FileHolder *self);
void FileDefaultDelete(File *file);

typedef char FileSizeCheck[sizeof(File) == 4 ? 1 : -1];
typedef char FileVTableSizeCheck[sizeof(FileVTable) == 96 ? 1 : -1];
typedef char FileReadOffsetCheck[offsetof(FileVTable, Read) == 40 ? 1 : -1];
typedef char FileSeekOffsetCheck[offsetof(FileVTable, Seek) == 56 ? 1 : -1];
typedef char FileRefCounterSizeCheck[sizeof(FileRefCounter) == 16 ? 1 : -1];
typedef char FileSharedPtrSizeCheck[sizeof(FileSharedPtr) == 8 ? 1 : -1];
typedef char FileHolderSizeCheck[sizeof(FileHolder) == 12 ? 1 : -1];
typedef char FileClassSizeCheck[sizeof(FileClass) == 104 ? 1 : -1];
typedef char FileHolderClassSizeCheck[sizeof(FileHolderClass) == 116 ? 1 : -1];

static inline __attribute__((always_inline))
void FileSharedPtrCopy(FileSharedPtr *self, const FileSharedPtr *src)
{
	self->m_ptr = src->m_ptr;
	self->m_ref_counter = src->m_ref_counter;
	if (self->m_ptr)
		sceAtomicIncrement32AcqRel(&self->m_ref_counter->m_counter);
}

static inline __attribute__((always_inline))
void FileSharedPtrRelease(const FileSharedPtr *self)
{
	if (self->m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_counter) == 1) {
		self->m_ref_counter->m_deleter(self->m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->m_ref_counter);
	}
}

static inline __attribute__((always_inline))
void FileSharedPtrAssign(FileSharedPtr *self, const FileSharedPtr *src)
{
	File *ptr = src->m_ptr;
	FileRefCounter *counter = src->m_ref_counter;

	if (ptr != self->m_ptr) {
		FileSharedPtrRelease(self);
		self->m_ptr = ptr;
		self->m_ref_counter = counter;
		if (ptr)
			sceAtomicIncrement32AcqRel(&counter->m_counter);
	}
}

#endif
