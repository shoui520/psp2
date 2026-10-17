/*
 * syscallcb.c - the system call table: kernel libraries exported to user
 * mode get a randomly placed range of syscall numbers
 */

#include "modulemgr_internal.h"
#include <excpmgr/excpmgr_cpu.h>

#define SYSCALLCB_MAX_SYSCALLNUM	0x1000

/* DACR with the domain of the (read-only) syscall table opened for writing */
#define SYSCALLCB_DACR_WRITABLE		0x17450000

/* one traced syscall (tool mode) */
typedef struct SyscallTraceInfo {
	void *func;
	SceNID libnid;
	SceNID funcnid;
	SceUInt32 index;		/* in the library */
	SceUInt32 enterArg;		/* LibEntCB.enterArg */
	SceUInt32 returnArg;
} SyscallTraceInfo;

static void *g_traceEntry;		/* called instead of traced syscalls */
static SyscallTraceInfo *g_traceInfo;

static struct {
	void **table;
	SceUInt32 low;			/* the free range of syscall numbers */
	SceUInt32 high;
} g_syscall;

static inline void SyscallCBWrite(SceUInt32 index, void *func)
{
	SceUInt32 dacr;

	dacr = readDacr();
	writeDacr(SYSCALLCB_DACR_WRITABLE);
	g_syscall.table[index] = func;
	writeDacr(dacr);
}

int func_noentry(void)
{
	SCE_DBG_PRINTF_LEVEL(1, 0, 55, "invalid syscall num\n");
	return SCE_KERNEL_ERROR_SYSCALL_NOENTRY;
}

int SyscallCBInit(void)
{
	SceUInt32 dacr;
	void **p, **end;
	int ret;

	ret = sceKernelAllocSystemCallTable(SYSCALLCB_MAX_SYSCALLNUM, (void **)&g_syscall.table);
	if (ret < 0)
		return ret;
	dacr = readDacr();
	writeDacr(SYSCALLCB_DACR_WRITABLE);
	end = g_syscall.table + SYSCALLCB_MAX_SYSCALLNUM;
	for (p = g_syscall.table; p != end; p++)
		*p = func_noentry;
	writeDacr(dacr);
	SceKernelUtilsForDriver_875B2A1C(&g_syscall.low, sizeof(g_syscall.low));
	g_syscall.low &= SYSCALLCB_MAX_SYSCALLNUM - 1;
	g_syscall.high = g_syscall.low + SYSCALLCB_MAX_SYSCALLNUM;
	return 0;
}

/* take count numbers off a random end of the free range */
int SyscallCBAssignEntry(SceUInt32 count)
{
	SceUInt32 low = g_syscall.low, high = g_syscall.high;
	SceUInt32 rnd, skip, n;

	SceKernelUtilsForDriver_875B2A1C(&rnd, sizeof(rnd));
	skip = rnd % 5;
	n = count + skip;
	if (n > high - low) {
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 115,
				     "no room. Please increase SYSCALLCB_MAX_SYSCALLNUM !\n");
		return SCE_KERNEL_ERROR_MODULEMGR_SYSCALL_REG;
	}
	if ((rnd & 1) != 0) {
		g_syscall.low = n + low;
		return skip + low;
	}
	g_syscall.high = high - n;
	return high - n;
}

/* name unknown: enter function index of a syscall library into the table */
void SyscallCBSetEntry(LibEntCB *ent, SceUInt32 index)
{
	SceModuleExports *exp = ent->exports;
	SceUInt32 num = (ent->syscallIndex + index) & (SYSCALLCB_MAX_SYSCALLNUM - 1);
	void *func = exp->entrytable[index];
	SyscallTraceInfo *trace;

	if (g_syscall.table[num] == func)
		return;
	SyscallCBWrite(num, func);
	if (g_traceInfo == NULL)
		return;
	if ((ent->flags & (LIBENT_FLAG_TRACE_TARGET | LIBENT_FLAG_TRACING)) ==
	    (LIBENT_FLAG_TRACE_TARGET | LIBENT_FLAG_TRACING))
		SyscallCBWrite(num, g_traceEntry);
	trace = &g_traceInfo[num];
	trace->func = func;
	trace->libnid = exp->libnid;
	trace->index = index;
	trace->enterArg = ent->enterArg;
	trace->returnArg = ent->returnArg;
	trace->funcnid = exp->nidtable[index];
}

/* name unknown */
void SyscallCBReleaseEntry(SceUInt32 num)
{
	if (num < SYSCALLCB_MAX_SYSCALLNUM)
		SyscallCBWrite(num, func_noentry);
}

void ksceKernelRegisterSyscall(SceUInt32 num, void *func)
{
	if (num < SYSCALLCB_MAX_SYSCALLNUM)
		SyscallCBWrite(num, func);
}

/* name unknown */
int SyscallCBIsInitialized(void)
{
	return g_syscall.table != NULL;
}

/* name unknown: install the syscall tracer (tool mode) */
void SceModulemgrForKernel_B23286B8(SyscallTraceInfo *info, void *entry)
{
	g_traceInfo = info;
	g_traceEntry = entry;
}
