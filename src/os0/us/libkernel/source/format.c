#include <stdarg.h>
#include <stdint.h>
#include "format.h"

/* the fields of an IEEE 754 double */
#define DOUBLE_EXPONENT_SHIFT		52
#define DOUBLE_EXPONENT_MASK		0x7ff
#define DOUBLE_EXPONENT_BIAS		1023
#define DOUBLE_FRACTION_MASK		UINT64_C(0x000fffffffffffff)
#define DOUBLE_MAGNITUDE_MASK		UINT64_C(0x7fffffffffffffff)

enum FormatFlags {
	FORMAT_LEFT = 1,
	FORMAT_PLUS = 2,
	FORMAT_ZERO = 4,
	FORMAT_SPACE = 8,
	FORMAT_ALTERNATE = 16,
	FORMAT_PRECISION = 32,
	FORMAT_BYTE = 0x100,
	FORMAT_SHORT = 0x200,
	FORMAT_LONG = 0x400,
	FORMAT_QUAD = 0x800,
	FORMAT_LONG_DOUBLE = 0x1000,
};

struct FormatState {
	const struct FormatOps *operations;
	int (*put)(void *, uintptr_t *, int);
	void *context;
	uintptr_t remaining;
	int count;
};

static inline __attribute__((always_inline))
void FormatFloating(struct FormatState *, unsigned int, int, int, int, double);
extern void *sceClibMemcpy(void *, const void *, unsigned int);
extern void *sceClibMemset(void *, int, unsigned int);

static inline __attribute__((always_inline))
void FormatEmit(struct FormatState *state, int character)
{
	state->count += state->put(state->context, &state->remaining,
			character);
}

static inline __attribute__((always_inline))
void FormatRepeat(struct FormatState *state, int character, int count)
{
	for (int index = 0; index < count; index++)
		FormatEmit(state, character);
}

static inline __attribute__((always_inline))
uint64_t FormatIntegerArgument(va_list *arguments, unsigned int flags, int sign)
{
	if (flags & FORMAT_BYTE) {
		unsigned int value = va_arg(*arguments, unsigned int);

		return sign ? (uint64_t)(int64_t)(signed char)value : (unsigned char)value;
	}
	if (flags & FORMAT_SHORT) {
		unsigned int value = va_arg(*arguments, unsigned int);

		return sign ? (uint64_t)(int64_t)(short)value : (unsigned short)value;
	}
	if ((flags & FORMAT_LONG) || !(flags & FORMAT_QUAD)) {
		unsigned int value = va_arg(*arguments, unsigned int);

		return sign ? (uint64_t)(int64_t)(int)value : value;
	}
	return va_arg(*arguments, uint64_t);
}

static inline __attribute__((always_inline))
void FormatCharacter(struct FormatState *state, unsigned int flags,
		int width, int character)
{
	if (!(flags & FORMAT_LEFT)) {
		FormatRepeat(state, flags & FORMAT_ZERO ? '0' : ' ', width - 1);
		FormatEmit(state, character);
	} else {
		FormatEmit(state, character);
		FormatRepeat(state, ' ', width - 1);
	}
}

static inline __attribute__((always_inline))
void FormatString(struct FormatState *state, unsigned int flags, int width,
		int precision, const void *string)
{
	int length = 0;
	int wide = flags & FORMAT_LONG;
	int padding;

	if (!string) {
		/* Native null strings bypass width and precision and do not contribute
		 * callback returns to the character count used by %n/the result. */
		const char *nullString = "(null)";

		for (int index = 0; index < 6; index++)
			state->put(state->context, &state->remaining,
					nullString[index]);
		return;
	}
	if (!wide) {
		length = state->operations->stringLength(string);
		if ((flags & FORMAT_PRECISION) && precision <= length)
			length = precision;
	} else {
		const int16_t *cursor = string;

		if (!(flags & FORMAT_PRECISION)) {
			while (*cursor++)
				length++;
		} else if (precision > 0) {
			while (length < precision && *cursor++)
				length++;
		}
	}
	padding = width - length;
	if (!(flags & FORMAT_LEFT))
		FormatRepeat(state, (flags & (FORMAT_ZERO | FORMAT_PRECISION)) == FORMAT_ZERO ?
				'0' : ' ', padding);
	if (wide) {
		const uint16_t *cursor = string;

		for (int index = 0; index < length; index++)
			FormatEmit(state, cursor[index]);
	} else {
		const unsigned char *cursor = string;

		for (int index = 0; index < length; index++)
			FormatEmit(state, state->operations->stringNext(&cursor));
	}
	if (flags & FORMAT_LEFT)
		FormatRepeat(state, ' ', padding);
}

