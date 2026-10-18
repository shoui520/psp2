# Shared module build for every module folder of the tree (release/modules.tsv).
#
#   psp2_module(<target> KIND kernel|user [COMPILER gcc48|snc] [SKELETON] [SCE_LAYOUT] [PARTIAL]
#               SOURCES <files...>)
#
# COMPILER: the compiler Sony built the module with (agent index compilers.<FW>.json): GCC 4.8
# (the kernel, LibKernel, some libraries) or SNC (most user libraries and apps). With the project's
# toolchain (cmake/toolchain.cmake) every module is built in Sony's layout (SCE_LAYOUT) by its
# compiler; a folder without COMPILER is not built with Sony's compilers yet (its roadmap stage
# has not started) and the superbuild skips it. vitasdk's toolchain (-DCMAKE_TOOLCHAIN_FILE)
# builds every module with its GCC and ignores COMPILER.
#
# PARTIAL: a module being converted to C whose code still calls functions not written
# yet. Its sources are compiled with the module's flags (errors and warnings show, and
# match.py finds the flags as usual) but nothing is linked and no .velf is made. Drop
# PARTIAL when the last function is C; the module then links and is checked again.
#
# SCE_LAYOUT: Sony's module layout. tools/mkprx.py generates the import stubs and
# SCE tables (scetables.S) from the module data below, cmake/prx.ld links them in
# Sony's section order, and mkprx.py turns the linked ELF into the .velf (instead
# of vita-elf-create and fixup_velf.py). Needs MODULE_IMPORT_NIDS.
#
# SKELETON: generated stub module; the data segment is pushed 64 KiB further out
# (sce_reserve.ld) so the SCE tables of large originals fit after the text.
# MODULE_SCE_RESERVE (set before the call) does the same for a C module.
#
# Per-firmware module data lives in <module>/fw/<FW>.cmake and sets:
#   MODULE_NID            module NID (vita-elf-create leaves it 0, patched afterwards)
#   MODULE_SDK_VERSION    value of the exported module_sdk_version variable
#   MODULE_ATTR           module attributes (vita-elf-create writes 0, patched afterwards)
#   MODULE_EXPORTS        export config (yml), relative to the module folder
#   MODULE_IMPORT_LIBS    LIBNID:VERSION:ATTR per imported library (import record header)
#   MODULE_IMPORT_NIDS    LIBNID:F|V:NID per import, in the original's order (SCE_LAYOUT)
#   MODULE_IMPORT_FORMAT  0x34 for the long import records (default 0x24)
#   MODULE_NID0_VAR_ORDER the module's own variables' NIDs, where not SDK version, extras, info
#   MODULE_EXPORT_ORDER   export record NIDs (0: the module's own) in the original's order
#   MODULE_EXPORT_ATTRS   LIBNID:ATTR for exports whose attributes the yml cannot express
#   MODULE_IMPORT_FUNCS   import symbols the module must reference (skeletons only)
#   MODULE_IMPORT_VARS
#   MODULE_NID0_VARS      NID:symbol per extra variable of the module's own (NID 0) export
#                         record, beyond module_info / module_sdk_version (process param,
#                         ...); vita-elf-create cannot emit them, fixup_velf.py splices them in
# A module absent from a firmware has no fw/<FW>.cmake and is skipped.
# Hand-written per-firmware extras go in <module>/fw/<FW>.local.cmake (never generated):
#   MODULE_EXPORT_AUX     LIBNID:AUXATTR for export records whose auxattr byte is set
#   MODULE_DEFINES        compile definitions (e.g. which data file a firmware uses)
#   MODULE_DEPENDS        extra files the sources depend on (.incbin data)
#
# Import stubs: one archive per library, generated from tools/nids/<kernel|user>.yml
# by tools/mkstubs.py into ${PSP2_STUBS}/<kernel|user>; KIND picks the set.

