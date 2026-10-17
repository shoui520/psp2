/*
 * SceSysLibTrace system call tracing.
 *
 * Every function of a traced syscall-exported library is re-registered with
 * the trampoline as its syscall handler. The trampoline calls the enter hook,
 * which swaps in the real function (remembered per syscall number) and the
 * client's enter callback, calls it, then the return hook, which restores the
 * caller's return address and calls the client's return callback.
 */

#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <syslibtrace/syslibtrace.h>
#include <threadmgr/threadcb.h>
#include "syslibtrace.h"
#include <stddef.h>

#define LIBENT_LIST_SIZE	0x1000
#define LIBENT_LIST_NUM		(LIBENT_LIST_SIZE / sizeof(SceSyslibtraceLibEntry *))

/* registers saved by the trampoline around each hook */
typedef struct SyslibtraceSyscallFrame {
	SceUInt32 r[4];
	SceUInt32 ip;
	void *lr;
} SyslibtraceSyscallFrame;

_Static_assert(offsetof(SceSyslibtraceLibEntry, enterArg) == 0x24, "library entry args");

SceSyslibtraceSyscallInfo g_syslibtraceSyscallInfo[SCE_SYSLIBTRACE_NUM_SYSCALLS];

SceSyslibtraceSyscallCallback g_enterCallback;
SceSyslibtraceSyscallCallback g_returnCallback;
SceSyslibtraceLibCallback g_registerLibCallback;
SceSyslibtraceLibCallback g_deregisterLibCallback;
SceSyslibtraceLibCallback g_setTargetCallback;
SceSyslibtraceLibCallback g_clearTargetCallback;

static inline int IsSyscallExport(const SceSyslibtraceLibEntry *lib)
{
	return (lib->exports->attribute & SCE_LIBRARY_ATTR_SYSCALL_EXPORT) != 0;
}