static inline __attribute__((always_inline))
void FormatNumber(struct FormatState *state, unsigned int flags, int width,
		int precision, const char *digits, int length, int sign,
		const char *prefix, int prefixLength)
{
	int zeroes = precision > length ? precision - length : 0;
	int padding = width - !!sign - length - prefixLength - zeroes;
	int padCharacter = (flags & (FORMAT_ZERO | FORMAT_PRECISION)) == FORMAT_ZERO &&
			padding > 0 ? '0' : ' ';

	if (!(flags & FORMAT_LEFT) && !(flags & FORMAT_ZERO))
		FormatRepeat(state, padCharacter, padding);
	if (sign)
		FormatEmit(state, sign);
	if (!(flags & FORMAT_LEFT) && (flags & FORMAT_ZERO) && padCharacter != '0')
		FormatRepeat(state, padCharacter, padding);
	for (int index = 0; index < prefixLength; index++)
		FormatEmit(state, prefix[index]);
	if (!(flags & FORMAT_LEFT) && (flags & FORMAT_ZERO) && padCharacter == '0')
		FormatRepeat(state, '0', padding);
	FormatRepeat(state, '0', zeroes);
	for (int index = 0; index < length; index++)
		FormatEmit(state, (signed char)digits[index]);
	if (flags & FORMAT_LEFT)
		FormatRepeat(state, ' ', padding);
}

