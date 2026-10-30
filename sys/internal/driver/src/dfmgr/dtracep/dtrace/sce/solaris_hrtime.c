/*
 * Solaris time (Sony's dtrace/sce/solaris_hrtime.c): nanoseconds from the
 * system timer.
 */
#include <dtrace/sce/solaris_hrtime.h>
#include <threadmgr/threadmgr.h>

hrtime_t gethrtime(void)
{
	return sceKernelGetSystemTimeWide() * 1000;
}
