enable_language(CXX)
# SceVideoExport (vs0:sys/external/libSceVideoExport.suprx), built by Sony with SNC 3.57 in C++. No debug
# record names its files; the files are named by what they hold (ours). The state of the export
# comes first, then the library's file and the media service's methods; libces's UTF-8 routines
# link after them (no runtime routine of SNC's libsnc is linked).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	video_export_data.cpp
	video_export.cpp
	video_export_ipc_client.cpp
	../ces/libces/src/UCS/ces_ucs_context.c
	../ces/libces/src/UCS/Str_UTF8_UTF16.c
	../ces/libces/src/UCS/UTF16_UTF8.c
	../ces/libces/src/UCS/UTF8.c
	../ces/libces/src/ces_context.c
	../ces/libces/src/UCS/ces_ucs_callback.c
	../ces/libces/src/UCS/UTF16.c
)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# Sony's psp2ld --strip-unused dropped what nothing references
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