static inline __attribute__((always_inline))
void FormatFloating(struct FormatState *state, unsigned int flags, int width,
		int precision, int conversion, double argument)
{
	union {
		double value;
		uint64_t bits;
	} number = { .value = argument };
	char output[32];
	int sign = number.bits >> 63 ? '-' : flags & FORMAT_PLUS ? '+' :
			flags & FORMAT_SPACE ? ' ' : 0;
	unsigned int binaryExponent = (number.bits >> DOUBLE_EXPONENT_SHIFT) & DOUBLE_EXPONENT_MASK;
	int lower = conversion == 'a' || conversion == 'e' || conversion == 'f' ||
			conversion == 'g';
	char *begin = output + !!sign;
	int firstLength = 0, middleZeroes = 0, lastLength = 0, lastZeroes = 0;
	int leadingZeroes = 0;
	int special = binaryExponent == DOUBLE_EXPONENT_MASK;
	int hexadecimal = conversion == 'a' || conversion == 'A';

	if (sign)
		output[0] = sign;
	if (special) {
		const char *text = number.bits & DOUBLE_FRACTION_MASK ?
				(lower ? "nan" : "NAN") : (lower ? "inf" : "INF");

		begin[0] = text[0];
		begin[1] = text[1];
		begin[2] = text[2];
		firstLength = 3;
		if (hexadecimal) {
			FormatNumber(state, flags, width, 0, begin, 3, sign, "", 0);
			return;
		}
	} else if (hexadecimal) {
		const char *alphabet = lower ? "0123456789abcdef" : "0123456789ABCDEF";
		uint64_t fraction = number.bits << 12;
		int exponent = binaryExponent ? (int)binaryExponent - DOUBLE_EXPONENT_BIAS :
				(number.bits & DOUBLE_MAGNITUDE_MASK ? 1 - DOUBLE_EXPONENT_BIAS : 0);
		char *cursor = output;

		*cursor++ = binaryExponent ? '1' : '0';
		if (!(flags & FORMAT_PRECISION) && precision == 0) {
			if (fraction) {
				*cursor++ = '.';
				do {
					*cursor++ = alphabet[fraction >> 60];
					fraction <<= 4;
				} while (fraction);
			}
		} else {
			*cursor++ = '.';
			/* Native explicit zero precision enters this loop too. Its
			 * cursor never reaches the preceding target before wraparound. */
			do {
				*cursor++ = alphabet[fraction >> 60];
				fraction <<= 4;
			} while (cursor != output + precision + 2);
		}
		*cursor++ = lower ? 'p' : 'P';
		*cursor++ = exponent < 0 ? '-' : '+';
		if (exponent < 0)
			exponent = -exponent;
		if (exponent >= 1000)
			*cursor++ = '1';
		if (exponent >= 100)
			*cursor++ = '0' + exponent / 100 % 10;
		if (exponent >= 10)
			*cursor++ = '0' + exponent / 10 % 10;
		*cursor++ = '0' + exponent % 10;
		FormatNumber(state, flags, width, 0, output, cursor - output, sign,
				lower ? "0x" : "0X", 2);
		return;
	} else {
		static const double powers[] = {
			1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256,
		};
		char storage[44];
		char *digits = storage + 1;
		int digitLength = 1;
		int exponent = 0;
		int kind = conversion & ~0x20;

		if (!(flags & FORMAT_PRECISION))
			precision = -1;
		if (precision < 0)
			precision = 6;
		else if (precision == 0)
			precision = kind == 'G';
		if ((number.bits & DOUBLE_MAGNITUDE_MASK) == 0) {
			digits = "0";
		} else {
			int estimate = binaryExponent ?
					((int)binaryExponent - DOUBLE_EXPONENT_BIAS) * 30103 / 100000 : -307;
			int scale = estimate - 4;
			double value = argument < 0.0 ? -argument : argument;

			if (scale < 0) {
				int magnitude = (7 - estimate) & ~3;

				scale = -magnitude;
				for (int index = 0; magnitude; index++, magnitude >>= 1) {
					if (magnitude & 1)
						value *= powers[index];
				}
			} else if (scale > 0) {
				double divisor = 1.0;
				int magnitude = scale &= ~3;

				for (int index = 0; magnitude; index++, magnitude >>= 1) {
					if (magnitude & 1)
						divisor *= powers[index];
				}
				value /= divisor;
			}
			exponent = scale + 7;
			int wanted = precision + (kind == 'F' ? scale + 11 : 7);

			if (wanted > 18)
				wanted = 19;
			storage[0] = '0';
			char *end = digits;

			while (wanted > 0 && value > 0.0) {
				int group = (int)value;

				if (wanted > 8)
					value = (value - (double)group) * 100000000.0;
				char *position = end + 8;

				if (group <= 0) {
					sceClibMemset(end, '0', 8);
				} else {
					do {
						*--position = '0' + group % 10;
						group /= 10;
					} while (group && position != end);
					if (position != end)
						sceClibMemset(end, '0', position - end);
				}
				end += 8;
				wanted -= 8;
			}
			while (*digits == '0') {
				digits++;
				exponent--;
			}
			int available = end - digits;

			digitLength = kind == 'F' ? precision + exponent + 1 :
					kind == 'E' ? precision + 1 : precision;
			if (digitLength > available)
				digitLength = available;
			if (digitLength < 0) {
				digits = "0";
				digitLength = 1;
			} else {
				int roundUp = digitLength < available && digits[digitLength] >= '5';
				int index = digitLength - 1;

				while (digits[index] == (roundUp ? '9' : '0')) {
					digitLength = index;
					index--;
				}
				if (roundUp)
					digits[index]++;
				if (index < 0) {
					digits--;
					exponent++;
					digitLength++;
				}
				if (digitLength < 1) {
					digits = "0";
					digitLength = 1;
				}
			}
		}
		if (kind == 'G') {
			if (exponent > -5 && exponent < precision) {
				if (!(flags & FORMAT_ALTERNATE) && digitLength <= precision)
					precision = digitLength;
				precision -= exponent + 1;
				if (precision < 0)
					precision = 0;
				kind = 'F';
			} else {
				if (!(flags & FORMAT_ALTERNATE) && digitLength < precision)
					precision = digitLength;
				if (--precision < 0)
					precision = 0;
				conversion = conversion == 'g' ? 'e' : 'E';
			}
		}
		if (kind == 'F') {
			int point = exponent + 1;

			if (point < 1) {
				begin[0] = '0';
				firstLength = 1;
				if (precision || (flags & FORMAT_ALTERNATE))
					begin[firstLength++] = '.';
				middleZeroes = -point < precision ? -point : precision;
				lastLength = precision - middleZeroes;
				if (lastLength > digitLength)
					lastLength = digitLength;
				lastZeroes = precision - middleZeroes - lastLength;
				sceClibMemcpy(begin + firstLength, digits, lastLength);
			} else if (digitLength < point) {
				firstLength = digitLength;
				middleZeroes = point - digitLength;
				sceClibMemcpy(begin, digits, firstLength);
				if (precision || (flags & FORMAT_ALTERNATE)) {
					begin[firstLength] = '.';
					lastLength = 1;
				}
				lastZeroes = precision;
			} else {
				sceClibMemcpy(begin, digits, point);
				firstLength = point;
				if (precision || (flags & FORMAT_ALTERNATE))
					begin[firstLength++] = '.';
				int fraction = digitLength - point;

				if (fraction > precision)
					fraction = precision;
				middleZeroes = precision - fraction;
				sceClibMemcpy(begin + firstLength, digits + point, fraction);
				firstLength += fraction;
			}
		} else {
			begin[0] = digits[0];
			firstLength = 1;
			if (precision > 0) {
				int fraction = digitLength - 1;

				if (fraction > precision)
					fraction = precision;
				begin[firstLength++] = '.';
				sceClibMemcpy(begin + firstLength, digits + 1, fraction);
				firstLength += fraction;
				middleZeroes = precision - fraction;
			} else if (flags & FORMAT_ALTERNATE) {
				begin[firstLength++] = '.';
			}
			char *suffix = begin + firstLength;

			*suffix++ = conversion;
			*suffix++ = exponent < 0 ? '-' : '+';
			if (exponent < 0)
				exponent = -exponent;
			char reversed[12];
			int count = 0;

			do {
				reversed[count++] = '0' + exponent % 10;
				exponent /= 10;
			} while (exponent);
			if (count < 2)
				*suffix++ = '0';
			while (count)
				*suffix++ = reversed[--count];
			lastLength = suffix - begin - firstLength;
		}
	}
	int total = !!sign + firstLength + middleZeroes + lastLength + lastZeroes;

	if (!special && (flags & (FORMAT_ZERO | FORMAT_LEFT)) == FORMAT_ZERO && width > total)
		leadingZeroes = width - total;
	int padding = width - total - leadingZeroes;

	if (!(flags & FORMAT_LEFT))
		FormatRepeat(state, ' ', padding);
	if (sign)
		FormatEmit(state, sign);
	FormatRepeat(state, '0', leadingZeroes);
	for (int index = 0; index < firstLength; index++)
		FormatEmit(state, (signed char)begin[index]);
	FormatRepeat(state, '0', middleZeroes);
	for (int index = 0; index < lastLength; index++)
		FormatEmit(state, (signed char)begin[firstLength + index]);
	FormatRepeat(state, '0', lastZeroes);
	if (flags & FORMAT_LEFT)
		FormatRepeat(state, ' ', padding);
}

