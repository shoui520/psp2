/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * SCE adaptation of the supported Berkeley vfprintf5.47 (Net2) upstream base.
 * Pristine source and license: src/vendor/bsd-stdio-net2-5.47/.
 * Archival source: https://www.tuhs.org/cgi-bin/utree.pl?file=Net2/usr/src/lib/libc/stdio/vfprintf.c
 * The exact historical Sony release is not uniquely established.
 *
 * SCE: character callbacks with200/201 events replace FILE/vector output.
 * Only integer/string conversions remain. LONGDBL becomes QUADINT; ll also
 * selects quads, while h is accepted without narrowing. Pointer formatting
 * defaults to width10, zero padding and a prefix; Lp consumes one word but
 * formats a zero quad. Strings use strnlen. Numeric loops reset the persistent
 * hex-digit table only when digits were generated. Unknown conversions emit
 * one character immediately. Native wrapping width/count arithmetic is explicit.
 */
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

extern int look_ctype_table(int character);

enum {
	LONGINT = 0x01,
	QUADINT = 0x02,
	SHORTINT = 0x04,
	ALT = 0x08,
	LADJUST = 0x10,
	ZEROPAD = 0x20,
	HEXPREFIX = 0x40
};

typedef void (*PrntOutput)(void *context, int character);

int __prnt(PrntOutput output, void *context, const char *format, va_list arguments)
{
	const char *digits = "0123456789abcdef";
	uint32_t count = 0;
	char buffer[24];

	if (!format)
		return 0;
	output(context, 0x200);
	for (;;) {
		int character = (signed char)*format++;
		uint32_t flags, width;
		int32_t precision, size, fieldSize, realSize;
		int sign, base;
		const char *text;
		char *cursor;
		uint32_t number;
		uint64_t quad;
		int signedNumber;

		if (!character)
			break;
		if (character != '%') {
			output(context, character);
			count++;
			continue;
		}
		flags = 0;
		width = 0;
		precision = -1;
		sign = 0;
readFlag:
		character = (signed char)*format++;
reswitch:
		switch (character) {
		case 0:
			goto done;
		case ' ':
			if (!sign)
				sign = ' ';
			goto readFlag;
		case '#':
			flags |= ALT;
			goto readFlag;
		case '*':
			width = va_arg(arguments, uint32_t);
			if ((int32_t)width >= 0)
				goto readFlag;
			width = 0U - width;
			/* Fall through. */
		case '-':
			flags |= LADJUST;
			goto readFlag;
		case '+':
			sign = '+';
			goto readFlag;
		case '.': {
			uint32_t value = 0;

			character = (signed char)*format++;
			if (character == '*') {
				precision = va_arg(arguments, int32_t);
				if (precision < 0)
					precision = -1;
				goto readFlag;
			}
			while (character >= 0 && (look_ctype_table(character) & 4)) {
				value = value * 10 + character - '0';
				character = (signed char)*format++;
			}
			precision = (int32_t)value < 0 ? -1 : (int32_t)value;
			goto reswitch;
		}
		case '0':
			flags |= ZEROPAD;
			goto readFlag;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			width = 0;
			do {
				width = width * 10 + character - '0';
				character = (signed char)*format++;
			} while (character >= 0 && (look_ctype_table(character) & 4));
			goto reswitch;
		case 'L':
			flags |= QUADINT;
			goto readFlag;
		case 'h':
			flags |= SHORTINT;
			goto readFlag;
		case 'l':
			if (flags & LONGINT) {
				flags &= ~LONGINT;
				flags |= QUADINT;
			} else {
				flags |= LONGINT;
			}
			goto readFlag;
		case 'c':
			buffer[0] = va_arg(arguments, int);
			text = buffer;
			size = 1;
			sign = 0;
			precision = 0;
			goto field;
		case 's':
			text = va_arg(arguments, const char *);
			if (!text)
				text = "(null)";
			size = strnlen(text, (uint32_t)precision);
			sign = 0;
			precision = 0;
			goto field;
		case 'D': case 'U': case 'O':
			flags |= LONGINT;
			/* Fall through. */
		case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p':
			break;
		default:
			output(context, character);
			count++;
			continue;
		}

		if (character == 'X')
			digits = "0123456789ABCDEF";
		base = (character == 'o' || character == 'O') ? 8 :
			(character == 'x' || character == 'X' || character == 'p') ? 16 : 10;
		signedNumber = character == 'd' || character == 'i' || character == 'D';
		if (!signedNumber)
			sign = 0;
		if (character == 'p') {
			if (!width)
				width = 10;
			flags |= ZEROPAD | HEXPREFIX;
			number = va_arg(arguments, uint32_t);
			quad = 0;
		} else if (flags & QUADINT) {
			quad = va_arg(arguments, uint64_t);
			if (signedNumber && (int64_t)quad < 0) {
				quad = 0ULL - quad;
				sign = '-';
			}
			if ((flags & ALT) && base == 16 && quad)
				flags |= HEXPREFIX;
		} else {
			number = va_arg(arguments, uint32_t);
			if (signedNumber && (int32_t)number < 0) {
				number = 0U - number;
				sign = '-';
			}
			if ((flags & ALT) && base == 16 && number)
				flags |= HEXPREFIX;
		}
		if (precision != -1)
			flags &= ~ZEROPAD;
		cursor = buffer + sizeof(buffer);
		if (flags & QUADINT) {
			if (quad || precision != 0) {
				do {
					*--cursor = digits[quad % (uint32_t)base];
					quad /= (uint32_t)base;
				} while (quad);
				if ((flags & ALT) && base == 8 && *cursor != '0')
					*--cursor = '0';
				digits = "0123456789abcdef";
			}
		} else if (number || precision != 0) {
			do {
				if (base == 10) {
					*--cursor = digits[number % 10];
					number /= 10;
				} else {
					*--cursor = digits[number & (base - 1)];
					number >>= 31 - __builtin_clz((uint32_t)base);
				}
			} while (number);
			if ((flags & ALT) && base == 8 && *cursor != '0')
				*--cursor = '0';
			digits = "0123456789abcdef";
		}
		text = cursor;
		size = buffer + sizeof(buffer) - cursor;
field:
		fieldSize = size;
		if (sign)
			fieldSize++;
		if (flags & HEXPREFIX)
			fieldSize += 2;
		realSize = precision > fieldSize ? precision : fieldSize;
		if (!(flags & (LADJUST | ZEROPAD))) {
			for (int32_t position = realSize; position < (int32_t)width; position++)
				output(context, ' ');
		}
		if (sign)
			output(context, sign);
		if (flags & HEXPREFIX) {
			int prefix;

			output(context, '0');
			/* SCE reloads the conversion byte after the first callback. */
			prefix = (signed char)format[-1];
			output(context, prefix == 'p' ? 'x' : prefix);
		}
		if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD) {
			for (int32_t position = realSize; position < (int32_t)width; position++)
				output(context, '0');
		}
		for (int32_t position = fieldSize; position < precision; position++)
			output(context, '0');
		for (int32_t position = 0; position < size; position++)
			output(context, (signed char)text[position]);
		if (flags & LADJUST) {
			for (int32_t position = realSize; position < (int32_t)width; position++)
				output(context, ' ');
		}
		count += (int32_t)width > realSize ? width : (uint32_t)realSize;
	}
done:
	output(context, 0x201);
	return count;
}
