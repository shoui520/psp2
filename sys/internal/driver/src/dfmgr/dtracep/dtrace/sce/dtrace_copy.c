/*
 * DTrace's safe user-memory accesses (Sony's dtrace/sce/dtrace_copy.c): the
 * copies go through Sysmem's per-process copies, and a failure marks the CPU's
 * DTrace state with a bad address instead of faulting.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/utility.h>
#include <sysmem/sysmem.h>

#define DTRACE_COPY_FAULT(addr) {						\
	DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);					\
	cpu_core[CPU->cpu_id].cpuc_dtrace_illval = (addr);			\
}

void dtrace_copyin(user_addr_t uaddr, uintptr_t kaddr, size_t size, volatile uint16_t *flags)
{
	if (uaddr + size < uaddr ||
	    sceKernelCopyFromUserProc(dtrace_get_current_process_id(), (void *)kaddr,
				      (const void *)uaddr, size) < 0)
		DTRACE_COPY_FAULT(uaddr)
}

void dtrace_copyinstr(user_addr_t uaddr, uintptr_t kaddr, size_t size, volatile uint16_t *flags)
{
	SceSSize len;

	if (uaddr + size < uaddr ||
	    (len = sceKernelStrncpyFromUserProc(dtrace_get_current_process_id(), (char *)kaddr,
						(const char *)uaddr, size)) < 0 ||
	    len == size)
		DTRACE_COPY_FAULT(uaddr)
}

void dtrace_copyout(uintptr_t kaddr, user_addr_t uaddr, size_t size, volatile uint16_t *flags)
{
	if (uaddr + size < uaddr ||
	    sceKernelCopyToUserProc(dtrace_get_current_process_id(), (void *)uaddr,
				    (const void *)kaddr, size) < 0)
		DTRACE_COPY_FAULT(uaddr)
}

void dtrace_copyoutstr(uintptr_t kaddr, user_addr_t uaddr, size_t size, volatile uint16_t *flags)
{
	if (uaddr + size < uaddr ||
	    sceKernelStrncpyToUserProc(dtrace_get_current_process_id(), (char *)uaddr,
				       (const char *)kaddr, size) < 0)
		DTRACE_COPY_FAULT(uaddr)
}

uint8_t dtrace_fuword8(user_addr_t uaddr)
{
	uint8_t val = 0;

	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	if (uaddr + sizeof(val) < uaddr ||
	    sceKernelCopyFromUserProc(dtrace_get_current_process_id(), &val,
				      (const void *)uaddr, sizeof(val)) != 0)
		DTRACE_COPY_FAULT(uaddr)
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
	return val;
}

uint16_t dtrace_fuword16(user_addr_t uaddr)
{
	uint16_t val = 0;

	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	if (uaddr + sizeof(val) < uaddr ||
	    sceKernelCopyFromUserProc(dtrace_get_current_process_id(), &val,
				      (const void *)uaddr, sizeof(val)) != 0)
		DTRACE_COPY_FAULT(uaddr)
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
	return val;
}

uint32_t dtrace_fuword32(user_addr_t uaddr)
{
	uint32_t val = 0;

	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	if (uaddr + sizeof(val) < uaddr ||
	    sceKernelCopyFromUserProc(dtrace_get_current_process_id(), &val,
				      (const void *)uaddr, sizeof(val)) != 0)
		DTRACE_COPY_FAULT(uaddr)
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
	return val;
}

uint64_t dtrace_fuword64(user_addr_t uaddr)
{
	uint64_t val = 0;

	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	if (uaddr + sizeof(val) < uaddr ||
	    sceKernelCopyFromUserProc(dtrace_get_current_process_id(), &val,
				      (const void *)uaddr, sizeof(val)) != 0)
		DTRACE_COPY_FAULT(uaddr)
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);
	return val;
}
