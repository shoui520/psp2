# libpaf's libtiff: pristine contrib/tiff-3.9.5 plus Sony's changes (tiff-3.9.5-sce.patch: the
# allocator TIFFClientOpen takes, kept in the TIFF and used for every allocation, so the helpers
# that allocate take the TIFF; no error or warning messages and nothing printed; snprintf for
# sprintf; paf's string and single-precision math functions, tif_config.h), patched into the
# build tree, with paf's memory functions (tif_paf.c). Sets <prefix>_SOURCES (the core, in Sony's
# link order), <prefix>_CODECS (the rest, linked after zlib) and <prefix>_INCLUDE.
set(PSP2_LIBTIFF_UPSTREAM "${PSP2_SRC}/contrib/tiff-3.9.5/libtiff")
set(PSP2_LIBTIFF_PATCH "${CMAKE_CURRENT_LIST_DIR}/tiff-3.9.5-sce.patch")
set(PSP2_LIBTIFF_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psp2_libtiff_sce prefix)
	set(dir "${CMAKE_BINARY_DIR}/tiff-3.9.5-sce")
	file(MD5 "${PSP2_LIBTIFF_PATCH}" patch_md5)
	set(stamp "")
	if(EXISTS "${dir}/.patched")
		file(READ "${dir}/.patched" stamp)
	endif()
	if(NOT stamp STREQUAL patch_md5)	# a changed patch is applied afresh
		file(REMOVE_RECURSE "${dir}")
		file(GLOB upstream "${PSP2_LIBTIFF_UPSTREAM}/*.[ch]")
		file(COPY ${upstream} DESTINATION "${dir}/libtiff")
		execute_process(COMMAND patch -s -p1 -d "${dir}" -i "${PSP2_LIBTIFF_PATCH}"
			RESULT_VARIABLE result ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "libtiff SCE patch failed: ${error}")
		endif()
		file(WRITE "${dir}/.patched" "${patch_md5}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PSP2_LIBTIFF_PATCH}")
	set(t "${dir}/libtiff")
	set(${prefix}_INCLUDE "${PSP2_LIBTIFF_DIR}" "${t}" PARENT_SCOPE)
	set(${prefix}_SOURCES "${t}/tif_close.c" "${t}/tif_dir.c" "${t}/tif_dirinfo.c"
		"${t}/tif_dirread.c" "${t}/tif_flush.c" "${t}/tif_getimage.c" "${t}/tif_open.c"
		"${t}/tif_read.c" "${t}/tif_strip.c" "${t}/tif_swab.c" "${t}/tif_tile.c"
		"${PSP2_LIBTIFF_DIR}/tif_paf.c" "${t}/tif_write.c" PARENT_SCOPE)
	set(${prefix}_CODECS "${t}/tif_aux.c" "${t}/tif_color.c" "${t}/tif_compress.c"
		"${t}/tif_dirwrite.c" "${t}/tif_codec.c" "${t}/tif_dumpmode.c" "${t}/tif_fax3.c"
		"${t}/tif_fax3sm.c" "${t}/tif_jpeg.c" "${t}/tif_luv.c" "${t}/tif_lzw.c"
		"${t}/tif_next.c" "${t}/tif_ojpeg.c" "${t}/tif_packbits.c" "${t}/tif_pixarlog.c"
		"${t}/tif_predict.c" "${t}/tif_thunder.c" "${t}/tif_zip.c" PARENT_SCOPE)
endfunction()
