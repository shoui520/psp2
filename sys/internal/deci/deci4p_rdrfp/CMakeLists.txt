# SceDeci4pRDrfp: Sony's rdrfp*.c (and the fd table, file name unknown).
psp2_module(os0_kd_deci4p_rdrfp KIND kernel SOURCES
	rdrfp.c
	rdrfp_queue.c
	rdrfp_buffer.c
	rdrfp_fs.c
	rdrfp_fs_cmds.c
	rdrfp_list.c
	rdrfp_vdentry.c
	rdrfp_vdtree.c
	rdrfp_fd.c
	rdrfp_writebuf_list.c
)
# Sony built this module with -O3 (fully unrolled month loops, prefetching in sendReadRes)
target_compile_options(os0_kd_deci4p_rdrfp PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
