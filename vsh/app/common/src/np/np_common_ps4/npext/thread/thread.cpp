/*
 * SceNpCommonPs4: threads.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

__attribute__((noinline)) int SceNpCommonPs4_04EC8796(SceUID *pThread, SceKernelThreadEntry entry, SceSize argSize, void *pArgp,
	int initPriority, SceSize stackSize, int cpuAffinity, const char *name)
{
	SceUInt32 affinityMask;
	SceUID thread;
	int result;

	affinityMask = 0;
	if (cpuAffinity == 255)
		affinityMask = 0x70000;
	thread = sceKernelCreateThread(name, entry, initPriority, stackSize, 0, affinityMask, NULL);
	result = thread;
	if (thread >= 0) {
		*pThread = thread;
		result = sceKernelStartThread(thread, argSize, pArgp);
		if (result < 0)
			sceKernelDeleteThread(*pThread);
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_CFB92A12(SceUID thread, int *pExitStatus)
{
	int result;

	result = sceKernelWaitThreadEnd(thread, pExitStatus, NULL);
	if (result >= 0) {
		result = sceKernelDeleteThread(thread);
		if (result >= 0)
			return 0;
	}
	return result;
}

int SceNpCommonPs4_5395A6B1(SceUID *pThreadId)
{
	int result;

	if (pThreadId == NULL)
		return NP_COMMON_PS4_ERROR_80558009;
	result = sceKernelGetThreadId();
	if (result < 0)
		return result;
	*pThreadId = result;
	return 0;
}

extern const void *g_threadVtable[];		/* 0x8102aa24 */

struct NpCommonPs4Thread *SceNpCommonPs4_5CE5C51F(struct NpCommonPs4Thread *thread)
{
	thread->vtable = NP_COMMON_PS4_VTABLE(g_threadVtable);
	thread->state = 0;
	return thread;
}

struct NpCommonPs4Thread *SceNpCommonPs4_04CBEB81(struct NpCommonPs4Thread *thread)
{
	thread->vtable = NP_COMMON_PS4_VTABLE(g_threadVtable);
	thread->state = 0;
	return thread;
}

struct NpCommonPs4Thread *SceNpCommonPs4_6D4C9272(struct NpCommonPs4Thread *thread)
{
	if (thread->state == 2) {
		thread->vtable = NP_COMMON_PS4_VTABLE(g_threadVtable);
		SceNpCommonPs4_CFB92A12(thread->thread, NULL);
	}
	thread->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return thread;
}

int SceNpCommonPs4_653C2CB0(struct NpCommonPs4Thread *thread)
{
	if (thread->state == 2) {
		thread->vtable = NP_COMMON_PS4_VTABLE(g_threadVtable);
		SceNpCommonPs4_CFB92A12(thread->thread, NULL);
	}
	thread->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(thread);
}

struct NpCommonPs4Thread *SceNpCommonPs4_502309F4(struct NpCommonPs4Thread *thread)
{
	if (thread->state == 2) {
		thread->vtable = NP_COMMON_PS4_VTABLE(g_threadVtable);
		SceNpCommonPs4_CFB92A12(thread->thread, NULL);
	}
	thread->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return thread;
}

int SceNpCommonPs4_D699FE34(struct NpCommonPs4Thread *thread, const char *name, int priority, SceSize stackSize,
	int cpuAffinity)
{
	if (thread->state != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	sceClibStrncpy(thread->name, name, 31);
	thread->priority = priority;
	thread->stackSize = stackSize;
	thread->name[31] = 0;
	thread->cpuAffinity = cpuAffinity;
	thread->exitStatus = -2141872385;
	thread->state = 1;
	return 0;
}

int SceNpCommonPs4_C8F1844F(struct NpCommonPs4Thread *thread, const struct NpCommonPs4HostParam *param)
{
	const char *name;
	SceSize stackSize;
	int priority;
	int cpuAffinity;
	int result;

	name = param->name;
	stackSize = param->stackSize;
	result = NP_COMMON_PS4_ERROR_80559E03;
	priority = param->priority;
	cpuAffinity = param->cpuAffinity;
	if (thread->state == 0) {
		sceClibStrncpy(thread->name, name, 31);
		thread->priority = priority;
		thread->stackSize = stackSize;
		thread->name[31] = 0;
		thread->cpuAffinity = cpuAffinity;
		thread->exitStatus = -2141872385;
		thread->state = 1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_8CBDFDB7(struct NpCommonPs4Thread *thread)
{
	static const char functionName[] = "Start";
	struct NpCommonPs4Thread *self;
	int result;

	NP_COMMON_PS4_ASSERT(thread->state == 1 || thread->state == 3, functionName, "npext\\thread\\thread.cpp", 126,
		"m_state == STATE_INIT || m_state == STATE_JOINED");
	self = thread;
	result = SceNpCommonPs4_04EC8796(&thread->thread, (SceKernelThreadEntry)npCommonPs4ThreadMain, 4, &self,
		thread->priority, thread->stackSize, thread->cpuAffinity, thread->name);
	if (result >= 0) {
		thread->state = 2;
		result = 0;
	}
	return result;
}

int SceNpCommonPs4_2C0585AD(struct NpCommonPs4Thread *thread, int *pExitStatus)
{
	static const char functionName[] = "Join";
	int result;

	NP_COMMON_PS4_ASSERT(thread->state == 2, functionName, "npext\\thread\\thread.cpp", 151, "m_state == STATE_RUNNING");
	result = SceNpCommonPs4_CFB92A12(thread->thread, NULL);
	if (result >= 0) {
		thread->state = 3;
		if (pExitStatus != NULL)
			*pExitStatus = thread->exitStatus;
		return 0;
	}
	return result;
}

int npCommonPs4ThreadMain(SceSize argSize, void *argp)
{
	struct NpCommonPs4Thread *thread;

	thread = *(struct NpCommonPs4Thread **)argp;
	thread->exitStatus = ((int (*)(struct NpCommonPs4Thread *))thread->vtable[2])(thread);
	return 0;
}

const void *g_threadVtable[] = {		/* 0x8102aa24 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_6D4C9272,
	(const void *)SceNpCommonPs4_653C2CB0,
	(const void *)npCommonPs4Breakpoint,
};

}
