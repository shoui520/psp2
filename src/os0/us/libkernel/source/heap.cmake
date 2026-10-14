function(libkernel_prepare_heap upstream patch output)
	configure_file("${upstream}" "${output}" COPYONLY)
	execute_process(COMMAND patch -s "${output}" "${patch}"
		RESULT_VARIABLE result ERROR_VARIABLE error)
	if(NOT result EQUAL 0)
		message(FATAL_ERROR "LibKernel allocator adaptation failed: ${error}")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${upstream}" "${patch}")
endfunction()
