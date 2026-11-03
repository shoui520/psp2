/*
 * SceGpuEs4User_E98914E4's untextured pixel event program: Imagination's
 * codegen/pds/pixelevent.asm.c (contrib/img-sgx-ddk-1.7.862890) for the
 * SGX543, with the feature macros expanded to their literals, so that the
 * DDK's own assembler (pdsasm) can read it.
 *
 * The sequencer checks the pixel event in ir1 and sends one of three task
 * control blocks to the USSE: the end of tile (EOT), the end of render (EOR)
 * or the pass offset (PTOFF). The data segment holds those blocks; the
 * caller patches them (E98914E4's pds->constant) before the GPU runs the
 * program, so their values here are only the ones the DDK leaves.
 *
 * The program is stored padded to 8 bytes: PDS addresses programs in
 * instruction pairs, so the odd word count takes one zero word at the end.
 */
temp dword param0, temp_EOT2;
data dword EOT0, EOT1, EOT2;
data dword EOR0, EOR1, EOR2;
data dword PTOFF0, PTOFF1, PTOFF2;

	!if0 bra	not_eot

	movs	doutu, EOT0, EOT1, ir0
	halt

not_eot:
	!if1 bra	not_eor

	and		param0, 0x00000100, ir1		/* PIX_EVENT_ZEROCNT */
	tstz	p0, param0
	and		param0, ir0, 0xfffeffff		/* ~PDM_EOR */
	or		param0, param0, 0x00020000	/* PDM_EOR_EOT */
	p0	mov32	param0, EOR2

	movs	doutu, EOR0, EOR1, param0
	halt

not_eor:
	movs	doutu, PTOFF0, PTOFF1, PTOFF2
	halt
