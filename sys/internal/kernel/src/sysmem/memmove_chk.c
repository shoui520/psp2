/* Debug memmove diagnostics precede the shared move engine. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include <sysmem/debug.h>

extern void *MoveMemoryCore(void *destination, const void *source, size_t size);

#if FW_DEBUG
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
#endif
