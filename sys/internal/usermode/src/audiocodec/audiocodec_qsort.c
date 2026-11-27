/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * BSD Bentley & McIlroy qsort (statically linked from libc.a).
 */
#include "audiocodec_internal.h"

static __attribute__((noinline)) void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
	if (swaptype <= 1) {
		size_t i = n / sizeof(long);
		long *pi = (long *)a;
		long *pj = (long *)b;
		do {
			long t = *pi;
			*pi++ = *pj;
			*pj++ = t;
		} while ((int)--i > 0);
	} else {
		int i = 0;
		do {
			char t = a[i];
			a[i] = b[i];
			b[i] = t;
			++i;
		} while ((int)(n - (size_t)i) > 0);
	}
}

static __attribute__((noinline)) char *
med3(char *a, char *b, char *c, int (*cmp)(const void *, const void *))
{
	return cmp(a, b) < 0 ?
		(cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a)) :
		(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c));
}

static inline size_t minSize(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

void audioQsort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *))
{
	for (;;) {
		char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
		size_t d;
		int swaptype, swapCnt;
		int r;

		swaptype = ((((uintptr_t)a | es) & (sizeof(long) - 1U)) != 0)
			? 2 : ((es == sizeof(long)) ? 0 : 1);
		swapCnt = 0;

		if ((int)n < 7) {
			for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es) {
				for (pl = pm; pl > (char *)a && cmp(pl - es, pl) > 0; pl -= es)
					swapfunc(pl, pl - es, es, swaptype);
			}
			return;
		}

		pm = (char *)a + ((int)n >> 1) * es;
		if ((int)n > 7) {
			pl = (char *)a;
			pn = (char *)a + (n - 1U) * es;
			if ((int)n > 40) {
				d = ((size_t)((int)n >> 3)) * es;
				pl = med3(pl, pl + d, pl + 2U * d, cmp);
				pm = med3(pm - d, pm, pm + d, cmp);
				pn = med3(pn - 2U * d, pn - d, pn, cmp);
			}
			pm = med3(pl, pm, pn, cmp);
		}
		swapfunc((char *)a, pm, es, swaptype);
		pa = pb = (char *)a + es;
		pc = pd = (char *)a + (n - 1U) * es;

		for (;;) {
			while (pb <= pc && (r = cmp(pb, a)) <= 0) {
				if (r == 0) {
					swapCnt = 1;
					swapfunc(pa, pb, es, swaptype);
					pa += es;
				}
				pb += es;
			}
			while (pb <= pc && (r = cmp(pc, a)) >= 0) {
				if (r == 0) {
					swapCnt = 1;
					swapfunc(pc, pd, es, swaptype);
					pd -= es;
				}
				pc -= es;
			}
			if (pb > pc)
				break;
			swapfunc(pb, pc, es, swaptype);
			swapCnt = 1;
			pb += es;
			pc -= es;
		}

		if (swapCnt == 0) {
			for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es) {
				for (pl = pm; pl > (char *)a && cmp(pl - es, pl) > 0; pl -= es)
					swapfunc(pl, pl - es, es, swaptype);
			}
			return;
		}

		pn = (char *)a + n * es;
		r = (int)minSize((size_t)(pa - (char *)a), (size_t)(pb - pa));
		if (r > 0)
			swapfunc((char *)a, pb - r, (size_t)r, swaptype);
		r = (int)minSize((size_t)(pd - pc), (size_t)(pn - pd - (ptrdiff_t)es));
		if (r > 0)
			swapfunc(pb, pn - r, (size_t)r, swaptype);

		r = (int)(pb - pa);
		if (r > (int)es)
			audioQsort(a, (size_t)((int)r / (int)es), es, cmp);

		r = (int)(pd - pc);
		if (r > (int)es) {
			a = pn - r;
			n = (size_t)((int)r / (int)es);
			continue;
		}
		return;
	}
}
