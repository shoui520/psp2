/*
 * Branch hints, as the kernel spells them: the compiler lays the unlikely
 * side out of line.
 */
#pragma once

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

/*
 * SNC's intrinsic for a breakpoint with a code, which GCC lacks. Sony's
 * scebase_target.h writes it the same way for GCC (_SCE_BREAK() is code 0).
 */
#define __breakpoint(code)	__asm__ volatile("bkpt %0" :: "i" (code) : "memory")

/* the codes Sony's runtime stops with (names ours: no SDK header names them) */
#define BKPT_DIVIDE_BY_ZERO	0x80	/* __aeabi_idiv0 / __aeabi_ldiv0 */
#define BKPT_CHECK_FAILED	0x81	/* a _chk function's size or overlap check */
#define BKPT_ABORT		0x88	/* abort(), or a call that cannot return did */
