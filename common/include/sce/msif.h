#ifndef SCE_MSIF_H
#define SCE_MSIF_H

#include <stdint.h>

struct MsproalHandle;

/* Complete result of SceMsifForDriver's media-information query. */
typedef struct SceMsInfo {
	int mediaType;
	int writeProtected;
	uint64_t nbytes;
	uint64_t nbytes2;
	uint64_t sector_size;
	uint32_t blockSize;
	uint32_t serialNumber;
	uint32_t assemblyModel;
	uint8_t assemblyMaker;
	uint8_t reservedBeforeDate[3];
	uint8_t assemblyDate[8];
	struct MsproalHandle *handle;
	uint8_t reserved[4];
} SceMsInfo;

int ksceMsifGetMsInfo(SceMsInfo *info);

#endif
