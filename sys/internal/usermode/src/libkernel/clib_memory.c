/* LibKernel public entry points for the shared Sysclib memory engines. */
#include <stddef.h>
#include <stdint.h>
#include <libkernel/libkernel.h>
#include <sysmem/compiler.h>

extern void *SetMemoryCore(void *destination, int value, size_t size);
extern int CompareMemoryCore(const void *left, const void *right, size_t size);
extern void *MoveMemoryCore(void *destination, const void *source, size_t size);

void *sceClibMemcpy_safe(void *destination, const void *source, SceSize size)
{
	uintptr_t dst = (uintptr_t)destination;
	uintptr_t src = (uintptr_t)source;

	if ((int32_t)size > 0) {
		if (dst == src) {
			sceClibPrintf("*** error: sceClibMemcpy(%p,%p,0x%08x) src == dst(lr=%p)\n",
				     destination, source, size, __builtin_return_address(0));
			__breakpoint(BKPT_CHECK_FAILED);
		} else if ((dst <= src && size > src - dst) ||
			   (dst >= src && size > dst - src)) {
			sceClibPrintf("*** error: sceClibMemcpy(%p,%p,0x%08x) src/dst overlap(lr=%p)\n",
				     destination, source, size, __builtin_return_address(0));
			__breakpoint(BKPT_CHECK_FAILED);
		}
	}
	return sceClibMemcpy(destination, source, size);
}

void *sceClibMemcpyChk(void *destination, const void *source, SceSize size,
		     SceSize capacity)
{
	if (capacity < size)
		__breakpoint(BKPT_CHECK_FAILED);
	return sceClibMemcpy(destination, source, size);
}

void *sceClibMemmove(void *destination, const void *source, SceSize size)
{
	return MoveMemoryCore(destination, source, size);
}

void *sceClibMemmoveChk(void *destination, const void *source, SceSize size,
		      SceSize capacity)
{
	if (capacity < size)
		__breakpoint(BKPT_CHECK_FAILED);
	return MoveMemoryCore(destination, source, size);
}

void *sceClibMemset(void *destination, int value, SceSize size)
{
	return SetMemoryCore(destination, value, size);
}

void *sceClibMemsetChk(void *destination, int value, SceSize size,
		     SceSize capacity)
{
	if (capacity < size)
		__breakpoint(BKPT_CHECK_FAILED);
	return SetMemoryCore(destination, value, size);
}

int sceClibMemcmp(const void *left, const void *right, SceSize size)
{
	return CompareMemoryCore(left, right, size);
}
