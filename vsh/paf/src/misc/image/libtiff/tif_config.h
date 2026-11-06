/*
 * libpaf's libtiff build configuration (configure's tif_config.h). The open modes are the
 * system's (SCE_O_RDONLY, SCE_O_WRONLY, SCE_O_RDWR, SCE_O_CREAT, SCE_O_TRUNC): Sony's
 * _TIFFgetMode gives "r" 0x1, "r+" 0x3, "a" 0x203 and "w" 0x603, and TIFFCleanup flushes
 * unless the mode is 0x1.
 */
#ifndef PAF_TIF_CONFIG_H
#define PAF_TIF_CONFIG_H

#include "tiffconf.h"

#define HAVE_ASSERT_H		1
#define HAVE_LIMITS_H		1

/* paf's C library in place of the system's (no <string.h>) */
#include <std/stdio.h>
#include <std/string.h>
#include "../../../std/stdlib.h"
#include "../../../std/math.h"
#include <std/setjmp.h>
#include <stdlib.h>
#include <math.h>

#define strlen		sce_paf_strlen
#define strcmp		sce_paf_strcmp
#define strncmp		sce_paf_strncmp
#define strncpy		sce_paf_strncpy
#define memcmp		sce_paf_memcmp
#define memcpy		sce_paf_memcpy
#define memmove		sce_paf_memmove
#define memset		sce_paf_memset
#define snprintf	sce_paf_snprintf
#define qsort		sce_paf_qsort
#define bsearch		sce_paf_bsearch
#define rand		sce_paf_rand
#define setjmp		sce_paf_setjmp
#define longjmp		sce_paf_longjmp

/* paf's single-precision mathematics in place of the C library's double */
#define pow		sce_paf_powf
#define floor		sce_paf_floorf
#define log		sce_paf_logf
#define exp		sce_paf_expf
#define atan2		sce_paf_atan2f
#define sqrt		__builtin_sqrtf
#define fabs		__builtin_fabsf
/* and its pi, a float (tif_luv's hue angles) */
#define M_PI		3.14159265358979323846f

/* paf's rand gives 0 to 0x7fff (tif_luv's dithering) */
#undef RAND_MAX
#define RAND_MAX		0x7fff

#define TIFF_INT64_T		long long
#define TIFF_UINT64_T		unsigned long long

#define O_RDONLY		0x0001
#define O_WRONLY		0x0002
#define O_RDWR			0x0003
#define O_CREAT			0x0200
#define O_TRUNC			0x0400

#endif
