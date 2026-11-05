psp2_module(os0_kd_bootimage_gpuinit_es4 KIND kernel COMPILER gcc48 SOURCES
	common/srvinit.c
	devices/sgx/sgxinit.c
	programs.S
)

# Bootimage payloads occupy the original writable segment at +0x2000.
target_compile_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Os)
target_link_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Wl,-Tdata=0x81002000)

include(${CMAKE_CURRENT_LIST_DIR}/../gpu/sgx_ddk.cmake)
target_include_directories(os0_kd_bootimage_gpuinit_es4 PRIVATE ${PSP2_SGX_DDK_INCLUDE_DIRS})

# The GPU programs, assembled from source by the SGX543's assemblers
# (toolchains/build-usetools.sh): the USE code of the master and slave microkernels and of the
# clear clip workaround (ukernel/), and the PDS programs (pds/). programs.S takes the binaries
# with .incbin, programs.h the PDS programs sgxinit.c keeps as words.
if(NOT PSP2_USEASM OR NOT PSP2_PDSASM)
	message(FATAL_ERROR "gpuinit_es4 needs the SGX assemblers useasm and pdsasm: run "
		"toolchains/build-usetools.sh")
endif()
set(programs "${CMAKE_CURRENT_BINARY_DIR}/programs")
file(MAKE_DIRECTORY "${programs}")

function(use_program output)
	list(TRANSFORM ARGN PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/ukernel/" OUTPUT_VARIABLE sources)
	add_custom_command(OUTPUT "${programs}/${output}"
		COMMAND "${PSP2_USEASM}" -o "${programs}/${output}" ${sources}
		DEPENDS ${sources} VERBATIM)
endfunction()

# pds_program(OUTPUT SOURCE...): the programs one after another, OUTPUT .bin or .inc
function(pds_program output)
	set(headers)
	foreach(source IN LISTS ARGN)
		set(header "${programs}/${source}.h")
		if(NOT header IN_LIST headers)
			add_custom_command(OUTPUT "${header}"
				COMMAND "${PSP2_PDSASM}" "${CMAKE_CURRENT_SOURCE_DIR}/pds/${source}.pds.asm" "${header}"
				DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pds/${source}.pds.asm" VERBATIM)
		endif()
		list(APPEND headers "${header}")
	endforeach()
	add_custom_command(OUTPUT "${programs}/${output}"
		COMMAND python3 -I "${PSP2_SRC}/tools/pdsbin.py" "${programs}/${output}" ${headers}
		DEPENDS ${headers} "${PSP2_SRC}/tools/pdsbin.py" VERBATIM)
endfunction()

use_program(program0.bin eventhandler.use.asm sgxinit.use.asm supervisor.use.asm
	sgx_timer.use.asm ta.asm sgx_utils.asm spm.asm 3d.asm 2d.asm)
use_program(program1.bin slave_eventhandler.use.asm slave_supervisor.use.asm sgx_utils.asm)
use_program(program10.bin clearclipwa.use.asm)
foreach(n 2 3 4 5 6 7)
	pds_program(program${n}.inc program${n})
endforeach()
pds_program(program9.bin program9 program9_if0 program9)

set_source_files_properties(programs.S PROPERTIES
	COMPILE_OPTIONS "-Wa,-I${programs}"
	OBJECT_DEPENDS "${programs}/program0.bin;${programs}/program1.bin;${programs}/program9.bin;${programs}/program10.bin")
set_source_files_properties(devices/sgx/sgxinit.c PROPERTIES
	OBJECT_DEPENDS "${programs}/program2.inc;${programs}/program3.inc;${programs}/program4.inc;${programs}/program5.inc;${programs}/program6.inc;${programs}/program7.inc")
target_include_directories(os0_kd_bootimage_gpuinit_es4 PRIVATE "${programs}")

