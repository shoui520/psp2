/*
 * SceSdif: the per-card ("part") context of an SD host port, as
 * ksceSdifGetSdContextPart*() hand it out.
 *
 * The SD initializer fills the complete 0xc0-byte context, including decoded
 * CSD, SCR and SD-status parameters following the raw card registers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

typedef struct SdifContextGlobal SdifContextGlobal;

typedef struct SdifIoResult {
	int errorCode;
	uint32_t index;
	void *argument;
	uint32_t response[4];
} SdifIoResult;

typedef struct SdifIoTransfer {
	uint32_t reserved;
	uint32_t address;
	uint32_t length;
	void *buffer;
	uint32_t flags;
	void *argument;
	void (*completion)(SdifIoResult *result);
	uint32_t index;
} SdifIoTransfer;

_Static_assert(sizeof(SdifIoResult) == 0x1c, "SdifIoResult");
_Static_assert(sizeof(SdifIoTransfer) == 0x20, "SdifIoTransfer");

/* SD card registers, kept as the controller returns them (low word first) */
typedef struct SdifCardReg128 {
	SceUInt32 word[4];
} SdifCardReg128;

typedef struct SdifCardReg64 {
	SceUInt32 word[2];
} SdifCardReg64;

typedef struct SdifCardInfo {
	SdifCardReg128 cid;
	SdifCardReg128 csd;
	SdifCardReg128 ssr[4];			/* SD status, 512 bits */
	SdifCardReg64 scr;
} SdifCardInfo;

/* Descriptive names for the decoded SD card register fields. */
typedef struct SdifSdCardParameters {
	SceUInt8 csdStructure;
	SceUInt8 reservedStructure[3];
	SceUInt32 maximumClock;
	SceUInt16 commandClasses;
	SceUInt16 reservedClasses;
	SceUInt32 blockCount;
	SceUInt32 readBlockLength;
	SceUInt32 eraseSectorSize;
	SceUInt64 capacity;
	SceUInt8 scrStructure;
	SceUInt8 sdSpecification;
	SceUInt8 dataStatusAfterErase;
	SceUInt8 sdSecurity;
	SceUInt8 sdBusWidths;
	SceUInt8 reservedScr[3];
	SceUInt16 additionalScrFields;
	SceUInt16 reservedScrFields;
	SceUInt8 dataBusWidth;
	SceUInt8 securedMode;
	SceUInt16 sdCardType;
	SceUInt32 protectedAreaSize;
	SceUInt8 speedClass;
	SceUInt8 performanceMove;
	SceUInt8 allocationUnitSize;
	SceUInt8 reservedStatus;
	SceUInt16 eraseSize;
	SceUInt8 eraseTimeout;
	SceUInt8 eraseOffset;
	SceUInt32 eraseGroupSize;
	SceUInt32 highSpeedClock;
	SceUInt32 reservedTail;
} SdifSdCardParameters;

typedef struct SdifContextPart {
	void *contextGlobal;			/* the port's SdifContextGlobal */
	SceUInt32 state;			/* bit 0 set during card init */
	SceUInt32 writeProtected;		/* host PRESENT_STATE bit 19, inverted */
	SceUInt16 blockLength;			/* 0x200 */
	SceUInt16 reserved;
	SdifCardInfo info;
	SdifSdCardParameters parameters;
} SdifContextPart;

_Static_assert(sizeof(SdifCardInfo) == 0x68, "SdifCardInfo");
_Static_assert(offsetof(SdifContextPart, info) == 0x10, "SdifContextPart.info");
_Static_assert(sizeof(SdifSdCardParameters) == 0x48, "SdifSdCardParameters");
_Static_assert(offsetof(SdifContextPart, parameters) == 0x78,
	"SdifContextPart.parameters");
_Static_assert(sizeof(SdifContextPart) == 0xc0, "SdifContextPart");

typedef struct SdifMmcPartition {
	uint64_t va;
	uint64_t size;
	uint64_t pa;
	uint32_t partitionType;
	uint8_t accessType;
	uint8_t reserved[3];
} SdifMmcPartition;

typedef struct SdifMmcCardParameters {
	uint8_t csdStructure;
	uint8_t specificationVersion;
	uint16_t reservedVersion;
	uint32_t accessTime;
	uint32_t accessClocks;
	uint32_t maximumClock;
	uint16_t commandClasses;
	uint16_t reservedClasses;
	uint32_t blockCount;
	uint8_t readBlockLength;
	uint8_t readBlockPartial;
	uint8_t writeBlockMisalign;
	uint8_t readBlockMisalign;
	uint8_t readToWriteFactor;
	uint8_t writeBlockLength;
	uint8_t writeBlockPartial;
	uint8_t reservedWrite;
	uint64_t capacity;
	uint32_t eraseGroupSize;
	uint32_t reservedErase;
	uint32_t sectorCount;
	uint32_t highSpeedClock;
	uint32_t highCapacityEraseGroupSize;
	uint8_t eraseGroupDefinition;
	uint8_t reservedTail[3];
} SdifMmcCardParameters;

