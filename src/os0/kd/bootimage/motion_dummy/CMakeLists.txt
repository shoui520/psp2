# os0:kd/bootimage/motion_dummy - SceMotionDevDummy. Sony built it at -O3: the
# process and ring-reader searches are unrolled into compare chains. No section
# anchors: each object is addressed by its own movw/movt.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_motion_dummy KIND kernel SOURCES source/motion_dummy.c
		../../../../common/motion/ring.c source/syscall.c
		../../../../common/motion/heap.c ../../../../common/motion/filter.c
	IMPORTS ksceKernelCreateHeap ksceKernelDeleteHeap __aeabi_uidivmod
		sceKernelGetSystemTimeWide SceProcessmgrForDriver_8CB01675
)
