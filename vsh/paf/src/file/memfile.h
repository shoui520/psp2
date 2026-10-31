/*
 * The C view of paf::MemFile (include/file/memfile.h) for paf's C sources:
 * its static Open overloads, which return the new file through result. A
 * SharedPtr argument is passed by reference.
 */
#ifndef PAF_FILE_MEMFILE_C_H
#define PAF_FILE_MEMFILE_C_H

#include "file.h"
#include "../buffer/buffer.h"

#define MemFileOpen _ZN3paf7MemFile4OpenEPKvjPi
#define MemFileOpenWritableBuffer _ZN3paf7MemFile4OpenENS_6common9SharedPtrINS_6BufferEEEPi

void MemFileOpen(FileSharedPtr *result, const void *addr, size_t size, int32_t *error);
void MemFileOpenWritableBuffer(FileSharedPtr *result, const BufferSharedPtr *buffer,
			       int32_t *error);

#endif
