# SceSdstor: C conversion in original source and address order.
psp2_module(os0_kd_sdstor KIND kernel SOURCES
	main.c
	sdstor.c
	sdpart.c
	sddev.c
	sdstor_vfs.c
	sdctrl.c
)
