#include <sysmem/sysclib.h>
#include "update_internal.h"
#include "snvs.h"
#include <syscon/syscon.h>

extern uint32_t updateLastResult;

int SpkgInfoUtilGetSNVSValue(int purpose, int valueType, int bank,
		void *buffer, SceSize size, int loadSm, int existingSmId)
{
	SpkgSnvsSector sector;
	SpkgSnvsSector encrypted;
	unsigned int entry;
	uint32_t sectorIndex;
	uint32_t sectorOffset;
	int useSnvs;
	int result;

	if (size - 1 > 31 || !buffer)
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
		if (result) {
			updateLastResult = 0x06010001;
			return result;
		}
	} else {
		result = sceSysconNvsReadData(32 * sectorIndex, encrypted.words, 16);
		if (!result)
			result = sceSysconNvsReadData(32 * sectorIndex + 16, encrypted.words + 4, 16);
		if (result) {
			updateLastResult = 0x06020001;
			return result;
		}
		result = SpkgInfoUtilSNVSDecryptSectors(sectorIndex, &encrypted, &sector,
				loadSm, existingSmId);
		if (result) {
			updateLastResult = 0x06010004;
			return result;
		}
	}
	memcpy(buffer, (const unsigned char *)&sector + sectorOffset, size);
	return 0;
}
