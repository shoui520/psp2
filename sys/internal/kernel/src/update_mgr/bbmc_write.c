#include <threadmgr/threadmgr.h>
#include "bbmc_update.h"

int BbmcUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
	uint64_t chunkIndex, uint64_t chunkCount, unsigned int nodeType,
	UpdateUtilNodeStatus *status)
{
	BbmcUpdatePackage package;
	unsigned int pattern = 0;
	UpdateUtilNodeStatus *nodeStatus;
	unsigned int complete;
	int result;

	if ((!size | !buffer) || ((chunkCount < chunkIndex) | !status)) {
		result = (int)0x800f0216;
		goto out;
	}
	UpdateUtilLockCom();
	if (!updateBbmcInitialized) {
		result = BbmcUtilInitialize();
		if (result) {
			UpdateUtilUnlockCom();
			goto out;
		}
	}
	UpdateUtilUnlockCom();
	if (!updateBbmcContext.chunkCount)
		updateBbmcContext.chunkCount = chunkCount;
	if (updateBbmcContext.chunkIndex + 1 != chunkIndex) {
		result = (int)0x800f0229;
		goto out;
	}
	updateBbmcContext.dataSize = size;
	complete = updateBbmcContext.complete;
	updateBbmcContext.chunkIndex = chunkIndex;
	updateBbmcContext.buffer = buffer;
	updateBbmcContext.nodeType = nodeType;
	updateBbmcContext.status = status;
	if (complete) {
		result = 0;
		goto out;
	}
	if (chunkIndex != 1) {
		updateBbmcContext.transferDone = 0;
		package.chunkCount = updateBbmcContext.chunkCount;
		package.chunkIndex = (uint32_t)chunkIndex - 1;
		package.buffer = (void *)buffer;
		package.size = size;
		result = SceBbmcForDriver_94A31A9C(
			updateBbmcContext.subscriber.subscriberId, &package);
	} else {
		updateBbmcContext.transferDone = 0;
		package.chunkCount = updateBbmcContext.chunkCount;
		package.chunkIndex = 0;
		package.buffer = (void *)buffer;
		package.size = size;
		result = SceBbmcForDriver_ACB0EC17(
			updateBbmcContext.subscriber.subscriberId, &package);
	}
	if (result)
		goto out;
	nodeStatus = updateBbmcContext.status;
	if (nodeStatus) {
		unsigned int sequence = nodeStatus->seq_no;

		nodeStatus->progress = 3;
		nodeStatus->written_rates = 0;
		nodeStatus->seq_no = sequence + 1;
		UpdateUtilUpdateNodeStatus(updateBbmcContext.nodeType, nodeStatus);
	}
	if (SceThreadmgrForDriver_8A35F714(updateBbmcContext.resultEventFlagId,
		3, 5, &pattern, NULL)) {
		result = (int)0x800f0216;
		goto out;
	}
	if (updateBbmcContext.shutdown)
		updateBbmcContext.complete = 1;
	result = pattern & 2 ? (int)0x800f0205 : 0;
out:
	updateBbmcContext.nodeType = 0;
	updateBbmcContext.status = NULL;
	return result;
}
