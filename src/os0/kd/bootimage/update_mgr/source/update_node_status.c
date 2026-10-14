#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "update_objects.h"

int UpdateUtilInitializeNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status)
{
	SceSblUpdateMgrNode *node;
	uint32_t requestId, sequence, progress, writtenRates;
	int result;
	void *buffer;

	if (nodeType - 1 > 2 || !status)
		return (int)0x800f0216;
	node = &updateNodes[nodeType];
	ksceKernelLockFastMutex((SceKernelFastMutex *)&node->mutex);
	result = status->result;
	progress = status->progress;
	requestId = status->requestId;
	sequence = status->seq_no;
	node->status.progress = progress;
	writtenRates = status->written_rates;
	buffer = status->buffer;
	node->initialized = 1;
	node->status.written_rates = writtenRates;
	node->status.requestId = requestId;
	node->status.seq_no = sequence;
	node->status.result = result;
	node->status.buffer = buffer;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&node->mutex);
	return 0;
}

int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status)
{
	SceSblUpdateMgrNode *node;

	if (nodeType - 1 > 2 || !status)
		return (int)0x800f0216;
	node = &updateNodes[nodeType];
	ksceKernelLockFastMutex((SceKernelFastMutex *)&node->mutex);
	if (node->status.requestId != status->requestId || !node->initialized) {
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&node->mutex);
		return (int)0x800f0216;
	}
	node->status.seq_no = status->seq_no;
	node->status.result = status->result;
	node->status.progress = status->progress;
	node->status.written_rates = status->written_rates;
	node->status.buffer = status->buffer;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&node->mutex);
	return 0;
}
