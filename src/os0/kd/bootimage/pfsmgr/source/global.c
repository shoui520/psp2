/*
 * What every PFS mount shares: the mapping slot, the crypt buffers and the
 * crypt engine, and a random number generator.
 */

#include <sce/sysmem.h>
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
	ksceMt19937Init(&g_pfsRandom, seed);
}

SceUInt32 pfsRandom(void)
{
	return ksceMt19937UInt(&g_pfsRandom);
}
