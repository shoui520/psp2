# ScePower: power requests, clocks, idle timers, suspend/resume and device state.
# The units follow Sony's: their functions keep the original's inlining.
psp2_module(os0_kd_bootimage_power KIND kernel SOURCES
	power_cpu.S
	power.c
	power_switch.c
	power_clock.c
	power_process.c
	power_idle.c
	power_display.c
	power_battery.c
	power_led.c
)

# Sony's power.c was built with IPA-CP cloning: the original carries
# _scePowerNotifyCallbackProc twice, the second a constprop copy for
# SCE_KERNEL_ALL_PROCESS_ID that the file's own callers use.
set_source_files_properties(power.c PROPERTIES
	COMPILE_OPTIONS "-fipa-cp-clone")
