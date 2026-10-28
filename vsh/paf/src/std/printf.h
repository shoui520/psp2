#ifndef PAF_STD_PRINTF_H
#define PAF_STD_PRINTF_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef void *(*PafFormatOutput)(void *context, int32_t ch);
typedef PafFormatOutput PafWideFormatOutput;

int ScePafStdc_AE0F3820(PafFormatOutput output, void *context,
		      const char *format, va_list args);
int sce_paf_sprintf(char *buffer, const char *format, ...);
int sce_paf_snprintf(char *buffer, size_t bufsz, const char *format, ...);
int sce_paf_vsprintf(char *buffer, const char *format, va_list args);
int sce_paf_vsnprintf(char *buffer, size_t bufsz, const char *format, va_list args);

int ScePafStdc_AFB23563(PafWideFormatOutput output, void *context,
		      const uint16_t *format, va_list args);
int PafWideFormatBody(PafWideFormatOutput output, void *context,
		      const uint16_t *format, va_list args, uint16_t *buffer);

#endif
