#include "update_internal.h"
#include "snvs.h"

int SpkgInfoUtilTranslateParams(int packageType, int flags,
		uint32_t *purpose, uint32_t *bank);

int SpkgInfoUtilSetDigest(int packageType, int flags, const void *digest,
		SceSize size, int loadSm, int existingSmId)
{
	uint32_t purpose, bank;
	int result;

	result = SpkgInfoUtilTranslateParams(packageType, flags, &purpose, &bank);
	if (result == (int)0x800f0202)
		return result;
	if (result)
		return result;
	return SpkgInfoUtilSetSNVSValue(purpose, 1, bank, digest, size,
			loadSm, existingSmId);
}
