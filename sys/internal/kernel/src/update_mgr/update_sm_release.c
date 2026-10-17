#include <sm_comm/sm_comm.h>
#include "update_internal.h"

int UpdateUtilStopSmWithResult(int *smId, SceSblSmCommPair *result);

int UpdateUtilStopSm(int *smId)
{
	SceSblSmCommPair stopResult;
	int result;

	if (!smId || *smId == -1)
		return (int)0x800f0216;
	result = UpdateUtilStopSmWithResult(smId, &stopResult);
	*smId = -1;
	return result;
}
