/* wcstold function */
#include <wchar.h>
#include <xwchar.h>

long double wcstold(const wchar_t *__restrict s, wchar_t **__restrict endptr)
{
	return _WStold(s, endptr, 0);
}
