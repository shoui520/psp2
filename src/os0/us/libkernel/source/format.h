#ifndef LIBKERNEL_FORMAT_H
#define LIBKERNEL_FORMAT_H

#include <stdint.h>
#include <stdarg.h>

struct FormatOps {
	int kind;
	void (*initialize)(void *context, uintptr_t state);
	void (*finish)(void *context);
	int (*put)(void *context, uintptr_t *state, int character);
	int (*get)(const char **cursor, int peek);
	int (*stringLength)(const char *string);
	unsigned int (*stringNext)(const unsigned char **cursor);
};

struct FormatFile {
	uint32_t reserved;
	int descriptor;
	uint32_t options;		/* cleared by the printf wrappers, read by nothing */
	int buffered;
	char data[1024];
};

struct FormatBuffer {
	uintptr_t limit;
	char *cursor;
};

extern const struct FormatOps formatFileOps;
extern const struct FormatOps formatBufferOps;

int FormatEngine(const struct FormatOps *operations, void *context,
		uintptr_t state, const char *format, va_list arguments);

#endif
