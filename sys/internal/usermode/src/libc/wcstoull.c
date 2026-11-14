/* wcstoull function */
#include <inttypes.h>
#include <wchar.h>
#include <xwchar.h>

unsigned long long wcstoull(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base)
{
	return _WStoull(s, endptr, base);
}
