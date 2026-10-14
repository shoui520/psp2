/*
 * SceKernelThreadMgr (os0:kd/threadmgr.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

/* Sony's original eight-byte option; vitasdk also exposes a larger revision. */
typedef struct SceKernelThreadOptParamV1 {
	SceSize size;
	SceUInt32 attr;
} SceKernelThreadOptParamV1;

_Static_assert(sizeof(SceKernelThreadOptParamV1) == 8,
	"SceKernelThreadOptParamV1 size");

/* NID 0x9DCB4B7A (vitasdk: ksceKernelGetProcessId); our NID db names it this way */
SceUID sceKernelGetProcessId(void);

int ksceKernelChangeThreadCpuAffinityMask(SceUID thid, int mask);
int ksceKernelGetThreadCpuAffinityMask(SceUID thid);

/* our NID db name for the kernel thread exit (vitasdk: ksceKernelExitThread) */
int sceKernelExitThread(int status);

/* nonzero when the calling thread runs in user mode (a syscall) */
int sceKernelIsUserModeThread(void);

/* name unknown: nonzero while the caller may already own a lock it waits on (thread-state query) */
int SceThreadmgrForDriver_20C228E4(void);

/* ARM performance monitor control of a thread (0: the caller) */
int sceKernelPMonSetControlRegister(SceUID thid, SceUInt32 value);
int sceKernelPMonSetUserEnableRegister(SceUID thid, SceUInt32 value);
int sceKernelSetPMUSERENR(SceUID thid, SceUInt32 value);
int pmonCpuSetEventSelf(SceUID thread, unsigned int counter, unsigned int event);

/* Names unknown: per-thread ARM performance counters. */
int SceThreadmgrForKernel_7F831213(SceUID thread, SceUInt32 counters);
int SceThreadmgrForKernel_1D2A6815(SceUID thread, SceUInt32 counters);
int SceThreadmgrForKernel_7B3368F1(SceUID thread, unsigned int counter,
	SceUInt32 value);
int SceThreadmgrForKernel_170F69D6(SceUID thread, unsigned int counter,
	SceUInt32 *value);

/* Names unknown: install the on-CPU, off-CPU and thread trace hooks. */
void SceThreadmgrForKernel_A3975A5A(void (*hook)(uint32_t, uint32_t, uint16_t, uint8_t));
void SceThreadmgrForKernel_F885ECCA(void (*hook)(uint32_t, uint32_t, uint32_t));
void SceThreadmgrForKernel_9605DF7D(void (*hook)(uint32_t, uint32_t, uint16_t, uint8_t));

/* Simple events and event callbacks; option layout is not yet recovered here. */
typedef struct SceKernelSimpleEventOptParam SceKernelSimpleEventOptParam;

SceUID ksceKernelCreateSimpleEvent(const char *name, SceUInt32 attr,
	SceUInt32 initPattern, const SceKernelSimpleEventOptParam *opt);
/* arg: what the callback is notified with */
SceInt32 ksceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId, SceUID arg);
SceInt32 ksceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId);

/* Wait-context query and idle entry registration, recovered from ThreadMgr. */
int sceKernelCheckWaitableStatus(void);
void SceThreadmgrForDriver_B4A05763(void (*idleEntry)(void));

/* NID 0xB7DF3EDF: the kernel side of libkernel's sceKernelSetEventWithNotifyCallback. */
int sceKernelSetEventWithNotifyCallback(SceUID eventId, SceUInt32 pattern,
	SceUInt64 userData, SceInt32 argument);

/* Timers (SceThreadmgrForDriver; the non-k names are Sony's). */
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr, const void *opt);
int sceKernelSetTimerEvent(SceUID timer, SceInt32 type, SceKernelSysClock *interval, SceInt32 repeat);
int ksceKernelStartTimer(SceUID timer);
int ksceKernelStopTimer(SceUID timer);
int SceThreadmgrForDriver_91A8F38B(SceUID timer);	/* delete timer */
/* calls handler(arg) once, usec from now, in interrupt context */
SceUID ksceKernelRegisterTimer(const char *name, SceUInt32 usec, SceUInt32 (*handler)(void *arg),
			       void *arg);

int sceKernelDelayThread(SceUInt32 usec);
SceInt64 sceKernelGetSystemTimeWide(void);
int ksceKernelWaitEvent(SceUID event, SceUInt32 bits, SceUInt32 *result, SceUInt64 *userData,
			SceUInt32 *timeout);

/* thread-local slots (names unknown) */
/* allocate a TLS slot */
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *memory, uintptr_t argument),
				   uintptr_t argument);
int SceThreadmgrForDriver_8C3C1F78(int slot);				/* free TLS slot */
void *SceThreadmgrForDriver_3A72C6D8(int slot);				/* TLS slot of the current thread */