typedef struct SdifMmcContext {
	SdifContextGlobal *contextGlobal;
	uint32_t state;
	uint16_t blockLength;
	uint16_t reserved;
	uint32_t bug16635Target;
	SdifCardReg128 cid;
	SdifCardReg128 csd;
	uint8_t extendedCsd[512];
	SdifMmcCardParameters parameters;
	SdifMmcPartition partitions[9];
	SdifMmcPartition *currentPartition;
	uint32_t activeEraseGroupSize;
} SdifMmcContext;

_Static_assert(sizeof(SdifMmcPartition) == 0x20, "SdifMmcPartition");
_Static_assert(sizeof(SdifMmcCardParameters) == 0x40, "SdifMmcCardParameters");
_Static_assert(offsetof(SdifMmcContext, parameters) == 0x230, "SdifMmcContext.parameters");
_Static_assert(offsetof(SdifMmcContext, partitions) == 0x270, "SdifMmcContext.partitions");
_Static_assert(offsetof(SdifMmcContext, currentPartition) == 0x390, "SdifMmcContext.currentPartition");
_Static_assert(sizeof(SdifMmcContext) == 0x398, "SdifMmcContext");

/* vitasdk: ksceSdifGetSdContextPartValidateMmc */
SdifMmcContext *SceSdifForDriver_6A71987F(unsigned int index);
/* vitasdk: ksceSdifGetSdContextPartValidateSd */
SdifContextPart *SceSdifForDriver_B9EA5B1E(unsigned int index);

typedef struct SdifSdioContext SdifSdioContext;

typedef struct SdifSdioFunctionIdentifier {
	uint32_t manufacturerId;
	uint32_t productId;
	uint32_t interfaceCode;
} SdifSdioFunctionIdentifier;

typedef struct SdifSdioFunction {
	uint32_t manufacturerId;
	uint32_t productId;
	uint32_t interfaceCode;
	uint32_t number;
	uint8_t cis[256];
	uint32_t cisLength;
	uint32_t standardInterface;
	uint32_t extendedInterface;
	uint32_t extensionType;
	uint32_t functionInfo;
	uint32_t standardIoRevision;
	uint32_t serialNumber;
	uint32_t codeStorageAreaSize;
	uint32_t codeStorageAreaProperty;
	uint16_t maximumBlockSize;
	uint16_t reservedBlockSize;
	uint32_t operationConditions;
	uint32_t operatingMinimumPower;
	uint32_t operatingAveragePower;
	uint32_t operatingMaximumPower;
	uint32_t standbyMinimumPower;
	uint32_t standbyAveragePower;
	uint32_t standbyMaximumPower;
	uint32_t minimumBandwidth;
	uint32_t optimumBandwidth;
	uint32_t enableTimeout;
	uint32_t standardPowerAverage;
	uint32_t standardPowerMaximum;
	uint32_t highPowerAverage;
	uint32_t highPowerMaximum;
	uint32_t lowPowerAverage;
	uint32_t lowPowerMaximum;
	SdifSdioContext *context;
	uint16_t blockSize;
	uint16_t reservedTail;
} SdifSdioFunction;

struct SdifSdioContext {
	SdifContextGlobal *contextGlobal;
	uint32_t reserved;
	uint32_t functionCount;
	SdifSdioFunction functions[8];
	uint32_t reservedCommon[4];
	uint8_t commonCis[256];
	uint32_t commonCisLength;
	uint32_t manufacturerId;
	uint32_t productId;
	uint32_t reservedCard;
	uint32_t maximumBlockSize;
	uint32_t maximumClock;
	uint8_t masterInterruptEnabled;
	uint8_t reservedTail[3];
};

_Static_assert(sizeof(SdifSdioFunctionIdentifier) == 12, "SdifSdioFunctionIdentifier");
_Static_assert(sizeof(SdifSdioFunction) == 0x180, "SdifSdioFunction");
_Static_assert(offsetof(SdifSdioFunction, context) == 0x178, "SdifSdioFunction.context");
_Static_assert(sizeof(SdifSdioContext) == 0xd38, "SdifSdioContext");
_Static_assert(offsetof(SdifSdioContext, commonCis) == 0xc1c, "SdifSdioContext.commonCis");
_Static_assert(offsetof(SdifSdioContext, masterInterruptEnabled) == 0xd34,
	       "SdifSdioContext.masterInterruptEnabled");

int SceSdifForDriver_AABAA0F0(unsigned int index, SdifSdioContext **part);
SdifSdioFunction *SceSdifForDriver_0C66E36F(SdifSdioContext *sdio,
					const SdifSdioFunctionIdentifier *identifier);
int SceSdifForDriver_BC45C83D(SdifSdioContext *sdio, unsigned int mode);
int SceSdifForDriver_733BC373(SdifSdioFunction *function, unsigned int size);
int SceSdifForDriver_3428884D(SdifSdioFunction *function);
int SceSdifForDriver_D0F78D9B(SdifSdioFunction *function, uint32_t address,
			    uint32_t size, uint8_t *buffer, uint32_t flags);
int SceSdifForDriver_3C4CDC8B(SdifSdioFunction *function, uint32_t address,
			    uint32_t size, uint8_t *buffer, uint32_t flags);
