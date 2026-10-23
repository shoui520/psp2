# slb2:kbl/SceSysmem - the secure world's SceSysmem. Sony built it from os0
# SceSysmem's sources with the Non-secure parts left out (SYSMEM_TZS).
get_filename_component(sysmem_dir .. ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(../sources.cmake)

# Only part of os0's code is in the secure world's image: each function and object in a
# section of its own, and the link keeps what the module's exports reach.
list(APPEND MODULE_COMPILE_OPTIONS -ffunction-sections -fdata-sections)
psp2_module(slb2_kbl_SceSysmem KIND kernel COMPILER gcc48 SOURCES
	${sysmem_sources}
)
if(TARGET slb2_kbl_SceSysmem)
	sysmem_settings(slb2_kbl_SceSysmem)
	target_compile_definitions(slb2_kbl_SceSysmem PRIVATE SYSMEM_TZS)
	get_target_property(type slb2_kbl_SceSysmem TYPE)
	if(type STREQUAL "EXECUTABLE")
		target_link_options(slb2_kbl_SceSysmem PRIVATE -Wl,--gc-sections)
	endif()
endif()
