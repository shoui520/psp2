/* The physical motion device creates its heap during module startup. */
#include <sysmem/sysmem.h>

extern SceUID motionHeap;

int motionCreateHeap(void)
{
	SceKernelHeapCreateOpt opt;

	__builtin_memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);

	motionHeap = SceSysmemForDriver_9328E0E8("SceMotionDev", 2048, &opt);
	return motionHeap;
}