set(PSP2_SRC "${CMAKE_CURRENT_LIST_DIR}/..")
# the include roots of every module, after its own folder: Sony's internal interfaces
# (sys/internal/include/<component>/), the SDK headers we write at Sony's SDK paths
# (sys/sdk/target/include), the build parameters (cmake/include: fw.h, skeleton.h) and the
# shims that complete vitasdk's headers (cmake/compat/vitasdk)
set(PSP2_INCLUDE_DIRS
	"${PSP2_SRC}/sys/internal/include"
	"${PSP2_SRC}/sys/sdk/target/include"
	"${PSP2_SRC}/cmake/include"
	"${PSP2_SRC}/cmake/compat/vitasdk")
# every module in Sony's layout (psp2_module SCE_LAYOUT), e.g. to check the tables
option(PSP2_SCE_LAYOUT "build every module with Sony's layout" OFF)
set(PSP2_STUBS "${CMAKE_BINARY_DIR}/stubs")
# Firmware extracted from the originals (<FW>/payload/<module>/image.bin, ...). Modules
# whose payload for another processor is not rebuilt yet embed the original image from
# here; it is not part of src/. Set -DPSP2_FW_DIR or the PSP2_FW_DIR environment variable.
set(PSP2_FW_DIR "$ENV{PSP2_FW_DIR}" CACHE PATH "extracted original firmware")

# import stub archives for vitasdk's packaging (vita-elf-create); Sony's layout makes its own stubs
foreach(ns kernel user)
	if(PSP2_TOOLCHAIN STREQUAL "sony")
		break()
	endif()
	set(db "${PSP2_SRC}/tools/nids/${ns}.yml")
	set(out "${PSP2_STUBS}/${ns}")
	if(NOT EXISTS "${out}/.stamp" OR "${db}" IS_NEWER_THAN "${out}/.stamp"
	   OR "${PSP2_SRC}/tools/mkstubs.py" IS_NEWER_THAN "${out}/.stamp")
		message(STATUS "psp2: generating ${ns} import stubs")
		file(REMOVE_RECURSE "${out}")
		execute_process(COMMAND python3 "${PSP2_SRC}/tools/mkstubs.py" "${db}" "${out}"
				RESULT_VARIABLE rc)
		if(rc)
			message(FATAL_ERROR "mkstubs failed")
		endif()
		file(TOUCH "${out}/.stamp")
	endif()
	# a changed database (or generator) reconfigures the build, which makes the stubs again
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${db}" "${PSP2_SRC}/tools/mkstubs.py")
	file(GLOB PSP2_STUB_LIBS_${ns} "${out}/*_stub.a")
endforeach()

