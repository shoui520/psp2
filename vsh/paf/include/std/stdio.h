/* paf's stdio (ScePafStdc): its FILE streams and formatted output. */
#ifndef PAF_STD_STDIO_H
#define PAF_STD_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sce_paf_FILE {
	int32_t type;
	int32_t fd;
	uint32_t status;
	int32_t unget_char;
	int32_t seek_offset;
	int32_t buff_attr;
	int32_t buff_off;
	int32_t buff_len;
} sce_paf_FILE;

int sce_paf_feof(sce_paf_FILE *stream);
int sce_paf_ferror(sce_paf_FILE *stream);
int sce_paf_fileno(sce_paf_FILE *stream);
void sce_paf_clearerr(sce_paf_FILE *stream);
int sce_paf_ungetc(int ch, sce_paf_FILE *stream);
int sce_paf_fclose(sce_paf_FILE *stream);
int sce_paf_fflush(sce_paf_FILE *stream);
sce_paf_FILE *sce_paf_fopen(const char *filename, const char *mode);
sce_paf_FILE *sce_paf_fdopen(int fd, const char *mode);
size_t sce_paf_fread(void *ptr, size_t size, size_t count, sce_paf_FILE *stream);
size_t sce_paf_fwrite(const void *ptr, size_t size, size_t count, sce_paf_FILE *stream);
int sce_paf_fgetc(sce_paf_FILE *stream);
int sce_paf_fputc(int ch, sce_paf_FILE *stream);
int sce_paf_fseek(sce_paf_FILE *stream, long offset, int origin);
long sce_paf_ftell(sce_paf_FILE *stream);

int sce_paf_sprintf(char *buffer, const char *format, ...);
int sce_paf_snprintf(char *buffer, size_t bufsz, const char *format, ...);
int sce_paf_vsprintf(char *buffer, const char *format, va_list args);
int sce_paf_vsnprintf(char *buffer, size_t bufsz, const char *format, va_list args);
int sce_paf_swprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, ...);
int sce_paf_vswprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif
