#ifndef PAF_FILE_BUFFEREDFILE_H
#define PAF_FILE_BUFFEREDFILE_H

#include "file.h"
#include "../buffer/buffer.h"

#define BufferedFileOpen ScePafMisc_228686A8

void BufferedFileOpen(FileSharedPtr *result, FileSharedPtr *file,
		      int32_t *error, BufferSharedPtr *buffer);

#endif
