#ifndef MSIF_CONTEXT_H
#define MSIF_CONTEXT_H

#include "library.h"
#include <threadmgr/fast_mutex_object.h>

/* The driver clears this complete work area at initialization. */
struct MsifContext {
	struct MsifControllerRegisters *controller;
	SceUID controllerMemBlock;
	int started;
	SceUID eventFlag;
	struct ThreadmgrFastMutex mutex;
	SceUID dmaMemBlock;
	uint32_t insertionInterruptMask;
	SceKernelSpinlock lock;
	uint8_t dummyReadWorkaround;
	uint8_t slowCardMode;
	uint8_t readDelaySequence;
	uint8_t writeDelaySequence;
	uint8_t *alignedData;
	uint32_t alignedLength;
	uint32_t headLength;
	uint32_t tailLength;
	uintptr_t headPhysicalAddress;
	uintptr_t tailPhysicalAddress;
	struct MsifDmaDescriptor *dmaDescriptors;
	struct MsproalHandle *handle;
	SceKernelPARange physicalRanges[32];
	uint8_t headBuffer[64];
	uint8_t tailBuffer[64];
	uint64_t commandStartTime;
	uint64_t interruptTime;
	uintptr_t dmaDescriptorsPhysicalAddress;
	uint8_t paddingBeforeDummyRead[4];
	uint8_t dummyReadBuffer[512];
	uint64_t slowDelayPeriod;
	uint64_t slowDelaySelector;
	SceUID sleepEventFlag;
	SceUID sleepThread;
	int mediaAccessTickEnabled;
	int savedMediaAccessTickEnabled;
	uint8_t reserved[8];
};

_Static_assert(sizeof(struct MsifContext) == 0x440, "MSIF driver work area");
_Static_assert(offsetof(struct MsifContext, physicalRanges) == 0x80,
	"MSIF physical-range work area");
_Static_assert(offsetof(struct MsifContext, commandStartTime) == 0x200,
	"MSIF command timestamp");
_Static_assert(offsetof(struct MsifContext, dummyReadBuffer) == 0x218,
	"MSIF dummy-read sector");

extern struct MsifContext gMsifContext;

#endif
