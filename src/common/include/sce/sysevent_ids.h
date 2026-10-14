/*
 * Sys event ids (SceKernelSuspendForDriver in os0:kd/sysmem.skprx).
 */

#pragma once

/*
 * The events, in the order ScePower dispatches them (resume == 0 while
 * suspending, 1 while resuming). A negative return from a query event stops
 * the sequence: SUSPEND_QUERY cancels the suspend, the IDLE_QUERY events are
 * retried until every handler is ready. The leveled events go through levels
 * 15 down to 0 while suspending and 0 up to 15 while resuming.
 */
#define SYSEVENT_SUSPEND_QUERY		0x100
#define SYSEVENT_SUSPEND_CANCEL		0x101
#define SYSEVENT_SUSPEND_START		0x102
#define SYSEVENT_SUSPEND_PREPARE(level)	(0x200 + (level))
#define SYSEVENT_SUSPEND_IDLE_ENTER	0x400	/* interrupts off */
#define SYSEVENT_SUSPEND_IDLE		0x401	/* every idle round, interrupts on */
#define SYSEVENT_SUSPEND_IDLE_QUERY	0x402	/* interrupts off */
#define SYSEVENT_SUSPEND_COMMIT		0x1000
#define SYSEVENT_SUSPEND_DEVICE(level)	(0x4000 + (level))
#define SYSEVENT_RESUME_DEVICE(level)	(0x10000 + (level))
#define SYSEVENT_RESUME_INTR		0x40000	/* the last with interrupts off */
#define SYSEVENT_RESUME_START		0x100000
#define SYSEVENT_RESUME_IDLE		0x100001
#define SYSEVENT_RESUME_IDLE_QUERY	0x100002
#define SYSEVENT_RESUME_COMPLETE	0x400000
