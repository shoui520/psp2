# SceLowio implementation; strict whole-module verification remains open.
# Sony built it at -O3: DsiShortWrite stores its constant command words as one
# NEON vector, the DSI VIC search and FIFO copies are fully unrolled, and the
# I2C and pixel clock helpers are inlined into every caller.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_lowio KIND kernel COMPILER gcc48 SOURCES
	smc.S
	lowio.c
	lowio_state.c
	pervasive.c
	pervasive_clock.c
	cdram.c
	gpio.c
	pwm.c
	i2c.c
	csi.c
	dsi.c
	iftu.c
	secure_controls.c
)
