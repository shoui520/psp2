/*
 * SceExcpmgr, secure-world build (slb2:kbl/SceExcpmgr).
 *
 * Sony built this module from the same excpmgr.c as os0:kd/excpmgr.skprx,
 * against the ForTZS libraries of the secure kernel. The shared parts live in
 * ../excpmgr_context.h and ../excpmgr_handler.h and call the os0 import
 * names; they are mapped to the ForTZS imports here, before any declaration.
 */
#pragma once

/* ForTZS imports: names unknown, the os0 function each one stands for */
#define SceSysrootForKernel_D7198963			SceSysrootForTZS_0567B4B9
#define SceCpuForDriver_BF82DEB2		SceCpuForTZS_71FD9AB5
#define sceKernelSpinlockLowUnlock		SceCpuForTZS_CD98416C
#define SceCpuForDriver_D32ACE9E	SceCpuForTZS_D67A4356
#define sceKernelSpinlockLowUnlockCpuResumeIntr SceCpuForTZS_B8F00FBE
#define SceCpuForDriver_9CB9F0CE	SceCpuForTZS_596784E9
#define SceDebugForDriver_02B04343		SceDebugForTZS_0D799DAE
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6
#define memcpy					SceSysclibForTZS_BE3AF2EE

/* the TZS parts of the shared excpmgr declarations */
#define EXCPMGR_TZS	1

#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

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

/* ForTZS imports, names unknown */
unsigned int *SceSysrootForTZS_2460ADFA(void);		/* exception vector page */
unsigned int *SceSysrootForTZS_D4E6D8E8(void);		/* monitor vector page */
unsigned int SceSysrootForTZS_317E7556(unsigned int cpu);	/* per-core block */
const unsigned int *SceSysrootForTZS_A8B7DAF9(void);	/* kernel MMU state */
int SceSysrootForTZS_EA572BD9(unsigned int slot, void *data);
void SceCpuForTZS_A5965CBF(void);			/* after code was written */
void SceCpuForTZS_17A88E69(void *start, SceSize size);	/* after the MVBAR page changed */
void SceCpuForTZS_B421FAFD(void *start, SceSize size);

void sceKernelDefaultHandlerFiqCfunc(void);
void sceKernelDefaultMonitorHandlerCfunc(void);
void sceKernelDefaultHandlerSmcCfunc(void);
void sceKernelPanicHandlerUndefCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerPabtCfunc(SceExcpmgrContext *context);
void sceKernelPanicHandlerDabtCfunc(SceExcpmgrContext *context);
int sceKernelRegisterMonitorEntry(unsigned int code, SceExcpmgrHandler *handler);
