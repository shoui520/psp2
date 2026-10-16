# Vendored open-source code for sysmem: pristine upstream from contrib plus Sony's
# adaptation patch, applied at configure time into the build directory.
function(sysmem_prepare_patched upstream patch output)
	configure_file("${upstream}" "${output}" COPYONLY)
	execute_process(COMMAND patch -s "${output}" "${patch}"
		RESULT_VARIABLE result ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "Sysmem adaptation of ${upstream} failed: ${error}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${upstream}" "${patch}")
endfunction()
