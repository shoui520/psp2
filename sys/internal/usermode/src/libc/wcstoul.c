/* wcstoul function */
#include <wchar.h>
#include <xwchar.h>

unsigned long wcstoul(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base)
{
	return _WStoul(s, endptr, base);
}
