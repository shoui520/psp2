# SceLibft2 (vs0:sys/external/libSceFt2.suprx): FreeType 2 (git 7f316f22d), built by Sony with GCC.
# The module's registration list is include/freetype/config/ftmodule.h.
set(FT2 ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/freetype-7f316f22d)
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
	${FT2}/src/base/ftbase.c
	${FT2}/src/base/ftbitmap.c
	${FT2}/src/base/ftglyph.c
	${FT2}/src/base/ftinit.c
	${FT2}/src/base/ftdebug.c
	${FT2}/src/autofit/autofit.c
	${FT2}/src/truetype/truetype.c
	${FT2}/src/type1/type1.c
	${FT2}/src/type42/type42.c
	${FT2}/src/psaux/psaux.c
	${FT2}/src/psnames/psnames.c
	${FT2}/src/pshinter/pshinter.c
	${FT2}/src/raster/raster.c
	${FT2}/src/sfnt/sfnt.c
	${FT2}/src/smooth/smooth.c
)
target_include_directories(${PSP2_TARGET} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include ${FT2}/include ${FT2}/src)
target_compile_definitions(${PSP2_TARGET} PRIVATE FT2_BUILD_LIBRARY)
