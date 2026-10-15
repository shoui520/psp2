#ifndef MSIF_LIBRARY_H
#define MSIF_LIBRARY_H

#include <stdint.h>
#include <stddef.h>
#include <sce/fat_boot.h>

/* The Memory Stick interface's 4 KiB register window (SceMsif) */
#define MSIF_REG_BASE		0xe0900000
#define MSIF_REG_SIZE		0x1000

struct MsifDmaChannelRegisters {
	volatile uint32_t dataAddress;
	volatile uint32_t nextDescriptorAddress;
	volatile uint32_t transferControl;
	volatile uint32_t diagnostic;
};

struct MsifDmaDescriptor {
	uint32_t dataAddress;
	uint32_t nextDescriptorAddress;
	uint32_t transferControl;
};

union MsproalFifoData {
	uint32_t words[2];
	uint8_t bytes[8];
	struct {
		uint8_t command;
		uint8_t count[2];
		uint8_t sector[4];
		uint8_t reserved;
	} extendedCommand;
	struct {
		uint8_t readAddress;
		uint8_t readCount;
		uint8_t writeAddress;
		uint8_t writeCount;
		uint8_t reserved[4];
	} registerWindow;
};

/* Hardware window starts at the transfer engine, after the sequencer bank. */
struct MsifRegisterWindow {
	union {
		struct {
			volatile uint16_t transferCommand;
			volatile uint16_t commandDiagnostic;
		};
		volatile uint32_t commandSnapshot;
	};
	volatile uint32_t transferFifo;
	union {
		struct {
			volatile uint16_t interruptStatus;
			volatile uint16_t interruptDiagnostic;
		};
		volatile uint32_t interruptSnapshot;
	};
	union {
		struct {
			volatile uint16_t transferControl;
			volatile uint16_t controlDiagnostic;
		};
		volatile uint32_t controlSnapshot;
	};
	volatile uint32_t diagnostic[2];
	uint8_t reserved[56];
	struct MsifDmaChannelRegisters dma[2];
	volatile uint32_t powerSupply;
};

struct MsifSequencerRegisters {
	volatile uint16_t control;
	volatile uint16_t command;
	volatile uint16_t reset;
	volatile uint16_t resetDiagnostic;
	volatile uint16_t result;
	volatile uint16_t resultDiagnostic;
	volatile uint32_t configuration;
	volatile uint16_t sectorCount;
	volatile uint16_t countDiagnostic;
	volatile uint16_t diagnostic;
	volatile uint16_t commandError;
	volatile uint32_t diagnosticWords[2];
	union {
		volatile uint32_t instructionDiagnostic;
		volatile uint16_t instructionFifo;
	};
	union {
		volatile uint32_t diagnostic;
		volatile uint8_t commandFifo;
	} commandPort;
	volatile uint32_t mediaStatus;
	volatile uint32_t finalDiagnostic;
};

struct MsifControllerRegisters {
	union {
		struct MsifSequencerRegisters sequencer;
		volatile uint32_t sequencerSnapshot[12];
	};
	struct MsifRegisterWindow transfer;
};

#define MSPROAL_CONTROLLER(handle) \
	((struct MsifControllerRegisters *)((uint8_t *)(handle)->registers - \
	 offsetof(struct MsifControllerRegisters, transfer)))

struct MsproalMediaRegisters {
	uint8_t reserved[2];
	uint8_t status0;
	uint8_t status1;
	uint8_t type;
	uint8_t category;
	uint8_t mediaClass;
	uint8_t subtype;
};

/* Multi-byte fields retain the medium's big-endian wire representation. */
struct MsproalSystemInformation {
	uint8_t mediaClass;
	uint8_t reservedInitial;
	uint8_t blockSize[2];
	uint8_t blockCount[2];
	union {
		struct {
			uint8_t userBlockCount[2];
			uint8_t pageSize[2];
		} pro;
		uint8_t xcUserBlockCount[4];
	} geometry;
	uint8_t reservedBeforeAssembly[2];
	uint8_t assemblyDate[8];
	uint8_t serialNumber[4];
	uint8_t assemblyMaker;
	uint8_t assemblyModel[3];
	uint8_t memoryMaker[2];
	uint8_t memoryModel[2];
	uint8_t xcBlockSize[4];
	uint8_t vcc;
	uint8_t vpp;
	uint8_t controllerNumber[2];
	uint8_t controllerFunction[2];
	uint8_t startSector[2];
	uint8_t unitSize[2];
	uint8_t mediaSubclass;
	uint8_t reservedBeforeInterface[4];
	uint8_t interfaceType;
	uint8_t controllerCode[2];
	uint8_t formatType;
	uint8_t reservedBeforeDevice;
	uint8_t deviceType;
	uint8_t reservedBeforeId[7];
	uint8_t id[16];
	uint8_t reservedTail[16];
};

struct MsifMediaCertificate {
	uint8_t version[4];
	uint8_t r[28];
	uint8_t s[28];
	uint8_t serviceData[452];
};

struct MsproalAttributeEntry {
	uint8_t address[4];
	uint8_t size[4];
	uint8_t id;
	uint8_t reserved[3];
};

