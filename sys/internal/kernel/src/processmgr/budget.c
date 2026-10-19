/*
 * budget.c - process budgets: the physical memory partitions a group of
 * processes (kernel, game, shell, mini applications) allocates from
 */

#include "processmgr_internal.h"
#include <processmgr/process_budget.h>

#define BUDGET_ID_CURRENT		0
#define BUDGET_ID_GAME			2
#define BUDGET_ID_SHELL			3

static SceClass g_budgetClass;
ProcessBudget *g_kernelBudget;
ProcessBudget *g_gameBudget;
static ProcessBudget *g_shellBudget;
static SceUID g_kernelBudgetId;
static SceUID g_gameBudgetId;
static SceUID g_shellBudgetId;

static void *g_kernelPart;			/* SceSysmemForKernel_0164D817 */
static void *g_phyContPart;			/* ScePhyMemPartPhyCont */
static void *g_sharedPart;			/* ScePhyMemPartShared */
static void *g_gameCDialogPart;			/* ScePhyMemPartGameCDialog */
static void *g_shellPart;			/* ScePhyMemPartShell */
static void *g_shellDummyPart;			/* ScePhyMemPartShellDummy */
static void *g_gamePart;			/* ScePhyMemPartGame */
static void *g_gameDummyPart;			/* ScePhyMemPartGameDummy */
static void *g_shellCdramPart;			/* ScePhyMemPartShellCdram */
static void *g_gameCdramPart;			/* ScePhyMemPartGameCdram */

/* the SceUIDProcBudgetClass constructor */
static int ProcessBudgetConstructor(ProcessBudget *budget)
{
	budget->mutex = -1;
	budget->uid = -1;
	budget->processCount = 0;
	budget->activeCount = 0;
	budget->mainSize = 0;
	memset(budget->process, 0, sizeof(budget->process));
	return 0;
}

static int SetKernelGamePhyMemPart(SceUID addressSpace)
{
	int ret;

	ret = SceSysmemForKernel_7DF07D60(addressSpace, 8, g_phyContPart);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(737);
	ret = SceSysmemForKernel_7DF07D60(addressSpace, 9, g_sharedPart);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(745);
	ret = SceSysmemForKernel_7DF07D60(addressSpace, 10, g_gameCDialogPart);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(753);
	return ret;
}

static void ShowPhyMemPartInfo(SceUIntPtr base, SceSize size)
{
	SCE_DBG_PRINTF_LEVEL(1, 0, 1055, "\tPhyMemPart: 0x%08x-0x%08x\n", base, base + size - 1);
}

/* the SceUIDProcBudgetClass destructor */
static int ProcessBudgetDestructor(ProcessBudget *budget)
{
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 1023, "\n");
	if (budget->type == PROCESS_TYPE_MINI_APP) {
		if (budget->mainPartition != NULL)
			sceKernelDeletePhyMemPart(budget->mainPartition);
		if (budget->cdialogPartition != NULL)
			sceKernelDeletePhyMemPart(budget->cdialogPartition);
		if (budget->phyContPartition != NULL)
			sceKernelDeletePhyMemPart(budget->phyContPartition);
		if (budget->cdramPartition != NULL)
			sceKernelDeletePhyMemPart(budget->cdramPartition);
	}
	if (budget->mutex > 0 && sceKernelDeleteMutex(budget->mutex) < 0)
		SCE_DBG_ASSERT_FAIL(1042);
	return 0;
}

static int CreateDefaultPhyMemPart(const char *name, SceUInt32 type, SceUIntPtr base, SceSize size,
				   void **partition)
{
	SceKernelPhyMemPartInfoCore info;
	int ret;

	ret = CreatePhyMemPartByPbase(name, type, base, size, partition);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1074,
			"sceKernelCreatePhyMemPart([%s], pbase 0x%08x, psize 0x%08x) failed 0x%08x\n",
			name, base, size, ret);
		return ret;
	}
	ret = sceKernelGetPhyMemPartInfoCore(*partition, &info);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(1081);
		return ret;
	}
	ShowPhyMemPartInfo(info.pbase, info.psize);
	return 0;
}

int BudgetLock(ProcessBudget *budget)
{
	return sceKernelLockMutex(budget->mutex, 1, NULL);
}

