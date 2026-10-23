# NetBSD's files with Sony's changes: the pristine file from contrib plus Sony's patch,
# applied at configure time into the build directory
function(net_ps_prepare_patched upstream patch output)
	configure_file("${upstream}" "${output}" COPYONLY)
	execute_process(COMMAND patch -s "${output}" "${patch}"
		RESULT_VARIABLE result ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "net_ps adaptation of ${upstream} failed: ${error}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${upstream}" "${patch}")
endfunction()
