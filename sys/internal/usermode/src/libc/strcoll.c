/* strcoll function */
#include <string.h>

#include "xstrxfrm.h"

typedef struct {
	char buf[32];		/* the transformed characters */
	const char *sout;	/* the next transformed character to compare */
	_Xfrmctl xfrm;		/* the transform's position in the source */
} Sctl;

int _Strcollx(const char *s1, const char *s2)
{
	size_t n1, n2;
	static const Sctl initial;
	Sctl st1 = initial;
	Sctl st2 = initial;

	st1.xfrm.s1 = s1;
	st2.xfrm.s1 = s2;
	for (n1 = n2 = 0;;) {	/* compare transformed characters */
		int ans;
		size_t n;

		if (n1 == 0) {
			st1.sout = st1.buf;
			n1 = _Strxfrm(st1.buf, sizeof(st1.buf), &st1.xfrm);
		}
		if (n2 == 0) {
			st2.sout = st2.buf;
			n2 = _Strxfrm(st2.buf, sizeof(st2.buf), &st2.xfrm);
		}
		n = n1 < n2 ? n1 : n2;
		if (n == 0)
			return n1 == n2 ? 0 : 0 < n2 ? -1 : +1;
		ans = memcmp(st1.sout, st2.sout, n);
		if (ans != 0 || (n1 == n2 && st1.sout[n - 1] == '\0'))
			return ans;
		st1.sout += n;
		n1 -= n;
		st2.sout += n;
		n2 -= n;
	}
}

int strcoll(const char *s1, const char *s2)
{
	return _Strcollx(s1, s2);
}
