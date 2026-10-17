# Select unchanged routines from a pinned upstream translation unit.
# Generated C stays in the build tree; the vendored source remains pristine.
function(gcc48_select_unwind upstream output prefix_end accessors_start accessors_end)
	file(READ "${upstream}" source)
	string(FIND "${source}" "${prefix_end}" prefix_size)
	string(FIND "${source}" "${accessors_start}" accessors_offset)
	string(FIND "${source}" "${accessors_end}" accessors_limit)
	if(prefix_size LESS 0 OR accessors_offset LESS prefix_size OR
	   accessors_limit LESS accessors_offset)
		message(FATAL_ERROR "Pinned GCC unwind source selection markers missing")
	endif()
	math(EXPR accessors_size "${accessors_limit} - ${accessors_offset}")
	string(SUBSTRING "${source}" 0 ${prefix_size} prefix)
	string(SUBSTRING "${source}" ${accessors_offset} ${accessors_size} accessors)
	file(WRITE "${output}" "${prefix}\n${accessors}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${upstream}")
endfunction()
