/* mbrtoc16 function */
#include <errno.h>
#include <uchar.h>

size_t mbrtoc16(char16_t *__restrict pc16, const char *__restrict s, size_t n,
		mbstate_t *__restrict pst)
{
	unsigned long wc = pst->_Wchar;
	signed char nleft = (signed char)pst->_State;
	const unsigned char *su;

	if (s == NULL) {
		s = "";
		pc16 = NULL;
		n = 1;
	}
	for (su = (const unsigned char *)s;; ++su, --n) {
		if (nleft == 16) {	/* deliver the low surrogate */
			if (pc16 != NULL)
				*pc16 = (char16_t)(wc & 0x3ff | 0xdc00);
			pst->_State = 0;
			return (size_t)-3;
		}
		if (n == 0) {	/* save the partial character */
			pst->_Wchar = wc;
			pst->_State = nleft;
			return (size_t)-2;
		}
		if (0 < nleft) {	/* a continuation byte */
			if ((*su & 0xc0) != 0x80)
				goto ilseq;
			wc = wc << 6 | *su & 0x3f;
			--nleft;
		} else if ((*su & 0x80) == 0) {
			wc = *su;
		} else if ((*su & 0xe0) == 0xc0) {
			nleft = 1;
			wc = *su & 0x1f;
		} else if ((*su & 0xf0) == 0xe0) {
			nleft = 2;
			wc = *su & 0x0f;
		} else if ((*su & 0xf8) == 0xf0) {
			nleft = 3;
			wc = *su & 0x07;
		} else if ((*su & 0xfc) == 0xf8) {
			nleft = 4;
			wc = *su & 0x03;
		} else if ((*su & 0xfc) == 0xfc) {
			nleft = 5;
			wc = *su & 0x03;
		} else {
			goto ilseq;
		}
		if (nleft == 0)
			break;
	}
	if (0x10ffff < wc)
		goto ilseq;
	if (wc <= 0xffff) {	/* a character of the basic plane */
		if (pc16 != NULL)
			*pc16 = (char16_t)wc;
		pst->_State = 0;
		return wc == 0 ? 0 : (const char *)su - s + 1;
	}
	/* the high surrogate; the low one comes next call */
	if (pc16 != NULL)
		*pc16 = (char16_t)((wc >> 10) - 0x40 | 0xd800);
	pst->_State = 16;
	return (const char *)su - s + 1;

ilseq:
	errno = EILSEQ;
	return (size_t)-1;
}
