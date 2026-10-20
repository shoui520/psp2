/* Recovered scheduler primitives; dispatch and lifecycle remain incomplete. */
#pragma once

#include <stdbool.h>
#include "private.h"

unsigned int moveStandbyToReady(void);
void addReadyTop(struct ThreadCB *thread);
void addReady(struct ThreadCB *thread);
void addStandby(struct ThreadMgrCpuCB *cpu, struct ThreadCB *thread);
int selectExecCpu(struct ThreadCB *thread);
int distributeThread(int cpuId, struct ThreadCB *thread);
void removeCommonRaw(struct ThreadCB *thread, int priority);
void removeReady(struct ThreadCB *thread, int priority);
uint32_t searchHighestPriority(void);

_Static_assert(offsetof(struct ThreadCB, status) == 0x9c,
	       "thread public state flags");
_Static_assert(offsetof(struct ThreadCB, currentCpuAffinityMask) == 0xc0,
	       "thread CPU affinity mask");
_Static_assert(offsetof(struct ThreadMgrCpuCB, processId) == 0x14,
	       "CPU process identity");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, lastSelectedCpu) == 0x2208,
	       "CPU selection rotation state");

void notifyOffCpu(uint32_t reason);
void findNextRunningThread(struct ThreadCB *current);
int checkThreadUserStack(struct ThreadCB *thread);
void issueCallPAScheduleHookIntrRequest(unsigned int cpuId, SceUID processId, SceUID threadId,
		 uint32_t event, int priority);
struct ThreadCB *setupNextContext_thread(
	struct ThreadCB *current, uint32_t reason,
	SceUID oldThreadId, int oldPriority);
void switchThread(uint32_t reason);
void switchThread2(uint32_t reason);

