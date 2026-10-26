/*
 * _Close_all: at exit, every open stream closed, and the low-level output flushed.
 */

#include <yfuns.h>
#include "xstdio.h"

void _Close_all(void)
{
	FILE **p = _Files;
	int i;

	for (i = FOPEN_MAX; i != 0; i--, p++)
		if (*p != 0)
			fclose(*p);
	__dwrite(-1, 0, 0);
}
