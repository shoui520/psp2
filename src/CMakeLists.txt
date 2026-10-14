# Superbuild: every module present in the selected firmware.
#
#   cmake -S src -B <builddir> -DFW=<FW> && cmake --build <builddir> -j12
#
# Modules come from fw/<FW>.list (module folders, relative to src/).

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	if(NOT DEFINED ENV{VITASDK})
		message(FATAL_ERROR "set VITASDK")
	endif()
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()

project(psp2 C ASM)

include(cmake/fw.cmake)
include(cmake/psp2.cmake)

set(PSP2_ONLY "" CACHE STRING "build only modules whose folder contains this text")

file(STRINGS "fw/${FW}.list" modules)
foreach(m IN LISTS modules)
	if(PSP2_ONLY AND NOT m MATCHES "${PSP2_ONLY}")
		continue()
	endif()
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${m}/CMakeLists.txt")
		add_subdirectory(${m})
	endif()
endforeach()
