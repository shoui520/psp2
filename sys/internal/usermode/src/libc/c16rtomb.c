/* c16rtomb function */
#include <uchar.h>

size_t c16rtomb(char *__restrict s, char16_t c16, mbstate_t *__restrict ps)
{
	char buf[_MBMAX];
	int c = c16;
	unsigned long wc = ps->_Wchar;
	int nextra;
	char *p;

	if (s == NULL) {
		c = 0;
		s = buf;
	}
	if ((char)ps->_State != 0) {
		/* the second half of a surrogate pair */
		if (c < 0xdc00 || 0xe000 <= c)
			return (size_t)-1;
		ps->_State = 0;
		wc |= c - 0xdc00;
	} else if (0xd800 <= c && c < 0xdc00) {
		/* the first half of a surrogate pair */
		ps->_State = 1;
		ps->_Wchar = (c - 0xd7c0) << 10;
		return 0;
	} else {
		wc = c;
	}
	if ((wc & ~(unsigned long)0x7f) == 0) {
		nextra = 0;
		*s = (char)wc;
	} else if ((wc & ~(unsigned long)0x7ff) == 0) {
		*s = (char)(0xc0 | wc >> 6);
		nextra = 1;
	} else if ((wc & ~(unsigned long)0xffff) == 0) {
		*s = (char)(0xe0 | wc >> 12);
		nextra = 2;
	} else if ((wc & ~(unsigned long)0x1fffff) == 0) {
		*s = (char)(0xf0 | wc >> 18);
		nextra = 3;
	} else if ((wc & ~(unsigned long)0x3ffffff) == 0) {
		*s = (char)(0xf8 | wc >> 24);
		nextra = 4;
	} else {
		*s = (char)(0xfc | wc >> 30);
		nextra = 5;
	}
	for (p = s + 1; 0 < nextra; nextra--)
		*p++ = (char)(0x80 | (wc >> 6 * (nextra - 1) & 0x3f));
	return p - s;
}
