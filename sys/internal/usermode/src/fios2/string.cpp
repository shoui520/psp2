/*
 * string.cpp: the library's own memory and string routines, its qsort and bsearch, and its
 * formatter. It defines sceClibMemcpy, sceClibMemset, sceClibMemcmp and sceClibMemmove itself
 * rather than importing SceLibKernel's (see fios2_internal.h).
 */
#include <stdlib.h>
#include "fios2_internal.h"

void *sceClibMemcpy(void *dst, const void *src, SceSize size)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (d != s)
		while (size-- != 0)
			*d++ = *s++;
	return dst;
}

namespace std {

void *memcpy(void *dst, const void *src, size_t size)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (d != s)
		while (size-- != 0)
			*d++ = *s++;
	return dst;
}

} /* namespace std */

void *sceClibMemset(void *dst, int c, SceSize size)
{
	uint8_t *d = (uint8_t *)dst;

	while (size-- != 0)
		*d++ = c;
	return dst;
}

namespace std {

void *memset(void *dst, int c, size_t size)
{
	uint8_t *d = (uint8_t *)dst;

	while (size-- != 0)
		*d++ = c;
	return dst;
}

} /* namespace std */

/* nothing calls it (Sony's binary keeps it) */
char *fiosStrchr(const char *s, int c)
{
	char ch = c;

	while (*s != ch)
		if (*s++ == 0)
			return NULL;
	return (char *)s;
}

#define LOWER(c)	((c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 'a' : (c))
#define LOWER4(c)	LOWER(c), LOWER((c) + 1), LOWER((c) + 2), LOWER((c) + 3)
#define LOWER16(c)	LOWER4(c), LOWER4((c) + 4), LOWER4((c) + 8), LOWER4((c) + 12)

/* each byte's lower case, for the case-insensitive compares */
uint8_t s_lowerCase[256] = {
	LOWER16(0x00), LOWER16(0x10), LOWER16(0x20), LOWER16(0x30),
	LOWER16(0x40), LOWER16(0x50), LOWER16(0x60), LOWER16(0x70),
	LOWER16(0x80), LOWER16(0x90), LOWER16(0xa0), LOWER16(0xb0),
	LOWER16(0xc0), LOWER16(0xd0), LOWER16(0xe0), LOWER16(0xf0),
};

int fiosStrcasecmp(const char *s1, const char *s2)
{
	for (;;) {
		uint8_t c1 = s_lowerCase[(uint8_t)*s1++];
		uint8_t c2 = s_lowerCase[(uint8_t)*s2++];

		if (c1 != c2)
			return c1 - c2;
		if (c1 == 0)
			return 0;
	}
}

int fiosStrncasecmp(const char *s1, const char *s2, size_t n)
{
	while (n-- != 0) {
		uint8_t c1 = s_lowerCase[(uint8_t)*s1++];
		uint8_t c2 = s_lowerCase[(uint8_t)*s2++];

		if (c1 != c2)
			return c1 - c2;
		if (c1 == 0)
			break;
	}
	return 0;
}

int fiosStrcmp(const char *s1, const char *s2)
{
	for (;;) {
		char c1 = *s1++;
		char c2 = *s2++;

		if (c1 != c2)
			return c1 - c2;
		if (c1 == 0)
			return 0;
	}
}

int fiosStrncmp(const char *s1, const char *s2, size_t n)
{
	while (n-- != 0) {
		char c1 = *s1++;
		char c2 = *s2++;

		if (c1 != c2)
			return c1 - c2;
		if (c1 == 0)
			break;
	}
	return 0;
}

size_t fiosStrlen(const char *s)
{
	size_t len = 0;

	while (*s++ != 0)
		len++;
	return len;
}

/* BSD's strlcpy: the length of src */
size_t fiosStrlcpy(char *dst, const char *src, size_t size)
{
	size_t len = 0;
	char c = *src;

	if (c != 0) {
		do {
			if (size <= 1)
				break;
			len++;
			*dst++ = *src;
			c = *++src;
			size--;
		} while (c != 0);
		if (c != 0) {
			do {
				c = *++src;
				len++;
			} while (c != 0);
		}
	}
	if (size != 0)
		*dst = 0;
	return len;
}

