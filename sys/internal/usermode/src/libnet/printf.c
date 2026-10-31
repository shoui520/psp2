/*
 * printf.c: libnet's own formatter for its debug messages (SceNetInternal_694F8996). It hands
 * each character to OUTPUT and returns how many it wrote. Conversions: c s d i o u x X p, the
 * long forms D O U, the flags space # + - 0, a width and a precision (or *), and the h l ll L
 * j sizes; any other character after % is written as it is.
 */
#include <stdarg.h>
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* the conversion flags */
#define LIBNET_PRINTF_LONG	0x01
#define LIBNET_PRINTF_LONGLONG	0x02
#define LIBNET_PRINTF_SHORT	0x04
#define LIBNET_PRINTF_ALTERNATE	0x08
#define LIBNET_PRINTF_LEFT	0x10
#define LIBNET_PRINTF_ZERO	0x20
#define LIBNET_PRINTF_PREFIX	0x40	/* "0x" before the digits */

#define LIBNET_PRINTF_ISDIGIT(c)	((unsigned int)(SceUChar8)(c) - '0' <= 9)

int libnet_vprintf(LibnetOutput output, const char *format, va_list ap)
{
	char null[7] = "(null)";
	char number[22];
	char *end = &number[sizeof(number) - 1];
	const char *digits, *conversion, *s;
	unsigned int value, base, flags;
	SceUInt64 value64;
	int count, width, precision, sign, size, field, length, i;
	signed char c;
	char character;

	if (format == NULL || output == NULL)
		return 0;
	c = *format;
	if (c == '\0')
		return 0;
	value = 0;
	count = 0;
	value64 = 0;
	digits = "0123456789abcdef";
	do {
		if (c != '%') {
			conversion = format;
			count++;
			output(NULL, c);
			goto next;
		}
		width = 0;
		flags = 0;
		precision = -1;
		sign = 0;
reswitch:
		c = *++format;
		switch (c) {
		case '\0':
			return count;
		case ' ':
			if (sign == 0)
				sign = ' ';
			goto reswitch;
		case '#':
			flags |= LIBNET_PRINTF_ALTERNATE;
			goto reswitch;
		case '*':
			width = va_arg(ap, int);
			if (width < 0) {
				width = -width;
				flags |= LIBNET_PRINTF_LEFT;
			}
			goto reswitch;
		case '+':
			sign = '+';
			goto reswitch;
		case '-':
			flags |= LIBNET_PRINTF_LEFT;
			goto reswitch;
		case '.':
			c = *++format;
			if (c == '*') {
				precision = va_arg(ap, int);
				if (precision < 0)
					precision = -1;
			} else {
				precision = 0;
				if (LIBNET_PRINTF_ISDIGIT(c)) {
					do {
						precision = precision * 10 + c - '0';
						c = *++format;
					} while (LIBNET_PRINTF_ISDIGIT(c));
					if (precision < 0)
						precision = -1;
				}
				format--;
			}
			goto reswitch;
		case '0':
			flags |= LIBNET_PRINTF_ZERO;
			goto reswitch;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			width = 0;
			for (;;) {
				width = width * 10 + c - '0';
				c = format[1];
				if (!LIBNET_PRINTF_ISDIGIT(c))
					break;
				format++;
			}
			goto reswitch;
		case 'h':
			flags |= LIBNET_PRINTF_SHORT;
			goto reswitch;
		case 'l':
			if (flags & LIBNET_PRINTF_LONG) {
				flags &= ~LIBNET_PRINTF_LONG;
				flags |= LIBNET_PRINTF_LONGLONG;
			} else {
				flags |= LIBNET_PRINTF_LONG;
			}
			goto reswitch;
		case 'L':
		case 'j':
			flags |= LIBNET_PRINTF_LONGLONG;
			goto reswitch;
		case 'c':
			conversion = format;
			character = va_arg(ap, int);
			s = &character;
			size = 1;
			length = 1;
			goto string;
		case 's':
			conversion = format;
			s = va_arg(ap, const char *);
			if (s == NULL)
				s = null;
			if (precision == -1)
				size = sceClibStrnlen(s, 512);
			else
				size = sceClibStrnlen(s, precision);
			length = size;
string:
			sign = 0;
			precision = 0;
			goto pad;
		case 'D':
			flags |= LIBNET_PRINTF_LONG;
			/* fall through */
		case 'd':
		case 'i':
			conversion = format;
			base = 10;
			if (flags & LIBNET_PRINTF_LONGLONG) {
				value64 = va_arg(ap, SceInt64);
				if ((SceInt64)value64 < 0) {
					value64 = -value64;
					sign = '-';
				}
				goto number64;
			}
			value = va_arg(ap, int);
			if ((int)value < 0) {
				sign = '-';
				value = -value;
			}
			goto number;
		case 'O':
			flags |= LIBNET_PRINTF_LONG;
			/* fall through */
		case 'o':
			base = 8;
			goto unsigned_number;
		case 'U':
			flags |= LIBNET_PRINTF_LONG;
			/* fall through */
		case 'u':
			base = 10;
unsigned_number:
			conversion = format;
			sign = 0;
			if (flags & LIBNET_PRINTF_LONGLONG) {
				value64 = va_arg(ap, SceUInt64);
				goto number64;
			}
			value = va_arg(ap, unsigned int);
			goto number;
		case 'X':
			digits = "0123456789ABCDEF";
			/* fall through */
		case 'x':
			conversion = format;
			base = 16;
			sign = 0;
			if (flags & LIBNET_PRINTF_LONGLONG) {
				value64 = va_arg(ap, SceUInt64);
				if ((flags & LIBNET_PRINTF_ALTERNATE) && value64 != 0)
					flags |= LIBNET_PRINTF_PREFIX;
				goto number64;
			}
			value = va_arg(ap, unsigned int);
			if ((flags & LIBNET_PRINTF_ALTERNATE) && value != 0)
				flags |= LIBNET_PRINTF_PREFIX;
			goto number;
		case 'p':
			conversion = format;
			if (width == 0)
				width = 10;
			value = va_arg(ap, unsigned int);
			flags |= LIBNET_PRINTF_PREFIX | LIBNET_PRINTF_ZERO;
			base = 16;
			sign = 0;
			if (flags & LIBNET_PRINTF_LONGLONG)
				goto number64;
			goto number;
		default:
			conversion = format;
			count++;
			output(NULL, c);
			goto next;
		}

number:
		if (precision != -1) {
			flags &= ~LIBNET_PRINTF_ZERO;
			if (value == 0 && precision == 0) {
				s = end;
				size = 0;
				length = 0;
				goto converted;
			}
		}
		s = end;
		do {
			*(char *)--s = digits[value % base];
			value /= base;
		} while (value != 0);
		goto digits_done;

number64:
		if (precision != -1) {
			flags &= ~LIBNET_PRINTF_ZERO;
			if (value64 == 0 && precision == 0) {
				s = end;
				size = 0;
				length = 0;
				goto converted;
			}
		}
		s = end;
		do {
			*(char *)--s = digits[value64 % base];
			value64 /= base;
		} while (value64 != 0);

digits_done:
		/* the alternate form of octal starts with 0 */
		if ((flags & LIBNET_PRINTF_ALTERNATE) && base == 8 && *s != '0')
			*(char *)--s = '0';
		digits = "0123456789abcdef";
		size = end - s;
		length = size;
converted:
		value = 0;
		value64 = 0;
		if (sign != 0)
			size++;
pad:
		if (flags & LIBNET_PRINTF_PREFIX)
			size += 2;
		field = size < precision ? precision : size;
		if (!(flags & (LIBNET_PRINTF_LEFT | LIBNET_PRINTF_ZERO)) && width != 0 &&
		    field < width) {
			for (i = field; i != width; i++)
				output(NULL, ' ');
		}
		if (sign != 0)
			output(NULL, (char)sign);
		if (flags & LIBNET_PRINTF_PREFIX) {
			output(NULL, '0');
			c = conversion[0];
			output(NULL, c == 'p' ? 'x' : c);
		}
		if ((flags & (LIBNET_PRINTF_LEFT | LIBNET_PRINTF_ZERO)) == LIBNET_PRINTF_ZERO) {
			for (i = field; i < width; i++)
				output(NULL, '0');
		}
		for (; size < precision; size++)
			output(NULL, '0');
		for (i = 0; i < length; i++)
			output(NULL, s[i]);
		if ((flags & LIBNET_PRINTF_LEFT) && field < width) {
			for (i = field; i != width; i++)
				output(NULL, ' ');
		}
		count += field < width ? width : field;
next:
		format = conversion;
		c = *++format;
	} while (c != '\0');
	return count;
}
