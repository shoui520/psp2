# Superbuild: every module present in the selected firmware.
#
#   cmake -S . -B <builddir> -DFW=<FW> && cmake --build <builddir> -j8
#
# release/<FW>.list names each module by its rel (its firmware path without the extension,
# os0/kd/sysmem); release/modules.tsv gives the folder of Sony's tree its sources live in.
# A module builds in <builddir>/<rel>, so build trees and target names keep its identity, and
# comes out there as the firmware ships it, under its firmware name (sysmem.skprx).

cmake_minimum_required(VERSION 3.19)

# Sony's compilers (cmake/toolchain.cmake) unless another toolchain file is given: vitasdk's
# (-DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake) still builds the modules of later
# roadmap stages, until they are built with Sony's compilers too
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE PATH "toolchain file")
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
	list(GET row 2 devpath)
	set(PSP2_FOLDER_${rel} "${folder}")
	set(PSP2_DEVPATH_${rel} "${devpath}")
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
		if(PSP2_TOOLCHAIN STREQUAL "sony")
			# built with Sony's compilers once its CMakeLists names one (psp2_module COMPILER);
			# until then a later roadmap stage
			file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/CMakeLists.txt" text)
			if(NOT text MATCHES "COMPILER (gcc48|snc)" AND NOT text MATCHES "psp2_image\\(")
				message(STATUS "psp2: ${m} has no COMPILER (a later roadmap stage), skipped")
				continue()
			endif()
		endif()
		# a folder Sony built once for several rels (byte-identical copies) names its
		# target from the rel: psp2_module(${PSP2_TARGET} ...)
		string(MAKE_C_IDENTIFIER "${m}" PSP2_TARGET)
		# the file it ships as, where that is a SELF (not a module inside another image)
		set(PSP2_SELF_NAME "")
		if(PSP2_DEVPATH_${m} MATCHES "([^/:]+\\.(skprx|suprx|self))$")
			set(PSP2_SELF_NAME "${CMAKE_MATCH_1}")
		endif()
		add_subdirectory(${folder} ${m})
	endif()
endforeach()