int SceSdifForDriver_855C95E1(SdifSdioFunction *function,
			    SdifIoTransfer *transfer, uint32_t *index);

int SceSdifForDriver_01E8EB6C(SdifSdioContext *sdio, void *buffer, uint32_t size);
int SceSdifForDriver_0203ECDC(SdifContextPart *sd, uint32_t sector, uint32_t count);
int SceSdifForDriver_0F157F49(SdifSdioFunction *function, uint32_t index);
/* vitasdk: ksceSdifReadCmd56 */
int SceSdifForDriver_134E06C4(SdifMmcContext *part, void *buffer, int length);
int SceSdifForDriver_180E7395(SdifSdioFunction *function, void *buffer, uint32_t size);
int SceSdifForDriver_1847B18C(SdifSdioFunction *function);
/* vitasdk: ksceSdifInitializeMmcDevice */
int SceSdifForDriver_22C82E79(unsigned int index, SdifMmcContext **part);
int SceSdifForDriver_235AD556(void);
int SceSdifForDriver_23A4EF01(SdifMmcContext *part, void *cid);	/* vitasdk: ksceSdifGetCID */
int SceSdifForDriver_35BA9DF8(unsigned int index, SceUID callback);
int SceSdifForDriver_36A2B01B(unsigned int index);	/* vitasdk: ksceSdifGetCardInsertState1 */
int SceSdifForDriver_3B6AB29E(SdifSdioFunction *function, SdifIoTransfer *transfer, uint32_t *index);
int SceSdifForDriver_475D8E45(unsigned int index);
int SceSdifForDriver_53518827(SdifContextGlobal *context, uint32_t *status);
SdifSdioContext *SceSdifForDriver_53962379(unsigned int index);
int SceSdifForDriver_55BAEB2D(SdifSdioFunction *function);
int SceSdifForDriver_5BAC6E70(SdifSdioFunction *function, uint32_t offset, uint32_t size, uint8_t *buffer);
int SceSdifForDriver_5D65E66B(SdifSdioContext *sdio, uint32_t offset, uint32_t size, uint8_t *buffer);
int SceSdifForDriver_60642F49(SdifMmcContext *part, unsigned int sector, unsigned int count);
/* vitasdk: ksceSdifGetSdContextPartValidateSdio */
SdifSdioContext *SceSdifForDriver_6A8235FC(unsigned int index);
int SceSdifForDriver_6CC8E28D(unsigned int index);
int SceSdifForDriver_763F1075(SdifSdioContext *sdio);
int SceSdifForDriver_849E3216(SdifSdioFunction *function, uint32_t index, uint32_t reserved, SdifIoResult *result);
int SceSdifForDriver_AB0222F2(SdifSdioContext *sdio);
int SceSdifForDriver_AF702FE7(SdifContextPart *sd, uint32_t blockLength);
int SceSdifForDriver_B05EFF68(SdifSdioFunction *function, uint32_t index);
int SceSdifForDriver_B32776C7(SdifMmcContext *part, uint32_t blockLength);
/* vitasdk: ksceSdifReadSectorSd */
int SceSdifForDriver_B9593652(SdifContextPart *sd, uint32_t sector, void *buffer, uint32_t count);
/* vitasdk: ksceSdifInitializeSdDevice */
int SceSdifForDriver_C1271539(unsigned int index, SdifContextPart **part);
int SceSdifForDriver_D3C1E2B6(SdifSdioFunction *function, uint32_t offset, uint32_t size, uint8_t *buffer);
/* vitasdk: ksceSdifGetSdContextGlobal */
SdifContextGlobal *SceSdifForDriver_DC8F52F8(unsigned int index);
int SceSdifForDriver_DECE963B(SdifSdioContext *sdio, uint32_t offset, uint32_t size, uint8_t *buffer);
int SceSdifForDriver_E091BA2E(SdifContextPart *sd, uint32_t sector, uint32_t count, int *writeProtected);
int SceSdifForDriver_E5E5F42E(void);
int SceSdifForDriver_E80293EF(SdifSdioFunction *function);
int SceSdifForDriver_F1A24EDD(SdifSdioFunction *function);
int SceSdifForDriver_FD9E5CFA(unsigned int index);	/* vitasdk: ksceSdifGetCardInsertState2 */
int SceSdifForDriver_FE6F3E7B(SdifSdioFunction *function);
int _sceSdifInitDmaAlignmentBuffer(void);
int sceMmcReadSector(SdifMmcContext *part, unsigned int sector, void *buffer, unsigned int count);
int sceMmcSendGeneralCmdData(SdifMmcContext *part, void *buffer, int length);
int sceMmcWriteSectorInternal(SdifMmcContext *part, unsigned int sector, const void *buffer, unsigned int count);
int sceSdWriteSectorInternal(SdifContextPart *sd, uint32_t sector, const void *buffer, uint32_t count);
int sceSdifDisableSlowCardMode(void);
int sceSdifEnableSlowCardMode(void);
int sendWriteProtectStatus(SdifMmcContext *part, unsigned int sector, unsigned int count, int *writeProtected);
