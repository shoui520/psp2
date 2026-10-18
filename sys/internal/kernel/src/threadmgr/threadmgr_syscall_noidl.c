/*
 * SceKernelThreadMgr (threadmgr_syscall_noidl.c): the thread creation
 * syscall. Its arguments past the third come in a user block
 * (extargs) that is copied in with the option and the name.
 */

#include <string.h>
#include <threadmgr/threadmgr.h>
#include "private.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>

#define THREAD_ATTR_USER		0x80000000
#define THREAD_NAME_SIZE		0x20
#define SCE_KERNEL_ERROR_ILLEGAL_SIZE	0x8002000b

/* what sceKernelCreateThread passes past its third argument */
struct ThreadmgrCreateThreadExtArgs {
	SceSize size;				/* PLACEHOLDER: not checked */
	SceSize stackSize;
	uint32_t attr;
	uint32_t cpuAffinityMask;		/* 0x0c: in bits 16-19 */
	const SceKernelThreadOptParam *option;	/* 0x10 */
	const void *callerAddr;			/* 0x14: where the user library was called from */
};

SceUID createThread(SceUID pid, const char *name, uintptr_t entry, int initPriority,
		    SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
		    const SceKernelThreadOptParam *option, uint32_t flags, const void *callerAddr);

/* sceKernelCreateThreadForUser */
SceUID sceKernelCreateThreadForUser(const char *name, uintptr_t entry, int initPriority,
				    const struct ThreadmgrCreateThreadExtArgs *extargs)
{
	struct ThreadmgrCreateThreadExtArgs extargs_ks;
	SceKernelThreadOptParam option;
	char name_ks[THREAD_NAME_SIZE];
	SceUID ret;

	ret = SceSysrootForDriver_9AF075D4(0);
	if (ret < 0)
		return ret;
	ret = sceKernelCopyFromUser(&extargs_ks, extargs, sizeof(extargs_ks));
	if (ret < 0)
		goto out;
	ret = sceKernelStrncpyFromUser(name_ks, name, sizeof(name_ks));
	if (ret < 0)
		goto out;
	name_ks[sizeof(name_ks) - 1] = '\0';
	if (extargs_ks.option != NULL) {
		ret = sceKernelCopyFromUser(&option.size, extargs_ks.option, sizeof(option.size));
		if (ret < 0)
			goto out;
		if (option.size > sizeof(option)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
			goto out;
		}
		ret = sceKernelCopyFromUser(&option, extargs_ks.option, option.size);
		if (ret < 0)
			goto out;
	} else {
		option = (SceKernelThreadOptParam){ .size = sizeof(option) };
	}
	extargs_ks.attr |= THREAD_ATTR_USER;
	if (extargs_ks.cpuAffinityMask & ~0xf0000) {
		SCE_DBG_PRINTF(0, 150, "*** error: sceKernelCreateThreadForUser(), "
			       "extargs_ks.cpuAffinityMask=0x%08X\n", extargs_ks.cpuAffinityMask);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		goto out;
	}
	extargs_ks.cpuAffinityMask = (int)extargs_ks.cpuAffinityMask >> 16;
	ret = createThread(0, name_ks, entry, initPriority, extargs_ks.stackSize, extargs_ks.attr,
			   extargs_ks.cpuAffinityMask, &option, 1, extargs_ks.callerAddr);
out:
	SceSysrootForDriver_1B3FA457(0);
	return ret;
}