/* BSD's strlcat: the length of dst (up to size) and src */
size_t fiosStrlcat(char *dst, const char *src, size_t size)
{
	size_t len = 0;
	char c = *src;

	while (size != 0 && *dst != 0) {
		size--;
		dst++;
		len++;
	}
	while (size > 1 && c != 0) {
		size--;
		*dst++ = *src;
		c = *++src;
		len++;
	}
	while (c != 0) {
		c = *++src;
		len++;
	}
	if (size != 0)
		*dst = 0;
	return len;
}

int sceClibMemcmp(const void *s1, const void *s2, SceSize size)
{
	const uint8_t *p1 = (const uint8_t *)s1;
	const uint8_t *p2 = (const uint8_t *)s2;

	while (size-- != 0) {
		int diff = *p1++ - *p2++;

		if (diff != 0)
			return diff;
	}
	return 0;
}

void *sceClibMemmove(void *dst, const void *src, SceSize size)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (d < s) {
		while (size-- != 0)
			*d++ = *s++;
	} else if (d > s) {
		d += size - 1;
		s += size - 1;
		while (size-- != 0)
			*d-- = *s--;
	}
	return dst;
}

#define QSORT_STACK_DEPTH	50	/* the ranges qsort keeps */
#define QSORT_MEDIAN_SPAN	6	/* a range this many bytes long takes the median of three */

struct QsortRange {
	uint8_t *pLo;
	uint8_t *pHi;		/* the last element */
};

