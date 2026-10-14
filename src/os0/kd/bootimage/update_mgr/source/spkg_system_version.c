#include "update_internal.h"

extern unsigned int spkgSystemSwVersion;

int SpkgInfoUtilInitSystemSwVersion(void)
{
	SceSblUsSpkgInfo info;
	int result;

	result = SpkgInfoUtilGetSpkgInfo(1, &info);
	if (result)
		spkgSystemSwVersion = FW_VERSION & 0xffff0000u;
	else
		spkgSystemSwVersion = info.version;
	return result;
}

int SpkgInfoUtilGetSystemSwVersion(void)
{
	return spkgSystemSwVersion;
}
