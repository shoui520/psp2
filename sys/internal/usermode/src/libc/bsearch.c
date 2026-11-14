/* bsearch function */
#include <stdlib.h>

void *bsearch(const void *key, const void *base, size_t nelem, size_t size,
	      int (*cmp)(const void *, const void *))
{
	const char *p;
	size_t n;

	for (p = (const char *)base, n = nelem; n > 0;) {
		const size_t pivot = n >> 1;
		const char *const q = p + size * pivot;
		const int val = (*cmp)(key, q);

		if (val < 0) {
			n = pivot;
		} else if (val == 0) {
			return (void *)q;
		} else {
			p = q + size;
			n -= pivot + 1;
		}
	}
	return NULL;
}
