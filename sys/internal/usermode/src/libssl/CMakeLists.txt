# SceLibSsl, built by Sony with SNC 3.57; implementation in progress.
set(libssl_ssleay ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/ssleay-0.9.1b)
include(vendor_patch.cmake)
libssl_prepare_patched(${libssl_ssleay}/crypto/lhash/lhash.c
	${CMAKE_CURRENT_SOURCE_DIR}/lhash/lhash_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/lhash_sce.inc)
psp2_module(vs0_sys_external_libssl KIND user COMPILER snc PARTIAL SOURCES
	asn1.c
	asn1interpreter.c
	lhash/lhash.c
	memory.c
	libssl.c
)
target_include_directories(vs0_sys_external_libssl PRIVATE
	${libssl_ssleay}/crypto ${libssl_ssleay}/crypto/stack
	${libssl_ssleay}/crypto/lhash ${CMAKE_CURRENT_BINARY_DIR})
