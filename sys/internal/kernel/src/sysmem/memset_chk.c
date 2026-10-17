/* Debug diagnostics wrap the shared optimized fill engine. */
#include <stddef.h>
#include <stdint.h>
#include <fw.h>
#include <sysmem/debug.h>

extern void *SetMemoryCore(void *destination, int value, size_t size);

#if FW_DEBUG
void *memset(void *destination, int value, size_t size)
{
	const void *caller = __builtin_return_address(0);

	if (!size) {
		SceDebugForDriver_D9703808(2, 0, SCE_DBGINFO(35),
			"*** warning: memset(%p,0x%02x,0) size zero(lr=%p)\n",
			destination, value, caller);
		if (SceDebugForDriver_374B7868() > 1)
			SceDebugForDriver_FD753E7A(2, 0, SCE_DBGINFO(36), caller);
	} else if ((int32_t)size < 0) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(40),
			"*** warning: memset(%p,0x%02x,0x%08x) size negative(lr=%p)\n",
			destination, value, size, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0, SCE_DBGINFO(41), caller);
	}
	return SetMemoryCore(destination, value, size);
}
#endif
