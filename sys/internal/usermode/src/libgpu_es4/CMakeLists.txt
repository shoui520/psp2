# os0:us/libgpu_es4.suprx - SGX services client (bridge into gpu_es4.skprx)
psp2_module(os0_us_libgpu_es4 KIND user SOURCES
	source/gpu_user.c
	source/sgx.c
	source/pvrsrv.c
	source/osfunc.c
	source/services.c
	source/pds.c
	source/usse.c
	IMPORTS sceClibMspaceCalloc
)
