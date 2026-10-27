/*
 * What every PFS mount shares: the mapping slot, the crypt buffers and the
 * crypt engine, and a random number generator.
 */

#include <sysmem/sysmem.h>
#include <sysmem/utils.h>
#include "pfsmgr_internal.h"

static SceMt19937Context g_pfsRandom;

int pfsGlobalInit(void)
{
	int ret;

	ret = pfsThreadMappingInit();
	if (ret != 0)
		return ret;
	ret = pfsCryptBufferInit();
	if (ret == 0) {
		ret = pfsCryptEngineInit();
		if (ret == 0)
			return 0;
		pfsCryptBufferTerm();
	}
	pfsThreadMappingTerm();
	return ret;
}

void pfsGlobalTerm(void)
{
	pfsCryptEngineTerm();
	pfsCryptBufferTerm();
	pfsThreadMappingTerm();
}

void pfsRandomInit(SceUInt32 seed)
{
	sceMt19937Init(&g_pfsRandom, seed);
}

SceUInt32 pfsRandom(void)
{
	return sceMt19937UInt(&g_pfsRandom);
}
