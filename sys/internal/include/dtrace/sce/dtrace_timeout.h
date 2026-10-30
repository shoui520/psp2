/*
 * Solaris timeouts (Sony's dtrace/sce/dtrace_timeout.c): a callback run
 * once, on a thread, after a time in nanoseconds.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

typedef uintptr_t timeout_id_t;

timeout_id_t dtrace_timeout(void (*func)(void *), void *arg, hrtime_t nsec);
