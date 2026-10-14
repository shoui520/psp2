# SceLowio implementation; strict whole-module verification remains open.
psp2_module(os0_kd_lowio KIND kernel SKELETON SOURCES
	source/lowio.c
	source/lowio_state.c
	source/pervasive_init.c
	source/cdram.c
	source/idle.S
	source/gpio.c
	source/pwm.c
	source/pwm_status.S
	source/i2c.c
	source/i2c_wait.S
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
	source/smc.S
)
