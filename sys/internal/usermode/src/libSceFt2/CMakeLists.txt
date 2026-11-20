# SceLibft2 (vs0:sys/external/libSceFt2.suprx): FreeType 2.4.9 (FT_New_Library sets 2.4.9), built by Sony with GCC.
# The module's registration list is include/freetype/config/ftmodule.h.
set(FT2 ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/freetype-2.4.9)
# Sony's changes to FreeType (freetype-sce.patch: the memory object takes the caller's allocator, no
# FT_Init_FreeType) are applied to a copy of the files they touch (src/psaux is copied whole: psaux.c includes its parts) in the build tree.
set(FT2_SCE ${CMAKE_CURRENT_BINARY_DIR}/freetype-sce)
file(MD5 ${CMAKE_CURRENT_SOURCE_DIR}/freetype-sce.patch ft2_patch_md5)
string(MD5 ft2_patch_md5 "${ft2_patch_md5} ${FT2}")
set(ft2_stamp "")
if(EXISTS ${FT2_SCE}/.patched)
	file(READ ${FT2_SCE}/.patched ft2_stamp)
endif()
if(NOT ft2_stamp STREQUAL ft2_patch_md5)
	file(REMOVE_RECURSE ${FT2_SCE})
	foreach(f src/base/ftsystem.c src/base/ftinit.c include/freetype/internal/ftobjs.h src/psaux)
		get_filename_component(d ${f} DIRECTORY)
		file(COPY ${FT2}/${f} DESTINATION ${FT2_SCE}/${d})
	endforeach()
	execute_process(COMMAND patch -s -p1 -d ${FT2_SCE} -i ${CMAKE_CURRENT_SOURCE_DIR}/freetype-sce.patch
		RESULT_VARIABLE ft2_patch_result ERROR_VARIABLE ft2_patch_error)
	if(NOT ft2_patch_result EQUAL 0)
		message(FATAL_ERROR "FreeType SCE patch failed: ${ft2_patch_error}")
	endif()
	file(WRITE ${FT2_SCE}/.patched ${ft2_patch_md5})
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/freetype-sce.patch)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
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
	${FT2}/src/truetype/truetype.c
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
# 2026-11-19); to be re-measured per unit once the pairs are written
target_compile_options(${PSP2_TARGET} PRIVATE -O3 -fno-stack-protector)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
# Sony's C library (libc.c) was built with plain char signed (the ldrsb loads of the mode and path strings)
set_source_files_properties(libc.c PROPERTIES COMPILE_OPTIONS -fsigned-char)
