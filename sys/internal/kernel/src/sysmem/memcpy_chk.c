/* Debug memcpy diagnostics precede the shared optimized copy engine. */
#include <stddef.h>
#include <stdint.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

extern void *CopyMemoryForward(void *destination, const void *source, size_t size);

#if FW_SINCE(0x04000000)
void *memcpy(void *destination, const void *source, size_t size)
{
	const void *caller = __builtin_return_address(0);
	uintptr_t dst = (uintptr_t)destination;
	uintptr_t src = (uintptr_t)source;

	if (!size) {
		SceDebugForDriver_D9703808(2, 0, SCE_DBGINFO(40),
			"*** warning: memcpy(%p,%p,0) size zero(lr=%p)\n",
			destination, source, caller);
		if (SceDebugForDriver_374B7868() > 1)
			SceDebugForDriver_FD753E7A(2, 0, SCE_DBGINFO(41), caller);
	} else if ((int32_t)size < 0) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(43),
			"*** warning: memcpy(%p,%p,0x%08x) size negative(lr=%p)\n",
			destination, source, size, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0, SCE_DBGINFO(44), caller);
	} else if (destination == source) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(46),
			"*** error: memcpy(%p,%p,0x%08x) src == dst(lr=%p)\n",
			destination, source, size, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0, SCE_DBGINFO(47), caller);
	} else if ((src >= dst && size > src - dst) || (dst >= src && size > dst - src)) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(50),
			"*** error: memcpy(%p,%p,0x%08x) src/dst overlap(lr=%p)\n",
			destination, source, size, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0, SCE_DBGINFO(51), caller);
	}
	return CopyMemoryForward(destination, source, size);
}
#endif
