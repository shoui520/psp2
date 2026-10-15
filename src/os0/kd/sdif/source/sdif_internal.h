/* SceSdif private host and command state. */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <sce/sdif.h>
#include <sce/sdif_regs.h>
#include <sce/fast_mutex.h>

typedef struct SdifHostInfo {
	volatile SdifHostRegisters *registers;
	uint32_t voltages;
	uint32_t baseClock;
	uint32_t busWidth;
	uint32_t clockFrequency;
	uint8_t timeoutControl;
	uint8_t specificationVersion;
	uint8_t vendorVersion;
	uint8_t reserved[9];
	uint64_t lastTransferTime;
} SdifHostInfo;

_Static_assert(sizeof(SdifHostInfo) == 0x28, "SdifHostInfo");
_Static_assert(offsetof(SdifHostInfo, lastTransferTime) == 0x20,
	       "SdifHostInfo.lastTransferTime");

typedef struct SdifDeviceInfo {
	uint32_t deviceType;
	uint32_t operationConditions;
	uint16_t relativeCardAddress;
	uint16_t reserved;
} SdifDeviceInfo;

_Static_assert(sizeof(SdifDeviceInfo) == 0xc, "SdifDeviceInfo");

typedef struct SdifCommandInfo {
	uint32_t flags;
	uint32_t command;
	uint32_t argument;
	void *buffer;
	uint16_t blockSize;
	uint16_t blockCount;
	uint32_t response[4];
	int errorCode;
} SdifCommandInfo;

_Static_assert(sizeof(SdifCommandInfo) == 0x28, "SdifCommandInfo");

typedef struct SdifAdmaDescriptor {
	uint16_t attributes;
	uint16_t length;
	uint32_t address;
} SdifAdmaDescriptor;


typedef struct SdifGeneralState {
	SceUID sysEventHandler;
	uint32_t portCount;
	uint32_t skipReset;
	uint8_t reserved[52];
} SdifGeneralState;
typedef struct SdifCommand {
	uint32_t size;
	uint32_t flags;
	uint32_t command;
	uint32_t argument;
	uint32_t response[4];
	void *buffer;
	uint16_t blockSize;
	uint16_t blockCount;
	int errorCode;
	uint8_t reservedCommand[20];
	SdifIoTransfer ioTransfer;
	struct SdifCommand *next;
	uint32_t state;
	uint32_t index;
	void (*callback)(struct SdifCommand *);
	SceUID eventFlag;
	struct SdifCommand *secondaryCommand;
	SdifContextGlobal *context;
	uint32_t descriptorPhysicalAddress;
	SdifAdmaDescriptor defaultDescriptors[16];
	SdifAdmaDescriptor *descriptors;
	SceKernelPARange physicalRanges[16];
	uint32_t defaultDescriptorPhysicalAddress;
	SceUID descriptorMemblock;
	uint8_t reservedTransfer[12];
	void *alignedBuffer;
	uint32_t alignedLength;
	uint32_t leadingLength;
	uint32_t trailingLength;
	uint32_t leadingPhysicalAddress;
	uint32_t trailingPhysicalAddress;
	uint64_t startTime;
	uint64_t endTime;
	uint8_t leadingBuffer[64];
	uint8_t trailingBuffer[64];
} SdifCommand;

typedef struct SdifCommandQueue {
	SdifCommand *head;
	SdifCommand *tail;
} SdifCommandQueue;

typedef struct SdifPortState {
	SdifCommandQueue freeCommands;
	SdifCommandQueue pendingCommands;
	uint32_t deviceType;
	void *part;
	uint32_t voltages;
	uint16_t relativeCardAddress;
	uint16_t reservedCard;
	uint32_t index;
	uint8_t commandComplete;
	uint8_t polling;
	uint8_t reservedMode[2];
	SdifCommand *currentCommand;
	SdifCommand *dataCommand;
	volatile SdifHostRegisters *registers;
	uint32_t interruptMask;
	uint8_t timeoutControl;
	uint8_t slowMode;
	uint8_t readRetries;
	uint8_t writeRetries;
	SceUID registerMemblock;
	SceUID eventFlag;
	struct ThreadmgrFastMutex mutex;
	SceUID descriptorMemblock;
	void *descriptorBuffer;
	uint32_t operationConditions;
	uint32_t busWidth;
	SceKernelSpinlock lock;
	uint64_t lastTransferTime;
	uint8_t interruptFunctions;
	uint8_t enabledFunctions;
	uint8_t pendingFunctions;
	uint8_t reservedInterrupt;
	uint32_t interruptNumber;
	uint32_t interruptArgument;
	uint32_t interruptDispatchPending;
	uint32_t removedDeviceType;
	void *removedPart;
	uint8_t powerControl;
	uint8_t reservedTail[7];
} SdifPortState;

struct SdifContextGlobal {
	SdifCommand commands[16];
	SdifPortState port;
};

_Static_assert(sizeof(SdifCommand) == 0x240, "SdifCommand");
_Static_assert(sizeof(SdifPortState) == 0xc0, "SdifPortState");
_Static_assert(sizeof(SdifContextGlobal) == 0x24c0, "SdifContextGlobal");
_Static_assert(offsetof(SdifPortState, lock) == 0x94, "SdifPortState.lock");

