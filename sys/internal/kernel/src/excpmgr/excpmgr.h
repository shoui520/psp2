/* Exception manager private state. */
#pragma once
#include <scetypes.h>
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
