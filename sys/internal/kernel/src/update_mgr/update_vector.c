#include "update_internal.h"

int SceSblSsMgrForDriver_C38D0CEA(SceUID heapId, SceKernelPAVector *vector);

int UpdateUtilFreePARangeVector(SceUID heapId, SceKernelPAVector *vector)
{
	if (vector && vector->pRanges)
		return SceSblSsMgrForDriver_C38D0CEA(heapId, vector);
	return (int)0x800f0216;
}
