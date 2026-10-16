/* Native intr-kind 0x14 producer and scheduling probe metadata. */
#include "priority_change.h"

/* The first descriptor word's framework role remains unproved. */
static uint32_t s_schedChangePriorityProbeId;
volatile uintptr_t s_schedChangePriorityHandler;
extern const char s_schedChangePriorityCodeSite[];

const struct ThreadmgrSchedProbeDescriptor s_schedChangePriorityDescriptor = {
	.probeId = &s_schedChangePriorityProbeId,
	.provider = "__sched_",
	.name = "change__pri",
	.codeSite = s_schedChangePriorityCodeSite,
	.handler = &s_schedChangePriorityHandler,
	.reserved = { 0 },
};

/* The native name is unavailable; the payload establishes this helper's role. */
__attribute__((noinline))
void issueCallPAPriorityHookIntrRequest(unsigned int cpuId, SceUID processId,
					SceUID threadId, int oldPriority,
					int newPriority)
{
	struct ThreadmgrIntrRequest *request;
	ThreadmgrTraceHook hook;

	request = allocIntrRequest(INTR_REQUEST_CALL_PA_PRIORITY_HOOK);
	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = processId;
	request->payload[1] = threadId;
	request->payload[2] = oldPriority;
	request->payload[3] = newPriority;
	if (hook)
		hook(0x2d, 4, processId, threadId, oldPriority, newPriority);
	issueIntrRequest(cpuId, request);
}
