/* qsort function */
#include <stdlib.h>
#include <string.h>

#define ISORT_MAX	32	/* maximum count for insertion sort */
#define MAX_BUF		256	/* chunk to copy on swap */

/* swap two elements */
static void swap(char *qa, char *qb, size_t size)
{
	if (MAX_BUF / 4 <= size) {
		char buf[MAX_BUF];
		size_t m;

		for (; size != 0; qa += m, qb += m, size -= m) {
			m = size < MAX_BUF ? size : MAX_BUF;
			memcpy(buf, qa, m);
			memcpy(qa, qb, m);
			memcpy(qb, buf, m);
		}
	} else {
		for (; size != 0; --size) {
			char ch = *qa;

			*qa++ = *qb;
			*qb++ = ch;
		}
	}
}

/* sort three elements in place */
static __attribute__((noinline)) void med3(char *qa, char *qb, char *qc, size_t size, _Cmpfun *cmp)
{
	if ((*cmp)(qb, qa) < 0)
		swap(qa, qb, size);
	if ((*cmp)(qc, qb) < 0)
		swap(qb, qc, size);
	if ((*cmp)(qb, qa) < 0)
		swap(qa, qb, size);
}

/* rotate three elements: qa <- qc <- qb <- qa */
static __attribute__((noinline)) void rot3(char *qa, char *qb, char *qc, size_t size)
{
	if (MAX_BUF / 4 <= size) {
		char buf[MAX_BUF];
		size_t m;

		for (; size != 0; qa += m, qb += m, qc += m, size -= m) {
			m = size < MAX_BUF ? size : MAX_BUF;
			memcpy(buf, qa, m);
			memcpy(qa, qc, m);
			memcpy(qc, qb, m);
			memcpy(qb, buf, m);
		}
	} else {
		for (; size != 0; ++qb, --size) {
			char ch = *qa;

			*qa++ = *qc;
			*qc++ = *qb;
			*qb = ch;
		}
	}
}

/* percolate the hole at h down, then the element back up, in a heap of n elements */
static __attribute__((noinline)) void adjust_heap(char *qb, size_t h, size_t n, size_t size, _Cmpfun *cmp)
{
	size_t top = h;
	size_t k;
	char *qh = qb + h * size;
	char *qk;

	for (k = 2 * h + 2; k <= n; k = 2 * k + 2) {
		qk = qb + k * size;
		if (k == n || (*cmp)(qk, qk - size) < 0) {
			qk -= size;
			--k;
		}
		swap(qh, qk, size);
		h = k;
		qh = qk;
	}
	for (; top < h; h = k, qh = qk) {
		k = (h - 1) >> 1;
		qk = qb + k * size;
		if ((*cmp)(qh, qk) <= 0)
			break;
		swap(qk, qh, size);
	}
}

/* sort n elements: quicksort, heapsort past the ideal depth, insertion sort when small */
static void sort(char *qb, size_t n, size_t ideal, size_t size, _Cmpfun *cmp)
{
	for (; ideal != 0 && ISORT_MAX < n; ) {
		char *qe = qb + n * size;
		char *ql = qe - size;
		char *pfirst = qb + (n >> 1) * size;
		char *plast = pfirst + size;
		char *gfirst, *glast;
		size_t count = (ql - qb) / size;
		size_t nl, nr;

		if (40 < count) {	/* median of nine */
			size_t step = (count / 8 + 1) * size;

			med3(qb, qb + step, qb + 2 * step, size, cmp);
			med3(pfirst - step, pfirst, pfirst + step, size, cmp);
			med3(ql - 2 * step, ql - step, ql, size, cmp);
			med3(qb + step, pfirst, ql - step, size, cmp);
		} else {
			med3(qb, pfirst, ql, size, cmp);
		}

		/* partition: [qb, glast) < [pfirst, plast) == pivot < [gfirst, qe) */
		for (; qb < pfirst; pfirst -= size)
			if ((*cmp)(pfirst - size, pfirst) != 0)
				break;
		for (; plast < qe; plast += size)
			if ((*cmp)(plast, pfirst) != 0)
				break;
		gfirst = plast;
		glast = pfirst;
		for (;;) {
			int c;

			for (; gfirst < qe; gfirst += size) {
				if ((c = (*cmp)(pfirst, gfirst)) < 0)
					;
				else if (0 < c)
					break;
				else {
					swap(plast, gfirst, size);
					plast += size;
				}
			}
			for (; qb < glast; glast -= size) {
				if ((c = (*cmp)(glast - size, pfirst)) < 0)
					;
				else if (0 < c)
					break;
				else {
					pfirst -= size;
					swap(pfirst, glast - size, size);
				}
			}
			if (glast == qb && gfirst == qe)
				break;
			if (glast == qb) {
				if (plast == gfirst)
					swap(gfirst, pfirst, size);
				else
					rot3(gfirst, pfirst, plast, size);
				gfirst += size;
				plast += size;
				pfirst += size;
			} else if (gfirst == qe) {
				pfirst -= size;
				glast -= size;
				plast -= size;
				if (glast == pfirst)
					swap(pfirst, plast, size);
				else
					rot3(glast, plast, pfirst, size);
			} else {
				glast -= size;
				swap(gfirst, glast, size);
				gfirst += size;
			}
		}

		/* sort the smaller partition, loop on the larger */
		ideal = (ideal >> 1) + (ideal >> 2);	/* allow 1.5 log2(n) divisions */
		nl = (pfirst - qb) / size;
		nr = (qe - plast) / size;
		if (nr < nl) {
			sort(plast, nr, ideal, size, cmp);
			n = nl;
		} else {
			n = nr;
			sort(qb, nl, ideal, size, cmp);
			qb = plast;
		}
	}

	if (n <= ISORT_MAX) {	/* insertion sort */
		char *qm = qb;

		if (1 < n) {
			for (; --n != 0; ) {
				size_t bytes = qm - qb + 2 * size;
				size_t j, m;

				qm += size;
				if ((*cmp)(qm, qb) < 0) {	/* move to the front */
					char buf[MAX_BUF];

					for (j = size; j != 0; j -= m) {
						m = j < MAX_BUF ? j : MAX_BUF;
						memcpy(buf, qm + size - m, m);
						memmove(qb + m, qb, bytes - m);
						memcpy(qb, buf, m);
					}
				} else {	/* find the hole, then rotate it in */
					char *qh = qm;
					char *ql;

					for (ql = qm - size; (*cmp)(qm, ql) < 0; ql -= size)
						qh = ql;
					if (qh != qm) {
						char buf[MAX_BUF];

						bytes = qm - qh + size;
						for (j = size; j != 0; j -= m) {
							m = j < MAX_BUF ? j : MAX_BUF;
							memcpy(buf, qm + size - m, m);
							memmove(qh + m, qh, bytes - m);
							memcpy(qh, buf, m);
						}
					}
				}
			}
		}
	} else {	/* heap sort */
		char *qe;
		size_t h;

		for (h = n >> 1; h != 0; )
			adjust_heap(qb, --h, n, size, cmp);
		for (qe = qb + n * size; 1 < n; ) {
			qe -= size;
			swap(qb, qe, size);
			adjust_heap(qb, 0, --n, size, cmp);
		}
	}
}

void qsort(void *base, size_t nelem, size_t size, _Cmpfun *cmp)
{
	if (base != NULL && cmp != NULL)
		sort((char *)base, nelem, nelem, size, cmp);
}
