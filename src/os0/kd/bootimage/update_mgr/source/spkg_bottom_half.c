#include <sce/sysmem.h>
#include <sce/sm_comm.h>
#include <sce/certified_file.h>
#include <psp2kern/kernel/cpu/cache.h>
#include <psp2kern/io/fcntl.h>
#include <string.h>
#include "update_internal.h"
#include "update_packet.h"
#include "spkg_peripherals.h"
#include "sbls.h"

extern int spkgMbrLoaded;
extern unsigned int spkgBlSwitchCount, spkgOsSwitchCount;
extern uint32_t spkgModifiedBanks;
extern uint64_t spkgOsChunkMask, spkgVshChunkMask;
extern uint64_t spkgSysdataChunkMask, spkgPidataChunkMask;
extern SceMbr spkgMbr;
int UpdateUtilLockMbr(void);
int UpdateUtilUnlockMbr(void);
int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status);
int UpdateUtilSetBufferState(unsigned int state, void *buffer);
int UpdateUtilCreatePacketPayload(UpdatePacketPayload **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		void *buffer, SceSize size);
int UpdateUtilFreePARangeVector(SceUID heapId, SceKernelPAVector *vector);
int UpdateUtilInflateWritePartition(const void *buffer, uint64_t size,
		uint64_t offset, const SceMbr *mbr, uint8_t partition,
		unsigned int nodeType, UpdateUtilNodeStatus *status, SceUID pid);
int StorageUtilSwitchOSBank(SceUID heapId, SceMbr *mbr);
int StorageUtilSwitchBLBank(SceUID heapId, SceMbr *mbr);
int SpkgUtilDoPersonalizeSLSK(const char *sourceName, const char *destinationName,
		const char *previousName, const char *retiredName, SceSlb2Header *sbls);
int SpkgInfoUtilSetDigest(int packageType, int flags, const void *digest,
		SceSize size, int verify, int updateMode);
int SceSblSsMgrForKernel_516ECC08(unsigned char *flag);
int SceQafMgrForDriver_3CB55F98(void);
int SceQafMgrForDriver_8C423C18(void);
int SceSblPostSsMgrForDriver_9D1D8EE5(void);
int ksceSblUtMgrHasComTestFlag(void);
int ksceSblUtMgrHasStoreFlag(void);
int ksceSblAimgrIsCEX(void);
int ksceSblAimgrIsDEX(void);
int ksceSblAimgrIsTool(void);
int ksceSblAimgrIsTest(void);
int ksceSha256Digest(const void *buffer, SceSize size, void *digest);

