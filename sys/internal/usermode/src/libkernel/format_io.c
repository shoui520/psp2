#include <scetypes.h>
#include "format.h"

extern int sceIoWrite(SceUID descriptor, const void *data, SceSize size);

int FormatStringLength(const char *string)
{
	const char *end = string;

	while (*end)
		end++;
	return (uintptr_t)end - (uintptr_t)string;
}

unsigned int FormatStringNext(const unsigned char **cursor)
{
	unsigned int character = **cursor;

	(*cursor)++;
	return character;
}

int FormatGetCharacter(const char **cursor, int peek)
{
	int character = *(const signed char *)*cursor;

	if (!peek)
		(*cursor)++;
	return character;
}

void FormatInitializeFile(void *argument, uintptr_t state)
{
	struct FormatFile *file = argument;

	file->buffered = 0;
}

int FormatPutFile(void *argument, uintptr_t *state, int character)
{
	struct FormatFile *file = argument;
	int count = file->buffered;
	int descriptor = file->descriptor;
	int next = count + 1;

	file->buffered = next;
	file->data[count] = character;
	if (next >= 1024) {
		if (descriptor >= 0)
			sceIoWrite(descriptor, file->data, next);
		file->buffered = 0;
	}
	return 1;
}

void FormatFinishFile(void *argument)
{
	struct FormatFile *file = argument;
	int count = file->buffered;
	int descriptor = file->descriptor;

	if (count > 0) {
		if (descriptor >= 0)
			sceIoWrite(descriptor, file->data, count);
		file->buffered = 0;
	}
}

int FormatPutBuffer(void *argument, uintptr_t *remaining, int character)
{
	struct FormatBuffer *buffer = argument;
	char *cursor;

	if (*remaining) {
		cursor = buffer->cursor;
		if (cursor) {
			buffer->cursor = cursor + 1;
			*cursor = character;
		}
		--*remaining;
	}
	return 1;
}

const struct FormatOps formatFileOps = {
	.kind = 2,
	.initialize = FormatInitializeFile,
	.finish = FormatFinishFile,
	.put = FormatPutFile,
	.get = FormatGetCharacter,
	.stringLength = FormatStringLength,
	.stringNext = FormatStringNext,
};

const struct FormatOps formatBufferOps = {
	.kind = 0,
	.put = FormatPutBuffer,
	.get = FormatGetCharacter,
	.stringLength = FormatStringLength,
	.stringNext = FormatStringNext,
};