int BudgetUnlock(ProcessBudget *budget)
{
	return sceKernelUnlockMutex(budget->mutex, 1);
}

SceUID BudgetGetKernelBudgetId(void)
{
	return g_kernelBudgetId;
}

/* name unknown: delete a mini application budget */
int SceProcessmgrForKernel_459A223B(SceUID budgetId)
{
	ProcessBudget *budget;
	int ret, isDefault;

	ret = SceThreadmgrForDriver_332E127C();
	if (ret != 0x40 && ret != 0x80)
		return SCE_ERROR_ERRNO_ENOSYS;
	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	isDefault = budgetId == BUDGET_ID_CURRENT || budgetId == BUDGET_ID_GAME
		 || budgetId == BUDGET_ID_SHELL;
	if (isDefault || budgetId == g_kernelBudgetId || budgetId == g_gameBudgetId
	    || budgetId == g_shellBudgetId)
		return SCE_KERNEL_ERROR_INVALID_BUDGET_ID;
	if (SceSysmemForKernel_368F1C36(budgetId) != &g_budgetClass)
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	budget = SceSysmemForKernel_FA6C098C(budgetId);
	if (budget == NULL)
		return SCE_KERNEL_ERROR_INVALID_BUDGET_ID;
	ret = sceKernelLockMutex(budget->mutex, 1, (SceUInt32 *)isDefault);
	if (ret < 0)
		return ret;
	if (budget->processCount != 0) {
		sceKernelUnlockMutex(budget->mutex, 1);
		return SCE_KERNEL_ERROR_PROCESS_REMAINING;
	}
	return sceGUIDClose(budgetId);
}

int sceKernelResetGameBudget(void)
{
	int ret;

	ret = sceKernelResetPhyMemPart(g_gamePart);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 407, "sceKernelResetPhyMemPart failed 0x%08x\n", ret);
	return ret;
}

/* name unknown: the information of one partition of a budget */
static inline __attribute__((always_inline)) int GetPartInfoForDebugger(
	SceKernelBudgetInfoForDebugger *info, SceUInt32 n, void *partition)
{
	int ret;

	info->part[n].info.size = sizeof(info->part[n].info);
	info->part[n].info.freeBlockMax = 0;
	info->part[n].info.freeBlocks = NULL;
	ret = sceKernelGetPhyMemPartInfoForDebugger(partition, &info->part[n].info);
	if (ret < 0)
		return ret;
	strncpy(info->part[n].name, info->part[n].info.name, sizeof(info->part[n].name));
	return ret;
}

int sceKernelGetBudgetInfoForDebugger(SceUID budgetId, SceUInt32 level,
				      SceKernelBudgetInfoForDebugger *info)
{
	ProcessBudget *budget;
	const char *name;
	void *partition;
	SceUInt32 n;
	int ret, i;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = sceGUIDReferObjectWithClassLevel(budgetId, &g_budgetClass, level, &budget);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 431, "sceGUIDReferObjectWithClassLevel failed 0x%08x\n", ret);
		return ret;
	}
	info->budgetId = budgetId;
	switch (budget->type) {
	case PROCESS_TYPE_MINI_APP:
		name = SceSysmemForDriver_E655852F(budgetId);
		break;
	case PROCESS_TYPE_GAME:
		name = SceSysmemForDriver_E655852F(g_gameBudgetId);
		break;
	case PROCESS_TYPE_SHELL:
		name = SceSysmemForDriver_E655852F(g_shellBudgetId);
		break;
	case PROCESS_TYPE_KERNEL:
		name = SceSysmemForDriver_E655852F(g_kernelBudgetId);
		break;
	default:
		sceGUIDReleaseObject(budgetId);
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
	}
	strncpy(info->name, name, sizeof(info->name));
	n = 0;
	if (budget->mainPartition != NULL) {
		ret = GetPartInfoForDebugger(info, 0, budget->mainPartition);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 463,
				"sceKernelGetPhyMemPartInfoForDebugger failed. 0x%08x\n", ret);
			goto out;
		}
		n = 1;
	}
	if (budget->type == PROCESS_TYPE_KERNEL && (partition = (void *)SceSysmemForKernel_0B1B4ACA()) != NULL) {
		ret = GetPartInfoForDebugger(info, n, (void *)SceSysmemForKernel_0B1B4ACA());
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 476,
				"sceKernelGetPhyMemPartInfoForDebugger failed. 0x%08x\n", ret);
			goto out;
		}
		n++;
	}
	if (budget->cdialogPartition != NULL) {
		ret = GetPartInfoForDebugger(info, n, budget->cdialogPartition);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 488,
				"sceKernelGetPhyMemPartInfoForDebugger failed. 0x%08x\n", ret);
			goto out;
		}
		n++;
	}
	if (budget->phyContPartition != NULL) {
		ret = GetPartInfoForDebugger(info, n, budget->phyContPartition);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 500,
				"sceKernelGetPhyMemPartInfoForDebugger failed. 0x%08x\n", ret);
			goto out;
		}
		n++;
	}
	if (budget->cdramPartition != NULL) {
		ret = GetPartInfoForDebugger(info, n, budget->cdramPartition);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 512,
				"sceKernelGetPhyMemPartInfoForDebugger failed. 0x%08x\n", ret);
			goto out;
		}
		n++;
	}
	info->count = n;
	info->processCount = budget->processCount;
	for (i = 0; i != PROCESS_MAX; i++)
		info->process[i] = budget->process[i];
