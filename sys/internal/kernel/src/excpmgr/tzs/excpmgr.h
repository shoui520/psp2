/*
 * SceExcpmgr, secure-world build (slb2:kbl/SceExcpmgr).
 *
 * Sony built this module from the same excpmgr.c as os0:kd/excpmgr.skprx,
 * against the ForTZS libraries of the secure kernel. The shared C lives in
 * ../excpmgr_handler.h, the shared exception assembly in ../excpmgr_initial.S
 * and ../excpmgr_context.S (built with EXCPMGR_TZS); both call the os0 import
 * names.
 */
#pragma once

/* the ForTZS imports under their os0 names, before any declaration */
#include <sysmem/debug_tzs.h>

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

/* excpmgr_vector.S, ../excpmgr_initial.S */
extern unsigned int exceptionVectors[], monitorVectors[], monitorVectorsEnd[];
extern SceExcpmgrHandler defaultHandler, resetHandler, svcHandler, reservedHandler;
extern SceExcpmgrHandler irqHandler, fiqHandler, monitorHandler, smcHandler;
extern SceExcpmgrHandler panicUndefHandler, panicPabtHandler, panicDabtHandler;
extern SceExcpmgrHandler initialUndefHandler, initialPabtHandler, initialDabtHandler;

void sceKernelDefaultHandlerFiqCfunc(void);
void sceKernelDefaultMonitorHandlerCfunc(void);
void sceKernelDefaultHandlerSmcCfunc(void);
void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context);
