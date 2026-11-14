# os0:kd/vnzimg.skprx - image container module
psp2_module(os0_kd_vnzimg KIND kernel COMPILER gcc48 SOURCES
	image.S
)

# The Venezia firmware, built from firmware/ with the MeP toolchain (tools/mepbuild.sh)
# into vnzimg.bin (firmware/image.ld), which image.S carries.
set(vnz "${CMAKE_CURRENT_BINARY_DIR}/mep")
file(GLOB_RECURSE vnz_sources CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.S"
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.ld")
file(GLOB silk_sources CONFIGURE_DEPENDS "${PSP2_SRC}/contrib/silk-1.0.8/src/*"
	"${PSP2_SRC}/contrib/silk-1.0.8/interface/*")
add_custom_command(OUTPUT "${vnz}/vnzimg/vnzimg.bin"
	COMMAND ${CMAKE_COMMAND} -E env "MEP_OUT=${vnz}"
		sh "${PSP2_SRC}/tools/mepbuild.sh" "${CMAKE_CURRENT_SOURCE_DIR}/firmware"
	DEPENDS ${vnz_sources} ${silk_sources} "${CMAKE_CURRENT_SOURCE_DIR}/firmware/Makefile"
		"${PSP2_SRC}/tools/mepbuild.sh"
	VERBATIM)
target_compile_definitions(os0_kd_vnzimg PRIVATE IMAGE_BIN="${vnz}/vnzimg/vnzimg.bin")
set_source_files_properties(image.S PROPERTIES OBJECT_DEPENDS "${vnz}/vnzimg/vnzimg.bin")
