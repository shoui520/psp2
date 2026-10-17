#include "update_internal.h"
#include "snvs.h"
#include <sm_comm/sm_comm.h>

int ksceSysconNvsReadData(unsigned int offset, void *buffer, SceSize size);

int SpkgInfoUtilGetNVSFlagStatus(uint32_t *status, uint32_t *flags)
{
	SpkgSnvsMgmtRequest request;
	SpkgSnvsSector sector;
	unsigned int serviceResult;
	int smId = -1;
	int tries = 3;
	int result;

	do {
		if (!ksceSysconNvsReadData(0, sector.words, 16) &&
		    !ksceSysconNvsReadData(16, &sector.words[4], 16))
			break;
	} while (--tries);
	request.sector = sector;
	result = UpdateUtilSetupSm(0, &smId, 0);
	if (!result) {
		result = ksceSblSmCommCallFunc(smId, 0x90002, &serviceResult,
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
