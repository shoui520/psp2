# vs0:sys/external/libgxm_dbg_es4 - SceGxmDbg, the debug build of SceGxm (..). Sony built
# both from one source tree: the same exports, plus the [SceGxmDbg] argument checks and
# messages (context_debug.c and the checks in the other files). Until those are written it
# stays a skeleton; then it compiles ..'s sources with the debug checks enabled.
# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(vs0_sys_external_libgxm_dbg_es4 KIND user SKELETON SOURCES
	libgxm_dbg_es4.c
)
