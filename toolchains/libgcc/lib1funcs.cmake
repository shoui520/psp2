# Build the SCE adaptation of the assembly runtime without changing the pristine upstream files.
# Sony's libgcc reaches __aeabi_idiv0 and __aeabi_ldiv0 from Thumb-2 (and ARM) code with the ARMv6-M
# tail call (the handler's address made PC-relative and popped into pc), where CodeSourcery's
# branches with b.w. Writes lib1funcs.S and bpabi.S into OUTPUT_DIR; the files they include
# come from UPSTREAM_DIR, which follows OUTPUT_DIR on the include path.
function(gcc48_prepare_lib1funcs upstream_dir output_dir)
	set(tail "\tpush\t{r0, r1, r2}\n\tldr\tr0, 4f\n\tadr\tr1, 4f\n\tadd\tr0, r1\n\tstr\tr0, [sp, #8]\n\tpop\t{r0, r1, pc}\n\t.align\t2\n4:\n")

	file(READ "${upstream_dir}/lib1funcs.S" source)
	set(branch "\tb.w\tSYM(__aeabi_idiv0) __PLT__\n#else")
	string(FIND "${source}" "${branch}" at)
	if(at LESS 0)
		message(FATAL_ERROR "Pinned GCC THUMB_LDIV0 branch marker missing")
	endif()
	string(REPLACE "${branch}"
		"\t/* SCE: the ARMv6-M tail call */\n${tail}\t.word\t__aeabi_idiv0 - 4b\n#else"
		source "${source}")
	# the same in ARM_LDIV0, for the ARM runtime (NSKBL's)
	set(branch "\tb\tSYM (__aeabi_idiv0) __PLT__\n.endm\n#else")
	string(FIND "${source}" "${branch}" at)
	if(at LESS 0)
		message(FATAL_ERROR "Pinned GCC ARM_LDIV0 branch marker missing")
	endif()
	string(REPLACE "${branch}"
		"\t/* SCE: the ARMv6-M tail call */\n${tail}\t.word\t__aeabi_idiv0 - 4b\n.endm\n#else"
		source "${source}")
	file(WRITE "${output_dir}/lib1funcs.S" "${source}")

	file(READ "${upstream_dir}/bpabi.S" source)
	set(branch "\tb\tSYM (__aeabi_ldiv0) __PLT__\n1:\n#else")
	string(FIND "${source}" "${branch}" at)
	if(at LESS 0)
		message(FATAL_ERROR "Pinned GCC test_div_by_zero branch marker missing")
	endif()
	string(REPLACE "${branch}"
		"\t/* SCE: the ARMv6-M tail call */\n${tail}\t.word\t__aeabi_ldiv0 - 4b\n1:\n#else"
		source "${source}")
	file(WRITE "${output_dir}/bpabi.S" "${source}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${upstream_dir}/lib1funcs.S" "${upstream_dir}/bpabi.S")
endfunction()

# TARGET assembles the runtime wrappers here (udiv.S, ldivmod.S, ...): the adapted files
# go into its build folder, which only those wrappers search, ahead of the upstream folder.
function(gcc48_use_lib1funcs target)
	set(upstream "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../contrib/gcc-4.8-2014.05/libgcc/config/arm")
	set(output "${CMAKE_CURRENT_BINARY_DIR}/gcc48-lib1funcs")
	file(MAKE_DIRECTORY "${output}")
	gcc48_prepare_lib1funcs("${upstream}" "${output}")
	get_target_property(sources ${target} SOURCES)
	list(FILTER sources INCLUDE REGEX "toolchains/libgcc/[^/]*\\.S$")
	set_property(SOURCE ${sources} TARGET_DIRECTORY ${target} APPEND PROPERTY
		INCLUDE_DIRECTORIES "${output}" "${upstream}")
endfunction()
