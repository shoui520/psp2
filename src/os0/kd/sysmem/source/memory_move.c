/* Debug memory diagnostics retain Sony's source lines and assert thresholds. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include <sce/debug.h>

extern void *MoveMemoryCore(void *destination, const void *source, size_t size);
extern int CompareMemoryCore(const void *left, const void *right, size_t size);

#if FW_DEBUG
#undef __FILE_NAME__
#define __FILE_NAME__ "memmove_chk.c"

void *memmove(void *destination, const void *source, size_t size)
{
	const void *caller = __builtin_return_address(0);

	if (!size) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(35),
			"*** warning: memmove(%p,%p,0) size zero(lr=%p)\n",
			destination, source, caller);
		if (ksceKernelGetAssertLevel() > 1)
			ksceKernelAssertLevel(2, 0, SCE_DBGINFO(36), caller);
	} else if ((int32_t)size < 0) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(40),
			"*** warning: memmove(%p,%p,0x%08x) size negative(lr=%p)\n",
			destination, source, size, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, SCE_DBGINFO(41), caller);
	}
	return MoveMemoryCore(destination, source, size);
}

#undef __FILE_NAME__
#define __FILE_NAME__ "memcmp_chk.c"

int memcmp(const void *left, const void *right, size_t size)
{
	const void *caller = __builtin_return_address(0);

	if (!size) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(37),
			"*** warning: memcmp(%p,%p,0) size zero(lr=%p)\n",
			left, right, caller);
		if (ksceKernelGetAssertLevel() > 1)
			ksceKernelAssertLevel(2, 0, SCE_DBGINFO(38), caller);
	} else if ((int32_t)size < 0) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(42),
			"*** warning: memcmp(%p,%p,0x%08x) size negative(lr=%p)\n",
			left, right, size, caller);
		if (ksceKernelGetAssertLevel() > 1)
			ksceKernelAssertLevel(2, 0, SCE_DBGINFO(43), caller);
	} else if (left == right) {
		ksceKernelPrintfLevelWithInfo(2, 0, SCE_DBGINFO(47),
			"*** warning: memcmp(%p,%p,0x%08x) s1 == s2(lr=%p)\n",
			left, right, size, caller);
	}
	return CompareMemoryCore(left, right, size);
}
#endif
