/*
 * Solaris processes (Sony's dtrace/sce/solaris_process.c): a proc_t is the
 * kernel's process object, found by its ID and held by a reference.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <processmgr/process.h>

typedef struct SceProcessObject proc_t;

/* the DTrace fields of the process object */
#define p_pid			pid
#define p_dtrace_probes		dtraceProbes
#define p_dtrace_count		dtraceCount
#define p_dtrace_helpers	dtraceHelpers

pid_t proc_pid(proc_t *p);
pid_t proc_ppid(proc_t *p);
proc_t *proc_find(pid_t pid);
int proc_rele(proc_t *p);
proc_t *sprlock(pid_t pid);
void sprunlock(proc_t *p);
void proc_lock(proc_t *p);
void proc_unlock(proc_t *p);
int uread(proc_t *p, void *kaddr, size_t len, uintptr_t uaddr);
int uwrite(proc_t *p, void *kaddr, size_t len, uintptr_t uaddr);
