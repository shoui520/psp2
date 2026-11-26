/*
 * 2014 PAF libpng-1.2.46 user configuration (included by pngconf.h when
 * PNG_USER_CONFIG is defined): disables stdio/console I/O and sCAL/tIME
 * chunks and redirects C library calls to ScePafStd.
 */
#ifndef PAF_LIBPNG12_PNGUSR_H
#define PAF_LIBPNG12_PNGUSR_H

#include <std/stdlib.h>
#include <std/string.h>
#include <std/setjmp.h>
#include "../../../std/math.h"

#define PNG_NO_CONSOLE_IO
#define PNG_NO_STDIO
#define PNG_NO_READ_sCAL
#define PNG_NO_WRITE_sCAL
#define PNG_NO_READ_tIME
#define PNG_NO_WRITE_tIME
#define PNG_USER_MEM_SUPPORTED

#define malloc	sce_paf_malloc
#define free	sce_paf_free
#define memcmp	sce_paf_memcmp
#define strlen	sce_paf_strlen
#define floor	sce_paf_floor
#define pow	sce_paf_pow
#define setjmp	sce_paf_setjmp
#define longjmp	sce_paf_longjmp

#endif
