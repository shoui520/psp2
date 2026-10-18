#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sysmem/sysclib.h>

extern const uint8_t gBtRfcommFcsTable[256];


void BtWriteLe16(uint8_t *destination, unsigned int value)
{
	destination[0] = value;
	destination[1] = value >> 8;
}

void BtWriteLe24(uint8_t *destination, unsigned int value)
{
	destination[0] = value;
	destination[1] = value >> 8;
	destination[2] = value >> 16;
}

int BtReadBe16(const uint8_t *source)
{
	return source[1] | source[0] << 8;
}

uint32_t BtReadBe32(const uint8_t *source)
{
	return (uint32_t)source[1] << 16 | (uint32_t)source[0] << 24 |
	       source[3] | source[2] << 8;
}

int BtReadLe16(const uint8_t *source)
{
	return source[0] | source[1] << 8;
}

int BtReadLe24(const uint8_t *source)
{
	return source[2] << 16 | source[1] << 8 | source[0];
}

int BtUnpack(const uint8_t *source, int remaining, const char *format, ...)
{
	va_list arguments;
	int token;

	va_start(arguments, format);
	while ((token = (unsigned char)*format++) != 0) {
		if (token == '-') {
			if (remaining >= 0)
				source++;
			remaining--;
		} else if (token == 's') {
			uint8_t *destination = va_arg(arguments, uint8_t *);
			int character;

			do {
				if (remaining-- < 0)
					break;
				character = *source++;
				*destination++ = character;
			} while (character != 0);
			*destination = 0;
		} else if ((unsigned int)(token - '1') <= 7) {
			uint64_t value = 0;
			int width = token - '0';
			int index;
			void *destination;

			remaining -= width;
			if (remaining >= 0) {
				for (index = width - 1; index >= 0; index--)
					value |= (uint64_t)source[index] << (8 * index);
				source += width;
			}
			destination = va_arg(arguments, void *);
			if (width <= 4)
				*(uint32_t *)destination = value;
			else
				*(uint64_t *)destination = value;
		} else {
			switch (token) {
			case '#': {
				uint8_t *destination = va_arg(arguments, uint8_t *);
				int length = va_arg(arguments, int);
				int index;

				remaining -= length;
				if (remaining >= 0 && length > 0) {
					for (index = 0; index < length; index++)
						destination[index] = source[index];
					source += length;
				}
				break;
			}
			case 'H':
				remaining -= 2;
				if (remaining >= 0) {
					uint32_t *destination = va_arg(arguments, uint32_t *);

					*destination = source[1] | source[0] << 8;
					source += 2;
				}
				break;
			case 'W':
				remaining -= 4;
				if (remaining >= 0) {
					uint32_t *destination = va_arg(arguments, uint32_t *);

					*destination = source[3] | (uint32_t)source[1] << 16 |
						       (uint32_t)source[0] << 24 | source[2] << 8;
					source += 4;
				}
				break;
			case 'E':
				remaining -= 6;
				if (remaining >= 0) {
					uint64_t *destination = va_arg(arguments, uint64_t *);
					uint32_t high = source[1] | source[0] << 8;
					uint32_t low = source[4] << 8 | (uint32_t)source[2] << 24 |
						       source[5] | (uint32_t)source[3] << 16;

					*destination = (uint64_t)high << 32 | low;
					source += 6;
				}
				break;
			default:
				break;
			}
		}
	}
	va_end(arguments);
	return remaining;
}

