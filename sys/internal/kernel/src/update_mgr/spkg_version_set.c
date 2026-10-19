#include "update_internal.h"
#include "snvs.h"
#include "spkg.h"

int SpkgInfoUtilSetVersion(int packageType, int flags, unsigned int version,
		int loadSm, int existingSmId)
{
	unsigned int value = version;
	uint32_t purpose, bank;
	int result;

	result = SpkgInfoUtilTranslateParams(packageType, flags, &purpose, &bank);
	if (result == (int)0x800f0202)
		return result;
	if (result)
		return result;
	return SpkgInfoUtilSetSNVSValue(purpose, 0, bank, &value, sizeof(value),
			loadSm, existingSmId);
}
