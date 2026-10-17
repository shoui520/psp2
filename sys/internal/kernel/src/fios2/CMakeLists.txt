# os0:kd/bootimage/fios2 - SceFios2Kernel: FIOS2 overlays (C++ built by Sony with GCC)
enable_language(CXX)
psp2_module(os0_kd_bootimage_fios2 KIND kernel COMPILER gcc48 SOURCES
	rwlock.cpp
	path.cpp
	init_term.cpp
	overlay.cpp
	resolve.cpp
	dh.cpp
	kernel.cpp
	storage.cpp
	platform.cpp
	api.cpp
	syscall.cpp
)

if(TARGET os0_kd_bootimage_fios2)
	foreach(f -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit)
		target_compile_options(os0_kd_bootimage_fios2 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${f}>)
	endforeach()
endif()
