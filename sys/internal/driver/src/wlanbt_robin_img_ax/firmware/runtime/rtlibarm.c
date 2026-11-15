/*
 * The ARM C library's ARM-state pieces (__rt_clear_errno 0x53cd0, the count of leading zeros
 * 0x53c64) and the library space their getters in boot/reset.S hand out. The filename is
 * inferred.
 */
#include <stdint.h>
#include "robin/runtime.h"

/*
 * The library space (0x5ae54, 96 bytes): errno at +4, the heap descriptor at +0x10 and the
 * locale descriptor at +0x20; __rt_stackheap_setup runs __user_initial_stackheap on a stack
 * at its end.
 */
uint32_t __libspace_start[RT_LIBSPACE_WORDS];

void __rt_clear_errno(void)
{
	*__rt_get_errno_addr() = 0;
}

int16_t robinClz16(uint32_t value)
{
	return (int16_t)(value != 0 ? __builtin_clz(value) : 32);
}
