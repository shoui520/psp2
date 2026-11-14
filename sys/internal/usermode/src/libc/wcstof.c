/* wcstof function */
#include <wchar.h>
#include <xwchar.h>

float wcstof(const wchar_t *__restrict s, wchar_t **__restrict endptr)
{
	return _WStof(s, endptr, 0);
}
