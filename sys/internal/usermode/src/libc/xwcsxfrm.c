/*
 * _Wcsxfrm function: transform the next part of a wide string ("C" locale: copy it).
 */
#include <stddef.h>
#include <wchar.h>
#include <xwchar.h>

size_t _Wcsxfrm(wchar_t *buf, const wchar_t **ps, size_t n, mbstate_t *pst)
{
	const wchar_t *s = *ps;
	size_t i;

	(void)pst;	/* the "C" locale keeps no state */
	for (i = 0; i < n; ++buf) {
		wchar_t wc = *s;

		++i;
		*buf = wc;
		if (wc == L'\0')
			break;
		++s;
	}
	*ps = s;
	return i;
}
