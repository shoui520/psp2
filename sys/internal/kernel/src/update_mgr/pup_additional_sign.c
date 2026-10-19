#include <sysmem/cpu.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <threadmgr/threadmgr.h>

int PupUtilVerifyPupAdditionalSignMain(SceSize arglen, PupVerifyThreadArgs *args)
{
	SceKernelPAVector *dataVector, *vectorVector;
	PupSegment *segments, *segment;
	PupSegmentSignature *signatures;
	PupHeaderDigest *headerDigest;
	SceUID heapId, fd;
	void *packet, *buffer;
	uint32_t serviceResult;
	uint64_t index, count;
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
	result = UpdateUtilCreatePacketPayloadPupASWithBuffer(&packet, &dataVector,
		&vectorVector, headerDigest, &buffer, 1024);
	if (result)
		goto cleanup;
	count = pupFileHeader.m_segment_num;
	if (!count)
		goto notFound;
	segments = (PupSegment *)(pupFileBuffer + 1);
	segment = segments;
	index = 0;
	while (index != count) {
		if (segment->segment_id == 0x401)
			goto verify;
		segment++;
		index++;
	}
notFound:
	result = (int)0x800f0202;
	goto cleanup;
verify:
	result = UpdateUtilIoRead(fd, segment->offset, 1024, buffer);
	if (result)
		goto cleanup;
	sceKernelDcacheCleanRange(buffer, 1024);
	if (loadSm) {
		pupSmId = -1;
		result = UpdateUtilSetupSm(0, &pupSmId, 0);
		if (result)
			goto cleanup;
	}
	result = SceSblSmCommForKernel_DB9FC204(pupSmId, 0xa0002, &serviceResult, packet, 4032);
	if (!result)
		result = serviceResult;
cleanup:
	SceThreadmgrForDriver_84EB1EA4(ioFlags);
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	if (dataVector)
		sceKernelFreeHeapMemory(heapId, dataVector);
	if (vectorVector)
		sceKernelFreeHeapMemory(heapId, vectorVector);
	if (packet)
		sceKernelFreeHeapMemory(heapId, packet);
	if (!residentSm && pupSmId != -1) {
		UpdateUtilStopSm(&pupSmId);
		pupSmId = -1;
	}
exit:
	SceKernelSuspendForDriver_2BB92967(0);
	return sceKernelExitThread(result);
}
