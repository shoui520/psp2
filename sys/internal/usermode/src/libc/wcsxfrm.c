/* wcsxfrm function */
#include <wchar.h>
#include <xwchar.h>

size_t _Wcsxfrmx(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n)
{
	size_t nx = 0;
	const wchar_t *s = s2;
	_Mbstinit(state);

	while (nx < n) {	/* translate and deliver */
		size_t i = _Wcsxfrm(s1, &s, n - nx, &state);

		s1 += i;
		nx += i;
		if (0 < i && s1[-1] == L'\0')
			return nx - 1;
		if (nx < n && *s == L'\0')
			s = s2;	/* rescan */
	}
	for (;;) {	/* translate and count */
		wchar_t buf[32];
		size_t i = _Wcsxfrm(buf, &s, sizeof(buf) / sizeof(wchar_t), &state);

		nx += i;
		if (0 < i && buf[i - 1] == L'\0')
			return nx - 1;
		if (i < sizeof(buf) / sizeof(wchar_t) && *s == L'\0')
			s = s2;	/* rescan */
	}
}

size_t wcsxfrm(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n)
{
	return _Wcsxfrmx(s1, s2, n);
}
