/*
 * The C view of paf::BufferedFile (include/file/bufferedfile.h) for paf's C
 * sources: its static Open, which returns the new file through result.
 */
#ifndef PAF_FILE_BUFFEREDFILE_C_H
#define PAF_FILE_BUFFEREDFILE_C_H

#include "file.h"
#include "../buffer/buffer.h"

#define BufferedFileOpen _ZN3paf12BufferedFile4OpenENS_6common9SharedPtrINS_4FileEEEPiNS2_INS_6BufferEEE

void BufferedFileOpen(FileSharedPtr *result, FileSharedPtr *file,
		      int32_t *error, BufferSharedPtr *buffer);

#endif
