/*
 * SceSblSmsched, the secure world's secure module scheduler
 * (slb2:kbl/SceSblSmsched): it starts the Cmep's secure kernel and runs
 * one secure module at a time on it, for the requests the Non-secure
 * kernel makes through its monitor calls (smsc_proxy).
 *
 * smsched.c schedules, sms_sk_proxy.c talks to the secure kernel through
 * the Cmep mailbox and tzs_bridge.c holds the argument areas shared with
 * the Non-secure kernel.
 */
#pragma once

/* ForTZS imports, named by the henkaku wiki (SceSysmem, SceSysroot) */
#define sceKernelVAtoPAForTZS			SceSysmemForTZS_1DEADF6C
#define sceKernelFreeUncacheHeapMemoryForTZS	SceSysmemForTZS_2F18E288
#define sceKernelFreeHeapMemoryForTZS		SceSysmemForTZS_42FB5B12
#define sceKernelAllocHeapMemoryForTZS		SceSysmemForTZS_473871D8
#define sceKernelCreateHeapForTZS		SceSysmemForTZS_56A16D84
#define sceKernelDeleteHeapForTZS		SceSysmemForTZS_624454BC
#define sceKernelAllocUncacheHeapMemoryWithOptionForTZS SceSysmemForTZS_6A3FBAF0
#define sceKernelVARangeToPAVectorForTZS	SceSysmemForTZS_9D43E416
#define sceKernelSysrootGetSharedMemoryForTZS	SceSysrootForTZS_1C96096F
#define sceKernelSysrootGetKblParamForTZS	SceSysrootForTZS_29C1049E
#define sceKernelSysrootSetModulePrivateForTZS	SceSysrootForTZS_EA572BD9
#define SceDebugForDriver_35A35322			SceDebugForTZS_2F9B8AA8
/* ForTZS imports: the names of their 0.990 stubs, os0's for the debug ones */
#define sceKernelDcacheCleanRange		SceCpuForTZS_190D96D5
#define sceKernelCpuId				SceCpuForTZS_44C423D3
#define SceCpuForDriver_BF82DEB2		SceCpuForTZS_71FD9AB5
#define SceCpuForDriver_27C0B340 SceCpuForTZS_ACF209F3
#define sceKernelSpinlockLowUnlockCpuResumeIntr SceCpuForTZS_B8F00FBE
#define sceKernelSpinlockLowUnlock		SceCpuForTZS_CD98416C
#define sceKernelDcacheInvalidateRange		SceCpuForTZS_CDD46655
#define SceCpuForDriver_D32ACE9E	SceCpuForTZS_D67A4356
#define atomicCompareAndSet8			SceCpuForTZS_EFD6F289
#define SceDebugForDriver_02B04343		SceDebugForTZS_0D799DAE
#define SceDebugForDriver_391B5B74				SceDebugForTZS_563B623D
#define SceDebugForDriver_FD753E7A			SceDebugForTZS_618F89E6
#define SceDebugForDriver_D9703808		SceDebugForTZS_D26EB4BB
#define SceDebugForDriver_374B7868		SceDebugForTZS_F851BA4F

#include <stdint.h>
#include <fw.h>
#include <psp2kern/types.h>
#include <sysmem/sysmem.h>
#include <kernel_boot_loader/kbl_param.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr_tzs.h>
#include "cmep_regs.h"
#include <smsc_proxy/smsc_proxy.h>
#include <sysmem/cpu.h>

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

int sceKernelVAtoPAForTZS(const void *va, uintptr_t *pa);
int sceKernelVARangeToPAVectorForTZS(const SceKernelVARange *range, SceKernelPAVector *vector);
void *sceKernelAllocUncacheHeapMemoryWithOptionForTZS(SceSize size, SceAllocOpt *opt);
int sceKernelFreeUncacheHeapMemoryForTZS(void *ptr);
void *sceKernelAllocHeapMemoryForTZS(SceUID heap, SceSize size);
int sceKernelFreeHeapMemoryForTZS(SceUID heap, void *ptr);
SceUID sceKernelCreateHeapForTZS(const char *name, SceSize size, SceKernelHeapCreateOpt *opt);
int sceKernelDeleteHeapForTZS(SceUID heap);
void *sceKernelSysrootGetSharedMemoryForTZS(void);
int sceKernelSysrootSetModulePrivateForTZS(int index, void *data);
const SceKblParam *sceKernelSysrootGetKblParamForTZS(void);
void sceKernelDcacheCleanRange(const void *addr, SceSize size);
void sceKernelDcacheInvalidateRange(const void *addr, SceSize size);
int atomicCompareAndSet8(volatile SceUInt8 *addr, SceUInt8 expected, SceUInt8 value);

