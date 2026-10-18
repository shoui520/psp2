#include <psp2kern/kernel/sysclib.h>
#include <sm_comm/sm_comm.h>
#include "update_internal.h"
#include "snvs.h"
#include <syscon/syscon.h>

extern int updateUseSysconSnvs;

int SpkgInfoUtilInitForUpdater(uint32_t *status, uint32_t *flags)
{
	SpkgSnvsInitRequest request;
	SpkgSnvsSector sector, decrypted, encrypted, input;
	SpkgSnvsMgmtScratch stored, verified;
	unsigned int serviceResult;
	unsigned int firstOffset = 32, secondOffset = 48;
	int sectorIndex = 1;
	int smId;
	int tries, result;
	unsigned int word;

	do {
		memset(&sector, 255, sizeof(sector));
		memset(&decrypted, 255, sizeof(decrypted));
		memset(&encrypted, 255, sizeof(encrypted));
		memset(&input, 255, sizeof(input));
		for (word = 0; word < 8; word++)
			sector.words[word] = 0xdeadbeef;
		if (updateUseSysconSnvs) {
			result = SpkgInfoUtilWriteSNVSSector(sectorIndex, &sector, 1, -1);
			if (result)
				return result;
			result = SpkgInfoUtilReadSNVSSector(sectorIndex, &input, 1, -1);
			if (result)
				return result;
			if (timingsafe_memcmp(&sector, &input, sizeof(sector)))
				return (int)0x800f0205;
		} else {
			result = SpkgInfoUtilSNVSEncryptSectors(sectorIndex, &sector,
					&encrypted, 1, -1);
			if (result)
				return result;
			for (tries = 3; tries; tries--) {
				if (!SceSysconForDriver_10C9657A(firstOffset, encrypted.words, 16) &&
				    !SceSysconForDriver_10C9657A(secondOffset, &encrypted.words[4], 16) &&
				    !SceSysconForDriver_ACAFA2B8(firstOffset, input.words, 16) &&
				    !SceSysconForDriver_ACAFA2B8(secondOffset, &input.words[4], 16) &&
				    !timingsafe_memcmp(&encrypted, &input, sizeof(encrypted)) &&
				    !SpkgInfoUtilSNVSDecryptSectors(sectorIndex, &input,
						&decrypted, 1, -1) &&
				    !timingsafe_memcmp(&decrypted, &sector, sizeof(decrypted)))
					break;
			}
		}
		sectorIndex++;
		secondOffset += 32;
		firstOffset += 32;
	} while (sectorIndex != 21);
	*(unsigned char *)flags &= ~1u;
	if (updateUseSysconSnvs) {
		request.snvs.status = *status;
		request.snvs.mode = 2;
		request.snvs.sectorCount = 0;
		smId = -1;
		request.snvs.flags = *flags;
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result)
			goto cleanup;
		result = SceSblSmCommForKernel_DB9FC204(smId, 0xc0002, &serviceResult,
				&request.snvs, sizeof(request.snvs));
		if (result)
			goto cleanup;
		result = serviceResult;
		if (result)
			goto cleanup;
		result = SceSysconForDriver_63683B9B(request.snvs.intermediate, 48,
				request.snvs.final, 16);
		if (result)
			goto cleanup;
		request.snvs.mode = 3;
		result = SceSblSmCommForKernel_DB9FC204(smId, 0xc0002, &serviceResult,
				&request.snvs, sizeof(request.snvs));
		if (result)
			goto cleanup;
		result = serviceResult;
		if (result)
			goto cleanup;
		result = SpkgInfoUtilGetSNVSFlagStatus(&stored.value, &verified.value,
				0, smId);
		if (result)
			goto cleanup;
		if (timingsafe_memcmp(&stored.value, status, sizeof(*status))) {
			result = (int)0x800f0205;
			goto cleanup;
		}
		result = timingsafe_memcmp(&verified.value, flags, sizeof(*flags));
		result = result ? (int)0x800f0205 : 0;
	} else {
		tries = 3;
		smId = -1;
		do {
			if (!SceSysconForDriver_ACAFA2B8(0, stored.sector.words, 16) &&
			    !SceSysconForDriver_ACAFA2B8(16, &stored.sector.words[4], 16))
				break;
		} while (--tries);
		request.nvs.sector = stored.sector;
		request.nvs.status = *status;
		request.nvs.flags = *flags;
		result = UpdateUtilSetupSm(0, &smId, 0);
		if (result)
			goto cleanup;
		if (SceSblSmCommForKernel_DB9FC204(smId, 0x80002, &serviceResult,
				&request.nvs, sizeof(request.nvs))) {
			result = serviceResult;
			goto cleanup;
		}
		stored.sector = request.nvs.sector;
		tries = 3;
		do {
			result = SceSysconForDriver_10C9657A(0, stored.sector.words, 16);
			if (!result) {
				result = SceSysconForDriver_10C9657A(16, &stored.sector.words[4], 16);
				if (!result) {
					result = SceSysconForDriver_ACAFA2B8(0, verified.sector.words, 16);
					if (!result) {
						result = SceSysconForDriver_ACAFA2B8(16,
								&verified.sector.words[4], 16);
						if (!result) {
							if (!timingsafe_memcmp(&stored.sector,
									&verified.sector, sizeof(stored.sector)))
								break;
							result = (int)0x800f0205;
						}
					}
				}
			}
		} while (--tries);
	}
cleanup:
	if (smId != -1)
		UpdateUtilStopSm(&smId);
	return result;
}
