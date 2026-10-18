#include "update_internal.h"
#include <ss_mgr/ss_mgr.h>

int UpdateUtilFreePARangeVector(SceUID heapId, SceKernelPAVector *vector)
{
	if (vector && vector->pRanges)
		return SceSblSsMgrForDriver_C38D0CEA(heapId, vector);
	return (int)0x800f0216;
}
