/*
 * SceExcpmgr, secure-world build (slb2:kbl/SceExcpmgr).
 *
 * Sony built this module from the same excpmgr.c as os0:kd/excpmgr.skprx,
 * against the ForTZS libraries of the secure kernel. The shared parts live in
 * ../excpmgr_context.h and ../excpmgr_handler.h and call the os0 import
 * names; they are mapped to the ForTZS imports here, before any declaration.
 */
#pragma once

/* ForTZS imports: the os0 name of each one's function */
#define SceDebugForDriver_02B04343		SceDebugForTZS_0D799DAE
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6

/* the TZS parts of the shared excpmgr declarations */
#define EXCPMGR_TZS	1

#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>

#define EXCP_STACK_SIZE		0x1000

extern SceExcpmgrData g_excpData;
extern ExcpHandlerTable g_excpHandlers;
/*
 * this core's stack pointers of the exception modes, which the vector stubs
 * switch to (one banked SP per mode is shared by the secure world's code)
 */
extern void *g_irqStack[4], *g_svcStack[4], *g_abortStack[4], *g_undefStack[4];

/* excpmgr_vector.S */
extern unsigned int exceptionVectors[], monitorVectors[], monitorVectorsEnd[];
extern SceExcpmgrHandler defaultHandler, resetHandler, svcHandler, reservedHandler;
extern SceExcpmgrHandler irqHandler, fiqHandler;
extern SceExcpmgrHandler panicUndefHandler, panicPabtHandler, panicDabtHandler;
extern SceExcpmgrHandler initialUndefHandler, initialPabtHandler, initialDabtHandler;

void sceKernelDefaultHandlerFiqCfunc(void);
void sceKernelDefaultMonitorHandlerCfunc(void);
void sceKernelDefaultHandlerSmcCfunc(void);
void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context);
