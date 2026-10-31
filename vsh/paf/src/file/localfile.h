/*
 * The C view of paf::LocalFile (include/file/localfile.h) for paf's C
 * sources: its static Open, which returns the new file through result.
 */
#ifndef PAF_FILE_LOCALFILE_C_H
#define PAF_FILE_LOCALFILE_C_H

#include "file.h"

#define LocalFileOpen _ZN3paf9LocalFile4OpenEPKcjjPi

void LocalFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		   uint32_t mode, int32_t *error);

#endif
