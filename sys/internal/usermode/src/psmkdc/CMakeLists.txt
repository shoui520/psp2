# SceDrmPsmKdc (vs0:sys/external/psmkdc.suprx), built by Sony with SNC 3.57. No debug record
# names its files; psmkdc.c is named by the module and psm_drm.c by ScePsmDrm (ours). It links
# GcAuthMgr's RSA check (built with GCC, as in the kernel), the SDK's MT19937 and SHA-256
# libraries, and SNC's runtime.
set(psmkdc_marlin ../../../driver/src/marlin_hci)
set(psmkdc_sdk ../../../../sdk/target/src)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	psmkdc.c
	../../../kernel/src/gcauthmgr/rsa_signature.c
	${psmkdc_marlin}/rsa_public_operation.c
	${psmkdc_marlin}/rsa_integer_arithmetic.c
	${psmkdc_marlin}/rsa_montgomery.c
	${psmkdc_marlin}/rsa_integer_multiply.c
	${psmkdc_marlin}/rsa_integer_remainder.c
	${psmkdc_marlin}/rsa_integer_square.c
	${psmkdc_marlin}/rsa_integer_tail.c
	${psmkdc_sdk}/mt19937/libmt19937.c
	${psmkdc_sdk}/libsnc/i64ops.c
	${psmkdc_sdk}/libsnc/i32ops.c
	${psmkdc_sdk}/libsnc/weak_calls.c
	${psmkdc_sdk}/libsnc/aeabi_div0.c
	psm_drm.c
	${psmkdc_sdk}/sha256/libsha256.c
	${psmkdc_sdk}/sha256/sha256.S
	${psmkdc_sdk}/mt19937/mtrand.S
)
# GcAuthMgr's RSA files, a section per function: the module links only the check
set_source_files_properties(
	../../../kernel/src/gcauthmgr/rsa_signature.c
	${psmkdc_marlin}/rsa_public_operation.c
	${psmkdc_marlin}/rsa_integer_arithmetic.c
	${psmkdc_marlin}/rsa_montgomery.c
	${psmkdc_marlin}/rsa_integer_multiply.c
	${psmkdc_marlin}/rsa_integer_remainder.c
	${psmkdc_marlin}/rsa_integer_square.c
	${psmkdc_marlin}/rsa_integer_tail.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS -ffunction-sections)
# the SDK's libraries: the module keeps sceSha256Digest, which nothing calls
set_source_files_properties(
	${psmkdc_sdk}/mt19937/libmt19937.c
	${psmkdc_sdk}/sha256/libsha256.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-fno-toplevel-reorder")
set_source_files_properties(
	${psmkdc_sdk}/sha256/sha256.S
	${psmkdc_sdk}/mt19937/mtrand.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
# Sony's psp2ld linked with --strip-unused: of SNC's runtime only what the module calls
set_source_files_properties(${psmkdc_sdk}/libsnc/weak_calls.c
	PROPERTIES COMPILE_DEFINITIONS SNC_WEAK_CALLS_ON)
set_source_files_properties(
	${psmkdc_sdk}/libsnc/i64ops.c
	${psmkdc_sdk}/libsnc/i32ops.c
	${psmkdc_sdk}/libsnc/aeabi_div0.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
