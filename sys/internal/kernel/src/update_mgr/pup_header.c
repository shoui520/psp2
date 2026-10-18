#include <iofilemgr/iofilemgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/cpu.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/qafmgr.h>
#include <post_ss_mgr/sblpostssmgr.h>

int SceSblSsMgrForKernel_516ECC08(unsigned char *flag);

static const char pupMagic[8] = "SCEUF";
static const uint32_t pupFallbackVersion = SCE_PSP2_SDK_VERSION;

int PupUtilVerifyPupHeaderAsync(SceSize arglen, PupVerifyThreadArgs *args)
{
	SceKernelPAVector *dataVector, *vectorVector;
	void *packet;
	SceUID fd, heapId;
	uint32_t serviceResult, version, platform;
	unsigned char updateFlag;
	int result, ioFlags, loadSm, residentSm, isDex, attributes, hardwareIndex;

	if (arglen != sizeof(*args)) {
		result = (int)0x800f0216;
		goto exit;
	}
	fd = args->fd;
	serviceResult = 0;
	dataVector = NULL;
	loadSm = args->load_sm;
	residentSm = args->resident_sm;
	vectorVector = NULL;
	packet = NULL;
	version = 0;
	heapId = UpdateUtilGetHeapId();
	updateFlag = 0;
	ioFlags = SceThreadmgrForDriver_56C18B11();
	if (ioFlags >= 0)
		ioFlags = SceThreadmgrForDriver_84EB1EA4(ioFlags | 1);
	if (fd < 0) {
		result = (int)0x800f0216;
		goto cleanup;
	}
	if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0)
		goto readError;
	result = UpdateUtilIoRead(fd, 0, sizeof(pupFileHeader), &pupFileHeader);
	if (result)
		goto cleanup;
	if (timingsafe_memcmp(&pupFileHeader, pupMagic, 7) ||
	    pupFileHeader.m_segment_num >= 161) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	pupFileBufferSize = (sizeof(PupSegment) + sizeof(PupSegmentSignature)) *
		(uint32_t)pupFileHeader.m_segment_num + sizeof(PupHeader) + sizeof(PupHeaderDigest);
	if (pupFileBuffer) {
		UpdateUtilReleaseBuffer(pupFileBuffer, sceKernelGetProcessId());
		pupFileBuffer = NULL;
	}
	result = UpdateUtilCreatePacketPayloadWithBuffer(&packet, &dataVector,
		&vectorVector, (void **)&pupFileBuffer, pupFileBufferSize);
	if (result)
		goto cleanup;
	if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0)
		goto readError;
	result = UpdateUtilIoRead(fd, 0, pupFileBufferSize, pupFileBuffer);
	if (result)
		goto cleanup;
	SceCpuForDriver_103872A5(pupFileBuffer, (pupFileBufferSize + 63) & ~63);
	if (loadSm) {
		pupSmId = -1;
		result = UpdateUtilSetupSm(0, &pupSmId, 0);
		if (result)
			goto cleanup;
	}
	result = SceSblSmCommForKernel_DB9FC204(pupSmId, 0x10002, &serviceResult, packet, 4032);
	if (result)
		goto cleanup;
	result = serviceResult;
	if (result)
		goto cleanup;
	if (timingsafe_memcmp(pupFileBuffer, &pupFileHeader, sizeof(PupHeader)) ||
	    ((pupFileHeader.m_attribute[0] & 1) &&
	     !SceQafMgrForDriver_3CB55F98() && !SceSblPostSsMgrForDriver_7ACCAA50() &&
	     !SceSblPostSsMgrForDriver_9D1D8EE5() && !SceSblPostSsMgrForDriver_9D2E2D39())) {
		result = (int)0x800f0227;
		goto cleanup;
	}
	if (SceSblSsMgrForKernel_516ECC08(&updateFlag))
		updateFlag = 0;
	attributes = pupFileHeader.m_attribute[0];
	if ((attributes & 2) && !updateFlag) {
		result = (int)0x800f0229;
		goto cleanup;
	}
	platform = pupFileHeader.platform;
	if (platform && !(SceSblAIMgrForDriver_D78B04A2() && platform == 2) &&
	    !((isDex = SceSblAIMgrForDriver_F4B98F66()) && platform == 4) &&
	    !((SceSblAIMgrForDriver_274663A0() || SceSblAIMgrForDriver_3B638885()) &&
	      (platform == 1 || (SceQafMgrForDriver_3CB55F98() && (attributes & 1)))) &&
	    !(isDex && platform == 2 && SceQafMgrForDriver_3CB55F98() && (attributes & 1))) {
		result = (int)0x800f0231;
		goto cleanup;
	}
	if (SpkgInfoUtilGetHardwareIndex(&hardwareIndex))
		hardwareIndex = 0;
	if (!(SpkgInfoUtilGetHardwareSupportMask(hardwareIndex) & pupFileHeader.m_support_list))
		goto versionError;
	if (updateUseSysconNvsService) {
		if (SpkgInfoUtilGetVersion(1, 16, &version, 0, pupSmId))
			version = pupFallbackVersion;
		if (SpkgInfoUtilIsBelowMinimumVersion(1, pupFileHeader.m_format_version, version))
			goto versionError;
	}
	if ((pupFileHeader.m_attribute[0] & 4) && pupFileHeader.m_base_version)
		result = pupFileHeader.m_base_version == version ? 0 : (int)0x800f0237;
cleanup:
	SceThreadmgrForDriver_84EB1EA4(ioFlags);
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
	return sceKernelExitThread(result);
readError:
	result = (int)0x800f0205;
	goto cleanup;
versionError:
	result = (int)0x800f0237;
	goto cleanup;
}
