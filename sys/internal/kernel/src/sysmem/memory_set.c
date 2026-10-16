/* Debug diagnostics wrap the shared optimized fill engine. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include <sce/debug.h>

extern void *SetMemoryCore(void *destination, int value, size_t size);

#if FW_DEBUG
#undef __FILE_NAME__
#define __FILE_NAME__ "memset_chk.c"

void *memset(void *destination, int value, size_t size)
{
	const void *caller = __builtin_return_address(0);

	if (!size) {
		ksceKernelPrintfLevelWithInfo(2, 0, SCE_DBGINFO(35),
			"*** warning: memset(%p,0x%02x,0) size zero(lr=%p)\n",
			destination, value, caller);
		if (ksceKernelGetAssertLevel() > 1)
			ksceKernelAssertLevel(2, 0, SCE_DBGINFO(36), caller);
	} else if ((int32_t)size < 0) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(40),
			"*** warning: memset(%p,0x%02x,0x%08x) size negative(lr=%p)\n",
			destination, value, size, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, SCE_DBGINFO(41), caller);
	}
	return SetMemoryCore(destination, value, size);
}
#endif
