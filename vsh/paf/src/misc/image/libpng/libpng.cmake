# libpaf's libpng: unmodified contrib/libpng-1.5.10, built with this folder's pnglibconf.h and
# pngusr.h (Sony's configuration). Sets <prefix>_SOURCES (in Sony's link order), <prefix>_NEON
# (the NEON filters) and <prefix>_INCLUDE.
set(PSP2_LIBPNG_UPSTREAM "${PSP2_SRC}/contrib/libpng-1.5.10")
set(PSP2_LIBPNG_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psp2_libpng_sce prefix)
	set(sources "")
	foreach(name png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset
			pngtrans pngwio pngwrite pngwtran pngwutil)
		list(APPEND sources "${PSP2_LIBPNG_UPSTREAM}/${name}.c")
	endforeach()
	set(${prefix}_SOURCES ${sources} PARENT_SCOPE)
	set(${prefix}_NEON "${PSP2_LIBPNG_UPSTREAM}/arm/filter_neon.S" PARENT_SCOPE)
	set(${prefix}_INCLUDE "${PSP2_LIBPNG_DIR}" "${PSP2_LIBPNG_UPSTREAM}" PARENT_SCOPE)
endfunction()
