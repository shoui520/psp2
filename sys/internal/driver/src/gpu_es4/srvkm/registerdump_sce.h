/* Complete SCE register-dump format, version 0x10003, in both TOOL originals. */
#ifndef GPU_REGISTERDUMP_SCE_H
#define GPU_REGISTERDUMP_SCE_H

#include <psp2common/types.h>

#define GPU_DUMP_MASTER_REGISTER_COUNT 266
#define GPU_DUMP_CORE_REGISTER_COUNT 399
#define GPU_DUMP_PERF_REGISTER_COUNT 109

typedef struct SceGpuRegisterDumpRegister {
	unsigned registerOffset;
	unsigned value;
} SceGpuRegisterDumpRegister;

typedef struct SceGpuRegisterDumpCoreRegister {
	unsigned registerOffset;
	unsigned values[4];
} SceGpuRegisterDumpCoreRegister;

typedef struct SceGpuRegisterDumpPerfRegister {
	unsigned selector;
	unsigned values[4];
} SceGpuRegisterDumpPerfRegister;

typedef struct SceGpuRegisterDumpContext {
	int processUid;
	char processName[20];
} SceGpuRegisterDumpContext;

typedef struct SceGpuRegisterDump {
	unsigned version;
	unsigned flags;
	SceDateTime localTime;
	unsigned socRevision;
	unsigned gpuClockSecond;
	unsigned gpuClockFirst;
	unsigned gpuXbarClock;
	unsigned coreCount;
	unsigned masterRegisterCount;
	unsigned coreRegisterCount;
	unsigned perfRegisterCount;
	SceGpuRegisterDumpRegister masterRegisters[GPU_DUMP_MASTER_REGISTER_COUNT];
	SceGpuRegisterDumpCoreRegister coreRegisters[GPU_DUMP_CORE_REGISTER_COUNT];
	SceGpuRegisterDumpPerfRegister perfRegisters[GPU_DUMP_PERF_REGISTER_COUNT];
	SceGpuRegisterDumpContext contexts[8];
	unsigned char contextIndices[5][8];
	/* Native TA/3D control snapshots; individual word names are not supplied. */
	unsigned frameNumSceneIndex[4];
} SceGpuRegisterDump;

_Static_assert(sizeof(SceGpuRegisterDump) == 0x3130, "native register dump");

SceGpuRegisterDump *GetRegisterDumpBuffer(void);

#endif
