/*
 * Sys event handlers (SceKernelSuspendForDriver in os0:kd/sysmem.skprx), as used so far.
 */

#pragma once

#include <psp2kern/types.h>
#include <stdint.h>
#include "sce/sysevent_ids.h"

typedef int (*SceKernelSysEventHandler)(int resume, int eventid, void *args, void *opt);

SceUID ksceKernelRegisterSysEventHandler(const char *name, SceKernelSysEventHandler handler, void *argp);
int ksceKernelUnregisterSysEventHandler(SceUID uid);

/*
 * The argument ScePower passes with its events: a PowerSuspendEvent while
 * suspending (resume == 0), a PowerResumeEvent while resuming.
 */
#define POWER_SLEEP_SUSPEND		0
#define POWER_SLEEP_POWER_OFF		1	/* standby, shutdown or hibernation */
#define POWER_SLEEP_COLD_RESET		2

struct PowerMonitorState;
struct PowerResumeInformation;

struct PowerSuspendEvent {
	uint32_t size;
	int sleepMode;				/* POWER_SLEEP_* */
	uint64_t started;
	struct PowerMonitorState *monitorState;
	uint32_t reserved;
};

struct PowerResumeEvent {
	uint32_t size;
	struct PowerResumeInformation *resumeInformation;
};

_Static_assert(sizeof(struct PowerSuspendEvent) == 0x18, "PowerSuspendEvent");
