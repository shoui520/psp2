enable_language(CXX)
# SceAppUtil (vs0:sys/external/apputil.suprx), built by Sony with SNC 3.57 in C++
# (vsh/app/common/src/apputil/webbrowser/webbrowser.cpp in the 1.50-1.80 binaries). No debug
# record names its files. The SDK's libSceCes links after them: its UTF-8 to UCS-2 string
# routines, what SceAppUtil_7DA34311 calls.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	savedata.cpp
	savedata_check.cpp
	apputil.cpp
	system_param.cpp
	app_event.cpp
	store.cpp
	addcont.cpp
	umass.cpp
	device.cpp
	bgdl.cpp
	npsignin.cpp
	book.cpp
	music.cpp
	webbrowser_callback/webbrowser_callback.cpp
	webbrowser_cookie/webbrowser_cookie.cpp
	webbrowser/webbrowser.cpp
	../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_context.c
	../../../../../sys/internal/usermode/src/ces/libces/src/UCS/Str_UTF8_UCS2.c
	../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF32_UTF8.c
	../../../../../sys/internal/usermode/src/ces/libces/src/UCS/UTF8.c
	../../../../../sys/internal/usermode/src/ces/libces/src/ces_context.c
	../../../../../sys/internal/usermode/src/ces/libces/src/UCS/ces_ucs_callback.c
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
# the checks and helpers several calls share stay calls; the title ID, passcode, file and
# callback helpers are inline
set_source_files_properties(addcont.cpp bgdl.cpp webbrowser_callback/webbrowser_callback.cpp
	webbrowser_cookie/webbrowser_cookie.cpp webbrowser/webbrowser.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=0)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# Sony's psp2ld --strip-unused dropped what nothing references
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
