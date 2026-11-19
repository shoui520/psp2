# SceLibft2 (vs0:sys/external/libSceFt2.suprx): FreeType 2 (git 7f316f22d), built by Sony with GCC.
# The module's registration list is include/freetype/config/ftmodule.h.
set(FT2 ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/freetype-7f316f22d)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
	setjmp.S
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	libc.c
	${FT2}/src/base/ftbase.c
	${FT2}/src/base/ftbitmap.c
	${FT2}/src/base/ftglyph.c
	${FT2}/src/base/ftinit.c
	${FT2}/src/base/ftdebug.c
	${FT2}/src/autofit/autofit.c
	${FT2}/src/truetype/truetype.c
	${FT2}/src/type1/type1.c
	${FT2}/src/cff/cff.c
	${FT2}/src/otvalid/otvalid.c
	${FT2}/src/base/ftotval.c
	${FT2}/src/base/ftsystem.c
	${FT2}/src/psaux/psaux.c
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
	${CMAKE_CURRENT_SOURCE_DIR}/include ${FT2}/include ${FT2}/src)
target_compile_definitions(${PSP2_TARGET} PRIVATE FT2_BUILD_LIBRARY)
# -O3 matches more functions than -O2 under similarity pairing (293 vs 230 exact+proven, 3.65-CEX,
# 2026-11-19); to be re-measured per unit once the pairs are written
target_compile_options(${PSP2_TARGET} PRIVATE -O3 -fno-stack-protector)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
