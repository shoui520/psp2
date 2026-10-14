#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/cpu/cache.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sce/threadmgr.h>
#include "update_internal.h"

int SpkgUtilSpackageTopHalf(unsigned int nodeType, int packageType,
		void *buffer, SceSize size, int flags, uint32_t *requestId);

int SpkgUtilUpdateSpackageSync(SceUID fd, const SceMbr *mbr, int nodeType,
		int packageType, const PupSegment *segment, int flags)
{
	void *buffer = NULL;
	uint32_t requestId = 0, seq_no = 0, nodeResult = 0;
	uint32_t progress = 0, written_rates = 0;
	int result, size;

	if ((fd < 0) | !mbr)
		return (int)0x800f0216;
	if (!segment || segment->size > 0x18000000 || segment->offset > 0x18000000)
		return (int)0x800f0216;
	result = UpdateUtilAllocateBuffer(segment->size, &buffer, sceKernelGetProcessId());
	if (result)
		goto release;
	if (ksceIoLseek(fd, segment->offset, 0) < 0) {
		result = (int)0x800f0205;
		goto release;
	}
	size = ksceIoRead(fd, buffer, segment->size);
	if ((uint32_t)size != (uint32_t)segment->size) {
		result = (int)0x800f0205;
		goto release;
	}
	ksceKernelDcacheCleanRange(buffer, (size + 63) & ~63);
	result = SpkgUtilSpackageTopHalf(1, packageType, buffer,
			segment->size, flags, &requestId);
	if (result)
		goto release;
	for (;;) {
		result = SpkgUtilGetStatus(1, requestId, &seq_no, &nodeResult,
				&progress, &written_rates);
		if (result)
			break;
		if (progress == 5) {
			result = nodeResult;
			break;
		}
		sceKernelDelayThread(100000);
	}
release:
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	return result;
}
