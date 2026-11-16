enable_language(CXX)
# SceIniFileProcessor (vs0:sys/external/ini_file_processor.suprx), built by Sony with SNC 3.57 in C++. No debug
# record names its files; they are split where its parts are (ours).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	memory.cpp
	scanner.cpp
	string.cpp
	file.cpp
	temp_file.cpp
	buffered_file.cpp
	processor.cpp
	exports.cpp
	../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../../sys/sdk/target/src/libsnc/i64shr.c
	../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# the C++ runtime of the module is ScePafStdc's: its imports serve under the names the compiler asks for
target_link_options(${PSP2_TARGET} PRIVATE
	-Wl,--defsym,__cxa_guard_acquire=ScePafStdc_CB26B866
	-Wl,--defsym,__cxa_guard_release=ScePafStdc_6AB297BE
	-Wl,--defsym,__aeabi_atexit=ScePafStdc_C95732BB
	-Wl,--defsym,__cxa_pure_virtual=ScePafStdc_2730262C
	-Wl,--defsym,_ZdlPv=ScePafStdc_1B77082E)
# Sony's psp2ld --strip-unused dropped what nothing references
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
