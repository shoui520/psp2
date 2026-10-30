/*
 * What DTrace may trace (Sony's dtrace/sce/dtrace_security.c): kernel and
 * system processes only on a kit that allows it.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

struct SceModuleCB;

int dtrace_security_check_internal(void);
int dtrace_security_check_access_process_state(SceUID pid);
int dtrace_security_check_access_current_process_state(void);
int dtrace_security_check_access_process(SceUID pid);
int dtrace_security_check_access_thread(SceUID thid);
int dtrace_security_check_module_probes(struct SceModuleCB *ctl);
int dtrace_security_check_module_info(struct SceModuleCB *ctl);
