#include <stdint.h>

#include "bt_internal.h"

int BtUpdateUtf8String(char *destination, const uint8_t *source, int remaining)
{
	int changes = 0;

	while (remaining > 0 && *source != 0) {
		int character = *source;
		int width = 1;
		char *target, *end;
		const uint8_t *input;

		if ((character & 0x80) != 0) {
			unsigned int mask = 0x80;

			width = 0;
			do {
				mask >>= 1;
				width++;
			} while ((character & mask) != 0);
			if (width == 1 || width > 4) {
				source++;
				continue;
			}
		}
		remaining -= width;
		if (remaining < 0)
			break;
		target = destination;
		end = target + width;
		input = source;
		do {
			/* Preserve the original signed comparison of existing bytes. */
			int oldByte = (signed char)*target++;
			int newByte = *input++;

			if (oldByte != newByte) {
				changes++;
				target[-1] = newByte;
			}
		} while (target != end);
		destination = target;
		source += width;
	}
	if (*destination != 0)
		changes++;
	*destination = 0;
	return changes;
}

int BtUtf8ToUtf16Be(uint8_t *destination, const uint8_t *source, int capacity)
{
	uint8_t *cursor = destination;
	uintptr_t limit = (uintptr_t)destination + (unsigned int)capacity;
	unsigned int character = *source;

	while (character != 0 && limit > (uintptr_t)cursor + 3) {
		if ((character & 0x80) != 0) {
			unsigned int second = source[1];

			if ((character & 0xe0) == 0xc0 && (second & 0xc0) == 0x80) {
				character = (second & 0x3f) | (character & 0x1f) << 6;
				source += 2;
			} else {
				unsigned int third;

				if ((character & 0xf0) != 0xe0 || (second & 0xc0) != 0x80 ||
				    ((third = source[2]) & 0xc0) != 0x80) {
					source++;
					character = second;
					continue;
				}
				character = (uint16_t)((third & 0x3f) | character << 12 |
						       (second & 0x3f) << 6);
				source += 3;
			}
		} else {
			source++;
		}
		cursor[1] = character;
		cursor[0] = character >> 8;
		cursor += 2;
		character = *source;
	}
	cursor[0] = 0;
	cursor[1] = 0;
	return cursor + 2 - destination;
}

uint8_t *BtUtf16BeToUtf8(uint8_t *destination, const uint8_t *source, int capacity)
{
	/* Each 16-bit code unit is encoded independently by the original. */
	uintptr_t limit = (uintptr_t)destination + (unsigned int)capacity;
	unsigned int character = (uint16_t)(source[1] | source[0] << 8);

	while (character != 0 && limit > (uintptr_t)destination + 3) {
		if ((character & 0xff80) != 0) {
			unsigned int last = (character & 0x3f) | 0x80;

			if ((character & 0xf800) != 0) {
				destination[0] = character >> 12 | 0xe0;
				destination[1] = (character >> 6 & 0x3f) | 0x80;
				destination[2] = last;
				destination += 3;
			} else {
				destination[0] = character >> 6 | 0xc0;
				destination[1] = last;
				destination += 2;
			}
		} else {
			*destination++ = character;
		}
		character = (uint16_t)(source[3] | source[2] << 8);
		source += 2;
	}
	*destination = 0;
	return destination;
}

const char *BtSkipSpaces(const char *cursor)
{
	int character = (signed char)*cursor;

	while (character == ' ' || character == '\t') {
		cursor++;
		character = (signed char)*cursor;
	}
	return cursor;
}

const char *BtParseDecimalList(const char *cursor, int32_t *values, int count)
{
	int index;

	for (index = 0; index < count; index++) {
		uint32_t value = 0;
		unsigned int byte = (unsigned char)*cursor;
		int character = (signed char)byte;

		while (character == ' ' || character == '\t') {
			byte = (unsigned char)*++cursor;
			character = (signed char)byte;
		}
		if (byte - '0' > 9)
			return "";
		do {
			byte = (unsigned char)*++cursor;
			value = character - '0' + value * 10;
			character = (signed char)byte;
		} while (byte - '0' <= 9);
		if (index < count - 1) {
			while (character == ' ' || character == '\t')
				character = (signed char)*++cursor;
			cursor++;
			if (character != ',')
				return "";
		}
		values[index] = (int32_t)value;
	}
	return cursor;
}
