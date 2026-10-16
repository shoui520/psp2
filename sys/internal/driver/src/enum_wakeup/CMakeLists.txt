# SceEnumWakeUp: the USB-charging wake-up screen and its MTP responder.
psp2_module(os0_kd_enum_wakeup KIND kernel SOURCES
	source/enum_wakeup.c
	source/draw.c
	source/mtp_utf.c
	source/mtp_operation.c
	source/mtp.c
	source/mtp_session.c
	source/mtp_propdesc.c
	source/mtp_device.c
	source/mtp_heap.c
	source/mtp_util.c
	source/mtp_cache.c
	source/mtp_xfer.c
	source/console.c
	source/font8x16.c
	source/icon.S
)
