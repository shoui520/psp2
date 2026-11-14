/* wcstod function */
#include <wchar.h>
#include <xwchar.h>

double wcstod(const wchar_t *__restrict s, wchar_t **__restrict endptr)
{
	return _WStod(s, endptr, 0);
}
