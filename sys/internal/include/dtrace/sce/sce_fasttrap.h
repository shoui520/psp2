/*
 * The pid provider's hook into the debugger (Sony's dtrace/sce/sce_fasttrap.c):
 * a user breakpoint hands DTrace the thread's registers.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

int fasttrap_sce_init(void);
int fasttrap_sce_fini(void);
