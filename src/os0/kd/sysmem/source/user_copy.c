/*
 * SceSysmem: copies between kernel and user memory. The user side is touched
 * only by the vmaccess.S primitives, so a bad user pointer returns an error
 * instead of faulting the kernel. The kernel side is touched first (its first,
 * middle and last byte) so that its pages fault in before the domain access
 * changes.
 *
 * The *Proc variants do the same in another process's address space, with
 * interrupts off while its translation context is in.
 */

#include "sce/cp15.h"
#include "sysmem_internal.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "sysmem.c"

/* the domain access while user memory is touched */
#define USER_COPY_DACR_TEXT		0x15450fc3	/* user text writable */
#define USER_COPY_DACR_TEXT_DBGP	0x1545cfc3	/* the same, for the debugger */

/* vmaccess.S */
int VmaccessCopyFromUser(void *dst, const void *userSrc, SceSize size);
int VmaccessCopyToUser(void *userDst, const void *src, SceSize size);
int VmaccessFillUser32(SceUInt32 *userDst, SceUInt32 value, SceSize count);
int VmaccessCopyUserToUser(void *userDst, const void *userSrc, SceSize size);
SceSize VmaccessStrnlenUser(const char *userStr, SceSize max);
SceSSize VmaccessStrncpyFromUser(char *dst, const char *userSrc, SceSize n);
SceSSize VmaccessStrncpyToUser(char *userDst, const char *src, SceSize n);
SceSSize VmaccessCountFillUser32(const void *userSrc, SceUInt32 value, SceSize size);
SceSSize VmaccessCountFillUser64(const void *userSrc, SceUInt32 low, SceUInt32 high, SceSize size);

int AddressSpaceSwitchContext(AddressSpaceObject *as);

/* fault in the kernel buffer's pages */
static inline void UserCopyTouchKernel(const void *buffer, SceSize size)
{
	const volatile SceUInt8 *p = buffer;

	(void)p[0];
	(void)p[size - 1];
	(void)p[size >> 1];
}

/* fault in a user page, with an unprivileged read */
static inline void UserCopyTouchUser(const void *user)
{
	SceUInt32 value;

	__asm__ volatile("ldrbt %0, [%1]" : "=r"(value) : "r"(user) : "memory");
}

static inline void UserCopyTouchUser32(const void *user)
{
	SceUInt32 value;

	__asm__ volatile("ldrt %0, [%1]" : "=r"(value) : "r"(user) : "memory");
}

int ksceKernelCopyFromUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0)
		UserCopyTouchKernel(dst, size);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyFromUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

/*
 * In pid's address space: interrupts off, its translation context in, the
 * call, back to ours.
 */
#define USER_COPY_IN_PROCESS(_pid, _ret, _call) do {			\
	MMUContext saved;						\
	void *as;							\
	int flags = ksceKernelCpuSuspendIntr();				\
									\
	MMUContextSave(&saved);						\
	_ret = SysrootPIDtoAddressSpaceCB((_pid), &as);			\
	if (_ret >= 0) {						\
		_ret = AddressSpaceSwitchContext(as);			\
		if (_ret >= 0)						\
			_ret = (_call);					\
	}								\
	MMUContextRestore(&saved);					\
	ksceKernelCpuResumeIntr(flags);					\
} while (0)

int ksceKernelCopyFromUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 224, "sceKernelCopyFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyFromUser(dst, src, size));
	return ret;
}

int ksceKernelCopyToUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0)
		UserCopyTouchKernel(src, size);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 280, "sceKernelCopyToUserProc");
	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyToUser(dst, src, size));
	return ret;
}

/* into user memory whatever its domain */
int ksceKernelCopyToUserDomain(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0) {
		UserCopyTouchKernel(src, size);
		UserCopyTouchUser(dst);
	}
	dacr = readDacr();
	writeDacr(USER_COPY_DACR_TEXT);
	ret = VmaccessCopyToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

