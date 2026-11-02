# SceLibSsl, built by Sony with SNC 3.57; implementation in progress.
psp2_module(vs0_sys_external_libssl KIND user COMPILER snc PARTIAL SOURCES
	asn1.c
	asn1interpreter.c
	memory.c
	libssl.c
)