out:
	sceGUIDReleaseObject(budgetId);
	return ret;
}

int sceKernelGetBudgetInfo(SceUID budgetId, SceKernelBudgetInfo *info)
{
	SceProcessObject *process;
	ProcessBudget *budget;
	void *partition;
	SceUInt32 count;
	int referred, ret, i;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	if (budgetId == BUDGET_ID_CURRENT) {
		process = GetCurrentProcess();
		referred = 0;
		ret = 0;
		switch (process->type) {
		case PROCESS_TYPE_GAME:
			budget = g_gameBudget;
			break;
		case PROCESS_TYPE_SHELL:
			budget = g_shellBudget;
			break;
		case PROCESS_TYPE_MINI_APP:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 561,
				"SCE_KERNEL_PROCESS_CURRENT_PROCESS_BUDGET, but current is mini application process\n");
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		case PROCESS_TYPE_KERNEL:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 555,
				"SCE_KERNEL_PROCESS_CURRENT_PROCESS_BUDGET, but current is kernel process\n");
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		default:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 567, "Unknown process type 0x%08x\n", process->type);
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		}
	} else {
		ret = sceGUIDReferObjectWithClassLevel(budgetId, &g_budgetClass, 7, &budget);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 578, "sceGUIDReferObjectWithClassLevel failed 0x%08x\n",
					     ret);
			return ret;
		}
		referred = 1;
	}
	for (i = 0; i != BUDGET_PART_MAX; i++) {
		info->part[i].pbase = 0;
		info->part[i].psize = 0;
		info->part[i].freeSize = 0;
		info->part[i].freeUnitCount = 0;
	}
	count = 0;
	if (budget->mainPartition != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(budget->mainPartition, &info->part[BUDGET_PART_MAIN]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 596, "sceKernelGetPhyMemPartInfoCore failed. 0x%08x\n", ret);
			goto out;
		}
		count = 1;
	}
	if (budget->type == PROCESS_TYPE_KERNEL && (partition = (void *)SceSysmemForKernel_0B1B4ACA()) != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(partition, &info->part[BUDGET_PART_KERNEL_EXTRA]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 606, "sceKernelGetPhyMemPartInfoCore failed. 0x%08x\n", ret);
			goto out;
		}
		count++;
	}
	if (budget->cdialogPartition != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(budget->cdialogPartition, &info->part[BUDGET_PART_CDIALOG]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 615, "sceKernelGetPhyMemPartInfoCore failed. 0x%08x\n", ret);
			goto out;
		}
		count++;
	}
	if (budget->phyContPartition != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(budget->phyContPartition, &info->part[BUDGET_PART_PHYCONT]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 624, "sceKernelGetPhyMemPartInfoCore failed. 0x%08x\n", ret);
			goto out;
		}
		count++;
	}
	if (budget->cdramPartition != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(budget->cdramPartition, &info->part[BUDGET_PART_CDRAM]);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 633, "sceKernelGetPhyMemPartInfoCore failed. 0x%08x\n", ret);
			goto out;
		}
		count++;
	}
	info->count = count;