struct MsproalAttributeDirectory {
	uint8_t signature[2];
	uint8_t version[2];
	uint8_t count;
	uint8_t reserved[11];
	struct MsproalAttributeEntry entries[12];
	uint8_t cachedRemainder[256];
};

struct MsproalDosPartitionEntry {
	uint8_t bootIndicator;
	uint8_t startingChs[3];
	uint8_t type;
	uint8_t endingChs[3];
	uint8_t startingSector[4];
	uint8_t sectorCount[4];
};

struct MsproalDosMasterBootRecord {
	union {
		uint8_t bootstrap[446];
		struct {
			uint8_t initialBootstrap[97];
			uint8_t alternatePartitionSector[4];
			uint8_t remainingBootstrap[345];
		} alternatePartition;
	};
	struct MsproalDosPartitionEntry partitions[4];
	uint8_t signature[2];
};

struct MsproalFormatProgress {
	uint32_t completionCounters[2];
	uint8_t deviceData[24];
};

struct MsproalFormatWorkspace {
	struct MsproalFormatProgress progress;
	uint8_t workArea[480];
	struct MsproalDosMasterBootRecord masterBootRecord;
	uint8_t remainingWorkArea[1536];
};

_Static_assert(sizeof(struct MsproalDosPartitionEntry) == 16,
	"MsproalDosPartitionEntry");
_Static_assert(sizeof(struct MsproalDosMasterBootRecord) == 512,
	"MsproalDosMasterBootRecord");
_Static_assert(sizeof(struct MsproalFormatWorkspace) == 2560,
	"MsproalFormatWorkspace");

struct MsproalHandle {
	union {
		uint8_t workBuffer[2560];
		struct MsifMediaCertificate certificate;
		struct MsproalFormatWorkspace formatWorkspace;
	};
	struct MsproalAttributeDirectory attributeDirectory;
	struct MsproalSystemInformation systemInformation;
	struct MsifRegisterWindow *registers;
	uint32_t partitionStart;
	uint32_t mediaType;
	uint32_t mountFlags;
	volatile uint32_t interruptFlags; /* Updated by the controller interrupt. */
	int busMode;
	int requestedBusMode;
	int powerClass;
	int writeEnabled;
	int writeProtected;
};

struct MsproalMediaInformation {
	uint32_t mediaType;
	uint32_t totalSectors;
	uint32_t userSectors;
	uint32_t sectorsPerBlock;
	int writeProtected;
};

struct MsproalOperations {
	int (*readAttributes)(struct MsproalHandle *handle, uint32_t sector,
		int count, const struct MsifDmaDescriptor *transfer);
	int (*read)(struct MsproalHandle *handle, uint32_t sector,
		int count, const struct MsifDmaDescriptor *transfer);
	int (*write)(struct MsproalHandle *handle, uint32_t sector,
		int count, const struct MsifDmaDescriptor *transfer);
	int (*getModelName)(struct MsproalHandle *handle, char *name);
	int (*setPowerClass)(struct MsproalHandle *handle, int powerClass);
	int (*format)(struct MsproalHandle *handle, int mode);
	int (*mount)(struct MsproalHandle *handle, int busMode);
	int (*sleep)(struct MsproalHandle *handle);
	int (*stopAndCheckStatus)(struct MsproalHandle *handle);
	int (*reset)(struct MsproalHandle *handle);
	int (*stop)(struct MsproalHandle *handle);
};

struct MsproalErrorRecord {
	int operation;
	int result;
	uint32_t sequence;
	uint32_t registerSnapshot[23];
	uint32_t reserved[6];
};

struct MsproalErrorLog {
	struct MsproalErrorRecord *records;
	uint32_t index;
	uint16_t capacity;
};

extern struct MsproalHandle *gMsproalHandle;
extern struct MsproalErrorLog gMsproalErrorLog;
extern const struct MsproalOperations *gMsproalOperations;
extern const struct MsproalOperations gMsproalDefaultOperations;
extern const struct MsproalOperations gMsproalProOperations;
extern uint64_t gMsproalTimerStartTime;
extern uint64_t gMsproalTimerDuration;

int MsproalMountPro(struct MsproalHandle *handle, int busMode);
int MsproalReadPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer);
int MsproalWritePro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer);
int MsproalReadSystemInformationPro(struct MsproalHandle *handle);
int MsproalReadExfatPartitionPro(struct MsproalHandle *handle,
	uint32_t *sectorCount);
int MsproalReadFatPartitionPro(struct MsproalHandle *handle);
int MsproalResetPro(struct MsproalHandle *handle);
int MsproalReadSectorsPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer);
int MsproalFormatPro(struct MsproalHandle *handle, int mode);
int MsproalRecordError(struct MsproalHandle *handle, int operation,
	int result, uint32_t *index, struct MsproalErrorRecord *record);
int MsproalUserGetRegisters(struct MsifRegisterWindow **registers);
void msproal_user_clear_flg(uint32_t preserveMask);
int MsproalUserSetBusClock(int busMode);
int MsproalResetController(struct MsproalHandle *handle);
int MsproalUpdatePowerSupply(struct MsproalHandle *handle,
	uint32_t value, uint32_t preserveMask);
