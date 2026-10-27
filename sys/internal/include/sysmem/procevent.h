/*
 * SceProcEventForDriver (os0:kd/sysmem.skprx, procevent.c): process event handlers. A driver
 * registers a SceKernelProcEventHandler; processmgr invokes every handler for an event, with
 * the event's parameters, and each handler gets back the pointer it registered with (common).
 */
#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the events (sceKernelInvokeProcEventHandler; Sony's names: "CREATE", "EXIT", ...) */
#define PROC_EVENT_CREATE		1
#define PROC_EVENT_EXIT			2	/* also a failed create */
#define PROC_EVENT_KILL			3
#define PROC_EVENT_SUSPEND		4
#define PROC_EVENT_RESUME		5
#define PROC_EVENT_PRELOAD		0x10
#define PROC_EVENT_SUSPEND_DONE		0x11

/* what processmgr passes to the exit, kill, suspend and resume handlers */
typedef struct ProcEventParam {
	SceSize size;
	SceUInt32 state;
	SceUInt32 kind;
	SceUInt32 type;
} ProcEventParam;

/* what processmgr passes to the create handlers */
typedef struct ProcEventCreateParam {
	SceSize size;
	SceUID pid;			/* processmgr gives 0 */
	SceUInt32 kind;
	SceUID prevPid;
	SceUInt32 type;
} ProcEventCreateParam;

typedef int (*ProcEventCreateHandler)(SceUID pid, ProcEventCreateParam *param, void *common);
typedef int (*ProcEventHandler)(SceUID pid, ProcEventParam *param, void *common);
typedef int (*ProcEventPhaseHandler)(SceUID pid, int phase, ProcEventParam *param, void *common);
typedef int (*ProcEventNotifyHandler)(int event, int phase, void *param, void *common);

/* what a driver registers (size 0x18, or 0x1c with notify) */
typedef struct SceKernelProcEventHandler {
	SceSize size;
	ProcEventCreateHandler create;
	ProcEventHandler exit;
	ProcEventHandler kill;
	ProcEventPhaseHandler suspend;		/* 0x10 */
	ProcEventPhaseHandler resume;
	ProcEventNotifyHandler notify;
} SceKernelProcEventHandler;

#ifndef __cplusplus
_Static_assert(sizeof(ProcEventParam) == 0x10, "ProcEventParam size");
_Static_assert(sizeof(ProcEventCreateParam) == 0x14, "ProcEventCreateParam size");
_Static_assert(sizeof(SceKernelProcEventHandler) == 0x1c, "SceKernelProcEventHandler size");
#endif

int sceKernelUnregisterProcEventHandler(SceUID uid);
int sceKernelInvokeProcEventHandler(SceUID pid, int event, int phase, void *info, SceUID *failed, SceUID stop);
SceUID sceKernelRegisterProcEventHandler(const char *name, const SceKernelProcEventHandler *handler, void *common);

#ifdef __cplusplus
}
#endif