# __FILE_NAME__ of each C/C++ source, as Sony's debug records name it: the path from its
# component's folder (the nearest folder above it with a CMakeLists.txt); the bare file name
# outside a component (contrib/, generated). tools/sonycc.py file_name() is the same. GCC 4.8
# has no built-in __FILE_NAME__, so every source gets the define (a header whose records name
# the header defines it itself).
function(psp2_file_names)
	get_filename_component(root "${PSP2_SRC}" REALPATH)
	foreach(src IN LISTS ARGN)
		if(NOT src MATCHES "\\.(c|cpp)$")
			continue()
		endif()
		get_filename_component(abs "${src}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
		get_filename_component(name "${abs}" NAME)
		get_filename_component(dir "${abs}" DIRECTORY)
		string(FIND "${dir}/" "${root}/" at)
		while(at EQUAL 0 AND NOT dir STREQUAL root)
			if(EXISTS "${dir}/CMakeLists.txt")
				file(RELATIVE_PATH name "${dir}" "${abs}")
				break()
			endif()
			get_filename_component(dir "${dir}" DIRECTORY)
			string(FIND "${dir}/" "${root}/" at)
		endwhile()
		set_property(SOURCE "${src}" APPEND PROPERTY COMPILE_DEFINITIONS "__FILE_NAME__=\"${name}\"")
	endforeach()
endfunction()

# the compile settings of a module's sources (psp2_module's variables in scope)
macro(psp2_compile_settings name)
	set(include_dirs ${PSP2_INCLUDE_DIRS})
	if(M_COMPILER STREQUAL "gcc48" AND DEFINED PSP2_SYSTEM_INCLUDE AND
	   (NOT M_KIND STREQUAL "kernel" OR MODULE_VITASDK_LIBC))
		# vitasdk's C library (newlib) and user headers ahead of the tree's C library: user-mode
		# C until the tree has the user-mode SDK headers, and the kernel modules that still build
		# NetBSD code over newlib's socket headers (MODULE_VITASDK_LIBC); the shims first
		set(include_dirs
			"${PSP2_SRC}/sys/internal/include"
			"${PSP2_SRC}/cmake/compat/vitasdk"
			"${PSP2_SYSTEM_INCLUDE}"
			"${PSP2_SRC}/sys/sdk/target/include"
			"${PSP2_SRC}/cmake/include")
	endif()
	target_include_directories(${name} PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}"
		${include_dirs}
		"${PSP2_SRC}")
	target_compile_definitions(${name} PRIVATE ${MODULE_DEFINES})
	# after the module's CMakeLists, which may set a source's COMPILE_DEFINITIONS outright
	set(names "")
	foreach(src IN LISTS M_SOURCES)
		string(APPEND names " [==[${src}]==]")
	endforeach()
	cmake_language(EVAL CODE "cmake_language(DEFER CALL psp2_file_names ${names})")
	if(DEFINED MODULE_SDK_VERSION)
		# the SDK the original was built with (Sony's sdk_version.h name)
		target_compile_definitions(${name} PRIVATE SCE_PSP2_SDK_VERSION=${MODULE_SDK_VERSION})
	endif()
	if(MODULE_DEPENDS)
		set_source_files_properties(${M_SOURCES} PROPERTIES OBJECT_DEPENDS "${MODULE_DEPENDS}")
	endif()
	# a call without a declaration is an error (GCC 4.8 only warns); GCC 14 and later make
	# incompatible pointer types an error, which Sony's GCC 4.8 warns about
	target_compile_options(${name} PRIVATE
		-Wall -Wno-unused-parameter -O2 -fno-builtin -ffreestanding -fstack-protector-strong
		"$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>"
		"$<$<AND:$<COMPILE_LANGUAGE:C>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,14>>:-Wno-error=incompatible-pointer-types>")
	if(PSP2_TOOLCHAIN STREQUAL "sony" AND M_COMPILER STREQUAL "snc")
		# SNC compiles in place of the GCC command CMake makes
		set_target_properties(${name} PROPERTIES C_COMPILER_LAUNCHER
			"python3;-I;${PSP2_SRC}/tools/sonycc.py;snc"
			CXX_COMPILER_LAUNCHER "python3;-I;${PSP2_SRC}/tools/sonycc.py;snc")
	endif()
	if(MODULE_COMPILE_OPTIONS)
		# per module, after the defaults: flags of Sony's own build that change behaviour
		target_compile_options(${name} PRIVATE ${MODULE_COMPILE_OPTIONS})
	endif()
	if(M_KIND STREQUAL "kernel")
		target_compile_definitions(${name} PRIVATE PSP2_KERNEL=1)
	endif()
endmacro()

