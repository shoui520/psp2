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
#ifdef PAF_BUILD_2014
#define PNG_NO_MNG_FEATURES
#define PNG_ABORT() ((void)0)
#endif

#ifdef PAF_BUILD_2014
#include <math.h>
#endif

#define malloc	sce_paf_malloc
#define free	sce_paf_free
#define memcmp	sce_paf_memcmp
#define strlen	sce_paf_strlen
#define abs	sce_paf_abs
#define floor	sce_paf_floor
#ifdef PAF_BUILD_2014
#define pow(x, y)	((double)sce_paf_powf((float)(x), (float)(y)))
#else
#define pow	sce_paf_pow
#endif
#define setjmp	sce_paf_setjmp
#define longjmp	sce_paf_longjmp

#endif
