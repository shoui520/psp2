/*
 * What libpaf's libpng calls in place of the C library (included first by pngpriv.h, as
 * PNG_USER_CONFIG has it): paf's allocator, comparison, string length, mathematics and non-local
 * jumps, as Sony's build calls them (png_malloc, png_sig_cmp, png_check_keyword, png_gamma_correct,
 * png_create_read_struct_2 at TOOL 0x8116fc1c, 0x8116d182, 0x81179b68, 0x8116eab4, 0x8116fd34).
 * Sony's file is not known.
 */
#ifndef PAF_PNGUSR_H
#define PAF_PNGUSR_H

#include <std/stdlib.h>
#include <std/string.h>
#include <std/setjmp.h>
#include "../../../std/math.h"

#define malloc	sce_paf_malloc
#define free	sce_paf_free
#define memcmp	sce_paf_memcmp
#define strlen	sce_paf_strlen
#define floor	sce_paf_floor
#define pow	sce_paf_pow
#define setjmp	sce_paf_setjmp
#define longjmp	sce_paf_longjmp

#endif
