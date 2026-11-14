/* wcscoll function */
#include <wchar.h>
#include <xwchar.h>

typedef struct {
	wchar_t buf[32];	/* the transformed characters */
	const wchar_t *s1;	/* the next source character */
	const wchar_t *s2;	/* the source, to rescan */
	const wchar_t *sout;	/* the next transformed character to compare */
	mbstate_t state;	/* the transform's conversion state */
} Sctl;

/* get transformed characters */
static size_t getwxfrm(Sctl *p)
{
	size_t i;

	do {	/* loop until characters are delivered */
		p->sout = p->buf;
		i = _Wcsxfrm(p->buf, &p->s1, sizeof(p->buf) / sizeof(wchar_t), &p->state);
		if (0 < i && p->buf[i - 1] == L'\0')
			return i;
		if (*p->s1 == L'\0')
			p->s1 = p->s2;	/* rescan */
	} while (i == 0);
	return i;
}

int _Wcscollx(const wchar_t *s1, const wchar_t *s2)
{
	size_t n1, n2;
	Sctl st1, st2;
	static const mbstate_t initial = {0};

	st1.s1 = s1;
	st1.s2 = s1;
	st1.state = initial;
	st2.s1 = s2;
	st2.s2 = s2;
	st2.state = initial;
	for (n1 = n2 = 0;;) {	/* compare transformed characters */
		int ans;
		size_t n;

		if (n1 == 0)
			n1 = getwxfrm(&st1);
		if (n2 == 0)
			n2 = getwxfrm(&st2);
		n = n1 < n2 ? n1 : n2;
		if (n == 0)
			return n1 == n2 ? 0 : 0 < n2 ? -1 : +1;
		ans = wmemcmp(st1.sout, st2.sout, n);
		if (ans != 0 || (n1 == n2 && st1.sout[n - 1] == L'\0'))
			return ans;
		st1.sout += n;
		n1 -= n;
		st2.sout += n;
		n2 -= n;
	}
}

int wcscoll(const wchar_t *s1, const wchar_t *s2)
{
	return _Wcscollx(s1, s2);
}
