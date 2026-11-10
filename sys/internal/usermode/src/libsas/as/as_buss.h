/* The BUSS class: the buffers racks mix into, per processor, buss and channel (as_buss.c). */
#ifndef SCE_SAS_AS_BUSS_H
#define SCE_SAS_AS_BUSS_H

#include "as.h"

/* the parameter block a BUSS locks (the high half of the block number) */
#define SCE_SAS_AS_BUSS_BLOCK_BUFFERS	0x81000000
#define SCE_SAS_AS_BUSS_BLOCK_MASK	0xffff0000

/* what a lock of the buffers gives */
typedef struct SceSasAsBussBuffers {
	SceUInt8 numBusses;
	SceUInt8 numChannels;
	SceUInt16 numGranularities;
	void **buffers;
} SceSasAsBussBuffers;

typedef struct SceSasAsBuss {
	SceSasAsHandle handle;
	volatile SceUInt32 lock;	/* bit 0 while the buffers are locked */
	volatile SceUInt32 updated;	/* bit 0 once they were written */
	SceSasAsLockedBlock locked;
	SceSasAsBussBuffers block;
	SceUInt32 numGranularities;
	SceUInt32 numChannels;
	SceUInt32 numBusses;
	SceUInt32 numProcessors;
	void **buffers;			/* [numProcessors][numBusses][numChannels] */
	SceSasAsMutex mutex;
	SceUInt8 reserved[0x400];
} SceSasAsBuss;

extern const SceSasAsClass sceSasAsBussClass;

void **sceSasAsBussGetBuffers(const SceSasAsBuss *buss, SceUInt32 *numBusses);
int sceSasAsBussLockBuffers(SceSasAsBuss *buss);
int sceSasAsBussUnlockBuffers(SceSasAsBuss *buss);

#endif
