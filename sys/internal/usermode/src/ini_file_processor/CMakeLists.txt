enable_language(CXX)
# SceIniFileProcessor (vs0:sys/external/ini_file_processor.suprx), built by Sony with SNC 3.57 in C++. No debug
# record names its files; they are split where its parts are (ours).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	memory.cpp
	scanner.cpp
	string.cpp
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
