/* Reduced integer formatter used by the crash console. */
#include "bsod_internal.h"

enum BsodFormatFlags {
	FORMAT_LEFT = 1,
	FORMAT_PLUS = 2,
	FORMAT_ZERO = 4,
	FORMAT_SPACE = 8,
	FORMAT_ALTERNATE = 16,
	FORMAT_PRECISION = 32,
	FORMAT_LONG = 64,
	FORMAT_SHORT = 128,
	FORMAT_CHARACTER = 256,
};

int bsodFormat(BsodFormatCallback callback, void *context, int count,
	       const char *format, const void *args)
{
	static const char lowerDigits[] = "0123456789abcdef";
	static const char upperDigits[] = "0123456789ABCDEF";
	const uint32_t *arguments = args;
	uint32_t state = count;
	uint32_t total = 0;

	while (*format) {
		char number[32], prefix[2];
		const char *text, *digits;
		char *first = number + sizeof(number);
		uint32_t flags = 0, width = 0, precision = 0, value;
		int length, prefixLength = 0, zeros, padding, padCharacter, i;
		int32_t signedValue;
		int character;

		if (*format != '%') {
			total += callback(context, &state, (signed char)*format++);
			continue;
		}
		format++;
		for (;;) {
			character = (signed char)*format;
			switch (character) {
			case '-': flags |= FORMAT_LEFT; break;
			case '+': flags |= FORMAT_PLUS; break;
			case '0': flags |= FORMAT_ZERO; break;
			case ' ': flags |= FORMAT_SPACE; break;
			case '#': flags |= FORMAT_ALTERNATE; break;
			case 'l': flags |= FORMAT_LONG; break;
			case 'h': flags |= FORMAT_SHORT; break;
			case '.':
				flags |= FORMAT_PRECISION;
				format++;
				if (*format == '*') {
					precision = *arguments++;
					format++;
				} else {
					precision = 0;
					while ((unsigned char)*format - '0' < 10u)
						precision = precision * 10 + *format++ - '0';
				}
				if ((int32_t)precision < 0)
					precision = UINT32_MAX;
				continue;
			default:
				if (character >= '1' && character <= '9') {
					width = 0;
					do {
						width = width * 10 + *format++ - '0';
					} while ((unsigned char)*format - '0' < 10u);
					continue;
				}
				goto conversion;
			}
			format++;
		}
	conversion:
		if (!character)
			break;
		format++;
		text = first;
		length = 0;
		switch (character) {
		case 's':
			text = (const char *)(uintptr_t)*arguments++;
			if (!text) {
				/* These six callback results do not enter Sony's total. */
				callback(context, &state, '(');
				callback(context, &state, 'n');
				callback(context, &state, 'u');
				callback(context, &state, 'l');
				callback(context, &state, 'l');
				callback(context, &state, ')');
				continue;
			}
			if (flags & FORMAT_PRECISION) {
				while (length < (int32_t)precision && text[length])
					length++;
			} else {
				while (text[length])
					length++;
			}
			break;
		case 'd':
		case 'i':
			value = *arguments++;
			if (!(flags & FORMAT_LONG) && (flags & FORMAT_SHORT))
				value = (int16_t)value;
			signedValue = (int32_t)value;
			if (signedValue < 0) {
				prefix[prefixLength++] = '-';
				signedValue = (int32_t)(0u - value);
			} else if (flags & FORMAT_PLUS) {
				prefix[prefixLength++] = '+';
			} else if (flags & FORMAT_SPACE) {
				prefix[prefixLength++] = ' ';
			}
			if (signedValue || !(flags & FORMAT_PRECISION) || precision) {
				/* The original uses signed division even for INT_MIN. */
				do {
					*--first = '0' + signedValue % 10;
					signedValue /= 10;
				} while (signedValue);
			}
			text = first;
			length = number + sizeof(number) - first;
			break;
		case 'u':
		case 'o':
		case 'x':
		case 'X':
		case 'p':
			value = *arguments++;
			if (character != 'p' && !(flags & FORMAT_LONG) &&
			    (flags & FORMAT_SHORT))
				value = (uint16_t)value;
			digits = character == 'X' ? upperDigits : lowerDigits;
			if (value) {
				if (character == 'u') {
					do {
						*--first = '0' + value % 10;
						value /= 10;
					} while (value);
				} else if (character == 'o') {
					do {
						*--first = '0' + (value & 7);
						value >>= 3;
					} while (value);
					if (flags & FORMAT_ALTERNATE)
						*--first = '0';
				} else {
					do {
						*--first = digits[value & 15];
						value >>= 4;
					} while (value);
					if (flags & FORMAT_ALTERNATE) {
						prefix[0] = '0';
						prefix[1] = character;
						prefixLength = 2;
					}
				}
			} else if (!(flags & FORMAT_PRECISION) || precision) {
				*--first = '0';
			}
			if (character == 'p') {
				prefix[0] = '0';
				prefix[1] = 'x';
				prefixLength = 2;
			}
			text = first;
			length = number + sizeof(number) - first;
			break;
		default:
			if (character == 'c')
				character = *arguments++;
			*--first = character;
			text = first;
			length = 1;
			flags |= FORMAT_CHARACTER;
			precision = 1;
			break;
		}

		zeros = (int32_t)precision > length ? precision - length : 0;
		padding = (int32_t)(width - length - prefixLength - zeros);
		padCharacter = ' ';
		if (padding > 0 &&
		    ((flags & FORMAT_CHARACTER) ? (flags & FORMAT_ZERO) :
		     (flags & (FORMAT_ZERO | FORMAT_PRECISION)) == FORMAT_ZERO))
			padCharacter = '0';
		if (!(flags & FORMAT_LEFT) && padCharacter != '0')
			for (i = 0; i < padding; i++)
				total += callback(context, &state, padCharacter);
		for (i = 0; i < prefixLength; i++)
			total += callback(context, &state, (signed char)prefix[i]);
		if (!(flags & FORMAT_LEFT) && padCharacter == '0')
			for (i = 0; i < padding; i++)
				total += callback(context, &state, '0');
		for (i = 0; i < zeros; i++)
			total += callback(context, &state, '0');
		for (i = 0; i < length; i++)
			total += callback(context, &state, (signed char)text[i]);
		if (flags & FORMAT_LEFT)
			for (i = 0; i < padding; i++)
				total += callback(context, &state, ' ');
	}
	callback(context, &state, 0);
	return total;
}
