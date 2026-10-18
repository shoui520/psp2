# os0:kd/bootimage/motion_dummy - SceMotionDevDummy. Sony built it at -O3: the
# process and ring-reader searches are unrolled into compare chains. No section
# anchors: each object is addressed by its own movw/movt.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_motion_dummy KIND kernel COMPILER gcc48 SOURCES motion_dummy.c
		../motion/ring.c syscall.c
		../motion/heap.c ../motion/filter.c
	IMPORTS sceKernelCreateHeap sceKernelDeleteHeap __aeabi_uidivmod
		sceKernelGetSystemTimeWide SceProcessmgrForDriver_8CB01675
)
