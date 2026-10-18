#pragma once

#include <scetypes.h>

typedef struct GpuTraceInfo {
	SceUInt32 socRevision;
	SceUInt32 gpuClockFrequency[2];
	SceUInt32 gpuXbarClockFrequency;
	SceUInt32 armClockFrequency;
	SceUInt32 timebaseFrequency;
	SceUInt32 sysClockFrequency;
	SceUInt32 busClockFrequency;
	SceUInt32 wirelessState;
} GpuTraceInfo;

typedef struct GpuTraceCounters {
	SceUID id[8];
	char name[8][0x20];
} GpuTraceCounters;

int SceGpuEs4ForDriver_990338F6(GpuTraceInfo *info);
int SceGpuEs4ForDriver_C0C73C1B(GpuTraceCounters *names);