out:
	if (referred == 1)
		sceGUIDReleaseObject(budgetId);
	return ret;
}

/* name unknown: the debugger information of every budget */
int SceProcessmgrForKernel_23EF9D8E(SceUInt32 level, SceKernelBudgetInfoForDebugger *info,
				    SceSize size, SceSize *count)
{
	SceSize num, n;
	SceUID *ids;
	int ret, i;

	n = 0;
	if (count == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*count = 0;
	ret = sceGUIDGetUIDVectorByClass(&g_budgetClass, level, NULL, 0, &n);
	if (ret < 0)
		return ret;
	num = size / sizeof(*info);
	if (ret < (int)num)
		num = ret;
	if (num != 0) {
		ids = SceSysmemForKernel_85571907(num * sizeof(*ids));
		sceGUIDGetUIDVectorByClass(&g_budgetClass, level, ids, num, &n);
		for (i = 0; i < (int)n; i++, info++) {
			info->size = sizeof(*info);
			ret = sceKernelGetBudgetInfoForDebugger(ids[i], level, info);
			if (ret < 0) {
				SceSysmemForKernel_4233C16D(ids);
				return ret;
			}
		}
		SceSysmemForKernel_4233C16D(ids);
	}
	*count = n;
	return num;
}

int BudgetIDtoBudgetObj(SceUID budgetId, ProcessBudget **budget)
{
	SceProcessObject *process;

	if (budgetId == BUDGET_ID_GAME) {
		*budget = g_gameBudget;
		return 0;
	}
	if (budgetId == BUDGET_ID_SHELL) {
		*budget = g_shellBudget;
		return 0;
	}
	if (budgetId == BUDGET_ID_CURRENT) {
		process = GetCurrentProcess();
		switch (process->type) {
		case PROCESS_TYPE_GAME:
			*budget = g_gameBudget;
			return 0;
		case PROCESS_TYPE_SHELL:
			*budget = g_shellBudget;
			return 0;
		case PROCESS_TYPE_MINI_APP:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 819,
				"SCE_KERNEL_PROCESS_CURRENT_PROCESS_BUDGET, but current is mini application process\n");
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		case PROCESS_TYPE_KERNEL:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 813,
				"SCE_KERNEL_PROCESS_CURRENT_PROCESS_BUDGET, but current is kernel process\n");
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		default:
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 825, "Unknown process type 0x%08x\n", process->type);
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		}
	}
	if (SceSysmemForKernel_368F1C36(budgetId) == &g_budgetClass) {
		*budget = SceSysmemForKernel_FA6C098C(budgetId);
		if (*budget != NULL)
			return 0;
	}
	return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
}