int BtVPack(uint8_t *destination, int remaining, const char *format,
	    va_list arguments)
{
	uint8_t *cursor = destination;
	uint8_t *checksumStart = NULL;
	uint8_t *checksumEnd = NULL;
	int token;
	int size = 0;

	if (destination == NULL) {
		while ((token = (unsigned char)*format++) != 0) {
			switch (token) {
			case '#':
				(void)va_arg(arguments, const void *);
				size += va_arg(arguments, int);
				break;
			case '%':
			case '-':
				size++;
				break;
			case '1': case '2': case '3': case '4':
				size += token - '0';
				(void)va_arg(arguments, int);
				break;
			case '5': case '6': case '7': case '8':
				size += token - '0';
				(void)va_arg(arguments, uint64_t);
				break;
			case 'C':
				(void)va_arg(arguments, uint32_t);
				(void)va_arg(arguments, uint32_t);
				break;
			case 'D':
				size += 8;
				(void)va_arg(arguments, uint64_t);
				break;
			case 'E':
				size += 6;
				(void)va_arg(arguments, uint64_t);
				break;
			case 'H':
				size += 2;
				(void)va_arg(arguments, int);
				break;
			case 'L':
				size += (va_arg(arguments, unsigned int) & 0xff80) ? 2 : 1;
				break;
			case 'N':
				size += 248;
				(void)va_arg(arguments, const char *);
				break;
			case 'P':
				size += 17;
				(void)va_arg(arguments, int);
				(void)va_arg(arguments, const void *);
				break;
			case 'W':
				size += 4;
				(void)va_arg(arguments, uint32_t);
				break;
			default:
				break;
			}
		}
		return size;
	}
	while ((token = (unsigned char)*format++) != 0) {
		switch (token) {
		case '#': {
			const void *source = va_arg(arguments, const void *);
			int length = va_arg(arguments, int);

			remaining -= length;
			if (remaining >= 0) {
				memcpy(cursor, source, length);
				cursor += length;
			}
			break;
		}
		case '%':
			if (checksumEnd == NULL)
				checksumEnd = cursor;
			if (checksumStart != NULL) {
				uint8_t checksum = 0xff;
				const uint8_t *source = checksumStart;

				while ((uintptr_t)source < (uintptr_t)checksumEnd)
					checksum = gBtRfcommFcsTable[checksum ^ *source++];
				checksumStart = checksumEnd;
				if (--remaining >= 0)
					*cursor++ = ~checksum;
			}
			break;
		case '-':
			*cursor++ = 0;
			break;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': {
			int width = token - '0';

			remaining -= width;
			if (remaining >= 0) {
				uint64_t value;
				int index;

				if (width <= 4)
					value = va_arg(arguments, int);
				else
					value = va_arg(arguments, uint64_t);
				for (index = 0; index < width; index++) {
					*cursor++ = value;
					value >>= 8;
				}
			}
			break;
		}
		case '<':
			checksumStart = cursor;
			break;
		case '>':
			checksumEnd = cursor;
			break;
		case 'C':
			(void)va_arg(arguments, uint32_t);
			(void)va_arg(arguments, uint32_t);
			break;
		case 'D':
		case 'E': {
			int width = token == 'D' ? 8 : 6;

			remaining -= width;
			if (remaining >= 0) {
				uint64_t value = va_arg(arguments, uint64_t);
				int index;

				for (index = width - 1; index >= 0; index--)
					*cursor++ = value >> (8 * index);
			}
			break;
		}
		case 'H':
			remaining -= 2;
			if (remaining >= 0) {
				unsigned int value = va_arg(arguments, unsigned int);

				*cursor++ = value >> 8;
				*cursor++ = value;
			}
			break;
		case 'L': {
			unsigned int value = va_arg(arguments, unsigned int);

			if ((value & 0xff80) != 0) {
				*cursor++ = value * 2;
				*cursor++ = value >> 7;
			} else {
				*cursor++ = value * 2 | 1;
			}
			break;
		}
		case 'N': {
			const char *source = va_arg(arguments, const char *);
			int length = strnlen(source, 248);

			memcpy(cursor, source, length);
			if (length <= 247)
				memset(cursor + length, 0, 248 - length);
			cursor += 248;
			break;
		}
		case 'P': {
			int length = va_arg(arguments, int);
			const uint8_t *source = va_arg(arguments, const uint8_t *);

			if (length > 16) {
				const uint32_t *words = (const uint32_t *)source;
				uint32_t *output = (uint32_t *)(cursor + 1);
				uint32_t first, second, third, fourth;

				*cursor = 16;
				first = words[0];
				second = words[1];
				third = words[2];
				fourth = words[3];
				output[0] = first;
				output[1] = second;
				output[2] = third;
				output[3] = fourth;
			} else {
				if (length <= 0) {
					length = 1;
					*cursor = 1;
					cursor[1] = *source;
				} else {
					*cursor = length;
					memcpy(cursor + 1, source, length);
				}
				if (length != 16)
					memset(cursor + 1 + length, 0, 16 - length);
			}
			cursor += 17;
			break;
		}
		case 'W':
			remaining -= 4;
			if (remaining >= 0) {
				uint32_t value = va_arg(arguments, uint32_t);

				*cursor++ = value >> 24;
				*cursor++ = value >> 16;
				*cursor++ = value >> 8;
				*cursor++ = value;
			}
			break;
		default:
			break;
		}
	}
	return cursor - destination;
}

int BtPack(uint8_t *destination, int remaining, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = BtVPack(destination, remaining, format, arguments);
	va_end(arguments);
	return result;
}
