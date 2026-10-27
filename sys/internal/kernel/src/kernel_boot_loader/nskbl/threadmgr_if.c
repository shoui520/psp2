/*
 * NSKBL: the thread manager's clock, which the module manager reads, on the sysroot's
 * timer: NSKBL has no thread manager. (File name ours, after iofilemgr_if.c: no record
 * or string names it.)
 */

#include <stdint.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>

SceInt64 sceKernelGetSystemTimeWide(void)
{
	return SceSysrootForKernel_E20F6FC8();
}

SceUInt32 SceThreadmgrForDriver_47F6DE49(void)
{
	return SceSysrootForKernel_41636522();
}
