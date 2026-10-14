/* Debug memcpy diagnostics precede the shared optimized copy engine. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include <sce/debug.h>

extern void *CopyMemoryForward(void *destination, const void *source, size_t size);

#if FW_SINCE(0x04000000)
#undef __FILE_NAME__
#define __FILE_NAME__ "memcpy_chk.c"

void *memcpy(void *destination, const void *source, size_t size)
{
	const void *caller = __builtin_return_address(0);
	uintptr_t dst = (uintptr_t)destination;
	uintptr_t src = (uintptr_t)source;

	if (!size) {
		ksceKernelPrintfLevelWithInfo(2, 0, SCE_DBGINFO(40),
			"*** warning: memcpy(%p,%p,0) size zero(lr=%p)\n",
			destination, source, caller);
		if (ksceKernelGetAssertLevel() > 1)
			ksceKernelAssertLevel(2, 0, SCE_DBGINFO(41), caller);
	} else if ((int32_t)size < 0) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(43),
			"*** warning: memcpy(%p,%p,0x%08x) size negative(lr=%p)\n",
			destination, source, size, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, SCE_DBGINFO(44), caller);
	} else if (destination == source) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(46),
			"*** error: memcpy(%p,%p,0x%08x) src == dst(lr=%p)\n",
			destination, source, size, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, SCE_DBGINFO(47), caller);
	} else if (size > (dst < src ? src - dst : dst - src)) {
		ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(50),
			"*** error: memcpy(%p,%p,0x%08x) src/dst overlap(lr=%p)\n",
			destination, source, size, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, SCE_DBGINFO(51), caller);
	}
	return CopyMemoryForward(destination, source, size);
}
#endif
