/*
 * libjpeg's configuration for libpaf (IJG 8c, contrib/jpeg-8c). Sony's jconfig.h is not
 * known; this one reproduces its build: an ANSI compiler with unsigned char and short, a
 * one-byte boolean (libpaf's jpeg_decompress_struct is 448 bytes, as jpeg_CreateDecompress
 * checks), no getenv and the fast integer DCT by default. The memory back end is jmemnobs, with Sony's change to allocate
 * from the paf heap the client data names (jpeg-8c-sce.patch).
 */
#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#define NO_GETENV

#define HAVE_BOOLEAN
typedef unsigned char boolean;

/* the fast integer DCT by default: default_decompress_parms and jpeg_set_defaults store 1 */
#define JDCT_DEFAULT	JDCT_IFAST
