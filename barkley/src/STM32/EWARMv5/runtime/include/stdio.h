/*
 * stdio.h: what the program uses of IAR's C library (DLib, Dinkumware's), whose printf
 * writes to the debug UART (runtime/, rebuilt from the image): the streams and their
 * buffers.
 */
#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF		(-1)
#define BUFSIZ		512
#define FOPEN_MAX	8

typedef struct {
	unsigned long _Wchar;
	unsigned long _State;
} _Mbstatet;

/* a stream: its mode (_M*), lock and handle; its buffer and the positions in it of the
   next character, the end of what can be read and written, and the characters pushed
   back; the wide-oriented positions and conversion state; a temporary file's name */
typedef struct _Filet {
	unsigned short _Mode;
	unsigned char _Lockno;
	signed char _Handle;
	unsigned char *_Buf, *_Bend, *_Next;
	unsigned char *_Rend, *_Wend, *_Rback;
	unsigned short *_WRback, _WBack[2];
	unsigned char *_Rsave, *_WRend, *_WWend;
	_Mbstatet _Wstate;
	char *_Tmpnam;
	unsigned char _Back[1], _Cbuf;
} FILE;

extern FILE _Stdin, _Stdout, _Stderr;

#define stdin	(&_Stdin)
#define stdout	(&_Stdout)
#define stderr	(&_Stderr)

int fclose(FILE *stream);
int fflush(FILE *stream);
int fputc(int c, FILE *stream);
int printf(const char *format, ...);
int putchar(int c);
int remove(const char *name);
int vsprintf(char *s, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif
