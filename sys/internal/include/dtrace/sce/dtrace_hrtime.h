/*
 * DTrace's high-resolution clock (Sony's dtrace/sce/dtrace_hrtime.c), on the
 * Cortex-A9 global timer.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

void dtrace_hrtime_init(void);
void dtrace_hrtime_exit(void);
hrtime_t dtrace_gethrtime(void);
hrtime_t dtrace_gethrestime(void);
