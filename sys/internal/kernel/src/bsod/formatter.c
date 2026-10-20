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
	const uint32_t *arguments = args;
	uint32_t state = count;
	int total = 0;
	char number[32];
	char prefix[2];

	while (*format) {
		const char *text, *end, *digits;
		char *digit;
		uint32_t value;
		int32_t signedValue;
		int flags, width, precision, prefixLength, length, zeros, padding, i;
		int character, padCharacter;

		if (*format != '%') {
			total += callback(context, &state, (signed char)*format);
			goto next;
		}
		precision = 0;
		width = 0;
		flags = 0;
		for (;;) {
			character = (signed char)*++format;
			switch (character) {
			case '-':
				flags |= FORMAT_LEFT;
				continue;
			case '+':
				flags |= FORMAT_PLUS;
				continue;
			case '0':
				flags |= FORMAT_ZERO;
				continue;
			case ' ':
				flags |= FORMAT_SPACE;
				continue;
			case '#':
				flags |= FORMAT_ALTERNATE;
				continue;
			case 'l':
				flags |= FORMAT_LONG;
				continue;
			case 'h':
				flags |= FORMAT_SHORT;
				continue;
			case '.':
				flags |= FORMAT_PRECISION;
				format++;
				if (*format == '*') {
					precision = *arguments++;
					if (precision < 0)
						precision = -1;
					continue;
				}
				precision = 0;
				while ((unsigned char)*format - '0' <= 9u)
					precision = *format++ - '0' + precision * 10;
				if (precision < 0)
					precision = -1;
				if (!*format)
					goto next;
				format--;
				continue;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				width = 0;
				for (;;) {
					width = *format - '0' + width * 10;
					if ((unsigned char)format[1] - '0' > 9u)
						break;
					format++;
				}
				if (!format[1]) {
					format++;
					goto next;
				}
				continue;
			case 'd':
			case 'i':
				if (flags & FORMAT_LONG) {
					signedValue = *arguments;
				} else {
					signedValue = *arguments;
					if (flags & FORMAT_SHORT)
						signedValue = (int16_t)signedValue;
				}
				arguments++;
				if (signedValue < 0) {
					prefixLength = 1;
					prefix[0] = '-';
				} else {
					if (flags & FORMAT_PLUS) {
						prefix[0] = '+';
						prefixLength = 1;
					} else if (flags & FORMAT_SPACE) {
						prefix[0] = ' ';
						prefixLength = 1;
					} else {
						prefixLength = 0;
					}
					if (!signedValue) {
						if ((flags & FORMAT_PRECISION) && !precision) {
							digit = &number[31];
						} else {
							number[31] = '0';
							digit = &number[30];
						}
						goto digits;
					}
				}
				signedValue = __builtin_abs(signedValue);
				digit = &number[31];
				do {
					*digit-- = signedValue % 10 + '0';
					signedValue /= 10;
				} while (signedValue);
			digits:
				text = digit + 1;
				length = &number[31] - digit;
				goto emit;
			case 'u':
				if ((flags & FORMAT_LONG) || !(flags & FORMAT_SHORT))
					value = *arguments;
				else
					value = *(const uint16_t *)arguments;
				arguments++;
				if (value) {
					digit = &number[31];
					do {
						*digit-- = value % 10 + '0';
						value /= 10;
					} while (value);
				} else if ((flags & FORMAT_PRECISION) && !precision) {
					digit = &number[31];
				} else {
					digit = &number[30];
					number[31] = '0';
				}
				text = digit + 1;
				length = &number[31] - digit;
				prefixLength = 0;
				goto emit;
			case 'o':
				if ((flags & FORMAT_LONG) || !(flags & FORMAT_SHORT))
					value = *arguments;
				else
					value = *(const uint16_t *)arguments;
				arguments++;
				if (value) {
					digit = &number[31];
					do {
						*digit-- = (value & 7) + '0';
						value >>= 3;
					} while (value);
					if (flags & FORMAT_ALTERNATE)
						*digit-- = '0';
				} else if ((flags & FORMAT_PRECISION) && !precision) {
					digit = &number[31];
				} else {
					number[31] = '0';
					digit = &number[30];
				}
				text = digit + 1;
				length = &number[31] - digit;
				prefixLength = 0;
				goto emit;
			case 'x':
			case 'X':
				prefixLength = flags & FORMAT_ALTERNATE;
				if (flags & FORMAT_ALTERNATE) {
					prefix[1] = character;
					prefix[0] = '0';
					prefixLength = 2;
				}
				digits = character == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
				if ((flags & FORMAT_LONG) || !(flags & FORMAT_SHORT))
					value = *arguments;
				else
					value = *(const uint16_t *)arguments;
				arguments++;
				if (value) {
					digit = &number[31];
					do {
						*digit-- = digits[value & 15];
						value >>= 4;
					} while (value);
				} else if ((flags & FORMAT_PRECISION) && !precision) {
					prefixLength = 0;
					digit = &number[31];
				} else {
					prefixLength = 0;
					number[31] = '0';
					digit = &number[30];
				}
				text = digit + 1;
				length = &number[31] - digit;
				goto emit;
			case 'p':
				value = *arguments++;
				if (value) {
					digit = &number[31];
					do {
						*digit-- = "0123456789abcdef"[value & 15];
						value >>= 4;
					} while (value);
				} else if ((flags & FORMAT_PRECISION) && !precision) {
					digit = &number[31];
				} else {
					number[31] = '0';
					digit = &number[30];
				}
				prefix[0] = '0';
				prefix[1] = 'x';
				text = digit + 1;
				length = &number[31] - digit;
				prefixLength = 2;
				goto emit;
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
					goto next;
				}
				if (!(flags & FORMAT_PRECISION)) {
					for (length = 0; text[length]; length++)
						;
				} else {
					for (length = 0; length < precision; length++)
						if (!text[length])
							break;
				}
				prefixLength = 0;
				goto emit;
			case 'c':
				character = *arguments++;
				goto single;
			default:
				if (!character)
					goto next;
			single:
				length = 1;
				flags |= FORMAT_CHARACTER;
				prefixLength = 0;
				precision = 1;
				number[31] = character;
				text = &number[31];
				goto emit;
			}
		}

	emit:
		if (precision > length)
			zeros = precision - length;
		else
			zeros = 0;
		end = text + length;
		padding = width - length - prefixLength - zeros;
		if (flags & FORMAT_CHARACTER)
			padCharacter = (flags & FORMAT_ZERO) && padding > 0 ? '0' : ' ';
		else
			padCharacter = (flags & (FORMAT_ZERO | FORMAT_PRECISION)) == FORMAT_ZERO &&
				       padding > 0 ? '0' : ' ';
		if (flags & FORMAT_LEFT) {
			for (i = 0; i < prefixLength; i++)
				total += callback(context, &state, (signed char)prefix[i]);
			for (i = 0; i < zeros; i++)
				total += callback(context, &state, '0');
			while (text < end)
				total += callback(context, &state, (signed char)*text++);
			for (i = 0; i < padding; i++)
				total += callback(context, &state, ' ');
		} else if (flags & FORMAT_ZERO) {
			if (padCharacter == '0') {
				for (i = 0; i < prefixLength; i++)
					total += callback(context, &state, (signed char)prefix[i]);
				for (i = 0; i < padding; i++)
					total += callback(context, &state, '0');
			} else {
				for (i = 0; i < padding; i++)
					total += callback(context, &state, padCharacter);
				for (i = 0; i < prefixLength; i++)
					total += callback(context, &state, (signed char)prefix[i]);
			}
			for (i = 0; i < zeros; i++)
				total += callback(context, &state, '0');
			while (text < end)
				total += callback(context, &state, (signed char)*text++);
		} else {
			for (i = 0; i < padding; i++)
				total += callback(context, &state, padCharacter);
			for (i = 0; i < prefixLength; i++)
				total += callback(context, &state, (signed char)prefix[i]);
			for (i = 0; i < zeros; i++)
				total += callback(context, &state, '0');
			while (text < end)
				total += callback(context, &state, (signed char)*text++);
		}
	next:
		if (*format)
			format++;
	}
	callback(context, &state, 0);
	return total;
}
