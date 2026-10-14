/* Native CPU-affinity transition contracts. */
#pragma once
#include "scheduler.h"

int checkCpuAffinityMaskPhy(uint32_t mask, uint32_t attributes, SceProcessObject *pProcess);
uint32_t sceKernelGetDefaultCpuAffinityMask(ProcessBudget *budget);
void issueChangeThreadCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t mask);
void issueChangeThreadPriorityAndCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId,
				       int priority, uint32_t mask);
void notifyOffCpuIntr(uint32_t reason);
void runningThreadPreemptByIntr(struct ThreadCB *thread, uint32_t reason);
int changeThreadCpuAffinityMask(SceUID threadId, uint32_t mask, uint32_t *previousMask);
int changeThreadPriorityAndCpuAffinityMask(struct ThreadCB *thread,
	int priority, uint32_t mask, int *previousPriority, uint32_t *previousMask);
