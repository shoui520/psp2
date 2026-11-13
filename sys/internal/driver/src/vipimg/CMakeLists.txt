# os0:kd/vipimg.skprx - image container module
psp2_module(os0_kd_vipimg KIND kernel COMPILER gcc48 SOURCES
	image.S
)

# The VIP firmware, built from firmware/ with the MeP toolchain (tools/mepbuild.sh)
# into vipimg.bin, which image.S carries.
set(vip "${CMAKE_CURRENT_BINARY_DIR}/mep")
file(GLOB_RECURSE vip_sources CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.S"
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.ld")
add_custom_command(OUTPUT "${vip}/vipimg/vipimg.bin"
	COMMAND ${CMAKE_COMMAND} -E env "MEP_OUT=${vip}"
		sh "${PSP2_SRC}/tools/mepbuild.sh" "${CMAKE_CURRENT_SOURCE_DIR}/firmware"
	DEPENDS ${vip_sources} "${CMAKE_CURRENT_SOURCE_DIR}/firmware/Makefile"
		"${CMAKE_CURRENT_SOURCE_DIR}/firmware/pack.py" "${PSP2_SRC}/tools/mepbuild.sh"
	VERBATIM)
target_compile_definitions(os0_kd_vipimg PRIVATE IMAGE_BIN="${vip}/vipimg/vipimg.bin")
set_source_files_properties(image.S PROPERTIES OBJECT_DEPENDS "${vip}/vipimg/vipimg.bin")
