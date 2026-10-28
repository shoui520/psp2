#ifndef PAF_FILE_LOCALFILE_H
#define PAF_FILE_LOCALFILE_H

#include "file.h"

#define LocalFileOpen ScePafMisc_B9FB9BD6

void LocalFileOpen(FileSharedPtr *result, const char *path, uint32_t flag,
		   uint32_t mode, int32_t *error);

#endif
