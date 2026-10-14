/* Native register-dump region and the live fields immediately following it. */
#pragma once

#include <stddef.h>
#include "sgxinit_sce.h"
#include "system_sce.h"
#include "registerdump_sce.h"

#define GPU_DUMP_DATA_SIZE 0x3130
#define GPU_DUMP_LIVE_SIZE 0x317c
#define GPU_DUMP_SPAN_SIZE 0x3400

typedef struct GpuDumpState {
	SceGpuRegisterDump dump;
	unsigned registerListsUnsorted;
	unsigned deviceIndex;
	int registerMemblockUid;
	SGX_DEVICE_MAP platform;
	SYS_DATA *system;
} GpuDumpState;

typedef union GpuDumpRegion {
	GpuDumpState state;
	unsigned char bytes[GPU_DUMP_SPAN_SIZE];
} GpuDumpRegion;

/* PLACEHOLDER: the last 0x284 bytes are outside the original PT_LOAD memsz.
 * Their native page-tail contents are unknown. Verification must supply and
 * observe those bytes explicitly; zeroed emulation pages do not prove that
 * hardware contents are zero. The live fields above must retain dump aliases.
 */
_Static_assert(sizeof(GpuDumpState) == GPU_DUMP_LIVE_SIZE, "live dump region");
_Static_assert(offsetof(GpuDumpState, platform) == 0x313c, "platform overlap");
_Static_assert(offsetof(GpuDumpState, system) == 0x3178, "system overlap");
extern GpuDumpRegion gpuDumpRegion;

/* SCE's system pointer occupies the live word following the device map. */
#define gpsSysData (gpuDumpRegion.state.system)
