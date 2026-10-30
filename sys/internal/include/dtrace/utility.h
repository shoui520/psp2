/*
 * The current thread and process as DTrace sees them (Sony's utility.c).
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/sce/solaris_process.h>

#include <dtrace/sce/solaris_thread.h>

kthread_t *dtrace_get_current_thread(void);
puid_t dtrace_get_current_thread_puid(void);
SceUID dtrace_get_current_thread_id(void);
proc_t *dtrace_get_current_process(void);
SceUID dtrace_get_current_process_main_thread_id(void);
SceUID dtrace_get_current_process_parent_id(void);
SceUID dtrace_get_current_process_id(void);