extern SdifContextGlobal gSdifContexts[3];
int sdifEnableFunctionInterrupt(SdifContextGlobal *context, int function);
int sdifDisableFunctionInterrupt(SdifContextGlobal *context, int function);
SceInt64 sceKernelGetSystemTimeWide(void);
int sceSdifAdmaKick(SdifCommand *command, uint16_t commandFlags);
SdifCommand *sdifCommandQueuePop(SdifCommandQueue *queue);
void _sceSdifCmdQueueDump(SdifCommandQueue *queue);
int freeAdmaDescriptorArea(SdifCommand *command);
int ksceKernelLockFastMutex(struct ThreadmgrFastMutex *mutex);
int ksceKernelUnlockFastMutex(struct ThreadmgrFastMutex *mutex);
int ksceKernelFinalizeFastMutex(struct ThreadmgrFastMutex *mutex);
int ScePervasiveForDriver_3D8A8712(int port);
int ScePervasiveForDriver_4E43299D(int port);
int ScePervasiveForDriver_043B33F5(int port, int value);
int ScePervasiveForDriver_9F8E589C(int port, int value);
int ScePervasiveForDriver_D326E15F(int port);
int ScePervasiveForDriver_3CF7A012(int port);
int ScePervasiveForDriver_453BD2EC(int port, int value);
int ksceKernelIsSomeBootMode(void);
SdifContextGlobal *ksceSdifGetSdContextGlobal(unsigned int index);
int sceSdifReset(SdifContextGlobal *context, int flags);
int sdifPowerOn(SdifContextGlobal *context, uint8_t powerControl);
int sdifIsCardInserted(SdifContextGlobal *context);
int sdifIsWriteProtected(SdifContextGlobal *context);
int sdifIdentifyCard(SdifContextGlobal *context);
int sdifReadCid(SdifContextGlobal *context, uint32_t *cid);
int sdifReadCsd(SdifContextGlobal *context, uint32_t *csd);
int sdifSelectCard(SdifContextGlobal *context, int select);
int sdifGetCardStatusError(uint32_t status);
int sdifSetClock(SdifContextGlobal *context, uint32_t frequency, int highSpeed);
int sdifSetBusWidth(SdifContextGlobal *context, int width);
int sdifStartMmcInitialization(unsigned int index, SdifMmcContext **part);
int sdifCompleteMmcInitialization(unsigned int index);
int ksceKernelInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
				 const char *name, int attr, void *opt);
int sceSdifInterrupt(int interrupt, SdifContextGlobal *context, unsigned int intrPriority);
void sdifStartFunctionInterrupt(SdifContextGlobal *context, int interrupt, unsigned int intrPriority);
void sdifDispatchFunctionInterrupt(SdifContextGlobal *context);
int ksceKernelTriggerSubIntr(int interrupt, int subInterrupt, void *argument);
void sdifCommandQueueInitialize(SdifCommandQueue *queue);
void sdifCommandQueuePush(SdifCommandQueue *queue, SdifCommand *command);
void sceSdifDumpAdmaInfo(SdifCommand *command);
int sceSdifWaitCmdCompletionCore(SdifCommand *command);
SdifCommand *sdifCommandQueueRemove(SdifCommandQueue *queue, unsigned int index);
SdifCommand *sdifCommandQueueFind(SdifCommandQueue *queue, unsigned int index);
void sdifCommandQueuePushFront(SdifCommandQueue *queue, SdifCommand *command);
void sdifFreeCommand(SdifContextGlobal *context, SdifCommand *command);
SdifCommand *sdifAllocateCommand(SdifContextGlobal *context);
void sdifCompleteFunctionMaskRead(SdifCommand *command);
int sdifPrepareSyncCommand(SdifContextGlobal *context, SdifCommand *command,
			  SdifCommand *secondary, int retries, int checkCard);
void sdifDelay(SdifContextGlobal *context, int microseconds);
int sdifSendApplicationCommand(SdifContextGlobal *context,
			       SdifCommand *secondary, int retries);
int sdifSendCheckedApplicationCommand(SdifContextGlobal *context,
				      SdifCommand *secondary, int retries);
int sdifQueueCommand(SdifContextGlobal *context, SdifCommand *command, int priority);
int setAdmaDescriptorsForAlignedData(SdifCommand *command);
int sdifResetCard(SdifContextGlobal *context);
SdifCommand *sdifAllocateCommandLocked(SdifContextGlobal *context);
void sdifFreeCommandLocked(SdifContextGlobal *context, SdifCommand *command);
int sdifSendCheckedSyncCommand(SdifContextGlobal *context, SdifCommand *command,
			       SdifCommand *secondary, int retries);
int sdifLockPort(SdifContextGlobal *context);
int sdifUnlockPort(SdifContextGlobal *context);
int SceSdifForDriver_53518827(SdifContextGlobal *context, uint32_t *status);
int sdifSetBlockLength(SdifContextGlobal *context, uint32_t blockLength);
