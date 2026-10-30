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
