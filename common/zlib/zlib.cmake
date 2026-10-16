# Sony's zlib 1.2.5: pristine contrib/zlib-1.2.5 plus zlib-1.2.5-sce.patch
# (state fields wrap/nocheck and wrap/align, the nocheck and padded Init entry
# points, builtin zero-length-safe copies, the kernel heap), configured into the
# build tree. Sets <prefix>_SOURCES and <prefix>_INCLUDE.
set(PSP2_ZLIB_UPSTREAM "${PSP2_SRC}/contrib/zlib-1.2.5")
set(PSP2_ZLIB_PATCH "${PSP2_SRC}/common/zlib/zlib-1.2.5-sce.patch")

function(psp2_zlib_sce prefix)
	set(dir "${CMAKE_BINARY_DIR}/zlib-1.2.5-sce")
	if(NOT EXISTS "${dir}/.patched")
		file(GLOB upstream "${PSP2_ZLIB_UPSTREAM}/*.[ch]")
		file(COPY ${upstream} DESTINATION "${dir}")
		execute_process(COMMAND patch -s -p1 -d "${dir}" -i "${PSP2_ZLIB_PATCH}"
			RESULT_VARIABLE result ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "zlib SCE patch failed: ${error}")
		endif()
		file(TOUCH "${dir}/.patched")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PSP2_ZLIB_PATCH}")
	set(${prefix}_INCLUDE "${dir}" PARENT_SCOPE)
	set(${prefix}_SOURCES
		"${dir}/compress.c" "${dir}/uncompr.c" "${dir}/deflate.c" "${dir}/trees.c"
		"${dir}/inflate.c" "${dir}/infback.c" "${dir}/inffast.c" "${dir}/inftrees.c"
		"${dir}/zutil.c" "${PSP2_SRC}/common/zlib/checksum.c"
		"${PSP2_SRC}/common/zlib/checksum_neon.S"
		PARENT_SCOPE)
endfunction()
