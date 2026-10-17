/* Debug memcmp diagnostics precede the shared compare engine. */
#include <stddef.h>
#include <stdint.h>
#include <fw.h>
#include <sysmem/debug.h>

extern int CompareMemoryCore(const void *left, const void *right, size_t size);

#if FW_DEBUG
int memcmp(const void *left, const void *right, size_t size)
{
	const void *caller = __builtin_return_address(0);

	if (!size) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(37),
			"*** warning: memcmp(%p,%p,0) size zero(lr=%p)\n",
			left, right, caller);
		if (SceDebugForDriver_374B7868() > 1)
			SceDebugForDriver_FD753E7A(2, 0, SCE_DBGINFO(38), caller);
	} else if ((int32_t)size < 0) {
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(42),
			"*** warning: memcmp(%p,%p,0x%08x) size negative(lr=%p)\n",
			left, right, size, caller);
		if (SceDebugForDriver_374B7868() > 1)
			SceDebugForDriver_FD753E7A(2, 0, SCE_DBGINFO(43), caller);
	} else if (left == right) {
		SceDebugForDriver_D9703808(2, 0, SCE_DBGINFO(47),
			"*** warning: memcmp(%p,%p,0x%08x) s1 == s2(lr=%p)\n",
			left, right, size, caller);
	}
	return CompareMemoryCore(left, right, size);
}
#endif
