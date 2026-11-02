/*
 * The GPU programs gpuinit_es4 loads, in the original's writable segment, shared by every
 * firmware. The binaries are assembled from ukernel/ and pds/ (CMakeLists.txt).
 */
.section .data.gpu_programs, "aw", %progbits
.balign 8

/* the clear clip workaround's USE code (BRN31542) */
.global program10
.type program10, %object
program10:
	.incbin "program10.bin"
.size program10, . - program10

/* the clear clip workaround's three PDS programs, 0x50 bytes each */
.global program9
.type program9, %object
program9:
	.incbin "program9.bin"
.size program9, . - program9

/* the master core's microkernel */
.global program0
.type program0, %object
program0:
	.incbin "program0.bin"
.size program0, . - program0

/*
 * The clear clip workaround's VDM control stream. sgxinit.c adds the addresses: the PDS
 * programs' (program9, +0x50, +0xa0; 16-byte units) to words 0, 5 and 7, and the index
 * stream's to word 3.
 */
.global program8
.type program8, %object
program8:
	.word 1 << 30					/* state: PDS program 0 */
	.word (3 << 27) | (1 << 25) | (1 << 17) | (2 << 13) | (1 << 9) | 5
							/* 48 data bytes, SD, MTE emit, USE pipe 2,
							   1 partition, 5 attribute units */
	.word (2 << 30) | (1 << 24) | (1 << 22) | 24	/* index list: triangles, words 2, 4 and 5,
							   24 indices */
	.word 0						/* the index stream (32-bit indices) */
	.word (8 << 28) | (1 << 24)			/* 8 input instances, 32-bit indices */
	.word (1 << 29) | (1 << 28)			/* PDS program 1: last task, complex */
	.word (1 << 25) | (1 << 24) | (4 << 9) | 3	/* SD, bit 24, 4 partitions,
							   3 attribute units */
	.word 1 << 30					/* state: PDS program 2 */
	.word (3 << 27) | (1 << 25) | (1 << 17) | (2 << 13) | (1 << 9) | 1
							/* 48 data bytes, SD, MTE emit, USE pipe 2,
							   1 partition, 1 attribute unit */
	.word 3 << 30					/* terminate */
.size program8, . - program8

/* the slave cores' microkernel */
.global program1
.type program1, %object
program1:
	.incbin "program1.bin"
.size program1, . - program1
