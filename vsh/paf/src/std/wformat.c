#include "../compiler.h"
#include "printf.h"
#include "string.h"
#include "ctype.h"

int PafWideFormatBody(PafWideFormatOutput output, void *context,
		      const uint16_t *format, va_list args, uint16_t *buffer)
{
	/* The ABI entry supplies the native, untouched conversion area. */
	uint16_t *const end = buffer + 31;
	const uint16_t *digits = L"0123456789abcdef";
	uint64_t wideValue = 0;
	uint32_t value = 0, total = 0;
	const uint16_t *cursor = format;

	if (!format)
		return 0;
	output(context, 0x20200);
	while (*cursor) {
		uint32_t flags = 0;
		int32_t width = 0, precision = -1, minimumDigits = 0;
		int32_t trailingZeros = 0, textLength, contentLength, paddedLength;
		uint16_t sign = 0, ch = *cursor;
		const uint16_t *text, *conversion, *nextFormat;
		uint32_t base;
		float fraction;
		int floating = 0, isSigned = 0;

		if (ch != '%') {
			output(context, ch);
			total++;
			cursor++;
			continue;
		}
		for (;;) {
			ch = *++cursor;
			if (!ch)
				goto finish;
			switch (ch) {
			case ' ':
				if (!sign)
					sign = ' ';
				continue;
			case '#':
				flags |= PAF_FORMAT_ALTERNATE;
				continue;
			case '*':
				width = va_arg(args, int32_t);
				if (width < 0) {
					width = 0U - (uint32_t)width;
					flags |= PAF_FORMAT_LEFT;
				}
				continue;
			case '+':
				sign = '+';
				continue;
			case '-':
				flags |= PAF_FORMAT_LEFT;
				continue;
			case '.':
				ch = *++cursor;
				if (ch == '*') {
					precision = va_arg(args, int32_t);
				} else {
					precision = 0;
					while (ch <= 0x7f && (sce_paf_look_ctype_table(ch) & 4)) {
						precision = 10U * (uint32_t)precision + *cursor - '0';
						ch = *++cursor;
					}
					cursor--;
				}
				if (precision < 0)
					precision = -1;
				continue;
			case '0':
				flags |= PAF_FORMAT_ZERO;
				continue;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				width = 0;
				do {
					width = 10U * (uint32_t)width + *cursor - '0';
					ch = *++cursor;
				} while (ch <= 0x7f && (sce_paf_look_ctype_table(ch) & 4));
				cursor--;
				continue;
			case 'L':
				flags |= PAF_FORMAT_LONG_LONG;
				continue;
			case 'h':
				flags |= PAF_FORMAT_SHORT;
				continue;
			case 'l':
				if (flags & PAF_FORMAT_LONG)
					flags = (flags & ~PAF_FORMAT_LONG) | PAF_FORMAT_LONG_LONG;
				else
					flags |= PAF_FORMAT_LONG;
				continue;
			case 'z':
				flags |= PAF_FORMAT_LONG;
				continue;
			default:
				break;
			}
			break;
		}
		conversion = cursor;
		nextFormat = cursor + 1;
		switch (ch) {
		case 'c':
			buffer[0] = va_arg(args, uint32_t);
			text = buffer;
			textLength = contentLength = 1;
			sign = 0;
			goto emit;
		case 's': {
			const uint16_t *nul;

			text = va_arg(args, const uint16_t *);
			if (!text)
				text = L"(null)";
			if (precision < 0) {
				textLength = sce_paf_wcslen(text);
			} else {
				nul = sce_paf_wmemchr(text, 0, precision);
				textLength = nul ?
					(int32_t)((uintptr_t)nul - (uintptr_t)text) / 2 : precision;
				if (textLength > precision)
					textLength = precision;
			}
			contentLength = textLength;
			sign = 0;
			goto emit;
		}
		case 'n': {
			void *ptr = va_arg(args, void *);

			if (!(flags & PAF_FORMAT_LONG) && (flags & PAF_FORMAT_SHORT))
				*(uint16_t *)ptr = total;
			else
				*(uint32_t *)ptr = total;
			cursor = nextFormat;
			continue;
		}
		case 'D':
			flags |= PAF_FORMAT_LONG;
			/* fall through */
		case 'd': case 'i':
			base = 10;
			isSigned = 1;
			break;
		case 'O':
			flags |= PAF_FORMAT_LONG;
			/* fall through */
		case 'o':
			base = 8;
			break;
		case 'U':
			flags |= PAF_FORMAT_LONG;
			/* fall through */
		case 'u':
			base = 10;
			break;
		case 'X':
			digits = L"0123456789ABCDEF";
			/* fall through */
		case 'x':
			base = 16;
			break;
		case 'p':
			value = va_arg(args, uint32_t);
			base = 16;
			fraction = 0.0f;
			sign = 0;
			goto number;
		case 'e': case 'f': case 'g':
			fraction = (float)va_arg(args, double);
			if (fraction < 0.0f) {
				fraction = -fraction;
				sign = '-';
			}
			if (fraction > 2147483648.0f) {
				value = 0x7fffffff;
				fraction = 0.0f;
			} else {
				value = (uint32_t)fraction;
				fraction -= (float)value;
			}
			base = 10;
			floating = 1;
			if (precision < 0)
				precision = 6;
			goto number;
		default:
			output(context, ch);
			total++;
			cursor = nextFormat;
			continue;
		}
		if (flags & PAF_FORMAT_LONG_LONG) {
			wideValue = va_arg(args, uint64_t);
			if (isSigned && (int64_t)wideValue < 0) {
				wideValue = 0ULL - wideValue;
				sign = '-';
			}
		} else {
			value = va_arg(args, uint32_t);
			if (isSigned && (int32_t)value < 0) {
				value = 0U - value;
				sign = '-';
			}
		}
		if (!isSigned)
			sign = 0;
		if (base == 16 && (flags & PAF_FORMAT_ALTERNATE) &&
		    ((flags & PAF_FORMAT_LONG_LONG) ? wideValue != 0 : value != 0))
			flags |= PAF_FORMAT_HEX_PREFIX;
		fraction = 0.0f;
number:
		text = end;
		if (floating) {
			minimumDigits = 1;
			trailingZeros = precision;
			if (precision) {
				uint16_t *dest = end - precision;
				int32_t generated = 0;

				text = dest - 1;
				dest[-1] = '.';
				if (precision > 0) {
					do {
						uint16_t digit = '0';

						fraction *= 10.0f;
						if ((double)fraction <= 2147484000.0)
							digit = (int32_t)fraction % 10 + '0';
						*dest++ = (sce_paf_look_ctype_table(digit) & 4) ? digit : '0';
						/* Keep the native limit and its trailing-zero count. */
						if (generated > 10)
							break;
						generated++;
					} while (precision > generated);
				}
				trailingZeros = (uint32_t)precision - generated;
			} else if (flags & PAF_FORMAT_ALTERNATE) {
				text = end - 1;
				end[-1] = '.';
			}
		} else {
			minimumDigits = precision;
			if (precision >= 0)
				flags &= ~PAF_FORMAT_ZERO;
		}
		if (value || wideValue || minimumDigits) {
			uint16_t *dest = (uint16_t *)text;

			if (wideValue) {
				do {
					*--dest = digits[wideValue % base];
					wideValue /= base;
				} while (wideValue);
			} else {
				do {
					*--dest = digits[value % base];
					value /= base;
				} while (value);
			}
			digits = L"0123456789abcdef";
			if ((flags & PAF_FORMAT_ALTERNATE) && base == 8 && *dest != '0')
				*--dest = '0';
			text = dest;
		}
		textLength = (int32_t)((uintptr_t)end - (uintptr_t)text) / 2;
		contentLength = (uint32_t)trailingZeros + textLength;
emit:
		if (sign)
			contentLength = (uint32_t)contentLength + 1;
		if (flags & PAF_FORMAT_HEX_PREFIX)
			contentLength = (uint32_t)contentLength + 2;
		paddedLength = minimumDigits > contentLength ? minimumDigits : contentLength;
		if (!(flags & (PAF_FORMAT_ZERO | PAF_FORMAT_LEFT)) && width && paddedLength < width) {
			uint32_t remaining = (uint32_t)width - paddedLength;

			do {
				output(context, ' ');
			} while (--remaining);
		}
		if (sign)
			output(context, sign);
		if (flags & PAF_FORMAT_HEX_PREFIX) {
			output(context, '0');
			output(context, *conversion);
		}
		if ((flags & (PAF_FORMAT_ZERO | PAF_FORMAT_LEFT)) == PAF_FORMAT_ZERO &&
		    paddedLength < width) {
			uint32_t remaining = (uint32_t)width - paddedLength;

			do {
				output(context, '0');
			} while (--remaining);
		}
		if (minimumDigits > contentLength) {
			uint32_t remaining = (uint32_t)minimumDigits - contentLength;

			do {
				output(context, '0');
			} while (--remaining);
		}
		for (int32_t remaining = (uint32_t)textLength - 1; remaining >= 0;
		     remaining = (uint32_t)remaining - 1)
			output(context, *text++);
		while ((trailingZeros = (uint32_t)trailingZeros - 1) >= 0)
			output(context, '0');
		if ((flags & PAF_FORMAT_LEFT) && paddedLength < width) {
			uint32_t remaining = (uint32_t)width - paddedLength;

			do {
				output(context, ' ');
			} while (--remaining);
		}
		total += paddedLength < width ? width : paddedLength;
		cursor = nextFormat;
	}
finish:
	output(context, 0x20201);
	return total;
}
