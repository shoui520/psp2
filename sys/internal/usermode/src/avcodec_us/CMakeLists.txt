# os0:us/avcodec_us.suprx - user side of the codec services (one syscall per export)
psp2_module(os0_us_avcodec_us KIND user COMPILER gcc48 SOURCES
	avcodec_us.c
)
# the original calls every syscall (push / blx / pop), never tail-branches; it has no stack
# protector (it imports no __stack_chk_guard)
target_compile_options(os0_us_avcodec_us PRIVATE -fno-optimize-sibling-calls -fno-stack-protector)
