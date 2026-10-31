#include "file.h"
#include "../paf_types.h"

extern void ScePafStdc_2730262C(void); /* __cxa_pure_virtual */

#define FILE_ABSTRACT_METHODS \
	(int32_t (*)(const File *))ScePafStdc_2730262C, \
	(int32_t (*)(const File *))ScePafStdc_2730262C, \
	(int64_t (*)(const File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *, const FileOpenArg *))ScePafStdc_2730262C, \
	(int32_t (*)(File *, const FileOpenArg *))ScePafStdc_2730262C, \
	(int32_t (*)(File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *))ScePafStdc_2730262C, \
	(unsigned char (*)(const File *))ScePafStdc_2730262C, \
	(unsigned char (*)(File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *, void *, size_t))ScePafStdc_2730262C, \
	(int32_t (*)(File *, void *, size_t))ScePafStdc_2730262C, \
	(int32_t (*)(File *, const void *, size_t))ScePafStdc_2730262C, \
	(int32_t (*)(File *, const void *, size_t))ScePafStdc_2730262C, \
	(int64_t (*)(File *, int64_t, int32_t))ScePafStdc_2730262C, \
	(int32_t (*)(File *, int64_t, int32_t))ScePafStdc_2730262C, \
	(int32_t (*)(File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *))ScePafStdc_2730262C, \
	(int32_t (*)(File *, int64_t *))ScePafStdc_2730262C, \
	(int32_t (*)(File *, int64_t *))ScePafStdc_2730262C

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
	ScePafStdc_1B77082E(self);
}

void FileDefaultDelete(File *file)
{
	if (file)
		file->vtable->Delete(file);
}

