# SceNpUtility (vs0:sys/external/np_utility.suprx), built by Sony with SNC 3.57. No debug
# record names its files: they are split where its parts are (ours).
psp2_module(vs0_sys_external_np_utility KIND user COMPILER snc PARTIAL SOURCES
	word_filter.c
	lookup.c
	client.c
	memory.c
	title.c
	trans.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
