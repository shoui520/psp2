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

int SceMsifForDriver_D0307849(SceMsInfo *info);	/* vitasdk: ksceMsifGetMsInfo */

#endif

int SceMsifForDriver_491E25B5(void);	/* vitasdk: ksceMsifGetSlowModeState */
/* vitasdk: ksceMsifReadSector */
int SceMsifForDriver_58654AA3(uint32_t sector, uint8_t *data, unsigned int count);
uint64_t SceMsifForDriver_855E1E38(void);
int SceMsifForDriver_F997286B(void);
int sceMsifDevNullPrepareDmaTable(uint32_t sector, unsigned int count);
int sceMsifDisableSlowCardMode(void);
int sceMsifEnableSlowCardMode(void);
int sceMsifFormat(int mode);
int sceMsifGetMediaId(uint8_t *mediaId);
int sceMsifSwReset(void);
int sceMsifWriteSector(uint32_t sector, uint8_t *data, unsigned int count);
