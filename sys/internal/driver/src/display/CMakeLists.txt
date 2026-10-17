# os0:kd/display - SceDisplay. Sony built it at -O3: its loops are vectorized
# (the CSC matrix gain) and prefetched (the per-head loops of the event handler).
# The parts are separate units: their functions are never inlined into each
# other and their BSS follows the link order.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_display KIND kernel COMPILER gcc48 SOURCES
	display.c
	vblank.c
	bootlogo.c
	oled.c
	lcd.c
	hdmi.c
	display_dtrace.c
	display_syscall.c
)
