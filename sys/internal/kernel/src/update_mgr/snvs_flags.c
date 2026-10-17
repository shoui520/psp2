#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "snvs.h"

int SceSysconForDriver_EBDF88B9(const void *input, unsigned int inputSize,
		void *output, unsigned int outputSize);

int SpkgInfoUtilGetSNVSFlagStatus(uint32_t *status, uint32_t *flags,
		int loadSm, int existingSmId)
{
	SpkgSnvsMgmtSectorRequest request;
	unsigned int serviceResult;
	int smId = -1;
	int result;

	request.mode = 0;
	request.sectorCount = 0;
	if (loadSm) {
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result)
			goto cleanup;
	} else {
		smId = existingSmId;
	}
	result = ksceSblSmCommCallFunc(smId, 0xc0002, &serviceResult,
			&request, sizeof(request));
	if (result)
		goto finish;
	result = serviceResult;
	if (result)
		goto finish;
	result = SceSysconForDriver_EBDF88B9(request.intermediate, 16,
			request.final, 48);
	if (result)
		goto finish;
	request.mode = 1;
	result = ksceSblSmCommCallFunc(smId, 0xc0002, &serviceResult,
			&request, sizeof(request));
	if (result)
		goto finish;
	result = serviceResult;
	if (result)
		goto finish;
	*status = request.status;
	*flags = request.flags;
finish:
	if (!loadSm)
		return result;
cleanup:
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