#define SCE_SBL_ERROR_NO_MEMORY			((int)0x800f040c)
#define SCE_SBL_ERROR_INVALID_ARGUMENT		((int)0x800f0416)
#define SCE_SBL_ERROR_NOT_SUPPORTED		((int)0x800f0425)
#define SCE_SBL_ERROR_ILLEGAL_STATE		((int)0x800f0429)
#define SCE_SBL_ERROR_NOT_READY			((int)0x800f0426)
#define SCE_SBL_ERROR_NO_REQUEST		((int)0x800f042b)
#define SCE_SBL_ERROR_BUSY			((int)0x800f042e)
#define SCE_SBL_ERROR_SECURE_KERNEL_START	((int)0x800f0427)	/* the Cmep refused the kernel */
#define SCE_SBL_ERROR_CMEP(message)		((int)(0x800f0300 | ((message) & 0xff)))

#define SMSCHED_DEBUG_AREA_SIZE			0x80
#define SMSCHED_SK_CMD_AREA_SIZE		0x100

/* a circular list: a head is a link no request contains */
typedef struct SmschedLink {
	struct SmschedLink *next;
	struct SmschedLink *prev;
} SmschedLink;

/* the scheduler's state (Sony: the instance smsSecureKernelProxySetInstance takes) */
typedef struct SmschedInstance {
	SceUID heap;				/* "SceSmsched" */
	void *debugArea;			/* SetDebug */
	void *skCmdArea;			/* the secure kernel's command arguments */
	SceKernelPARange *self;			/* kprx_auth_sm: the scheduler's own secure module */
	uintptr_t selfPA;
	struct SmschedRequest *requests;
	SmschedLink freeList;
} SmschedInstance;

/* mailboxes 1-3 of a request's module while it does not run: the saved
 * registers, which the Cmep reads and writes too */
typedef struct SmschedMailbox {
	SceUInt32 arm2Cry[CMEP_MAILBOXES - 1];
	SceUInt32 cry2Arm[CMEP_MAILBOXES - 1];
} SmschedMailbox;

/* a request's state, as cry2Arm0IntHandler moves it on the secure kernel's messages */
#define SMSCHED_STATE_READY			1	/* queued, new or suspended */
#define SMSCHED_STATE_RUNNING			2
#define SMSCHED_STATE_EXITED			3	/* done: result set */
#define SMSCHED_STATE_KILLED			4	/* done */
#define SMSCHED_STATE_STOPPED			5	/* done */
#define SMSCHED_STATE_STARTING			6	/* Run or Resume sent */
#define SMSCHED_STATE_SUSPENDING		7
#define SMSCHED_STATE_STOPPING			8
#define SMSCHED_STATE_KILLING			9
#define SMSCHED_STATE_EXITING			10
#define SMSCHED_STATE_PREEMPTING		11	/* Suspend sent */
#define SMSCHED_STATE_SOFT_KILLING		12	/* SoftKill sent */

/* the Cmep holds the request's module: it runs, or is being preempted or killed */
static inline int SmschedStateOnCmep(SceUInt32 state)
{
	switch (state) {
	case SMSCHED_STATE_RUNNING:
	case SMSCHED_STATE_PREEMPTING:
	case SMSCHED_STATE_SOFT_KILLING:
		return 1;
	}
	return 0;
}

#define SMSCHED_REQUESTS			64
#define SMSCHED_REQUEST_FREE			0x80000000	/* in id */

/* the Cmep's interrupts: Cry2Arm0 (the secure kernel's messages), Cry2Arm1-3 */
#define SMSCHED_INTR_CRY2ARM0			200
#define SMSCHED_SOFT_INTR_PROXY			4	/* to the Non-secure kernel's smsc_proxy */

/* a Cry2Arm interrupt handler of a request (mailboxes 0-3) */
typedef void (*SmschedIntrHandler)(SceInt32 proxyId, int mailbox, void *arg,
				   SceUInt32 status, SceInt32 result);

typedef struct SmschedIntrEntry {
	SmschedIntrHandler handler;
	void *arg;
	SceUInt32 enabled;
} SmschedIntrEntry;

/* a request's area, shared with the secure module (uncached; its PA is
 * passed to Run and Resume) */
typedef struct SmschedArea {
	SceInt32 result;			/* the module's exit value */
	SceUInt32 reserved04[15];
	SmschedMailbox mailbox;			/* 0x40: mailboxes 1-3 while off the Cmep */
	SceUInt32 reserved58[10];
} SmschedArea;

_Static_assert(sizeof(SmschedArea) == 128, "SmschedArea");

