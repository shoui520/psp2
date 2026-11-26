# 2014 PAF libpng: unmodified contrib/libpng-1.2.46, built with this folder's
# pngusr.h (Sony's 2014 configuration for pup:psp2swu.self). Sets <prefix>_SOURCES
# and <prefix>_INCLUDE.
set(PSP2_LIBPNG12_UPSTREAM "${PSP2_SRC}/contrib/libpng-1.2.46")
set(PSP2_LIBPNG12_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psp2_libpng12_sce prefix)
	set(sources "")
	foreach(name png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset
			pngtrans pngwio pngwrite pngwtran pngwutil)
		list(APPEND sources "${PSP2_LIBPNG12_UPSTREAM}/${name}.c")
	endforeach()
	set(${prefix}_SOURCES ${sources} PARENT_SCOPE)
	set(${prefix}_INCLUDE "${PSP2_LIBPNG12_DIR}" "${PSP2_LIBPNG12_UPSTREAM}" PARENT_SCOPE)
endfunction()
