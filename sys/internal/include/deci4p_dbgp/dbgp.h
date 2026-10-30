/*
 * SceDeci4pDbgpForDriver: the hooks DTrace sets on the debugger's
 * breakpoint handling (deci4p_dtracep's providers, syslibtrace).
 */

#pragma once

#include <scetypes.h>

/*
 * Called for a DTrace breakpoint or USDT probe a process hits, with the
 * instruction there; the debugger steps over a USDT probe the handler takes
 * (returns >= 0).
 */
typedef int (*DbgpDTraceHandler)(SceUID pid, SceUID thid, SceUInt32 pc,
				 SceUInt32 sp, SceUInt32 cpsr,
				 SceUInt32 armOpcode, SceUInt16 thumbOpcode,
				 int isThumb);

void sceDbgpSetDTraceBreakpointHandler(DbgpDTraceHandler handler);
DbgpDTraceHandler sceDbgpGetDTraceBreakpointHandler(void);
void sceDbgpSetDTraceUsdtHandler(DbgpDTraceHandler handler);
DbgpDTraceHandler sceDbgpGetDTraceUsdtHandler(void);

/*
 * SceDeci4pDbgp: the syscalls a test program uses to suspend and resume an
 * application as the debugger would, and to wait until it has.
 */
int sceDeci4pDbgpSuspendAppForTestForUser(SceUID pid);
int sceDeci4pDbgpResumeAppForTestForUser(SceUID pid);
int SceDeci4pDbgp_6180DC1A(SceUID pid, SceUInt32 *timeout);	/* wait for the suspend */
int SceDeci4pDbgp_073A6B4B(SceUID pid, SceUInt32 *timeout);	/* wait for the resume */
