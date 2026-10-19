#include <ss_mgr/ss_mgr.h>
#include "update_internal.h"
#include "spkg_hardware.h"
#include <sysmem/dipsw.h>
#include <sysmem/sblaimgr.h>

int SpkgInfoUtilGetHardwareIndex(int *index)
{
	ScePsCode pscode;
	unsigned int entry;
	int result;
	int hardwareIndex;

	if (!index)
		return (int)0x800f0216;
	result = SceSblSsMgrForDriver_E0DC2587(&pscode);
	if (result)
		return result;
	if (SceSblAIMgrForDriver_6D5A3FC9()) {
		hardwareIndex = 1;
	} else {
		hardwareIndex = 0;
		for (entry = 0; entry < 89; entry++) {
			if (spkgHardwareIndices[entry].minimumProductCode <= pscode.product_code &&
			    spkgHardwareIndices[entry].maximumProductCode >= pscode.product_code &&
			    spkgHardwareIndices[entry].productSubCode == pscode.product_sub_code) {
				hardwareIndex = spkgHardwareIndices[entry].hardwareIndex;
				break;
			}
		}
	}
	*index = hardwareIndex;
	return 0;
}

uint32_t SpkgInfoUtilGetHardwareSupportMask(unsigned int index)
{
	if (index > 89)
		return 0;
	return spkgHardwarePolicies[index].supportMask;
}

int SpkgInfoUtilIsBelowMinimumVersion(int packageType, uint32_t version,
		uint32_t currentVersion)
{
	int index = 0;
	uint32_t minimumVersion;

	if (packageType != 1 && packageType != 9)
		return 0;
	if (SpkgInfoUtilGetHardwareIndex(&index))
		return 1;
	minimumVersion = spkgHardwarePolicies[index].minimumVersion;
	if (minimumVersion == UINT32_MAX) {
		minimumVersion = currentVersion;
	} else if (SceSblAIMgrForDriver_963CA644() && SceSblAIMgrForDriver_274663A0() &&
		   sceKernelCheckDipsw(DIPSW_PLATFORM_DOLCE) && minimumVersion < 0x03000000) {
		minimumVersion = 0x03000000;
	}
	return minimumVersion > version;
}
