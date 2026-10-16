# This module contains only module metadata and two variable exports.
set(fwfile "${CMAKE_CURRENT_SOURCE_DIR}/fw/${FW}.cmake")
if(NOT EXISTS "${fwfile}")
	return()
endif()
include("${fwfile}")
set(name vs0_sys_external_np_message_padding)
add_executable(${name} metadata.S)
target_compile_definitions(${name} PRIVATE
	MODULE_NID=${MODULE_NID} MODULE_SDK_VERSION=${MODULE_SDK_VERSION})
target_link_options(${name} PRIVATE -nostdlib
	-Wl,--no-fix-cortex-a8,--emit-relocs,-T,${CMAKE_CURRENT_SOURCE_DIR}/layout.ld)
set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/layout.ld")
add_custom_command(OUTPUT ${name}.velf
	COMMAND python3 "${PSP2_SRC}/tools/pack_data_module.py"
		$<TARGET_FILE:${name}> ${name}.velf --module-info-symbol moduleInfo
	DEPENDS ${name} $<TARGET_FILE:${name}>
		"${PSP2_SRC}/tools/pack_data_module.py" "${fwfile}"
	VERBATIM)
add_custom_target(${name}_velf ALL DEPENDS ${name}.velf)
