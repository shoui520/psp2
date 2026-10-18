/*
 * SceExcpmgr (os0:kd/excpmgr.skprx, slb2:kbl/SceExcpmgr): ARM exception
 * handler chains and the saved CPU context they receive.
 */

#pragma once

#include <psp2kern/types.h>
#include <threadmgr/threadcb.h>
#include <processmgr/process.h>

/* Official SDK exception error names */
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR		0x80020006
#define SCE_KERNEL_ERROR_ILLEGAL_EXCPCODE	0x80027001
#define SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER	0x80027002
#define SCE_KERNEL_ERROR_NOTFOUND_EXCPHANDLER	0x80027003
#define SCE_KERNEL_ERROR_ILLEGAL_PRIORITY	0x80028023
#define SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS	0x80022005

/* exception codes: the vector slot (0x100 + code * 4 on the vector page) */
#define SCE_EXCP_RESET		0
#define SCE_EXCP_UNDEF		1
#define SCE_EXCP_SVC		2
#define SCE_EXCP_PABT		3
#define SCE_EXCP_DABT		4
#define SCE_EXCP_RESERVED	5
#define SCE_EXCP_IRQ		6
#define SCE_EXCP_FIQ		7
#define SCE_EXCP_MAX		8

#define SCE_EXCP_PRIORITY_MAX	8

/*
 * A handler is ARM code preceded by two link words. Registration patches
 * next (the chain is walked by jumping to next + 8); reserved must stay 0.
 */
typedef struct SceExcpmgrHandler {
	struct SceExcpmgrHandler *next;
	unsigned int reserved;
	unsigned int code[];
} SceExcpmgrHandler;

/* one waypoint record of the per-core trace buffer (ARM DBGWPT layout) */
typedef struct SceExcpmgrWaypoint {
	unsigned int reserved;
	unsigned int pc;
	unsigned int targetPc;
	unsigned int flags;	/* bit 31 valid, 2:0 kind, 7:4 exception, 12:8 encoding */
} SceExcpmgrWaypoint;

/* PMU event counter as saved on entry */
typedef struct SceExcpmgrPmuCounter {
	unsigned int type;	/* PMXEVTYPER */
	unsigned int count;	/* PMXEVCNTR */
} SceExcpmgrPmuCounter;

/* the 0x400 byte frame the initial handlers build for every exception */
typedef struct SceExcpmgrContext {
	unsigned int r[13];			/* 0x000 */
	unsigned int sp;			/* 0x034 banked user SP */
	unsigned int lr;			/* 0x038 banked user LR */
	unsigned int pc;			/* 0x03c faulting instruction */
	unsigned int excpCode;			/* 0x040 SCE_EXCP_*; 0 = no VFP state */
	unsigned int cpsr;			/* 0x044 SPSR of the exception mode */
	unsigned int cpacr;			/* 0x048 */
	unsigned int fpscr;			/* 0x04c */
	unsigned int fpexc;			/* 0x050 */
	unsigned int contextId;			/* 0x054 CONTEXTIDR */
	unsigned int tpidrurw;			/* 0x058 */
	unsigned int tpidruro;			/* 0x05c */
	ThreadCB *thread;				/* 0x060 TPIDRPRW: current thread */
	unsigned int ttbr1;			/* 0x064 */
	unsigned int reserved0;			/* 0x068 not saved */
	unsigned int dacr;			/* 0x06c */
	unsigned int dfsr;			/* 0x070 */
	unsigned int ifsr;			/* 0x074 */
	unsigned int dfar;			/* 0x078 */
	unsigned int ifar;			/* 0x07c */
	unsigned int par;			/* 0x080 */
	unsigned int teehbr;			/* 0x084 */
	unsigned int pmcr;			/* 0x088 */
	unsigned int pmcntenset;		/* 0x08c */
	unsigned int pmcntenset2;		/* 0x090 read twice by the original */
	unsigned int pmselr;			/* 0x094 */
	unsigned int pmccntr;			/* 0x098 */
	unsigned int pmuserenr;			/* 0x09c */
	SceExcpmgrPmuCounter pmuCounter[6];	/* 0x0a0 */
	unsigned int reserved1;			/* 0x0d0 not saved */
	unsigned int waypointControl;		/* 0x0d4 trace area word 0x100 */
	unsigned int dbgdscr;			/* 0x0d8 */
	unsigned int reserved2[9];		/* 0x0dc not saved */
	unsigned long long vfp[32];		/* 0x100 d0-d31 */
	SceExcpmgrWaypoint waypoint[32];	/* 0x200 */
} SceExcpmgrContext;


