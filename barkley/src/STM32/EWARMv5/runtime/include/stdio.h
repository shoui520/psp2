/*
 * stdio.h: what the program uses of IAR's C library (DLib), whose printf writes to the
 * debug UART (runtime/, rebuilt from the image).
 */
#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char *format, ...);
int putchar(int c);

#ifdef __cplusplus
}
#endif

#endif
