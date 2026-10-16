#include <psp2kern/kernel/cpu/cache.h>
#include <psp2kern/kernel/sm_comm.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/sysmem.h>
#include "update_internal.h"

int SceThreadmgrForDriver_56C18B11(void);
int SceThreadmgrForDriver_84EB1EA4(int value);

int PupUtilVerifyPupWatermarkMain(SceSize arglen, PupVerifyThreadArgs *args)
{
	SceKernelPAVector *dataVector, *vectorVector;
	PupSegment *segments, *segment;
	PupSegmentSignature *signatures;
	PupHeaderDigest *headerDigest;
	SceUID heapId, fd;
	void *packet, *buffer;
	uint32_t serviceResult;
	uint64_t index;
	int result, ioFlags, loadSm, residentSm;

	SceKernelSuspendForDriver_4DF40893(0);
	if (arglen != sizeof(*args)) {
		result = (int)0x800f0216;
		goto exit;
	}
	fd = args->fd;
	serviceResult = 0;
	dataVector = NULL;
	vectorVector = NULL;
	loadSm = args->load_sm;
	residentSm = args->resident_sm;
	packet = NULL;
	buffer = NULL;
	heapId = UpdateUtilGetHeapId();
	ioFlags = SceThreadmgrForDriver_56C18B11();
	if (ioFlags >= 0)
		ioFlags = SceThreadmgrForDriver_84EB1EA4(ioFlags | 1);
	if (fd < 0) {
		result = (int)0x800f0216;
		goto cleanup;
	}
	if (!pupFileBuffer) {
		result = (int)0x800f0229;
		goto cleanup;
	}
	segments = (PupSegment *)(pupFileBuffer + 1);
	signatures = (PupSegmentSignature *)(segments + (uint32_t)pupFileHeader.m_segment_num);
	headerDigest = (PupHeaderDigest *)(signatures + (uint32_t)pupFileHeader.m_segment_num);
	result = UpdateUtilCreatePacketPayloadPupWMWithBuffer(&packet, &dataVector,
		&vectorVector, headerDigest, &buffer, 4096);
	if (result)
		goto cleanup;
	segment = segments;
	index = 0;
	while (index != pupFileHeader.m_segment_num) {
		if (segment->segment_id == 0x400)
			goto verify;
		segment++;
		index++;
	}
	result = (int)0x800f0202;
	goto cleanup;
verify:
	result = UpdateUtilIoRead(fd, segment->offset, 4096, buffer);
	if (result)
		goto cleanup;
	ksceKernelDcacheCleanRange(buffer, 4096);
	if (loadSm) {
		pupSmId = -1;
		result = UpdateUtilSetupSm(0, &pupSmId, 0);
		if (result)
			goto cleanup;
	}
	result = ksceSblSmCommCallFunc(pupSmId, 0x30002, &serviceResult, packet, 4032);
	if (!result)
		result = serviceResult;
cleanup:
	SceThreadmgrForDriver_84EB1EA4(ioFlags);
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	if (dataVector)
		ksceKernelFreeHeapMemory(heapId, dataVector);
	if (vectorVector)
		ksceKernelFreeHeapMemory(heapId, vectorVector);
	if (packet)
		ksceKernelFreeHeapMemory(heapId, packet);
	if (!residentSm && pupSmId != -1) {
		UpdateUtilStopSm(&pupSmId);
		pupSmId = -1;
	}
exit:
	SceKernelSuspendForDriver_2BB92967(0);
	return sceKernelExitThread(result);
}
