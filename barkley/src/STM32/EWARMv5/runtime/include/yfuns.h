/*
 * yfuns.h: the C library's low-level I/O (IAR DLib), which the program supplies.
 */
#ifndef _YFUNS_H
#define _YFUNS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* size bytes of buffer written to the handle (0 stdin, 1 stdout, 2 stderr); the bytes
   written, or (size_t)-1 */
size_t __write(int handle, const unsigned char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
