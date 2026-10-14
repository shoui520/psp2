#include <psp2kern/kernel/sysclib.h>
#include "update_internal.h"
#include "snvs.h"

extern uint32_t updateLastResult;
int ksceSysconNvsReadData(unsigned int offset, void *buffer, SceSize size);
int ksceSysconNvsWriteData(unsigned int offset, const void *buffer, SceSize size);

int SpkgInfoUtilSetSNVSValue(int purpose, int valueType, int bank,
		const void *buffer, SceSize size, int loadSm, int existingSmId)
{
	SpkgSnvsSector sector;
	SpkgSnvsSector encrypted;
	unsigned int entry;
	uint32_t sectorIndex;
	uint32_t sectorOffset;
	int useSnvs;
	int result;

	if (!buffer || size - 1 > 31)
		return (int)0x800f0216;
	for (entry = 0; entry < 25; entry++) {
		if (purpose == spkgSnvsMappings[entry].purpose &&
		    bank == spkgSnvsMappings[entry].bank &&
		    valueType == spkgSnvsMappings[entry].valueType)
			break;
	}
	if (entry == 25)
		return (int)0x800f0202;
	useSnvs = updateUseSysconSnvs;
	sectorIndex = spkgSnvsMappings[entry].sectorIndex;
	sectorOffset = spkgSnvsMappings[entry].sectorOffset;
	sector = (SpkgSnvsSector){ { 0 } };
	encrypted = (SpkgSnvsSector){ { 0 } };
	if (useSnvs) {
		result = SpkgInfoUtilReadSNVSSector(sectorIndex, &sector, loadSm, existingSmId);
		if (result)
			goto snvsReadFailed;
		memcpy((unsigned char *)&sector + sectorOffset, buffer, size);
		result = SpkgInfoUtilWriteSNVSSector(sectorIndex, &sector, loadSm, existingSmId);
		if (result) {
			updateLastResult = 0x06010002;
			return result;
		}
		result = SpkgInfoUtilReadSNVSSector(sectorIndex, &encrypted, loadSm, existingSmId);
		if (result)
			goto snvsReadFailed;
		if (!timingsafe_memcmp(&encrypted, &sector, 32))
			return 0;
		updateLastResult = 0x06010005;
		return (int)0x800f0205;
	} else {
		result = ksceSysconNvsReadData(32 * sectorIndex, encrypted.words, 16);
		if (result)
			goto nvsReadFailed;
		result = ksceSysconNvsReadData(32 * sectorIndex + 16, encrypted.words + 4, 16);
		if (result)
			goto nvsReadFailed;
		result = SpkgInfoUtilSNVSDecryptSectors(sectorIndex, &encrypted, &sector,
				loadSm, existingSmId);
		if (result) {
			updateLastResult = 0x06020004;
			return result;
		}
		memcpy((unsigned char *)&sector + sectorOffset, buffer, size);
		result = SpkgInfoUtilSNVSEncryptSectors(sectorIndex, &sector, &encrypted,
				loadSm, existingSmId);
		if (result) {
			updateLastResult = 0x06020003;
			return result;
		}
		result = ksceSysconNvsWriteData(32 * sectorIndex, encrypted.words, 16);
		if (!result)
			result = ksceSysconNvsWriteData(32 * sectorIndex + 16, encrypted.words + 4, 16);
		if (result) {
			updateLastResult = 0x06020002;
			return result;
		}
		result = ksceSysconNvsReadData(32 * sectorIndex, sector.words, 16);
		if (!result)
			result = ksceSysconNvsReadData(32 * sectorIndex + 16, sector.words + 4, 16);
		if (result)
			goto nvsReadFailed;
		if (!timingsafe_memcmp(&encrypted, &sector, 32))
			return 0;
		updateLastResult = 0x06020005;
		return (int)0x800f0205;
	}
snvsReadFailed:
	updateLastResult = 0x06010001;
	return result;
nvsReadFailed:
	updateLastResult = 0x06020001;
	return result;
}
