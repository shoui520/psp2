# SceSdstor: C conversion in original source and address order.
psp2_module(os0_kd_sdstor KIND kernel SOURCES
	source/main.c
	source/sdstor.c
	source/sdpart.c
	source/sddev.c
	source/sdstor_vfs.c
	source/sdctrl.c
)
