# psp2_paf_cg(TARGET [FOLDER [LIST_FILE]]): PAF's GPU programs. Each program LIST_FILE (default
# programs.S) lists is compiled from its Cg source in this folder (Sony's paf/cg/ paths; canvas/
# is ours: the canvas painter's programs carry no path), or in FOLDER for another module's
# programs, by psp2cgc (tools/cgc.py) into the build folder, and LIST_FILE takes the binaries
# with .incbin.
function(psp2_paf_cg target)
	set(cg "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
	if(ARGC GREATER 1)
		get_filename_component(cg "${ARGV1}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	endif()
	set(list_file "programs.S")
	if(ARGC GREATER 2)
		set(list_file "${ARGV2}")
	endif()
	set(out "${CMAKE_CURRENT_BINARY_DIR}/cg")
	file(STRINGS "${cg}/${list_file}" lines REGEX "^\tPROGRAM ")
	set(binaries)
	foreach(line IN LISTS lines)
		string(REGEX REPLACE "^.*\"(.*)\\.gxp\"$" "\\1" program "${line}")
		# The files a program includes (one level, relative to its own folder).
		get_filename_component(dir "${cg}/${program}" DIRECTORY)
		file(STRINGS "${cg}/${program}" includes REGEX "^#include \"")
		set(depends "${cg}/${program}")
		foreach(include IN LISTS includes)
			string(REGEX REPLACE "^#include \"(.*)\".*$" "\\1" include "${include}")
			list(APPEND depends "${dir}/${include}")
			set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${cg}/${program}")
		endforeach()
		add_custom_command(OUTPUT "${out}/${program}.gxp"
			COMMAND python3 -I "${PSP2_SRC}/tools/cgc.py" "${cg}/${program}" "${out}/${program}.gxp"
			DEPENDS ${depends} "${PSP2_SRC}/tools/cgc.py" VERBATIM)
		list(APPEND binaries "${out}/${program}.gxp")
	endforeach()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${cg}/${list_file}")
	set_source_files_properties("${cg}/${list_file}" TARGET_DIRECTORY ${target} PROPERTIES
		COMPILE_OPTIONS "-Wa,-I${out}" OBJECT_DEPENDS "${binaries}")
endfunction()
