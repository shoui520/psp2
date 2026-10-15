/*
 * SceSysmem: the system time's low word for procevent.c's handler timing. It
 * is a source file of its own: the original calls the export rather than
 * inlining it.
 */

#include <stdint.h>

#if FW_DEBUG
uint32_t SceSysrootForKernel_41636522(void);

/* name unknown: the low word of the system time */
uint32_t SysrootGetTimeLow(void)
{
	return SceSysrootForKernel_41636522();
}
#endif
