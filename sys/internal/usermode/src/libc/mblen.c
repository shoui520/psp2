/* mblen function */
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

int mblen(const char *s, size_t n)
{
	_Mbstinit(mbst);
	int ans = _Mbtowc(NULL, s, n, &mbst);

	return ans < 0 ? -1 : ans;
}
