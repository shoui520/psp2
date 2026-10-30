/*
 * The syscall provider's probes (Sony's dtrace/sce/sce_systrace.c): one per
 * system library function, entry and return, hooked through syslibtrace.
 */
#pragma once

#include <sys/dtrace.h>

void systrace_provide_sce(void *arg, const dtrace_probedesc_t *desc);
void systrace_probe_enable_sce(void *arg, dtrace_id_t id, void *parg);
void systrace_probe_disable_sce(void *arg, dtrace_id_t id, void *parg);
void systrace_probe_destroy_sce(void *arg, dtrace_id_t id, void *parg);
void systrace_detach_sce(void);
