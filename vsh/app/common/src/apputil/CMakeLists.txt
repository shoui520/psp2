enable_language(CXX)
# SceAppUtil (vs0:sys/external/apputil.suprx), built by Sony with SNC 3.57 in C++
# (vsh/app/common/src/apputil/webbrowser/webbrowser.cpp in the 1.50-1.80 binaries). No debug
# record names its files.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	savedata.cpp
	savedata_check.cpp
	apputil.cpp
	system_param.cpp
	app_event.cpp
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/lowlayer/src")
# Sony's inlining: the checks of 4 to 6 instructions go into their callers, checkMountPoint
# (75 instructions) does not
set_source_files_properties(savedata_check.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=24)
# the parameter checks (9 to 15 instructions) stay calls
set_source_files_properties(system_param.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
# parseNpCommunicationId (19 instructions) stays a call
set_source_files_properties(app_event.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
