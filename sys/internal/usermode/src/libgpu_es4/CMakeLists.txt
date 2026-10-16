# os0:us/libgpu_es4.suprx - SGX services client (bridge into gpu_es4.skprx)
psp2_module(os0_us_libgpu_es4 KIND user SOURCES
	gpu_user.c
	sgx.c
	pvrsrv.c
	osfunc.c
	services.c
	pds.c
	usse.c
	IMPORTS sceClibMspaceCalloc
)
