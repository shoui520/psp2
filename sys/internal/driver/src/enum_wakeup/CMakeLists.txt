# SceEnumWakeUp: the USB-charging wake-up screen and its MTP responder.
psp2_module(os0_kd_enum_wakeup KIND kernel COMPILER gcc48 SOURCES
	enum_wakeup.c
	draw.c
	mtp_utf.c
	mtp_operation.c
	mtp.c
	mtp_session.c
	mtp_propdesc.c
	mtp_device.c
	mtp_heap.c
	mtp_util.c
	mtp_cache.c
	mtp_xfer.c
	console.c
	font8x16.c
	icon.S
)
