# SceLowio implementation; strict whole-module verification remains open.
# Sony built it at -O3: DsiShortWrite stores its constant command words as one
# NEON vector, the DSI VIC search and FIFO copies are fully unrolled, and the
# I2C and pixel clock helpers are inlined into every caller.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_lowio KIND kernel SKELETON SOURCES
	source/smc.S
	source/lowio.c
	source/lowio_state.c
	source/pervasive_init.c
	source/cdram.c
	source/gpio.c
	source/pwm.c
	source/i2c.c
	source/csi.c
	source/dsi.c
	source/iftu.c
	source/pervasive_controls.c
	source/pervasive_reset.c
	source/pervasive_gate.c
	source/pervasive_registers.c
	source/pervasive_config.c
	source/pervasive_clock.c
	source/pervasive_pixel.c
	source/secure_controls.c
)