/* uid -1: a new UID; else the fixed UID of a default budget */
static SceUID _CreateBudget(SceUID uid, const char *name, SceUInt32 type,
			    const ProcessBudgetOpt *opt, ProcessBudget **out)
{
	ProcessBudget *budget, *other;
	PhyMemPart *mainPart = NULL, *cdialogPart = NULL, *phyContPart = NULL, *cdramPart = NULL;
	char partName[0x20];
	SceSize mainSize;
	SceUID mutex;
	int ret;
	int fixedUid = uid != -1;

	if (type == PROCESS_TYPE_MINI_APP) {
		if (opt == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		if (opt->flags & 1) {
			ret = BudgetIDtoBudgetObj(opt->cdram, &other);
			if (ret < 0)
				return ret;
			cdramPart = other->cdramPartition;
			SceSysmemForKernel_6D563733(cdramPart);
		} else if (opt->cdram != 0) {
			snprintf(partName, 0x1f, "cdram: %s", name);
			ret = sceKernelCreatePhyMemPart(partName, 0x30000, opt->cdram, &cdramPart);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 173, "sceKernelCreatePhyMemPart(%s) failed. 0x%08x\n",
						     partName, ret);
				return ret;
			}
		}
		ret = SCE_KERNEL_ERROR_INVALID_BUDGET_SIZE;
		if (opt->mainSize == 0)
			goto delete_cdram;
		snprintf(partName, 0x1f, "ddrmain: %s", name);
		ret = sceKernelCreatePhyMemPart(partName, 0x10000, opt->mainSize, &mainPart);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 186, "sceKernelCreatePhyMemPart(%s) failed. 0x%08x\n",
					     partName, ret);
			goto delete_cdram;
		}
		mainSize = opt->mainSize;
		if (opt->cdialogSize != 0) {
			snprintf(partName, 0x1f, "ddrcdlg: %s", name);
			ret = sceKernelCreatePhyMemPart(partName, 0x10000, opt->cdialogSize, &cdialogPart);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 203,
						     "sceKernelCreatePhyMemPart(%s) failed. 0x%08x\n", partName, ret);
				goto delete_main;
			}
		}
		if (opt->phyContSize != 0) {
			snprintf(partName, 0x1f, "ddrpcnt: %s", name);
			ret = sceKernelCreatePhyMemPart(partName, 0x10001, opt->phyContSize, &phyContPart);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xb, 214,
						     "sceKernelCreatePhyMemPart(%s) failed. 0x%08x\n", partName, ret);
				goto delete_cdialog;
			}
		}
	} else if (type == PROCESS_TYPE_GAME) {
		if (g_gameBudgetId != 0)
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		mainSize = 0;
		mainPart = g_gamePart;
		cdialogPart = g_gameCDialogPart;
		phyContPart = g_phyContPart;
		cdramPart = g_gameCdramPart;
	} else if (type == PROCESS_TYPE_SHELL) {
		if (g_shellBudgetId != 0)
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		mainSize = 0;
		mainPart = g_shellPart;
		cdramPart = g_shellCdramPart;
	} else if (type == PROCESS_TYPE_KERNEL) {
		if (g_kernelBudgetId != 0)
			return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
		mainSize = 0;
		mainPart = g_kernelPart;
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 231, "Unknown process type 0x%08x\n", type);
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
	}

	if (uid == -1) {
		uid = sceGUIDKernelCreate(&g_budgetClass, name, &budget);
		if (uid < 0) {
			ret = uid;
			goto failed;
		}
	} else {
		budget = SceSysmemForKernel_85571907(sizeof(*budget));
		if (budget == NULL) {
			ret = SCE_ERROR_ERRNO_ENOMEM;
			goto failed;
		}
		ProcessBudgetConstructor(budget);
		ret = SceSysmemForKernel_5EDDEEB5(uid, &g_budgetClass, name, budget, 0x1551000);
		if (ret < 0) {
			SceSysmemForKernel_4233C16D(budget);
			goto failed;
		}
	}
	budget->type = type;
	budget->reserved = 0;
	budget->uid = uid;
	budget->mainSize = mainSize;
	budget->mainPartition = mainPart;
	budget->cdialogPartition = cdialogPart;
	budget->phyContPartition = phyContPart;
	budget->cdramPartition = cdramPart;
	mutex = sceKernelCreateMutex(name, 2, 0, NULL);
	if (mutex < 0) {
		if (fixedUid)
			SCE_PANIC(292);
		sceGUIDClose(uid);
		return mutex;
	}
	budget->mutex = mutex;
	if (out != NULL)
		*out = budget;
	return uid;

failed:
	if (type != PROCESS_TYPE_MINI_APP)
		return ret;
	if (phyContPart != NULL)
		sceKernelDeletePhyMemPart(phyContPart);
delete_cdialog:
	if (cdialogPart != NULL)
		sceKernelDeletePhyMemPart(cdialogPart);
delete_main:
	if (mainPart != NULL)
		sceKernelDeletePhyMemPart(mainPart);
delete_cdram:
	if (cdramPart != NULL)
		sceKernelDeletePhyMemPart(cdramPart);
	return ret;
}

