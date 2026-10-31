/* paf's formatted output (ScePafStdc). */
#ifndef PAF_STD_STDIO_H
#define PAF_STD_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int sce_paf_sprintf(char *buffer, const char *format, ...);
int sce_paf_snprintf(char *buffer, size_t bufsz, const char *format, ...);
int sce_paf_vsprintf(char *buffer, const char *format, va_list args);
int sce_paf_vsnprintf(char *buffer, size_t bufsz, const char *format, va_list args);
int sce_paf_vswprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif
