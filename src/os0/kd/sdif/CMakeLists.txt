# SceSdif C implementation, in Sony source order.
psp2_module(os0_kd_sdif KIND kernel SOURCES
	source/sdif.c
	source/cmd_queue.c
	source/sdio_intr.c
	source/card_cmd.c
	source/mmc.c
	source/sd.c
	source/sdio.c
	source/adma.c
)