/* name unknown: create a mini application budget */
SceUID SceProcessmgrForKernel_DC18FCEF(const char *name, SceUInt32 type, const ProcessBudgetOpt *opt)
{
	int ret;

	ret = SceThreadmgrForDriver_332E127C();
	if (ret != 0x40 && ret != 0x80)
		return SCE_ERROR_ERRNO_ENOSYS;
	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (opt == NULL || name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (opt->size != sizeof(*opt))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	return _CreateBudget(-1, name, type, opt, NULL);
}

/* give an address space the partitions of its budget */
int SetBudget(SceUInt32 type, SceUID addressSpace, ProcessBudget *budget)
{
	void *partition;
	int ret;

	ret = SceSysmemForKernel_7DF07D60(addressSpace, 6, g_kernelPart);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(863);
		return ret;
	}
	partition = (void *)SceSysmemForKernel_0B1B4ACA();
	if (partition != NULL) {
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 15, partition);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(872);
			return ret;
		}
	}
	SceSysmemForKernel_7DF07D60(addressSpace, 5, g_gamePart);
	switch (type) {
	case PROCESS_TYPE_MINI_APP:
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 11, budget->mainPartition);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(929);
			return ret;
		}
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 10, budget->cdialogPartition);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(937);
			return ret;
		}
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 8, budget->phyContPartition);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(945);
			return ret;
		}
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 12, g_shellCdramPart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(955);
			return ret;
		}
		if (SceSysmemForKernel_7DF07D60(addressSpace, 9, g_sharedPart) < 0)
			SCE_DBG_ASSERT_FAIL(965);
		return 0;
	case PROCESS_TYPE_GAME:
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 11, g_gamePart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(898);
			return ret;
		}
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 10, g_gameCDialogPart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(906);
			return ret;
		}
		if (SetKernelGamePhyMemPart(addressSpace) < 0)
			SCE_DBG_ASSERT_FAIL(912);
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 12, g_gameCdramPart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(919);
			return ret;
		}
		return 0;
	case PROCESS_TYPE_SHELL:
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 11, g_shellPart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(974);
			return ret;
		}
		ret = SceSysmemForKernel_7DF07D60(addressSpace, 12, g_shellCdramPart);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(983);
			return ret;
		}
		if (SceSysmemForKernel_7DF07D60(addressSpace, 9, g_sharedPart) < 0)
			SCE_DBG_ASSERT_FAIL(992);
		return 0;
	case PROCESS_TYPE_KERNEL:
		SCE_DBG_ASSERT_FAIL(890);
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
	default:
		SCE_DBG_ASSERT_FAIL(996);
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;
	}
}

static inline __attribute__((always_inline)) int initializeBudget(void)
{
	int ret;

	ret = sceUIDClassInitClass(&g_budgetClass, "SceUIDProcBudgetClass", SceSysmemForDriver_85336A1C(),
				    sizeof(ProcessBudget), (SceClassCallback)ProcessBudgetConstructor,
				    (SceClassCallback)ProcessBudgetDestructor);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1359, "Create budget class failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1360);
	}
	return ret;
}

static inline __attribute__((always_inline)) void SetKernelPhyMemPart(void)
{
	if (SceSysmemForKernel_7DF07D60(SCE_KERNEL_ADDRESS_SPACE_ID, 5, g_gamePart) < 0)
		SCE_DBG_ASSERT_FAIL(768);
	if (SetKernelGamePhyMemPart(SCE_KERNEL_ADDRESS_SPACE_ID) < 0)
		SCE_DBG_ASSERT_FAIL(775);
	if (SceSysmemForKernel_7DF07D60(SCE_KERNEL_ADDRESS_SPACE_ID, 12, g_gameCdramPart) < 0)
		SCE_DBG_ASSERT_FAIL(782);
	if (SceSysmemForKernel_7DF07D60(SCE_KERNEL_ADDRESS_SPACE_ID, 13, g_gameCdramPart) < 0)
		SCE_DBG_ASSERT_FAIL(789);
	if (SceSysmemForKernel_7DF07D60(SCE_KERNEL_ADDRESS_SPACE_ID, 14, g_shellCdramPart) < 0)
		SCE_DBG_ASSERT_FAIL(796);
}

/* the shell partition is larger on these models */
static inline __attribute__((always_inline)) SceSize ShellPartSize(void)
{
	SceUInt32 model = SceSysrootForDriver_4D98B15B();

	switch (model) {
	case 0x20100:
	case 0x20101:
	case 0x20110:
		return 0x3600000;
#if FW_DEBUG
	case 0x10100:
	case 0x10150:
	case 0x10200:
	case 0x10220:
		return 0x3200000;
	default:
		SceDebugForDriver_D9703808(1, 0xb, SCE_DBGINFO_FUNC(1171, "CreateDefaultBudget"),
					      "Unknown model 0x%08x\n", model);
		return 0x3200000;
#else
	default:
		return 0x3200000;
#endif
	}
}

