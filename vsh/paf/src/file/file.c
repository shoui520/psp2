#include "file.h"
#include "localfile.h"
#include "httpfile.h"
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

const FileHolderClass fileHolderClass = {
	0, NULL, {
		{
			FILE_ABSTRACT_METHODS,
			(int32_t (*)(File *, FileStat *))FileHolderGetstat,
			(int32_t (*)(File *, int32_t))FileHolderSetPriority,
			(File *(*)(File *))FileHolderDestroy,
			(void (*)(File *))FileHolderDelete
		},
		FileHolderSetFile, FileHolderClearFile, FileHolderGetFile
	}
};

int32_t FileHolderGetstat(FileHolder *self, FileStat *stat)
{
	File *file = self->m_file.m_ptr;

	if (file)
		return file->vtable->Getstat(file, stat);
	return SCE_PAF_ERROR_FILE_PROTECTED;
}

int32_t FileHolderSetPriority(FileHolder *self, int32_t priority)
{
	File *file = self->m_file.m_ptr;

	if (file)
		return file->vtable->SetPriority(file, priority);
	return SCE_PAF_ERROR_FILE_PROTECTED;
}

void FileHolderSetFile(FileSharedPtr *result, FileHolder *self, FileSharedPtr *file)
{
	FileSharedPtrCopy(result, &self->m_file);
	FileSharedPtrAssign(&self->m_file, file);
}

void FileHolderClearFile(FileSharedPtr *result, FileHolder *self)
{
	const FileHolderVTable *vtable = (const FileHolderVTable *)self->base.vtable;
	FileSharedPtr empty = { NULL, NULL };

	vtable->SetFile(result, self, &empty);
	FileSharedPtrRelease(&empty);
}

void FileHolderGetFile(FileSharedPtr *result, const FileHolder *self)
{
	FileSharedPtrCopy(result, &self->m_file);
}

void HttpFileDefaultDelete(File *file)
{
	if (file)
		file->vtable->Delete(file);
}

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

void LocalFileDefaultDelete(File *file)
{
	if (file)
		file->vtable->Delete(file);
}

FileHolder *FileHolderDestroy(FileHolder *self)
{
	if (self->m_file.m_ptr) {
		self->base.vtable = &fileHolderClass.methods.base;
		FileSharedPtrRelease(&self->m_file);
	}
	self->base.vtable = &fileClass.methods;
	return self;
}

void FileHolderDelete(FileHolder *self)
{
	FileHolderDestroy(self);
	ScePafStdc_1B77082E(self);
}
