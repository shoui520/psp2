/* Native priority-change callbacks and unresolved DTrace descriptor fields. */
#pragma once

#include "private.h"

struct ThreadmgrSchedProbeDescriptor {
	uint32_t *probeId;			/* the DTrace module's ID for it */
	const char *provider;
	const char *name;
	const void *codeSite;
	volatile uintptr_t *handler;
	uint32_t reserved[5];
};

/* a probe site: its ID, its handler and the descriptor that names its code site */
#define SCHED_PROBE(symbol, providerName, probeName)				\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = providerName, .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

extern volatile uintptr_t s_schedChangePriorityHandler;
extern const struct ThreadmgrSchedProbeDescriptor s_schedChangePriorityDescriptor;

void issueCallPAPriorityHookIntrRequest(unsigned int cpuId, SceUID processId,
					SceUID threadId, int oldPriority,
					int newPriority);
int changeThreadPriority(struct ThreadCB *thread, int priority);

_Static_assert(sizeof(struct ThreadmgrSchedProbeDescriptor) == 0x28,
	       "native scheduling probe descriptor size");
