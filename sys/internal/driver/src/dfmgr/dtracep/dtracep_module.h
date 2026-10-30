/*
 * The DTrace module's start and stop (Sony's dtracep_module.c): what the
 * rest of the module reads from it.
 */
#pragma once

#include <scetypes.h>

/* each CPU's stack for probes, from the DTrace heap at module start */
#define DTRACE_PROBE_STACK_SIZE	0x2000

extern void *dtraceProbeStack[4];
extern SceSize dtraceProbeStackSize[4];