int sceSblUsUpdateSpackageBH(SceSize arglen, SpkgThreadArgs *args)
{
	SpkgChunkInfo chunk;
	SpkgPackageInfo info;
	cf_header header;
	UpdateUtilNodeStatus status;
	UpdatePacketPayload *packet;
	SceKernelPAVector *dataVector, *vectorVector;
	unsigned char updateFlag = 0;
	unsigned int version = 0, currentVersion, bootVersion;
	unsigned int partition, bodyOffset, payloadSize, i;
	unsigned int serviceResult;
	unsigned char digest[32];
	uint64_t completed, expected;
	void *payload;
	SceUID heapId, fd;
	int smId, result, hardwareIndex, isDex, compressed;

	SceKernelSuspendForDriver_4DF40893(0);
	UpdateUtilLockMbr();
	result = UpdateUtilGetNodeStatusLocked(args->nodeType, &status);
	if (result)
		goto error;
	switch (args->packageType) {
	case 1:
	case 21:
		if (spkgOsSwitchCount)
			goto alreadyModified;
		if (!spkgMbrLoaded) {
			result = StorageUtilReadMbr(&spkgMbr);
			if (result)
				goto error;
			spkgMbrLoaded = 1;
		}
		if (spkgModifiedBanks & 2)
			goto alreadyModified;
		partition = 3;
		break;
	case 9:
		if (spkgBlSwitchCount)
			goto alreadyModified;
		if (!spkgMbrLoaded) {
			result = StorageUtilReadMbr(&spkgMbr);
			if (result)
				goto error;
			spkgMbrLoaded = 1;
		}
		if (spkgModifiedBanks & 1)
			goto alreadyModified;
		partition = 2;
		break;
	case 10:
		if (!spkgMbrLoaded) {
			result = StorageUtilReadMbr(&spkgMbr);
			if (result)
				goto error;
			spkgMbrLoaded = 1;
		}
		partition = 4;
		break;
	case 23:
		if (!spkgMbrLoaded) {
			result = StorageUtilReadMbr(&spkgMbr);
			if (result)
				goto error;
			spkgMbrLoaded = 1;
		}
		partition = 12;
		break;
	case 24:
		if (!spkgMbrLoaded) {
			result = StorageUtilReadMbr(&spkgMbr);
			if (result)
				goto error;
			spkgMbrLoaded = 1;
		}
		partition = 14;
		break;
	case 3: case 4: case 8: case 11: case 12: case 13:
	case 15: case 16: case 17: case 18: case 19: case 20:
	case 22: case 25: case 27: case 28:
		partition = 0;
		break;
	default:
		goto unsupported;
	}
	status.progress = 2;
	status.seq_no++;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
	serviceResult = 0;
	dataVector = NULL;
	vectorVector = NULL;
	heapId = UpdateUtilGetHeapId();
	packet = NULL;
	smId = -1;
	result = UpdateUtilCreatePacketPayload(&packet, &dataVector, &vectorVector,
			args->buffer, args->size);
	if (result)
		goto releasePacket;
	ksceKernelDcacheCleanRange(args->buffer, (args->size + 63) & ~63);
	if (args->packageType == 27) {
		result = UpdateUtilSetupSm(1, &smId, 0);
		if (result)
			goto releasePacket;
		result = ksceSblSmCommCallFunc(smId, 0xe0002,
				&serviceResult, packet, sizeof(*packet));
	} else {
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result)
			goto releasePacket;
		result = ksceSblSmCommCallFunc(smId, 0x40002,
				&serviceResult, packet, sizeof(*packet));
	}
	if (result)
		goto releasePacket;
	result = serviceResult;
	if (result)
		goto releasePacket;
	ksceKernelDcacheInvalidateRange(args->buffer, (args->size + 63) & ~63);
	info = packet->records.returned.packageInfo;
	chunk = packet->records.returned.chunkInfo;
