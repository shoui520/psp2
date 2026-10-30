/*
 * BSD's qsort, built for DTrace (Sony's dtrace/bsd/qsort.c).
 */
#pragma once

#include <stddef.h>

void qsort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *));