void *MsproalUserCopy(void *destination, const void *source,
	unsigned int length);
void *MsproalUserFill(void *destination, int value, unsigned int length);
int MsproalInitializeHandle(struct MsproalHandle *handle,
	struct MsifRegisterWindow *registers);
int MsproalIdentifyMedia(struct MsproalHandle *handle, uint32_t *mediaType,
	int *writeEnabled, int *writeProtected);
int MsproalReadMediaRegisters(struct MsproalHandle *handle, int address,
	int count, void *registers);
struct MsproalHandle *MsproalClearDmaChannels(struct MsproalHandle *handle);
int MsproalSetDmaChannel(struct MsproalHandle *handle,
	const struct MsifDmaDescriptor *descriptor, int channel);
int MsproalSetRegisterWindow(struct MsproalHandle *handle,
	int readAddress, int readCount, int writeAddress, int writeCount);
int MsproalWriteRegisters(struct MsproalHandle *handle, int count,
	const void *registers);
int MsproalSetHostBusMode(struct MsproalHandle *handle, int busMode);
int MsproalSwitchBusMode(struct MsproalHandle *handle, int busMode);
int MsproalSetModeAndWait(struct MsproalHandle *handle, int mode);
int MsproalUserSetMode(int mode);
int MsproalUserWaitMilliseconds(int milliseconds);
int MsproalSendExtendedCommand(struct MsproalHandle *handle,
	int command, uint32_t sector, int count);
int MsproalWaitTransfer(struct MsproalHandle *handle, int timeout, int mode);
int MsproalGetInterrupt(struct MsproalHandle *handle, uint8_t *interrupt);
int MsproalExecuteExtendedCommand(struct MsproalHandle *handle,
	int command, uint32_t sector, int count, int timeout);
int MsproalWaitCommand(struct MsproalHandle *handle, int timeout,
	uint8_t *interrupt);
int MsproalReadRegisters(struct MsproalHandle *handle, int count, void *registers);
int MsproalResetSequencer(struct MsproalHandle *handle);
int MsproalExecuteSequencer(struct MsproalHandle *handle, int command);
int MsproalUserPollEventFlag(uint32_t bits);
int MsproalUserWaitEventFlag(uint32_t bits, int milliseconds);
int MsproalLoadSequencerProgram(struct MsproalHandle *handle,
	int address, int count, const uint16_t *program);
int64_t MsproalTimerStart(int milliseconds);
int MsproalTimerCheck(void);
void MsproalTimerStop(void);
int MsproalResetTransferEngine(struct MsproalHandle *handle);
int MsproalSendCommand(struct MsproalHandle *handle, int command);
int MsproalExecuteCommand(struct MsproalHandle *handle, int command, int timeout);
int MsproalWriteMediaRegisters(struct MsproalHandle *handle, int address,
	int count, const void *registers);
int MsproalStopPro(struct MsproalHandle *handle);
int MsproalStopAndCheckStatusPro(struct MsproalHandle *handle);
int MsproalSetPowerClassPro(struct MsproalHandle *handle, int powerClass);
int MsproalSleepPro(struct MsproalHandle *handle);
int MsproalCommandWriteShortData(struct MsproalHandle *handle, int command,
	int count, const void *data, int timeout);
int MsproalCommandReadShortData(struct MsproalHandle *handle, int command,
	int count, void *data, int timeout);
int MsproalCommandReadLongData(struct MsproalHandle *handle, int command,
	void *data, int timeout);
int MsproalWriteShortData(struct MsproalHandle *handle, int count,
	const void *data);
int MsproalReadShortData(struct MsproalHandle *handle, int count, void *data);
int MsproalReadLongData(struct MsproalHandle *handle, void *data);
int MsproalWaitTransferComplete(struct MsproalHandle *handle, int timeout);
int MsproalReadFifoPair(struct MsproalHandle *handle, uint32_t *data);
int MsproalReadFifo(struct MsproalHandle *handle, int count, void *data);
int MsproalWriteFifoPair(struct MsproalHandle *handle, const uint32_t *data);
int MsproalWriteFifo(struct MsproalHandle *handle, int count, const void *data);

int MsproalUserInitialize(void);
void MsproalUserCancelTransfer(struct MsproalHandle *handle);
void MsproalUserCompleteTransfer(struct MsproalHandle *handle);
void MsproalUserInvalidateCache(void *address, unsigned int length);
void msproal_user_virt_to_bus(const void *address, uintptr_t *physicalAddress);
int MsproalReadAttributesPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer);
int MsproalGetModelNamePro(struct MsproalHandle *handle, char *name);
struct MsproalErrorRecord **MsproalUserGetErrorLogBuffer(
	struct MsproalErrorRecord **records, uint16_t *capacity);
void *MsproalInitializeErrorLog(struct MsproalHandle *handle,
	unsigned int capacity, uint32_t *index,
	struct MsproalErrorRecord *records);

#endif
