# os0:kd/deci4p_dfmgr - the DECI4P link manager (SDIO and USB links, DCMP)
psp2_module(os0_kd_deci4p_dfmgr KIND kernel SOURCES
	dfmgr.c
	dcmp.c
	sdio.c
	usb.c
)
# Sony built this module with -O3 (dcmpInit's buffer loop is vectorised)
target_compile_options(os0_kd_deci4p_dfmgr PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
