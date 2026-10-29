# os0:kd/bootimage/touch - front and back touch panels (syscon). Built at -O3 like touch_dummy,
# whose ring, user API and tracking units Sony linked from the same sources.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_touch KIND kernel COMPILER gcc48 SOURCES
	touch.c ring.c parse.c touch_user.c tracking.c filter.c
)
