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
#define sceKernelAllocUncacheHeapMemoryWithOptionForTZS SceSysmemForTZS_6A3FBAF0

#include <stdint.h>
#include <psp2kern/types.h>
#include "sce/sysmem.h"
#include "sce/cmep_regs.h"

/* the compiler's stack-protector symbols are the ForTZS sysclib ones here */
__asm__(".set __stack_chk_fail, SceSysclibForTZS_E5457B97\n\t"
	".set __stack_chk_guard, SceSysclibForTZS_BA505B15");

int sceKernelVAtoPAForTZS(const void *va, uintptr_t *pa);
void *sceKernelAllocUncacheHeapMemoryWithOptionForTZS(SceSize size, SceAllocOpt *opt);
int sceKernelFreeUncacheHeapMemoryForTZS(void *ptr);

#define SCE_SBL_ERROR_NO_MEMORY			((int)0x800f040c)
#define SCE_SBL_ERROR_INVALID_ARGUMENT		((int)0x800f0416)
#define SCE_SBL_ERROR_SECURE_KERNEL_START	((int)0x800f0427)	/* the Cmep refused the kernel */
#define SCE_SBL_ERROR_CMEP(message)		((int)(0x800f0300 | ((message) & 0xff)))

#define SMSCHED_DEBUG_AREA_SIZE			0x80
#define SMSCHED_SK_CMD_AREA_SIZE		0x100

/* the scheduler's state (Sony: the instance smsSecureKernelProxySetInstance takes) */
typedef struct SmschedInstance {
	SceUID heap;				/* "SceSmsched" */
	void *debugArea;			/* SetDebug */
	void *skCmdArea;			/* the secure kernel's command arguments */
	void *self;
	uintptr_t selfPA;
	struct SmschedRequest *requests;
	struct SmschedRequest *freeHead;
	struct SmschedRequest *freeTail;
} SmschedInstance;

/* mailboxes 1-3 of a request's module while it does not run: the saved
 * registers, which the Cmep reads and writes too */
typedef struct SmschedMailbox {
	volatile SceUInt32 arm2Cry[CMEP_MAILBOXES - 1];
	volatile SceUInt32 cry2Arm[CMEP_MAILBOXES - 1];
} SmschedMailbox;

/* the secure module start arguments the Non-secure kernel passes */
typedef struct SmschedInvokeArg {
	SceUInt32 word[12];
} SmschedInvokeArg;

/* a Cry2Arm interrupt handler of a request (mailboxes 0-3) */
typedef void (*SmschedIntrHandler)(SceInt32 proxyId, int mailbox, void *arg,
				   SceUInt32 status, SceInt32 result);

typedef struct SmschedIntrEntry {
	SmschedIntrHandler handler;
	void *arg;
	SceUInt32 enabled;
} SmschedIntrEntry;

/* one of the 64 requests: a secure module and its run state */
typedef struct SmschedRequest {
	struct SmschedRequest *next;		/* in a run queue or the free list */
	struct SmschedRequest *prev;
	SceUInt32 id;				/* the index, bit 31 set while free */
	SceUInt32 priority;			/* the run queue */
	uintptr_t selfPA;
	SceUInt32 arg1;
	SceUInt64 arg2;
	SceUInt32 arg3;
	SceUInt32 arg4;
	SceInt32 result;
	SceUInt32 state;
	uintptr_t areaPA;
	SceUInt32 contextType;
	uintptr_t vectorPA;			/* the saved context's PA vector */
	SceUInt32 vectorCount;
	void *context;				/* 0x20000 bytes, while suspended */
	void *vector;
	SmschedMailbox *volatile mailbox;	/* in area */
	uintptr_t mailboxPA;
	SceInt32 proxyId;			/* the Non-secure kernel's id, -1 none */
	SceUInt32 notifyMask;
	SceUInt8 waitFlag;
	SceUInt8 padding[3];
	SceUInt32 lock;
	SmschedInvokeArg invokeArg;
	SmschedIntrEntry intr[CMEP_MAILBOXES];
	void *area;				/* 128 bytes, uncached */
	SceUInt32 reserved;
} SmschedRequest;

_Static_assert(sizeof(SmschedRequest) == 200, "SmschedRequest");

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