/* one of the 64 requests: a secure module and its run state */
typedef struct SmschedRequest {
	SmschedLink link;			/* in a run queue, the done list or the free list */
	volatile SceUInt32 id;			/* the index, bit 31 set while free */
	SceUInt32 priority;			/* the run queue */
	uintptr_t selfPA;
	SceUInt32 arg1;
	SceUInt32 arg2Low;			/* arg2, a 64-bit value in two words */
	SceUInt32 arg2High;
	SceUInt32 arg3;
	SceUInt32 arg4;
	volatile SmschedIntrStatus status;	/* result and state, as the handler gets them */
	uintptr_t areaPA;
	SceUInt32 contextType;
	uintptr_t vectorPA;			/* the saved context's PA vector */
	SceUInt32 vectorCount;
	void *volatile context;				/* 0x20000 bytes, while suspended */
	void *volatile vector;
	SmschedMailbox *volatile mailbox;	/* in area */
	uintptr_t mailboxPA;
	SceInt32 proxyId;			/* the Non-secure kernel's id, -1 none */
	SceUInt32 notifyMask;
	SceUInt8 waitFlag;
	SceUInt8 padding[3];
	SceKernelSpinlock lock;			/* inside s_lock */
	SmschedInvokeArg invokeArg;
	SmschedIntrEntry intr[CMEP_MAILBOXES];
	struct SmschedArea *volatile area;	/* uncached */
	SceUInt32 reserved;
} SmschedRequest;

_Static_assert(sizeof(SmschedRequest) == 200, "SmschedRequest");

/* contextType: how reschedule starts the request; 1 and 4 run the module
 * anew, 2 and 3 resume a saved context */
#define SMSCHED_CONTEXT_NEW			1
#define SMSCHED_CONTEXT_PREEMPTED		3	/* saved when reschedule preempted it */
#define SMSCHED_CONTEXT_SIZE			0x20000	/* the secure kernel's saved context */
#define SMSCHED_CONTEXT_RANGES			16	/* its PA vector */

static inline void SmschedLinkRemove(SmschedLink *link)
{
	link->next->prev = link->prev;
	link->prev->next = link->next;
	link->next = link;
	link->prev = link;
}

static inline void SmschedLinkAddTail(SmschedLink *head, SmschedLink *link)
{
	link->next = head;
	link->prev = head->prev;
	head->prev = link;
	link->prev->next = link;
}

/* notifyMask: the state changes reported to the Non-secure kernel */
#define SMSCHED_NOTIFY_READY			0x1	/* suspended */
#define SMSCHED_NOTIFY_RUNNING			0x2
#define SMSCHED_NOTIFY_EXITED			0x4
#define SMSCHED_NOTIFY_KILLED			0x8
#define SMSCHED_NOTIFY_STOPPED			0x10
#define SMSCHED_NOTIFY_DONE			(SMSCHED_NOTIFY_EXITED | SMSCHED_NOTIFY_KILLED | \
						 SMSCHED_NOTIFY_STOPPED)

/* smsched.c: the mailbox operations, on the Cmep's registers (request NULL)
 * or on a request's saved mailbox */
int ackCry2Arm(int mailbox, SceUInt32 bits, SmschedRequest *request);
int setArm2Cry(int mailbox, SceUInt32 bits, SmschedRequest *request);
int readArm2Cry(int mailbox, SceUInt32 *value, SmschedRequest *request);
int clearArm2Cry(int mailbox, SceUInt32 bits, SmschedRequest *request);
int readCry2Arm(int mailbox, SceUInt32 *value, SmschedRequest *request);

/* sms_sk_proxy.c: the secure kernel's commands */
int smsSecureKernelProxySetInstance(SmschedInstance *instance);
int smsSecureKernelProxyStartSecureKernel(uintptr_t enpPA, SceSize enpSize);
int smsSecureKernelProxyStopSecureKernel(void);
int smsSecureKernelProxyInstallRevokeList(uintptr_t listPA, SceSize listSize);
int smsSecureKernelProxyRunSecureModule(uintptr_t selfPA, SceUInt32 arg1, SceUInt64 arg2,
					SceUInt32 arg3, SceUInt32 arg4,
					const SmschedInvokeArg *invokeArg, uintptr_t areaPA);
int smsSecureKernelProxySuspendSecureModule(uintptr_t vectorPA, SceUInt32 vectorCount,
					    uintptr_t areaPA, uintptr_t mailboxPA);
int smsSecureKernelProxyResumeSecureModule(uintptr_t vectorPA, SceUInt32 vectorCount,
					   uintptr_t areaPA, uintptr_t mailboxPA);
int smsSecureKernelProxyHardKillSecureModule(void);
int smsSecureKernelProxySoftKillSecureModule(void);
int smsSecureKernelProxySetDebug(const void *area, SceSize size);
int smsSecureKernelProxyGenerateIntr0(void);
int smsSecureKernelProxyGenerateIntr1(void);
int smsSecureKernelProxyGenerateIntr2(void);
int smsSecureKernelProxyGenerateIntr3(void);
int smsSecureKernelProxyGenerateIntr4(void);
int smsSecureKernelProxyReady(void);

/* tzs_bridge.c */
#include <smsc_proxy/tzs_bridge.h>
