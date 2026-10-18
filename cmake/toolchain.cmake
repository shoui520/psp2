# The project's toolchain: Sony's compilers (CMakeLists.txt uses it unless given another).
#
# GCC 4.8.3 (toolchains/build-csl.sh: CodeSourcery 2014.05, the base of Sony's GCC, with the
# corrections toward Sony's) and its binutils (toolchains/build-binutils.sh) compile, assemble and
# link, with Sony's flags. A module Sony built with SNC 3.57 (the SDK's psp2snc.exe) has it
# compile its sources instead (psp2_module COMPILER snc).
#
# Both are programs of another system (GCC 4.8 of Linux, SNC of Windows): tools/sonycc.py writes
# a command per GCC 4.8 program into the build folder that runs it on this host (natively, in
# Lima, in WSL2; tools/host.py), and the flags (toolchain/flags.cmake).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(PSP2_TOOLCHAIN sony)

set(PSP2_TOOLCHAIN_DIR "${CMAKE_BINARY_DIR}/toolchain")
execute_process(COMMAND python3 -I "${CMAKE_CURRENT_LIST_DIR}/../tools/sonycc.py" toolchain
		"${PSP2_TOOLCHAIN_DIR}"
		RESULT_VARIABLE rc ERROR_VARIABLE err)
if(rc)
	message(FATAL_ERROR "${err}")
endif()
include("${PSP2_TOOLCHAIN_DIR}/flags.cmake")

set(CMAKE_C_COMPILER "${PSP2_GCC48_GCC}")
set(CMAKE_CXX_COMPILER "${PSP2_GCC48_GXX}")
set(CMAKE_ASM_COMPILER "${PSP2_GCC48_GCC}")
set(CMAKE_AR "${PSP2_GCC48_AR}")
set(CMAKE_RANLIB "${PSP2_GCC48_RANLIB}")
set(CMAKE_OBJCOPY "${PSP2_GCC48_OBJCOPY}")
set(CMAKE_NM "${PSP2_GCC48_NM}")
set(CMAKE_READELF "${PSP2_GCC48_READELF}")
set(CMAKE_STRIP "${PSP2_GCC48_STRIP}")
# The compilers are known, and CMake's probes would compile files of its own installation, which
# the system the compiler runs in may not see: no identification or checks
foreach(lang C CXX)
	set(CMAKE_${lang}_COMPILER_ID_RUN TRUE)
	set(CMAKE_${lang}_COMPILER_FORCED TRUE)
	set(CMAKE_${lang}_COMPILER_ID GNU)
	set(CMAKE_${lang}_COMPILER_VERSION 4.8.3)
	set(CMAKE_${lang}_SIZEOF_DATA_PTR 4)
	set(CMAKE_${lang}_COMPILER_ABI ELF)
endforeach()
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT 90)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT 98)
# nothing links without a module's linker script: the compiler checks build a library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Thumb unless a source asks for ARM (-marm); the C library's headers are the tree's
# (sys/sdk/target/include), as Sony's GCC builds read the SDK's
set(CMAKE_C_FLAGS_INIT "-mthumb ${PSP2_GCC48_TARGET} -std=gnu11 ${PSP2_GCC48_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "-mthumb ${PSP2_GCC48_TARGET} ${PSP2_GCC48_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "-mthumb ${PSP2_GCC48_TARGET}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
