/*
 * DTrace's per-thread state, in Sony's header (the 0.990 deci4p_dtracep names
 * sys/internal/include/dtrace/sce/dtrace_thread.h). Threadmgr allocates it
 * (dtraceThreadCBInit) and marks a thread's start for the lwp-start probe; DTrace's
 * accessors here (dtrace_get_thread_tracing, _reentering, _predcache, _vtime, the probe
 * lock: ThreadCB.dtraceLock) keep the rest. The fields are illumos's kthread_t DTrace
 * fields (t_dtrace_start, t_dtrace_vtime, t_predcache, the fasttrap flags), at the
 * offsets 4.00's dtracep reads them: dtrace_probe the times and the predicate cache,
 * dtrace_probe_sdt the flags, fasttrap_pid_probe the step flag, taskq_wait the queue.
 */
#pragma once

#include <stdint.h>

#define DTRACE_THREAD_START		0x00000004	/* __proc_:lwp__start pending */
#define DTRACE_THREAD_REENTERING	0x00000008	/* in a probe: dtrace_probe_sdt sets it */

struct taskq;

typedef struct SceDtraceThreadCB {
	int64_t start;				/* 0x00: when its virtual time last started */
	int64_t vtime;				/* 0x08: its virtual time */
	uint32_t predcache;			/* 0x10: the predicate cache ID */
	uint32_t flags;				/* 0x14: DTRACE_THREAD_* */
	union {
		struct {
			uint8_t on;		/* 0x18: fasttrap's flags */
			uint8_t step;		/* 0x19: fasttrap_pid_probe checks it */
			uint8_t ret;
			uint8_t ast;
		};
		uint32_t ft;			/* 0x18: all four, cleared together */
	};
	uint32_t reserved[2];			/* 0x1c: zeroed at allocation, never read */
	struct taskq *taskq;			/* 0x24: the task queue it works for */
} SceDtraceThreadCB;
_Static_assert(sizeof(SceDtraceThreadCB) == 0x28, "SceDtraceThreadCB size");

#define DTRACE_THREAD_NAME_LEN	31
#define DTRACE_OK		0

/* dtrace_get_current_thread_type (Sony's sce.d: DTRACETHR_*) */
#define DT_THREAD_UNKNOWN	(-1)
#define DT_THREAD_GAME		0
#define DT_THREAD_SYSTEM	1
#define DT_THREAD_KERNEL	2
#define DT_THREAD_KERNEL_IDLE	3

/*
 * PLACEHOLDER: what the interrupt entry leaves right below the guard of a
 * nesting level (SceIntrmgrForKernel_01911612), as DTrace reads it: the
 * interrupted thread, or the interrupted pc and cpsr when no thread ran (the
 * frame's layout and names unknown)
 */
struct ThreadCB;
typedef struct dtrace_intr_frame {
	struct ThreadCB *thread;		/* 0x000 */
	uint32_t reserved[0x4d];
	uint32_t pc;				/* 0x138 */
	uint32_t cpsr;				/* 0x13c */
} dtrace_intr_frame_t;
_Static_assert(sizeof(dtrace_intr_frame_t) == 0x140, "dtrace_intr_frame_t size");

/* a thread's registers, as the debugger hands them to the pid provider */
#define REG_R0		0
#define REG_R1		1
#define REG_R2		2
#define REG_R3		3
#define REG_SP		13
#define REG_LR		14
#define REG_PC		15
#define REG_CPSR	16

typedef struct dtrace_thread_state {
	uintptr_t reg[18];	/* r0-r12, sp, lr, pc, cpsr, fpscr */
} dtrace_thread_state_t;

struct taskq;

void dtrace_set_thread_taskq(int thread, struct taskq *tq);
uint64_t dtrace_getreg_thread(struct ThreadCB *thread, unsigned int reg);
void dtrace_get_current_thread_name(char *name, unsigned int size);
int dtrace_get_current_thread_type(void);
int dtrace_thread_state_init(void);
int dtrace_thread_state_exit(void);

/*
 * Sony's accessors, inlined everywhere they are used: the state threadmgr keeps
 * for DTrace is read through the thread each time, and the probe lock is the
 * thread's DTrace spinlock.
 */
#include <sysmem/cpu.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>

static inline int64_t dtrace_get_thread_tracing(struct ThreadCB *thread)
{
	return thread->dtraceState->start;
}

static inline void dtrace_set_thread_tracing(struct ThreadCB *thread, int64_t tracing)
{
	thread->dtraceState->start = tracing;
}

static inline int64_t dtrace_get_thread_vtime(struct ThreadCB *thread)
{
	return thread->dtraceState->vtime;
}

static inline void dtrace_set_thread_vtime(struct ThreadCB *thread, int64_t vtime)
{
	thread->dtraceState->vtime = vtime;
}

static inline uint32_t dtrace_get_thread_predcache(struct ThreadCB *thread)
{
	return thread->dtraceState->predcache;
}

static inline void dtrace_set_thread_predcache(struct ThreadCB *thread, uint32_t predcache)
{
	thread->dtraceState->predcache = predcache;
}

static inline int dtrace_get_thread_reentering(struct ThreadCB *thread)
{
	return thread->dtraceState->flags & DTRACE_THREAD_REENTERING;
}

static inline void dtrace_set_thread_reentering(struct ThreadCB *thread, int reentering)
{
	if (reentering)
		thread->dtraceState->flags |= DTRACE_THREAD_REENTERING;
	else
		thread->dtraceState->flags &= ~DTRACE_THREAD_REENTERING;
}

static inline void dtrace_thread_probelock_lock(struct ThreadCB *thread)
{
	sceKernelSpinlockLowLock(&thread->dtraceLock);
}

static inline void dtrace_thread_probelock_unlock(struct ThreadCB *thread)
{
	sceKernelSpinlockLowUnlock(&thread->dtraceLock);
}

static inline int dtrace_thread_start(SceUID thread, SceSize argSize, void *argp)
{
	return sceKernelStartThread(thread, argSize, argp);
}

static inline void dtrace_thread_exit(int status)
{
	sceKernelExitDeleteThread(status);
}
