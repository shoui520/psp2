#include "../compiler.h"
#include "printf.h"

typedef struct PafOutputState {
	int32_t limit;
	uint32_t count;
	char *next;
} PafOutputState;

typedef char PafOutputStateSizeCheck[sizeof(PafOutputState) == 12 ? 1 : -1];

static void *PafStringOutput(void *context, int32_t ch)
{
	char **cursor = context;
	char *next = *cursor;

	if (ch < 0x100) {
		*cursor = next + 1;
		*next = ch;
		return context;
	}
	*next = 0;
	return next;
}

static void *PafBufferOutput(void *context, int32_t ch)
{
	PafOutputState *state = context;
	char *next;

	if (ch < 0x100 && state->limit > (int32_t)state->count) {
		next = state->next;
		state->next = next + 1;
		*next = ch;
		state->count++;
		return state;
	}
	next = state->next;
	*next = 0;
	return next;
}

static void *PafCountOutput(void *context, int32_t ch)
{
	PafOutputState *state = context;

	if (ch < 0x100)
		state->count++;
	return state;
}

int sce_paf_sprintf(char *buffer, const char *format, ...)
{
	char *next = buffer;
	va_list args;
	int result;

	va_start(args, format);
	result = ScePafStdc_AE0F3820(PafStringOutput, &next, format, args);
	va_end(args);
	return result;
}

int sce_paf_snprintf(char *buffer, size_t bufsz, const char *format, ...)
{
	PafOutputState state = {
		.limit = (int32_t)(bufsz - 1),
		.count = 0,
		.next = buffer
	};
	PafFormatOutput output = bufsz ? PafBufferOutput : PafCountOutput;
	va_list args;
	int result;

	va_start(args, format);
	result = ScePafStdc_AE0F3820(output, &state, format, args);
	va_end(args);
	return result;
}

int sce_paf_vsprintf(char *buffer, const char *format, va_list args)
{
	char *next = buffer;

	return ScePafStdc_AE0F3820(PafStringOutput, &next, format, args);
}

int sce_paf_vsnprintf(char *buffer, size_t bufsz, const char *format, va_list args)
{
	PafOutputState state = {
		.limit = (int32_t)(bufsz - 1),
		.count = 0,
		.next = buffer
	};
	PafFormatOutput output = bufsz ? PafBufferOutput : PafCountOutput;

	return ScePafStdc_AE0F3820(output, &state, format, args);
}