/* the C library's, declared in namespace std by the SDK's C++ headers */
namespace std {

/*
 * A quicksort on its own stack of ranges, the smaller part of each partition sorted first;
 * the pivot is the median of the first, middle and last elements.
 */
void qsort(void *base, size_t nelem, size_t size, int (*compar)(const void *, const void *))
{
	QsortRange stack[QSORT_STACK_DEPTH];
	QsortRange *pRange;
	int depth;

	if (nelem <= 1)
		return;
	stack[0].pLo = (uint8_t *)base;
	stack[0].pHi = (uint8_t *)base + nelem * size - size;
	pRange = stack;
	depth = 0;
	do {
		uint8_t *pLo = pRange->pLo;
		uint8_t *pHi = pRange->pHi;
		uint8_t *pPivot = pLo;
		uint8_t *pLeft = pLo;
		uint8_t *pRight = pHi;
		QsortRange *pNext;

		if ((size_t)(pHi - pLo) >= QSORT_MEDIAN_SPAN) {
			pPivot = pLo + ((size_t)(pHi - pLo) / size >> 1) * size;
			if (compar(pLo, pPivot) >= 0) {
				if (compar(pLo, pHi) < 0)
					pPivot = pLo;
				else if (compar(pHi, pPivot) >= 0)
					pPivot = pHi;
			} else if (compar(pPivot, pHi) >= 0) {
				if (compar(pHi, pLo) >= 0)
					pPivot = pHi;
				else
					pPivot = pLo;
			}
		}
		do {
			while (compar(pLeft, pPivot) < 0)
				pLeft += size;
			while (compar(pRight, pPivot) > 0)
				pRight -= size;
			if (pRight < pLeft)
				break;
			if (pLeft < pRight) {
				uint8_t *p = pLeft;
				uint8_t *q = pRight;
				size_t n;

				for (n = size; n != 0; n--) {
					uint8_t c = *p;

					*p++ = *q;
					*q++ = c;
				}
				if (pLeft == pPivot)
					pPivot = pRight;
				else if (pRight == pPivot)
					pPivot = pLeft;
			}
			pLeft += size;
			pRight -= size;
		} while (pRight >= pLeft);

		pNext = pRange;
		if (pLo < pRight) {
			WWS_ASSERT(depth < QSORT_STACK_DEPTH);
			depth++;
			pRange->pHi = pRight;
			pNext++;
		}
		if (pLeft < pHi) {
			WWS_ASSERT(depth < QSORT_STACK_DEPTH);
			depth++;
			pNext->pLo = pLeft;
			pNext->pHi = pHi;
			pNext++;
		}
		if (pLo < pRight && pLeft < pHi && pRight - pLo < pHi - pLeft) {
			QsortRange range;

			WWS_ASSERT(depth >= 2);
			range = pNext[-1];
			pNext[-1] = pNext[-2];
			pNext[-2] = range;
		}
		pRange = pNext - 1;
		depth--;
	} while (depth >= 0);
}

/* compar is given the element first, then the key */
void *bsearch(const void *key, const void *base, size_t nelem, size_t size,
	      int (*compar)(const void *, const void *))
{
	int lo;
	int hi;

	if (base == NULL || compar == NULL || nelem == 0 || size == 0)
		return NULL;
	lo = 0;
	hi = (int)nelem - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		const uint8_t *p = (const uint8_t *)base + mid * size;
		int cmp = compar(p, key);

		if (cmp == 0)
			return (void *)p;
		if (cmp <= 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

} /* namespace std */

/* fiosVsnprintf's states: where in a conversion the next format character is */
enum {
	FORMAT_TEXT,		/* copied out */
	FORMAT_PERCENT,		/* after a '%' */
	FORMAT_FLAGS,
	FORMAT_WIDTH,
	FORMAT_DOT,		/* after a negative '*' width */
	FORMAT_PRECISION,
	FORMAT_LENGTH,
	FORMAT_CONVERSION
};

/* a conversion's flags */
#define FORMAT_ZERO	0x01	/* '0' */
#define FORMAT_MINUS	0x02	/* '-' */
#define FORMAT_PLUS	0x04	/* '+' */
#define FORMAT_SPACE	0x08	/* ' ' */
#define FORMAT_ALT	0x10	/* '#' */
#define FORMAT_UNSIGNED	0x20
#define FORMAT_UPPER	0x40

/* one character at count, if it fits */
#define FORMAT_OUT(c)						\
	do {							\
		if (size > count)				\
			dst[count] = (c);			\
		count++;					\
	} while (0)

/*
 * A small vsnprintf: c, d, i, o, p, s, u, x and X with the flags, width, precision and
 * l or ll; a floating-point argument prints "UNSUP". "%%" prints '%' and ends as a
 * conversion it does not know. The length of the whole output, as if size were enough.
 */
int fiosVsnprintf(char *dst, size_t size, const char *fmt, va_list ap)
{
	size_t count = 0;
	int state = FORMAT_TEXT;
	int flags = 0;
	int width = 0;
	int precision = -1;
	int length = 0;
	char c;

	if (dst == NULL)
		size = 0;
	for (c = *fmt; c != 0; c = *++fmt) {
		switch (state) {
		case FORMAT_TEXT:
			if (c == '%') {
				state = FORMAT_PERCENT;
				flags = 0;
				width = 0;
				length = 0;
				precision = -1;
			} else {
				FORMAT_OUT(c);
			}
			break;
		case FORMAT_PERCENT:
			if (c == '%')
				FORMAT_OUT('%');
			else
				state = FORMAT_FLAGS;
			/* fall through */
		case FORMAT_FLAGS:
			if (c == '0') {
				flags |= FORMAT_ZERO;
				break;
			}
			if (c == '-') {
				flags |= FORMAT_MINUS;
				break;
			}
			if (c == '+') {
				flags |= FORMAT_PLUS;
				break;
			}
			if (c == ' ') {
				flags |= FORMAT_SPACE;
				break;
			}
			if (c == '#') {
				flags |= FORMAT_ALT;
				break;
			}
			/* fall through */
		case FORMAT_WIDTH:
			state = FORMAT_WIDTH;
			if (c >= '0' && c <= '9') {
				width = width * 10 + c - '0';
				break;
			}
			if (c == '*') {
				width = va_arg(ap, int);
				if (width < 0) {
					width = -width;
					state = FORMAT_DOT;
					flags |= FORMAT_MINUS;
				}
				break;
			}
			/* fall through */
		case FORMAT_DOT:
			if (c == '.') {
				state = FORMAT_PRECISION;
				break;
			}
			/* fall through */
		case FORMAT_PRECISION:
			state = FORMAT_PRECISION;
			if (c >= '0' && c <= '9') {
				if (precision == -1)
					precision = 0;
				precision = precision * 10 + c - '0';
				break;
			}
			if (c == '*') {
				precision = va_arg(ap, int);
				if (precision < 0)
					precision = -1;
				state = FORMAT_LENGTH;
				break;
			}
			/* fall through */
		case FORMAT_LENGTH:
			state = FORMAT_LENGTH;
			if (c == 'l') {
				if (length < 2)
					length++;
				break;
			}
			/* fall through */
		case FORMAT_CONVERSION: {
			int base = 10;

			switch (c) {
			case 'A':
			case 'E':
			case 'F':
			case 'G':
			case 'a':
			case 'e':
			case 'f':
			case 'g':
				(void)va_arg(ap, double);
				FORMAT_OUT('U');
				FORMAT_OUT('N');
				FORMAT_OUT('S');
				FORMAT_OUT('U');
				FORMAT_OUT('P');
				break;
			case 'c':
				FORMAT_OUT((char)va_arg(ap, int));
				break;
			case 's': {
				const char *s = va_arg(ap, const char *);
				const char *p;
				int len = 0;
				int pad;

				if (s == NULL)
					s = "(null)";
				if (precision == -1)
					precision = 0x7fffffff;
				for (p = s; len < precision && *p != 0; p++)
					len++;
				pad = width - len;
				if (pad < 0)
					pad = 0;
				if (!(flags & FORMAT_MINUS))
					while (pad-- > 0)
						FORMAT_OUT(' ');
				for (; *s != 0 && precision-- > 0; s++)
					FORMAT_OUT(*s);
				if (flags & FORMAT_MINUS)
					while (pad-- > 0)
						FORMAT_OUT(' ');
				break;
			}
			case 'p':
				length = 1;
				flags |= FORMAT_ALT;
				/* fall through */
			case 'X':
				if (c == 'X')
					flags |= FORMAT_UPPER;
				/* fall through */
			case 'x':
				base = 16;
				/* fall through */
			case 'u':
			unsigned_number:
				flags |= FORMAT_UNSIGNED;
				/* fall through */
			case 'd':
			case 'i': {
				char buf[48];
				const char *digits = (flags & FORMAT_UPPER) ? "0123456789ABCDEF" :
									      "0123456789abcdef";
				int upper = flags & FORMAT_UPPER;
				int minus = flags & FORMAT_MINUS;
				int defaultPrecision = precision == -1;
				int64_t value;
				uint64_t magnitude;
				char sign = 0;
				char prefix = 0;
				int ndigits = 0;
				int zeros;
				int len;
				int pad;

				if (length <= 1)
					value = va_arg(ap, int);
				else if (length <= 2)
					value = va_arg(ap, int64_t);
				else
					value = va_arg(ap, int);
				if (flags & FORMAT_UNSIGNED) {
					magnitude = value;
				} else {
					magnitude = value < 0 ? -value : value;
					if (value < 0)
						sign = '-';
					else if (flags & FORMAT_PLUS)
						sign = '+';
					else if (flags & FORMAT_SPACE)
						sign = ' ';
				}
				do {
					buf[ndigits++] = digits[magnitude % base];
					magnitude /= base;
				} while (magnitude != 0);
				if ((flags & FORMAT_ALT) && value != 0) {
					if (base == 8) {
						if (precision <= ndigits)
							precision = ndigits + 1;
					} else if (base == 16) {
						prefix = upper ? 'X' : 'x';
					}
				}
				zeros = precision - ndigits;
				if (zeros < 0)
					zeros = 0;
				len = (ndigits > precision ? ndigits : precision) + (sign ? 1 : 0) +
				      (prefix ? 2 : 0);
				if (len < 0)
					len = 0;
				pad = width - len;
				if ((flags & (FORMAT_ZERO | FORMAT_MINUS)) == FORMAT_ZERO &&
				    defaultPrecision) {
					zeros += pad;
					pad = 0;
				}
				if (!minus)
					while (pad-- > 0)
						FORMAT_OUT(' ');
				if (sign)
					FORMAT_OUT(sign);
				if (prefix) {
					FORMAT_OUT('0');
					FORMAT_OUT(prefix);
				}
				while (zeros-- > 0)
					FORMAT_OUT('0');
				while (ndigits-- > 0)
					FORMAT_OUT(buf[ndigits]);
				if (minus)
					while (pad-- > 0)
						FORMAT_OUT(' ');
				break;
			}
			case 'o':
				base = 8;
				goto unsigned_number;
			default:
				break;
			}
			state = FORMAT_TEXT;
			break;
		}
		default:
			state = FORMAT_TEXT;
			break;
		}
	}
	if (size != 0)
		dst[count < size - 1 ? count : size - 1] = 0;
	return count;
}

int fiosSnprintf(char *dst, size_t size, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = fiosVsnprintf(dst, size, fmt, ap);
	va_end(ap);
	return ret;
}