/* Native operation-table formatting engine; original internal name unknown. */
int FormatEngine(const struct FormatOps *operations, void *context,
		uintptr_t remaining, const char *format, va_list arguments)
{
	struct FormatState state = {
		.operations = operations,
		.put = operations->put,
		.context = context,
		.remaining = remaining,
		.count = 0,
	};
	int (*get)(const char **, int) = operations->get;
	va_list cursor;
	int character;

	va_copy(cursor, arguments);
	if (operations->initialize)
		operations->initialize(context, remaining);
	while ((character = get(&format, 0)) != 0) {
		unsigned int flags = 0;
		int width = 0;
		int precision = 0;
		char number[44];
		char *digits = number + sizeof(number);
		int length = 0;
		int sign = 0;
		const char *alphabet = "0123456789abcdef";
		const char *prefix = "0x";
		int prefixLength = 0;
		uint64_t value;

		if (character != '%') {
			FormatEmit(&state, character);
			continue;
		}
		for (;;) {
			character = get(&format, 0);
			if (!character)
				goto finish;
			switch (character) {
			case ' ': flags |= FORMAT_SPACE; continue;
			case '#': flags |= FORMAT_ALTERNATE; continue;
			case '+': flags |= FORMAT_PLUS; continue;
			case '-': flags |= FORMAT_LEFT; continue;
			case '0': flags |= FORMAT_ZERO; continue;
			case '*':
				width = va_arg(cursor, int);
				if (width < 0) {
					width = -width;
					flags |= FORMAT_LEFT;
				}
				continue;
			case '.':
				flags |= FORMAT_PRECISION;
				precision = 0;
				if (get(&format, 1) == '*') {
					get(&format, 0);
					precision = va_arg(cursor, int);
				} else {
					while (get(&format, 1) > '0' - 1 && get(&format, 1) < '9' + 1)
						precision = precision * 10 + get(&format, 0) - '0';
				}
				if (precision < 0)
					precision = -1;
				continue;
			case 'h':
				if (!(flags & FORMAT_BYTE)) {
					if (flags & FORMAT_SHORT)
						flags = (flags & ~FORMAT_SHORT) | FORMAT_BYTE;
					else
						flags |= FORMAT_SHORT;
				}
				continue;
			case 'l':
				if (!(flags & FORMAT_QUAD)) {
					if (flags & FORMAT_LONG)
						flags = (flags & ~FORMAT_LONG) | FORMAT_QUAD;
					else
						flags |= FORMAT_LONG;
				}
				continue;
			case 'j': flags |= FORMAT_QUAD; continue;
			case 't': case 'z': flags |= FORMAT_LONG; continue;
			case 'L': flags |= FORMAT_LONG_DOUBLE; continue;
			default:
				if (character >= '1' && character <= '9') {
					width = character - '0';
					while (get(&format, 1) > '0' - 1 && get(&format, 1) < '9' + 1)
						width = width * 10 + get(&format, 0) - '0';
					continue;
				}
				break;
			}
			break;
		}
		if (character == 's') {
			FormatString(&state, flags, width, precision, va_arg(cursor, const void *));
			continue;
		}
		if (character == 'c') {
			character = va_arg(cursor, int);
			if (operations->kind != 0)
				character = flags & FORMAT_LONG ? (unsigned short)character :
						(unsigned char)character;
			FormatCharacter(&state, flags, width, character);
			continue;
		}
		if (character == 'a' || character == 'A' || character == 'e' ||
		    character == 'E' || character == 'f' || character == 'F' || character == 'g') {
			FormatFloating(&state, flags, width, precision, character,
					va_arg(cursor, double));
			continue;
		}
		if (character == 'n') {
			void *destination = va_arg(cursor, void *);

			if (flags & FORMAT_BYTE)
				*(char *)destination = state.count;
			else if (flags & FORMAT_SHORT)
				*(short *)destination = state.count;
			else if (flags & FORMAT_LONG)
				*(int *)destination = state.count;
			else if (flags & FORMAT_QUAD)
				*(uint64_t *)destination = (unsigned int)state.count;
			else
				*(int *)destination = state.count;
		} else if (character == 'd' || character == 'i') {
			value = FormatIntegerArgument(&cursor, flags, 1);
			sign = (int64_t)value < 0 ? '-' : flags & FORMAT_PLUS ? '+' :
					flags & FORMAT_SPACE ? ' ' : 0;
			if (value) {
				uint64_t magnitude = (int64_t)value < 0 ? -value : value;

				do {
					*--digits = '0' + (int64_t)magnitude % 10;
					magnitude = (int64_t)magnitude / 10;
				} while (magnitude);
			}
			goto zeroNumber;
		} else if (character == 'u' || character == 'o' || character == 'x' ||
			   character == 'X' || character == 'p') {
			value = character == 'p' ? va_arg(cursor, unsigned int) :
					FormatIntegerArgument(&cursor, flags, 0);
			if (character == 'X') {
				alphabet = "0123456789ABCDEF";
				prefix = "0X";
			}
			if (character == 'p' || ((character == 'x' || character == 'X') &&
					(flags & FORMAT_ALTERNATE) && value))
				prefixLength = 2;
			if (value) {
				do {
					if (character == 'u') {
						*--digits = '0' + value % 10;
						value /= 10;
					} else if (character == 'o') {
						*--digits = '0' + (value & 7);
						value >>= 3;
					} else {
						*--digits = alphabet[value & 15];
						value >>= 4;
					}
				} while (value);
				if (character == 'o' && (flags & FORMAT_ALTERNATE))
					*--digits = '0';
			}
zeroNumber:
			if (digits == number + sizeof(number) &&
			    !((flags & FORMAT_PRECISION) && precision == 0))
				*--digits = '0';
			length = number + sizeof(number) - digits;
		} else {
			if ((unsigned int)character < 0x80)
				FormatCharacter(&state, flags, width, character);
			continue;
		}
		FormatNumber(&state, flags, width, precision, digits, length, sign,
				prefix, prefixLength);
	}
finish:
	if (operations->kind < 2)
		state.put(context, &state.remaining, 0);
	if (operations->finish)
		operations->finish(context);
	va_end(cursor);
	return state.count;
}
