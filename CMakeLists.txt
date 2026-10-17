# Superbuild: every module present in the selected firmware.
#
#   cmake -S . -B <builddir> -DFW=<FW> && cmake --build <builddir> -j8
#
# release/<FW>.list names each module by its rel (its firmware path without the extension,
# os0/kd/sysmem); release/modules.tsv gives the folder of Sony's tree its sources live in.
# A module builds in <builddir>/<rel>, so build trees and target names keep its identity.

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

set(PSP2_ONLY "" CACHE STRING "build only modules whose rel or folder contains this text")

file(STRINGS "release/modules.tsv" manifest REGEX "^[^#]")
foreach(row IN LISTS manifest)
	string(REPLACE "\t" ";" row "${row}")
	list(GET row 0 rel)
	list(GET row 1 folder)
	set(PSP2_FOLDER_${rel} "${folder}")
endforeach()

file(STRINGS "release/${FW}.list" modules)
foreach(m IN LISTS modules)
	set(folder "${PSP2_FOLDER_${m}}")
	if(NOT folder)
		message(FATAL_ERROR "${m} (release/${FW}.list) has no folder in release/modules.tsv")
	endif()
	if(PSP2_ONLY AND NOT m MATCHES "${PSP2_ONLY}" AND NOT folder MATCHES "${PSP2_ONLY}")
		continue()
	endif()
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/CMakeLists.txt")
		# a folder Sony built once for several rels (byte-identical copies) names its
		# target from the rel: psp2_module(${PSP2_TARGET} ...)
		string(MAKE_C_IDENTIFIER "${m}" PSP2_TARGET)
		add_subdirectory(${folder} ${m})
	endif()
endforeach()
