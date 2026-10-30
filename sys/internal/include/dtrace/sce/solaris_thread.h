/*
 * Solaris threads: a kthread_t is the kernel's thread control block, its
 * DTrace fields in the state threadmgr allocates for DTrace
 * (dtrace/sce/dtrace_thread.h).
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <threadmgr/threadcb.h>
#include <dtrace/sce/dtrace_thread.h>

typedef ThreadCB kthread_t;

#include <dtrace/utility.h>

#define t_dtrace_start		dtraceState->start
#define t_dtrace_vtime		dtraceState->vtime
#define t_predcache		dtraceState->predcache
#define t_dtrace_on		dtraceState->on
#define t_dtrace_step		dtraceState->step
#define t_dtrace_ret		dtraceState->ret
#define t_dtrace_ast		dtraceState->ast
#define t_dtrace_ft		dtraceState->ft
#define t_taskq			dtraceState->taskq

extern int maxclsyspri;

#ifndef _KERNEL
#define _KERNEL
#endif
#include <sys/taskq.h>
