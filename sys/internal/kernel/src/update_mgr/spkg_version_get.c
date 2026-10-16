#include "update_internal.h"
#include "snvs.h"

int SpkgInfoUtilTranslateParams(int packageType, int flags,
		uint32_t *purpose, uint32_t *bank);

int SpkgInfoUtilGetVersion(int packageType, int flags, unsigned int *version,
		int loadSm, int existingSmId)
{
	unsigned int value = 0;
	uint32_t purpose, bank;
	int result;

	if (!version)
		return (int)0x800f0216;
	result = SpkgInfoUtilTranslateParams(packageType, flags, &purpose, &bank);
	if (result == (int)0x800f0202)
		return result;
	if (result)
		return result;
	result = SpkgInfoUtilGetSNVSValue(purpose, 0, bank, &value, sizeof(value),
			loadSm, existingSmId);
	if (result)
		return result;
	if (value != 0xdeadbeef) {
		*version = value;
		return result;
	}
	if ((unsigned int)packageType - 1 <= 20) {
		if (((1u << ((unsigned int)packageType - 1)) & 0x100101) != 0)
			result = FW_VERSION & 0xffff0000u;
		else
			result = 0;
	}
	*version = result;
	return 0;
}
