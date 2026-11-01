# SceLibHttp (vs0:sys/external/libhttp.suprx), built by Sony with SNC 3.57.
set(libhttp_zlib ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/zlib-1.2.5)
psp2_module(vs0_sys_external_libhttp KIND user COMPILER snc PARTIAL SOURCES
	auth.c
	authcache.c
	conn.c
	cookie.c debug.c header.c request.c http.c object.c pool.c sock.c parse.c redirect.c req.c template.c uri.c util.c cert.c
	${libhttp_zlib}/inflate.c ${libhttp_zlib}/inffast.c ${libhttp_zlib}/inftrees.c
	zlib/checksum.c zlib/checksum_neon.S
	libhttp.c
)
# zlib 1.2.5's inflate, for gzip bodies (req.c): pristine upstream, its memory copies through
# libhttp's zmemcpy (pool.c), and the NEON check values (zlib/)
target_include_directories(vs0_sys_external_libhttp PRIVATE ${libhttp_zlib}
	../../../../../cmake/compat/zlib)
set_source_files_properties(${libhttp_zlib}/inflate.c ${libhttp_zlib}/inffast.c
	${libhttp_zlib}/inftrees.c zlib/checksum.c PROPERTIES
	COMPILE_DEFINITIONS NO_MEMCPY)
