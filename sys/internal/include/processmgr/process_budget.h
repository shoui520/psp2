/* Process budget information supplied to debugger clients. */
#pragma once

#include <sysmem/sysmem.h>

typedef struct SceKernelBudgetInfoForDebugger {
	SceSize size;
	SceUID budgetId;
	char name[0x20];
	SceUInt32 count;
	struct {
		char name[0x20];
		SceKernelPhyMemPartInfoForDebugger info;
	} part[5];
	SceUInt32 processCount;
	SceUID process[16];
} SceKernelBudgetInfoForDebugger;

_Static_assert(sizeof(SceKernelBudgetInfoForDebugger) == 0x480,
	       "SceKernelBudgetInfoForDebugger size");
_Static_assert(offsetof(SceKernelBudgetInfoForDebugger, processCount) == 0x43c &&
	       offsetof(SceKernelBudgetInfoForDebugger, process) == 0x440,
	       "SceKernelBudgetInfoForDebugger process list");

int sceKernelGetBudgetInfoForDebugger(SceUID budgetId, SceUInt32 level,
				    SceKernelBudgetInfoForDebugger *info);
int SceProcessmgrForKernel_23EF9D8E(SceUInt32 level,
	SceKernelBudgetInfoForDebugger *info, SceSize size, SceSize *count);
