/*
 * DTrace's per-thread state, in Sony's header (the 0.990 deci4p_dtracep names
 * sys/internal/include/dtrace/sce/dtrace_thread.h). Threadmgr allocates it
 * (dtraceThreadCBInit) and marks a thread's start for the lwp-start probe; DTrace's
 * dtrace/sce/dtrace_thread.c keeps the rest (dtrace_get_thread_tracing, _reentering,
 * _predcache, _vtime, the probe lock).
 */
#pragma once

#include <stdint.h>

#define DTRACE_THREAD_START		0x00000004	/* __proc_:lwp__start pending */

typedef struct SceDtraceThreadCB {
	uint32_t unk00[5];
	uint32_t flags;				/* 0x14: DTRACE_THREAD_START when allocated */
	uint32_t unk18[4];
} SceDtraceThreadCB;
_Static_assert(sizeof(SceDtraceThreadCB) == 0x28, "SceDtraceThreadCB size");
