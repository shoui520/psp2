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
