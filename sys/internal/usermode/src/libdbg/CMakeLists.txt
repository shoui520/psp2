# SceLibDbg (vs0:sys/external/libdbg.suprx), built by Sony with GCC: the SDK's assertion and
# logging handlers, two translation units (each has its own string pool).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	assert.c
	log.c
)
