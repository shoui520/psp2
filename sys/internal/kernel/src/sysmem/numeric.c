#include <stdint.h>
#include <sysmem/sysclib.h>

/* Name unknown; shared character classification used by integer parsers. */
__attribute__((noinline))
uint32_t NumericDigitValue(int value)
{
	int classification = look_ctype_table((unsigned char)value);

	if (classification & 4)
		return value & 15;
	if (classification & 3)
		return (value & 31) + 9;
	return 255;
}

long strtol(const char *string, char **end, int requestedBase)
{
	const signed char *cursor = (const signed char *)string;
	uint32_t base = requestedBase, result = 0, digit;
	int current, negative = 0;

	do {
		current = *cursor++;
	} while (look_ctype_table(current) & 8);
	cursor--;
	if (current == '-' || current == '+') {
		negative = current == '-';
		current = *++cursor;
	}
	if (base - 2 > 34)
		base = 0;
	if (current == '0') {
		int prefix;

		current = *++cursor;
		prefix = toupper(current);
		if (prefix == 'B') {
			if (!(base & ~2U)) {
				base = 2;
				current = *++cursor;
			}
		} else if (prefix == 'X') {
			if (!(base & ~16U)) {
				base = 16;
				current = *++cursor;
			}
		} else if (!(base & ~8U)) {
			base = 8;
		}
	} else {
		if (!base)
			base = 10;
		if (toupper(current) == 'O') {
			base = 8;
			current = *++cursor;
		}
	}
	while ((digit = NumericDigitValue(current)) < base) {
		result = result * base + digit;
		current = *++cursor;
	}
	if (end)
		*end = (char *)cursor;
	return (int32_t)(negative ? 0U - result : result);
}

unsigned long strtoul(const char *string, char **end, int requestedBase)
{
	const signed char *cursor = (const signed char *)string;
	uint32_t base = requestedBase, result = 0, digit;
	int current;

	do {
		current = *cursor++;
	} while (look_ctype_table(current) & 8);
	cursor--;
	if (base - 2 > 34)
		base = 10;
	if (current == '0') {
		int prefix;

		current = *++cursor;
		prefix = toupper(current);
		if (prefix == 'B' || prefix == 'X') {
			base = prefix == 'B' ? 2 : 16;
			current = *++cursor;
		}
	} else if (toupper(current) == 'O') {
		base = 8;
		current = *++cursor;
	}
	while ((digit = NumericDigitValue(current)) < base) {
		result = result * base + digit;
		current = *++cursor;
	}
	if (end)
		*end = (char *)cursor;
	return result;
}
