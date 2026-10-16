#ifndef UPDATE_MGR_SNVS_H
#define UPDATE_MGR_SNVS_H

#include <stddef.h>
#include <stdint.h>
#include <psp2common/types.h>

typedef struct SpkgSnvsSector {
	uint32_t words[8];
} SpkgSnvsSector;

typedef struct SpkgSnvsSectorRequest {
	uint32_t mode;
	uint32_t sectorIndex;
	SpkgSnvsSector sector;
	uint8_t intermediate[48];
	uint8_t final[48];
} SpkgSnvsSectorRequest;

typedef struct SpkgSnvsSectorsRequest {
	int32_t sectorIndex;
	uint32_t sectorCount;
	SpkgSnvsSector sectors[31];
} SpkgSnvsSectorsRequest;

typedef struct SpkgSnvsMapping {
	int32_t purpose;
	int32_t valueType;
	int32_t bank;
	uint32_t sectorIndex;
	uint32_t sectorOffset;
} SpkgSnvsMapping;

typedef struct SpkgSnvsMgmtRequest {
	uint32_t status;
	uint32_t flags;
	SpkgSnvsSector sector;
} SpkgSnvsMgmtRequest;

_Static_assert(sizeof(SpkgSnvsMgmtRequest) == 40, "SNVS management request size");

typedef struct SpkgSnvsMgmtSectorRequest {
	uint32_t mode;
	uint32_t sectorCount;
	uint32_t status;
	uint32_t flags;
	uint8_t intermediate[48];
	uint8_t final[48];
} SpkgSnvsMgmtSectorRequest;

_Static_assert(sizeof(SpkgSnvsMgmtSectorRequest) == 112,
		"SNVS management sector request size");

typedef union SpkgSnvsInitRequest {
	SpkgSnvsMgmtRequest nvs;
	SpkgSnvsMgmtSectorRequest snvs;
} SpkgSnvsInitRequest;

typedef union SpkgSnvsMgmtScratch {
	SpkgSnvsSector sector;
	uint32_t value;
} SpkgSnvsMgmtScratch;

_Static_assert(sizeof(SpkgSnvsInitRequest) == 112, "SNVS init request size");
_Static_assert(sizeof(SpkgSnvsMgmtScratch) == 32, "SNVS management scratch size");
_Static_assert(offsetof(SpkgSnvsMgmtSectorRequest, intermediate) == 16 &&
		offsetof(SpkgSnvsMgmtSectorRequest, final) == 64,
		"SNVS management sector transport layout");

_Static_assert(sizeof(SpkgSnvsMapping) == 20, "SNVS value mapping size");
extern const SpkgSnvsMapping spkgSnvsMappings[25];

_Static_assert(sizeof(SpkgSnvsSector) == 32, "SNVS sector size");
_Static_assert(sizeof(SpkgSnvsSectorRequest) == 136, "SNVS sector request size");
_Static_assert(sizeof(SpkgSnvsSectorsRequest) == 1000, "SNVS sectors request capacity");
_Static_assert(offsetof(SpkgSnvsSectorRequest, intermediate) == 40,
		"SNVS intermediate offset");
_Static_assert(offsetof(SpkgSnvsSectorRequest, final) == 88,
		"SNVS final offset");

int SpkgInfoUtilReadSNVSSector(int sectorIndex, SpkgSnvsSector *sector,
		int loadSm, int smId);
int SpkgInfoUtilWriteSNVSSector(int sectorIndex, const SpkgSnvsSector *sector,
		int loadSm, int smId);
int SpkgInfoUtilSNVSDecryptSectors(int sectorIndex, const SpkgSnvsSector *input,
		SpkgSnvsSector *output, int loadSm, int existingSmId);
int SpkgInfoUtilSNVSEncryptSectors(int sectorIndex, const SpkgSnvsSector *input,
		SpkgSnvsSector *output, int loadSm, int existingSmId);
int SpkgInfoUtilGetSNVSValue(int purpose, int valueType, int bank,
		void *buffer, SceSize size, int loadSm, int existingSmId);
int SpkgInfoUtilSetSNVSValue(int purpose, int valueType, int bank,
		const void *buffer, SceSize size, int loadSm, int existingSmId);

#endif