releasePacket:
	if (dataVector) {
		UpdateUtilFreePARangeVector(heapId, dataVector);
		ksceKernelFreeHeapMemory(heapId, dataVector);
	}
	if (vectorVector) {
		UpdateUtilFreePARangeVector(heapId, vectorVector);
		ksceKernelFreeHeapMemory(heapId, vectorVector);
	}
	if (packet)
		ksceKernelFreeHeapMemory(heapId, packet);
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	if (result)
		goto error;
	memcpy(&header, args->buffer, sizeof(header));
	if (header.file_offset > 1024)
		goto invalidRequirements;
	if (SceSblSsMgrForKernel_516ECC08(&updateFlag))
		updateFlag = 0;
	if ((info.requirements & 2) && !updateFlag)
		goto alreadyModified;
	if (info.platform && (!ksceSblAimgrIsCEX() || !(info.platform & 0x40000000))) {
		isDex = ksceSblAimgrIsDEX();
		if ((!isDex || !(info.platform & 0x20000000)) &&
		    ((!ksceSblAimgrIsTool() && !ksceSblAimgrIsTest()) ||
		     (!(info.platform & 0x10000000) &&
		      (!SceQafMgrForDriver_3CB55F98() || !(info.requirements & 1)))) &&
		    (!isDex || !(info.platform & 0x40000000) ||
		     !SceQafMgrForDriver_3CB55F98() || !(info.requirements & 1))) {
			result = (int)0x800f0231;
			goto error;
		}
	}
	switch (args->packageType) {
	case 1: case 9: case 21:
		if (SpkgInfoUtilGetHardwareIndex(&hardwareIndex))
			hardwareIndex = 0;
		if (!(info.hardwareInfo & SpkgInfoUtilGetHardwareSupportMask(hardwareIndex)))
			goto invalidVersion;
		break;
	case 8:
		if (!updateUseMotionService)
			goto unsupported;
		result = SysconUtilCheckFirmwareType(info.hardwareInfo, 0);
		break;
	case 11:
		if (!updateCpupConnected)
			goto unsupported;
		result = CpUtilCheckFirmwareType(info.hardwareInfo);
		break;
	case 12: case 15:
		if (!updateMotionConnected)
			goto unsupported;
		result = MotionUtilCheckFirmwareType(info.hardwareInfo);
		break;
	case 13:
		result = BbmcUtilCheckFirmwareType(info.hardwareInfo);
		break;
	case 16: case 17:
		if (!updateUseTouchService)
			goto unsupported;
		result = TouchUtilCheckFirmwareType(info.hardwareInfo, args->packageType == 17);
		break;
	case 18: case 19:
		if (!updateUseBatteryService)
			goto unsupported;
		result = args->packageType == 18 ? BicUtilCheckFirmwareType(info.hardwareInfo) :
			BicUtilCheckDataFlashType(info.hardwareInfo);
		break;
	case 20:
		if (!updateUseSysconConfigStorage)
			goto unsupported;
		result = SysconUtilCheckFirmwareType(info.hardwareInfo, 2);
		break;
	case 23: case 24:
		fd = StorageUtileGetPartitionFd(partition, 1);
		if (fd < 0)
			goto unsupported;
		ksceIoClose(fd);
		break;
	case 25:
		if (!updateUseSysconUpdateService)
			goto unsupported;
		result = SysconUtilCheckFirmwareType(info.hardwareInfo, 3);
		break;
	}
	if (result)
		goto error;
	if (args->nodeType != 3) {
		result = SpkgInfoUtilGetVersion(args->packageType, 16, &version, 1, -1);
		if (result != (int)0x800f0202 && result)
			goto error;
		currentVersion = version;
		if (!SceQafMgrForDriver_8C423C18()) {
			switch (args->packageType) {
			case 1: case 9:
				if ((args->flags & 8) && version > (uint32_t)info.updateVersion &&
				    (!updateFlag || !(info.requirements & 2)))
					goto invalidVersion;
				break;
			case 10: case 21: case 22: case 23: case 24:
				if ((args->flags & 8) && version > (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 8:
				if (!updateUseMotionService)
					goto unsupported;
				currentVersion = updateSysconFirmwareVersion;
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 11:
				if (!updateCpupConnected)
					goto unsupported;
				currentVersion = updateCpupBoardVersion;
				if (currentVersion > (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 12: case 15:
				if (!updateMotionConnected)
					goto unsupported;
				currentVersion = updateMotionFirmwareVersion;
				if (currentVersion >= (uint32_t)info.updateVersion && currentVersion != 65535)
					goto invalidVersion;
				break;
			case 13:
				if (BbmcUtilGetFirmwareVersion(&currentVersion))
					goto unsupported;
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 16:
				if (!updateUseTouchService)
					goto unsupported;
				currentVersion = TouchUtilGetConfigVersionByHardware(info.hardwareInfo);
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 17:
				if (!updateUseTouchService)
					goto unsupported;
				currentVersion = TouchUtilGetFirmwareVersionByHardware(info.hardwareInfo);
				if (TouchUtilRequiresNewerFirmware(info.hardwareInfo) ?
				    currentVersion >= (uint32_t)info.updateVersion :
				    currentVersion > (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 18: case 19:
				if (!updateUseBatteryService)
					goto unsupported;
				currentVersion = args->packageType == 18 ?
					updateBatteryFirmwareVersion : updateBatteryDataFlashVersion;
				if (currentVersion == 65534)
					currentVersion = 0;
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 20:
				if (!updateUseSysconConfigStorage || SysconUtilGetConfigVersion(&currentVersion))
					goto unsupported;
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 25:
				if (!updateUseSysconUpdateService || SysconUtilGetDownloaderVersion(&currentVersion))
					goto unsupported;
				if (currentVersion >= (uint32_t)info.updateVersion)
					goto invalidVersion;
				break;
			case 28:
				if ((args->flags & 8) && ((uint32_t)info.updateVersion & ~4095u) <=
				    (version & ~4095u) && (((uint32_t)info.updateVersion ^ version) & ~4095u))
					goto invalidVersion;
				break;
			}
		}
		if (info.baseVersion) {
			bootVersion = 0;
			result = SpkgInfoUtilGetVersion(9, 16, &bootVersion, 1, -1);
			if (result)
				goto error;
			if (info.baseVersion > bootVersion) {
				result = (int)0x800f0226;
				goto error;
			}
		}
	}
	if (SpkgInfoUtilIsBelowMinimumVersion(args->packageType, info.updateVersion, version))
		goto invalidVersion;
	if ((info.requirements & 1) && !SceQafMgrForDriver_3CB55F98() &&
	    !ksceSblUtMgrHasComTestFlag() && !SceSblPostSsMgrForDriver_9D1D8EE5() &&
	    !ksceSblUtMgrHasStoreFlag())
		goto invalidRequirements;
	compressed = info.finalSize != info.decryptedSize;
	if (compressed && ((unsigned int)(args->packageType - 1) > 27 ||
	    !((1u << (args->packageType - 1)) & 0x0cf0020d)))
		goto invalidFormat;
	if (args->nodeType != 1) {
		result = 0;
		goto finish;
	}
	status.progress = 3;
	status.seq_no++;
	UpdateUtilUpdateNodeStatus(1, &status);
	switch (args->packageType) {
	case 1: case 23: case 24:
		if (chunk.chunkCount > 64)
			goto invalidFormat;
		break;
	case 10: case 13: case 21: case 22: case 28:
		break;
	default:
		if (chunk.chunkCount != 1)
			goto invalidFormat;
	}
	bodyOffset = (uint32_t)header.file_offset + 128;
	payloadSize = args->size - bodyOffset;
	if ((uint64_t)payloadSize != chunk.size && (uint64_t)payloadSize != info.decryptedSize)
		goto invalidFormat;
	payload = (uint8_t *)args->buffer + bodyOffset;
	switch (args->packageType) {
	case 9:
		result = SpkgUtilDoPersonalizeSLSK("second_loader.enc", "second_loader.enp",
				"second_loader.enp", "second_loader.enp_", payload);
		if (result)
			goto error;
		result = SpkgUtilDoPersonalizeSLSK("secure_kernel.enc", "secure_kernel.enp",
				"secure_kernel.enp", "secure_kernel.enp_", payload);
		if (result)
			goto error;
	case 1: case 10: case 23: case 24:
		if (compressed)
			result = UpdateUtilInflateWritePartition(payload, payloadSize, chunk.offset,
					&spkgMbr, partition, args->nodeType, &status, args->pid);
		else
			result = UpdateUtilWritePartition(payload, payloadSize, chunk.offset,
					&spkgMbr, partition, args->nodeType, &status);
		break;
	case 8: case 20: case 25:
		result = SysconUtilWriteFirmware(args->packageType == 8 ? 0 :
				args->packageType == 20 ? 2 : 3, payload, payloadSize,
				args->nodeType, &status);
		break;
	case 11:
		result = CpUtilWriteFirmware(payload, payloadSize, info.updateVersion, args->nodeType, &status);
		break;
	case 12: case 15:
		result = MotionUtilWriteFirmware(info.hardwareInfo, payload, payloadSize,
				info.updateVersion, args->nodeType, &status);
		break;
	case 13:
		result = BbmcUtilWriteFirmware(payload, payloadSize, info.updateVersion,
				chunk.chunkIndex, chunk.chunkCount, args->nodeType, &status);
		break;
	case 16:
		result = TouchUtilWriteFirmware(info.hardwareInfo, payload, payloadSize,
				info.updateVersion, args->nodeType, &status);
		break;
	case 17:
		result = TouchUtilWriteConfig(info.hardwareInfo, payload, payloadSize,
				info.updateVersion, args->nodeType, &status);
		break;
	case 18: case 19:
		result = BicUtilWriteFirmware(payload, payloadSize, info.updateVersion, args->nodeType, &status);
		break;
	default:
		result = 0;
	}
	if (result)
		goto error;
	status.progress = 3;
	status.seq_no++;
	status.written_rates = 100;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
	heapId = UpdateUtilGetHeapId();
	completed = 0;
	switch (args->packageType) {
	case 1:
		spkgOsChunkMask |= 1ULL << ((uint32_t)chunk.chunkIndex - 1);
		completed = spkgOsChunkMask;
		break;
	case 10:
		spkgVshChunkMask |= 1ULL << ((uint32_t)chunk.chunkIndex - 1);
		completed = spkgVshChunkMask;
		break;
	case 23:
		spkgSysdataChunkMask |= 1ULL << ((uint32_t)chunk.chunkIndex - 1);
		completed = spkgSysdataChunkMask;
		break;
	case 24:
		spkgPidataChunkMask |= 1ULL << ((uint32_t)chunk.chunkIndex - 1);
		completed = spkgPidataChunkMask;
		break;
	case 9: {
		SceSlb2Header *sbls = payload;
		SceSlb2Entry *last;
		if (!payloadSize || !sbls) {
			result = (int)0x800f0216;
			goto error;
		}
		if (sbls->magic != SCE_SLB2_MAGIC)
			goto invalidFormat;
		last = &sbls->entryList[sbls->entryNum - 1];
		result = ksceSha256Digest(sbls, (last->fileStartSector << 9) +
				((last->fileSizeInByte + 511) >> 9 << 9), digest);
		if (result)
			goto error;
		result = SpkgInfoUtilSetDigest(args->packageType, 32, digest, sizeof(digest), 1, -1);
		if (result)
			goto error;
		result = StorageUtilSwitchBLBank(heapId, &spkgMbr);
		if (result)
			goto error;
		spkgModifiedBanks |= 1;
		spkgBlSwitchCount++;
		goto versionUpdated;
	}
	case 21:
		goto switchOs;
	default:
		goto versionUpdated;
	}
	expected = 0;
	for (i = 0; (uint64_t)i < chunk.chunkCount; i++)
		expected |= 1ULL << i;
	if (args->packageType == 1) {
		if (completed == expected)
			goto switchOs;
		goto partialComplete;
	}
	status.progress = 4;
	status.seq_no++;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
	if (completed != expected) {
		result = 0;
		goto finish;
	}
	goto storeVersion;
switchOs:
	result = StorageUtilSwitchOSBank(heapId, &spkgMbr);
	if (result)
		goto error;
	spkgModifiedBanks |= 2;
	spkgOsSwitchCount++;
versionUpdated:
	status.progress = 4;
	status.seq_no++;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
storeVersion:
	result = SpkgInfoUtilSetVersion(args->packageType, 32, info.updateVersion, 1, -1);
	if (result && result != (int)0x800f0202)
		goto error;
	if (args->packageType == 24) {
		result = SpkgInfoUtilSetVersion(28, 64, 0, 1, -1);
		if (result && result != (int)0x800f0202)
			goto error;
	}
	if (args->packageType == 28) {
		result = SpkgInfoUtilSetVersion(28, 64, info.updateVersion, 1, -1);
		if (result && result != (int)0x800f0202)
			goto error;
	}
	result = 0;
	goto finish;
partialComplete:
	status.progress = 4;
	status.seq_no++;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
	result = 0;
	goto finish;
unsupported:
	result = (int)0x800f0202;
	goto error;
alreadyModified:
	result = (int)0x800f0229;
	goto error;
invalidRequirements:
	result = (int)0x800f0227;
	goto error;
invalidVersion:
	result = (int)0x800f0237;
	goto error;
invalidFormat:
	result = (int)0x800f0224;
error:
	memset(args->buffer, 255, args->size);
finish:
	UpdateUtilSetBufferState(0, args->buffer);
	status.progress = 5;
	status.seq_no++;
	status.result = result;
	UpdateUtilUpdateNodeStatus(args->nodeType, &status);
	UpdateUtilUnlockMbr();
	SceKernelSuspendForDriver_2BB92967(0);
	return sceKernelExitDeleteThread(result);
}
