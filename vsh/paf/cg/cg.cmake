# psp2_paf_cg(TARGET): PAF's GPU programs. Each program programs.S lists is compiled from its
# Cg source in this folder (Sony's paf/cg/ paths; canvas/ is ours: the canvas painter's programs
# carry no path) by psp2cgc (tools/cgc.py) into the build folder, and programs.S takes the
# binaries with .incbin.
function(psp2_paf_cg target)
	set(cg "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
	set(out "${CMAKE_CURRENT_BINARY_DIR}/cg")
	file(STRINGS "${cg}/programs.S" lines REGEX "^\tPROGRAM ")
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
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${cg}/programs.S")
	set_source_files_properties("${cg}/programs.S" TARGET_DIRECTORY ${target} PROPERTIES
		COMPILE_OPTIONS "-Wa,-I${out}" OBJECT_DEPENDS "${binaries}")
endfunction()