int CreateDefaultBudget(void)
{
	SceKernelPhyMemPartInfoCore info;
	SceUIntPtr base;
	SceSize size;
	void *partition;
	int ret;

	ret = initializeBudget();
	if (ret < 0)
		return ret;
	SceSysmemForKernel_02DAD732();
	g_kernelPart = (void *)SceSysmemForKernel_0164D817();
	ret = sceKernelGetPhyMemPartInfoCore(g_kernelPart, &info);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(1106);
		return 0;
	}
	ShowPhyMemPartInfo(info.pbase, info.psize);
	base = info.pbase + info.psize;
	ret = CreateDefaultPhyMemPart("ScePhyMemPartPhyCont", 0x10005, base, 0x1a00000, &g_phyContPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1121, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1122);
		return 0;
	}
	ret = CreateDefaultPhyMemPart("ScePhyMemPartShared", 0x10002, base + 0x1a00000, 0x800000,
				      &g_sharedPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1135, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1136);
		return 0;
	}
	ret = CreateDefaultPhyMemPart("ScePhyMemPartGameCDialog", 0x10064, base + 0x2200000, 0x900000,
				      &g_gameCDialogPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1151, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1152);
		return 0;
	}
	size = ShellPartSize();
	ret = CreateDefaultPhyMemPart("ScePhyMemPartShell", 0x10002, base + 0x2b00000, size,
				      &g_shellPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1183,
			"CreateDefaultPhyMemPart(pbase 0x%08x, psize 0x%08x) failed 0x%08x\n",
			base + 0x2b00000, size, ret);
		SCE_DBG_ASSERT_FAIL(1184);
		return 0;
	}
	if (sceKernelCheckDipsw(DIPSW_GROW_SHELL_PHYMEMPART) != 0) {
		ret = sceKernelGrowPhyMemPartByPbase(g_shellPart, 0x78000000, 0x3000000);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1196, "sceKernelGrowPhyMemPartByPbase failed 0x%08x\n",
					     ret);
			SCE_DBG_ASSERT_FAIL(1197);
			return 0;
		}
		ret = sceKernelGrowPhyMemPartByPbase(g_sharedPart, 0x7b000000, 0x5000000);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1206, "sceKernelGrowPhyMemPartByPbase failed 0x%08x\n",
					     ret);
			SCE_DBG_ASSERT_FAIL(1207);
			return 0;
		}
	} else if (sceKernelCheckDipsw(DIPSW_NO_SHELL_DUMMY_PHYMEMPART) == 0 &&
		   sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART) != 0) {
		ret = CreateDefaultPhyMemPart("ScePhyMemPartShellDummy", 0x10002, 0x78000000, 0x8000000,
					      &g_shellDummyPart);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1224, "sceKernelGrowPhyMemPartByPbase failed 0x%08x\n",
					     ret);
			return 0;
		}
	}
	ret = CreateDefaultPhyMemPart("ScePhyMemPartGame", 0x10004, 0x50000000,
				      sceKernelCheckDipsw(DIPSW_MEMORY_SIZE) != 0 ?
				      0x20000000 : 0x10000000,
				      &g_gamePart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1248, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1249);
		return 0;
	}
	if (sceKernelCheckDipsw(DIPSW_MEMORY_SIZE) == 0) {
		ret = CreateDefaultPhyMemPart("ScePhyMemPartGameDummy", 0x10002, 0x60000000, 0x10000000,
					      &g_gameDummyPart);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1263, "sceKernelCreatePhyMemPart failed 0x%08x\n", ret);
	}
	partition = (void *)SceSysmemForKernel_0B1B4ACA();
	if (partition != NULL) {
		ret = sceKernelGetPhyMemPartInfoCore(partition, &info);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(1272);
			return 0;
		}
		ShowPhyMemPartInfo(info.pbase, info.psize);
	}
	SceSysmemForKernel_3753C584(g_gamePart);
	ret = CreateDefaultPhyMemPart("ScePhyMemPartShellCdram", 0x30002, 0x27000000, 0x1000000,
				      &g_shellCdramPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1289, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1290);
		return 0;
	}
	ret = CreateDefaultPhyMemPart("ScePhyMemPartGameCdram", 0x30004, 0x20000000, 0x7000000,
				      &g_gameCdramPart);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1302, "CreateDefaultPhyMemPart failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1303);
		return 0;
	}
	SetKernelPhyMemPart();
	g_kernelBudgetId = _CreateBudget(0x10059, "SceProcessmgrKernelBudget", PROCESS_TYPE_KERNEL, NULL,
					 &g_kernelBudget);
	if (g_kernelBudgetId < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1318, "Create Kernel budget failed. 0x%08x\n",
				     g_kernelBudgetId);
		SCE_DBG_ASSERT_FAIL(1319);
		return 0;
	}
	g_gameBudgetId = _CreateBudget(0x1005d, "SceProcessmgrFullGameBudget", PROCESS_TYPE_GAME, NULL,
				       &g_gameBudget);
	if (g_gameBudgetId < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1328, "Create GAME budget failed. 0x%08x\n", g_gameBudgetId);
		SCE_DBG_ASSERT_FAIL(1329);
		return 0;
	}
	g_shellBudgetId = _CreateBudget(0x1005b, "SceProcessmgrShellBudget", PROCESS_TYPE_SHELL, NULL,
					&g_shellBudget);
	if (g_shellBudgetId < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1338, "Create SHELL budget failed. 0x%08x\n", g_shellBudgetId);
		SCE_DBG_ASSERT_FAIL(1339);
	}
	return 0;
}

