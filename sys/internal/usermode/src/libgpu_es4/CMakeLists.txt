# os0:us/libgpu_es4.suprx - SGX services client (bridge into gpu_es4.skprx)
psp2_module(os0_us_libgpu_es4 KIND user COMPILER gcc48 SOURCES
	gpu_user.c
	sgx.c
	pvrsrv.c
	osfunc.c
	services.c
	pds.c
	usse.c
	IMPORTS sceClibMspaceCalloc
)
# Sony built this library at -O3 without strict aliasing, like libgxm_es4: its USSE emitter is
# vectorized and the bridge wrappers keep their stores in source order
target_compile_options(os0_us_libgpu_es4 PRIVATE -O3 -fno-strict-aliasing)
