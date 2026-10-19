#include <sysmem/cpu.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include "update_packet.h"
#include <threadmgr/threadmgr.h>

int PupUtilVerifyPupSegmentMain(SceSize arglen, PupVerifyThreadArgs *args)
{
	SceKernelPAVector *dataVector, *vectorVector;
	UpdatePacketPayload *packet;
	PupSegment segment;
	void *buffer;
	SceUID fd, heapId;
	uint32_t serviceResult;
	uint64_t index;
	int result, ioFlags, needHeaderVerify, loadSm, residentSm;

	SceKernelSuspendForDriver_4DF40893(0);
	if (arglen != sizeof(*args)) {
		result = (int)0x800f0216;
		goto exit;
	}
	fd = args->fd;
	vectorVector = NULL;
	serviceResult = 0;
	dataVector = NULL;
	needHeaderVerify = args->need_header_verify;
	index = args->segment_index;
	residentSm = args->resident_sm;
	loadSm = args->load_sm;
	packet = NULL;
	buffer = NULL;
	heapId = UpdateUtilGetHeapId();
	ioFlags = SceThreadmgrForDriver_56C18B11();
	if (ioFlags >= 0)
		ioFlags = SceThreadmgrForDriver_84EB1EA4(ioFlags | 1);
	if (fd < 0)
		goto invalid;
	if (needHeaderVerify) {
		result = PupUtilVerifyPupHeader(fd, loadSm, residentSm);
		if (result)
			goto cleanup;
	}
	if (!pupFileBuffer || index >= pupFileHeader.m_segment_num)
		goto invalid;
	segment = ((PupSegment *)(pupFileBuffer + 1))[index];
	result = (uint32_t)segment.sign_algorithm;
	if (!result)
		goto cleanup;
	if (segment.size > 0x18000000)
		goto invalid;
	result = UpdateUtilCreatePacketPayloadWithBuffer((void **)&packet, &dataVector,
		&vectorVector, &buffer, segment.size);
	if (result)
		goto cleanup;
	packet->segment_id = segment.segment_id;
	result = UpdateUtilIoRead(fd, segment.offset, segment.size, buffer);
	if (result)
		goto cleanup;
	sceKernelDcacheCleanRange(buffer, ((uint32_t)segment.size + 63) & ~63);
	if (loadSm) {
		pupSmId = -1;
		result = UpdateUtilSetupSm(0, &pupSmId, 0);
		if (result)
			goto cleanup;
	}
	result = sceSblSmCommCallFunc(pupSmId, 0x20002, &serviceResult,
		packet, sizeof(*packet));
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
	if (!residentSm && pupSmId != -1)
		UpdateUtilStopSm(&pupSmId);
exit:
	SceKernelSuspendForDriver_2BB92967(0);
	return sceKernelExitThread(result);
invalid:
	result = (int)0x800f0216;
	goto cleanup;
}