int sceKernelIsGameBudget(void)
{
	ThreadCB *cb = getCurrentThreadCB();

	if (cb == NULL)
		return 0;
	return cb->process->budgetId == g_gameBudgetId;
}

int sceKernelIsCDialogAvailable(void)
{
	ThreadCB *cb;
	ProcessBudget *budget;
	SceUInt32 state;
	int ret;

	state = ProcessSyscallEnter();
	cb = getCurrentThreadCB();
	ret = 0;
	if (cb != NULL && (ret = BudgetIDtoBudgetObj(cb->process->budgetId, &budget)) >= 0
	    && (ret = sceKernelLockMutex(budget->mutex, 1, NULL)) >= 0) {
		ret = budget->cdialogPartition != NULL;
		sceKernelUnlockMutex(budget->mutex, 1);
	}
	ProcessSyscallLeave(state);
	return ret;
}

int sceKernelGrowBudget(SceUID budgetId, int index, SceSize size)
{
	ProcessBudget *budget;
	void *partition;
	int ret;

	if (size == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 1449, "budgetId 0x%08x, idx %d, size 0x%x\n", budgetId, index, size);
	ret = BudgetIDtoBudgetObj(budgetId, &budget);
	if (ret < 0)
		return ret;
	ret = sceKernelLockMutex(budget->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	switch (index) {
	case 0:
		partition = budget->mainPartition;
		break;
	case 1:
		partition = budget->cdialogPartition;
		break;
	case 2:
		partition = budget->phyContPartition;
		break;
	case 5:
		partition = budget->cdramPartition;
		break;
	default:
		sceKernelUnlockMutex(budget->mutex, 1);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0xb, 1479, "pPhyMemPart %p\n", partition);
	ret = SceSysmemForKernel_5B434019(partition, size);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 1483, "grow partition failed. 0x%08x\n", ret);
		sceKernelUnlockMutex(budget->mutex, 1);
		return ret;
	}
	sceKernelUnlockMutex(budget->mutex, 1);
	return 0;
}

/* name unknown: the default physical memory partitions */
void *SceProcessmgrForKernel_923AF3CC(int kind)
{
	switch (kind) {
	case 1:
		return (void *)SceSysmemForKernel_0164D817();
	case 2:
		return (void *)SceSysmemForKernel_0B1B4ACA();
	case 3:
		return (void *)SceSysmemForKernel_62800A6B();
	case 4:
		return g_phyContPart;
	case 5:
		return g_sharedPart;
	case 6:
		return g_gameCDialogPart;
	case 7:
		return g_shellPart;
	default:
		return NULL;
	}
}
