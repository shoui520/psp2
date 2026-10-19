#include "update_internal.h"
#include "snvs.h"
#include <sm_comm/sm_comm.h>
#include <syscon/syscon.h>

int SpkgInfoUtilGetNVSFlagStatus(uint32_t *status, uint32_t *flags)
{
	SpkgSnvsMgmtRequest request;
	SpkgSnvsSector sector;
	unsigned int serviceResult;
	int smId = -1;
	int tries = 3;
	int result;

	do {
		if (!SceSysconForDriver_ACAFA2B8(0, sector.words, 16) &&
		    !SceSysconForDriver_ACAFA2B8(16, &sector.words[4], 16))
			break;
	} while (--tries);
	request.sector = sector;
	result = UpdateUtilSetupSm(0, &smId, 0);
	if (!result) {
		result = sceSblSmCommCallFunc(smId, 0x90002, &serviceResult,
				&request, sizeof(request));
		if (result)
			result = serviceResult;
		else {
			*status = request.status;
			*flags = request.flags;
		}
	}
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
