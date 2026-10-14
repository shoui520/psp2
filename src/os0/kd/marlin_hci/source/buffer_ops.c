/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"

static const unsigned char marlinObfuscationTable[11] = {
	0xf9, 0xa3, 0x97, 0xa2, 0x55, 0xbe, 0xf1, 0x53, 0xfc, 0x6b, 0x79
};

unsigned int MarlinNextEraseValue(unsigned int value)
{
	return (279470273U * value) % 0xfffffffbU;
}

void MarlinEraseMemory(void *data, unsigned int size)
{
	unsigned int value = (unsigned int)data;
	int index = 0;

	for (index = 0; index < size; index++) {
		value = MarlinNextEraseValue(value);
		((unsigned char *)data)[index] = value;
	}
}

int MarlinBufferEraseAndDestroy(MarlinBuffer *buffer)
{
	int result = 0;

	if (buffer != 0) {
		MarlinEraseMemory(buffer->data, buffer->size);
		MarlinBufferDestroy(0, buffer);
	}
	return result;
}

#define MARLIN_OBFUSCATE_BYTE() \
	data[--size] ^= marlinObfuscationTable[previous]; \
	previous = data[size] % 11U

void MarlinObfuscateData(unsigned char *data, unsigned int size)
{
	unsigned char previous = 0;

	switch ((size - 1) % 11U) {
	case 10:
		do {
			MARLIN_OBFUSCATE_BYTE();
	case 9:
			MARLIN_OBFUSCATE_BYTE();
	case 8:
			MARLIN_OBFUSCATE_BYTE();
	case 7:
			MARLIN_OBFUSCATE_BYTE();
	case 6:
			MARLIN_OBFUSCATE_BYTE();
	case 5:
			MARLIN_OBFUSCATE_BYTE();
	case 4:
			MARLIN_OBFUSCATE_BYTE();
	case 3:
			MARLIN_OBFUSCATE_BYTE();
	case 2:
			MARLIN_OBFUSCATE_BYTE();
	case 1:
			MARLIN_OBFUSCATE_BYTE();
	case 0:
			MARLIN_OBFUSCATE_BYTE();
		} while (size != 0);
	}
}

#undef MARLIN_OBFUSCATE_BYTE

#define MARLIN_DEOBFUSCATE_BYTE() \
	--size; \
	next = data[size] % 11U; \
	data[size] ^= marlinObfuscationTable[previous]; \
	previous = next

void MarlinDeobfuscateData(unsigned char *data, unsigned int size)
{
	unsigned char previous = 0;
	unsigned char next;

	switch ((size - 1) % 11U) {
	case 10:
		do {
			MARLIN_DEOBFUSCATE_BYTE();
	case 9:
			MARLIN_DEOBFUSCATE_BYTE();
	case 8:
			MARLIN_DEOBFUSCATE_BYTE();
	case 7:
			MARLIN_DEOBFUSCATE_BYTE();
	case 6:
			MARLIN_DEOBFUSCATE_BYTE();
	case 5:
			MARLIN_DEOBFUSCATE_BYTE();
	case 4:
			MARLIN_DEOBFUSCATE_BYTE();
	case 3:
			MARLIN_DEOBFUSCATE_BYTE();
	case 2:
			MARLIN_DEOBFUSCATE_BYTE();
	case 1:
			MARLIN_DEOBFUSCATE_BYTE();
	case 0:
			MARLIN_DEOBFUSCATE_BYTE();
		} while (size != 0);
	}
}

#undef MARLIN_DEOBFUSCATE_BYTE

int MarlinBufferObfuscate(MarlinBuffer *buffer)
{
	int result = 0;

	if (buffer == 0)
		result = 3;
	else
		MarlinObfuscateData(buffer->data, buffer->size);
	return result;
}

int MarlinBufferDeobfuscateCopy(const MarlinBuffer *buffer, MarlinBuffer **copy)
{
	int result = 0;
	MarlinBuffer *temporary = 0;

	if (buffer == 0) {
		result = 3;
	} else {
		temporary = MarlinBufferCreate(0, buffer->size, buffer->data);
		if (temporary == 0) {
			result = 2;
		} else {
			MarlinDeobfuscateData(temporary->data, temporary->size);
			*copy = temporary;
			temporary = 0;
		}
	}
	MarlinBufferDestroy(0, temporary);
	return result;
}
