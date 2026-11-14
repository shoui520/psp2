# SceLibc (vs0:sys/external/libc.suprx), built by Sony with SNC 3.57: Dinkumware's C and C++
# libraries (SceLibc, SceLibm, SceLibstdcxx) over Sony's psp2 system interface. One
# Dinkumware function per file, in the original's link order.
psp2_module(vs0_sys_external_libc KIND user COMPILER snc PARTIAL SOURCES
	psp2io.c
	psp2time.c
	module.c
	abort.c
	abs.c
	asctime.c
	atof.c
	atoff.c
	atoi.c
	atol.c
	atoll.c
	bsearch.c
	btowc.c
	c16rtomb.c
	c32rtomb.c
	clearerr.c
	ctime.c
	asctime_s.c
	bsearch_s.c
	ctime_s.c
	fopen_s.c
	fprintf_s.c
	freopen_s.c
	fscanf_s.c
	fwprintf_s.c
	fwscanf_s.c
	gets_s.c
	gmtime_s.c
	localtime_s.c
)
if(TARGET vs0_sys_external_libc)
	# the library defines the C11 bounds-checking interfaces
	target_compile_definitions(vs0_sys_external_libc PRIVATE __STDC_WANT_LIB_EXT1__=1)
	target_include_directories(vs0_sys_external_libc BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
