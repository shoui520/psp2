/* bsearch_s function */
#include <stdint.h>
#include <stdlib.h>

void *bsearch_s(const void *key, const void *base, rsize_t nelem, rsize_t size,
		_Cmpfun_s *cmp, void *context)
{
	const char *p;
	size_t n;

	if (RSIZE_MAX < nelem || RSIZE_MAX < size
	    || (nelem != 0 && (key == NULL || base == NULL || cmp == NULL))) {
		_EXT1_FAIL("bsearch_s: bad argument");
		return NULL;
	}
	for (p = (const char *)base, n = nelem; n > 0;) {
		const size_t pivot = n >> 1;
		const char *const q = p + size * pivot;
		const int val = (*cmp)(key, q, context);

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
