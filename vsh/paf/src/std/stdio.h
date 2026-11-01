/* paf's FILE streams, the parts only stdio.c uses. */
#ifndef PAF_SRC_STD_STDIO_H
#define PAF_SRC_STD_STDIO_H

#include <std/stdio.h>

enum PafFileStatus {
	PAF_FILE_STATUS_ERROR = 1,
	PAF_FILE_STATUS_EOF = 2
};

#define _stdinit_stdio sce_paf_private_fflush

void *_stdinit_stdio(void);

#endif
