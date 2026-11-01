# SceLibHttp (vs0:sys/external/libhttp.suprx), built by Sony with SNC 3.57.
set(libhttp_zlib ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/zlib-1.2.5)
psp2_module(vs0_sys_external_libhttp KIND user COMPILER snc PARTIAL SOURCES
	auth.c
	authcache.c
	conn.c
	cookie.c debug.c header.c request.c https.c http.c object.c pool.c sock.c parse.c redirect.c req.c template.c uri.c util.c cert.c ssl.c
	${libhttp_zlib}/inflate.c ${libhttp_zlib}/inffast.c ${libhttp_zlib}/inftrees.c
	zlib/checksum.c zlib/checksum_neon.S
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/base64/base64.c
	../../../../sdk/target/src/md5/libmd5.c ../../../../sdk/target/src/md5/md5.S
	libhttp.c
)
# zlib 1.2.5's inflate, for gzip bodies (req.c): pristine upstream, its memory copies through
# libhttp's zmemcpy (pool.c), and the NEON check values (zlib/)
target_include_directories(vs0_sys_external_libhttp PRIVATE ${libhttp_zlib}
	../../../../../cmake/compat/zlib)
set_source_files_properties(${libhttp_zlib}/inflate.c ${libhttp_zlib}/inffast.c
	${libhttp_zlib}/inftrees.c zlib/checksum.c PROPERTIES
	COMPILE_DEFINITIONS NO_MEMCPY)
# the SDK's libraries in it, which Sony built with GCC 4.8 and without stack guards
set_source_files_properties(../../../../sdk/target/src/base64/base64.c
	../../../../sdk/target/src/md5/libmd5.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS -fno-stack-protector)
