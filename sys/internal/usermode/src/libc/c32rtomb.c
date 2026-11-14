/* c32rtomb function */
#include <uchar.h>

size_t c32rtomb(char *__restrict s, char32_t c32, mbstate_t *__restrict ps)
{
	char buf[_MBMAX];
	unsigned long wc = c32;
	int nextra;
	char *p;

	if (s == NULL) {
		wc = 0;
		s = buf;
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
