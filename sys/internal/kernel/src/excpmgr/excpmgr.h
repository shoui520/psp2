/* Exception manager private state. */
#pragma once
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>

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

/* Import names unknown: sysroot per-core state and debug services. */
void *SceSysrootForKernel_06182D59(unsigned int cpu);
int SceSysrootForKernel_0A12227B(SceUID pid, unsigned int value);
int SceSysrootForKernel_1D84C4D4(SceUID pid, unsigned int pc, SceUID *uid,
				unsigned int *base, char *name);
int sceKernelSysrootPrintBacktrace(SceUID pid, const SceUID *uids, unsigned int count, unsigned int flags);
int sceKernelSysrootBacktrace(unsigned int flags, SceUID *uids, unsigned int count,
				 unsigned int *returned, unsigned int kind);
void sceKernelSysrootReturnFromExcpToThread(SceExcpmgrContext *context, void *thread);
int ksceKernelIsColdBoot(void);
void SceDebugForKernel_CCABDD98(const SceKernelDebugInfo *info, SceExcpmgrContext *context,
			     unsigned int reason, const char *message) __attribute__((noreturn));
int SceKernelSuspendForDriver_CE7A2207(unsigned int slot, int (*handler)(void), void *arg);
int SceKernelSuspendForDriver_105C5752(unsigned int slot, int (*handler)(void), void *arg);
int SceQafMgrForDriver_382C71E8(void);
int ksceGUIDGetName(SceUID uid, char **name);

/* excpmgr.c */
void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
				  const SceKernelDebugInfo *info, const char *message);
int ClassifyAbortInstruction(unsigned int pc, unsigned int cpsr);
unsigned int PatchUserInstruction(SceExcpmgrContext *context);
void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerReturnFromNestedExceptionCfunc(SceExcpmgrContext *context);
