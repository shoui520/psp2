#include "update_internal.h"

int SpkgUtilSpackageTopHalf(unsigned int nodeType, int packageType,
		void *buffer, SceSize size, int flags, uint32_t *requestId);

int SpkgUtilUpdateSpackage(int packageType, void *buffer, SceSize size,
		uint32_t flags, uint32_t *requestId)
{
	return SpkgUtilSpackageTopHalf(1, packageType, buffer, size, flags, requestId);
}

int SpkgUtilInspectSpackage(int packageType, void *buffer, SceSize size,
		uint32_t flags, uint32_t *requestId)
{
	if (!updateInitialized)
		return (int)0x800f0229;
	return SpkgUtilSpackageTopHalf(2, packageType, buffer, size, flags, requestId);
}

int SpkgUtilExtractSpackage(int packageType, void *buffer, SceSize size,
		uint32_t flags, uint32_t *requestId)
{
	return SpkgUtilSpackageTopHalf(3, packageType, buffer, size, flags, requestId);
}
