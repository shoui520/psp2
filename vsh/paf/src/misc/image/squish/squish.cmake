# libpaf's squish: pristine contrib/squish-1.11 plus Sony's changes (squish-1.11-sce.patch: a
# Vec4 on ARM NEON, simd_neon.h, chosen by SQUISH_USE_NEON), patched into the build tree, on
# paf's float maths (this folder's cmath, cfloat and algorithm). Sony links squish.cpp after
# libtiff's core and the rest (the alpha, colour set and fit classes, maths) after
# tif_dirwrite.c. Sets <prefix>_API (squish.cpp), <prefix>_SOURCES (the rest, in Sony's link
# order) and <prefix>_INCLUDE.
set(PSP2_PAF_SQUISH_UPSTREAM "${PSP2_SRC}/contrib/squish-1.11")
set(PSP2_PAF_SQUISH_PATCH "${CMAKE_CURRENT_LIST_DIR}/squish-1.11-sce.patch")
set(PSP2_PAF_SQUISH_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psp2_paf_squish prefix)
	set(dir "${CMAKE_BINARY_DIR}/squish-1.11-sce")
	file(MD5 "${PSP2_PAF_SQUISH_PATCH}" patch_md5)
	set(stamp "")
	if(EXISTS "${dir}/.patched")
		file(READ "${dir}/.patched" stamp)
	endif()
	if(NOT stamp STREQUAL patch_md5)	# a changed patch is applied afresh
		file(REMOVE_RECURSE "${dir}")
		file(GLOB upstream "${PSP2_PAF_SQUISH_UPSTREAM}/*.cpp" "${PSP2_PAF_SQUISH_UPSTREAM}/*.h"
			"${PSP2_PAF_SQUISH_UPSTREAM}/*.inl")
		file(COPY ${upstream} DESTINATION "${dir}")
		execute_process(COMMAND patch -s -p1 -d "${dir}" -i "${PSP2_PAF_SQUISH_PATCH}"
			RESULT_VARIABLE result ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "squish SCE patch failed: ${error}")
		endif()
		file(WRITE "${dir}/.patched" "${patch_md5}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PSP2_PAF_SQUISH_PATCH}")
	set(s "${dir}")
	set(${prefix}_API "${s}/squish.cpp" PARENT_SCOPE)
	set(${prefix}_SOURCES "${s}/alpha.cpp" "${s}/clusterfit.cpp" "${s}/colourblock.cpp"
		"${s}/colourfit.cpp" "${s}/colourset.cpp" "${s}/maths.cpp" "${s}/rangefit.cpp"
		"${s}/singlecolourfit.cpp" PARENT_SCOPE)
	set(${prefix}_INCLUDE "${PSP2_PAF_SQUISH_DIR}" "${s}" PARENT_SCOPE)
endfunction()
