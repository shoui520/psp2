/* Bounded formatter output and its buffer callback. */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

struct FormatBuffer {
	uint32_t capacity;
	uint32_t count;
	char *cursor;
};

typedef void (*FormatOutput)(struct FormatBuffer *buffer, int character);
extern int __prnt(FormatOutput output, struct FormatBuffer *buffer,
		  const char *format, va_list arguments);

__attribute__((noinline))
void FormatBufferCharacter(struct FormatBuffer *buffer, int character)
{
	if (character == 0x200)
		return;
	if (character == 0x201) {
		if (buffer->capacity)
			*buffer->cursor++ = '\0';
		return;
	}
	if (buffer->count == UINT32_MAX)
		return;
	buffer->count++;
	if (buffer->capacity > buffer->count)
		*buffer->cursor++ = character;
}

int vsnprintf(char *destination, size_t capacity, const char *format,
	      va_list arguments)
{
	struct FormatBuffer buffer = { capacity, 0, destination };

	__prnt(FormatBufferCharacter, &buffer, format, arguments);
	return buffer.count;
}

int snprintf(char *destination, size_t capacity, const char *format, ...)
{
	struct FormatBuffer buffer = { capacity, 0, destination };
	va_list arguments;

	va_start(arguments, format);
	__prnt(FormatBufferCharacter, &buffer, format, arguments);
	va_end(arguments);
	return buffer.count;
}
