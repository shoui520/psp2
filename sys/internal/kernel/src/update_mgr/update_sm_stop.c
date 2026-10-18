#include <sm_comm/sm_comm.h>

int UpdateUtilStopSmWithResult(int *smId, SceSblSmCommPair *result)
{
	if (!result || !smId)
		return (int)0x800f0216;
	result->result = -1;
	result->additionalResult = -1;
	return SceSblSmCommForKernel_0631F8ED(*smId, result);
}
