#include "update_internal.h"

int CpUtilGetBoardVersion(uint32_t *version)
{
	if (!updateCpupConnected)
		return (int)0x800f0202;
	*version = updateCpupBoardVersion;
	return 0;
}
