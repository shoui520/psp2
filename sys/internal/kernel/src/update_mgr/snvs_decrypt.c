#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "snvs.h"

int SpkgInfoUtilSNVSDecryptSectors(int sectorIndex, const SpkgSnvsSector *input,
		SpkgSnvsSector *output, int loadSm, int existingSmId)
{
	SpkgSnvsSectorsRequest request;
	unsigned int serviceResult;
	int smId = -1;
	int result;

	if (!sectorIndex || (unsigned int)sectorIndex + 1 > 31) {
		result = (int)0x800f0216;
		goto finish;
	}
	request.sectorIndex = sectorIndex;
	request.sectorCount = 1;
	request.sectors[0] = *input;
	if (loadSm) {
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result)
			goto cleanup;
	} else {
		smId = existingSmId;
	}
	result = SceSblSmCommForKernel_DB9FC204(smId, 0x70002, &serviceResult, &request, 40);
	if (result)
		result = serviceResult;
	else
		*output = request.sectors[0];
finish:
	if (!loadSm)
		return result;
cleanup:
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
