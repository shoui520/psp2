/*
 * zlib's zconf.h includes <sys/types.h> for off_t under STDC; the SDK's C library has no
 * such header. zlib 1.2.5 uses off_t only for _LARGEFILE64_SOURCE, which nothing sets.
 */
#ifndef ZLIB_COMPAT_SYS_TYPES_H
#define ZLIB_COMPAT_SYS_TYPES_H

typedef long off_t;

#endif
