# SceLibSsl, built by Sony with SNC 3.57; implementation in progress.
set(libssl_ssleay ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/ssleay-0.9.1b)
include(vendor_patch.cmake)
libssl_prepare_patched(${libssl_ssleay}/crypto/lhash/lhash.c
	${CMAKE_CURRENT_SOURCE_DIR}/lhash/lhash_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/lhash_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/stack/stack.c
	${CMAKE_CURRENT_SOURCE_DIR}/stack/stack_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/stack_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/ex_data.c
	${CMAKE_CURRENT_SOURCE_DIR}/ex_data_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/ex_data_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/bio/bio.h
	${CMAKE_CURRENT_SOURCE_DIR}/bio/bio_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/bio_sce.h)
libssl_prepare_patched(${libssl_ssleay}/crypto/bio/bio_lib.c
	${CMAKE_CURRENT_SOURCE_DIR}/bio/bio_lib_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/bio_lib_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/bio/bss_sock.c
	${CMAKE_CURRENT_SOURCE_DIR}/bio/bss_sock_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/bss_sock_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/bio/b_print.c
	${CMAKE_CURRENT_SOURCE_DIR}/bio/b_print_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/b_print_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/bio/b_dump.c
	${CMAKE_CURRENT_SOURCE_DIR}/bio/b_dump_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/b_dump_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/cryptlib.c
	${CMAKE_CURRENT_SOURCE_DIR}/cryptlib_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/cryptlib_sce.inc)
libssl_prepare_patched(${libssl_ssleay}/crypto/mem.c
	${CMAKE_CURRENT_SOURCE_DIR}/mem_ctrl_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/mem_ctrl_sce.inc)
psp2_module(vs0_sys_external_libssl KIND user COMPILER snc PARTIAL SOURCES
	asn1.c
	bio/bio_lib.c
	bio/bss_sock.c
	bio/b_print.c
	bio/b_dump.c
	asn1interpreter.c
	ex_data.c
	err/err.c
	bio/bio_flags.c
	string.c
	format.c
	lhash/lhash.c
	stack/stack.c
	runtime.c
	memory.c
	libssl.c
)
target_include_directories(vs0_sys_external_libssl PRIVATE
	${libssl_ssleay}/crypto ${libssl_ssleay}/crypto/stack
	${libssl_ssleay}/crypto/lhash ${CMAKE_CURRENT_BINARY_DIR})
