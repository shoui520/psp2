/* Sysclib ASCII classifications, identical in all four firmware originals. */
#include <stdint.h>
#include <sysmem/sysclib.h>

enum AsciiClass {
	ASCII_UPPER = 0x01,
	ASCII_LOWER = 0x02,
	ASCII_DIGIT = 0x04,
	ASCII_WHITESPACE = 0x08,
	ASCII_OTHER_PRINTABLE = 0x10,
	ASCII_CONTROL = 0x20,
	ASCII_HEX_LETTER = 0x40,
};

static const uint8_t asciiClass[128] = {
	[0 ... 8] = ASCII_CONTROL,
	['\t' ... '\r'] = ASCII_WHITESPACE,
	[14 ... 31] = ASCII_CONTROL,
	[' '] = ASCII_WHITESPACE | ASCII_OTHER_PRINTABLE,
	['!' ... '/'] = ASCII_OTHER_PRINTABLE,
	['0' ... '9'] = ASCII_DIGIT,
	[':' ... '@'] = ASCII_OTHER_PRINTABLE,
	['A' ... 'F'] = ASCII_UPPER | ASCII_HEX_LETTER,
	['G' ... 'Z'] = ASCII_UPPER,
	['[' ... '`'] = ASCII_OTHER_PRINTABLE,
	['a' ... 'f'] = ASCII_LOWER | ASCII_HEX_LETTER,
	['g' ... 'z'] = ASCII_LOWER,
	['{' ... '~'] = ASCII_OTHER_PRINTABLE,
	[127] = ASCII_CONTROL,
};

int look_ctype_table(int value)
{
	return (uint32_t)value < 128 ? asciiClass[value] : 0;
}

#if !SYSMEM_NSKBL	/* the boot loader keeps only look_ctype_table */
int tolower(int value)
{
	if ((uint32_t)value < 128 && (asciiClass[value] & ASCII_UPPER))
		value += 'a' - 'A';
	return value;
}

int toupper(int value)
{
	if ((uint32_t)value < 128 && (asciiClass[value] & ASCII_LOWER))
		value -= 'a' - 'A';
	return value;
}
#endif
