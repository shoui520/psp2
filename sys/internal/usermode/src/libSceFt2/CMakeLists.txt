# SceLibft2 (vs0:sys/external/libSceFt2.suprx): FreeType 2.4.9 (FT_New_Library sets 2.4.9), built by Sony with GCC.
# The module's registration list is include/freetype/config/ftmodule.h.
set(FT2 ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/freetype-2.4.9)
# Sony's changes to FreeType (freetype-sce.patch: the memory object takes the caller's allocator, no
# FT_Init_FreeType; the TrueType driver keeps the global advance width of fixed-pitch fonts unless
# FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH is set, the reverse of upstream's 8c82ec5b of 2.4.5) are applied to a
# copy of the files they touch (src/psaux and src/truetype are copied whole: psaux.c and truetype.c include
# their parts) in the build tree.
# freetype-inline.patch is not Sony's: it marks the TrueType instructions Sony's GCC kept out of TT_RunIns
# (noinline) and those it put in (always_inline), where ours decides otherwise.
set(FT2_SCE ${CMAKE_CURRENT_BINARY_DIR}/freetype-sce)
file(MD5 ${CMAKE_CURRENT_SOURCE_DIR}/freetype-sce.patch ft2_patch_md5)
file(MD5 ${CMAKE_CURRENT_SOURCE_DIR}/freetype-inline.patch ft2_inline_md5)
string(MD5 ft2_patch_md5 "${ft2_patch_md5} ${ft2_inline_md5} ${FT2}")
set(ft2_stamp "")
if(EXISTS ${FT2_SCE}/.patched)
	file(READ ${FT2_SCE}/.patched ft2_stamp)
endif()
if(NOT ft2_stamp STREQUAL ft2_patch_md5)
	file(REMOVE_RECURSE ${FT2_SCE})
	foreach(f src/base/ftsystem.c src/base/ftinit.c include/freetype/internal/ftobjs.h src/psaux src/truetype)
		get_filename_component(d ${f} DIRECTORY)
		file(COPY ${FT2}/${f} DESTINATION ${FT2_SCE}/${d})
	endforeach()
	foreach(patch freetype-sce.patch freetype-inline.patch)
		execute_process(COMMAND patch -s -p1 -d ${FT2_SCE} -i ${CMAKE_CURRENT_SOURCE_DIR}/${patch}
			RESULT_VARIABLE ft2_patch_result ERROR_VARIABLE ft2_patch_error)
		if(NOT ft2_patch_result EQUAL 0)
			message(FATAL_ERROR "FreeType patch ${patch} failed: ${ft2_patch_error}")
		endif()
	endforeach()
	file(WRITE ${FT2_SCE}/.patched ${ft2_patch_md5})
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/freetype-sce.patch
	${CMAKE_CURRENT_SOURCE_DIR}/freetype-inline.patch)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	setjmp.S
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	libc.c
	${FT2}/src/base/ftbase.c
	${FT2}/src/base/ftbitmap.c
	${FT2}/src/base/ftglyph.c
	${FT2_SCE}/src/base/ftinit.c
	${FT2}/src/base/ftdebug.c
	${FT2}/src/autofit/autofit.c
	${FT2_SCE}/src/truetype/truetype.c
	${FT2}/src/type1/type1.c
	${FT2}/src/cff/cff.c
	${FT2}/src/otvalid/otvalid.c
	${FT2}/src/base/ftotval.c
	${FT2_SCE}/src/base/ftsystem.c
	${FT2_SCE}/src/psaux/psaux.c
	${FT2}/src/psnames/psnames.c
	${FT2}/src/pshinter/pshinter.c
	${FT2}/src/raster/raster.c
	${FT2}/src/sfnt/sfnt.c
	${FT2}/src/smooth/smooth.c
)
target_include_directories(${PSP2_TARGET} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../toolchains/libgcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../toolchains/libgcc/target
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/gcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/libgcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/include
	${CMAKE_CURRENT_SOURCE_DIR}/include ${FT2_SCE}/include ${FT2}/include ${FT2}/src ${FT2}/src/base)
target_compile_definitions(${PSP2_TARGET} PRIVATE FT2_BUILD_LIBRARY)
# -O3 matches more functions than -O2 under similarity pairing (293 vs 230 exact+proven, 3.65-CEX,
# 2026-11-19). Sony built FreeType hosted (its fixed-size memset and memcpy calls are expanded inline) and
# without -fno-delete-null-pointer-checks (a pointer once dereferenced is not tested for NULL again:
# tt_face_goto_table's table), with -fstack-protector (only the two functions with a character
# array call __stack_chk_fail: FT_Raccess_Get_HeaderInfo and tt_face_init).
target_compile_options(${PSP2_TARGET} PRIVATE -O3 -fstack-protector -fhosted -fbuiltin
	-fdelete-null-pointer-checks -ffunction-sections)
# Sony's link dropped the functions nothing calls (FreeType's debug and little-endian stream readers).
# Export 04DCBE87 is FT_New_Memory as freetype-sce.patch changes it; Sony's name for it is not known.
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections -Wl,--defsym=SceFt2_04DCBE87=FT_New_Memory)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
# Sony's C library (libc.c) was built freestanding, with plain char signed (the ldrsb loads of the mode
# and path strings)
set_source_files_properties(libc.c PROPERTIES COMPILE_OPTIONS "-ffreestanding;-fno-builtin;-fsigned-char")
