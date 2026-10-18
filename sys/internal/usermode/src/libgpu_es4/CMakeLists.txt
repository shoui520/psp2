# os0:us/libgpu_es4.suprx - SGX services client (bridge into gpu_es4.skprx)
# PARTIAL in Sony's layout: the C still calls sceClibMemcpy (pds.c), which the original does not
# import, and __aeabi_uidiv (dms.c), which the original has from its statically linked libgcc
# vitasdk's user headers and newlib until this module is on the tree's SDK headers
set(MODULE_VITASDK_LIBC ON)
psp2_module(os0_us_libgpu_es4 KIND user COMPILER gcc48 PARTIAL SOURCES
	gpu_user.c
	sgx.c
	pvrsrv.c
	osfunc.c
	services.c
	pds.c
	usse.c
	IMPORTS sceClibMspaceCalloc
)
