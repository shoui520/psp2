#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "snvs.h"
#include <syscon/syscon.h>

extern uint8_t updateSnvsFailureStage;

int SpkgInfoUtilWriteSNVSSector(int sectorIndex, const SpkgSnvsSector *sector,
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
	request.mode = 2;
	request.sectorIndex = sectorIndex;
	request.sector = *sector;
	if (loadSm) {
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result) {
			updateSnvsFailureStage = 8;
			goto cleanup;
		}
	} else {
		smId = existingSmId;
	}
	result = SceSblSmCommForKernel_DB9FC204(smId, 0xb0002, &serviceResult,
			&request, sizeof(request));
	if (result) {
		updateSnvsFailureStage = 9;
		goto finish;
	}
	result = serviceResult;
	if (result) {
		updateSnvsFailureStage = 10;
		goto finish;
	}
	result = SceSysconForDriver_63683B9B(request.intermediate, 48, request.final, 16);
	if (result) {
		updateSnvsFailureStage = 11;
		goto finish;
	}
	request.mode = 3;
	result = SceSblSmCommForKernel_DB9FC204(smId, 0xb0002, &serviceResult,
			&request, sizeof(request));
	if (result) {
		updateSnvsFailureStage = 12;
		goto finish;
	}
	result = serviceResult;
	if (result)
		updateSnvsFailureStage = 13;
finish:
	if (!loadSm)
		return result;
cleanup:
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
