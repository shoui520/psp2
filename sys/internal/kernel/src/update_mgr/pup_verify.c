#include <sm_comm/sm_comm.h>
#include "update_internal.h"

int PupUtilVerifyPup(SceUID fd)
{
	SceSblSmCommPair stopResult;
	uint64_t index;
	int result;

	UpdateUtilLockPupHeader();
	if (fd < 0) {
		UpdateUtilUnlockPupHeader();
		return (int)0x800f0216;
	}
	result = PupUtilVerifyPupHeader(fd, 1, 1);
	if (result)
		goto cleanup;
	index = 0;
	while (index < pupFileHeader.m_segment_num) {
		result = PupUtilVerifyPupSegment(fd, index, 0, 0, 1);
		if (result)
			goto cleanup;
		index++;
	}
	result = PupUtilVerifyPupWatermark(fd, 0, 0, 1);
	if (!result)
		result = PupUtilVerifyPupAdditionalSign(fd, 0, 0, 1);
cleanup:
	if (pupSmId != -1)
		UpdateUtilStopSmWithResult(&pupSmId, &stopResult);
	UpdateUtilUnlockPupHeader();
	return result;
}