/* into user text: the written lines are cleaned for the instruction fetch */
static inline __attribute__((always_inline))
int UserCopyToText(void *dst, const void *src, SceSize size, SceUInt32 textDacr)
{
	SceUInt32 dacr = readDacr();
	int ret;

	writeDacr(textDacr);
	ret = VmaccessCopyToUser(dst, src, size);
	ksceKernelDcacheCleanRange((void *)((uintptr_t)dst & ~0x1f),
				   (((uintptr_t)dst + size + 0x1f) & ~0x1f) -
				   ((uintptr_t)dst & ~0x1f));
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyToUserTextDomain(void *dst, const void *src, SceSize size)
{
	if (size != 0) {
		UserCopyTouchKernel(src, size);
		UserCopyTouchUser(dst);
	}
	return UserCopyToText(dst, src, size, USER_COPY_DACR_TEXT);
}

int ksceKernelCopyToUserProcTextDomain(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyToUserTextDomain(dst, src, size));
	return ret;
}

int sceKernelCopyToUserProcTextDomainForDBGP(SceUID pid, void *dst, const void *src,
					     SceSize size)
{
	MMUContext saved;
	void *as;
	int flags, ret;

	SCE_DBG_ASSERT(pid != SCE_KERNEL_PROCESS_ID, 439);
	if (size != 0)
		UserCopyTouchKernel(src, size);
	flags = ksceKernelCpuSuspendIntr();
	MMUContextSave(&saved);
	ret = SysrootPIDtoAddressSpaceCB(pid, &as);
	if (ret >= 0) {
		ret = AddressSpaceSwitchContext(as);
		if (ret >= 0) {
			if (size != 0)
				UserCopyTouchUser(dst);
			ret = UserCopyToText(dst, src, size, USER_COPY_DACR_TEXT_DBGP);
		}
	}
	MMUContextRestore(&saved);
	ksceKernelCpuResumeIntr(flags);
	return ret;
}

/* name unknown: fills user words (text domain) with a value */
int SceSysmemForKernel_FCB5745A(SceUInt32 *dst, SceUInt32 value, SceSize count)
{
	SceUInt32 dacr;
	int ret;

	if (count != 0)
		UserCopyTouchUser32(dst);
	dacr = readDacr();
	writeDacr(USER_COPY_DACR_TEXT);
	ret = VmaccessFillUser32(dst, value, count);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyFromToUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr = readDacr();
	int ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyUserToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyFromToUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 551, "sceKernelCopyFromToUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCopyUserToUser(dst, src, size));
	return ret;
}

/* the bytes from src on that hold value, word by word */
SceSSize ksceKernelCountFillValueFromUser(const void *src, SceUInt32 value, SceSize size)
{
	SceUInt32 dacr = readDacr();
	SceSSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCountFillUser32(src, value, size);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelCountFillValueFromUserProc(SceUID pid, const void *src, SceUInt32 value,
					     SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 595, "sceKernelCountFillValueFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCountFillUser32(src, value, size));
	return ret;
}

SceSSize ksceKernelCountFillValue64FromUser(const void *src, SceUInt64 value, SceSize size)
{
	SceUInt32 dacr = readDacr();
	SceSSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCountFillUser64(src, (SceUInt32)value, (SceUInt32)(value >> 32), size);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelCountFillValue64FromUserProc(SceUID pid, const void *src, SceUInt64 value,
					       SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 639, "sceKernelCountFillValue64FromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCountFillUser64(src, (SceUInt32)value,
							       (SceUInt32)(value >> 32), size));
	return ret;
}

SceSize ksceKernelStrnlenFromUser(const char *str, SceSize max)
{
	SceUInt32 dacr = readDacr();
	SceSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrnlenUser(str, max);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrnlenFromUserProc(SceUID pid, const char *str, SceSize max)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 679, "sceKernelStrnlenFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrnlenUser(str, max));
	return ret;
}

SceSSize ksceKernelStrncpyFromUser(char *dst, const char *src, SceSize n)
{
	SceUInt32 dacr;
	SceSSize ret;

	if (n != 0)
		UserCopyTouchKernel(dst, n);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrncpyFromUser(dst, src, n);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrncpyFromUserProc(SceUID pid, char *dst, const char *src, SceSize n)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 734, "sceKernelStrncpyFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrncpyFromUser(dst, src, n));
	return ret;
}

SceSSize ksceKernelStrncpyToUser(char *dst, const char *src, SceSize n)
{
	SceUInt32 dacr;
	SceSSize ret;

	(void)*(const volatile char *)src;
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrncpyToUser(dst, src, n);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrncpyToUserProc(SceUID pid, char *dst, const char *src, SceSize n)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 786, "sceKernelStrncpyToUserProc");
	(void)*(const volatile char *)src;
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrncpyToUser(dst, src, n));
	return ret;
}
