# os0:us/avcodec_us.suprx - user side of the codec services (one syscall per export)
psp2_module(os0_us_avcodec_us KIND user SOURCES
	source/avcodec_us.c
)
# the original calls every syscall (push / blx / pop), never tail-branches
target_compile_options(os0_us_avcodec_us PRIVATE -fno-optimize-sibling-calls)
