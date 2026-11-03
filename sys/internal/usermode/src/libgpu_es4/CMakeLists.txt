# os0:us/libgpu_es4.suprx - SGX services client (bridge into gpu_es4.skprx)
psp2_module(os0_us_libgpu_es4 KIND user COMPILER gcc48 SOURCES
	gpu_user.c
	sgx.c
	pvrsrv.c
	osfunc.c
	services.c
	pds.c
	usse.c
	IMPORTS sceClibMspaceCalloc
)
# Sony built this library at -O3 without strict aliasing, like libgxm_es4: its USSE emitter is
# vectorized and the bridge wrappers keep their stores in source order
target_compile_options(os0_us_libgpu_es4 PRIVATE -O3 -fno-strict-aliasing)

# The library's GPU programs, assembled from source by the SGX543's assemblers
# (toolchains/build-usetools.sh), like gpuinit_es4's: the USSE programs of ukernel/ and the
# PDS programs of pds/. pds.c takes the PDS programs' words from the generated files.
if(NOT PSP2_USEASM OR NOT PSP2_PDSASM)
	message(FATAL_ERROR "libgpu_es4 needs the SGX assemblers useasm and pdsasm: run "
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

# pds_program(NAME KIND): NAME.pds.asm assembled, and its words as NAME.inc (KIND inc), or
# as NAME_code.h, a #define per code word (KIND code, the data segment left out for a caller
# that fills it).
function(pds_program name kind)
	add_custom_command(OUTPUT "${programs}/${name}.h"
		COMMAND "${PSP2_PDSASM}" "${CMAKE_CURRENT_SOURCE_DIR}/pds/${name}.pds.asm"
			"${programs}/${name}.h"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pds/${name}.pds.asm" VERBATIM)
	if(kind STREQUAL "code")
		set(args --code)
		set(output "${name}_code.h")
	else()
		set(args)
		set(output "${name}.inc")
	endif()
	add_custom_command(OUTPUT "${programs}/${output}"
		COMMAND python3 -I "${PSP2_SRC}/tools/pdsbin.py" ${args}
			"${programs}/${output}" "${programs}/${name}.h"
		DEPENDS "${programs}/${name}.h" "${PSP2_SRC}/tools/pdsbin.py" VERBATIM)
endfunction()

use_program(end.bin end.use.asm)
use_program(load_sequence.bin load_sequence.use.asm)
foreach(name end load_sequence)
	add_custom_command(OUTPUT "${programs}/${name}_code.h"
		COMMAND python3 -I "${PSP2_SRC}/tools/pdsbin.py"
			"${programs}/${name}_code.h" "${programs}/${name}.bin"
		DEPENDS "${programs}/${name}.bin" "${PSP2_SRC}/tools/pdsbin.py" VERBATIM)
endforeach()

foreach(name pixelevent pixelevent_msnods_2x pixelevent_msnods_4x)
	pds_program(${name} inc)
endforeach()
pds_program(stateupdate code)

set_source_files_properties(pds.c PROPERTIES OBJECT_DEPENDS
	"${programs}/pixelevent.inc;${programs}/pixelevent_msnods_2x.inc;${programs}/pixelevent_msnods_4x.inc;${programs}/stateupdate_code.h;${programs}/end_code.h")
target_include_directories(os0_us_libgpu_es4 PRIVATE "${programs}")

# the USSE programs' words, for the emitters' tables
set_source_files_properties(usse.c PROPERTIES OBJECT_DEPENDS
	"${programs}/end_code.h;${programs}/load_sequence_code.h")
