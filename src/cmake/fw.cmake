# Firmware selection: -DFW=<id>, where cmake/fw/<id>.cmake exists.
# Exposes FW_VERSION / FW_TYPE / FW_SDK_VERSION / FW_DEBUG to CMake and to C.

set(FW "4.00-DEBUGGER" CACHE STRING "firmware to build for")
set(FW_FILE "${CMAKE_CURRENT_LIST_DIR}/fw/${FW}.cmake")
if(NOT EXISTS "${FW_FILE}")
	file(GLOB known RELATIVE "${CMAKE_CURRENT_LIST_DIR}/fw" "${CMAKE_CURRENT_LIST_DIR}/fw/*.cmake")
	message(FATAL_ERROR "unknown FW '${FW}', known: ${known}")
endif()
include("${FW_FILE}")

add_compile_definitions(
	FW_VERSION=${FW_VERSION}
	FW_TYPE_${FW_TYPE}=1
	FW_SDK_VERSION=${FW_SDK_VERSION}
	FW_DEBUG=${FW_DEBUG}
)

# fw_value(<out> <name>): pick <name>_<FW> from the caller's per-firmware table
function(fw_value out name)
	if(NOT DEFINED ${name}_${FW})
		message(FATAL_ERROR "${name} has no value for FW ${FW}")
	endif()
	set(${out} "${${name}_${FW}}" PARENT_SCOPE)
endfunction()
