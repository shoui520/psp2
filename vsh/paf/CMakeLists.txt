# ScePaf was built with SNC. Unconverted exports remain in libpaf.c.
enable_language(CXX)
psp2_module(vs0_sys_external_libpaf KIND user COMPILER snc PARTIAL SOURCES
	src/module.c
	src/avout/sound.cpp
	src/avout/wave.c
	src/common/call_list.c
	src/common/main_thread_call_list.c
	src/common/main_thread_call.c
	libpaf.c
)
