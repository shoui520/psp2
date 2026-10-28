#include "../compiler.h"
#include "printf.h"
#include "string.h"

typedef struct PafWideOutputState {
	int32_t limit;
	uint32_t count;
	uint16_t *next;
} PafWideOutputState;

typedef char PafWideOutputStateSizeCheck[sizeof(PafWideOutputState) == 12 ? 1 : -1];

static void *PafWideBufferOutput(void *context, int32_t ch)
{
	PafWideOutputState *state = context;

	if (ch >= 0x10000 || state->limit <= (int32_t)state->count) {
		uint16_t *next = state->next;

		*next = 0;
		return next;
	} else {
		uint16_t *next = state->next;
		uint32_t count = state->count;

		*next = ch;
		state->next = next + 1;
		state->count = count + 1;
		return state;
	}
}

static void *PafWideCountOutput(void *context, int32_t ch)
{
	PafWideOutputState *state = context;

	if (ch < 0x10000)
		state->count++;
	return state;
}

int sce_paf_swprintf(uint16_t *buffer, size_t bufsz, const uint16_t *format, ...)
{
	PafWideOutputState state = {
		.limit = (int32_t)(bufsz - 1),
		.count = 0,
		.next = buffer
	};
	PafWideFormatOutput output = bufsz ? PafWideBufferOutput : PafWideCountOutput;
	va_list args;
	int result;

	va_start(args, format);
	result = ScePafStdc_AFB23563(output, &state, format, args);
	va_end(args);
	return result;
}

int sce_paf_vswprintf(uint16_t *buffer, size_t bufsz, const uint16_t *format,
		     va_list args)
{
	PafWideOutputState state = {
		.limit = (int32_t)(bufsz - 1),
		.count = 0,
		.next = buffer
	};
	PafWideFormatOutput output = bufsz ? PafWideBufferOutput : PafWideCountOutput;

	return ScePafStdc_AFB23563(output, &state, format, args);
}
