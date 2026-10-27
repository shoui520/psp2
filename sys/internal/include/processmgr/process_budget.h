/* SceProcessmgr: the budgets (memory partitions a process may use) and their information. */
#pragma once

#include <sysmem/sysmem.h>

/* sceKernelGetBudgetInfo's output */
/* SceKernelBudgetInfo.part[]: the partitions of a budget */
#define BUDGET_PART_MAIN		0
#define BUDGET_PART_CDIALOG		1
#define BUDGET_PART_PHYCONT		2
#define BUDGET_PART_CDRAM		5
#define BUDGET_PART_KERNEL_EXTRA	7
#define BUDGET_PART_MAX			8	/* 3, 4 and 6 are never filled */

typedef struct SceKernelBudgetInfo {
	SceSize size;				/* 0x88 */
	SceUInt32 count;
	SceKernelPhyMemPartInfoCore part[BUDGET_PART_MAX];
} SceKernelBudgetInfo;

_Static_assert(sizeof(SceKernelBudgetInfo) == 0x88, "SceKernelBudgetInfo size");

/* the options of a mini application budget (sceKernelCreateBudget) */
typedef struct ProcessBudgetOpt {
	SceSize size;				/* 0x18 */
	SceUInt32 flags;			/* 1: the CDRAM of another budget */
	SceSize mainSize;
	SceSize cdialogSize;
	SceSize phyContSize;
	SceUInt32 cdram;			/* a size, or with flags 1 the budget id */
} ProcessBudgetOpt;

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
