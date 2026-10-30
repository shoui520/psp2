/*
 * Solaris assertions, panics and console messages (Sony's
 * dtrace/sce/solaris_debug.c): assertions and panics go to the kernel's,
 * console messages to DTrace's log.
 */
#pragma once

#include <stdarg.h>
#include <sysmem/debug.h>
#include <dtrace/logging.h>

#define SCE_PURE_ASSERT(_cond)		SCE_DBG_ASSERT((_cond), __LINE__)
#define SCE_PURE_ASSERT1(_cond)		SCE_DBG_ASSERT_CHECK((_cond), __LINE__)

#ifndef ASSERT
#define ASSERT(_cond)			SCE_PURE_ASSERT(_cond)
#endif

#define panic(_fmt, ...)		SCE_PANIC()

extern int panic_quiesce;

/* cmn_err's levels; every message is an error in DTrace's log */
#define CE_CONT		0
#define CE_NOTE		1
#define CE_WARN		2
#define CE_PANIC	3
#define CE_IGNORE	4

#define cmn_err(_ce, _fmt, ...)		DTRACE_LOG_ERROR(_fmt, ##__VA_ARGS__)

int vuprintf(const char *fmt, va_list ap);
int uprintf(const char *fmt, ...);
