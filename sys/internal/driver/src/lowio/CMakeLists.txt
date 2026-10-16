# SceLowio implementation; strict whole-module verification remains open.
# Sony built it at -O3: DsiShortWrite stores its constant command words as one
# NEON vector, the DSI VIC search and FIFO copies are fully unrolled, and the
# I2C and pixel clock helpers are inlined into every caller.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_lowio KIND kernel SOURCES
	smc.S
	lowio.c
	lowio_state.c
	pervasive_init.c
	cdram.c
	gpio.c
	pwm.c
	i2c.c
	csi.c
	dsi.c
	iftu.c
	pervasive_controls.c
	pervasive_reset.c
	pervasive_gate.c
	pervasive_registers.c
	pervasive_config.c
	pervasive_clock.c
	pervasive_pixel.c
	secure_controls.c
)
