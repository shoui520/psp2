# ScePower: power requests, clocks, idle timers, suspend/resume and device state.
# The units follow Sony's: their functions keep the original's inlining.
psp2_module(os0_kd_bootimage_power KIND kernel SOURCES
	source/power_cpu.S
	source/power.c
	source/power_switch.c
	source/power_clock.c
	source/power_process.c
	source/power_idle.c
	source/power_display.c
	source/power_battery.c
	source/power_led.c
)

# Sony's power.c was built with IPA-CP cloning: the original carries
# _scePowerNotifyCallbackProc twice, the second a constprop copy for
# SCE_KERNEL_ALL_PROCESS_ID that the file's own callers use.
set_source_files_properties(source/power.c PROPERTIES
	COMPILE_OPTIONS "-fipa-cp-clone")