/* called by the trampoline before the traced function: frame->lr becomes the function */
void SceSyslibtraceSyscallEnterHook(SyslibtraceSyscallFrame *frame)
{
	ThreadCB *thread = getCurrentThreadCB();
	SceSyslibtraceSyscallInfo *info;
	SceSyslibtraceSyscallCallback callback;
	SceUInt32 number;

	SCE_DBG_ASSERT(frame != NULL, 75);
	SCE_DBG_ASSERT(thread != NULL, 76);
	SCE_DBG_ASSERT(thread->syslibtraceState != NULL, 77);

	thread->syslibtraceState->savedLr = frame->lr;
	number = thread->userFrame->r[12];
	SCE_DBG_ASSERT(number < SCE_SYSLIBTRACE_NUM_SYSCALLS, 85);

	info = &g_syslibtraceSyscallInfo[number];
	callback = g_enterCallback;
	frame->lr = info->entry;
	if (info != NULL && callback != NULL)
		callback(info->libraryNid, info->nid, info->index, info->enterArg,
			 frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
}

/* called by the trampoline after the traced function: frame->lr is the caller again */
void SceSyslibtraceSyscallReturnHook(SyslibtraceSyscallFrame *frame)
{
	ThreadCB *thread = getCurrentThreadCB();
	SceSyslibtraceSyscallInfo *info;
	SceSyslibtraceSyscallCallback callback;
	SceUInt32 number;

	SCE_DBG_ASSERT(frame != NULL, 107);
	SCE_DBG_ASSERT(thread != NULL, 108);
	SCE_DBG_ASSERT(thread->syslibtraceState != NULL, 109);

	frame->lr = thread->syslibtraceState->savedLr;
	number = thread->userFrame->r[12];
	SCE_DBG_ASSERT(number < SCE_SYSLIBTRACE_NUM_SYSCALLS, 117);

	info = &g_syslibtraceSyscallInfo[number];
	callback = g_returnCallback;
	if (info != NULL && callback != NULL)
		callback(info->libraryNid, info->nid, info->index, info->returnArg,
			 frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
}

/* name unknown: called by module start, nothing to set up */
__attribute__((noipa)) int SyslibtraceInitialize(void)
{
	return 0;
}

/* name unknown: install the client's callbacks */
int SceSyslibtraceForKernel_F9CB8BA9(const SceSyslibtraceCallbacks *callbacks, SceUInt32 flags)
{
	if (flags != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	g_enterCallback = callbacks->enter;
	g_returnCallback = callbacks->leave;
	g_registerLibCallback = callbacks->registerLib;
	g_deregisterLibCallback = callbacks->deregisterLib;
	g_setTargetCallback = callbacks->setTarget;
	g_clearTargetCallback = callbacks->clearTarget;
	return sceKernelSetSyslibtraceThreadCBEnable(1);
}

/* name unknown: remove the client's callbacks */
int SceSyslibtraceForKernel_BD326721(SceUInt32 flags)
{
	if (flags != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	g_enterCallback = NULL;
	g_returnCallback = NULL;
	g_registerLibCallback = NULL;
	g_deregisterLibCallback = NULL;
	g_setTargetCallback = NULL;
	g_clearTargetCallback = NULL;
	sceKernelSetSyslibtraceThreadCBEnable(0);
	return 0;
}

/* report every syscall-exported library not yet registered to the register callback */
int sceSyslibtraceCallbackRegisterLibs(SceUInt32 flags)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i;
	int ret;

	if (flags != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT(0, 190);
		return ret;
	}

	for (i = 0; i < num; i++) {
		SceSyslibtraceLibEntry *lib = list[i];
		SceSyslibtraceLibCallback callback;

		if (!IsSyscallExport(lib) || (lib->traceFlags & SCE_SYSLIBTRACE_LIB_REGISTERED))
			continue;
		callback = g_registerLibCallback;
		if (callback == NULL)
			continue;
		callback(lib, &lib->enterArg, &lib->returnArg);
		list[i]->traceFlags |= SCE_SYSLIBTRACE_LIB_REGISTERED;
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

/* report every registered syscall-exported library to the deregister callback */
int sceSyslibtraceCallbackDeregisterLibs(SceUInt32 flags)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i;
	int ret;

	if (flags != 0)
		return SCE_ERROR_ERRNO_EINVAL;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT(0, 232);
		return ret;
	}

	for (i = 0; i < num; i++) {
		SceSyslibtraceLibEntry *lib = list[i];
		SceSyslibtraceLibCallback callback;

		if (!IsSyscallExport(lib) || !(lib->traceFlags & SCE_SYSLIBTRACE_LIB_REGISTERED))
			continue;
		callback = g_deregisterLibCallback;
		if (callback == NULL)
			continue;
		callback(lib, &lib->enterArg, &lib->returnArg);
		list[i]->traceFlags &= ~SCE_SYSLIBTRACE_LIB_REGISTERED;
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

/*
 * Index range [*first, *end) of the libraries libraryNid selects: all of them,
 * or the first one with that NID. Returns 0 if there is none.
 */
static inline int FindTargetLibs(SceSyslibtraceLibEntry **list, SceSize num, SceUInt32 libraryNid,
				 SceUInt32 *first, SceUInt32 *end)
{
	SceUInt32 i;

	if (libraryNid == SCE_SYSLIBTRACE_ALL_LIBS) {
		*first = 0;
		*end = num;
		return 1;
	}
	for (i = 0; i < num; i++) {
		if (libraryNid == list[i]->exports->libraryNid) {
			*first = i;
			*end = i + 1;
			return 1;
		}
	}
	return 0;
}

/* select a syscall-exported library (or all of them) for tracing */
int sceSyslibtraceSetTargetSyscallLib(SceUInt32 libraryNid)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i, first, end;
	int ret;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT(0, 271);
		return ret;
	}

	if (FindTargetLibs(list, num, libraryNid, &first, &end)) {
		for (i = first; i < end; i++) {
			SceSyslibtraceLibEntry *lib = list[i];
			SceSyslibtraceLibCallback callback;

			if (!IsSyscallExport(lib) || (lib->traceFlags & SCE_SYSLIBTRACE_LIB_TARGET))
				continue;
			callback = g_setTargetCallback;
			lib->traceFlags |= SCE_SYSLIBTRACE_LIB_TARGET;
			if (callback != NULL)
				callback(lib, &lib->enterArg, &lib->returnArg);
		}
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

/* deselect a syscall-exported library (or all of them) */
int sceSyslibtraceClearTargetSyscallLib(SceUInt32 libraryNid)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i, first, end;
	int ret;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT(0, 328);
		return ret;
	}

	if (FindTargetLibs(list, num, libraryNid, &first, &end)) {
		for (i = first; i < end; i++) {
			SceSyslibtraceLibEntry *lib = list[i];
			SceSyslibtraceLibCallback callback;

			if (!IsSyscallExport(lib) || !(lib->traceFlags & SCE_SYSLIBTRACE_LIB_TARGET))
				continue;
			callback = g_clearTargetCallback;
			lib->traceFlags &= ~SCE_SYSLIBTRACE_LIB_TARGET;
			if (callback != NULL)
				callback(lib, &lib->enterArg, &lib->returnArg);
		}
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

static inline SceUInt32 SyscallNumber(const SceSyslibtraceLibEntry *lib, SceUInt32 index)
{
	return (SceUInt32)(lib->syscallBase + index) % SCE_SYSLIBTRACE_NUM_SYSCALLS;
}

/* route the syscalls of every target library through the trampoline */
int sceSyslibtraceStartSyscallTrace(void)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i, k;
	int ret;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT_LEVEL(0, 0, 386);
		return ret;
	}

	for (i = 0; i < num; i++) {
		SceSyslibtraceLibEntry *lib = list[i];

		if (!(lib->traceFlags & SCE_SYSLIBTRACE_LIB_TARGET) ||
		    (lib->traceFlags & SCE_SYSLIBTRACE_LIB_TRACING))
			continue;
		lib->traceFlags |= SCE_SYSLIBTRACE_LIB_TRACING;

		for (k = 0; k < lib->exports->numFunctions; k++) {
			SceUInt32 number = SyscallNumber(lib, k);
			SceSyslibtraceSyscallInfo *info = &g_syslibtraceSyscallInfo[number];
			const SceModuleExports *exports;

			ksceKernelRegisterSyscall(number, SyslibtraceSyscallTrampoline);
			lib = list[i];
			exports = lib->exports;
			info->entry = exports->entryTable[k];
			info->libraryNid = exports->libraryNid;
			info->nid = exports->nidTable[k];
			info->index = k;
			info->enterArg = lib->enterArg;
			info->returnArg = lib->returnArg;
		}
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

/* give the traced syscalls their functions back */
int sceSyslibtraceStopSyscallTrace(void)
{
	SceSyslibtraceLibEntry **list;
	SceSize num;
	SceUInt32 i, k;
	int ret;

	list = SceSysmemForKernel_85571907(LIBENT_LIST_SIZE);
	if (list == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	num = LIBENT_LIST_NUM;
	ret = ksceKernelGetLibEntCBListForSyslibtrace((void **)list, &num);
	if (ret < 0) {
		SceSysmemForKernel_4233C16D(list);
		SCE_DBG_ASSERT_LEVEL(0, 0, 438);
		return ret;
	}

	for (i = 0; i < num; i++) {
		SceSyslibtraceLibEntry *lib = list[i];

		if ((lib->traceFlags & (SCE_SYSLIBTRACE_LIB_TARGET | SCE_SYSLIBTRACE_LIB_TRACING)) !=
		    (SCE_SYSLIBTRACE_LIB_TARGET | SCE_SYSLIBTRACE_LIB_TRACING))
			continue;
		lib->traceFlags &= ~SCE_SYSLIBTRACE_LIB_TRACING;

		for (k = 0; k < lib->exports->numFunctions; k++) {
			lib = list[i];
			ksceKernelRegisterSyscall(SyscallNumber(lib, k), lib->exports->entryTable[k]);
		}
	}

	SceSysmemForKernel_4233C16D(list);
	return 0;
}

/* name unknown: the NID of the function traced at a syscall number */
int SceSyslibtraceForKernel_7CC73CDA(SceUInt32 syscall, SceUInt32 *nid)
{
	if (nid == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (syscall >= SCE_SYSLIBTRACE_NUM_SYSCALLS)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	*nid = g_syslibtraceSyscallInfo[syscall].nid;
	return 0;
}
