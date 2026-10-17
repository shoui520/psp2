# SceSdif C implementation, in Sony source order.
psp2_module(os0_kd_sdif KIND kernel COMPILER gcc48 SOURCES
	sdif.c
	cmd_queue.c
	sdio_intr.c
	card_cmd.c
	mmc.c
	sd.c
	sdio.c
	adma.c
)
