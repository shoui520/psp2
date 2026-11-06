# libpaf's giflib: pristine contrib/giflib-4.1.6 plus Sony's changes (giflib-4.1.6-sce.patch:
# the heap and four memory functions DGifOpen takes, kept in GifFileType and used for every
# allocation; paf's streams and string functions; no encoder hash table), patched into the
# build tree. Sets <prefix>_SOURCES (in Sony's link order) and <prefix>_INCLUDE.
set(PSP2_GIFLIB_UPSTREAM "${PSP2_SRC}/contrib/giflib-4.1.6/lib")
set(PSP2_GIFLIB_PATCH "${CMAKE_CURRENT_LIST_DIR}/giflib-4.1.6-sce.patch")

function(psp2_giflib_sce prefix)
	set(dir "${CMAKE_BINARY_DIR}/giflib-4.1.6-sce")
	file(MD5 "${PSP2_GIFLIB_PATCH}" patch_md5)
	set(stamp "")
	if(EXISTS "${dir}/.patched")
		file(READ "${dir}/.patched" stamp)
	endif()
	if(NOT stamp STREQUAL patch_md5)	# a changed patch is applied afresh
		file(REMOVE_RECURSE "${dir}")
		file(GLOB upstream "${PSP2_GIFLIB_UPSTREAM}/*.[ch]")
		file(COPY ${upstream} DESTINATION "${dir}/lib")
		execute_process(COMMAND patch -s -p1 -d "${dir}" -i "${PSP2_GIFLIB_PATCH}"
			RESULT_VARIABLE result ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "giflib SCE patch failed: ${error}")
		endif()
		file(WRITE "${dir}/.patched" "${patch_md5}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PSP2_GIFLIB_PATCH}")
	set(${prefix}_INCLUDE "${dir}/lib" PARENT_SCOPE)
	set(${prefix}_SOURCES "${dir}/lib/dgif_lib.c" "${dir}/lib/gif_err.c" "${dir}/lib/gifalloc.c"
		PARENT_SCOPE)
endfunction()
