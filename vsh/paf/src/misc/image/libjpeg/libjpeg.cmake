# libpaf's libjpeg: pristine contrib/jpeg-8c plus Sony's changes (jpeg-8c-sce.patch: the
# duplicate component check in get_sos, huffval zeroed in get_dht, error messages formatted
# by paf and not shown, jmemnobs allocating from the paf heap), configured into the build
# tree with this folder's jconfig.h. Sets <prefix>_SOURCES (in Sony's link order) and
# <prefix>_INCLUDE.
set(PSP2_LIBJPEG_UPSTREAM "${PSP2_SRC}/contrib/jpeg-8c")
set(PSP2_LIBJPEG_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(PSP2_LIBJPEG_PATCH "${PSP2_LIBJPEG_DIR}/jpeg-8c-sce.patch")

function(psp2_libjpeg_sce prefix)
	set(dir "${CMAKE_BINARY_DIR}/jpeg-8c-sce")
	file(MD5 "${PSP2_LIBJPEG_PATCH}" patch_md5)
	set(stamp "")
	if(EXISTS "${dir}/.patched")
		file(READ "${dir}/.patched" stamp)
	endif()
	if(NOT stamp STREQUAL patch_md5)	# a changed patch is applied afresh
		file(REMOVE_RECURSE "${dir}")
		file(GLOB upstream "${PSP2_LIBJPEG_UPSTREAM}/*.[ch]")
		file(COPY ${upstream} DESTINATION "${dir}")
		execute_process(COMMAND patch -s -p1 -d "${dir}" -i "${PSP2_LIBJPEG_PATCH}"
			RESULT_VARIABLE result ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR "libjpeg SCE patch failed: ${error}")
		endif()
		file(WRITE "${dir}/.patched" "${patch_md5}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PSP2_LIBJPEG_PATCH}")
	set(${prefix}_INCLUDE "${dir}" "${PSP2_LIBJPEG_DIR}" PARENT_SCOPE)
	# the decoder's core, the rest of the decoder, the compressor: three places in libpaf
	set(core jcomapi jdapimin jdapistd jdinput jdmarker jdmaster jdmerge jdpostct jdsample
		jerror jmemmgr jquant1 jquant2 jutils)
	set(decoder jdarith jaricom jdcoefct jdcolor jddctmgr jdhuff jdmainct jidctflt jidctfst
		jidctint)
	set(compressor jcapimin jcapistd jcinit jcmainct jcmarker jcmaster jcparam jcprepct
		jcsample jcarith jccoefct jccolor jcdctmgr jchuff jfdctflt jfdctfst jfdctint)
	set(sources "")
	foreach(name IN LISTS core decoder compressor)
		list(APPEND sources "${dir}/${name}.c")
	endforeach()
	list(APPEND sources "${dir}/jmemnobs.c")
	set(${prefix}_SOURCES ${sources} PARENT_SCOPE)
endfunction()
