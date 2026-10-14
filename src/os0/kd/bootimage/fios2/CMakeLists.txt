# os0:kd/bootimage/fios2 - SceFios2Kernel: FIOS2 overlays (C++ built by Sony with GCC)
enable_language(CXX)
psp2_module(os0_kd_bootimage_fios2 KIND kernel SOURCES
	source/rwlock.cpp
	source/path.cpp
	source/init_term.cpp
	source/overlay.cpp
	source/resolve.cpp
	source/dh.cpp
	source/kernel.cpp
	source/storage.cpp
	source/platform.cpp
	source/api.cpp
	source/syscall.cpp
)

if(TARGET os0_kd_bootimage_fios2)
	foreach(f -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit)
		target_compile_options(os0_kd_bootimage_fios2 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${f}>)
	endforeach()
endif()
