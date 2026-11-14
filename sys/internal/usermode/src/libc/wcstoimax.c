/* wcstoimax function */
#include <inttypes.h>
#include <wchar.h>
#include <xwchar.h>

intmax_t wcstoimax(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base)
{
	return _WStoll(s, endptr, base);
}
