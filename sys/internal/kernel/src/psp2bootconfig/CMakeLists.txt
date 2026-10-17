# os0:psp2bootconfig.skprx - first boot module: loads and starts the core kernel
psp2_module(os0_psp2bootconfig KIND kernel COMPILER gcc48 SOURCES
	bootconfig.c
)
