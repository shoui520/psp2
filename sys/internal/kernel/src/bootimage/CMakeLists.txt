# Children under conversion do not emit VELFs yet, so the container cannot link.
# Remove this partial declaration and early return when every child links again.
psp2_module(os0_kd_bootimage KIND kernel PARTIAL SOURCES bootimage.c)
return()

# The parent contains an ordered table of independently rebuilt child VELFs.
include(modules.cmake)
list(LENGTH BOOTIMAGE_MODULES BOOTIMAGE_COUNT)
set(BOOTIMAGE_ENTRIES "")
set(BOOTIMAGE_PAYLOADS "")
set(bootimage_child_targets "")
set(bootimage_child_files "")
foreach(child IN LISTS BOOTIMAGE_MODULES)
	set(child_target "os0_kd_bootimage_${child}")
	set(child_file "${CMAKE_CURRENT_BINARY_DIR}/${child}/${child_target}.velf")
	list(APPEND bootimage_child_targets "${child_target}_velf")
	list(APPEND bootimage_child_files "${child_file}")
	string(APPEND BOOTIMAGE_ENTRIES
		"\t.word BootimagePath_${child}\n"
		"\t.word BootimageImage_${child}\n"
		"\t.word BootimageEnd_${child} - BootimageImage_${child}\n")
	string(APPEND BOOTIMAGE_PAYLOADS
		"\t.balign 4\nBootimagePath_${child}:\n"
		"\t.asciz \"os0:kd/${child}.skprx\"\n"
		"\t.balign 4\nBootimageImage_${child}:\n"
		"\t.incbin \"${child_file}\"\nBootimageEnd_${child}:\n")
endforeach()

configure_file(bootimage.S.in "${CMAKE_CURRENT_BINARY_DIR}/bootimage.S" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/bootimage.S"
	PROPERTIES OBJECT_DEPENDS "${bootimage_child_files}")
psp2_module(os0_kd_bootimage KIND kernel SOURCES
	bootimage.c
	"${CMAKE_CURRENT_BINARY_DIR}/bootimage.S")
add_dependencies(os0_kd_bootimage ${bootimage_child_targets})
