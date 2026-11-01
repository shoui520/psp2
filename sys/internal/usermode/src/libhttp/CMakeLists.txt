# SceLibHttp (vs0:sys/external/libhttp.suprx), built by Sony with SNC 3.57.
psp2_module(vs0_sys_external_libhttp KIND user COMPILER snc PARTIAL SOURCES
	auth.c
	authcache.c
	conn.c
	cookie.c debug.c header.c request.c http.c object.c pool.c sock.c parse.c redirect.c req.c
	libhttp.c
)
# zlib 1.2.5's inflate, for gzip bodies (req.c)
target_include_directories(vs0_sys_external_libhttp PRIVATE ../../../../../contrib/zlib-1.2.5
	../../../../../cmake/compat/zlib)
