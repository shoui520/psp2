#ifndef SCE_QAF_CACHE_H
#define SCE_QAF_CACHE_H

#include <sce/qafmgr.h>

/* Name unknown: the Pamgr/dtracep permission gate caches an unset/allowed/denied
 * state as 0/1/2 and returns 0 for permission, -1 otherwise. */
static inline __attribute__((always_inline)) int
QafCheckCachedDebugPermission(int *cache)
{
	int state = *cache;

	if (state == 0) {
		state = SceQafMgrForDriver_382C71E8() ? 1 : 2;
		*cache = state;
	}
	return state == 1 ? 0 : -1;
}

#endif
