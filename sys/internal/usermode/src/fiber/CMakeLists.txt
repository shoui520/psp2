# SceFiber (vs0:sys/external/libfiber.suprx), built by Sony with SNC 3.57.
# ulobj_dbg.c's helpers, each called once, are not inlined: no automatic inlining
set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=0>)
psp2_module(vs0_sys_external_libfiber KIND user COMPILER snc SOURCES
	fiber_context.S
	fiber.c
	ulobj_dbg.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
)
