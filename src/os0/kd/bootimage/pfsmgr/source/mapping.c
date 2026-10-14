/*
 * The block request a thread reads through: a thread-local slot that PFS's
 * read checks before reading a file as usual.
 */

#include <sce/threadmgr.h>
#include "pfsmgr_internal.h"

SceUID g_pfsThreadMappingKey = -1;

int pfsThreadMappingInit(void)
{
	SceUID key;

	if (g_pfsThreadMappingKey != -1)
		return 0;
	key = SceThreadmgrForDriver_B645C7EF(sizeof(ScePfsBlockRequest *), 0, 0);
	if (key < 0)
		return key;
	g_pfsThreadMappingKey = key;
	return 0;
}

int pfsThreadMappingTerm(void)
{
	if (g_pfsThreadMappingKey != -1)
		return SceThreadmgrForDriver_8C3C1F78(g_pfsThreadMappingKey);
	return 0;
}

/* the calling thread's mapping slot */
ScePfsBlockRequest **pfsThreadMapping(void)
{
	return SceThreadmgrForDriver_3A72C6D8(g_pfsThreadMappingKey);
}
