# libpaf's zlib: unmodified contrib/zlib-1.2.5 (the parts libpaf links), its check values from the
# user-mode zlib revision libhttp has (zlib/checksum.c: adler32 and crc32's table;
# zlib/checksum_neon.S: the NEON adler32 and crc32), and paf's default allocator in place of
# zutil.c's (zalloc.c, with MY_ZCALLOC). Sets <prefix>_SOURCES (deflate.c to zutil.c in Sony's link
# order), <prefix>_CHECKSUM, <prefix>_INFFAST and <prefix>_INCLUDE.
set(PSP2_PAF_ZLIB_UPSTREAM "${PSP2_SRC}/contrib/zlib-1.2.5")
set(PSP2_PAF_ZLIB_CHECKSUM "${PSP2_SRC}/sys/internal/usermode/src/libhttp/zlib")
set(PSP2_PAF_ZLIB_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psp2_paf_zlib prefix)
	set(z "${PSP2_PAF_ZLIB_UPSTREAM}")
	set(${prefix}_SOURCES "${z}/deflate.c" "${z}/inflate.c" "${z}/inftrees.c" "${z}/trees.c"
		"${z}/zutil.c" "${PSP2_PAF_ZLIB_DIR}/zalloc.c" PARENT_SCOPE)
	set(${prefix}_INFFAST "${z}/inffast.c" PARENT_SCOPE)
	set(${prefix}_CHECKSUM "${PSP2_PAF_ZLIB_CHECKSUM}/checksum.c"
		"${PSP2_PAF_ZLIB_CHECKSUM}/checksum_neon.S" PARENT_SCOPE)
	# zconf.h's <sys/types.h>
	set(${prefix}_INCLUDE "${z}" "${PSP2_SRC}/cmake/compat/zlib" PARENT_SCOPE)
endfunction()
