/*
 * stdio.h: the SDK's standard input and output, with Sony's psp2 declarations (SDK Release
 * 03.570.011, target/include/stdio.h and yvals.h). FILE and fpos_t are the user-mode C
 * library's records; their layouts, and the macro forms of getc and putc that read them, come
 * with the user-mode SDK headers.
 */
#ifndef _STDIO
#define _STDIO

#include <stdarg.h>

#ifndef NULL
#define NULL		0
#endif

#define _IOFBF		0
#define _IOLBF		1
#define _IONBF		2

#define BUFSIZ		(64 * 1024)
#define EOF		(-1)
#define FILENAME_MAX	1024
#define FOPEN_MAX	128

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#define stdin		(&_Stdin)
#define stdout		(&_Stdout)
#define stderr		(&_Stderr)

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

typedef struct fpos_t fpos_t;
#ifndef _FILET
#define _FILET
typedef struct _Dnk_filet _Filet;
#endif
typedef _Filet FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE _Stdin, _Stdout, _Stderr;

void clearerr(FILE *stream);
int fclose(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
int fgetc(FILE *stream);
int fgetpos(FILE *__restrict stream, fpos_t *__restrict pos);
char *fgets(char *__restrict s, int n, FILE *__restrict stream);
FILE *fopen(const char *__restrict filename, const char *__restrict mode);
int fprintf(FILE *__restrict stream, const char *__restrict format, ...);
int fputc(int c, FILE *stream);
int fputs(const char *__restrict s, FILE *__restrict stream);
size_t fread(void *__restrict ptr, size_t size, size_t nelem, FILE *__restrict stream);
FILE *freopen(const char *__restrict filename, const char *__restrict mode,
	      FILE *__restrict stream);
int fscanf(FILE *__restrict stream, const char *__restrict format, ...);
int fseek(FILE *stream, long offset, int mode);
int fsetpos(FILE *stream, const fpos_t *pos);
long ftell(FILE *stream);
size_t fwrite(const void *__restrict ptr, size_t size, size_t nelem, FILE *__restrict stream);
char *gets(char *s);
void perror(const char *s);
int printf(const char *__restrict format, ...);
int puts(const char *s);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
void rewind(FILE *stream);
int scanf(const char *__restrict format, ...);
void setbuf(FILE *__restrict stream, char *__restrict buf);
int setvbuf(FILE *__restrict stream, char *__restrict buf, int mode, size_t size);
int sprintf(char *__restrict s, const char *__restrict format, ...);
int sscanf(const char *__restrict s, const char *__restrict format, ...);
int ungetc(int c, FILE *stream);
int vfprintf(FILE *__restrict stream, const char *__restrict format, va_list ap);
int vprintf(const char *__restrict format, va_list ap);
int vsprintf(char *__restrict s, const char *__restrict format, va_list ap);
int snprintf(char *__restrict s, size_t n, const char *__restrict format, ...);
int vsnprintf(char *__restrict s, size_t n, const char *__restrict format, va_list ap);
int vfscanf(FILE *__restrict stream, const char *__restrict format, va_list ap);
int vscanf(const char *__restrict format, va_list ap);
int vsscanf(const char *__restrict s, const char *__restrict format, va_list ap);
int getc(FILE *stream);
int getchar(void);
int putc(int c, FILE *stream);
int putchar(int c);

/* extensions */
FILE *fdopen(int fd, const char *mode);
int fileno(FILE *stream);
FILE *sceLibcFopenWithFD(int fd, const char *mode);
FILE *sceLibcFopenWithFH(signed int fh, const char *mode);
int sceLibcGetFD(FILE *stream);
signed int sceLibcGetFH(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO */
