#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "snvs.h"

extern uint8_t updateSnvsFailureStage;
int SceSysconForDriver_EBDF88B9(const void *input, unsigned int inputSize,
		void *output, unsigned int outputSize);

int SpkgInfoUtilReadSNVSSector(int sectorIndex, SpkgSnvsSector *sector,
		int loadSm, int existingSmId)
{
	SpkgSnvsSectorRequest request;
	unsigned int serviceResult;
	int smId = -1;
	int result;

	updateSnvsFailureStage = 0;
	if ((unsigned int)(sectorIndex - 1) > 30) {
		result = (int)0x800f0216;
		goto finish;
	}
	request.mode = 0;
	request.sectorIndex = sectorIndex;
	if (loadSm) {
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result) {
			updateSnvsFailureStage = 0;
			goto cleanup;
		}
	} else {
		smId = existingSmId;
	}
	result = ksceSblSmCommCallFunc(smId, 0xb0002, &serviceResult,
			&request, sizeof(request));
	if (result) {
		updateSnvsFailureStage = 1;
		goto finish;
	}
	result = serviceResult;
	if (result) {
		updateSnvsFailureStage = 2;
		goto finish;
	}
	result = SceSysconForDriver_EBDF88B9(request.intermediate, 16, request.final, 48);
	if (result) {
		updateSnvsFailureStage = 3;
		goto finish;
	}
	request.mode = 1;
	result = ksceSblSmCommCallFunc(smId, 0xb0002, &serviceResult,
			&request, sizeof(request));
	if (result) {
		updateSnvsFailureStage = 4;
		goto finish;
	}
	result = serviceResult;
	if (result) {
		updateSnvsFailureStage = 5;
		goto finish;
	}
	*sector = request.sector;
finish:
	if (!loadSm)
		return result;
cleanup:
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
