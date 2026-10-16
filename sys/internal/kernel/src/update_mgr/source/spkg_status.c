#include "update_internal.h"

int SpkgUtilGetStatus(int nodeType, uint32_t requestId, uint32_t *seq_no,
		uint32_t *result, uint32_t *progress, uint32_t *written_rates)
{
	UpdateUtilNodeStatus status;
	int error;

	if (!seq_no || !result)
		return (int)0x800f0216;
	if (!progress || !written_rates)
		return (int)0x800f0216;
	error = UpdateUtilGetNodeStatusLocked(nodeType, &status);
	if (error)
		return error;
	if (status.requestId != requestId && requestId)
		return (int)0x800f0202;
	*seq_no = status.seq_no;
	*progress = status.progress;
	*written_rates = status.written_rates;
	if (!requestId)
		*result = updateLastResult;
	else
		*result = status.result;
	return error;
}