function(psp2_module name)
	cmake_parse_arguments(M "SKELETON;SCE_LAYOUT;PARTIAL" "KIND;COMPILER" "SOURCES;IMPORTS" ${ARGN})
	set(fwfile "${CMAKE_CURRENT_SOURCE_DIR}/fw/${FW}.cmake")
	if(NOT EXISTS "${fwfile}")
		message(STATUS "psp2: ${name} not in ${FW}, skipped")
		return()
	endif()
	if(PSP2_TOOLCHAIN STREQUAL "sony" AND NOT M_COMPILER)
		message(FATAL_ERROR "${name}: psp2_module needs COMPILER with Sony's toolchain")
	endif()
	set(MODULE_ATTR 0)
	set(MODULE_IMPORT_LIBS "")
	set(MODULE_EXPORT_ATTRS "")
	set(MODULE_DEFINES "")
	set(MODULE_DEPENDS "")
	set(MODULE_NID0_VARS "")
	set(MODULE_IMPORT_NIDS "")
	set(MODULE_EXPORT_ORDER "")
	set(MODULE_EXPORT_AUX "")
	set(MODULE_IMPORT_FORMAT 0x24)
	set(MODULE_NID0_VAR_ORDER "")
	include("${fwfile}")
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fw/${FW}.local.cmake")
		include("${CMAKE_CURRENT_SOURCE_DIR}/fw/${FW}.local.cmake")
	endif()
	foreach(dep IN LISTS MODULE_DEPENDS)
		# extracted originals a module embeds (PSP2_FW_DIR)
		if(NOT EXISTS "${dep}")
			message(FATAL_ERROR "${CMAKE_CURRENT_SOURCE_DIR} needs ${dep}: set PSP2_FW_DIR "
				"to the extracted firmware (<FW>/payload/<module>/...)")
		endif()
	endforeach()

	if(M_PARTIAL)
		add_library(${name} OBJECT ${M_SOURCES})
		psp2_compile_settings(${name})
		return()
	endif()

	if(M_SCE_LAYOUT OR PSP2_SCE_LAYOUT OR PSP2_TOOLCHAIN STREQUAL "sony")
		psp2_sce_layout(${name})
		return()
	endif()

	# IMPORTS: imports the original carries although no code left references them
	# (callers dropped by Sony's linker); referenced here so they stay in the table
	list(APPEND MODULE_IMPORT_FUNCS ${M_IMPORTS})

	# skeleton import references: keep every original import in the import table
	set(imports "${CMAKE_CURRENT_BINARY_DIR}/imports.c")
	set(c "/* generated: import references for ${name} (${FW}) */\n")
	foreach(s IN LISTS MODULE_IMPORT_FUNCS)
		string(APPEND c "extern void ${s}(void);\n")
	endforeach()
	foreach(s IN LISTS MODULE_IMPORT_VARS)
		string(APPEND c "extern char ${s}[];\n")
	endforeach()
	# SNC removes unreferenced file-local constants despite the used attribute.
	# External linkage keeps these interface references in both compilers.
	string(APPEND c "__attribute__((used)) void *const module_import_references[] = {\n\t(void *)0,\n")
	foreach(s IN LISTS MODULE_IMPORT_FUNCS MODULE_IMPORT_VARS)
		string(APPEND c "\t(void *)${s},\n")
	endforeach()
	string(APPEND c "};\n")
	if(NOT MODULE_IMPORT_FUNCS AND NOT MODULE_IMPORT_VARS)
		# vita-elf-create needs at least one relocation
		string(APPEND c "__attribute__((used)) static const void *const anchor = &anchor;\n")
	endif()
	if(DEFINED MODULE_SDK_VERSION)
		string(APPEND c "\nconst int module_sdk_version = ${MODULE_SDK_VERSION};\n")
	endif()
	file(CONFIGURE OUTPUT "${imports}" CONTENT "${c}" @ONLY)

	# extra NID 0 variables: table fixup_velf.py --nid0 turns into the record's tables.
	# Room for 8 entries of vita-elf-create's own, then ours; weak placeholder objects
	# stand in for symbols the module does not define (skeletons).
	set(nid0 "")
	set(nid0_args "")
	if(MODULE_NID0_VARS)
		set(nid0 "${CMAKE_CURRENT_BINARY_DIR}/nid0.S")
		set(nid0_args --nid0)
		set(n "")
		set(e "")
		set(weak "")
		list(LENGTH MODULE_NID0_VARS count)
		foreach(v IN LISTS MODULE_NID0_VARS)
			string(REPLACE ":" ";" v "${v}")
			list(GET v 0 vnid)
			list(GET v 1 vsym)
			string(APPEND n "\t.word\t${vnid}\n")
			string(APPEND e "\t.word\t${vsym}\n")
			string(APPEND weak "\t.weak\t${vsym}\n\t.type\t${vsym}, %object\n${vsym}:\n\t.word\t0\n")
		endforeach()
		file(CONFIGURE OUTPUT "${nid0}" CONTENT
"/* generated: extra NID 0 export variables for ${name} (${FW}) */
	.section .rodata.sce_nid0, \"a\"
	.balign	4
	.ascii	\"NID0\"
	.word	${count}
	.space	32
${n}	.space	32
${e}
	.section .rodata.sce_nid0_weak, \"a\"
	.balign	4
${weak}" @ONLY)
	endif()

	add_executable(${name} ${M_SOURCES} "${imports}" ${nid0})
	psp2_compile_settings(${name})
	if(M_SKELETON OR MODULE_SCE_RESERVE)
		# room for the SCE tables after the text (a 2 MiB page instead merged
		# text and data into one segment, which vita-elf-create rejects)
		target_link_options(${name} PRIVATE
			-Wl,-T,${PSP2_SRC}/cmake/sce_reserve.ld)
	endif()
	target_link_libraries(${name} PRIVATE
		-nostdlib -Wl,--start-group ${PSP2_STUB_LIBS_${M_KIND}} -Wl,--end-group gcc)

	add_custom_command(TARGET ${name} POST_BUILD
		COMMAND python3 "${PSP2_SRC}/tools/dbginfo.py" $<TARGET_FILE:${name}>
		VERBATIM)

	add_custom_command(OUTPUT ${name}.velf
		COMMAND vita-elf-create -n -p -e "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_EXPORTS}" $<TARGET_FILE:${name}> ${name}.velf
		COMMAND python3 "${PSP2_SRC}/tools/fixup_velf.py" ${name}.velf --nid ${MODULE_NID}
			--attr ${MODULE_ATTR} --import ${MODULE_IMPORT_LIBS} --export ${MODULE_EXPORT_ATTRS} ${nid0_args}
		DEPENDS ${name} $<TARGET_FILE:${name}> "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_EXPORTS}" "${PSP2_SRC}/tools/fixup_velf.py" "${fwfile}"
		VERBATIM)
	add_custom_target(${name}_velf ALL DEPENDS ${name}.velf)
endfunction()

# A raw boot image (no SCE module around it, e.g. NSKBL): linked at ADDRESS with the
# reset vector first, written as <name>.elf and the flat <name>.bin.
#   psp2_image(<target> ADDRESS <addr> [LINKER_SCRIPT <file>] [SKELETON] [PARTIAL]
#              SOURCES <files...>)
# fw/<FW>.cmake says the image is in that firmware (and may set MODULE_DEFINES).
# LINKER_SCRIPT (relative to the module) replaces cmake/image.ld for an image of several
# segments; IMAGE_ADDRESS is still defined for it.
function(psp2_image name)
	cmake_parse_arguments(M "SKELETON;PARTIAL" "ADDRESS;LINKER_SCRIPT" "SOURCES" ${ARGN})
	set(script "${PSP2_SRC}/cmake/image.ld")
	if(M_LINKER_SCRIPT)
		set(script "${CMAKE_CURRENT_SOURCE_DIR}/${M_LINKER_SCRIPT}")
	endif()
	set(fwfile "${CMAKE_CURRENT_SOURCE_DIR}/fw/${FW}.cmake")
	if(NOT EXISTS "${fwfile}")
		message(STATUS "psp2: ${name} not in ${FW}, skipped")
		return()
	endif()
	set(MODULE_DEFINES "")
	set(MODULE_DEPENDS "")
	include("${fwfile}")
	set(M_KIND kernel)

	if(M_PARTIAL)
		add_library(${name} OBJECT ${M_SOURCES})
		psp2_compile_settings(${name})
		return()
	endif()

	add_executable(${name} ${M_SOURCES})
	psp2_compile_settings(${name})
	set_target_properties(${name} PROPERTIES SUFFIX ".elf" LINK_DEPENDS "${script}")
	# RWX regions, like the originals (NSKBL runs from an RWX identity mapping)
	target_link_options(${name} PRIVATE -nostdlib -Wl,--defsym=IMAGE_ADDRESS=${M_ADDRESS}
		-Wl,-T,${script})
	if(NOT PSP2_TOOLCHAIN STREQUAL "sony")
		target_link_options(${name} PRIVATE -Wl,--no-warn-rwx-segments)
		target_link_libraries(${name} PRIVATE gcc)
	endif()
	add_custom_command(OUTPUT ${name}.bin
		COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${name}.bin
		DEPENDS ${name}
		VERBATIM)
	add_custom_target(${name}_bin ALL DEPENDS ${name}.bin)
endfunction()

# Sony's layout (psp2_module SCE_LAYOUT): called with psp2_module's variables in scope.
macro(psp2_sce_layout name)
	set(tables "${CMAKE_CURRENT_BINARY_DIR}/scetables.S")
	set(sdk "")
	if(DEFINED MODULE_SDK_VERSION)
		set(sdk --sdk-version ${MODULE_SDK_VERSION})
	endif()
	set(nid0 "")
	foreach(v IN LISTS MODULE_NID0_VARS)
		list(APPEND nid0 ${v})
	endforeach()
	add_custom_command(OUTPUT "${tables}"
		COMMAND python3 "${PSP2_SRC}/tools/mkprx.py" tables -o "${tables}"
			--exports "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_EXPORTS}"
			--nids "${PSP2_SRC}/tools/nids/${M_KIND}.yml" "${PSP2_SRC}/tools/nids/user.yml"
			--nid ${MODULE_NID} --attr ${MODULE_ATTR} ${sdk}
			--import ${MODULE_IMPORT_LIBS} --import-nids ${MODULE_IMPORT_NIDS}
			--import-format ${MODULE_IMPORT_FORMAT}
			--export-attr ${MODULE_EXPORT_ATTRS} --export-order ${MODULE_EXPORT_ORDER}
			--export-aux ${MODULE_EXPORT_AUX} --nid0 ${nid0}
			--nid0-var-order ${MODULE_NID0_VAR_ORDER}
		DEPENDS "${PSP2_SRC}/tools/mkprx.py" "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_EXPORTS}" "${fwfile}"
			"${PSP2_SRC}/tools/nids/${M_KIND}.yml" "${PSP2_SRC}/tools/nids/user.yml"
		VERBATIM)

	# the stubs bind every import; the old path's hand-made bindings would collide
	list(FILTER M_SOURCES EXCLUDE REGEX "(^|/)import_bindings\\.S$")
	add_executable(${name} ${M_SOURCES} "${tables}")
	# Sony's objects carry unwind tables (an .ARM.exidx entry per function)
	target_compile_options(${name} PRIVATE "$<$<COMPILE_LANGUAGE:C>:-funwind-tables>")
	psp2_compile_settings(${name})
	# -q keeps the relocations, which mkprx.py turns into the module's SCE relocations
	set_target_properties(${name} PROPERTIES LINK_DEPENDS "${PSP2_SRC}/cmake/prx.ld")
	target_link_options(${name} PRIVATE -nostdlib -Wl,-q -Wl,-T,${PSP2_SRC}/cmake/prx.ld
		-Wl,--no-merge-exidx-entries)

	add_custom_command(TARGET ${name} POST_BUILD
		COMMAND python3 "${PSP2_SRC}/tools/dbginfo.py" $<TARGET_FILE:${name}>
		VERBATIM)
	add_custom_command(OUTPUT ${name}.velf
		COMMAND python3 "${PSP2_SRC}/tools/mkprx.py" elf $<TARGET_FILE:${name}> ${name}.velf
			--exports "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_EXPORTS}"
		DEPENDS ${name} $<TARGET_FILE:${name}> "${PSP2_SRC}/tools/mkprx.py"
		VERBATIM)
	add_custom_target(${name}_velf ALL DEPENDS ${name}.velf)
endmacro()