/* the block SceExcpmgrForKernel_96C2869C returns (also given to sysroot slot 4) */
typedef struct SceExcpmgrData {
	unsigned int nestCount[4];		/* 0x00 exception depth per core */
	unsigned int reserved0[4];
	void *stackBase[4];			/* 0x20 */
	void *stackTop[4];			/* 0x30 */
	const unsigned int *kernelMmu;		/* 0x40 [0] TTBR1, [2] context ID */
	ProcessHwBreakpoints *kernelBreakpoints;	/* 0x44 */
	ProcessHwWatchpoints *kernelWatchpoints;	/* 0x48 */
	unsigned int reserved1[13];
} SceExcpmgrData;


/*
 * The "SceVfpIntRegs" block, one 0x1000 page per core (physical 0xe3320000):
 * the running thread's VFP exception mask, which ThreadMgr switches with the
 * thread, then ExcpMgr's trace control and waypoints.
 */
#define VFP_INT_REG_BASE		0xe3320000

typedef struct ExcpTraceArea {
	volatile uint32_t vfpException;		/* the VFP exceptions that trap */
	unsigned int reserved0[63];
	unsigned int control;			/* 0x100 */
	unsigned int reserved1[63];
	SceExcpmgrWaypoint waypoint[32];	/* 0x200 */
} ExcpTraceArea;

/*
 * One core's bank of the exception vector page (VBAR = page + core * 0x40):
 * the vectors, then the per-core words Init fills in.
 */
typedef struct ExcpVectorBank {
	unsigned int vector[8];			/* ldr pc, slot */
	void *sysrootCore;			/* 0x20: the sysroot per-core block */
	ExcpTraceArea *trace;			/* 0x24 */
	unsigned int *nesting;			/* 0x28: the exception nesting level */
	unsigned int reserved[5];
} ExcpVectorBank;

_Static_assert(sizeof(ExcpVectorBank) == 0x40, "ExcpVectorBank size");

/* name unknown: handler chains and the vector page */
typedef struct ExcpHandlerTable {
	int lock;
	unsigned int *vectors;
	SceSize vectorSize;
	SceExcpmgrHandler *defaultHandler;
	SceExcpmgrHandler *first[SCE_EXCP_MAX][SCE_EXCP_PRIORITY_MAX];
	SceExcpmgrHandler *head[SCE_EXCP_MAX];
#if EXCPMGR_TZS
	/* secure-world build: the monitor vector page (MVBAR) and its chain */
	unsigned int *monitorVectors;		/* 0x130 */
	SceSize monitorVectorSize;
	SceExcpmgrHandler *monitorFirst;	/* first monitor handler ever registered */
	SceExcpmgrHandler *monitorHead;
#endif
} ExcpHandlerTable;

typedef void (*SceExcpmgrCallback)(SceExcpmgrContext *context, unsigned int zero,
				   const unsigned int *kernelMmu, unsigned int dacr);

int sceKernelRegisterExceptionHandler(unsigned int code, unsigned int priority,
				      SceExcpmgrHandler *handler);
int sceKernelRegisterDefaultExceptionHandler(SceExcpmgrHandler *handler);
int sceKernelReleaseExceptionHandler(unsigned int code, SceExcpmgrHandler *handler);
SceExcpmgrData *SceExcpmgrForKernel_96C2869C(void);
void sceKernelInitialHandlerDebugUndefCfunc(SceExcpmgrContext *context);
void sceKernelInitialHandlerDebugPabtCfunc(SceExcpmgrContext *context);
void sceKernelInitialHandlerDebugDabtCfunc(SceExcpmgrContext *context);
SceExcpmgrHandler *SceExcpmgrForKernel_4337DD78(void);	/* the default handler */

/* the callbacks the initial debug handlers give a prefetch abort, data abort, undefined context */
void SceExcpmgrForKernel_58F7212B(SceExcpmgrCallback callback);
void SceExcpmgrForKernel_9EE59C6E(SceExcpmgrCallback callback);
void SceExcpmgrForKernel_1FBF5654(SceExcpmgrCallback callback);

/* the kernel process's hardware breakpoints and watchpoints */
void SceExcpmgrForKernel_293DFA04(ProcessHwBreakpoints *regs);
void SceExcpmgrForKernel_E7487AFD(ProcessHwWatchpoints *regs);

/* exception nesting depth of this core, 0 in thread context */
int SceExcpmgrForKernel_B615A7DA(void);
/* the top of a CPU's exception stack */
void *SceExcpmgrForKernel_5420ED8F(int cpu);
/* the code whose faults are SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS */
int SceExcpmgrForKernel_44CE04B8(unsigned int start, unsigned int end);
