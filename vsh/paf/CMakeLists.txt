# ScePaf was built with SNC. Unconverted exports remain in libpaf.c.
enable_language(CXX)
psp2_module(vs0_sys_external_libpaf KIND user COMPILER snc PARTIAL SOURCES
	src/module.c
	src/avout/sound.cpp
	src/avout/wave.c
	src/common/call_list.c
	src/common/main_thread_call_list.c
	src/common/main_thread_call.c
	src/common/char_encoding.c
	src/common/resource_hooks.c
	src/common/frame_count.c
	src/common/halt_assert.c
	src/common/sequence.c
	src/common/job_call_queue.c
	src/common/job_slots.c
	src/common/path.c
	src/common/view.c
	src/plugin.c
	src/std/new.c
	src/std/string.c
	libpaf.c
)
