#ifndef PAF_STD_PRINTF_H
#define PAF_STD_PRINTF_H

#include <stdarg.h>
#include <stdint.h>

typedef void *(*PafWideFormatOutput)(void *context, int32_t ch);

int ScePafStdc_AFB23563(PafWideFormatOutput output, void *context,
		      const uint16_t *format, va_list args);

#endif
