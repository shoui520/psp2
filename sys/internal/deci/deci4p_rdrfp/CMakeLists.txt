# SceDeci4pRDrfp: Sony's rdrfp*.c (and the fd table, file name unknown).
psp2_module(os0_kd_deci4p_rdrfp KIND kernel SOURCES
	source/rdrfp.c
	source/rdrfp_queue.c
	source/rdrfp_buffer.c
	source/rdrfp_fs.c
	source/rdrfp_fs_cmds.c
	source/rdrfp_list.c
	source/rdrfp_vdentry.c
	source/rdrfp_vdtree.c
	source/rdrfp_fd.c
	source/rdrfp_writebuf_list.c
)
# Sony built this module with -O3 (fully unrolled month loops, prefetching in sendReadRes)
target_compile_options(os0_kd_deci4p_rdrfp PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
