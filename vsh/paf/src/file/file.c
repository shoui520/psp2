#include "file.h"
#include "../paf_types.h"
#include "../std/new.h"


#define FILE_ABSTRACT_METHODS \
	(int32_t (*)(const File *))__cxa_pure_virtual, \
	(int32_t (*)(const File *))__cxa_pure_virtual, \
	(int64_t (*)(const File *))__cxa_pure_virtual, \
	(int32_t (*)(File *, const FileOpenArg *))__cxa_pure_virtual, \
	(int32_t (*)(File *, const FileOpenArg *))__cxa_pure_virtual, \
	(int32_t (*)(File *))__cxa_pure_virtual, \
	(int32_t (*)(File *))__cxa_pure_virtual, \
	(unsigned char (*)(const File *))__cxa_pure_virtual, \
	(unsigned char (*)(File *))__cxa_pure_virtual, \
	(int32_t (*)(File *))__cxa_pure_virtual, \
	(int32_t (*)(File *, void *, size_t))__cxa_pure_virtual, \
	(int32_t (*)(File *, void *, size_t))__cxa_pure_virtual, \
	(int32_t (*)(File *, const void *, size_t))__cxa_pure_virtual, \
	(int32_t (*)(File *, const void *, size_t))__cxa_pure_virtual, \
	(int64_t (*)(File *, int64_t, int32_t))__cxa_pure_virtual, \
	(int32_t (*)(File *, int64_t, int32_t))__cxa_pure_virtual, \
	(int32_t (*)(File *))__cxa_pure_virtual, \
	(int32_t (*)(File *))__cxa_pure_virtual, \
	(int32_t (*)(File *, int64_t *))__cxa_pure_virtual, \
	(int32_t (*)(File *, int64_t *))__cxa_pure_virtual

const FileClass fileClass = {
	0, NULL, {
		FILE_ABSTRACT_METHODS,
		FileGetstat, FileSetPriority, FileDestroy, FileDelete
	}
};

int32_t FileGetstat(File *self, FileStat *stat)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t FileSetPriority(File *self, int32_t priority)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

File *FileDestroy(File *self)
{
	self->vtable = &fileClass.methods;
	return self;
}

void FileDelete(File *self)
{
	self->vtable = &fileClass.methods;
	PafDelete(self);
}

void FileDefaultDelete(File *file)
{
	if (file)
		file->vtable->Delete(file);
}

