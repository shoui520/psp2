/*
 * Solaris time (Sony's dtrace/sce/solaris_hrtime.c): nanoseconds from the
 * system timer, and the clock tick DTrace's waits count in.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

typedef long clock_t;

extern int hz;			/* clock ticks per second */
extern clock_t lbolt;		/* clock ticks since boot */

hrtime_t gethrtime(void);
void delay(clock_t ticks);
