/* Exception manager private state. */
#pragma once
#include <scetypes.h>
#include <fw.h>
#include <sysmem/sysclib.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>

#define EXCP_STACK_SIZE		0x1000

extern SceExcpmgrData g_excpData;
extern SceExcpmgrCallback g_undefCallback, g_pabtCallback, g_dabtCallback;
extern unsigned int g_codeStart, g_codeEnd, g_vmStart, g_vmEnd;
extern unsigned char g_excpStack[4][EXCP_STACK_SIZE];
extern ExcpHandlerTable g_excpHandlers;

/* the offsets excpmgr_vector.S uses */
#define EXCP_OFFSET_CHECK(type, field, offset) \
	_Static_assert(__builtin_offsetof(type, field) == (offset), #type "." #field)
_Static_assert(sizeof(SceExcpmgrContext) == 0x400, "SceExcpmgrContext size");
EXCP_OFFSET_CHECK(SceExcpmgrContext, sp, 0x34);
EXCP_OFFSET_CHECK(SceExcpmgrContext, pc, 0x3c);
EXCP_OFFSET_CHECK(SceExcpmgrContext, excpCode, 0x40);
EXCP_OFFSET_CHECK(SceExcpmgrContext, cpsr, 0x44);
EXCP_OFFSET_CHECK(SceExcpmgrContext, cpacr, 0x48);
EXCP_OFFSET_CHECK(SceExcpmgrContext, fpscr, 0x4c);
EXCP_OFFSET_CHECK(SceExcpmgrContext, fpexc, 0x50);
EXCP_OFFSET_CHECK(SceExcpmgrContext, contextId, 0x54);
EXCP_OFFSET_CHECK(SceExcpmgrContext, tpidruro, 0x5c);
EXCP_OFFSET_CHECK(SceExcpmgrContext, thread, 0x60);
EXCP_OFFSET_CHECK(SceExcpmgrContext, ttbr1, 0x64);
EXCP_OFFSET_CHECK(SceExcpmgrContext, dacr, 0x6c);
EXCP_OFFSET_CHECK(SceExcpmgrContext, dfsr, 0x70);
EXCP_OFFSET_CHECK(SceExcpmgrContext, dfar, 0x78);
EXCP_OFFSET_CHECK(SceExcpmgrContext, par, 0x80);
EXCP_OFFSET_CHECK(SceExcpmgrContext, pmcr, 0x88);
EXCP_OFFSET_CHECK(SceExcpmgrContext, pmuCounter, 0xa0);
EXCP_OFFSET_CHECK(SceExcpmgrContext, waypointControl, 0xd4);
EXCP_OFFSET_CHECK(SceExcpmgrContext, dbgdscr, 0xd8);
EXCP_OFFSET_CHECK(SceExcpmgrContext, vfp, 0x100);
EXCP_OFFSET_CHECK(SceExcpmgrContext, waypoint, 0x200);
EXCP_OFFSET_CHECK(SceExcpmgrData, stackTop, 0x30);
EXCP_OFFSET_CHECK(SceExcpmgrData, kernelMmu, 0x40);
EXCP_OFFSET_CHECK(SceExcpmgrData, kernelBreakpoints, 0x44);
EXCP_OFFSET_CHECK(SceExcpmgrData, kernelWatchpoints, 0x48);
EXCP_OFFSET_CHECK(ExcpVectorBank, trace, 0x24);
EXCP_OFFSET_CHECK(ExcpTraceArea, control, 0x100);
EXCP_OFFSET_CHECK(ExcpTraceArea, waypoint, 0x200);
EXCP_OFFSET_CHECK(ThreadCB, process, 0x18);
EXCP_OFFSET_CHECK(ThreadCB, thbp, 0x1c);

/* excpmgr_vector.S */
extern unsigned int exceptionVectors[];
extern SceExcpmgrHandler defaultHandler, resetHandler, svcHandler, reservedHandler, irqHandler;
extern SceExcpmgrHandler panicUndefHandler, panicPabtHandler, panicDabtHandler;
extern SceExcpmgrHandler initialUndefHandler, initialPabtHandler, initialDabtHandler;
void excpResumeContext(SceExcpmgrContext *context) __attribute__((noreturn));

/* excpmgr.c */
void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
				  const SceKernelDebugInfo *info, const char *message);
int ClassifyAbortInstruction(unsigned int pc, unsigned int cpsr);
unsigned int PatchUserInstruction(SceExcpmgrContext *context);
void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerReturnFromNestedExceptionCfunc(SceExcpmgrContext *context);
